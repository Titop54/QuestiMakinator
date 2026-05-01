#include <glad/glad.h>
#include "integration/model.h"
#include "integration/client.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <webp/encode.h>
#include <webp/mux.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <numbers>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace
{

static const char* vertex_shader_source = R"glsl(
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
)glsl";

static const char* fragment_shader_source = R"glsl(
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

    /**
     * @brief Compiles an OpenGL shader of the given type.
     * @param type Shader type (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER).
     * @param source GLSL source code string.
     * @return The compiled shader ID.
     */
    GLuint compile_shader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            char info_log[512];
            glGetShaderInfoLog(shader, 512, nullptr, info_log);
            std::cerr << "Error compiling shader: " << info_log << std::endl;
        }
        return shader;
    }

    /**
     * @brief Creates a linked shader program from the built-in vertex/fragment sources.
     * @return The linked program ID.
     */
    GLuint create_shader_program()
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if(!success)
        {
            char info_log[512];
            glGetProgramInfoLog(program, 512, nullptr, info_log);
            std::cerr << "Error linking program: " << info_log << std::endl;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    /**
     * @brief Creates an OpenGL 2D texture from raw pixel data.
     * @param pixels RGBA8 pixel vector.
     * @param width Texture width.
     * @param height Texture height.
     * @return The generated texture ID.
     */
    GLuint create_gl_texture(const std::vector<uint8_t>& pixels, int width, int height)
    {
        GLuint tex_id = 0;
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex_id;
    }

    /**
     * @brief Rotates a point around an origin by a given angle on the specified axis.
     * @param point The point to rotate.
     * @param origin The rotation origin.
     * @param axis The axis of rotation ("x", "y", or "z").
     * @param angle The rotation angle in degrees.
     * @return The rotated point.
     */
    sf::Vector3f rotate_point(sf::Vector3f point, sf::Vector3f origin, const std::string& axis, float angle)
    {
        if(angle == 0.0f) return point;
        float rad = angle * (std::numbers::pi_v<float> / 180.0f);
        float c = cos(rad), s = sin(rad);
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
        return { nx + origin.x, ny + origin.y, nz + origin.z };
    }

    /**
     * @brief Projects a 3D point to screen coordinates using custom pitch/yaw rotation.
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param z Z coordinate.
     * @param pitch Rotation around the X axis in degrees.
     * @param yaw Rotation around the Y axis in degrees.
     * @param scale Scale factor.
     * @param centerx Screen X centre.
     * @param centery Screen Y centre.
     * @param pivotX Model pivot X (default: 8.0).
     * @param pivotY Model pivot Y (default: 8.0).
     * @param pivotZ Model pivot Z (default: 8.0).
     * @return The projected 3D point with depth in z.
     */
    sf::Vector3f project_3d(float x, float y, float z, float pitch, float yaw, float scale,
        float centerx, float centery, float pivotX = 8, float pivotY = 8, float pivotZ = 8)
    {
        x -= pivotX;
        y -= pivotY;
        z -= pivotZ;
        float pitch_rad = pitch * (std::numbers::pi_v<float> / 180.0f);
        float yaw_rad = yaw * (std::numbers::pi_v<float> / 180.0f);
        float x1 = x * cos(yaw_rad) - z * sin(yaw_rad);
        float z1 = x * sin(yaw_rad) + z * cos(yaw_rad);
        float y1 = y;
        float x2 = x1;
        float y2 = y1 * cos(pitch_rad) - z1 * sin(pitch_rad);
        float z2 = y1 * sin(pitch_rad) + z1 * cos(pitch_rad);
        return { centerx + x2 * scale, centery - y2 * scale, z2 };
    }

    /**
     * @brief Converts 3D coordinates to 2D isometric screen coordinates.
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param z Z coordinate.
     * @param scale Scale factor.
     * @param centerx Screen X centre.
     * @param centery Screen Y centre.
     * @return The 2D isometric screen position.
     */
    [[maybe_unused]] sf::Vector2f to_iso(float x, float y, float z, float scale, float centerx, float centery)
    {
        const float cos30 = 0.866025f;
        const float sin30 = 0.5f;
        float iso_x = (x - z) * cos30 * scale;
        float iso_y = ((x + z) * sin30 - y) * scale;
        return { centerx + iso_x, centery + iso_y };
    }

    /**
     * @brief Returns a tint colour based on face direction for basic shading.
     * @param face_dir Face direction string (e.g. "north", "up").
     * @param is_flat_item True if the model is a flat 2D item.
     * @return The shading colour.
     */
    sf::Color get_face_shading(const std::string& face_dir, bool is_flat_item)
    {
        if(is_flat_item) return sf::Color::White;
        if(face_dir == "west" || face_dir == "east") return { 200, 200, 200 };
        if(face_dir == "south" || face_dir == "north") return { 150, 150, 150 };
        if(face_dir == "down") return { 100, 100, 100 };
        return sf::Color::White;
    }

    /**
     * @brief Strips directory and extension from a texture path.
     * @param path The original path (may contain backslashes).
     * @return The clean file name without extension.
     */
    std::string clean_texture_name(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        size_t last_slash = path.find_last_of('/');
        if(last_slash != std::string::npos)
        {
            path = path.substr(last_slash + 1);
        }

        size_t last_dot = path.find_last_of('.');
        if(last_dot != std::string::npos)
        {
            path = path.substr(0, last_dot);
        }

        return path;
    }
}

/**
 * @brief Tries to download a PNG from a list of URLs and returns the first valid one.
 * @param client The network client.
 * @param urls List of URLs to attempt.
 * @param img_data Output string holding the PNG data.
 * @param successful_url Output string with the URL that succeeded.
 * @return true if a valid PNG was found, false otherwise.
 */
bool ModelGenerator::try_load_png(Client& client, const std::vector<std::string>& urls,
    std::string& img_data, std::string& successful_url)
{
    for(const auto& url : urls)
    {
        if(!client.sendHttpRequest("GET", url, "", img_data))
        {
            continue;
        }

        if(img_data.empty() || img_data.find("error") != std::string::npos)
        {
            continue;
        }
        successful_url = url;
        return true;
    }
    return false;
}

/**
 * @brief Falls back to searching the client's texture list for a block texture.
 * @param ns The texture namespace (e.g. "minecraft").
 * @param path_hint Partial path to match.
 * @param client The network client.
 * @param img_data Output PNG data.
 * @param successful_url Output URL of the downloaded image.
 * @return true if a fallback texture was downloaded successfully.
 */
