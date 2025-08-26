#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>

int main() {
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(800, 600)), "Prueba");
    ImGui::SFML::Init(window);

    // Dentro del main(), después de ImGui::SFML::Init
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.1f, 0.9f); // Fondo azul oscuro
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.5f, 0.8f, 1.0f); // Azul brillante para títulos
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.6f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Texto blanco
    style.WindowRounding = 0.0f; // Esquinas cuadradas (estilo digital)
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 0.0f;

    sf::Clock deltaClock;
    while (window.isOpen()) {
        while (std::optional<sf::Event> event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // Aquí construyes tu interfaz ImGui
        ImGui::Begin("Ventana");
        ImGui::Text("Hello");
        ImGui::End();

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();
}