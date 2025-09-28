#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/WindowEnums.hpp>
#include "imgui_stdlib.h"
#include "parser/raw.h"

int main()
{
    sf::RenderWindow window(
                     sf::VideoMode(sf::Vector2u(800, 600)),
                    "Prueba",
                    sf::Style::Default,
                    sf::State::Fullscreen,
                    {}
    );
    if(!ImGui::SFML::Init(window)) return -1;
    window.setFramerateLimit(60);
    window.setPosition(ImGui::GetMainViewport()->GetCenter());
    std::string inputText1 = "";
    std::string inputText2 = "";
    std::string resultado = "";


    //After calling init, we need to pass the style
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.1f, 0.9f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.5f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.6f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.WindowRounding = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 0.0f;

    float zoom_factor = 3.0f;
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = zoom_factor;
    sf::Clock deltaClock;

    while(window.isOpen())
    {
        while (std::optional<sf::Event> event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if(event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());
        ImVec2 center = ImVec2(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        ImGui::Begin("Ventana");
        ImGui::InputText("##input1", &inputText1);
        ImGui::InputText("##input2", &inputText2);
        if(ImGui::Button("Convert"))
        {
            inputText2 = raw::to_json(inputText1, true);
        }

        if(ImGui::Button("Exit"))
        {
            window.close();
        }
        
        ImGui::End();

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();

    return 0;
}