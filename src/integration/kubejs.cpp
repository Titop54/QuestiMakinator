#include <integration/kubejs.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

KubeJSClient client(61423, "");

using json = nlohmann::json;

KubeJSClient::KubeJSClient(int port, const std::string& auth) 
    : port(port), auth(auth), connected(false) {}

bool KubeJSClient::connect() {
    return tryConnect();
}

void KubeJSClient::disconnect() {
    socket.disconnect();
    connected = false;
}

bool KubeJSClient::isConnected() const {
    return connected;
}

bool KubeJSClient::tryConnect() {
    if (connected) return true;
    
    for (int i = 0; i < 5; ++i) {
        if (socket.connect(sf::IpAddress::LocalHost, port) == sf::Socket::Status::Done) {
            connected = true;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

std::string KubeJSClient::extractHttpBody(const std::string& httpResponse) {
    size_t headerEnd = httpResponse.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        return httpResponse.substr(headerEnd + 4);
    }
    return httpResponse;
}

bool KubeJSClient::sendReloadCommand(const ReloadType& type_reload) {
    std::string typeStr;
    switch(type_reload) {
        case ReloadType::CLIENT: typeStr = "client"; break;
        case ReloadType::SERVER: typeStr = "server"; break;
        case ReloadType::STARTUP: typeStr = "startup"; break;
    }
    return sendHttpRequest("POST", "/api/reload/" + typeStr);
}

bool KubeJSClient::sendHttpRequest(const std::string& method, const std::string& path, 
                                 const std::string& body, std::string& response) {
    if (!connected) {
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

bool KubeJSClient::sendHttpRequest(const std::string& method, const std::string& path, const std::string& body) {
    std::string dummy;
    return sendHttpRequest(method, path, body, dummy);
}

std::vector<Mod> KubeJSClient::getAvailableMods() {
    std::string resp;
    if (!sendHttpRequest("GET", "/api/mods", "", resp)) return {};

    std::vector<Mod> mods;
    try {
        auto j = json::parse(resp);
        for (const auto& item : j) {
            mods.push_back({
                item.value("name", "Unknown"),
                item.value("id", ""),
                item.value("version", "")
            });
        }
    } catch (...) { std::cerr << "Error parsing mods JSON\n"; }
    return mods;
}

std::vector<ListResponse> KubeJSClient::listAllAssets() {
    std::vector<ListResponse> allAssets;
    
    auto fetchAndAppend = [&](const std::string& url, TypeElement type) {
        std::string resp;
        if (sendHttpRequest("GET", url, "", resp)) {
            try {
                auto j = json::parse(resp);
                for (auto& [modId, files] : j.items()) {
                    for (const auto& file : files) {
                        allAssets.push_back({modId, file.get<std::string>(), type});
                    }
                }
            } catch (...) {}
        }
    };

    fetchAndAppend("/api/client/assets/list/models", TypeElement::BLOCK); // Usamos BLOCK genérico para modelos
    fetchAndAppend("/api/client/assets/list/textures/block", TypeElement::BLOCK);
    fetchAndAppend("/api/client/assets/list/textures/item", TypeElement::ITEM);

    return allAssets;
}

std::vector<ListResponse> KubeJSClient::listAssetsByPrefix(const std::string& prefix) {
    auto all = listAllAssets();
    std::vector<ListResponse> filtered;
    for(const auto& asset : all) {
        if(asset.path.find(prefix) != std::string::npos) {
            filtered.push_back(asset);
        }
    }
    return filtered;
}

bool KubeJSClient::downloadToFile(const std::string& urlPath, const std::string& outputPath) {
    std::string respBody;
    if (!sendHttpRequest("GET", urlPath, "", respBody)) return false;
    
    // Crear directorios
    std::filesystem::path path(outputPath);
    if(path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

    std::ofstream file(outputPath, std::ios::binary);
    file.write(respBody.c_str(), respBody.size());
    return file.good();
}

inline std::string replaceId(std::string id) {
    std::replace(id.begin(), id.end(), ':', '/');
    return id;
}

bool KubeJSClient::downloadAssetFile(const std::string& id) {
    std::string sanitized = replaceId(id);
    std::string url = "/api/client/assets/get/" + sanitized + ".json"; 
    return downloadToFile(url, sanitized + ".json");
}

std::vector<std::string> KubeJSClient::searchBlocks() {
    std::string resp;
    if (!sendHttpRequest("GET", "/api/client/search/blocks", "", resp)) return {};
    std::vector<std::string> ids;
    try {
        auto j = json::parse(resp);
        for(const auto& block : j)
        {
            ids.emplace_back(block["id"]);
        }
    } catch(...) {}
    return ids;
}

std::vector<std::string> KubeJSClient::searchItems() {
    std::string resp;
    if (!sendHttpRequest("GET", "/api/client/search/items", "", resp)) return {};
    std::vector<std::string> ids;
    try {
        auto j = json::parse(resp);
        for(const auto& res : j["results"]) {
            if(!res.contains("block")) {
                ids.push_back(res["id"]);
            }
        }
    } catch(...) {}
    return ids;
}

std::vector<std::string> KubeJSClient::searchFluids() {
    //Not for now
    return {};
}

sf::Image KubeJSClient::getPreview(const std::string& id, int size, TypeElement type, bool isTag) {
    std::string typeStr = (type == TypeElement::BLOCK) ? "block" : "item";
    if(isTag) typeStr += "-tag";
    
    std::string sanitized = replaceId(id);
    std::string url = "/img/" + std::to_string(size) + "/" + typeStr + "/" + sanitized;
    
    std::string imgData;
    sf::Image img;
    if(sendHttpRequest("GET", url, "", imgData)) {
        if(!img.loadFromMemory(imgData.data(), imgData.size()))
        {
            std::cerr << "Error loading the texture" << std::endl;
        }
    }
    return img;
}

std::vector<sf::Image> KubeJSClient::splitVerticalFrames(const sf::Image& spriteSheet, int frameHeight) {
    std::vector<sf::Image> frames;
    if(frameHeight <= 0) return frames;

    unsigned int width = spriteSheet.getSize().x;
    unsigned int height = spriteSheet.getSize().y;
    int count = height / frameHeight;

    for(int i = 0; i < count; ++i) {
        sf::Image frame(sf::Vector2u(width, static_cast<unsigned int>(frameHeight)));
        if (!frame.copy(spriteSheet, {0, 0}, sf::IntRect({0, i * frameHeight}, {(int)width, frameHeight}))) {
             std::cerr << "Failed to copy frame " << i << "\n";
        }
        
        frames.emplace_back(frame);
    }
    return frames;
}