bool ModelGenerator::load_fallback_texture(const std::string& ns, const std::string& path_hint,
    Client& client, std::string& img_data,
    std::string& successful_url)
{
    std::string asset_list;
    if(!client.sendHttpRequest("GET", "/api/client/assets/list/textures/block", "", asset_list))
    {
        return false;
    }

    auto list = nlohmann::json::parse(asset_list, nullptr, false, true);
    if(list.is_discarded() || !list.contains(ns))
    {
        return false;
    }

    std::string best_match;
    for(const auto& item : list[ns])
    {
        std::string path_str = item.get<std::string>();
        if(path_str.find(path_hint) == std::string::npos)
        {
            continue;
        }
            
        if(path_str.find("still") != std::string::npos)
        {
            best_match = path_str;
            break;
        }
        if(best_match.empty())
        {
            best_match = path_str;
        }       
    }

    if(best_match.empty())
    {
        return false;
    }

    successful_url = "/api/client/assets/get/" + ns + "/textures/block/" + best_match;
    if(!client.sendHttpRequest("GET", successful_url, "", img_data))
    {
        return false;
    }
        
    return !img_data.empty() && img_data.find("error") == std::string::npos;
}

/**
 * @brief Downloads and parses an optional .mcmeta file for animation data.
 * @param meta_url URL of the .mcmeta file.
 * @param client The network client.
 * @param anim_data The animation structure to fill.
 */
void ModelGenerator::load_animation_meta(const std::string& meta_url, Client& client,
    TextureAnimation& anim_data)
{
    std::string meta_data;
    if(!client.sendHttpRequest("GET", meta_url, "", meta_data))
    {
        return;   
    }

    anim_data.rawMcmeta = meta_data;
    auto meta_json = nlohmann::json::parse(meta_data, nullptr, false, true);
    if(meta_json.is_discarded() || !meta_json.contains("animation"))
    {
        return;
    }

    auto& anim = meta_json["animation"];
    anim_data.defaultFrameTime = anim.value("frametime", 1);
    if(!anim.contains("frames") || !anim["frames"].is_array())
    {
        return;
    }

    for(const auto& elem : anim["frames"])
    {
        AnimationFrame frame;
        if(elem.is_number())
        {
            frame.index = elem.get<int>();
            frame.time = anim_data.defaultFrameTime;
        }
        else if(elem.is_object())
        {
            frame.index = elem.value("index", 0);
            frame.time = elem.value("time", anim_data.defaultFrameTime);
        }
        else
        {
            continue;
        }
        anim_data.sequence.emplace_back(frame);
    }
}

/**
 * @brief Loads texture pixels from raw PNG data, handles rotation and animation meta.
 * @param img_data The PNG file content.
 * @param successful_url The URL the PNG came from (used for .mcmeta lookup).
 * @param client The network client.
 * @param anim_data The animation structure to fill.
 * @return true if the texture was loaded successfully.
 */
bool ModelGenerator::load_texture_from_memory(const std::string& img_data,
    const std::string& successful_url,
    Client& client, TextureAnimation& anim_data)
{
    sf::Image img;
    if(!img.loadFromMemory(img_data.data(), img_data.size()))
    {
        return false;
    }

    unsigned int w = img.getSize().x;
    unsigned int h = img.getSize().y;
    anim_data.texture.size.x = w;
    anim_data.texture.size.y = h;

    const unsigned char* raw = img.getPixelsPtr();
    size_t total = w * h * 4;
    if(!raw || total == 0)
    {
        return false;
    }

    anim_data.texture.pixels.assign(raw, raw + total);

    if(anim_data.texture.size.x > anim_data.texture.size.y)
    {
        anim_data.texture.rotate90Clockwise();
        w = anim_data.texture.size.x;
        h = anim_data.texture.size.y;
    }

    anim_data.frameHeight = w;
    if(h > w)
    {
        anim_data.isAnimated = true;
        std::string meta_url = successful_url + ".mcmeta";
        load_animation_meta(meta_url, client, anim_data);
    }
    return true;
}

/**
 * @brief Fully loads a Minecraft texture by trying direct URLs, fallback lists, and optional .mcmeta.
 * @param client The network client.
 * @param texture_path The texture identifier (e.g. "minecraft:block/stone").
 * @param anim_data Output animation/texture structure.
 * @return true if the texture was loaded and decoded.
 */
bool ModelGenerator::try_load_texture_full(Client& client, const std::string& texture_path,
    TextureAnimation& anim_data)
{
    std::string ns = "minecraft";
    std::string path_part = texture_path;
    size_t colon = texture_path.find(':');
    if(colon != std::string::npos)
    {
        ns = texture_path.substr(0, colon);
        path_part = texture_path.substr(colon + 1);
    }

    std::vector<std::string> urls = {
        "/api/client/assets/get/" + ns + "/textures/" + path_part + ".png",
        "/api/client/assets/get/" + ns + "/textures/block/" + path_part + ".png",
        "/api/client/assets/get/" + ns + "/textures/fluid/" + path_part + "_still.png",
        "/api/client/assets/get/" + ns + "/textures/block/" + path_part + "_still.png",
        "/api/client/assets/get/" + ns + "/textures/block/fluids/" + path_part + "_still.png",
        "/api/client/assets/get/" + ns + "/textures/block/fluid/" + path_part + "_still.png"
    };

    std::string img_data, successful_url;
    if(!try_load_png(client, urls, img_data, successful_url))
    {
        if(!load_fallback_texture(ns, path_part, client, img_data, successful_url))
        {
            return false;
        }
    }

    return load_texture_from_memory(img_data, successful_url, client, anim_data);
}

/**
 * @brief Applies fluid container specialisation (replaces textures).
 */
void ModelGenerator::process_fluid_container()
{
    if(!model_json.contains("loader"))
    {
        return;
    }

    const std::string& loader = model_json["loader"];
    if(loader != "neoforge:fluid_container" && loader != "immersiveengineering:potion_bucket")
    {
        return;
    }

    std::string fluid_id = "minecraft:water";
    if(model_json.contains("fluid"))
    {
        fluid_id = model_json["fluid"];
    }

    model_json["textures"]["layer1"] = fluid_id;
    model_json["parent"] = "minecraft:item/generated";
    model_json["textures"]["layer0"] = "minecraft:item/bucket";
    model_json["is_fluid_container"] = true;
}

/**
 * @brief Downloads parent models recursively and merges textures/elements.
 * @param client The network client.
 */
