#pragma once

#include "kubejs.h"
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Image.hpp>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "tiny_obj_loader.h"
#include <string>
#include <vector>
#include <map>
#include <set>

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
        newPixels.reserve(pixels.size());
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
private:
    std::string id;
    nlohmann::json modelJson;
    std::map<std::string, TextureAnimation> textures;
    std::set<std::string> uniqueTexturePaths;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    
    int calculateTotalLoopTicks(); 

public:

    bool isObjModel = false;

    ModelGenerator(const std::string& rawJson, KubeJSClient& client, const std::string& id);

    ModelGenerator(const std::string& objData, const std::string& mtlData, KubeJSClient& client, const std::string& checkNamespace, const std::string& id);

    /**
     * @brief Generate a sequence of images to display
     * @param outputSize Output size (64x64 or 128x128)
     * @param customRotation Use custom 3D rotation instead of default isometric
     * @param pitch X-axis rotation
     * @param yaw Y-axis rotation
     * @return vector containing 1 or more images to display
     */
    std::vector<RenderedFrame> generateIsometricSequence(unsigned int outputSize = 128, bool customRotation = false, float pitch = 30.0f, float yaw = 45.0f);

    /**
     * @brief Generate a sequence of images to display
     * @param outputSize Output size (128x128)
     * @param customRotation Use custom 3D rotation instead of default isometric
     * @param pitch X-axis rotation
     * @param yaw Y-axis rotation
     * @return vector containing 1 or more images to display
     */
    std::vector<RenderedFrame> generateIsometricSequenceOBJ(unsigned int outputSize = 128, bool customRotation = false, float pitch = 30.0f, float yaw = 45.0f);

    /**
     * @brief Downloads JSON and all textures to a folder named "mod_itemid_assets"
     */
    void saveAssets(const std::string& itemId, bool customRotation = false, int customSize = 128, float pitch = 30.0f, float yaw = 45.0f);

    /**
     * @brief Generates an animated .webp file from the generated sequence
     */
    void saveAnimationWebP(const std::string& itemId, const std::string& outputDir, const std::vector<RenderedFrame>& frames);

    /**
     * @brief Exports the 3D model to .obj and .mtl for Blender
     */
    void exportToObj(const std::string& itemId, const std::string& outputdir);

};
