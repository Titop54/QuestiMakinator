#include <SFML/Graphics/Color.hpp>
#define _USE_MATH_DEFINES
#define TINYOBJLOADER_IMPLEMENTATION

#include "integration/kubejs.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <cstddef>
#include "integration/model.h"
#include <iostream>
#include <cmath>
#include <numeric>
#include <set>
#include <fstream>
#include <webp/encode.h>
#include <webp/mux.h>
#include <numbers>

namespace fs = std::filesystem;

struct CustomVertex
{
    sf::Vector2f position;
    sf::Color color;
    sf::Vector2f texCoords;
};

struct RenderQuad
{
    std::vector<CustomVertex> vertices;
    const Texture* texture;
    float depth;
};


ModelGenerator::ModelGenerator(const std::string& rawJson, KubeJSClient& client, const std::string& id) {
    try {
        modelJson = nlohmann::json::parse(rawJson);
    } catch(...) {
        return;
    }
    this->id = id;
    int recursionDepth = 0;
    nlohmann::json currentLevel = modelJson;

    //we want all layers, but up to a limit
    while (currentLevel.contains("parent") && recursionDepth < 10) {
        std::string parentPath = currentLevel["parent"].get<std::string>();

        if (parentPath.find("builtin/") != std::string::npos) break;

        //"minecraft:block/cube_all"
        std::string ns = "minecraft";
        std::string path = parentPath;
        size_t colon = parentPath.find(':');
        if (colon != std::string::npos) {
            ns = parentPath.substr(0, colon);
            path = parentPath.substr(colon + 1);
        }

        std::string parentUrl = "/api/client/assets/get/" + ns + "/models/" + path + ".json";
        std::string parentData;

        if (client.sendHttpRequest("GET", parentUrl, "", parentData)) {
            try {
                auto parentJson = nlohmann::json::parse(parentData);

                if (parentJson.contains("textures")) {
                    if (!modelJson.contains("textures")) {
                        modelJson["textures"] = parentJson["textures"];
                    } else {
                        for (auto& [key, val] : parentJson["textures"].items()) {
                            if (!modelJson["textures"].contains(key)) {
                                modelJson["textures"][key] = val;
                            }
                        }
                    }
                }

                if (!modelJson.contains("elements") && parentJson.contains("elements")) {
                    modelJson["elements"] = parentJson["elements"];
                }

                currentLevel = parentJson;
                recursionDepth++;

            } catch (...) {
                std::cerr << "Error parsing parent JSON: " << parentPath << std::endl;
                break;
            }
        } else {
            std::cerr << "Failed to download parent model: " << parentUrl << std::endl;
            break;
        }
    }

    if (modelJson.contains("textures")) {
        for (auto& [key, path] : modelJson["textures"].items()) {
            std::string texturePath = path.get<std::string>();
            
            int refLimit = 0;
            while (!texturePath.empty() && texturePath[0] == '#' && refLimit < 10) {
                std::string ref = texturePath.substr(1);
                if (modelJson["textures"].contains(ref)) {
                    texturePath = modelJson["textures"][ref];
                } else {
                    break; 
                }
                refLimit++;
            }
            
            if (uniqueTexturePaths.find(texturePath) == uniqueTexturePaths.end()) {
                uniqueTexturePaths.insert(texturePath);
                
                std::string namespace_id = texturePath;
                size_t colon = namespace_id.find(':');
                std::string ns = (colon == std::string::npos) ? "minecraft" : namespace_id.substr(0, colon);
                std::string p = (colon == std::string::npos) ? namespace_id : namespace_id.substr(colon + 1);
                
                std::string pngUrl = "/api/client/assets/get/" + ns + "/textures/" + p + ".png";
                std::string imgData;
                
                TextureAnimation animData;
                
                if (client.sendHttpRequest("GET", pngUrl, "", imgData)) {
                    sf::Image img;
                    if(img.loadFromMemory(imgData.data(), imgData.size()))
                    {
                        unsigned int w = img.getSize().x;
                        unsigned int h = img.getSize().y;
                        animData.frameHeight = w; 
                        
                        if (h > w) {
                            animData.isAnimated = true;
                            std::string metaUrl = pngUrl + ".mcmeta";
                            std::string metaData;
                            if (client.sendHttpRequest("GET", metaUrl, "", metaData)) {
                                try {
                                    auto metaJson = nlohmann::json::parse(metaData);
                                    if (metaJson.contains("animation")) {
                                        auto& anim = metaJson["animation"];
                                        animData.defaultFrameTime = anim.value("frametime", 1);
                                        if (anim.contains("frames") && anim["frames"].is_array()) {
                                            for (const auto& frameElement : anim["frames"]) {
                                                AnimationFrame frameInfo;
                                                if (frameElement.is_number()) {
                                                    frameInfo.index = frameElement.get<int>();
                                                    frameInfo.time = animData.defaultFrameTime;
                                                } else if (frameElement.is_object()) {
                                                    frameInfo.index = frameElement.value("index", 0);
                                                    frameInfo.time = frameElement.value("time", animData.defaultFrameTime);
                                                }
                                                animData.sequence.emplace_back(frameInfo);
                                            }
                                        }
                                        animData.isAnimated = true;
                                    }
                                } catch (...) {}
                            }
                        }
                        
                        animData.texture.size.x = w;
                        animData.texture.size.y = h;
                        const unsigned char* rawPixels = img.getPixelsPtr();
                        size_t totalBytes = w * h * 4;
                        if(rawPixels && totalBytes > 0)
                        {
                            animData.texture.pixels.assign(rawPixels, rawPixels + totalBytes);
                        } 
                    }
                }
                textures[texturePath] = animData;
            }
        }
    }
}