void ModelGenerator::merge_parent_models(Client& client)
{
    int recursion_depth = 0;
    nlohmann::json current_level = model_json;
    while(current_level.contains("parent") && recursion_depth < 10)
    {
        std::string parent_path = current_level["parent"].get<std::string>();
        if(parent_path.find("builtin/") != std::string::npos)
        {
            break;
        }

        std::string ns = "minecraft";
        std::string path = parent_path;
        size_t colon = parent_path.find(':');
        if(colon != std::string::npos)
        {
            ns = parent_path.substr(0, colon);
            path = parent_path.substr(colon + 1);
        }

        std::string url = "/api/client/assets/get/" + ns + "/models/" + path + ".json";
        std::string response;
        if(!client.sendHttpRequest("GET", url, "", response))
        {
            std::cerr << "Failed to download parent model: " << url << std::endl;
            break;
        }

        auto parent_json = nlohmann::json::parse(response, nullptr, false, true);
        if(parent_json.is_discarded())
        {
            break;
        }

        if(parent_json.contains("textures"))
        {
            if(!model_json.contains("textures"))
            {
                model_json["textures"] = parent_json["textures"];
            }
            else
            {
                for(auto& [key, val] : parent_json["textures"].items())
                {
                    if(!model_json["textures"].contains(key))
                    {
                        model_json["textures"][key] = val;
                    }
                }
            }
        }
        if(!model_json.contains("elements") && parent_json.contains("elements"))
        {
            model_json["elements"] = parent_json["elements"];
        }

        current_level = parent_json;
        recursion_depth++;
    }
}

/**
 * @brief If no elements exist, tries to pull them from the first override model.
 * @param client The network client.
 */
void ModelGenerator::merge_override_model(Client& client)
{
    if(model_json.contains("elements"))
    {
        return;
    }

    if(!model_json.contains("overrides") || !model_json["overrides"].is_array() || model_json["overrides"].empty())
    {
        return;
    }

    auto first_override = model_json["overrides"][0];
    if(!first_override.contains("model"))
    {
        return;
    }

    std::string override_path = first_override["model"].get<std::string>();
    std::string ns = "minecraft";
    std::string path = override_path;
    size_t colon = override_path.find(':');
    if(colon != std::string::npos)
    {
        ns = override_path.substr(0, colon);
        path = override_path.substr(colon + 1);
    }

    std::string url = "/api/client/assets/get/" + ns + "/models/" + path + ".json";
    std::string override_data;
    if(!client.sendHttpRequest("GET", url, "", override_data))
    {
        return;
    }

    auto override_json = nlohmann::json::parse(override_data, nullptr, false, true);
    if(override_json.is_discarded())
    {
        return;
    }

    if(override_json.contains("elements"))
    {
        model_json["elements"] = override_json["elements"];
    }

    if(override_json.contains("textures"))
    {
        for(auto& [key, val] : override_json["textures"].items())
        {
            if(!model_json["textures"].contains(key))
            {
                model_json["textures"][key] = val;
            }
        }
    }
}

/**
 * @brief Resolves all texture references and downloads missing textures.
 * @param client The network client.
 */
void ModelGenerator::resolve_and_load_textures(Client& client)
{
    if(!model_json.contains("textures"))
    {
        return;
    }

    for(auto& [key, texture_ref] : model_json["textures"].items())
    {
        std::string texture_path = texture_ref.get<std::string>();
        int ref_limit = 0;
        while(!texture_path.empty() && texture_path[0] == '#' && ref_limit < 10)
        {
            std::string ref = texture_path.substr(1);
            if(model_json["textures"].contains(ref))
            {
                texture_path = model_json["textures"][ref];
            }
            else
            {
                break;
            }
            ref_limit++;
        }

        if(uniqueTexturePaths.find(texture_path) != uniqueTexturePaths.end())
        {
            continue;
        }
        uniqueTexturePaths.insert(texture_path);

        TextureAnimation anim_data;
        if(!try_load_texture_full(client, texture_path, anim_data))
        {
            continue;
        }
        textures[texture_path] = anim_data;
    }
}

/**
 * @brief Clears alpha for specific pixels on bucket textures to simulate fluid container.
 */
void ModelGenerator::apply_fluid_container_alpha()
{
    if(!model_json.value("is_fluid_container", false))
    {
        return;
    }

    std::string bucket_tex = "minecraft:item/bucket";
    if(model_json.contains("textures") && model_json["textures"].contains("layer0"))
    {
        bucket_tex = model_json["textures"]["layer0"];
    }

    auto it = textures.find(bucket_tex);
    if(it == textures.end())
    {
        return;
    }

    auto& tex = it->second.texture;
    auto clear_block = [&](int x, int y) {
        unsigned int x_start = (x * tex.size.x) / 16;
        unsigned int x_end = ((x + 1) * tex.size.x) / 16;
        unsigned int y_start = (y * tex.size.y) / 16;
        unsigned int y_end = ((y + 1) * tex.size.y) / 16;
        for(unsigned int ty = y_start; ty < y_end; ty++)
        {
            for(unsigned int tx = x_start; tx < x_end; tx++)
            {
                if(tx < tex.size.x && ty < tex.size.y)
                {
                    tex.pixels[(ty * tex.size.x + tx) * 4 + 3] = 0;
                }
            }
        }
    };

    for(int x = 5; x <= 10; x++) clear_block(x, 3);
    for(int x = 4; x <= 11; x++) clear_block(x, 4);
    for(int x = 5; x <= 10; x++) clear_block(x, 5);
}

/**
 * @brief Computes UV coordinates for a single face.
 * @param face_json The JSON describing the face (may contain "uv" and "rotation").
 * @param face The face geometry (direction and vertices).
 * @param v_scale Vertical scale factor (frame height / texture height).
 * @param uv Output array of 4 UV coordinates.
 */
void ModelGenerator::compute_uv_for_face(const nlohmann::json& face_json, const FaceDef& face,
    float v_scale, sf::Vector2f uv[4])
{
    if(face_json.contains("uv"))
    {
        float u1 = face_json["uv"][0], v1 = face_json["uv"][1];
        float u2 = face_json["uv"][2], v2 = face_json["uv"][3];
        sf::Vector2f base[4] = {
            { u1 / 16.0f, v1 / 16.0f * v_scale },
            { u2 / 16.0f, v1 / 16.0f * v_scale },
            { u2 / 16.0f, v2 / 16.0f * v_scale },
            { u1 / 16.0f, v2 / 16.0f * v_scale }
        };
        int uv_rot = face_json.value("rotation", 0);
        int shift = (4 - (uv_rot / 90) % 4) % 4;
        for(int i = 0; i < 4; i++)
        {
            uv[i] = base[(i + shift) % 4];
        }
        return;
    }

    for(int i = 0; i < 4; i++)
    {
        float ux, vy;
        if(face.dir == "up" || face.dir == "down")
        {
            ux = face.v[i].x;
            vy = face.v[i].z;
        }
        else if(face.dir == "north" || face.dir == "south")
        {
            ux = face.v[i].x;
            vy = 16.0f - face.v[i].y;
        }
        else
        {
            ux = face.v[i].z;
            vy = 16.0f - face.v[i].y;
        }
        uv[i] = { ux / 16.0f, vy / 16.0f * v_scale };
    }
}

