#pragma once

#include "client.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Image.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "tiny_obj_loader.h"
#include <string>
#include <vector>
#include <map>
#include <set>

typedef int GLint;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;

struct RenderedFrame
{
    sf::Image image;
    int timeInTicks = 1;
};

struct AnimationFrame
{
    int index;
    int time;
};

struct Texture
{
    struct Size
    {
        unsigned int x = 0;
        unsigned int y = 0;
    };

    Size size = {};
    std::vector<uint8_t> pixels;

    Size getSize() const
    {
        return size;
    }

    sf::Image copyToImage() const
    {
        if(pixels.empty()) return {};
        sf::Image img({size.x, size.y}, pixels.data());
        return img;
    }

    void rotate90Clockwise()
    {
        if(pixels.empty() || size.x == 0 || size.y == 0) return;
        
        std::vector<uint8_t> newPixels;
        newPixels.resize(pixels.size());
        for(unsigned int y = 0; y < size.y; ++y)
        {
            for(unsigned int x = 0; x < size.x; ++x)
            {
                unsigned int newX = size.y - 1 - y;
                unsigned int newY = x;
                
                unsigned int oldIdx = (y * size.x + x) * 4;
                unsigned int newIdx = (newY * size.y + newX) * 4; 
                
                newPixels[newIdx] = pixels[oldIdx];
                newPixels[newIdx + 1] = pixels[oldIdx + 1];
                newPixels[newIdx + 2] = pixels[oldIdx + 2];
                newPixels[newIdx + 3] = pixels[oldIdx + 3];
            }
        }
        pixels = std::move(newPixels);
        std::swap(size.x, size.y);
    }
};

struct TextureAnimation
{
    Texture texture;
    bool isAnimated = false;
    unsigned int frameHeight;
    
    int defaultFrameTime = 1; 
    std::vector<AnimationFrame> sequence; 
    std::string rawMcmeta;

    int getTotalDuration() const
    {
        if(!sequence.empty())
        {
            int total = 0;
            for(const auto& f : sequence) total += f.time;
            return total;
        }
        if(frameHeight == 0) return 0;
        int physicalFrames = texture.getSize().y / frameHeight;
        return physicalFrames * defaultFrameTime;
    }
};

inline std::string changeFilename(const std::string& input)
{
    std::string output = input;
    std::replace(output.begin(), output.end(), ':', '_');
    std::replace(output.begin(), output.end(), '/', '_');
    return output;
}

/**
 * @brief Based on the following:
 * url: https://www.gamedev.net/blogs/entry/2250273-isometric-map-in-sfml/
 * url: https://stackoverflow.com/questions/14179931/sfml-generate-isometric-tile
 * url: https://stackoverflow.com/questions/33906516/2d-isometric-sfml-right-formulas-wrong-coordinate-range
 */
class ModelGenerator
{
public:
    ModelGenerator() = default;

    ModelGenerator(const std::string& raw_json, Client& client, const std::string& id);

    ModelGenerator(const std::string& obj_data, const std::string& mtl_data, Client& client, const std::string& namespac, const std::string& id);

    std::vector<RenderedFrame> generateIsometricSequence(unsigned int output_size = 128, float pitch = 30.0f, float yaw = 45.0f, bool wireframe = false);

    std::vector<RenderedFrame> generateIsometricSequenceOBJ(unsigned int output_size = 128, float pitch = 30.0f, float yaw = 45.0f, bool wireframe = false);

    void saveAssets(const std::string& item_id, int output_size = 128, float pitch = 30.0f, float yaw = 45.0f, bool wireframe = false);
    
    void saveAssets(const std::string& item_id, const std::vector<RenderedFrame>& frames);

    void saveAnimationWebP(const std::string& item_id, const std::string& output_dir, const std::vector<RenderedFrame>& frames);

    void saveAnimationPNG(const std::string& item_id, const std::string& output_dir, const std::vector<RenderedFrame>& frames);

    void saveAnimationAPNG(const std::string& item_id, const std::string& output_dir, const std::vector<RenderedFrame>& frames);

