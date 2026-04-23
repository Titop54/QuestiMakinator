#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Color.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
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
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

struct GpuVertex
{
    float x, y, z;
    float r, g, b, a; //Vertex color
    float u, v; //Texture coord
};

struct Triangle
{
    GpuVertex v[3];
    const Texture *texture;
    float depth;
};

static const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform vec2 uTexOffset;

out vec4 vColor;
out vec2 TexCoord;

void main()
{
    gl_Position = uProjection * vec4(aPos, 1.0);
    vColor = aColor;
    TexCoord = aTexCoord + uTexOffset;
}
)";

static const char *fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
in vec4 vColor;
in vec2 TexCoord;
uniform sampler2D uTexture;
uniform bool uWireframe;

void main()
{
    vec4 texColor = texture(uTexture, TexCoord);
    if(!uWireframe && texColor.a < 0.1) discard;
    FragColor = texColor * vColor;
}
)glsl";

static GLuint CompileShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if(!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Error compiling shader: " << infoLog << std::endl;
    }
    return shader;
}

static GLuint CreateShaderProgram()
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Error linking program: " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

inline sf::Vector3f rotatePoint(sf::Vector3f point, sf::Vector3f origin, std::string axis, float angle)
{
    if(angle == 0.0f) return point;
    float rad = angle * (std::numbers::pi_v<float> / 180.0f);
    float c = cos(rad);
    float s = sin(rad);

    float x = point.x - origin.x;
    float y = point.y - origin.y;
    float z = point.z - origin.z;

    float nx = x, ny = y, nz = z;

    if(axis == "x")
    {
        ny = y * c - z * s;
        nz = y * s + z * c;
    }
    else if(axis == "y")
    {
        nx = x * c - z * s;
        nz = x * s + z * c;
    }
    else if(axis == "z")
    {
        nx = x * c - y * s;
        ny = x * s + y * c;
    }

    return {nx + origin.x, ny + origin.y, nz + origin.z};
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

inline std::string cleanTextureName(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    size_t lastSlash = path.find_last_of('/');
    if(lastSlash != std::string::npos)
    {
        path = path.substr(lastSlash + 1);
    }

    size_t lastDot = path.find_last_of('.');
    if(lastDot != std::string::npos)
    {
        path = path.substr(0, lastDot);
    }
    return path;
}

inline sf::Vector2f toIso(float x, float y, float z, float scale, float centerx, float centery)
{
    const float cos30 = 0.866025f;
    const float sin30 = 0.5f;

    float isoX = (x - z) * cos30 * scale;
    float isoY = ((x + z) * sin30 - y) * scale;

    return {centerx + isoX, centery + isoY};
}

static sf::Color getFaceShading(const std::string &faceDir, bool isFlatItem)
{
    if(isFlatItem) return sf::Color::White;
    if(faceDir == "west" || faceDir == "east") return sf::Color(200, 200, 200);
    if(faceDir == "south" || faceDir == "north") return sf::Color(150, 150, 150);
    if(faceDir == "down") return sf::Color(100, 100, 100);
    return sf::Color::White;
}

static GLuint CreateGLTextureFromPixels(const std::vector<uint8_t> &pixels, int width, int height)
{
    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texID;
}

ModelGenerator::ModelGenerator(const std::string &rawJson, KubeJSClient &client, const std::string &id)
{
    try
    {
        modelJson = nlohmann::json::parse(rawJson);
    }
    catch(...)
    {
        return;
    }
    this->id = id;
    int recursionDepth = 0;
    nlohmann::json currentLevel = modelJson;

    if(modelJson.contains("loader") && (modelJson["loader"] == "neoforge:fluid_container" || modelJson["loader"] == "immersiveengineering:potion_bucket"))
    {
        std::string fluidId = "minecraft:water";
        if(modelJson.contains("fluid")) fluidId = modelJson["fluid"];
        
        modelJson["textures"]["layer1"] = fluidId; 
        modelJson["parent"] = "minecraft:item/generated";
        modelJson["textures"]["layer0"] = "minecraft:item/bucket";
        modelJson["is_fluid_container"] = true;
    }

    //we want all layers, but up to a limit
    while(currentLevel.contains("parent") && recursionDepth < 10)
    {
        std::string parentPath = currentLevel["parent"].get<std::string>();

        if(parentPath.find("builtin/") != std::string::npos) break;

        std::string ns = "minecraft";
        std::string path = parentPath;
        size_t colon = parentPath.find(':');

        if(colon != std::string::npos)
        {
            ns = parentPath.substr(0, colon);
            path = parentPath.substr(colon + 1);
        }

        std::string parentUrl = "/api/client/assets/get/" + ns + "/models/" + path + ".json";
        std::string parentData;

        if(client.sendHttpRequest("GET", parentUrl, "", parentData))
        {
            try
            {
                auto parentJson = nlohmann::json::parse(parentData);

                if(parentJson.contains("textures"))
                {
                    if(!modelJson.contains("textures"))
                    {
                        modelJson["textures"] = parentJson["textures"];
                    }
                    else
                    {
                        for(auto &[key, val] : parentJson["textures"].items())
                        {
                            if(!modelJson["textures"].contains(key))
                            {
                                modelJson["textures"][key] = val;
                            }
                        }
                    }
                }

                if(!modelJson.contains("elements") && parentJson.contains("elements"))
                {
                    modelJson["elements"] = parentJson["elements"];
                }

                currentLevel = parentJson;
                recursionDepth++;
            }
            catch(...)
            {
                std::cerr << "Error parsing parent JSON: " << parentPath << std::endl;
                break;
            }
        }
        else
        {
            std::cerr << "Failed to download parent model: " << parentUrl << std::endl;
            break;
        }
    }

    if(modelJson.contains("textures"))
    {
        for(auto &[key, path] : modelJson["textures"].items())
        {
            std::string texturePath = path.get<std::string>();

            int refLimit = 0;
            while(!texturePath.empty() && texturePath[0] == '#' && refLimit < 10)
            {
                std::string ref = texturePath.substr(1);
                if(modelJson["textures"].contains(ref))
                {
                    texturePath = modelJson["textures"][ref];
                }
                else
                {
                    break;
                }
                refLimit++;
            }

            if(uniqueTexturePaths.find(texturePath) == uniqueTexturePaths.end())
            {
                uniqueTexturePaths.insert(texturePath);

                std::string namespace_id = texturePath;
                size_t colon = namespace_id.find(':');
                std::string ns = (colon == std::string::npos) ? "minecraft" : namespace_id.substr(0, colon);
                std::string p = (colon == std::string::npos) ? namespace_id : namespace_id.substr(colon + 1);

                std::vector<std::string> pathsToTry;
                pathsToTry.push_back("/api/client/assets/get/" + ns + "/textures/" + p + ".png");
                pathsToTry.push_back("/api/client/assets/get/" + ns + "/textures/block/" + p + ".png");
                pathsToTry.push_back("/api/client/assets/get/" + ns + "/textures/fluid/" + p + "_still.png");
                pathsToTry.push_back("/api/client/assets/get/" + ns + "/textures/block/" + p + "_still.png");
                pathsToTry.push_back("/api/client/assets/get/" + ns + "/textures/block/fluids/" + p + "_still.png");
                pathsToTry.push_back("/api/client/assets/get/" + ns + "/textures/block/fluid/" + p + "_still.png");

                std::string imgData;
                bool loaded = false;
                std::string successfulUrl = "";
                TextureAnimation animData;

                for(const auto& pngUrl : pathsToTry)
                {
                    if(client.sendHttpRequest("GET", pngUrl, "", imgData))
                    {
                        if(imgData.empty() || imgData.find("error") != std::string::npos) continue;
                        successfulUrl = pngUrl;
                        loaded = true;
                        break;
                    }
                }

                if(!loaded)
                {
                    std::string assetListJson;
                    if(client.sendHttpRequest("GET", "/api/client/assets/list/textures/block", "", assetListJson))
                    {
                        try
                        {
                            auto list = nlohmann::json::parse(assetListJson);
                            if(list.contains(ns))
                            {
                                std::string bestMatch = "";
                                for(const auto& assetPath : list[ns])
                                {
                                    std::string pathStr = assetPath.get<std::string>();
                                    if(pathStr.find(p) != std::string::npos)
                                    {
                                        if(pathStr.find("still") != std::string::npos)
                                        {
                                            bestMatch = pathStr;
                                            break;
                                        }
                                        if(bestMatch.empty()) bestMatch = pathStr;
                                    }
                                }

                                if(!bestMatch.empty())
                                {
                                    successfulUrl = "/api/client/assets/get/" + ns + "/textures/block/" + bestMatch;
                                    if(client.sendHttpRequest("GET", successfulUrl, "", imgData))
                                    {
                                        if(!imgData.empty() && imgData.find("error") == std::string::npos)
                                        {
                                            loaded = true;
                                        }
                                    }
                                }
                            }
                        }
                        catch(...)
                        {}
                    }
                }

                if(loaded)
                {
                    sf::Image img;
                    if(img.loadFromMemory(imgData.data(), imgData.size()))
                    {
                        unsigned int w = img.getSize().x;
                        unsigned int h = img.getSize().y;

                        animData.texture.size.x = w;
                        animData.texture.size.y = h;
                        const unsigned char *rawPixels = img.getPixelsPtr();
                        size_t totalBytes = w * h * 4;

                        if(rawPixels && totalBytes > 0)
                        {
                            animData.texture.pixels.assign(rawPixels, rawPixels + totalBytes);
                        }

                        if(animData.texture.size.x > animData.texture.size.y)
                        {
                            animData.texture.rotate90Clockwise();
                            w = animData.texture.size.x;
                            h = animData.texture.size.y;
                        }

                        animData.frameHeight = w;

                        if(h > w)
                        {
                            animData.isAnimated = true;
                            std::string metaUrl = successfulUrl + ".mcmeta";
                            std::string metaData;
                            if(client.sendHttpRequest("GET", metaUrl, "", metaData))
                            {
                                animData.rawMcmeta = metaData;
                                try
                                {
                                    auto metaJson = nlohmann::json::parse(metaData);
                                    if(metaJson.contains("animation"))
                                    {
                                        auto &anim = metaJson["animation"];
                                        animData.defaultFrameTime = anim.value("frametime", 1);
                                        if(anim.contains("frames") && anim["frames"].is_array())
                                        {
                                            for(const auto &frameElement : anim["frames"])
                                            {
                                                AnimationFrame frameInfo;
                                                if(frameElement.is_number())
                                                {
                                                    frameInfo.index = frameElement.get<int>();
                                                    frameInfo.time = animData.defaultFrameTime;
                                                }
                                                else if(frameElement.is_object())
                                                {
                                                    frameInfo.index = frameElement.value("index", 0);
                                                    frameInfo.time = frameElement.value("time", animData.defaultFrameTime);
                                                }
                                                animData.sequence.emplace_back(frameInfo);
                                            }
                                        }
                                    }
                                }
                                catch(...) {}
                            }
                        }
                        textures[texturePath] = animData;
                    }
                }
            }
        }
    }

    if(modelJson.value("is_fluid_container", false))
    {
        std::string bucketTexPath = "minecraft:item/bucket";
        if(modelJson.contains("textures") && modelJson["textures"].contains("layer0"))
        {
            bucketTexPath = modelJson["textures"]["layer0"];
        }

        if(textures.count(bucketTexPath))
        {
            auto &tex = textures[bucketTexPath].texture;
            auto clearBlock = [&](int x, int y) {
                unsigned int xStart = (x * tex.size.x) / 16;
                unsigned int xEnd = ((x + 1) * tex.size.x) / 16;
                unsigned int yStart = (y * tex.size.y) / 16;
                unsigned int yEnd = ((y + 1) * tex.size.y) / 16;

                for(unsigned int ty = yStart; ty < yEnd; ty++)
                {
                    for(unsigned int tx = xStart; tx < xEnd; tx++)
                    {
                        if(tx < tex.size.x && ty < tex.size.y)
                        {
                            tex.pixels[(ty * tex.size.x + tx) * 4 + 3] = 0;
                        }
                    }
                }
            };
            // Row 3: 5-10
            for(int x = 5; x <= 10; ++x) clearBlock(x, 3);
            // Row 4: 4-11
            for(int x = 4; x <= 11; ++x) clearBlock(x, 4);
            // Rows 5-10: 5-10
            for(int x = 5; x <= 10; ++x) clearBlock(x, 5);
        }
    }
}

ModelGenerator::ModelGenerator(const std::string &objData, const std::string &mtlData, KubeJSClient &client, const std::string &checkNamespace, const std::string &id)
{
    isObjModel = true;
    this->id = id;
    std::string warn;
    std::string err;

    std::istringstream objStream(objData);

    class MemMaterialReader : public tinyobj::MaterialReader
    {
      public:
        std::string mtlData;
        MemMaterialReader(std::string data) : mtlData(data)
        {
        }
        bool operator()(const std::string &matId, std::vector<tinyobj::material_t> *materials,
                        std::map<std::string, int> *matMap, std::string *warn, std::string *err) override
        {
            std::istringstream mtlStream(mtlData);
            tinyobj::LoadMtl(matMap, materials, &mtlStream, warn, err);
            int a = matId.size()+1;// Just to delete the warning
            if(a) return true;
            return true;
        }
    };
    MemMaterialReader matReader(mtlData);

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &objStream, &matReader, true);

    if(!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if(!err.empty()) std::cerr << "ERR: " << err << std::endl;
    if(!ret) return;

    for(const auto &mat : materials)
    {
        std::string texName = mat.diffuse_texname;
        if(texName.empty()) continue;
        std::string ns = checkNamespace;
        std::string path = texName;

        size_t colonPos = texName.find(':');
        if(colonPos != std::string::npos)
        {
            ns = texName.substr(0, colonPos);
            path = texName.substr(colonPos + 1);
        }

        size_t dotPos = path.rfind('.');
        if(dotPos != std::string::npos && path.substr(dotPos) == ".png")
        {
            path = path.substr(0, dotPos);
        }

        std::vector<std::string> pathsToTry;

        if(path.find("textures/") == 0)
        {
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/" + path + ".png");
        }
        else
        {
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/textures/" + path + ".png");
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/textures/block/" + path + ".png");
            pathsToTry.emplace_back("/api/client/assets/get/" + ns + "/textures/item/" + path + ".png");
            if(ns != "minecraft")
            {
                pathsToTry.emplace_back("/api/client/assets/get/minecraft/textures/block/" + path + ".png");
            }
        }

        bool loaded = false;
        std::string imgData;

        for(const auto &url : pathsToTry)
        {

            if(client.sendHttpRequest("GET", url, "", imgData))
            {
                if(imgData.empty() || imgData.find("error") != std::string::npos) continue;

                sf::Image img;
                if(img.loadFromMemory(imgData.data(), imgData.size()))
                {
                    TextureAnimation animData;
                    animData.texture.size.x = img.getSize().x;
                    animData.texture.size.y = img.getSize().y;
                    const unsigned char *rawPixels = img.getPixelsPtr();
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

        if(!loaded)
        {
            std::cerr << "Failed to load texture for OBJ: " << texName << std::endl;
        }
    }
}

int ModelGenerator::calculateTotalLoopTicks()
{
    int totalTicks = 1;
    for(const auto &[path, data] : textures)
    {
        if(data.isAnimated)
        {
            int duration = data.getTotalDuration();
            if(duration > 0) totalTicks = std::lcm(totalTicks, duration);
        }
    }
    return totalTicks;
}

std::vector<RenderedFrame> ModelGenerator::generateIsometricSequence(unsigned int outputSize, bool customRotation, float pitch, float yaw, bool wireframe)
{
    bool isFlatItem = false;
    if(modelJson.contains("parent") && modelJson["parent"].get<std::string>().find("item/generated") != std::string::npos) isFlatItem = true;
    if(modelJson.contains("textures") && modelJson["textures"].contains("layer0")) isFlatItem = true;

    if(!modelJson.contains("elements"))
    {
        if(isFlatItem)
        {
            auto elementsArray = nlohmann::json::array();
            for(int i = 0; i < 10; ++i) 
            {
                std::string layerKey = "layer" + std::to_string(i);
                if(modelJson["textures"].contains(layerKey))
                {
                    nlohmann::json flatItem;
                    if(modelJson.value("is_fluid_container", false) && i == 1)
                    {
                        flatItem["from"] = {4, 3, 0.0f};
                        flatItem["to"] = {12, 13, 0.0f};
                        flatItem["faces"]["north"] = {{"texture", "#" + layerKey}, {"uv", {4, 3, 12, 13}}};
                    }
                    else
                    {
                        flatItem["from"] = {0, 0, (i + 1) * 1.0f};
                        flatItem["to"] = {16, 16, (i + 1) * 1.0f};
                        flatItem["faces"]["north"] = {{"texture", "#" + layerKey}, {"uv", {0, 0, 16, 16}}};
                    }
                    elementsArray.push_back(flatItem);
                }
            }
            if(!elementsArray.empty())
            {
                modelJson["elements"] = elementsArray;
            }
            else
            {
                nlohmann::json flatItem;
                flatItem["from"] = {0, 0, 0};
                flatItem["to"] = {16, 16, 0};
                flatItem["faces"]["north"] = {{"texture", "#layer0"}, {"uv", {0, 0, 16, 16}}};
                modelJson["elements"] = nlohmann::json::array({flatItem});
            }
        }
        else
        {
            nlohmann::json defaultCube;
            defaultCube["from"] = {0, 0, 0};
            defaultCube["to"] = {16, 16, 16};

            auto findTexture = [&](const std::string &dir, const std::vector<std::string> &fb) -> std::string
            {
                if(modelJson["textures"].contains(dir)) return "#" + dir;
                for(auto &k : fb)
                    if(modelJson["textures"].contains(k)) return "#" + k;
                if(modelJson["textures"].contains("all")) return "#all";
                
                if(modelJson["textures"].contains("particle")) return "#particle";
                
                if(!modelJson["textures"].empty()) return "#" + modelJson["textures"].begin().key();
                
                return "";
            };

            std::string t_up = findTexture("up", {"top"}), t_down = findTexture("down", {"bottom"}),
                        t_north = findTexture("north", {"front", "side"}), t_south = findTexture("south", {"back", "side"}),
                        t_east = findTexture("east", {"side"}), t_west = findTexture("west", {"side"});
            if(!t_up.empty()) defaultCube["faces"]["up"] = {{"texture", t_up}};
            if(!t_down.empty()) defaultCube["faces"]["down"] = {{"texture", t_down}};
            if(!t_north.empty()) defaultCube["faces"]["north"] = {{"texture", t_north}};
            if(!t_south.empty()) defaultCube["faces"]["south"] = {{"texture", t_south}};
            if(!t_east.empty()) defaultCube["faces"]["east"] = {{"texture", t_east}};
            if(!t_west.empty()) defaultCube["faces"]["west"] = {{"texture", t_west}};
            modelJson["elements"] = nlohmann::json::array({defaultCube});
        }
    }

    std::vector<Triangle> triangles;
    float scale, centerX, centerY;
    if(isFlatItem)
    {
        scale = (float)outputSize / 16.0f;
        centerX = 0;
        centerY = 0;
    }
    else
    {
        scale = (float)outputSize / 38.0f;
        centerX = outputSize / 2.0f;
        centerY = (outputSize / 2.0f) + (scale * 2.0f);
    }

    float minZ = 1e9, maxZ = -1e9;

    for(const auto &element : modelJson["elements"])
    {
        auto from = element["from"];
        auto to = element["to"];
        sf::Vector3f pMin(from[0], from[1], from[2]);
        sf::Vector3f pMax(to[0], to[1], to[2]);

        sf::Vector3f rotOrigin(8, 8, 8);
        std::string rotAxis = "y";
        float rotAngle = 0;
        if(element.contains("rotation"))
        {
            auto &rot = element["rotation"];
            rotOrigin = {rot["origin"][0], rot["origin"][1], rot["origin"][2]};
            rotAxis = rot.value("axis", "y");
            rotAngle = rot.value("angle", 0.0f);
        }

        struct FaceDef
        {
            std::string dir;
            sf::Vector3f v[4];
        };
        FaceDef faces[] = {
            {"up", {{pMin.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMax.z}, {pMin.x, pMax.y, pMax.z}}},
            {"north", {{pMax.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMin.z}, {pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}}},
            {"east", {{pMax.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}}},
            {"west", {{pMin.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMax.z}, {pMin.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMin.z}}},
            {"south", {{pMin.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMax.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}},
            {"down", {{pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}}};

        for(const auto &f : faces)
        {
            if(!element.contains("faces") || !element["faces"].contains(f.dir)) continue;
            auto faceJson = element["faces"][f.dir];
            std::string texRef = faceJson.value("texture", "");
            if(texRef.empty()) continue;
            int depth = 0;
            if(texRef[0] == '#')
            {
                std::string key = texRef.substr(1);
                while(modelJson["textures"].contains(key) && depth < 5)
                {
                    texRef = modelJson["textures"][key];
                    if(texRef[0] == '#')
                        key = texRef.substr(1);
                    else
                        break;
                    depth++;
                }
            }
            if(textures.find(texRef) == textures.end()) continue;
            TextureAnimation &anim = textures[texRef];
            const Texture *tex = &anim.texture;

            if(tex->size.x == 0 || tex->size.y == 0) continue;

            float texH = (float)tex->size.y;
            float vScale = (float)anim.frameHeight / texH;
            sf::Vector2f uv[4];

            if(faceJson.contains("uv"))
            {
                float u1 = faceJson["uv"][0], v1 = faceJson["uv"][1];
                float u2 = faceJson["uv"][2], v2 = faceJson["uv"][3];
                
                sf::Vector2f baseUv[4] = {
                    {u1 / 16.0f, v1 / 16.0f * vScale},
                    {u2 / 16.0f, v1 / 16.0f * vScale},
                    {u2 / 16.0f, v2 / 16.0f * vScale},
                    {u1 / 16.0f, v2 / 16.0f * vScale}
                };
                
                int uvRot = faceJson.value("rotation", 0);
                int shift = (4 - (uvRot / 90) % 4) % 4;
                
                for(int i = 0; i < 4; i++)
                {
                    uv[i] = baseUv[(i + shift) % 4];
                }
            }
            else
            {
                for(int i = 0; i < 4; i++)
                {
                    float ux, vy;
                    if(f.dir == "up" || f.dir == "down")
                    {
                        ux = f.v[i].x;
                        vy = f.v[i].z;
                    }
                    else if(f.dir == "north" || f.dir == "south")
                    {
                        ux = f.v[i].x;
                        vy = 16.0f - f.v[i].y;
                    }
                    else
                    {
                        ux = f.v[i].z;
                        vy = 16.0f - f.v[i].y;
                    }
                    uv[i] = {ux / 16.0f, vy / 16.0f * vScale};
                }
            }

            sf::Color shade = getFaceShading(f.dir, isFlatItem);
            sf::Vector3f screenPos[4];
            float avgZ = 0;
            for(int i = 0; i < 4; i++)
            {
                float bias = 0.0f;
                if(f.dir == "up") bias = 0.01f;
                else if(f.dir == "north") bias = 0.02f;
                else if(f.dir == "east") bias = 0.03f;
                else if(f.dir == "west") bias = 0.04f;
                else if(f.dir == "south") bias = 0.05f;
                else if(f.dir == "down") bias = 0.06f;

                sf::Vector3f rotV = rotatePoint(f.v[i], rotOrigin, rotAxis, rotAngle);
                if(isFlatItem)
                {
                    screenPos[i] = {rotV.x * scale, (16.0f - rotV.y) * scale, rotV.z + bias};
                }
                else if(customRotation)
                {
                    screenPos[i] = project3DTransform(rotV.x, rotV.y, rotV.z, pitch, yaw, scale, centerX, centerY, 8, 8, 8);
                    screenPos[i].z += bias;
                }
                else
                {
                    sf::Vector2f iso = toIso(rotV.x, rotV.y, rotV.z, scale, centerX, centerY);
                    screenPos[i] = {iso.x, iso.y, rotV.x + rotV.y + rotV.z + bias};
                }
                avgZ += screenPos[i].z;
            }
            avgZ /= 4.0f;

            if(!isFlatItem && !customRotation)
            {
                sf::Vector2f p0(screenPos[0].x, screenPos[0].y), p1(screenPos[1].x, screenPos[1].y), p2(screenPos[2].x, screenPos[2].y);
                if(((p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x)) < 0) continue;
            }

            Triangle tri1, tri2;
            tri1.texture = tex;
            tri2.texture = tex;
            tri1.depth = tri2.depth = avgZ;
            for(int i = 0; i < 3; i++)
            {
                int idx = i;
                tri1.v[i].x = screenPos[idx].x;
                tri1.v[i].y = screenPos[idx].y;
                tri1.v[i].z = screenPos[idx].z;
                tri1.v[i].r = shade.r / 255.0f;
                tri1.v[i].g = shade.g / 255.0f;
                tri1.v[i].b = shade.b / 255.0f;
                tri1.v[i].a = 1.0f;
                tri1.v[i].u = uv[idx].x;
                tri1.v[i].v = uv[idx].y;
            }
            for(int i = 0; i < 3; i++)
            {
                int idx = (i == 0) ? 2 : (i == 1) ? 3 : 0;
                tri2.v[i].x = screenPos[idx].x;
                tri2.v[i].y = screenPos[idx].y;
                tri2.v[i].z = screenPos[idx].z;
                tri2.v[i].r = shade.r / 255.0f;
                tri2.v[i].g = shade.g / 255.0f;
                tri2.v[i].b = shade.b / 255.0f;
                tri2.v[i].a = 1.0f;
                tri2.v[i].u = uv[idx].x;
                tri2.v[i].v = uv[idx].y;
            }
            triangles.push_back(tri1);
            triangles.push_back(tri2);

            for(int i = 0; i < 4; i++)
            {
                if(screenPos[i].z < minZ) minZ = screenPos[i].z;
                if(screenPos[i].z > maxZ) maxZ = screenPos[i].z;
            }
        }
    }

    if(triangles.empty()) return {};

    std::stable_sort(triangles.begin(), triangles.end(), [](const Triangle &a, const Triangle &b)
              { return a.depth < b.depth; });

    if(minZ >= maxZ)
    {
        minZ = -1.0f;
        maxZ = 1.0f;
    }
    glm::mat4 proj = glm::ortho(0.0f, (float)outputSize, 0.0f, (float)outputSize, -300.0f, 300.0f);

    static GLuint shaderProgram = 0;
    if(shaderProgram == 0) shaderProgram = CreateShaderProgram();

    GLuint fbo, renderTex, rbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &renderTex);
    glBindTexture(GL_TEXTURE_2D, renderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outputSize, outputSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTex, 0);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, outputSize, outputSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        return {};
    }

    std::map<const Texture *, GLuint> glTextures;
    for(auto &[path, anim] : textures)
    {
        if(!anim.texture.pixels.empty())
            glTextures[&anim.texture] = CreateGLTextureFromPixels(anim.texture.pixels, anim.texture.size.x, anim.texture.size.y);
    }

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    std::vector<GpuVertex> allVertices;
    for(const auto &tri : triangles)
    {
        for(int i = 0; i < 3; i++)
        {
            allVertices.push_back(tri.v[i]);
        }
    }

    glBufferData(GL_ARRAY_BUFFER, allVertices.size() * sizeof(GpuVertex), allVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void *)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, outputSize, outputSize);
    if(wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
    }
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(shaderProgram, "uWireframe"), wireframe ? 1 : 0);
    GLint texOffsetLoc = glGetUniformLocation(shaderProgram, "uTexOffset");

    std::vector<RenderedFrame> resultFrames;
    int totalTicks = calculateTotalLoopTicks();
    bool anyAnimated = false;
    for(auto &[k, v] : textures)
    {
        if(v.isAnimated) anyAnimated = true;
    }

    std::vector<int> prevFrameIndices;

    for(int tick = 0; tick < totalTicks; ++tick)
    {
        std::vector<int> currentFrameIndices;
        
        if(anyAnimated)
        {
            for(auto &[path, anim] : textures)
            {
                if(anim.isAnimated)
                {
                    int duration = anim.getTotalDuration();
                    int frameIndex = 0;
                    if(duration > 0)
                    {
                        int localTick = tick % duration;
                        if(anim.sequence.empty())
                            frameIndex = (localTick / anim.defaultFrameTime) % (anim.texture.size.y / anim.frameHeight);
                        else
                        {
                            int acc = 0;
                            for(auto &fr : anim.sequence)
                            {
                                acc += fr.time;
                                if(localTick < acc)
                                {
                                    frameIndex = fr.index;
                                    break;
                                }
                            }
                        }
                    }
                    currentFrameIndices.push_back(frameIndex);
                }
            }

            if(tick > 0 && currentFrameIndices == prevFrameIndices)
            {
                resultFrames.back().timeInTicks++;
                continue; 
            }
            prevFrameIndices = currentFrameIndices;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(vao);

        size_t vertexOffset = 0;
        const Texture *currentTex = nullptr;
        GLuint currentGLTex = 0;
        int batchStart = 0, batchCount = 0;

        for(size_t i = 0; i < triangles.size(); ++i)
        {
            const Triangle &tri = triangles[i];
            if(tri.texture != currentTex)
            {
                if(batchCount > 0)
                {
                    glBindTexture(GL_TEXTURE_2D, currentGLTex);
                    float frameOffsetY = 0.0f;
                    if(anyAnimated)
                    {
                        for(auto &[path, anim] : textures)
                        {
                            if(&anim.texture == currentTex && anim.isAnimated)
                            {
                                int duration = anim.getTotalDuration();
                                if(duration > 0)
                                {
                                    int localTick = tick % duration;
                                    int frameIndex = 0;
                                    if(anim.sequence.empty())
                                        frameIndex = (localTick / anim.defaultFrameTime) % (currentTex->size.y / anim.frameHeight);
                                    else
                                    {
                                        int acc = 0;
                                        for(auto &fr : anim.sequence)
                                        {
                                            acc += fr.time;
                                            if(localTick < acc)
                                            {
                                                frameIndex = fr.index;
                                                break;
                                            }
                                        }
                                    }
                                    frameOffsetY = (frameIndex * anim.frameHeight) / (float)currentTex->size.y;
                                }
                                break;
                            }
                        }
                    }
                    glUniform2f(texOffsetLoc, 0.0f, frameOffsetY);
                    glDrawArrays(GL_TRIANGLES, batchStart, batchCount * 3);
                }
                currentTex = tri.texture;
                currentGLTex = glTextures[currentTex];
                batchStart = vertexOffset;
                batchCount = 1;
            }
            else
            {
                batchCount++;
            }
            vertexOffset += 3;
        }
        if(batchCount > 0)
        {
            glBindTexture(GL_TEXTURE_2D, currentGLTex);
            float frameOffsetY = 0.0f;
            if(anyAnimated)
            {
                for(auto &[path, anim] : textures)
                {
                    if(&anim.texture == currentTex && anim.isAnimated)
                    {
                        int duration = anim.getTotalDuration();
                        if(duration > 0)
                        {
                            int localTick = tick % duration;
                            int frameIndex = 0;
                            if(anim.sequence.empty())
                                frameIndex = (localTick / anim.defaultFrameTime) % (currentTex->size.y / anim.frameHeight);
                            else
                            {
                                int acc = 0;
                                for(auto &fr : anim.sequence)
                                {
                                    acc += fr.time;
                                    if(localTick < acc)
                                    {
                                        frameIndex = fr.index;
                                        break;
                                    }
                                }
                            }
                            frameOffsetY = (frameIndex * anim.frameHeight) / (float)currentTex->size.y;
                        }
                        break;
                    }
                }
            }
            glUniform2f(texOffsetLoc, 0.0f, frameOffsetY);
            glDrawArrays(GL_TRIANGLES, batchStart, batchCount * 3);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::vector<unsigned char> pixels(outputSize * outputSize * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, outputSize, outputSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        sf::Image img({outputSize, outputSize}, pixels.data());
        img.flipHorizontally();
        
        resultFrames.push_back({img, 1});
        
        if(!anyAnimated) break;
    }
    if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    for(auto &[tex, id] : glTextures)
    {
        glDeleteTextures(1, &id);
    }

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &renderTex);
    glDeleteRenderbuffers(1, &rbo);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    if(!depthTest) glDisable(GL_DEPTH_TEST);
    if(!blend)
        glDisable(GL_BLEND);
    else
        glBlendFunc(blendSrc, blendDst);
    glUseProgram(0);

    return resultFrames;
}

std::vector<RenderedFrame> ModelGenerator::generateIsometricSequenceOBJ(unsigned int outputSize, bool customRotation, float pitch, float yaw, bool wireframe)
{
    float minX = 1e9, maxX = -1e9;
    float minY = 1e9, maxY = -1e9;
    float minZ_local = 1e9, maxZ_local = -1e9;
    bool hasVertices = false;

    for(size_t i = 0; i < attrib.vertices.size() / 3; i++)
    {
        float vx = attrib.vertices[3 * i + 0];
        float vy = attrib.vertices[3 * i + 1];
        float vz = attrib.vertices[3 * i + 2];
        if(vx < minX) minX = vx;
        if(vx > maxX) maxX = vx;
        if(vy < minY) minY = vy;
        if(vy > maxY) maxY = vy;
        if(vz < minZ_local) minZ_local = vz;
        if(vz > maxZ_local) maxZ_local = vz;
        hasVertices = true;
    }
    if(!hasVertices) return {};

    float localCenterX = (minX + maxX) / 2.0f;
    float localCenterY = (minY + maxY) / 2.0f;
    float localCenterZ = (minZ_local + maxZ_local) / 2.0f;

    const float cos30 = 0.866025f;
    const float sin30 = 0.5f;

    auto projectRawIso = [&](float vx, float vy, float vz) -> sf::Vector3f
    {
        if(customRotation)
        {
            return project3DTransform(vx, vy, vz, pitch, yaw, 1.0f, 0.0f, 0.0f, localCenterX, localCenterY, localCenterZ);
        }
        else
        {
            float rx = (vx - vz) * cos30;
            float ry = (vx + vz) * sin30 - vy;
            return {rx, ry, vx + vy + vz};
        }
    };

    float maxDistSq = 0;
    for(size_t i = 0; i < attrib.vertices.size() / 3; i++)
    {
        float dx = attrib.vertices[3 * i + 0] - localCenterX;
        float dy = attrib.vertices[3 * i + 1] - localCenterY;
        float dz = attrib.vertices[3 * i + 2] - localCenterZ;
        float distSq = dx * dx + dy * dy + dz * dz;
        if(distSq > maxDistSq) maxDistSq = distSq;
    }
    float radius = std::sqrt(maxDistSq);
    if(radius < 0.001f) radius = 1.0f;

    const float padding = 0.85f;
    float finalScale = (outputSize * padding) / (radius * 2.0f);

    auto projectToScreen = [&](float vx, float vy, float vz) -> sf::Vector3f
    {
        sf::Vector3f centerProj = projectRawIso(localCenterX, localCenterY, localCenterZ);
        sf::Vector3f vertexProj = projectRawIso(vx, vy, vz);
        
        float offsetX = vertexProj.x - centerProj.x;
        float offsetY = vertexProj.y - centerProj.y;
        
        return {
            (float)outputSize / 2.0f + offsetX * finalScale,
            (float)outputSize / 2.0f + offsetY * finalScale,
            vertexProj.z
        };
    };

    std::vector<Triangle> triangles;

    for(const auto &shape : shapes)
    {
        size_t index_offset = 0;
        for(size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            int fv = shape.mesh.num_face_vertices[f];
            if(fv != 3)
            {
                index_offset += fv;
                continue;
            }

            const Texture *tex = nullptr;
            int matId = shape.mesh.material_ids[f];
            if(matId >= 0 && static_cast<size_t>(matId) < materials.size())
            {
                std::string rawTexName = materials[matId].diffuse_texname;
                if(!rawTexName.empty())
                {
                    if(textures.count(rawTexName))
                    {
                        tex = &textures[rawTexName].texture;
                    }
                    else
                    {
                        std::string cleanName = cleanTextureName(rawTexName);
                        for(auto &[key, val] : textures)
                        {
                            if(cleanTextureName(key) == cleanName)
                            {
                                tex = &val.texture;
                                break;
                            }
                        }
                    }
                }
            }

            Triangle tri;
            tri.texture = tex;
            float avgZ = 0.0f;
            for(int v = 0; v < 3; v++)
            {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                float vx = attrib.vertices[3 * idx.vertex_index + 0];
                float vy = attrib.vertices[3 * idx.vertex_index + 1];
                float vz = attrib.vertices[3 * idx.vertex_index + 2];

                sf::Vector3f screen = projectToScreen(vx, vy, vz);
                avgZ += screen.z;

                GpuVertex gv;
                gv.x = screen.x;
                gv.y = screen.y;
                gv.z = screen.z;
                gv.r = 1.0f;
                gv.g = 1.0f;
                gv.b = 1.0f;
                gv.a = 1.0f;

                if(tex && idx.texcoord_index >= 0)
                {
                    float u = attrib.texcoords[2 * idx.texcoord_index + 0];
                    float v_coord = attrib.texcoords[2 * idx.texcoord_index + 1];
                    gv.u = u;
                    gv.v = 1.0f - v_coord;
                }
                else
                {
                    gv.u = 0.0f;
                    gv.v = 0.0f;
                }
                tri.v[v] = gv;
            }
            tri.depth = avgZ / 3.0f;
            triangles.push_back(tri);

            index_offset += fv;
        }
    }

    if(triangles.empty()) return {};

    std::sort(triangles.begin(), triangles.end(), [](const Triangle &a, const Triangle &b)
              { return a.depth < b.depth; });

    glm::mat4 proj = glm::ortho(0.0f, (float)outputSize, 0.0f, (float)outputSize, -1000.0f, 1000.0f);

    static GLuint shaderProgram = 0;
    if(shaderProgram == 0)
    {
        shaderProgram = CreateShaderProgram();
    }

    GLuint fbo, renderTex, rbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &renderTex);
    glBindTexture(GL_TEXTURE_2D, renderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outputSize, outputSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTex, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, outputSize, outputSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer not complete!" << std::endl;
        return {};
    }

    std::map<const Texture *, GLuint> glTextures;
    for(auto &[path, anim] : textures)
    {
        if(anim.texture.pixels.empty()) continue;
        GLuint texID = CreateGLTextureFromPixels(anim.texture.pixels, anim.texture.size.x, anim.texture.size.y);
        glTextures[&anim.texture] = texID;
    }

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    std::vector<GpuVertex> allVertices;
    for(const auto &tri : triangles)
    {
        for(int i = 0; i < 3; i++) allVertices.push_back(tri.v[i]);
    }
    glBufferData(GL_ARRAY_BUFFER, allVertices.size() * sizeof(GpuVertex), allVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void *)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);
    GLboolean cullFace = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, outputSize, outputSize);
    if(wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
    }

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(shaderProgram, "uWireframe"), wireframe ? 1 : 0);
    GLint texOffsetLoc = glGetUniformLocation(shaderProgram, "uTexOffset");

    std::vector<RenderedFrame> resultFrames;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(vao);

    size_t vertexOffset = 0;
    const Texture *currentTex = nullptr;
    GLuint currentGLTex = 0;
    int batchStart = 0, batchCount = 0;

    for(size_t i = 0; i < triangles.size(); ++i)
    {
        const Triangle &tri = triangles[i];
        if(tri.texture != currentTex)
        {
            if(batchCount > 0)
            {
                glBindTexture(GL_TEXTURE_2D, currentGLTex);
                glUniform2f(texOffsetLoc, 0.0f, 0.0f);
                glDrawArrays(GL_TRIANGLES, batchStart, batchCount * 3);
            }

            currentTex = tri.texture;
            currentGLTex = currentTex ? glTextures[currentTex] : 0;
            batchStart = vertexOffset;
            batchCount = 1;
        }
        else
        {
            batchCount++;
        }
        vertexOffset += 3;
    }
    if(batchCount > 0)
    {
        glBindTexture(GL_TEXTURE_2D, currentGLTex);
        glUniform2f(texOffsetLoc, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLES, batchStart, batchCount * 3);
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<unsigned char> pixels(outputSize * outputSize * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, outputSize, outputSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    sf::Image img({outputSize, outputSize}, pixels.data());

    img.flipHorizontally();
    resultFrames.push_back({img, 1});

    if(depthTest) glEnable(GL_DEPTH_TEST);
    if(!blend)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        glBlendFunc(blendSrc, blendDst);
    }

    if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if(cullFace) glEnable(GL_CULL_FACE);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glUseProgram(0);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    for(auto &[tex, glid] : glTextures) glDeleteTextures(1, &glid);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &renderTex);
    glDeleteRenderbuffers(1, &rbo);

    return resultFrames;
}

