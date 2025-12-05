#include <gui/display/Image.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui-SFML.h>


void parseId(const std::string& fullId, std::string& ns, std::string& path)
{
    size_t colon = fullId.find(':');
    if (colon == std::string::npos) {
        ns = "minecraft";
        path = fullId;
    } else {
        ns = fullId.substr(0, colon);
        path = fullId.substr(colon + 1);
    }
}

void KubeJSImageBrowser::loadAssets() {
    if(assetsLoaded)
        return;
    isLoading = true;

    allBlocks = client.searchBlocks();
    allItems = client.searchItems();

    validIds = allBlocks;
    for (const auto &item : allItems) {
        bool found = false;
        for (const auto &block : allBlocks)
        if (block == item)
            found = true;
        if (!found)
        validIds.push_back(item);
    }

    assetsLoaded = true;
    isLoading = false;
}

void KubeJSImageBrowser::update(float deltaTime) {
    if(currentAnimation && currentAnimation->isPlaying &&
        currentAnimation->totalFrames > 1)
    {
        currentAnimation->accumulatedTime += deltaTime;

        if(currentAnimation->accumulatedTime >= currentAnimation->SECONDS_PER_TICK)
        {
            while(currentAnimation->accumulatedTime >= currentAnimation->SECONDS_PER_TICK)
            {
                currentAnimation->accumulatedTime -= currentAnimation->SECONDS_PER_TICK;
                currentAnimation->currentFrame = (currentAnimation->currentFrame + 1) %
                                                  currentAnimation->totalFrames;
            }
            updateDisplayTexture();
        }
    }
}

void KubeJSImageBrowser::render() {
    ImGui::Begin("KubeJS Image Browser");

    filteredCandidates = filterResults(idInputBuffer, validIds, 50, acState);

    CallbackData cbData;
    cbData.state = &acState;
    cbData.candidates = &filteredCandidates;

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_CallbackAlways |
                                ImGuiInputTextFlags_CallbackCompletion |
                                ImGuiInputTextFlags_CallbackHistory;

    bool enterPressed = ImGui::InputText("ID (Enter to load)", &idInputBuffer,
                                        flags, InputCallback, &cbData);
    bool isInputActive = ImGui::IsItemActive();

    if(isInputActive && !filteredCandidates.empty() && !idInputBuffer.empty())
    {
        acState.isPopupOpen = true;
    } 
    else if(filteredCandidates.empty() || idInputBuffer.empty())
    {
        if(acState.activeIdx == -1)
        {
            acState.isPopupOpen = false;
        }
    }

    if(enterPressed && !acState.isPopupOpen) {
        loadImage(idInputBuffer);
    }

    if(enterPressed && acState.isPopupOpen) {
        idInputBuffer = filteredCandidates[acState.activeIdx];
        acState.isPopupOpen = false;
        acState.activeIdx = -1;
        loadImage(idInputBuffer);
    }

    drawMenu(acState, "##autocomplete", filteredCandidates, idInputBuffer,
            isInputActive, [this](std::string s) { this->loadImage(s); });

    ImGui::Separator();
    ImGui::Text("Current amount of items: %zu", allItems.size());
    ImGui::Text("Current amount of blocks: %zu", allBlocks.size());
    ImGui::Separator();

    if (currentTexture.getSize().x > 0)
    {
        float scale = 2.0f;
        ImVec2 textureSize(currentTexture.getSize().x * scale,
                        currentTexture.getSize().y * scale);

        float windowWidth = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX((windowWidth - textureSize.x) * 0.5f);

        ImGui::Image(currentTexture, textureSize, sf::Color::White,
                    sf::Color::Transparent);
    }
    else if(!isLoading && !currentId.empty() && !currentAnimation)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load model or texture.");
    }

    if (currentAnimation)
    {
        ImGui::Separator();

        if(currentAnimation->totalFrames > 1)
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
                currentAnimation->currentFrame = 0;
                currentAnimation->accumulatedTime = 0.0f;
                updateDisplayTexture();
            }

            int frame = currentAnimation->currentFrame;
            if(ImGui::SliderInt("Timeline (Ticks)", &frame, 0,
                                currentAnimation->totalFrames - 1))
            {
                currentAnimation->currentFrame = frame;
                currentAnimation->isPlaying = false;
                updateDisplayTexture();
            }
        }

        ImGui::Separator();
        ImGui::Text("Export Options:");

        if(ImGui::Button("Download Assets"))
        {
            if(currentGenerator)
            {
                currentGenerator->saveAssets(currentId);
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Saves model.json and all textures to folder img/mod_id_assets and Blender .obj");
        }
        

        ImGui::SameLine();
        if (ImGui::Button("Download Animation"))
        {
            if(currentGenerator)
            {
                std::string safeName = changeFilename(idInputBuffer);
                std::string targetDir = "img/" + safeName;
                currentGenerator->saveAnimationWebP(currentId, targetDir,
                                                    currentAnimation->frames);
            }
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Saves as WebP");

        ImGui::SameLine();
        if(ImGui::Button("Clear"))
        {
            currentId.clear();
            idInputBuffer[0] = '\0';
            currentAnimation = nullptr;
            currentGenerator.reset();
            sf::Texture empty;
            currentTexture = empty;
        }
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Clear the current item/block");
    }

    if(isLoading)
    {
        ImGui::TextDisabled("Downloading and generating model...");
    }

    ImGui::End();
}
void KubeJSImageBrowser::loadImage(const std::string &id)
{
    if(id.empty()) 
    {
        return;
    }
    currentId = id;

    isLoading = true;
    currentAnimation = nullptr;
    currentGenerator.reset();

    std::string ns, path;
    parseId(id, ns, path);

    std::string jsonBody;
    std::string urlBlock = "/api/client/assets/get/" + ns + "/models/block/" + path + ".json";
    std::string urlItem = "/api/client/assets/get/" + ns + "/models/item/" + path + ".json";

    client.sendHttpRequest("GET", urlBlock, "", jsonBody);
    if(jsonBody.empty() || jsonBody.find("error") != std::string::npos)
    {
        client.sendHttpRequest("GET", urlItem, "", jsonBody);
    }

    if(!jsonBody.empty())
    {
        currentGenerator = std::make_unique<ModelGenerator>(jsonBody, client);

        std::vector<sf::Image> frames = currentGenerator->generateIsometricSequence(128);

        if(!frames.empty())
        {
            AnimationData anim;
            anim.frames = frames;
            anim.totalFrames = frames.size();
            anim.currentFrame = 0;

            animations[id] = anim;
            currentAnimation = &animations[id];
            updateDisplayTexture();
        }
        else
        {
            std::cerr << "ModelGenerator generated 0 frames for " << id << std::endl;
        }
    }
    else
    {
        std::cerr << "Could not find model json for " << id << std::endl;
    }

    isLoading = false;
}
