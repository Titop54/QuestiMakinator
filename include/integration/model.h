#ifndef MODEL_GENERATOR_H
#define MODEL_GENERATOR_H

#include "kubejs.h"
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
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

class ModelGenerator {
private:
    nlohmann::json modelJson;
    std::map<std::string, TextureAnimation> textures;
    std::set<std::string> uniqueTexturePaths;
    
    int calculateTotalLoopTicks(); 

public:
    ModelGenerator(const std::string& rawJson, KubeJSClient& client);

    /**
     * @brief Generate a sequence of images to display
     * @param outputSize Output size (64x64 or 128x128)
     * @return vector containing 1 or more images to display
     */
    std::vector<sf::Image> generateIsometricSequence(unsigned int outputSize = 64);
};

#endif