#pragma once
#include <functional>
#include <imgui.h>
#include <string>
#include <vector>

// Based on https://github.com/ocornut/imgui/issues/718

inline void draw_search_bar(const std::string& label,
    const std::vector<std::string>& candidates,
    std::string& current_value,
    std::function<void(std::string)> on_select)
{
    bool is_empty = current_value.empty();
    const char* preview = is_empty ? "Select an element..." : current_value.c_str();

    if(is_empty)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }

    ImGui::PushID(label.c_str());
    bool combo_open = ImGui::BeginCombo("##combo", preview);

    if(is_empty)
    {
        ImGui::PopStyleColor();
    }

    if(combo_open)
    {
        static ImGuiTextFilter filter;
        filter.Draw("##search", -FLT_MIN);

        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Type to filter results...");
        }

        if(ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere(-1);

        int displayed_count = 0;
        for(const std::string& candidate : candidates)
        {
            if(displayed_count >= 50)
                break;

            if(filter.PassFilter(candidate.c_str()))
            {
                displayed_count++;

                bool is_selected = (current_value == candidate);
                if(ImGui::Selectable(candidate.c_str(), is_selected))
                {
                    current_value = candidate;
                    if(on_select)
                    {
                        on_select(candidate);
                    }
                }

                if(is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        if(displayed_count >= 50)
        {
            ImGui::Separator();
            ImGui::TextDisabled("...and more. Refine your search.");
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();
}