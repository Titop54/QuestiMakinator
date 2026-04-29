#include "gui/display/window.h"
#include "gui/elements/button.h"
#include "gui/elements/menu.h"
#include "integration/client.h"
#include "integration/model.h"
#include <cstddef>
#include <gui/display/Image.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>

static void rotateSfImage90(sf::Image& image)
{
    sf::Vector2u size = image.getSize();
    sf::Image rotated(size);
    for(unsigned int y = 0; y < size.y; y++)
    {
        for(unsigned int x = 0; x < size.x; x++)
        {
            rotated.setPixel({ size.y - 1 - y, x }, image.getPixel({ x, y }));
        }
    }
    image = rotated;
}

void parseId(const std::string& fullId, std::string& ns, std::string& path)
{
    size_t colon = fullId.find(':');
    if(colon == std::string::npos)
    {
        ns = "minecraft";
        path = fullId;
    }
    else
    {
        ns = fullId.substr(0, colon);
        path = fullId.substr(colon + 1);
    }
}

ImageBrowser::ImageBrowser()
{
    buttons.reserve(10);
    buttons.push_back({
        // 0
        "Pause",
        "Pause the animation",
    });

    buttons.push_back({
        // 1
        " Play ",
        "Start the animation",
    });

    buttons.push_back({
        // 2
        "Reset",
        "Restart the animation timer",
    });

    buttons.push_back({
        // 3
        "Reset View",
        "Put default values",
    });

    buttons.push_back({
        // 4
        "Download Assets",
        "Saves model.json and all textures to folder img/mod_id_assets and Blender .obj",
    });

    buttons.push_back({
        // 5
        "Download Animation",
        "Saves as WebP",
    });

    buttons.push_back({
        // 6
        "Clear",
        "Clear the current item/block",
    });

    buttons.push_back({
        // 7
        "Connect to KubeJS",
        "Try to connect to KubeJS localhost server",
    });

    buttons.push_back({
        // 8
        "Download all mods assets",
        "Download every asset from the mod",
    });

    buttons.push_back({
        // 9
        "Download all mods assets (WebP)",
        "Make a .webp from all id of the mod",
    });

    if(!client.isConnected())
    {
        client.connect();
    }
}

void ImageBrowser::loadAssets()
{
    if(assetsLoaded) return;
    isLoading = true;

    auto blocks = client.searchBlocks();
    auto items = client.searchItems();
    auto fluids = client.searchFluids();

    std::set<std::string> fluidSet(fluids.begin(), fluids.end());

    validIds.reserve(blocks.size() + items.size() + fluids.size());
    for(const auto& b : blocks)
    {
        if(fluidSet.find(b) == fluidSet.end()) validIds.push_back("[Block] " + b);
    }
    for(const auto& i : items)
    {
        if(fluidSet.find(i) == fluidSet.end()) validIds.push_back("[Item] " + i);
    }
    for(const auto& f : fluids)
    {
        validIds.push_back("[Fluid] " + f);
    }

    models = client.listAssetsByPath("models/");

    assetsLoaded = true;
    isLoading = false;
    allBlocks = blocks.size();
    allItems = blocks.size();
    allFluids = fluids.size();
}

void ImageBrowser::update(float deltaTime)
{
    if(!currentAnimation && !currentAnimation->isPlaying) return;

    if(currentAnimation->totalTicks == 1) return; // we don't have animation to play

    currentAnimation->accumulatedTime += deltaTime;
    bool tickChanged = false;

    while(currentAnimation->accumulatedTime >= AnimationData::SECONDS_PER_TICK)
    {
        currentAnimation->accumulatedTime -= AnimationData::SECONDS_PER_TICK;
        currentAnimation->currentTick = (currentAnimation->currentTick + 1) % currentAnimation->totalTicks;
        tickChanged = true;
    }

    if(tickChanged)
    {
        updateDisplayTexture();
    }
}

