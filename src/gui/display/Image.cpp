#include "gui/elements/button.h"
#include "gui/elements/menu.h"
#include "integration/kubejs.h"
#include <cstddef>
#include <gui/display/Image.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <GLFW/glfw3.h>
#include <iostream>

static void rotateSfImage90(sf::Image& image)
{
    sf::Vector2u size = image.getSize();
    sf::Image rotated({size.y, size.x});
    for(unsigned int y = 0; y < size.y; y++)
    {
        for(unsigned int x = 0; x < size.x; x++)
        {
            rotated.setPixel({size.y - 1 - y, x}, image.getPixel({x, y}));
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

void KubeJSImageBrowser::loadAssets()
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

    assetsLoaded = true;
    isLoading = false;
    allBlocks = blocks.size();
    allItems = blocks.size();
    allFluids = fluids.size();
}

void KubeJSImageBrowser::update(float deltaTime)
{
    if(currentAnimation && currentAnimation->isPlaying &&
        currentAnimation->totalTicks > 1)
    {
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
}

void KubeJSImageBrowser::render()
{
    ImGui::Begin("Image Browser");
    if(client.needs_manual)
    {
        ImGui::Text("Not connect to KubeJS\nNeeds to be loaded on port localhost:61423\n");
        Button a = {
            "Connect to KubeJS",
            "Try to connect to KubeJS localhost server"
        };

        static bool has_tried = false;
        generateSlowedButton(a, [&]() {
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

    draw_search_bar("Item / Block ID", validIds, idInputBuffer, [this](std::string s) {
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

    if(currentAnimation.has_value())
    {
        ImGui::Separator();

        if(currentAnimation->totalTicks > 1)
        {
            float width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width * 0.5f - 80);

            if(ImGui::Button(currentAnimation->isPlaying ? "Pause" : " Play "))
            {
                currentAnimation->isPlaying = !currentAnimation->isPlaying;
            }
            ImGui::SameLine();
            if(ImGui::Button("Reset"))
            {
                currentAnimation->currentTick = 0;
                currentAnimation->accumulatedTime = 0.0f;
                updateDisplayTexture();
            }

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
        if(ImGui::Button("Reset View"))
        {
            viewYaw = 45.0f;
            viewPitch = 30.0f;
            useCustomView = false;
            viewChanged = true;
            currentOutputSize = 128;
            need_first_refresh = true;
        }

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
                if(currentGenerator->isObjModel)
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

        if(ImGui::Button("Download Assets"))
        {
            if(currentGenerator)
            {
                currentGenerator->saveAssets(currentId, useCustomView, currentOutputSize, viewPitch, viewYaw);
            }
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Saves model.json and all textures to folder img/mod_id_assets and Blender .obj");
        }

        ImGui::SameLine();
        if(ImGui::Button("Download Animation"))
        {
            if(currentGenerator)
            {
                std::string safeName = changeFilename(idInputBuffer);
                std::string targetDir = "img/" + safeName;
                currentGenerator->saveAnimationWebP(currentId, targetDir, currentAnimation->frames);
            }
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Saves as WebP");

        ImGui::SameLine();
        if(ImGui::Button("Clear"))
        {
            currentId.clear();
            idInputBuffer[0] = '\0';
            currentAnimation.reset();
            currentGenerator.reset();
            animations.clear();

            if(currentTexture != 0)
            {
                glDeleteTextures(1, &currentTexture);
                currentTexture = 0;
            }
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Clear the current item/block");
    }

    if(isLoading)
    {
        ImGui::TextDisabled("Downloading and generating model...");
    }

    ImGui::End();
}

void KubeJSImageBrowser::loadImage(const std::string& id)
{
    if(id.empty())
    {
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

    std::string ns, path;
    std::string cleanId = id;
    bool forceBlock = false;
    bool forceItem = false;
    bool forceFluid = false;

    if(id.substr(0, 8) == "[Block] ")
    {
        forceBlock = true;
        cleanId = id.substr(8);
    }
    else if(id.substr(0, 7) == "[Item] ")
    {
        forceItem = true;
        cleanId = id.substr(7);
    }
    else if(id.substr(0, 8) == "[Fluid] ")
    {
        forceFluid = true;
        cleanId = id.substr(8);
    }

    parseId(cleanId, ns, path);

    std::string jsonBody;
    bool modelFound = false;

    if(forceFluid)
    {
        nlohmann::json fluidModel;
        fluidModel["parent"] = "minecraft:block/cube_all";
        fluidModel["textures"]["all"] = cleanId;
        currentGenerator.emplace(fluidModel.dump(), client, cleanId);
        modelFound = true;
    }

    if(!modelFound && !forceItem)
    {
        std::string urlBlock = "/api/client/assets/get/" + ns + "/models/block/" + path + ".json";
        client.sendHttpRequest("GET", urlBlock, "", jsonBody);

        if(!jsonBody.empty() && jsonBody.find("error") == std::string::npos)
        {
            currentGenerator.emplace(jsonBody, client, cleanId);
            modelFound = true;
        }
    }

    if(!modelFound)
    {
        auto objList = client.listAssetsByPrefix(".obj");

        std::string foundObjPath = "";
        for(const auto& asset : objList)
        {
            std::string p = asset.path;
            if(p.find("/" + path + ".obj") != std::string::npos ||
                p.find("/" + path + ".obj.ie") != std::string::npos ||
                p == path + ".obj")
            {
                foundObjPath = p;
                break;
            }
        }

        if(!foundObjPath.empty())
        {
            std::string objData, mtlData;
            std::string objUrl = "/api/client/assets/get/" + ns + "/models/" + foundObjPath;
            client.sendHttpRequest("GET", objUrl, "", objData);
            if(!objData.empty() && objData.find("error") == std::string::npos)
            {
                std::string mtlPath = foundObjPath;
                size_t extPos = mtlPath.rfind(".obj");

                if(extPos != std::string::npos)
                {
                    mtlPath = mtlPath.substr(0, extPos) + ".mtl";
                }
                else
                {
                    mtlPath += ".mtl";
                }

                std::string mtlUrl = "/api/client/assets/get/" + ns + "/models/" + mtlPath;
                client.sendHttpRequest("GET", mtlUrl, "", mtlData);
                currentGenerator.emplace(objData, mtlData, client, ns, id);
                modelFound = true;
            }
        }
    }

    if(!modelFound && !forceBlock)
    {
        std::string urlItem = "/api/client/assets/get/" + ns + "/models/item/" + path + ".json";
        jsonBody.clear();
        client.sendHttpRequest("GET", urlItem, "", jsonBody);

        if(!jsonBody.empty() && jsonBody.find("error") == std::string::npos)
        {
            currentGenerator.emplace(jsonBody, client, cleanId);
            modelFound = true;
        }
    }

    if(modelFound && currentGenerator.has_value())
    {
        GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        glDisable(GL_SCISSOR_TEST);

        std::vector<RenderedFrame> frames;
        try
        {
            if(currentGenerator->isObjModel)
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

        if(scissorEnabled)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

        if(!frames.empty())
        {
            AnimationData anim;
            anim.frames = frames;

            int tTicks = 0;
            for(const auto& f : frames) tTicks += f.timeInTicks;
            anim.totalTicks = std::max(1, tTicks);
            anim.currentTick = 0;

            animations[id] = anim;
            currentAnimation = animations[id];
            updateDisplayTexture();
        }
        else
        {
            std::cerr << "ModelGenerator generated 0 frames for " << id << std::endl;
        }
    }
    else
    {
        std::cerr << "Could not find model (Block JSON, OBJ, or Item JSON) for " << id << std::endl;
    }

    isLoading = false;
}

void KubeJSImageBrowser::updateDisplayTexture()
{
    if(currentAnimation && !currentAnimation->frames.empty())
    {
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
    else
    {
        if(currentTexture != 0)
        {
            glDeleteTextures(1, &currentTexture);
            currentTexture = 0;
        }
    }
}
