#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <integration/client.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <set>

Client client(61423, "");

using json = nlohmann::json;

Client::Client(int port, const std::string& auth) 
    : port(port), auth(auth), connected(false) {}

bool Client::connect()
{
    return tryConnect();
}

void Client::disconnect()
{
    socket.disconnect();
    connected = false;
}

bool Client::isConnected() const
{
    return connected;
}

//We are on localhost, not across the globe
bool Client::tryConnect()
{
    if(connected) return true;
    if(needs_manual) return false;

    
    for(int i = 0; i < 5; i++)
    {
        if(socket.connect(sf::IpAddress::LocalHost, port + i, sf::milliseconds(100)) == sf::Socket::Status::Done)
        {
            connected = true;
            return true;
        }
        //std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "You need KubeJS on 1.21.1 or WebKit on 1.20.1 at port 61423 to see this page\n";
    needs_manual = true;
    return false;
}

std::string Client::extractHttpBody(const std::string& httpResponse)
{
    size_t headerEnd = httpResponse.find("\r\n\r\n");
    if(headerEnd != std::string::npos)
    {
        return httpResponse.substr(headerEnd + 4);
    }
    return httpResponse;
}

bool Client::sendReloadCommand(const ReloadType& type_reload)
{
    std::string typeStr;
    switch(type_reload)
    {
        case ReloadType::CLIENT: typeStr = "client"; break;
        case ReloadType::SERVER: typeStr = "server"; break;
        case ReloadType::STARTUP: typeStr = "startup"; break;
    }
    return sendHttpRequest("POST", "/api/reload/" + typeStr);
}

bool Client::sendHttpRequest(const std::string& method, const std::string& path, 
                                   const std::string& body, std::string& response)
{
    if(!connected)
    {
        if(!tryConnect()) return false;
    }
    else
    {
        disconnect();
        connect();
    }

    std::string request = method + " " + path + " HTTP/1.1\r\n";
    request += "Host: localhost:" + std::to_string(port) + "\r\n";
    request += "Authorization: " + auth + "\r\n";
    request += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    request += body;

    if(socket.send(request.c_str(), request.length()) != sf::Socket::Status::Done)
    {
        connected = false;
        return false;
    }

    std::vector<char> buffer(4000);
    std::size_t received;
    response.clear();

    while(socket.receive(buffer.data(), buffer.size(), received) == sf::Socket::Status::Done)
    {
        response.append(buffer.data(), received);
    }

    response = extractHttpBody(response);
    return true;
}

bool Client::sendHttpRequest(const std::string& method, const std::string& path, const std::string& body)
{
    std::string dummy;
    return sendHttpRequest(method, path, body, dummy);
}

std::vector<Mod> Client::getAvailableMods()
{
    std::string resp;
    if(!sendHttpRequest("GET", "/api/mods", "", resp)) return {};

    std::vector<Mod> mods;
    auto j = json::parse(resp, nullptr, false, true);
    if(j.is_discarded()) return {};

    mods.resize(j.size());
    for(const auto& item : j)
    {
        mods.push_back({
            item.value("name", "Unknown"),
            item.value("id", "unknown"),
            item.value("version", "1.0")
        });
    }
    return mods;
}

std::vector<ListResponse> Client::listAllAssets()
{
    std::vector<ListResponse> assets;
    
    auto fetch_and_append = [&](const std::string& url, TypeElement type) {
        std::string resp;
        if(!sendHttpRequest("GET", url, "", resp))
        {
            return;
        }

        auto j = json::parse(resp, nullptr, false,true);
        if(j.is_discarded()) return;
        for(auto& [modId, files] : j.items())
        {
            for(const auto& file : files)
            {
                try
                {
                    assets.push_back({
                        modId,
                        file.get<std::string>(),
                        type
                    });
                }
                catch(...) 
                {}
            }
        }
    };

    fetch_and_append("/api/client/assets/list/models", TypeElement::BLOCK);
    fetch_and_append("/api/client/assets/list/textures/block", TypeElement::BLOCK);
    fetch_and_append("/api/client/assets/list/textures/item", TypeElement::ITEM);

    return assets;
}

std::vector<ListResponse> Client::listAssetsByPrefix(const std::string& prefix)
{
    auto all = listAllAssets();
    std::vector<ListResponse> filtered;
    for(const auto& asset : all)
    {
        if(asset.path.find(prefix) != std::string::npos)
        {
            filtered.emplace_back(asset);
        }
    }
    return filtered;
}

std::vector<std::string> Client::listAssetsByPath(const std::string& path)
{
    std::vector<std::string> assets;
    std::string resp;
    
    std::string url = "/api/client/assets/list/" + path;

    if(!sendHttpRequest("GET", url, "", resp))
    {
        return assets;
    }
    auto j = json::parse(resp, nullptr, false, true);
    if(j.is_discarded()) 
    {
        return assets;
    }
    
    for(auto& [modId, files] : j.items())
    {
        for(const auto& file : files)
        {
            try
            {
                std::string file_path = file.get<std::string>();
                assets.push_back(modId + ":" + file_path);
            }
            catch(...) 
            {
            }
        }
    }

    return assets;
}

bool Client::downloadToFile(const std::string& urlPath, const std::string& outputPath)
{
    std::string respBody;
    if(!sendHttpRequest("GET", urlPath, "", respBody)) return false;

    std::filesystem::path path(outputPath);
    if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

    std::ofstream file(outputPath, std::ios::binary);
    file.write(respBody.c_str(), respBody.size());
    return file.good();
}

inline std::string replaceId(std::string id)
{
    std::replace(id.begin(), id.end(), ':', '/');
    return id;
}

bool Client::downloadAssetFile(const std::string& id)
{
    std::string sanitized = replaceId(id);
    std::string url = "/api/client/assets/get/" + sanitized + ".json"; 
    return downloadToFile(url, sanitized + ".json");
}

std::vector<std::string> Client::searchBlocks()
{
    std::string resp;
    if(!sendHttpRequest("GET", "/api/client/search/blocks", "", resp)) return {};
    
    auto j = json::parse(resp, nullptr, false, true);
    if(j.is_discarded()) return {};

    std::vector<std::string> ids;
    for(const auto& block : j)
    {
        if(block.contains("id"))
        {
            ids.emplace_back(block["id"]);
        }
    }

    return ids;
}

std::vector<std::string> Client::searchItems()
{
    std::string resp;
    if(!sendHttpRequest("GET", "/api/client/search/items", "", resp)) return {};

    auto j = json::parse(resp, nullptr, false, true);
    if(j.is_discarded()) return {};
    if(!j.contains("results")) return {};

    std::vector<std::string> ids;
    for(const auto& res : j["results"])
    {
        if(!res.contains("id")) continue;
        if(res.contains("block"))
        {
            if(res["id"] != res["block"])
            {
                ids.emplace_back(res["id"]);
            }
        }
        else
        {
            ids.emplace_back(res["id"]);
        }
    }
    return ids;
}

std::vector<std::string> Client::searchFluids()
{
    std::string resp;
    if(!sendHttpRequest("GET", "/api/client/assets/list/textures/block", "", resp)) return {};
    //Why is this different from the rest? Because we got lied on 1.21.1 and there is no search/fluid on 1.20.1

    auto j = json::parse(resp, nullptr, false, true);
    if(j.is_discarded()) return {};

    std::set<std::string> ids;
    for(auto& [mod_id, files] : j.items())
    {
        for(const auto& file : files)
        {
            std::string path = file.get<std::string>();
                
            bool in_fluid_dir = (path.find("/fluid/") != std::string::npos     || 
                                 path.find("/fluids/") != std::string::npos    ||
                                 path.substr(0, 6) == "fluid/"             ||
                                 path.substr(0, 7) == "fluids/");

            if(!in_fluid_dir) continue;
                
            std::vector<std::string> suffixes = {"_still", "_flow", "_gas_still", "_liquid_still", "_gas", "_liquid"};
            bool has_suffix = false;
            std::string name = "";

            size_t last_slash = path.find_last_of('/');
            name = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);

            size_t dot = name.find_last_of('.');
            if(dot != std::string::npos) name = name.substr(0, dot);

            for(const auto& s : suffixes)
            {
                if(name.length() < s.length()) continue;
                if(name.substr(name.length() - s.length()) != s) continue;

                name = name.substr(0, name.length() - s.length());
                has_suffix = true;
                break;
            }
                    
            if(has_suffix && !name.empty())
            {
                ids.insert(mod_id + ":" + name);
            }
        }
    }

    ids.insert("minecraft:water");
    ids.insert("minecraft:lava");

    return std::vector<std::string>(ids.begin(), ids.end());
}