/**
 * @brief Processes one element from the model JSON and appends triangles to the list.
 * @param element The JSON element.
 * @param is_flat True if the model is a flat 2D item.
 * @param scale Scale factor for projection.
 * @param cx Screen X centre.
 * @param cy Screen Y centre.
 * @param pitch Pitch rotation (custom) in degrees.
 * @param yaw Yaw rotation (custom) in degrees.
 * @param triangles Output list of triangles.
 */
void ModelGenerator::add_triangles_from_element(const nlohmann::json& element, bool is_flat,
    float scale, float cx, float cy,
    float pitch, float yaw,
    std::vector<Triangle>& triangles)
{
    auto from = element["from"], to = element["to"];
    sf::Vector3f p_min(from[0], from[1], from[2]);
    sf::Vector3f p_max(to[0], to[1], to[2]);

    sf::Vector3f rot_origin(8, 8, 8);
    std::string rot_axis = "y";
    float rot_angle = 0;
    if(element.contains("rotation"))
    {
        auto& rot = element["rotation"];
        rot_origin = { rot["origin"][0], rot["origin"][1], rot["origin"][2] };
        rot_axis = rot.value("axis", "y");
        rot_angle = rot.value("angle", 0.0f);
    }

    FaceDef faces[] = {
        { "up", { { p_min.x, p_max.y, p_min.z }, { p_max.x, p_max.y, p_min.z }, { p_max.x, p_max.y, p_max.z }, { p_min.x, p_max.y, p_max.z } } },
        { "north", { { p_max.x, p_max.y, p_min.z }, { p_min.x, p_max.y, p_min.z }, { p_min.x, p_min.y, p_min.z }, { p_max.x, p_min.y, p_min.z } } },
        { "east", { { p_max.x, p_max.y, p_max.z }, { p_max.x, p_max.y, p_min.z }, { p_max.x, p_min.y, p_min.z }, { p_max.x, p_min.y, p_max.z } } },
        { "west", { { p_min.x, p_max.y, p_min.z }, { p_min.x, p_max.y, p_max.z }, { p_min.x, p_min.y, p_max.z }, { p_min.x, p_min.y, p_min.z } } },
        { "south", { { p_min.x, p_max.y, p_max.z }, { p_max.x, p_max.y, p_max.z }, { p_max.x, p_min.y, p_max.z }, { p_min.x, p_min.y, p_max.z } } },
        { "down", { { p_min.x, p_min.y, p_min.z }, { p_max.x, p_min.y, p_min.z }, { p_max.x, p_min.y, p_max.z }, { p_min.x, p_min.y, p_max.z } } }
    };

    for(const auto& face : faces)
    {
        if(!element.contains("faces") || !element["faces"].contains(face.dir))
        {
            continue;
        }
        auto face_json = element["faces"][face.dir];
        std::string tex_ref = face_json.value("texture", "");
        if(tex_ref.empty())
        {
            continue;
        }

        int depth = 0;
        while(!tex_ref.empty() && tex_ref[0] == '#' && depth < 5)
        {
            std::string key = tex_ref.substr(1);
            if(model_json.contains("textures") && model_json["textures"].contains(key))
            {
                tex_ref = model_json["textures"][key];
            }
            else
            {
                break;
            }
            depth++;
        }

        auto it = textures.find(tex_ref);
        if(it == textures.end())
        {
            continue;
        }
        const Texture* tex = &it->second.texture;
        if(tex->size.x == 0 || tex->size.y == 0)
        {
            continue;
        }

        float v_scale = (float)it->second.frameHeight / tex->size.y;
        sf::Vector2f uv[4];
        compute_uv_for_face(face_json, face, v_scale, uv);

        sf::Color shade = get_face_shading(face.dir, is_flat);
        sf::Vector3f screen_pos[4];
        float avg_z = 0;
        for(int i = 0; i < 4; i++)
        {
            sf::Vector3f rot_v = rotate_point(face.v[i], rot_origin, rot_axis, rot_angle);
            screen_pos[i] = project_3d(rot_v.x, rot_v.y, rot_v.z, pitch, yaw, scale, cx, cy);
            screen_pos[i].z += i * 0.005f;
            avg_z += screen_pos[i].z;
        }
        avg_z /= 4.0f;

        Triangle tri1, tri2;
        tri1.texture = tri2.texture = tex;
        tri1.depth = tri2.depth = avg_z;
        for(int i = 0; i < 3; i++)
        {
            tri1.v[i] = { screen_pos[i].x, screen_pos[i].y, screen_pos[i].z,
                shade.r / 255.0f, shade.g / 255.0f, shade.b / 255.0f, 1.0f,
                uv[i].x, uv[i].y };
        }
        for(int i = 0; i < 3; i++)
        {
            int idx = (i == 0) ? 2 : (i == 1) ? 3 : 0;
            tri2.v[i] = { screen_pos[idx].x, screen_pos[idx].y, screen_pos[idx].z,
                shade.r / 255.0f, shade.g / 255.0f, shade.b / 255.0f, 1.0f,
                uv[idx].x, uv[idx].y };
        }
        triangles.push_back(tri1);
        triangles.push_back(tri2);
    }
}

/**
 * @brief Calculates the vertical UV offset for an animated texture at a given tick.
 * @param anim The animation data.
 * @param tex The texture (needed for dimensions).
 * @param tick The current animation tick.
 * @return The Y offset for texture coordinates.
 */
float ModelGenerator::compute_animation_offset(const TextureAnimation& anim, const Texture* tex, int tick)
{
    if(!anim.isAnimated) return 0.0f;
    int duration = anim.getTotalDuration();
    if(duration <= 0) return 0.0f;
    int local_tick = tick % duration;
    int frame_index = 0;
    if(anim.sequence.empty())
    {
        int total_frames = tex->size.y / anim.frameHeight;
        if(total_frames > 0)
        {
            frame_index = (local_tick / anim.defaultFrameTime) % total_frames;
        }
    }
    else
    {
        int acc = 0;
        for(const auto& fr : anim.sequence)
        {
            acc += fr.time;
            if(local_tick < acc)
            {
                frame_index = fr.index;
                break;
            }
        }
    }
    return (frame_index * anim.frameHeight) / (float)tex->size.y;
}