ModelGenerator::ModelGenerator(const std::string& objData, const std::string& mtlData, KubeJSClient& client, const std::string& checkNamespace, const std::string& id) {
    isObjModel = true;
    this->id = id;
    std::string warn;
    std::string err;

    std::istringstream objStream(objData);

    class MemMaterialReader : public tinyobj::MaterialReader {
    public:
        std::string mtlData;
        MemMaterialReader(std::string data) : mtlData(data) {}
        bool operator()(const std::string& matId, std::vector<tinyobj::material_t>* materials,
                        std::map<std::string, int>* matMap, std::string* warn, std::string* err) override {
            std::istringstream mtlStream(mtlData);
            tinyobj::LoadMtl(matMap, materials, &mtlStream, warn, err);
            int a = matId.size()+1;// Just to delete the warning
            if(a) return true;
            return true;
        }
    };
    MemMaterialReader matReader(mtlData);

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &objStream, &matReader);

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!err.empty()) std::cerr << "ERR: " << err << std::endl;
    if (!ret) return;

    for (const auto& mat : materials) {
        std::string texName = mat.diffuse_texname;
        if (texName.empty()) continue;
        std::string ns = checkNamespace;
        std::string path = texName;

        size_t colonPos = texName.find(':');
        if (colonPos != std::string::npos) {
            ns = texName.substr(0, colonPos);
            path = texName.substr(colonPos + 1);
        }

        size_t dotPos = path.rfind('.');
        if (dotPos != std::string::npos && path.substr(dotPos) == ".png") {
            path = path.substr(0, dotPos);
        }

        std::vector<std::string> pathsToTry;

        if (path.find("textures/") == 0) {
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/" + path + ".png");
        } 
        else {
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/textures/" + path + ".png");
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/textures/block/" + path + ".png");
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/textures/item/" + path + ".png");
            if (ns != "minecraft") {
                pathsToTry.emplace_back("/api/client/assets/get/minecraft/textures/block/" + path + ".png");
            }
        }

        bool loaded = false;
        std::string imgData;

        for(const auto& url : pathsToTry) {
            
            if(client.sendHttpRequest("GET", url, "", imgData)) {
                if (imgData.empty() || imgData.find("error") != std::string::npos) continue;

                sf::Image img;
                if (img.loadFromMemory(imgData.data(), imgData.size())) {
                    TextureAnimation animData;
                    animData.texture.size.x = img.getSize().x;
                    animData.texture.size.y = img.getSize().y;
                    const unsigned char* rawPixels = img.getPixelsPtr();
                    size_t totalBytes = img.getSize().x * img.getSize().y * 4;
                    if(rawPixels && totalBytes > 0)
                    {
                        animData.texture.pixels.assign(rawPixels, rawPixels + totalBytes);
                    }
                    
                    animData.frameHeight = img.getSize().y;
                    textures[texName] = animData;
                    
                    loaded = true;
                    break;
                }
            }
        }

        if(!loaded) {
            std::cerr << "Failed to load texture for OBJ: " << texName << std::endl;
        }
    }
}

int ModelGenerator::calculateTotalLoopTicks() {
    int totalTicks = 1;
    for (const auto& [path, data] : textures) {
        if (data.isAnimated) {
            int duration = data.getTotalDuration();
            if (duration > 0) totalTicks = std::lcm(totalTicks, duration);
        }
    }
    return totalTicks;
}

inline sf::Vector3f rotatePoint(sf::Vector3f point, sf::Vector3f origin, std::string axis, float angle) {
    if (angle == 0.0f) return point;
    float rad = angle * (std::numbers::pi_v<float> / 180.0f);
    float c = cos(rad);
    float s = sin(rad);
    
    float x = point.x - origin.x;
    float y = point.y - origin.y;
    float z = point.z - origin.z;
    
    float nx = x, ny = y, nz = z;
    
    if (axis == "x") {
        ny = y * c - z * s;
        nz = y * s + z * c;
    } else if (axis == "y") {
        nx = x * c - z * s;
        nz = x * s + z * c;
    } else if (axis == "z") {
        nx = x * c - y * s;
        ny = x * s + y * c;
    }
    
    return {nx + origin.x, ny + origin.y, nz + origin.z};
}

