#include "integration/kubejs.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <nlohmann/json.hpp>

#include <gui/display/menu.h>
#include <integration/model.h>

#include <iostream>

using json = nlohmann::json;

struct AnimationData {
    std::vector<sf::Image> frames;
    int totalFrames = 0;
    int currentFrame = 0;
    bool isPlaying = true;
    float accumulatedTime = 0.0f;
    
    static constexpr float SECONDS_PER_TICK = 0.05f;
};

void parseId(const std::string& fullId, std::string& ns, std::string& path);

class KubeJSImageBrowser {
private:
    std::map<std::string, AnimationData> animations;
    std::vector<std::string> allBlocks;
    std::vector<std::string> allItems;
    std::vector<std::string> validIds;

    //Autocomplete
    std::vector<std::string> filteredCandidates;
    AutoCompleteState acState;

    //Textures display
    std::string currentId;
    sf::Texture currentTexture;
    AnimationData* currentAnimation = nullptr;
    std::unique_ptr<ModelGenerator> currentGenerator; 
    
    bool isLoading = false;
    bool assetsLoaded = false;
    std::string idInputBuffer = "";

public:
    KubeJSImageBrowser() {
        if (!client.isConnected()) {
            client.connect();
        }
    }

    void loadAssets();

    void update(float deltaTime);

    void render();

  private:
    void loadImage(const std::string &id);

    inline void updateDisplayTexture() {
        if(currentAnimation && !currentAnimation->frames.empty())
        {
            const auto& img = currentAnimation->frames[currentAnimation->currentFrame];
            if(!currentTexture.loadFromImage(img))
            {
                std::cerr << "Error while loading texture" << std::endl;
            }
        }
        else
        {
            sf::Texture a(sf::Vector2u{1,1});
            currentTexture = a;
        }
    }
};

inline void createKubejsImageBrowser(float deltaTime, sf::RenderWindow& window) {
    static KubeJSImageBrowser browser;
    static bool firstRun = true;
    
    if (firstRun) {
        browser.loadAssets();
        auto image = client.getPreview("minecraft:written_book", 128, TypeElement::ITEM, false);
        window.setIcon(image);
        firstRun = false;
    }

    browser.update(deltaTime);
    browser.render();
}