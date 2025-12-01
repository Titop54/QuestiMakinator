#include <cstddef>
#include <integration/model.h>
#include <iostream>
#include <cmath>
#include <numeric>
#include <set>
#include <fstream>
#include <webp/encode.h>
#include <webp/mux.h>

namespace fs = std::filesystem;


ModelGenerator::ModelGenerator(const std::string& rawJson, KubeJSClient& client) {
    try {
        modelJson = nlohmann::json::parse(rawJson);
    } catch(...) {
        return;
    }

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
                    if (img.loadFromMemory(imgData.data(), imgData.size())) {
                        
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
                                                animData.sequence.push_back(frameInfo);
                                            }
                                        }
                                        animData.isAnimated = true;
                                    }
                                } catch (...) {}
                            }
                        }
                        
                        if(!animData.texture.loadFromImage(img)) {
                            std::cout << "Error to load image\n" << std::endl; 
                        }
                        animData.texture.setSmooth(false); 
                    }
                }
                textures[texturePath] = animData;
            }
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

inline sf::Vector3f rotatePoint(sf::Vector3f point, sf::Vector3f origin, std::string axis, float angleDeg) {
    if (angleDeg == 0.0f) return point;
    float angleRad = angleDeg * (3.14159265f / 180.0f);
    float s = sin(angleRad);
    float c = cos(angleRad);
    sf::Vector3f p = point - origin;
    sf::Vector3f r;
    if (axis == "x") { r.x = p.x; r.y = p.y * c - p.z * s; r.z = p.y * s + p.z * c; }
    else if (axis == "y") { r.x = p.x * c - p.z * s; r.y = p.y; r.z = p.x * s + p.z * c; }
    else if (axis == "z") { r.x = p.x * c - p.y * s; r.y = p.x * s + p.y * c; r.z = p.z; }
    else return point;
    return r + origin;
}

inline sf::Vector2f toIso(float x, float y, float z, float scale, float centerX, float centerY) {
    float isoX = (x - z) * cos(0.523599f) * scale + centerX;
    float isoY = (x + z) * sin(0.523599f) * scale - (y * scale) + centerY;
    return {isoX, isoY};
}

struct RenderQuad {
    std::vector<sf::Vertex> vertices;
    const sf::Texture* texture;
    float depth;
};

std::vector<sf::Image> ModelGenerator::generateIsometricSequence(unsigned int outputSize) {
    
    //We need to find out if its an item or not first
    bool isFlatItem = false;
    if (modelJson.contains("parent") && modelJson["parent"].get<std::string>().find("item/generated") != std::string::npos) isFlatItem = true;
    if (modelJson.contains("textures") && modelJson["textures"].contains("layer0")) isFlatItem = true;

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
    
    sf::RenderTexture renderTex({outputSize, outputSize});

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
        renderTex.clear(sf::Color::Transparent);
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
                            p[i] = toIso(rotV[i].x, rotV[i].y, rotV[i].z, scale, centerX, centerY);
                            avgZ += (rotV[i].x + rotV[i].y + rotV[i].z);
                        }
                        avgZ /= 4.0f;
                        if (isBackFace(p[0], p[1], p[2])) continue; 
                    }

                    sf::Color color = sf::Color::White;
                    if (!isFlatItem) {
                        if (f.dir == "west" || f.dir == "east") color = sf::Color(200, 200, 200);
                        else if (f.dir == "south" || f.dir == "north") color = sf::Color(150, 150, 150);
                        else if (f.dir == "down") color = sf::Color(100, 100, 100);
                    }

                    RenderQuad q;
                    q.vertices = {
                        sf::Vertex{p[0], color, uv[0]}, sf::Vertex{p[1], color, uv[1]},
                        sf::Vertex{p[2], color, uv[2]}, sf::Vertex{p[2], color, uv[2]},
                        sf::Vertex{p[3], color, uv[3]}, sf::Vertex{p[0], color, uv[0]}
                    };
                    q.texture = &anim.texture;
                    q.depth = avgZ;
                    renderQueue.push_back(q);
                }
            }
        }

        if (!isFlatItem) {
            std::stable_sort(renderQueue.begin(), renderQueue.end(), [](const RenderQuad& a, const RenderQuad& b) {
                return a.depth < b.depth;
            });
        }

        for (const auto& q : renderQueue) {
            renderTex.draw(&q.vertices[0], q.vertices.size(), sf::PrimitiveType::Triangles, q.texture);
        }
        
        renderTex.display();
        resultFrames.push_back(renderTex.getTexture().copyToImage());

        bool anyAnimated = false;
        for(auto& [k, v] : textures) if(v.isAnimated) anyAnimated = true;
        if(!anyAnimated) break;
    }
    return resultFrames;
}

void ModelGenerator::saveAssets(const std::string& itemId) {
    std::string safeName = changeFilename(itemId);
    std::string targetDir = "img/" + safeName;
    
    if (!fs::exists(targetDir)) {
        fs::create_directories(targetDir);
    }
    std::ofstream jsonFile(targetDir + "/model.json");
    jsonFile << std::setw(4) << modelJson << std::endl;
    jsonFile.close();

    for (const auto& [path, animData] : textures) {
        if (animData.texture.getSize().x == 0) continue;
        
        std::string texName = changeFilename(path) + ".png";
        sf::Image img = animData.texture.copyToImage();
        img.saveToFile(targetDir + "/" + texName);
    }
    bool isItem = false;
    if (modelJson.contains("parent") && modelJson["parent"].get<std::string>().find("item/generated") != std::string::npos) isItem = true;
    if (modelJson.contains("textures") && modelJson["textures"].contains("layer0")) isItem = true;
    if(!isItem) exportToObj(itemId, targetDir);
    saveAnimationWebP(itemId, targetDir, generateIsometricSequence(128));
}


void ModelGenerator::saveAnimationWebP(const std::string& itemId, const std::string& outputDir, const std::vector<sf::Image>& frames) {
    if (frames.empty()) return;

    std::string baseName = changeFilename(itemId);
    std::string filename = baseName + ".webp";
    std::string fullPath = outputDir + "/" + filename;

    int width = frames[0].getSize().x;
    int height = frames[0].getSize().y;

    WebPAnimEncoderOptions enc_options;
    WebPAnimEncoderOptionsInit(&enc_options);
    
    WebPAnimEncoder* enc = WebPAnimEncoderNew(width, height, &enc_options);
    if (!enc) {
        std::cerr << "WebP: Failed to create encoder" << std::endl;
        return;
    }

    int timestamp_ms = 0;
    int frame_duration = 50; // 1 tick (0.05s)

    for (const auto& img : frames) {
        WebPConfig config;
        WebPConfigInit(&config);
        config.lossless = 1;

        WebPPicture pic;
        WebPPictureInit(&pic);
        pic.width = width;
        pic.height = height;
        pic.use_argb = 1;
        
        if (!WebPPictureAlloc(&pic)) {
            WebPAnimEncoderDelete(enc);
            return;
        }

        const uint8_t* pixels = img.getPixelsPtr();
        WebPPictureImportRGBA(&pic, pixels, width * 4);

        if (!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config)) {
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

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }
    std::ofstream file(fullPath, std::ios::binary);
    if (file.is_open()) {
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