void drawTriangle(sf::Image& target, const CustomVertex& v0, const CustomVertex& v1, const CustomVertex& v2, const Texture* tex) {
    unsigned int minX = std::max(0, (int)std::min({v0.position.x, v1.position.x, v2.position.x}));
    unsigned int maxX = std::min((int)target.getSize().x - 1, (int)std::max({v0.position.x, v1.position.x, v2.position.x}));
    unsigned int minY = std::max(0, (int)std::min({v0.position.y, v1.position.y, v2.position.y}));
    unsigned int maxY = std::min((int)target.getSize().y - 1, (int)std::max({v0.position.y, v1.position.y, v2.position.y}));

    auto edgeFunction = [](const sf::Vector2f& a, const sf::Vector2f& b, const sf::Vector2f& c) {
        return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
    };

    float area = edgeFunction(v0.position, v1.position, v2.position);
    if (std::abs(area) < 0.00001f) return; // Evitar división por 0

    for (unsigned int y = minY; y <= maxY; ++y) {
        for (unsigned int x = minX; x <= maxX; ++x) {
            sf::Vector2f p(x + 0.5f, y + 0.5f);
            float w0 = edgeFunction(v1.position, v2.position, p);
            float w1 = edgeFunction(v2.position, v0.position, p);
            float w2 = edgeFunction(v0.position, v1.position, p);

            // Si el pixel está dentro del triángulo
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                w0 /= area; w1 /= area; w2 /= area;
                
                // Interpolar Coordenadas UV y Color
                float u = w0 * v0.texCoords.x + w1 * v1.texCoords.x + w2 * v2.texCoords.x;
                float v = w0 * v0.texCoords.y + w1 * v1.texCoords.y + w2 * v2.texCoords.y;
                float r = w0 * v0.color.r + w1 * v1.color.r + w2 * v2.color.r;
                float g = w0 * v0.color.g + w1 * v1.color.g + w2 * v2.color.g;
                float b = w0 * v0.color.b + w1 * v1.color.b + w2 * v2.color.b;
                
                if (tex && !tex->pixels.empty()) {
                    int texX = (int)u % tex->size.x; if (texX < 0) texX += tex->size.x;
                    int texY = (int)v % tex->size.y; if (texY < 0) texY += tex->size.y;
                    
                    int texIndex = (texY * tex->size.x + texX) * 4;
                    uint8_t texR = tex->pixels[texIndex];
                    uint8_t texG = tex->pixels[texIndex + 1];
                    uint8_t texB = tex->pixels[texIndex + 2];
                    uint8_t texA = tex->pixels[texIndex + 3];
                    
                    if (texA > 0) { // Alpha blending simple
                        sf::Color bg = target.getPixel({x, y});
                        float alpha = texA / 255.0f;
                        uint8_t finalR = (texR * r / 255.0f) * alpha + bg.r * (1.0f - alpha);
                        uint8_t finalG = (texG * g / 255.0f) * alpha + bg.g * (1.0f - alpha);
                        uint8_t finalB = (texB * b / 255.0f) * alpha + bg.b * (1.0f - alpha);
                        uint8_t finalA = std::max((uint8_t)bg.a, texA);
                        target.setPixel({x, y}, sf::Color(finalR, finalG, finalB, finalA));
                    }
                } else {
                    target.setPixel({x, y}, sf::Color(r, g, b, 255));
                }
            }
        }
    }
}

inline sf::Vector3f project3DTransform(float x, float y, float z, float pitch, float yaw, float scale, float centerx, float centery, float pivotX = 8.0f, float pivotY = 8.0f, float pivotZ = 8.0f)
{
    x -= pivotX; 
    y -= pivotY; 
    z -= pivotZ;

    float pitchRad = pitch * (std::numbers::pi_v<float> / 180.0f);
    float yawRad = yaw * (std::numbers::pi_v<float> / 180.0f);

    //Yaw
    float x1 = x * cos(yawRad) - z * sin(yawRad);
    float z1 = x * sin(yawRad) + z * cos(yawRad);
    float y1 = y;

    //Pitch
    float x2 = x1;
    float y2 = y1 * cos(pitchRad) - z1 * sin(pitchRad);
    float z2 = y1 * sin(pitchRad) + z1 * cos(pitchRad);

    return {centerx + x2 * scale, centery - y2 * scale, z2};
}

inline sf::Vector2f toIso(float x, float y, float z, float scale, float centerx, float centery)
{
    const float cos30 = 0.866025f;
    const float sin30 = 0.5f;
    
    float isoX = (x - z) * cos30 * scale;
    float isoY = ((x + z) * sin30 - y) * scale;
    
    return {centerx + isoX, centery + isoY};
}