    void exportToObj(const std::string& item_id, const std::string& output_dir);

    std::string getID() const;

    bool isValid() const
    {
        return !textures.empty() || !materials.empty();
    }

    bool isObj() const
    {
        return !shapes.empty();
    }

private:
    std::string id;
    nlohmann::json model_json;
    std::map<std::string, TextureAnimation> textures;
    std::set<std::string> uniqueTexturePaths;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    
    //bool allow_free = false;
    int calculateTotalLoopTicks();

    struct GpuVertex
    {
        float x, y, z;
        float r, g, b, a;
        float u, v;
    };

    struct Triangle
    {
        GpuVertex v[3];
        const Texture *texture;
        float depth;
    };

    struct FaceDef
    {
        std::string dir;
        sf::Vector3f v[4];
    };

    struct FramebufferObjects
    {
        GLuint fbo = 0;
        GLuint render_tex = 0;
        GLuint rbo = 0;
    };

    struct OpenGLState
    {
        GLint viewport[4];
        GLint prev_fbo;
        GLboolean depth_test;
        GLboolean blend;
        GLint blend_src;
        GLint blend_dst;
    };

    /**
     * @brief Tries to download a PNG from a list of URLs and returns the first valid one.
     */
    static bool try_load_png(Client &client, const std::vector<std::string> &urls,
                             std::string &img_data, std::string &successful_url);

    /**
     * @brief Falls back to searching the client's texture list for a block texture.
     */
    static bool load_fallback_texture(const std::string &ns, const std::string &path_hint,
                                      Client &client, std::string &img_data, std::string &successful_url);

    /**
     * @brief Downloads and parses an optional .mcmeta file for animation data.
     */
    static void load_animation_meta(const std::string &meta_url, Client &client, TextureAnimation &anim_data);

    /**
     * @brief Loads texture pixels from raw PNG data, handles rotation and animation meta.
     */
    static bool load_texture_from_memory(const std::string &img_data, const std::string &successful_url,
                                         Client &client, TextureAnimation &anim_data);

    /**
     * @brief Loads a Minecraft texture by trying direct URLs, fallback lists, and optional .mcmeta.
     */
    static bool try_load_texture_full(Client &client, const std::string &texture_path,
                                      TextureAnimation &anim_data);

    /**
     * @brief Applies fluid container specialisation (replaces textures).
     */
    void process_fluid_container();

    /**
     * @brief Downloads parent models recursively and merges textures/elements.
     */
    void merge_parent_models(Client &client);

    /**
     * @brief If no elements exist, tries to pull them from the first override model.
     */
    void merge_override_model(Client &client);

    /**
     * @brief Resolves all texture references and downloads missing textures.
     */
    void resolve_and_load_textures(Client &client);

    /**
     * @brief Clears alpha for specific pixels on bucket textures to simulate fluid container.
     */
    void apply_fluid_container_alpha();

    /**
     * @brief Computes UV coordinates for a single face.
     */
    static void compute_uv_for_face(const nlohmann::json &face_json, const FaceDef &face,
                                    float v_scale, sf::Vector2f uv[4]);

    /**
     * @brief Processes one element from the model JSON and appends triangles to the list.
     */
    void add_triangles_from_element(const nlohmann::json &element, bool is_flat, float scale,
                                    float cx, float cy, float pitch, float yaw,
                                    std::vector<Triangle> &triangles);

    /**
     * @brief Calculates the vertical UV offset for an animated texture at a given tick.
     */
    static float compute_animation_offset(const TextureAnimation &anim, const Texture *tex, int tick);

    /**
     * @brief Renders a sequence of frames for an animated model into an FBO and captures them.
     */
    void render_sequence_frames(const std::vector<Triangle> &triangles,
                                const std::map<const Texture *, GLuint> &gl_textures,
                                GLuint shader_program, GLuint fbo, GLuint vao,
                                int outputSize, bool any_animated, int total_ticks,
                                std::vector<RenderedFrame> &out_frames);

    /**
     * @brief Ensures the model JSON contains an "elements" array, creating defaults if missing.
     */
    void ensure_elements_exist(bool is_flat);
};