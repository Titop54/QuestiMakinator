#include <SFML/Graphics/Texture.hpp>
#include <integration/kubejs.h>
#include <integration/model.h>
#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
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

class KubeJSImageBrowser {
private:
    std::map<std::string, AnimationData> animations;
    std::vector<std::string> allBlocks;
    std::vector<std::string> allItems;
    std::vector<std::string> validIds;
    
    std::string currentId;
    sf::Texture currentTexture;
    AnimationData* currentAnimation = nullptr;
    
    bool isLoading = false;
    bool assetsLoaded = false;
    char idInputBuffer[256] = "";

    void parseId(const std::string& fullId, std::string& ns, std::string& path) {
        size_t colon = fullId.find(':');
        if (colon == std::string::npos) {
            ns = "minecraft";
            path = fullId;
        } else {
            ns = fullId.substr(0, colon);
            path = fullId.substr(colon + 1);
        }
    }

public:
    KubeJSImageBrowser() {
        if (!client.isConnected()) {
            client.connect();
        }
    }

    void loadAssets() {
        if (assetsLoaded) return;
        isLoading = true;
        
        allBlocks = client.searchBlocks();
        allItems = client.searchItems();
        
        validIds = allBlocks;
        // Evitar duplicados si un ID está en ambos
        for(const auto& item : allItems) {
            bool found = false; 
            for(const auto& block : allBlocks) if(block == item) found = true;
            if(!found) validIds.push_back(item);
        }
            
        assetsLoaded = true;
        isLoading = false;
    }

    void update(float deltaTime) {
        if (currentAnimation && currentAnimation->isPlaying && currentAnimation->totalFrames > 1) {
            currentAnimation->accumulatedTime += deltaTime;
            
            // Usamos la constante de Minecraft: 1 frame generado = 1 tick (0.05s)
            if (currentAnimation->accumulatedTime >= currentAnimation->SECONDS_PER_TICK) {
                // Consumir tiempo en pasos fijos para mantener sincronía
                while (currentAnimation->accumulatedTime >= currentAnimation->SECONDS_PER_TICK) {
                    currentAnimation->accumulatedTime -= currentAnimation->SECONDS_PER_TICK;
                    currentAnimation->currentFrame = (currentAnimation->currentFrame + 1) % currentAnimation->totalFrames;
                }
                updateDisplayTexture();
            }
        }
    }

    void render() {
        ImGui::Begin("KubeJS Image Browser");

        if (ImGui::InputText("ID (Enter to load)", idInputBuffer, sizeof(idInputBuffer), 
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
            loadImage(idInputBuffer);
        }

        // --- Autocomplete Logic ---
        if (ImGui::IsItemActive()) {
            // ... (Lógica de autocomplete igual que antes) ...
            sf::Vector2 pos(ImGui::GetItemRectMin().x, ImGui::GetItemRectSize().y + ImGui::GetItemRectMin().y);
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 200));
            if (ImGui::Begin("##autocomplete", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_Tooltip)) {
                std::string inputStr = idInputBuffer;
                int count = 0;
                for (const auto& id : validIds) {
                    if (id.find(inputStr) != std::string::npos) {
                        if (ImGui::Selectable(id.c_str())) {
                            strncpy(idInputBuffer, id.c_str(), sizeof(idInputBuffer));
                            loadImage(id);
                            ImGui::CloseCurrentPopup();
                        }
                        if(++count > 50) break;
                    }
                }
                ImGui::End();
            }
        }

        if (currentTexture.getSize().x > 0) {
            float scale = 1.0f;
            ImVec2 textureSize(currentTexture.getSize().x * scale, currentTexture.getSize().y * scale);
            
            // Centrar imagen
            float windowWidth = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((windowWidth - textureSize.x) * 0.5f);
            
            ImGui::Image(currentTexture, textureSize, sf::Color::White, sf::Color::Transparent); // Borde transparente
        } else if (!isLoading && !currentId.empty() && !currentAnimation) {
            ImGui::TextColored(ImVec4(1,0,0,1), "Failed to load model or texture.");
        }

        // --- Controls ---
        if (currentAnimation && currentAnimation->totalFrames > 1) {
            ImGui::Separator();
            
            // Botones centrados
            float width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width * 0.5f - 80);
            
            if (ImGui::Button(currentAnimation->isPlaying ? "Pause" : " Play ")) {
                currentAnimation->isPlaying = !currentAnimation->isPlaying;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                currentAnimation->currentFrame = 0;
                currentAnimation->accumulatedTime = 0.0f;
                updateDisplayTexture();
            }
            
            // Slider timeline
            int frame = currentAnimation->currentFrame;
            if (ImGui::SliderInt("Timeline (Ticks)", &frame, 0, currentAnimation->totalFrames - 1)) {
                currentAnimation->currentFrame = frame;
                currentAnimation->isPlaying = false; // Pausar al arrastrar
                updateDisplayTexture();
            }

            ImGui::Text("Total Duration: %d ticks (%.2fs)", currentAnimation->totalFrames, currentAnimation->totalFrames / 20.0f);
        }

        if (isLoading) {
            ImGui::TextDisabled("Downloading and generating model...");
        }

        ImGui::End();
    }

private:
    void loadImage(const std::string& id) {
        if (id.empty()) return;
        currentId = id;
        
        auto it = animations.find(id);
        if (it != animations.end()) {
            currentAnimation = &it->second;
            updateDisplayTexture();
            return;
        }

        isLoading = true;
        currentAnimation = nullptr;
        
        std::string ns, path;
        parseId(id, ns, path);

        std::string jsonBody;
        std::string urlBlock = "/api/client/assets/get/" + ns + "/models/block/" + path + ".json";
        std::string urlItem  = "/api/client/assets/get/" + ns + "/models/item/" + path + ".json";

        client.sendHttpRequest("GET", urlBlock, "", jsonBody);
        if (jsonBody.empty() || jsonBody.find("error") != std::string::npos) {
            client.sendHttpRequest("GET", urlItem, "", jsonBody);
        }

        if(!jsonBody.empty()) {
            ModelGenerator generator(jsonBody, client);
            
            std::vector<sf::Image> frames = generator.generateIsometricSequence(128);

            if (!frames.empty()) {
                AnimationData anim;
                anim.frames = frames;
                anim.totalFrames = frames.size();
                anim.currentFrame = 0;
                
                animations[id] = anim;
                currentAnimation = &animations[id];
                updateDisplayTexture();
            } else {
                std::cerr << "ModelGenerator generated 0 frames for " << id << std::endl;
            }
        } else {
             std::cerr << "Could not find model json for " << id << std::endl;
        }
        
        isLoading = false;
    }

    inline void updateDisplayTexture() {
        if (currentAnimation && !currentAnimation->frames.empty()) {
            const auto& img = currentAnimation->frames[currentAnimation->currentFrame];
            if(!currentTexture.loadFromImage(img))
            {
                std::cerr << "Error while loading texture" << std::endl;
            }
        } else {
            sf::Texture a({1,1});
            currentTexture = a;
        }
    }
};

inline void createKubejsImageBrowser(float deltaTime) {
    static KubeJSImageBrowser browser;
    static bool firstRun = true;
    
    if (firstRun) {
        browser.loadAssets();
        firstRun = false;
    }

    browser.update(deltaTime);
    browser.render();
}