std::vector<sf::Image> ModelGenerator::generateIsometricSequence(unsigned int outputSize, bool customRotation, float pitch, float yaw) {
    //We need to find out if its an item or not first
    bool isFlatItem = false;
    if (modelJson.contains("parent") && modelJson["parent"].get<std::string>().find("item/generated") != std::string::npos) isFlatItem = true;
    if (modelJson.contains("textures") && modelJson["textures"].contains("layer0")) isFlatItem = true;

    if(modelJson.contains("loader"))
    {
        std::vector<sf::Image> temp;
        std::string response = "";
        temp.emplace_back(client.getPreview(this->id, outputSize, isFlatItem ? TypeElement::ITEM : TypeElement::BLOCK, false));
        return temp;
    }

    if (!modelJson.contains("elements")) {
        if (isFlatItem) {
            nlohmann::json flatItem;
            flatItem["from"] = {0, 0, 0}; flatItem["to"] = {16, 16, 0}; 
            nlohmann::json faces;
            faces["north"] = { {"texture", "#layer0"}, {"uv", {0, 0, 16, 16}} };
            flatItem["faces"] = faces;
            modelJson["elements"] = nlohmann::json::array({flatItem});
        }
        else {
            nlohmann::json defaultCube;
            defaultCube["from"] = {0, 0, 0}; defaultCube["to"] = {16, 16, 16};
            nlohmann::json faces;
            
            auto findTexture = [&](const std::string& dir, const std::vector<std::string>& fallbacks) -> std::string {
                if (modelJson["textures"].contains(dir)) return "#" + dir;
                for (const auto& key : fallbacks) {
                    if (modelJson["textures"].contains(key)) return "#" + key;
                }
                if (modelJson["textures"].contains("all")) return "#all";
                return ""; 
            };

            std::string t_up    = findTexture("up",    {"top"});
            std::string t_down  = findTexture("down",  {"bottom"});
            std::string t_north = findTexture("north", {"front", "side"});
            std::string t_south = findTexture("south", {"back", "side"});
            std::string t_east  = findTexture("east",  {"side"});
            std::string t_west  = findTexture("west",  {"side"});

            if(!t_up.empty())    faces["up"]    = { {"texture", t_up} };
            if(!t_down.empty())  faces["down"]  = { {"texture", t_down} };
            if(!t_north.empty()) faces["north"] = { {"texture", t_north} };
            if(!t_south.empty()) faces["south"] = { {"texture", t_south} };
            if(!t_east.empty())  faces["east"]  = { {"texture", t_east} };
            if(!t_west.empty())  faces["west"]  = { {"texture", t_west} };

            defaultCube["faces"] = faces;
            modelJson["elements"] = nlohmann::json::array({defaultCube});
        }
    }

    std::vector<sf::Image> resultFrames;
    int totalTicks = calculateTotalLoopTicks();
    
    sf::Image renderTarget({outputSize, outputSize}, sf::Color::Transparent);

    float scale, centerX, centerY;
    if (isFlatItem) {
        scale = (float)outputSize / 16.0f;
        centerX = 0; centerY = 0;
    } else {
        scale = (float)outputSize / 38.0f;
        centerX = outputSize / 2.0f;
        centerY = (outputSize / 2.0f) + (scale * 2.0f);
    }

    auto isBackFace = [](const sf::Vector2f& p0, const sf::Vector2f& p1, const sf::Vector2f& p2) {
        return ((p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x)) < 0; 
    };

    for (int tick = 0; tick < totalTicks; ++tick) {
        renderTarget = sf::Image({outputSize, outputSize}, sf::Color::Transparent);
        std::vector<RenderQuad> renderQueue;

        if (modelJson.contains("elements")) {
            for (const auto& element : modelJson["elements"]) {
                auto from = element["from"];
                auto to = element["to"];
                
                sf::Vector3f pMin(from[0], from[1], from[2]);
                sf::Vector3f pMax(to[0], to[1], to[2]);

                sf::Vector3f rotOrigin(8, 8, 8);
                std::string rotAxis = "y";
                float rotAngle = 0;
                if (element.contains("rotation")) {
                    auto& rot = element["rotation"];
                    rotOrigin = {rot["origin"][0], rot["origin"][1], rot["origin"][2]};
                    rotAxis = rot.value("axis", "y");
                    rotAngle = rot.value("angle", 0.0f);
                }

                struct FaceDef { std::string dir; sf::Vector3f v[4]; };
                FaceDef faces[] = {
                    {"up",    {{pMin.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMax.z}, {pMin.x, pMax.y, pMax.z}}},
                    {"north", {{pMax.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMin.z}, {pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}}},
                    {"east",  {{pMax.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}}},
                    {"west",  {{pMin.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMax.z}, {pMin.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMin.z}}},
                    {"south", {{pMin.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMax.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}},
                    {"down",  {{pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}}
                };

                for (const auto& f : faces) {
                    if (!element["faces"].contains(f.dir)) continue;
                    auto faceJson = element["faces"][f.dir];
                    
                    std::string texRef = faceJson.value("texture", "");
                    if (texRef.empty()) continue;
                    
                    int depth = 0; 
                    if (texRef[0] == '#') {
                        std::string key = texRef.substr(1);
                        while(modelJson["textures"].contains(key) && depth < 5) {
                            texRef = modelJson["textures"][key];
                            if(texRef[0] == '#') key = texRef.substr(1);
                            else break;
                            depth++;
                        }
                    }

                    if (textures.find(texRef) == textures.end()) continue;
                    TextureAnimation& anim = textures[texRef];

                    int currentFrameIndex = 0;
                    if (anim.isAnimated && anim.getTotalDuration() > 0) {
                        int localTick = tick % anim.getTotalDuration();
                        if (anim.sequence.empty()) {
                            currentFrameIndex = (localTick / anim.defaultFrameTime) % (anim.texture.getSize().y / anim.frameHeight);
                        } else {
                            int acc = 0;
                            for (const auto& fr : anim.sequence) {
                                acc += fr.time;
                                if (localTick < acc) { currentFrameIndex = fr.index; break; }
                            }
                        }
                    }

                    float frameOffset = currentFrameIndex * anim.frameHeight;
                    float texW = (float)anim.texture.getSize().x;
                    float k = texW / 16.0f;
                    sf::Vector2f uv[4];

                    if (faceJson.contains("uv")) {
                        
                        float u1 = faceJson["uv"][0], v1 = faceJson["uv"][1];
                        float u2 = faceJson["uv"][2], v2 = faceJson["uv"][3];
                        
                        uv[0] = {u1 * k, v1 * k + frameOffset};
                        uv[1] = {u2 * k, v1 * k + frameOffset};
                        uv[2] = {u2 * k, v2 * k + frameOffset};
                        uv[3] = {u1 * k, v2 * k + frameOffset};
                    } else {
                        
                        for(int i=0; i<4; i++) {
                            float ux, vy;
                            
                            if (f.dir == "up" || f.dir == "down") {
                                ux = f.v[i].x; 
                                vy = f.v[i].z;
                            } else if (f.dir == "north" || f.dir == "south") {
                                ux = f.v[i].x; 
                                vy = 16.0f - f.v[i].y;
                            } else { //east/west
                                ux = f.v[i].z; 
                                vy = 16.0f - f.v[i].y;
                            }
                            
                            uv[i] = {ux * k, vy * k + frameOffset};
                        }
                    }

                    sf::Vector2f p[4];
                    float avgZ = 0;
                    
                    if (isFlatItem) {
                        p[0] = {f.v[0].x * scale, (16.0f - f.v[0].y) * scale}; 
                        p[1] = {f.v[1].x * scale, (16.0f - f.v[1].y) * scale};
                        p[2] = {f.v[2].x * scale, (16.0f - f.v[2].y) * scale};
                        p[3] = {f.v[3].x * scale, (16.0f - f.v[3].y) * scale};
                    } else {
                        sf::Vector3f rotV[4];
                        for(int i=0; i<4; i++) {
                            rotV[i] = rotatePoint(f.v[i], rotOrigin, rotAxis, rotAngle);
                            
                            if (customRotation) {
                                sf::Vector3f proj = project3DTransform(rotV[i].x, rotV[i].y, rotV[i].z, pitch, yaw, scale, centerX, centerY, 8.0f, 8.0f, 8.0f);
                                p[i] = {proj.x, proj.y};
                                avgZ += proj.z; // Usamos el Z transformado para el Painter's Algorithm
                            } else {
                                p[i] = toIso(rotV[i].x, rotV[i].y, rotV[i].z, scale, centerX, centerY);
                                avgZ += (rotV[i].x + rotV[i].y + rotV[i].z);
                            }
                        }
                        avgZ /= 4.0f;
                        if(!customRotation)
                        {
                            if(isBackFace(p[0], p[1], p[2])) continue; 
                        }
                    }

                    sf::Color color = sf::Color::White;
                    if (!isFlatItem) {
                        if (f.dir == "west" || f.dir == "east") color = sf::Color(200, 200, 200);
                        else if (f.dir == "south" || f.dir == "north") color = sf::Color(150, 150, 150);
                        else if (f.dir == "down") color = sf::Color(100, 100, 100);
                    }

                    RenderQuad q;
                    q.vertices = {
                        CustomVertex{p[0], color, uv[0]}, CustomVertex{p[1], color, uv[1]},
                        CustomVertex{p[2], color, uv[2]}, CustomVertex{p[2], color, uv[2]},
                        CustomVertex{p[3], color, uv[3]}, CustomVertex{p[0], color, uv[0]}
                    };
                    q.texture = &anim.texture;
                    q.depth = avgZ;
                    renderQueue.emplace_back(q);
                }
            }
        }

        if (!isFlatItem) {
            std::stable_sort(renderQueue.begin(), renderQueue.end(), [](const RenderQuad& a, const RenderQuad& b) {
                return a.depth < b.depth;
            });
        }

        for (const auto& q : renderQueue) {
            for (size_t i = 0; i < q.vertices.size(); i += 3) { // Procesamos 2 triángulos por Face
                drawTriangle(renderTarget, q.vertices[i], q.vertices[i+1], q.vertices[i+2], q.texture);
            }
        }
        
        sf::Image frameImg = renderTarget;
        frameImg.flipHorizontally(); 
        resultFrames.emplace_back(frameImg);

        bool anyAnimated = false;
        for(auto& [k, v] : textures) if(v.isAnimated) anyAnimated = true;
        if(!anyAnimated) break;
    }
    return resultFrames;
}

inline std::string cleanTextureName(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos) {
        path = path.substr(lastSlash + 1);
    }
    size_t lastDot = path.find_last_of('.');
    if (lastDot != std::string::npos) {
        path = path.substr(0, lastDot);
    }
    return path;
}

std::vector<sf::Image> ModelGenerator::generateIsometricSequenceOBJ(unsigned int outputSize, bool customRotation, float pitch, float yaw) {
    float minIsoX = 1e9, maxIsoX = -1e9;
    float minIsoY = 1e9, maxIsoY = -1e9;
    bool hasVertices = false;

    const float cos30 = 0.866025f;
    const float sin30 = 0.5f;

    auto getProjected = [&](float vx, float vy, float vz) -> sf::Vector3f {
        if (customRotation) {
            // El pivote de un OBJ asume el origen de coordenadas (0,0,0)
            return project3DTransform(vx, vy, vz, pitch, yaw, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        } else {
            float rx = (vx - vz) * 0.866025f;
            float ry = (vx + vz) * 0.5f - vy;
            return {rx, ry, vx + vy + vz};
        }
    };

    for (size_t i = 0; i < attrib.vertices.size() / 3; i++) {
        float vx = attrib.vertices[3 * i + 0];
        float vy = attrib.vertices[3 * i + 1];
        float vz = attrib.vertices[3 * i + 2];
        
        sf::Vector3f proj = getProjected(vx, vy, vz);

        if (proj.x < minIsoX) minIsoX = proj.x;
        if (proj.x > maxIsoX) maxIsoX = proj.x;
        if (proj.y < minIsoY) minIsoY = proj.y;
        if (proj.y > maxIsoY) maxIsoY = proj.y;

        hasVertices = true;
    }

    if (!hasVertices) return {};

    float contentWidth = maxIsoX - minIsoX;
    float contentHeight = maxIsoY - minIsoY;

    if (contentWidth < 0.001f) contentWidth = 1.0f;
    if (contentHeight < 0.001f) contentHeight = 1.0f;

    float padding = 0.85f; 
    float scaleX = (outputSize * padding) / contentWidth;
    float scaleY = (outputSize * padding) / contentHeight;
    float finalScale = std::min(scaleX, scaleY);
    float isoCenterX = (minIsoX + maxIsoX) / 2.0f;
    float isoCenterY = (minIsoY + maxIsoY) / 2.0f;
    float screenCenterX = outputSize / 2.0f;
    float screenCenterY = outputSize / 2.0f;


    auto projectToScreen = [&](float vx, float vy, float vz) -> sf::Vector3f {
        sf::Vector3f proj = getProjected(vx, vy, vz);
        float centeredX = proj.x - isoCenterX;
        float centeredY = proj.y - isoCenterY;

        return {
            screenCenterX + (centeredX * finalScale),
            screenCenterY + (centeredY * finalScale),
            proj.z
        };
    };

    sf::Image renderTarget({outputSize, outputSize}, sf::Color::Transparent);

    std::vector<RenderQuad> renderQueue;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];
            RenderQuad poly;
            float avgDepth = 0;

            for (size_t v = 0; v < static_cast<size_t>(fv); v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                
                float vx = attrib.vertices[3 * idx.vertex_index + 0];
                float vy = attrib.vertices[3 * idx.vertex_index + 1];
                float vz = attrib.vertices[3 * idx.vertex_index + 2];

                sf::Vector3f screenProj = projectToScreen(vx, vy, vz);
                sf::Vector2f isoPos = {screenProj.x, screenProj.y};
                avgDepth += screenProj.z; 

                CustomVertex vert;
                vert.position = isoPos;
                vert.color = sf::Color::White;
                poly.vertices.emplace_back(vert);
            }
            poly.depth = avgDepth / fv;

            int matId = shape.mesh.material_ids[f];
            if (matId >= 0 && static_cast<size_t>(matId) < materials.size()) {
                std::string rawTexName = materials[matId].diffuse_texname;
                Texture* foundTex = nullptr;

                if (textures.count(rawTexName)) {
                    foundTex = &textures[rawTexName].texture;
                } else {
                    std::string cleanName = cleanTextureName(rawTexName);
                    for(auto& [key, val] : textures) {
                        if (cleanTextureName(key) == cleanName) {
                            foundTex = &val.texture;
                            break;
                        }
                    }
                }

                if (foundTex) {
                    poly.texture = foundTex;
                    float texW = (float)foundTex->getSize().x;
                    float texH = (float)foundTex->getSize().y;

                    for(size_t v = 0; v < static_cast<size_t>(fv) && v < poly.vertices.size(); v++) {
                        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                        if (idx.texcoord_index >= 0) {
                            float u = attrib.texcoords[2 * idx.texcoord_index + 0];
                            float v_coord = attrib.texcoords[2 * idx.texcoord_index + 1];
                            poly.vertices[v].texCoords = sf::Vector2f(u * texW, (1.0f - v_coord) * texH);
                        }
                    }
                } else {
                    for(auto& v : poly.vertices) v.color = sf::Color(150, 150, 150);
                    poly.texture = nullptr;
                }
            }

            if (poly.vertices.size() >= 3) renderQueue.emplace_back(poly);
            index_offset += fv;
        }
    }

    std::sort(renderQueue.begin(), renderQueue.end(), [](const RenderQuad& a, const RenderQuad& b) {
        return a.depth < b.depth;
    });

    for (const auto& q : renderQueue) {
        // Simular un TriangleFan
        for (size_t i = 1; i + 1 < q.vertices.size(); i++) {
            drawTriangle(renderTarget, q.vertices[0], q.vertices[i], q.vertices[i+1], q.texture);
        }
    }

    //sf::Image finalImg = renderTarget;
    return { renderTarget };
}

