#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Window.hpp>
namespace WindowUtils {
    void maximize(sf::Window& window);

    sf::RenderWindow createWindow();
}

