#pragma once

#include "gui/elements/button.h"
#include "gui/elements/textfield.h"
#include <SFML/System/Vector3.hpp>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <format>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

inline ImVec4 hex_to_imvec4(const std::string& hex)
{
    if(hex.length() < 7 || hex[0] != '#') return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    try
    {
        unsigned int r = std::stoul(hex.substr(1, 2), nullptr, 16);
        unsigned int g = std::stoul(hex.substr(3, 2), nullptr, 16);
        unsigned int b = std::stoul(hex.substr(5, 2), nullptr, 16);

        return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    }
    catch(...)
    {
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

struct Palette
{
    std::string name;
    std::vector<std::string> colors;
};

static std::vector<Palette> loaded_palettes;
static size_t current_palette_idx = 0;
static bool palettes_initialized = false;

inline void save_palettes()
{
    json j = json::array();
    for(const auto& p : loaded_palettes)
    {
        json p_json;
        p_json["name"] = p.name;
        p_json["colors"] = p.colors;
        j.push_back(p_json);
    }
    std::ofstream file("colors.json");
    if(file.is_open())
    {
        file << j.dump(4);
    }
}

inline void load_palettes()
{
    std::ifstream file("colors.json");
    loaded_palettes.clear();
    if(file.is_open())
    {
        try
        {
            json j;
            file >> j;
            for(const auto& p : j)
            {
                Palette pal;
                pal.name = p["name"];
                for(const auto& c : p["colors"])
                {
                    pal.colors.push_back(c);
                }
                loaded_palettes.push_back(pal);
            }
        }
        catch(...)
        {
        }
    }

    if(loaded_palettes.empty())
    {

        loaded_palettes.push_back({ "Default",
            { "#000000", "#0000AA", "#00AA00", "#00AAAA", "#AA0000", "#AA00AA", "#FFAA00", "#AAAAAA", "#555555", "#5555FF", "#55FF55", "#55FFFF", "#FF5555", "#FF55FF", "#FFFF55", "#FFFFFF" } });

        loaded_palettes.push_back({ "Rainbow",
            { "#FF0000", "#FF7F00", "#FFFF00", "#00FF00", "#0000FF", "#4B0082", "#9400D3" } });

        loaded_palettes.push_back({ "Pastel",
            { "#FFB3BA", "#FFDFBA", "#FFFFBA", "#BAFFC9", "#BAE1FF", "#D0BAFF", "#FFB3E6", "#EAC4D5" } });

        loaded_palettes.push_back({ "Cyberpunk",
            { "#FF003C", "#00FFFF", "#FCEE09", "#B900FF", "#00FF9F", "#FF00AA", "#7109AA", "#050A30" } });

        loaded_palettes.push_back({ "Ocean",
            { "#03045E", "#023E8A", "#0077B6", "#0096C7", "#00B4D8", "#48CAE4", "#90E0EF", "#CAF0F8" } });

        loaded_palettes.push_back({ "Sunset",
            { "#2A0845", "#6441A5", "#FF512F", "#F09819", "#FF9966", "#FFB75E", "#FFE259", "#FFA751" } });

        loaded_palettes.push_back({ "Forest",
            { "#081C15", "#1B4332", "#2D6A4F", "#40916C", "#52B788", "#74C69D", "#95D5B2", "#B7E4C7" } });

        loaded_palettes.push_back({ "Monochrome",
            { "#000000", "#1C1C1C", "#383838", "#555555", "#717171", "#8D8D8D", "#AAAAAA", "#C6C6C6", "#E2E2E2", "#FFFFFF" } });

        save_palettes();
    }

    if(current_palette_idx >= loaded_palettes.size())
    {
        current_palette_idx = 0;
    }
}

inline void createColorWheel(TextField& field)
{
    if(!palettes_initialized)
    {
        load_palettes();
        palettes_initialized = true;
    }

    static float colors[3] = { 0 };
    static bool upper = false;

    ImGui::SetNextWindowSizeConstraints(ImVec2(550, 420), ImVec2(1000, 1000));
    ImGui::Begin("Color");

    ImGui::BeginChild("LeftPane", ImVec2(280, 0), false);
    ImGui::ColorPicker3("Colors", colors);

    static Button apply_btn = {
        "Apply color",
        "Put the selected color",
    };

    ImGui::Checkbox("Upper Case", &upper);

    generateSlowedButton(apply_btn, [&field]() {
        std::string hex = "&#" + decimal_to_hex(colors, upper);
        if(field.hasSelection)
        {
            field.wrapSelection(hex, "&r");
        }
        else
        {
            field.wrapSelection(hex);
        }
    });
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPane", ImVec2(0, 0), true);

    ImGui::Text("Palettes");
    ImGui::Separator();

    if(ImGui::BeginCombo("##PaletteCombo", loaded_palettes[current_palette_idx].name.c_str()))
    {
        for(size_t i = 0; i < loaded_palettes.size(); i++)
        {
            bool is_selected = (current_palette_idx == i);
            if(ImGui::Selectable(loaded_palettes[i].name.c_str(), is_selected))
            {
                current_palette_idx = i;
            }
            if(is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::InputText("Rename Palette", &loaded_palettes[current_palette_idx].name);
    if(ImGui::IsItemDeactivatedAfterEdit())
    {
        save_palettes();
    }

    ImGui::Dummy(ImVec2(0, 5));

    static Button add_pal_btn = { "Add Palette", "Create a new empty palette" };
    generateSlowedButton(add_pal_btn, []() {
        std::string new_name = std::format("New Palette {}", loaded_palettes.size() + 1);
        
        loaded_palettes.push_back({ new_name, {} });
        current_palette_idx = loaded_palettes.size() - 1;
        save_palettes();
    });

    ImGui::SameLine();

    static Button remove_pal_btn = { "Remove", "Delete the currently selected palette"};
    generateSlowedButton(remove_pal_btn, []() {
        if(loaded_palettes.size() > 1)
        {
            loaded_palettes.erase(loaded_palettes.begin() + current_palette_idx);
            if(current_palette_idx >= loaded_palettes.size())
            {
                current_palette_idx = loaded_palettes.size() - 1;
            }
            save_palettes();
        }
    });

    ImGui::Separator();
    static Button add_color_btn = { "+ Add Current Color", "Add the color from the wheel to this palette"};
    generateSlowedButton(add_color_btn, []() {
        auto& pal = loaded_palettes[current_palette_idx];
        if(pal.colors.size() < 16)
        {
            pal.colors.push_back("#" + decimal_to_hex(colors, upper));
            save_palettes();
        }
    });

    ImGui::SameLine();

    static Button remove_color_btn = { "- Remove Last", "Remove the last color added to this palette" };
    generateSlowedButton(remove_color_btn, []() {
        auto& pal = loaded_palettes[current_palette_idx];
        if(!pal.colors.empty())
        {
            pal.colors.pop_back();
            save_palettes();
        }
    });

    static bool delete_mode = false;
    ImGui::PushStyleColor(ImGuiCol_Text, delete_mode ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
    ImGui::Checkbox("Delete Mode", &delete_mode);
    ImGui::PopStyleColor();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("When checked, clicking a color removes it from the palette.");
    }

    ImGui::Dummy(ImVec2(0, 10));

    auto& current_colors = loaded_palettes[current_palette_idx].colors;

    static std::vector<Tooltip> tooltip_states;
    if(tooltip_states.size() < current_colors.size())
    {
        tooltip_states.resize(current_colors.size());
    }

    ImGuiStyle& style = ImGui::GetStyle();
    float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    int color_to_delete = -1;

    for(size_t i = 0; i < current_colors.size(); i++)
    {
        ImGui::PushID(static_cast<int>(i));
        std::string hex_str = current_colors[i];
        ImVec4 col = hex_to_imvec4(hex_str);

        if(ImGui::ColorButton("##colbtn", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoAlpha, ImVec2(35, 35)))
        {
            if(delete_mode)
            {
                color_to_delete = static_cast<int>(i);
            }
            else
            {
                std::string insert_hex = hex_str;
                if(upper) to_upper_case(insert_hex);

                if(field.hasSelection)
                {
                    field.wrapSelection(insert_hex, "&r");
                }
                else
                {
                    field.wrapSelection(insert_hex);
                }
            }
        }

        ShowDelayedTooltip(tooltip_states[i], hex_str + (delete_mode ? " (Click to Remove)" : ""));

        float last_button_x2 = ImGui::GetItemRectMax().x;
        float next_button_x2 = last_button_x2 + style.ItemSpacing.x + 35.0f;
        if(i + 1 < current_colors.size() && next_button_x2 < window_visible_x2)
        {
            ImGui::SameLine();
        }

        ImGui::PopID();
    }

    if(color_to_delete != -1)
    {
        current_colors.erase(current_colors.begin() + color_to_delete);
        save_palettes();
    }

    ImGui::EndChild();
    ImGui::End();
}