void ModelGenerator::saveAssets(const std::string& itemId, bool customRotation, float pitch, float yaw) {
    std::string safeName = changeFilename(itemId);
    std::string targetDir = "img/" + safeName;
    
    if(!fs::exists(targetDir))
    {
        fs::create_directories(targetDir);
    }

    std::ofstream jsonFile(targetDir + "/model.json");
    jsonFile << std::setw(4) << modelJson << std::endl;
    jsonFile.close();

    for(const auto& [path, animData] : textures)
    {
        if(animData.texture.getSize().x == 0) continue;
        
        std::string texName = changeFilename(path) + ".png";
        sf::Image img = animData.texture.copyToImage();
        if(!img.saveToFile(targetDir + "/" + texName)) std::cerr << "Error loading image" << std::endl;
    }

    bool isItem = false;
    if(modelJson.contains("parent") && modelJson["parent"].get<std::string>().find("item/generated") != std::string::npos) isItem = true;
    if(modelJson.contains("textures") && modelJson["textures"].contains("layer0")) isItem = true;
    if(!isItem) exportToObj(itemId, targetDir);
    if(isObjModel)
    {
        saveAnimationWebP(itemId, targetDir, generateIsometricSequenceOBJ(128, customRotation, pitch, yaw));
    }
    else
    {
        saveAnimationWebP(itemId, targetDir, generateIsometricSequence(128, customRotation, pitch, yaw));
    }
    
}