void ModelGenerator::saveAssets(const std::string &itemId, bool customRotation, int customSize, float pitch, float yaw)
{
    std::string safeName = changeFilename(itemId);
    std::string targetDir = "img/" + safeName;

    if(!fs::exists(targetDir))
    {
        fs::create_directories(targetDir);
    }

    std::ofstream jsonFile(targetDir + "/model.json");
    jsonFile << std::setw(4) << modelJson << std::endl;
    jsonFile.close();

    for(const auto &[path, animData] : textures)
    {
        if(animData.texture.getSize().x == 0) continue;

        std::string texName = changeFilename(path) + ".png";
        sf::Image img = animData.texture.copyToImage();
        if(!img.saveToFile(targetDir + "/" + texName)) std::cerr << "Error loading image" << std::endl;

        if(!animData.rawMcmeta.empty())
        {
            std::string metaName = texName + ".mcmeta";
            std::ofstream metaFile(targetDir + "/" + metaName);
            metaFile << animData.rawMcmeta;
            metaFile.close();
        }
    }

    bool isItem = false;
    if(modelJson.contains("parent") && modelJson["parent"].get<std::string>().find("item/generated") != std::string::npos) isItem = true;
    if(modelJson.contains("textures") && modelJson["textures"].contains("layer0")) isItem = true;
    if(!isItem) exportToObj(itemId, targetDir);
    if(isObjModel)
    {
        saveAnimationWebP(itemId, targetDir, generateIsometricSequenceOBJ(customSize, customRotation, pitch, yaw));
    }
    else
    {
        saveAnimationWebP(itemId, targetDir, generateIsometricSequence(customSize, customRotation, pitch, yaw));
    }
}

