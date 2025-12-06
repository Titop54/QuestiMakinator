#ifndef MODEL_GENERATOR_H
#define MODEL_GENERATOR_H

#include "kubejs.h"
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include "tiny_obj_loader.h"
#include <string>
#include <vector>
#include <map>
#include <set>

struct AnimationFrame {
    int index;
    int time;
};

struct TextureAnimation {
    sf::Texture texture;
    bool isAnimated = false;
    unsigned int frameHeight;
    
    int defaultFrameTime = 1; 
    std::vector<AnimationFrame> sequence; 

    int getTotalDuration() const {
        if (!sequence.empty()) {
            int total = 0;
            for(const auto& f : sequence) total += f.time;
            return total;
        }
        int physicalFrames = texture.getSize().y / frameHeight;
        return physicalFrames * defaultFrameTime;
    }
};

inline std::string changeFilename(const std::string& input) {
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
class ModelGenerator {
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
     * @return vector containing 1 or more images to display
     */
    std::vector<sf::Image> generateIsometricSequence(unsigned int outputSize = 64);

    /**
     * @brief Generate a sequence of images to display
     * @param outputSize Output size (128x128)
     * @return vector containing 1 or more images to display
     */
    std::vector<sf::Image> generateIsometricSequenceOBJ(unsigned int outputSize = 128);

    /**
     * @brief Downloads JSON and all textures to a folder named "mod_itemid_assets"
     */
    void saveAssets(const std::string& itemId);

    /**
     * @brief Generates an animated .webp file from the generated sequence
     */
    void saveAnimationWebP(const std::string& itemId, const std::string& outputDir, const std::vector<sf::Image>& frames);

    /**
     * @brief Exports the 3D model to .obj and .mtl for Blender
     */
    void exportToObj(const std::string& itemId, const std::string& outputdir);

};

#endif