void ModelGenerator::saveAnimationWebP(const std::string& itemId, const std::string& outputDir, const std::vector<sf::Image>& frames)
{
    if(frames.empty()) return;

    std::string baseName = changeFilename(itemId);
    std::string filename = baseName + ".webp";
    std::string fullPath = outputDir + "/" + filename;

    int width = frames[0].getSize().x;
    int height = frames[0].getSize().y;

    WebPAnimEncoderOptions enc_options;
    WebPAnimEncoderOptionsInit(&enc_options);
    
    WebPAnimEncoder* enc = WebPAnimEncoderNew(width, height, &enc_options);
    if(!enc)
    {
        std::cerr << "WebP: Failed to create encoder" << std::endl;
        return;
    }

    int timestamp_ms = 0;
    int frame_duration = 50; // 1 tick (0.05s)

    for(const auto& img : frames)
    {
        WebPConfig config;
        WebPConfigInit(&config);
        config.lossless = 1;

        WebPPicture pic;
        WebPPictureInit(&pic);
        pic.width = width;
        pic.height = height;
        pic.use_argb = 1;
        
        if(!WebPPictureAlloc(&pic))
        {
            WebPAnimEncoderDelete(enc);
            return;
        }

        const uint8_t* pixels = img.getPixelsPtr();
        WebPPictureImportRGBA(&pic, pixels, width * 4);

        if(!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config))
        {
            std::cerr << "WebP: Error adding frame" << std::endl;
            WebPPictureFree(&pic);
            WebPAnimEncoderDelete(enc);
            return;
        }

        WebPPictureFree(&pic);
        timestamp_ms += frame_duration;
    }

    WebPAnimEncoderAdd(enc, nullptr, timestamp_ms, nullptr);

    WebPData webp_data;
    WebPDataInit(&webp_data);
    WebPAnimEncoderAssemble(enc, &webp_data);

    if(!fs::exists(outputDir))
    {
        fs::create_directories(outputDir);
    }

    std::ofstream file(fullPath, std::ios::binary);
    if(file.is_open())
    {
        file.write(reinterpret_cast<const char*>(webp_data.bytes), webp_data.size);
        file.close();
    }

    WebPDataClear(&webp_data);
    WebPAnimEncoderDelete(enc);
}