/**
 * @brief Renders a sequence of frames for an animated model into an FBO and captures them.
 * @param triangles The triangle list (already sorted).
 * @param gl_textures Map of texture pointers to OpenGL texture IDs.
 * @param shader_program The shader program to use.
 * @param fbo The target framebuffer.
 * @param vao The VAO containing the batch.
 * @param outputSize Width and height of the output frames.
 * @param any_animated True if any texture is animated.
 * @param total_ticks The total number of ticks to simulate.
 * @param out_frames Output vector of rendered frames (appended).
 */
void ModelGenerator::render_sequence_frames(const std::vector<Triangle>& triangles,
    const std::map<const Texture*, GLuint>& gl_textures,
    GLuint shader_program, GLuint fbo, GLuint vao,
    int outputSize, bool any_animated, int total_ticks,
    std::vector<RenderedFrame>& out_frames)
{
    std::vector<int> prev_frame_indices;
    GLint tex_offset_loc = glGetUniformLocation(shader_program, "uTexOffset");

    for(int tick = 0; tick < total_ticks; tick++)
    {
        std::vector<int> current_frame_indices;
        if(any_animated)
        {
            for(const auto& [path, anim] : textures)
            {
                if(!anim.isAnimated) continue;
                int dur = anim.getTotalDuration();
                if(dur <= 0) continue;
                int local_tick = tick % dur;
                int frame_idx = 0;
                if(anim.sequence.empty())
                {
                    int total_frames = anim.texture.size.y / anim.frameHeight;
                    if(total_frames > 0)
                    {
                        frame_idx = (local_tick / anim.defaultFrameTime) % total_frames;
                    }
                }
                else
                {
                    int acc = 0;
                    for(const auto& fr : anim.sequence)
                    {
                        acc += fr.time;
                        if(local_tick < acc)
                        {
                            frame_idx = fr.index;
                            break;
                        }
                    }
                }
                current_frame_indices.push_back(frame_idx);
            }
            if(tick > 0 && current_frame_indices == prev_frame_indices)
            {
                out_frames.back().timeInTicks++;
                prev_frame_indices = current_frame_indices;
                continue;
            }
            prev_frame_indices = current_frame_indices;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(vao);

        size_t vertex_offset = 0;
        const Texture* current_tex = nullptr;
        GLuint current_gl_tex = 0;
        int batch_start = 0, batch_count = 0;

        for(size_t i = 0; i < triangles.size(); i++)
        {
            const Triangle& tri = triangles[i];
            if(tri.texture != current_tex)
            {
                if(batch_count > 0)
                {
                    glBindTexture(GL_TEXTURE_2D, current_gl_tex);
                    float offset_y = 0.0f;
                    if(any_animated)
                    {
                        for(auto& [path, anim] : textures)
                        {
                            if(&anim.texture == current_tex && anim.isAnimated)
                            {
                                offset_y = compute_animation_offset(anim, current_tex, tick);
                                break;
                            }
                        }
                    }
                    glUniform2f(tex_offset_loc, 0.0f, offset_y);
                    glDrawArrays(GL_TRIANGLES, batch_start, batch_count * 3);
                }
                current_tex = tri.texture;
                current_gl_tex = gl_textures.at(current_tex);
                batch_start = vertex_offset;
                batch_count = 1;
            }
            else
            {
                batch_count++;
            }
            vertex_offset += 3;
        }
        if(batch_count > 0)
        {
            glBindTexture(GL_TEXTURE_2D, current_gl_tex);
            float offset_y = 0.0f;
            if(any_animated)
            {
                for(auto& [path, anim] : textures)
                {
                    if(&anim.texture == current_tex && anim.isAnimated)
                    {
                        offset_y = compute_animation_offset(anim, current_tex, tick);
                        break;
                    }
                }
            }
            glUniform2f(tex_offset_loc, 0.0f, offset_y);
            glDrawArrays(GL_TRIANGLES, batch_start, batch_count * 3);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::vector<unsigned char> pixels(outputSize * outputSize * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, outputSize, outputSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        sf::Image img({ static_cast<unsigned int>(outputSize), static_cast<unsigned int>(outputSize) }, pixels.data());
        img.flipHorizontally();
        out_frames.push_back({ img, 1 });

        if(!any_animated) break;
    }
}

/**
 * @brief Ensures the model JSON contains an "elements" array, creating defaults if missing.
 * @param is_flat True if the model is a flat 2D item; false for 3D.
 */
void ModelGenerator::ensure_elements_exist(bool is_flat)
{
    if(model_json.contains("elements"))
    {
        return;
    }

    if(is_flat)
    {
        auto arr = nlohmann::json::array();
        for(int i = 0; i < 10; i++)
        {
            std::string layer_key = "layer" + std::to_string(i);
            if(!model_json["textures"].contains(layer_key)) continue;
            nlohmann::json item;
            if(model_json.value("is_fluid_container", false) && i == 1)
            {
                item["from"] = { 4, 3, 0.0f };
                item["to"] = { 12, 13, 0.0f };
                item["faces"]["north"] = { { "texture", "#" + layer_key }, { "uv", { 4, 3, 12, 13 } } };
            }
            else
            {
                item["from"] = { 0, 0, (i + 1) * 1.0f };
                item["to"] = { 16, 16, (i + 1) * 1.0f };
                item["faces"]["north"] = { { "texture", "#" + layer_key }, { "uv", { 0, 0, 16, 16 } } };
            }
            arr.push_back(item);
        }
        if(arr.empty())
        {
            nlohmann::json fallback;
            fallback["from"] = { 0, 0, 0 };
            fallback["to"] = { 16, 16, 0 };
            fallback["faces"]["north"] = { { "texture", "#layer0" }, { "uv", { 0, 0, 16, 16 } } };
            arr.push_back(fallback);
        }
        model_json["elements"] = arr;
        return;
    }

    // 3D fallback cube
    nlohmann::json cube;
    cube["from"] = {0, 0, 0};
    cube["to"] = {16, 16, 16};

    auto find_texture = [&](const std::string& dir, const std::vector<std::string>& fb) -> std::string {
        if(model_json["textures"].contains(dir))
        {
            return "#" + dir;
        }

        for(const auto& k : fb)
        {
            if(model_json["textures"].contains(k)) return "#" + k;
        }

        if(model_json["textures"].contains("all"))
        {
            return "#all";
        }

        if(model_json["textures"].contains("particle"))
        {
            return "#particle";
        }

        if(!model_json["textures"].empty())
        {
            return "#" + model_json["textures"].begin().key();
        }
        return "";
    };

    std::string t_up = find_texture("up", { "top" });
    std::string t_down = find_texture("down", { "bottom" });
    std::string t_north = find_texture("north", { "front", "side" });
    std::string t_south = find_texture("south", { "back", "side" });
    std::string t_east = find_texture("east", { "side" });
    std::string t_west = find_texture("west", { "side" });

    if(!t_up.empty())
    {
        cube["faces"]["up"] = {{ 
            "texture", t_up 
        }};
    }

    if(!t_down.empty()) 
    {
        cube["faces"]["down"] = {{
            "texture", t_down 
        }};
    }

    if(!t_north.empty())
    {
        cube["faces"]["north"] = {{
            "texture", t_north
        }};
    }

    if(!t_south.empty())
    {
        cube["faces"]["south"] = {{
            "texture", t_south
        }};
    }

    if(!t_east.empty())
    {
        cube["faces"]["east"] = {{
            "texture", t_east
        }};
    }

    if(!t_west.empty())
    {
        cube["faces"]["west"] = {{
            "texture", t_west
        }};
    }

    model_json["elements"] = nlohmann::json::array({ cube });
}

ModelGenerator::ModelGenerator(const std::string& raw_json, Client& client, const std::string& id)
{
    model_json = nlohmann::json::parse(raw_json, nullptr, false, true);
    if(model_json.is_discarded()) 
    {
        return;
    }

    this->id = id;

    process_fluid_container();
    merge_parent_models(client);
    merge_override_model(client);
    resolve_and_load_textures(client);
    apply_fluid_container_alpha();
}

ModelGenerator::ModelGenerator(const std::string& obj_data, const std::string& mtl_data, Client& client, const std::string& namespac, const std::string& id)
{
    this->id = id;
    std::string warn, err;
    std::istringstream obj_stream(obj_data);

    class MemMaterialReader : public tinyobj::MaterialReader
    {
      public:
        std::string mtlData;
        MemMaterialReader(std::string data) : mtlData(std::move(data))
        {}
        bool operator()(const std::string&, std::vector<tinyobj::material_t>* materials,
            std::map<std::string, int>* matMap, std::string* warn, std::string* err) override
        {
            std::istringstream mtl_stream(mtlData);
            tinyobj::LoadMtl(matMap, materials, &mtl_stream, warn, err);
            return true;
        }
    } mat_reader(mtl_data);

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &obj_stream, &mat_reader, true);
    if(!warn.empty())
    {
        std::cout << "WARN: " << warn << std::endl;
    }

    if(!err.empty())
    {
        std::cerr << "ERR: " << err << std::endl;
    }

    if(!ret)
    {
        return;
    }

    for(const auto& mat : materials)
    {
        std::string tex_name = mat.diffuse_texname;
        if(tex_name.empty())
        {
            continue;
        }

        std::string ns = namespac;
        std::string path = tex_name;
        size_t colon_pos = tex_name.find(':');
        if(colon_pos != std::string::npos)
        {
            ns = tex_name.substr(0, colon_pos);
            path = tex_name.substr(colon_pos + 1);
        }

        size_t dot_pos = path.rfind('.');
        if(dot_pos != std::string::npos && path.substr(dot_pos) == ".png")
        {
            path = path.substr(0, dot_pos);
        }

        std::vector<std::string> urls;
        if(path.find("textures/") == 0)
        {
            urls.emplace_back("/api/client/assets/get/" + ns + "/" + path + ".png");
        }
        else
        {
            urls.emplace_back("/api/client/assets/get/" + ns + "/textures/" + path + ".png");
            urls.emplace_back("/api/client/assets/get/" + ns + "/textures/block/" + path + ".png");
            urls.emplace_back("/api/client/assets/get/" + ns + "/textures/item/" + path + ".png");
            if(ns != "minecraft")
            {
                urls.emplace_back("/api/client/assets/get/minecraft/textures/block/" + path + ".png");
            }
        }

        std::string img_data, dummy_url;
        if(!try_load_png(client, urls, img_data, dummy_url))
        {
            std::cerr << "Failed to load texture for OBJ: " << tex_name << std::endl;
            continue;
        }

        sf::Image img;
        if(!img.loadFromMemory(img_data.data(), img_data.size()))
        {
            continue;
        }

        TextureAnimation anim;
        anim.texture.size.x = img.getSize().x;
        anim.texture.size.y = img.getSize().y;
        const unsigned char* raw = img.getPixelsPtr();
        size_t total = anim.texture.size.x * anim.texture.size.y * 4;
        if(raw && total > 0)
        {
            anim.texture.pixels.assign(raw, raw + total);
        }
        anim.frameHeight = anim.texture.size.y;
        textures[tex_name] = anim;
    }
}

int ModelGenerator::calculateTotalLoopTicks()
{
    int total_ticks = 1;
    for(const auto& [path, data] : textures)
    {
        if(data.isAnimated)
        {
            int duration = data.getTotalDuration();
            if(duration > 0)
            {
                total_ticks = std::lcm(total_ticks, duration);
            }
        }
    }
    return total_ticks;
}

/**
 * @brief Generate a sequence of images to display (isometric or custom rotation).
 * @param outputSize Output size of the images (default: 128x128).
 * @param customRotation Use custom 3D rotation instead of default isometric (default: false).
 * @param pitch X-axis rotation in degrees (default: 30.0).
 * @param yaw Y-axis rotation in degrees (default: 45.0).
 * @param wireframe Render in wireframe mode (default: false).
 * @return vector containing 1 or more images to display.
 */
std::vector<RenderedFrame> ModelGenerator::generateIsometricSequence(unsigned int output_size, float pitch, float yaw, bool wireframe)
{
    bool is_flat = false;
    if(model_json.contains("parent") && model_json["parent"].get<std::string>().find("item/generated") != std::string::npos)
    {
        is_flat = true;
    }

    if(model_json.contains("textures") && model_json["textures"].contains("layer0"))
    {
        is_flat = true;
    }

    ensure_elements_exist(is_flat);

    float scale, cx, cy;
    if(is_flat)
    {
        scale = (float)output_size / 16.0f;
        cx = output_size / 2.0f;
        cy = output_size / 2.0f;
        pitch = 0.0f;
        yaw = 0.0f;
    }
    else
    {
        scale = (float)output_size / 38.0f;
        cx = output_size / 2.0f;
        cy = (output_size / 2.0f) + (scale * 2.0f);
    }

    /*
    if(is_flat && !allow_free)
    {
        scale = (float)output_size / 16.0f;
        cx = cy = 0;
    }
    else if(is_flat && allow_free)
    {
        scale = (float)output_size / 16.0f;
        cx = output_size / 2.0f;
        cy = output_size / 2.0f;
    }
    else
    {
        scale = (float)output_size / 38.0f;
        cx = output_size / 2.0f;
        cy = (output_size / 2.0f) + (scale * 2.0f);
    }
    */

    std::vector<Triangle> triangles;
    for(const auto& elem : model_json["elements"])
    {
        add_triangles_from_element(elem, is_flat, scale, cx, cy, pitch, yaw, triangles);
    }

    if(triangles.empty()) return {};

    std::stable_sort(triangles.begin(), triangles.end(),
        [](const Triangle& a, const Triangle& b) {
            return a.depth < b.depth;
        });

    glm::mat4 proj = glm::ortho(0.0f, (float)output_size, 0.0f, (float)output_size, -300.0f, 300.0f);

    thread_local GLuint shader_program = 0;
    if(shader_program == 0)
    {
        shader_program = create_shader_program();
    }

    // Framebuffer creation
    FramebufferObjects fb;
    glGenFramebuffers(1, &fb.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glGenTextures(1, &fb.render_tex);
    glBindTexture(GL_TEXTURE_2D, fb.render_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, output_size, output_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.render_tex, 0);
    glGenRenderbuffers(1, &fb.rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, output_size, output_size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.rbo);
    
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer not complete!" << std::endl;
        glDeleteFramebuffers(1, &fb.fbo);
        glDeleteTextures(1, &fb.render_tex);
        glDeleteRenderbuffers(1, &fb.rbo);
        return {};
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::map<const Texture*, GLuint> gl_textures;
    for(auto& [path, anim] : textures)
    {
        if(!anim.texture.pixels.empty())
        {
            gl_textures[&anim.texture] = create_gl_texture(anim.texture.pixels, anim.texture.size.x, anim.texture.size.y);
        }
    }

    std::vector<GpuVertex> all_vertices;
    for(const auto& tri : triangles)
    {
        for(int i = 0; i < 3; i++)
        {
            all_vertices.push_back(tri.v[i]);
        }
    }
        

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, all_vertices.size() * sizeof(GpuVertex), all_vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    OpenGLState saved_state;
    glGetIntegerv(GL_VIEWPORT, saved_state.viewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved_state.prev_fbo);
    saved_state.depth_test = glIsEnabled(GL_DEPTH_TEST);
    saved_state.blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &saved_state.blend_src);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &saved_state.blend_dst);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, output_size, output_size);
    if(wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
    }

    glUseProgram(shader_program);
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(shader_program, "uWireframe"), wireframe ? 1 : 0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    bool any_animated = std::any_of(textures.begin(), textures.end(),
        [](const auto& p){
            return p.second.isAnimated;
        });
    int total_ticks = calculateTotalLoopTicks();

    std::vector<RenderedFrame> result;
    render_sequence_frames(triangles, gl_textures, shader_program, fb.fbo, vao, output_size,
                           any_animated, total_ticks, result);
    //
    if(wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glEnable(GL_CULL_FACE);
    glViewport(saved_state.viewport[0], saved_state.viewport[1], saved_state.viewport[2], saved_state.viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, saved_state.prev_fbo);
    if(!saved_state.depth_test)
    {
        glDisable(GL_DEPTH_TEST);
    }

    if(saved_state.blend)
    {
        glBlendFunc(saved_state.blend_src, saved_state.blend_dst);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    glUseProgram(0);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    for(auto& [tex, tex_id] : gl_textures)
    {
        glDeleteTextures(1, &tex_id);
    }
    glDeleteFramebuffers(1, &fb.fbo);
    glDeleteTextures(1, &fb.render_tex);
    glDeleteRenderbuffers(1, &fb.rbo);

    return result;
}

/**
 * @brief Generate a sequence of images to display from an OBJ model (single frame).
 * @param outputSize Output size of the images (default: 128x128).
 * @param customRotation Use custom 3D rotation instead of default isometric (default: false).
 * @param pitch X-axis rotation in degrees (default: 30.0).
 * @param yaw Y-axis rotation in degrees (default: 45.0).
 * @param wireframe Render in wireframe mode (default: false).
 * @return vector containing 1 image (no animation support).
 */
std::vector<RenderedFrame> ModelGenerator::generateIsometricSequenceOBJ(unsigned int output_size, float pitch, float yaw, bool wireframe)
{
    if(shapes.empty())
    {
        return {};
    }

    // Bounding box
    float min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9, min_z = 1e9, max_z = -1e9;
    for(size_t i = 0; i < attrib.vertices.size() / 3; i++)
    {
        float vx = attrib.vertices[3 * i], vy = attrib.vertices[3 * i + 1], vz = attrib.vertices[3 * i + 2];
        if(vx < min_x) min_x = vx;
        if(vx > max_x) max_x = vx;
        if(vy < min_y) min_y = vy;
        if(vy > max_y) max_y = vy;
        if(vz < min_z) min_z = vz;
        if(vz > max_z) max_z = vz;
    }

    float lcx = (min_x + max_x) / 2.0f, lcy = (min_y + max_y) / 2.0f, lcz = (min_z + max_z) / 2.0f;
    auto project_raw = [&](float vx, float vy, float vz) -> sf::Vector3f {
        return project_3d(vx, vy, vz, pitch, yaw, 1.0f, 0.0f, 0.0f, lcx, lcy, lcz);
    };

    float max_dist_sq = 0;
    for(size_t i = 0; i < attrib.vertices.size() / 3; i++)
    {
        float dx = attrib.vertices[3 * i] - lcx, dy = attrib.vertices[3 * i + 1] - lcy, dz = attrib.vertices[3 * i + 2] - lcz;
        float d2 = dx * dx + dy * dy + dz * dz;
        if(d2 > max_dist_sq) max_dist_sq = d2;
    }

    float radius = std::sqrt(max_dist_sq);
    if(radius < 0.001f)
    {
        radius = 1.0f;
    }

    float final_scale = (output_size * 0.85f) / (radius * 2.0f);

    auto project_to_screen = [&](float vx, float vy, float vz) -> sf::Vector3f {
        sf::Vector3f cp = project_raw(lcx, lcy, lcz);
        sf::Vector3f vp = project_raw(vx, vy, vz);
        return { (float)output_size / 2.0f + (vp.x - cp.x) * final_scale,
            (float)output_size / 2.0f + (vp.y - cp.y) * final_scale, vp.z };
    };

    auto get_texture = [&](int mat_id) -> const Texture* {
        if(mat_id < 0 || static_cast<size_t>(mat_id) >= materials.size())
        {
            return nullptr;
        }

        std::string raw_name = materials[mat_id].diffuse_texname;
        if(raw_name.empty())
        {
            return nullptr;
        }
        if(textures.count(raw_name))
        {
            return &textures[raw_name].texture;
        }

        std::string clean = clean_texture_name(raw_name);
        for(auto& [k, v] : textures)
        {
            if(clean_texture_name(k) == clean)
            {
                return &v.texture;
            }
        }
            
        return nullptr;
    };
    std::vector<Triangle> triangles;
    for(const auto& shape : shapes)
    {
        size_t idx_off = 0;
        const auto& num_face_vertices = shape.mesh.num_face_vertices;
        for(size_t f = 0; f < num_face_vertices.size(); f++)
        {
            int fv = num_face_vertices[f];
            if(fv != 3)
            {
                idx_off += fv;
                continue;
            }

            const Texture* tex = get_texture(shape.mesh.material_ids[f]);

            Triangle tri;
            tri.texture = tex;
            float avg_z = 0.0f;
            for(int v = 0; v < 3; v++)
            {
                tinyobj::index_t idx = shape.mesh.indices[idx_off + v];
                float vx = attrib.vertices[3 * idx.vertex_index];
                float vy = attrib.vertices[3 * idx.vertex_index + 1];
                float vz = attrib.vertices[3 * idx.vertex_index + 2];
                sf::Vector3f screen = project_to_screen(vx, vy, vz);
                avg_z += screen.z;

                GpuVertex gv;
                gv.x = screen.x;
                gv.y = screen.y;
                gv.z = screen.z;
                gv.r = gv.g = gv.b = gv.a = 1.0f;
                if(tex && idx.texcoord_index >= 0)
                {
                    gv.u = attrib.texcoords[2 * idx.texcoord_index];
                    gv.v = 1.0f - attrib.texcoords[2 * idx.texcoord_index + 1];
                }
                else
                {
                    gv.u = gv.v = 0.0f;
                }
                tri.v[v] = gv;
            }
            tri.depth = avg_z / 3.0f;
            triangles.push_back(tri);
            idx_off += fv;
        }
    }

    if(triangles.empty()) return {};

    std::sort(triangles.begin(), triangles.end(), [](const Triangle& a, const Triangle& b) {
        return a.depth < b.depth;
    });

    glm::mat4 proj = glm::ortho(0.0f, (float)output_size, 0.0f, (float)output_size, -1000.0f, 1000.0f);
    thread_local GLuint shader = 0;
    if(!shader)
    {
        shader = create_shader_program();
    }

    FramebufferObjects fb;
    glGenFramebuffers(1, &fb.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glGenTextures(1, &fb.render_tex);
    glBindTexture(GL_TEXTURE_2D, fb.render_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, output_size, output_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.render_tex, 0);
    glGenRenderbuffers(1, &fb.rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, output_size, output_size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.rbo);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer not complete!" << std::endl;
        glDeleteFramebuffers(1, &fb.fbo);
        glDeleteTextures(1, &fb.render_tex);
        glDeleteRenderbuffers(1, &fb.rbo);
        return {};
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::map<const Texture*, GLuint> gl_tex;
    for(auto& [path, anim] : textures)
    {
        if(!anim.texture.pixels.empty())
        {
            gl_tex[&anim.texture] = create_gl_texture(anim.texture.pixels, anim.texture.size.x, anim.texture.size.y);
        }
    }

    std::vector<GpuVertex> verts;
    for(const auto& t : triangles)
    {
        for(int i = 0; i < 3; i++)
        {
            verts.push_back(t.v[i]);
        }
    }
        

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GpuVertex), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    OpenGLState saved;
    glGetIntegerv(GL_VIEWPORT, saved.viewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved.prev_fbo);
    saved.depth_test = glIsEnabled(GL_DEPTH_TEST);
    saved.blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &saved.blend_src);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &saved.blend_dst);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, output_size, output_size);
    if(wireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);
    }

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(shader, "uWireframe"), wireframe ? 1 : 0);
    GLint offset_loc = glGetUniformLocation(shader, "uTexOffset");

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(vao);

    size_t vert_off = 0;
    const Texture* cur_tex = nullptr;
    GLuint cur_gl_tex = 0;
    int batch_start = 0, batch_cnt = 0;

    for(size_t i = 0; i < triangles.size(); i++)
    {
        const Triangle& tri = triangles[i];
        if(tri.texture != cur_tex)
        {
            if(batch_cnt > 0)
            {
                glBindTexture(GL_TEXTURE_2D, cur_gl_tex);
                glUniform2f(offset_loc, 0.0f, 0.0f);
                glDrawArrays(GL_TRIANGLES, batch_start, batch_cnt * 3);
            }
            cur_tex = tri.texture;
            cur_gl_tex = cur_tex ? gl_tex.at(cur_tex) : 0;
            batch_start = vert_off;
            batch_cnt = 1;
        }
        else
        {
            batch_cnt++;
        }
        vert_off += 3;
    }
    if(batch_cnt > 0)
    {
        glBindTexture(GL_TEXTURE_2D, cur_gl_tex);
        glUniform2f(offset_loc, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLES, batch_start, batch_cnt * 3);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::vector<unsigned char> pixels(output_size * output_size * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.fbo);
    glReadPixels(0, 0, output_size, output_size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    sf::Image img({ output_size, output_size }, pixels.data());
    img.flipHorizontally();

    if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glViewport(saved.viewport[0], saved.viewport[1], saved.viewport[2], saved.viewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, saved.prev_fbo);
    if(!saved.depth_test)
    {
        glDisable(GL_DEPTH_TEST);
    }

    if(saved.blend)
    {
        glBlendFunc(saved.blend_src, saved.blend_dst);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    glUseProgram(0);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    for(auto& [tex, id] : gl_tex)
    {
        glDeleteTextures(1, &id);
    }

    glDeleteFramebuffers(1, &fb.fbo);
    glDeleteTextures(1, &fb.render_tex);
    glDeleteRenderbuffers(1, &fb.rbo);

    return { { img, 1 } };
}

std::string ModelGenerator::getID() const
{
    return id;
}