void ImageBrowser::render()
{
    ImGui::Begin("Image Browser");
    if(client.needs_manual)
    {
        ImGui::Text("Not connect to KubeJS\nNeeds to be loaded on port localhost:61423\n");

        static bool has_tried = false;
        generateSlowedButton(buttons[7], [&]() {
            client.needs_manual = false;
            has_tried = !client.connect();
        });

        if(has_tried)
        {
            ImGui::NewLine();
            ImGui::NewLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to connect to the local server\n");
        }

        ImGui::End();
        return;
    }

    draw_search_bar("Item / Block ID", validIds, id_input_buffer, [this](std::string s) {
        try
        {
            loadImage(s);
        }
        catch(...)
        {}
    });

    draw_search_bar("Exact Model", models, model_input_buffer, [this](std::string s) {
        try
        {
            loadImage(s);
        }
        catch(...)
        {}
    });

    ImGui::Separator();
    ImGui::Text("Current amount of items: %d", allItems);
    ImGui::Text("Current amount of blocks: %d", allBlocks);
    ImGui::Text("Current amount of fluids: %d", allFluids);
    ImGui::Separator();

    if(currentTexture != 0 && currentAnimation && !currentAnimation->frames.empty())
    {
        float scale = 2.0f;

        ImVec2 textureSize(currentOutputSize * scale, currentOutputSize * scale);

        float windowWidth = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((windowWidth - textureSize.x) * 0.5f);

        ImGui::Image((ImTextureID)(intptr_t)currentTexture, textureSize,
            ImVec2(0, 0), ImVec2(1, 1),
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
            ImVec4(0.5f, 0.5f, 0.5f, 0.2f));
    }
    else if(!isLoading && !currentId.empty() && !currentAnimation)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load model or texture.");
    }

    if(!currentAnimation.has_value())
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();

    if(currentAnimation->totalTicks > 1)
    {
        float width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width * 0.5f - 80);

        if(currentAnimation->isPlaying)
        {
            generateSlowedButton(buttons[0], [this]() {
                currentAnimation->isPlaying = false;
            });
        }
        else
        {
            generateSlowedButton(buttons[1], [this]() {
                currentAnimation->isPlaying = true;
            });
        }

        ImGui::SameLine();

        generateSlowedButton(buttons[2], [this]() {
            currentAnimation->currentTick = 0;
            currentAnimation->accumulatedTime = 0.0f;
            updateDisplayTexture();
        });

        int tick = currentAnimation->currentTick;
        if(ImGui::SliderInt("Timeline (Ticks)", &tick, 0,
               currentAnimation->totalTicks - 1))
        {
            currentAnimation->currentTick = tick;
            currentAnimation->isPlaying = false;
            updateDisplayTexture();
        }
    }

    ImGui::Separator();
    ImGui::Text("3D View Controls");
    bool viewChanged = false;

    if(ImGui::SliderInt("Resolution", &currentOutputSize, 32, 512))
    {
        viewChanged = true;
        need_first_refresh = true;
    }

    if(ImGui::Checkbox("Flip Horizontal", &flip_horizontal))
    {
        viewChanged = true;
    }
    ImGui::SameLine();
    if(ImGui::Checkbox("Flip Vertical", &flip_vertical))
    {
        viewChanged = true;
    }
    ImGui::NewLine();
    if(ImGui::Checkbox("WireFrame", &wireframe))
    {
        viewChanged = true;
    }
    ImGui::SameLine();
    if(ImGui::Checkbox("Rotate 90º", &rotate))
    {
        viewChanged = true;
    }

    if(ImGui::SliderFloat("Horizontal rotation", &viewYaw, -180.0f, 180.0f))
    {
        viewChanged = true;
        useCustomView = true;
    }

    if(ImGui::SliderFloat("Vertical Rotation", &viewPitch, -90.0f, 90.0f))
    {
        viewChanged = true;
        useCustomView = true;
    }

    ImGui::NewLine();

    generateSlowedButton(buttons[3], [&]() {
        viewYaw = 45.0f;
        viewPitch = 30.0f;
        useCustomView = false;
        viewChanged = true;
        currentOutputSize = 128;
        need_first_refresh = true;
    });

    if(need_first_refresh)
    {
        need_first_refresh = false;
        updateDisplayTexture();
    }

    if(viewChanged && currentGenerator)
    {
        std::vector<RenderedFrame> frames;
        try
        {
            if(currentGenerator->isObj())
            {
                frames = currentGenerator->generateIsometricSequenceOBJ(currentOutputSize, useCustomView, viewPitch, viewYaw, wireframe);
            }
            else
            {
                frames = currentGenerator->generateIsometricSequence(currentOutputSize, useCustomView, viewPitch, viewYaw, wireframe);
            }
        }
        catch(...)
        {
        }

        for(auto& img : frames)
        {
            if(flip_horizontal) img.image.flipHorizontally();
            if(flip_vertical) img.image.flipVertically();
            if(rotate) rotateSfImage90(img.image);
        }

        if(!frames.empty())
        {
            currentAnimation->frames = frames;
            updateDisplayTexture();
            updateDisplayTexture();
        }
    }

    ImGui::Separator();
    ImGui::Text("Export Options:");

    generateSlowedButton(buttons[4], [&]() { // Download
        if(!currentGenerator) return;

        std::thread([id = currentId, this]() mutable {
            auto window = WindowUtils::createHiddenContext();
            WindowUtils::makeContextCurrent(window);
            auto model = generateModel(id);
            if(!model.isValid()) return;

            model.saveAssets(id, true, currentOutputSize, viewPitch, viewYaw, wireframe);
            WindowUtils::destroyWindow(window);
        }).detach();
    });

    ImGui::SameLine();

    generateSlowedButton(buttons[5], [this]() { // webp
        if(!currentGenerator || !currentAnimation) return;

        std::thread([id = currentId, this]() mutable {
            auto window = WindowUtils::createHiddenContext();
            WindowUtils::makeContextCurrent(window);

            auto model = generateModel(id);
            if(!model.isValid()) return;

            std::string safeName = changeFilename(id);
            std::string targetDir = "img/" + safeName;
            std::vector<RenderedFrame> frames;
            if(model.isObj())
            {
                frames = model.generateIsometricSequenceOBJ(currentOutputSize, true, viewPitch, viewYaw, wireframe);
            }
            else
            {
                frames = model.generateIsometricSequence(currentOutputSize, true, viewPitch, viewYaw, wireframe);
            }

            model.saveAnimationWebP(id, targetDir, frames);

            WindowUtils::destroyWindow(window);
        }).detach();
    });

    ImGui::SameLine();

    generateSlowedButton(buttons[6], [&]() { // Reset
        currentId.clear();
        id_input_buffer.clear();
        id_input_buffer.shrink_to_fit();

        model_input_buffer.clear();
        model_input_buffer.shrink_to_fit();
        currentAnimation.reset();
        currentGenerator.reset();

        if(currentTexture == 0) return;

        glDeleteTextures(1, &currentTexture);
        currentTexture = 0;
    });

    ImGui::End();
}