void ModelGenerator::exportToObj(const std::string& itemId, const std::string& outputDir) {
    std::string baseName = changeFilename(itemId);
    std::string objFilename = outputDir + "/" + baseName + ".obj";
    std::string mtlFilename = outputDir + "/" + baseName + ".mtl";
    std::string mtlBaseName = baseName + ".mtl";

    std::ofstream objFile(objFilename);
    std::ofstream mtlFile(mtlFilename);

    if (!objFile.is_open() || !mtlFile.is_open()) return;

    objFile << "# Exported using QuestiMakinator\n";
    objFile << "mtllib " << mtlBaseName << "\n";

    int vertexCount = 0;
    int uvCount = 0;
     
    std::set<std::string> usedMaterials;

    if (modelJson.contains("elements")) {
        for (const auto& element : modelJson["elements"]) {
            auto from = element["from"];
            auto to = element["to"];
             
            sf::Vector3f pMin(from[0], from[1], from[2]);
            sf::Vector3f pMax(to[0], to[1], to[2]);
            sf::Vector3f rotOrigin(8, 8, 8);

            std::string rotAxis = "y";
            float rotAngle = 0;
            if (element.contains("rotation")) {
                auto& rot = element["rotation"];
                rotOrigin = {rot["origin"][0], rot["origin"][1], rot["origin"][2]};
                rotAxis = rot.value("axis", "y");
                rotAngle = rot.value("angle", 0.0f);
            }

            struct FaceDef { std::string dir; sf::Vector3f v[4]; sf::Vector3f normal; };
            FaceDef faces[] = {
                {"up",    {{pMin.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMax.z}, {pMin.x, pMax.y, pMax.z}}, {0,1,0}},
                {"down",  {{pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}, {0,-1,0}},
                {"north", {{pMax.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMin.z}, {pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}}, {0,0,-1}},
                {"south", {{pMin.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMax.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}, {0,0,1}},
                {"west",  {{pMin.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMax.z}, {pMin.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMin.z}}, {-1,0,0}},
                {"east",  {{pMax.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}}, {1,0,0}}
            };

            for (const auto& f : faces) {
                if (!element["faces"].contains(f.dir)) continue;
                auto faceJson = element["faces"][f.dir];
                
                std::string texRef = faceJson.value("texture", "");
                if (texRef.empty()) continue;

                int depth = 0;
                std::string resolvedPath = texRef;
                while (!resolvedPath.empty() && resolvedPath[0] == '#' && depth < 5) {
                    std::string key = resolvedPath.substr(1);
                    if (modelJson["textures"].contains(key)) resolvedPath = modelJson["textures"][key];
                    else break;
                    depth++;
                }
                if (resolvedPath.empty()) continue;

                std::string matName = changeFilename(resolvedPath);
                usedMaterials.insert(resolvedPath);
                objFile << "usemtl " << matName << "\n";

                // Vertices
                for (int i = 0; i < 4; i++) {
                    sf::Vector3f rv = rotatePoint(f.v[i], rotOrigin, rotAxis, rotAngle);
                    objFile << "v " << rv.x/16.0f << " " << rv.y/16.0f << " " << rv.z/16.0f << "\n";
                }

                // UVs
                float u1=0, v1=0, u2=16, v2=16;
                if (faceJson.contains("uv")) {
                    u1 = faceJson["uv"][0]; v1 = faceJson["uv"][1];
                    u2 = faceJson["uv"][2]; v2 = faceJson["uv"][3];
                } else {
                    if (f.dir == "up" || f.dir == "down") { u1=f.v[0].x; v1=f.v[0].z; u2=f.v[2].x; v2=f.v[2].z; }
                    else { u1=0; v1=0; u2=16; v2=16; } 
                }
                
                sf::Vector2f uvs[4];
                uvs[0] = {u1, v1}; uvs[1] = {u2, v1}; uvs[2] = {u2, v2}; uvs[3] = {u1, v2};

                for(int k=0; k<4; k++) {
                   objFile << "vt " << uvs[k].x/16.0f << " " << (16.0f - uvs[k].y)/16.0f << "\n";
                }

                int baseV = vertexCount + 1;
                int baseVT = uvCount + 1;
                
                objFile << "f " 
                        << baseV << "/" << baseVT << " "
                        << baseV+1 << "/" << baseVT+1 << " "
                        << baseV+2 << "/" << baseVT+2 << " "
                        << baseV+3 << "/" << baseVT+3 << "\n";

                vertexCount += 4;
                uvCount += 4;
            }
        }
    }
    objFile.close();

    for (const auto& mat : usedMaterials) {
        std::string matName = changeFilename(mat);
        std::string texFilename = matName + ".png"; 
        
        mtlFile << "newmtl " << matName << "\n";
        mtlFile << "Ka 1.000 1.000 1.000\n";
        mtlFile << "Kd 1.000 1.000 1.000\n";
        mtlFile << "d 1.0\n";
        mtlFile << "illum 2\n";
        mtlFile << "map_Kd " << texFilename << "\n\n";
    }
    mtlFile.close();

}