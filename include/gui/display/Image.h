#pragma once

#include <GL/gl.h>
#include <nlohmann/json.hpp>

#include <integration/model.h>
#include <optional>
#include "gui/elements/button.h"
#include "integration/kubejs.h"

#include <GLFW/glfw3.h>

using json = nlohmann::json;

struct AnimationData
{
    std::vector<RenderedFrame> frames;
    int totalTicks = 0;
    int currentTick = 0;
    bool isPlaying = true;
    float accumulatedTime = 0.0f;
    
    static constexpr float SECONDS_PER_TICK = 0.05f;

    const sf::Image& getCurrentImage() const
    {
        if(frames.empty())
        {
            static sf::Image empty;
            return empty;
        }

        int accumulatedTicks = 0;
        for(const auto& frame : frames)
        {
            accumulatedTicks += frame.timeInTicks;
            if(currentTick < accumulatedTicks)
            {
                return frame.image;
            }
        }
        return frames.back().image;
    }

};

void parseId(const std::string& fullId, std::string& ns, std::string& path);

class ImageBrowser
{
private:
    std::map<std::string, AnimationData> animations;
    int allBlocks = 0;
    int allItems = 0;
    int allFluids = 0;
    std::vector<std::string> validIds;


    //Textures display
    std::string currentId;
    GLuint currentTexture = 0; 
    std::optional<AnimationData> currentAnimation;
    std::optional<ModelGenerator> currentGenerator; 
    
    bool isLoading = false;
    bool assetsLoaded = false;
    std::string idInputBuffer = "";

    float viewYaw = 45.0f;
    float viewPitch = 30.0f;
    bool useCustomView = false;
    int currentOutputSize = 128;
    bool need_first_refresh = false;
    bool flip_vertical = false;
    bool flip_horizontal = false;
    bool wireframe = false;
    bool rotate = false;

    unsigned int lastTexWidth = 0;
    unsigned int lastTexHeight = 0;

    std::vector<Button> buttons;

public:
    ImageBrowser();

    ~ImageBrowser()
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

    void updateDisplayTexture();

    ModelGenerator generateModel(const std::string& id_to_search);
};

inline void createImageBrowser(ImageBrowser& browser, bool& firstRun, float deltaTime, GLFWwindow* window)
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