void ModelGenerator::saveAnimationWebP(const std::string &itemId, const std::string &outputDir, const std::vector<RenderedFrame>& frames)
{
    if(frames.empty()) return;

    std::string baseName = changeFilename(itemId);
    std::string filename = baseName + ".webp";
    std::string fullPath = outputDir + "/" + filename;

    int width = frames[0].image.getSize().x;
    int height = frames[0].image.getSize().y;

    WebPAnimEncoderOptions enc_options;
    WebPAnimEncoderOptionsInit(&enc_options);

    WebPAnimEncoder *enc = WebPAnimEncoderNew(width, height, &enc_options);
    if(!enc)
    {
        std::cerr << "WebP: Failed to create encoder" << std::endl;
        return;
    }

    int timestamp_ms = 0;

    for(const auto &img : frames)
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

        const uint8_t *pixels = img.image.getPixelsPtr();
        WebPPictureImportRGBA(&pic, pixels, width * 4);

        if(!WebPAnimEncoderAdd(enc, &pic, timestamp_ms, &config))
        {
            std::cerr << "WebP: Error adding frame" << std::endl;
            WebPPictureFree(&pic);
            WebPAnimEncoderDelete(enc);
            return;
        }

        WebPPictureFree(&pic);
        timestamp_ms += img.timeInTicks * 50; 
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
        file.write(reinterpret_cast<const char *>(webp_data.bytes), webp_data.size);
        file.close();
    }

    WebPDataClear(&webp_data);
    WebPAnimEncoderDelete(enc);
}