sf::Image Client::getPreview(const std::string& id, int size, TypeElement type, bool isTag)
{
    std::string typeStr = (type == TypeElement::BLOCK) ? "block" : "item";
    if(isTag) typeStr += "-tag";
    
    std::string sanitized = replaceId(id);
    std::string url = "/img/" + std::to_string(size) + "/" + typeStr + "/" + sanitized;
    
    std::string imgData;
    sf::Image img;
    if(!sendHttpRequest("GET", url, "", imgData)) return {};

    if(!img.loadFromMemory(imgData.data(), imgData.size()))
    {
        std::cerr << "Error loading the texture" << std::endl;
    }
    return img;
}

std::vector<sf::Image> Client::splitVerticalFrames(const sf::Image& spriteSheet)
{
    unsigned int width = spriteSheet.getSize().x;
    int w = width;
    unsigned int height = spriteSheet.getSize().y;
    int count = height / width;

    std::vector<sf::Image> frames;
    frames.resize(count);

    for(int i = 0; i < count; i++)
    {
        sf::Image frame(sf::Vector2u(width, width));
        if(!frame.copy(spriteSheet, {0, 0}, {{0, i*w}, {w, w}}))
        {
            std::cerr << "Failed to copy frame " << i << "\n";
            continue;
        }
        
        frames.emplace_back(frame);
    }
    return frames;
}