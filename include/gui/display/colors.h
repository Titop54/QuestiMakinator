#pragma once

#include "gui/elements/button.h"
#include "gui/elements/textfield.h"
#include <SFML/System/Vector3.hpp>
#include <algorithm>
#include <cctype>
#include <format>
#include <imgui.h>
#include <string>

const std::string COLOR_CODES[16] = {
    "#000000", "#0000AA", "#00AA00", "#00AAAA", "#AA0000", "#AA00AA",
    "#FFAA00", "#AAAAAA", "#555555", "#5555FF", "#55FF55", "#55FFFF",
    "#FF5555", "#FF55FF", "#FFFF55", "#FFFFFF"
};

inline void to_upper_case(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
}

inline unsigned int argb_hex_to_decimal(const std::string& argb_hex)
{
    if(argb_hex.length() != 8)
    {
        return 0;
    }

    try
    {
        return std::stoul(argb_hex, nullptr, 16);
    }
    catch(...)
    {
        return 0;
    }
}

inline std::string decimal_to_hex(const float colors[3], bool upper = false)
{
    int r = static_cast<int>(std::round(std::clamp(colors[0], 0.0f, 1.0f) * 255.0f));
    int g = static_cast<int>(std::round(std::clamp(colors[1], 0.0f, 1.0f) * 255.0f));
    int b = static_cast<int>(std::round(std::clamp(colors[2], 0.0f, 1.0f) * 255.0f));
    std::string s = std::format("{:02x}{:02x}{:02x}", r, g, b);
    if(upper) to_upper_case(s);
    return s;

}

inline void createColorWheel(TextField& field)
{
    static float colors[3] = { 0 };
    ImGui::Begin("Color");
    ImGui::ColorPicker3("Colors", colors);
    static Button data = {
        "Apply color",
        "",
        "Put the selected color",
        ""
    };

    static bool upper = false;
    ImGui::Checkbox("Upper Case", &upper);

    generateSlowedButton(data, [&field]() {
        if(field.hasSelection)
        {
            field.wrapSelection("#" + decimal_to_hex(colors, upper), "&r");
        }
        else
        {
            field.wrapSelection("#" + decimal_to_hex(colors, upper));
        }
    });

    ImGui::End();
}