void ModelGenerator::exportToObj(const std::string &itemId, const std::string &outputDir)
{
    std::string baseName = changeFilename(itemId);
    std::string objFilename = outputDir + "/" + baseName + ".obj";
    std::string mtlFilename = outputDir + "/" + baseName + ".mtl";
    std::string mtlBaseName = baseName + ".mtl";

    std::ofstream objFile(objFilename);
    std::ofstream mtlFile(mtlFilename);

    if(!objFile.is_open() || !mtlFile.is_open()) return;

    objFile << "# Exported using QuestiMakinator\n";
    objFile << "mtllib " << mtlBaseName << "\n";

    int vertexCount = 0;
    int uvCount = 0;

    std::set<std::string> usedMaterials;

    if(modelJson.contains("elements"))
    {
        for(const auto &element : modelJson["elements"])
        {
            auto from = element["from"];
            auto to = element["to"];

            sf::Vector3f pMin(from[0], from[1], from[2]);
            sf::Vector3f pMax(to[0], to[1], to[2]);
            sf::Vector3f rotOrigin(8, 8, 8);

            std::string rotAxis = "y";
            float rotAngle = 0;
            if(element.contains("rotation"))
            {
                auto &rot = element["rotation"];
                rotOrigin = {rot["origin"][0], rot["origin"][1], rot["origin"][2]};
                rotAxis = rot.value("axis", "y");
                rotAngle = rot.value("angle", 0.0f);
            }

            struct FaceDef
            {
                std::string dir;
                sf::Vector3f v[4];
                sf::Vector3f normal;
            };
            FaceDef faces[] = {
                {"up", {{pMin.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMax.z}, {pMin.x, pMax.y, pMax.z}}, {0, 1, 0}},
                {"down", {{pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}, {0, -1, 0}},
                {"north", {{pMax.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMin.z}, {pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}}, {0, 0, -1}},
                {"south", {{pMin.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMax.z}, {pMax.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMax.z}}, {0, 0, 1}},
                {"west", {{pMin.x, pMax.y, pMin.z}, {pMin.x, pMax.y, pMax.z}, {pMin.x, pMin.y, pMax.z}, {pMin.x, pMin.y, pMin.z}}, {-1, 0, 0}},
                {"east", {{pMax.x, pMax.y, pMax.z}, {pMax.x, pMax.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z}}, {1, 0, 0}}};

            for(const auto &f : faces)
            {
                if(!element.contains("faces") || !element["faces"].contains(f.dir)) continue;
                auto faceJson = element["faces"][f.dir];

                std::string texRef = faceJson.value("texture", "");
                if(texRef.empty()) continue;

                int depth = 0;
                std::string resolvedPath = texRef;
                while(!resolvedPath.empty() && resolvedPath[0] == '#' && depth < 5)
                {
                    std::string key = resolvedPath.substr(1);
                    if(modelJson["textures"].contains(key))
                        resolvedPath = modelJson["textures"][key];
                    else
                        break;
                    depth++;
                }
                if(resolvedPath.empty()) continue;

                std::string matName = changeFilename(resolvedPath);
                usedMaterials.insert(resolvedPath);
                objFile << "usemtl " << matName << "\n";

                // Vertices
                for(int i = 0; i < 4; i++)
                {
                    sf::Vector3f rv = rotatePoint(f.v[i], rotOrigin, rotAxis, rotAngle);
                    objFile << "v " << rv.x / 16.0f << " " << rv.y / 16.0f << " " << rv.z / 16.0f << "\n";
                }

                // UVs
                float u1 = 0, v1 = 0, u2 = 16, v2 = 16;
                if(faceJson.contains("uv"))
                {
                    u1 = faceJson["uv"][0];
                    v1 = faceJson["uv"][1];
                    u2 = faceJson["uv"][2];
                    v2 = faceJson["uv"][3];
                }
                else
                {
                    if(f.dir == "up" || f.dir == "down")
                    {
                        u1 = f.v[0].x;
                        v1 = f.v[0].z;
                        u2 = f.v[2].x;
                        v2 = f.v[2].z;
                    }
                    else
                    {
                        u1 = 0;
                        v1 = 0;
                        u2 = 16;
                        v2 = 16;
                    }
                }

                sf::Vector2f uvs[4];
                uvs[0] = {u1, v1};
                uvs[1] = {u2, v1};
                uvs[2] = {u2, v2};
                uvs[3] = {u1, v2};

                for(int k = 0; k < 4; k++)
                {
                    objFile << "vt " << uvs[k].x / 16.0f << " " << (16.0f - uvs[k].y) / 16.0f << "\n";
                }

                int baseV = vertexCount + 1;
                int baseVT = uvCount + 1;

                objFile << "f "
                        << baseV << "/" << baseVT << " "
                        << baseV + 1 << "/" << baseVT + 1 << " "
                        << baseV + 2 << "/" << baseVT + 2 << " "
                        << baseV + 3 << "/" << baseVT + 3 << "\n";

                vertexCount += 4;
                uvCount += 4;
            }
        }
    }
    objFile.close();

    for(const auto &mat : usedMaterials)
    {
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