ModelGenerator ImageBrowser::generateModel(const std::string& id_to_search)
{
    bool item = false;
    bool fluid = false;

    std::string id, ns, path;
    if(id_to_search.substr(0, 8) == "[Block] ")
    {
        id = id_to_search.substr(8);
    }
    else if(id_to_search.substr(0, 7) == "[Item] ")
    {
        item = true;
        id = id_to_search.substr(7);
    }
    else if(id_to_search.substr(0, 8) == "[Fluid] ")
    {
        fluid = true;
        id = id_to_search.substr(8);
    }
    else
    {
        id = id_to_search;
    }

    parseId(id, ns, path);

    if(fluid)
    {
        nlohmann::json fluidModel;
        fluidModel["parent"] = "minecraft:block/cube_all";
        fluidModel["textures"]["all"] = id;
        return ModelGenerator(fluidModel.dump(), client, id);
    }

    auto ends_with = [](const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    bool is_json = ends_with(path, ".json");
    bool is_obj = ends_with(path, ".obj") || ends_with(path, ".obj.ie");

    std::string response;
    std::string url;

    if(is_json)
    {
        url = "/api/client/assets/get/" + ns + "/models/" + path;
        client.sendHttpRequest("GET", url, "", response);

        if(response.empty()) return {};
        return ModelGenerator(response, client, id);
    }
    else if(is_obj)
    {
        std::string obj_data;
        url = "/api/client/assets/get/" + ns + "/models/" + path;
        client.sendHttpRequest("GET", url, "", obj_data);

        if(obj_data.empty()) return {};

        std::string mtl_path = path;
        size_t extension = mtl_path.rfind(".obj");
        if(extension != std::string::npos)
        {
            mtl_path = mtl_path.substr(0, extension) + ".mtl";
        }
        else
        {
            mtl_path += ".mtl";
        }

        std::string mtl_data;
        url = "/api/client/assets/get/" + ns + "/models/" + mtl_path;
        client.sendHttpRequest("GET", url, "", mtl_data);

        if(mtl_data.empty()) return {};

        return ModelGenerator(obj_data, mtl_data, client, ns, id);
    }

    if(item)
    {
        url = "/api/client/assets/get/" + ns + "/models/item/" + path + ".json";
        client.sendHttpRequest("GET", url, "", response);

        if(response.empty()) return {};
        return ModelGenerator(response, client, id);
    }

    url = "/api/client/assets/get/" + ns + "/models/block/" + path + ".json";
    client.sendHttpRequest("GET", url, "", response);

    if(!response.empty()) return ModelGenerator(response, client, id);

    auto list_obj = client.listAssetsByPrefix(".obj");
    std::string obj_path = "";

    for(const auto& obj : list_obj)
    {
        std::string pth = obj.path;
        if(pth.find("/" + path + ".obj") != std::string::npos    ||
           pth.find("/" + path + ".obj.ie") != std::string::npos ||
           pth == path + ".obj")
        {
            obj_path = pth;
            break;
        }
    }

    if(obj_path.empty()) return {};

    std::string obj_data = "";
    url = "/api/client/assets/get/" + ns + "/models/" + obj_path;
    client.sendHttpRequest("GET", url, "", obj_data);

    if(obj_data.empty()) return {};

    std::string mtl_path = obj_path;
    size_t extension = mtl_path.rfind(".obj");
    if(extension != std::string::npos)
    {
        mtl_path = mtl_path.substr(0, extension) + ".mtl";
    }
    else
    {
        mtl_path += ".mtl";
    }

    url = "/api/client/assets/get/" + ns + "/models/" + mtl_path;
    std::string mtl_data;
    client.sendHttpRequest("GET", url, "", mtl_data);

    if(mtl_data.empty()) return {};

    return ModelGenerator(obj_data, mtl_data, client, ns, id);
}

void ImageBrowser::loadImage(const std::string& id)
{
    if(id.empty())
    {
        isLoading = false;
        return;
    }

    useCustomView = true;
    viewYaw = 45.0f;
    viewPitch = 30.0f;
    currentId = id;
    currentOutputSize = 128;
    need_first_refresh = true;

    isLoading = true;
    currentAnimation.reset();
    currentGenerator.reset();

    ModelGenerator m = generateModel(id);
    if(m.isValid()) currentGenerator.emplace(m);

    if(!currentGenerator.has_value())
    {
        isLoading = false;
        return;
    }

    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glDisable(GL_SCISSOR_TEST);

    std::vector<RenderedFrame> frames;
    try
    {
        if(currentGenerator->isObj())
        {
            frames = currentGenerator->generateIsometricSequenceOBJ(currentOutputSize, useCustomView, viewPitch, viewYaw, wireframe);
        }
        else
        {
            frames = currentGenerator->generateIsometricSequence(currentOutputSize, useCustomView, viewPitch, viewYaw, wireframe);
        }
    }
    catch(...)
    {}

    for(auto& img : frames)
    {
        if(flip_horizontal) img.image.flipHorizontally();
        if(flip_vertical) img.image.flipVertically();
        if(rotate) rotateSfImage90(img.image);
    }

    if(scissorEnabled)
    {
        glEnable(GL_SCISSOR_TEST);
    }
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    if(frames.empty())
    {
        std::cerr << "ModelGenerator generated 0 frames for " << id << std::endl;
        isLoading = false;
        return;
    }

    AnimationData anim;
    anim.frames = frames;

    int tTicks = 0;
    for(const auto& f : frames) tTicks += f.timeInTicks;
    anim.totalTicks = std::max(1, tTicks);
    anim.currentTick = 0;

    currentAnimation = anim;
    updateDisplayTexture();

    isLoading = false;
}

void ImageBrowser::updateDisplayTexture()
{
    if(!currentAnimation)
    {
        if(currentTexture != 0)
        {
            glDeleteTextures(1, &currentTexture);
            currentTexture = 0;
        }
        return;
    }

    if(currentAnimation->frames.empty())
    {
        return;
    }

    const auto& img = currentAnimation->getCurrentImage();
    unsigned int width = img.getSize().x;
    unsigned int height = img.getSize().y;

    if(width == 0 || height == 0) return;

    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    if(currentTexture == 0 || lastTexWidth != width || lastTexHeight != height)
    {
        if(currentTexture != 0)
        {
            glDeleteTextures(1, &currentTexture);
        }

        glGenTextures(1, &currentTexture);
        glBindTexture(GL_TEXTURE_2D, currentTexture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        lastTexWidth = width;
        lastTexHeight = height;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, currentTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
            GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    if(scissorWasEnabled)
    {
        glEnable(GL_SCISSOR_TEST);
    }
}
