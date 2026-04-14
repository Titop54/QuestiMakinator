#pragma once

#include <nlohmann/json.hpp>

#include <gui/display/menu.h>
#include <integration/model.h>
#include "integration/kubejs.h"

#include <GLFW/glfw3.h>

using json = nlohmann::json;

struct AnimationData
{
    std::vector<sf::Image> frames;
    int totalFrames = 0;
    int currentFrame = 0;
    bool isPlaying = true;
    float accumulatedTime = 0.0f;
    
    static constexpr float SECONDS_PER_TICK = 0.05f;
};

void parseId(const std::string& fullId, std::string& ns, std::string& path);

class KubeJSImageBrowser
{
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
    GLuint currentTexture = 0; 
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

    ~KubeJSImageBrowser()
    {
        if(currentTexture != 0)
        {
            glDeleteTextures(1, &currentTexture);
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
            if(currentTexture != 0)
            {
                glDeleteTextures(1, &currentTexture);
                currentTexture = 0;
            }

            glGenTextures(1, &currentTexture);
            glBindTexture(GL_TEXTURE_2D, currentTexture);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.getSize().x, img.getSize().y, 
                         0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            if(currentTexture != 0)
            {
                glDeleteTextures(1, &currentTexture);
                currentTexture = 0;
            }
        }
    }
};

inline void createKubejsImageBrowser(KubeJSImageBrowser& browser, bool& firstRun, float deltaTime, GLFWwindow* window)
{   
    if(firstRun && !client.needs_manual)
    {
        browser.loadAssets();
        auto image = client.getPreview("minecraft:written_book", 128, TypeElement::ITEM, false);
        GLFWimage icon[1];
        icon[0].width = image.getSize().x;
        icon[0].height = image.getSize().y;
        icon[0].pixels = const_cast<unsigned char*>(image.getPixelsPtr());
        
        if(icon[0].pixels != nullptr)
        {
            glfwSetWindowIcon(window, 1, icon);
        }
        
        firstRun = false;
    }

    browser.update(deltaTime);
    browser.render();
}