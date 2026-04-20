#include "gui/elements/button.h"
#include <functional>
#include <imgui.h>

bool ShowDelayedTooltip(Tooltip& state, std::string desc)
{
    bool showTooltip = false;
    if(ImGui::IsItemHovered())
    {
        if(!state.isHovering)
        {
            state.isHovering = true;
            state.hoverTime = 0.0f;
        }
        state.hoverTime += ImGui::GetIO().DeltaTime;
        
        if(state.hoverTime >= 1.0f)
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc.data());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
            showTooltip = true;
        }
    }
    else
    {
        state.isHovering = false;
        state.hoverTime = 0.0f;
    }
    
    return showTooltip;
}

bool generateSlowedButton(TextField& field, Button& button)
{
    bool clicked = false;
    ImVec2 original_position = ImGui::GetCursorPos();
    if(button.pos.does_work())
    {
        ImGui::SetCursorPos(button.pos.vector());
    }
    
    if(ImGui::Button(button.label.data(), button.size.vector()))
    {
        field.wrapSelection(button.prefix, button.ending);
        clicked = true;
    }
    ShowDelayedTooltip(button.state, button.tooltip);
    if(button.pos.does_work()) 
    {
        ImGui::SetCursorPos(original_position);
    }

    return clicked;
}

bool generateSlowedButton(Button& button, std::function<void()> onClickAction)
{
    bool clicked = false;
    ImVec2 original_position = ImGui::GetCursorPos();
    if(button.pos.does_work())
    {
        ImGui::SetCursorPos(button.pos.vector());
    }

    if(ImGui::Button(button.label.data(), button.size.vector()))
    {
        if(onClickAction)
        {
            onClickAction();
        }
        clicked = true;
    }
    ShowDelayedTooltip(button.state, button.tooltip);
    if(button.pos.does_work()) 
    {
        ImGui::SetCursorPos(original_position);
    }
    
    return clicked;
}