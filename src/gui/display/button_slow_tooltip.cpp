#include "gui/display/button_slow_tooltip.h"
#include <functional>
#include <imgui.h>

bool ShowDelayedTooltip(Tooltip& state, const char* desc)
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
            ImGui::TextUnformatted(desc);
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

bool generateSlowedButton(TextField& editorState, Button& data)
{
    bool clicked = false;
    ImVec2 original_position = ImGui::GetCursorPos();
    if(data.pos.does_work())
    {
        ImGui::SetCursorPos(data.pos.vector());
    }
    
    if(ImGui::Button(data.label, data.size.vector()))
    {
        editorState.text = editorState.wrapSelection(data.prefix, data.ending);
        clicked = true;
    }
    ShowDelayedTooltip(data.state, data.tooltip);
    ImGui::SetCursorPos(original_position);

    return clicked;
}

bool generateSlowedButton(Button& data, std::function<void()> onClickAction)
{
    bool clicked = false;
    ImVec2 original_position = ImGui::GetCursorPos();
    if(data.pos.does_work())
    {
        ImGui::SetCursorPos(data.pos.vector());
    }

    if(ImGui::Button(data.label, data.size.vector()))
    {
        if(onClickAction)
        {
            onClickAction();
        }
        clicked = true;
    }
    ShowDelayedTooltip(data.state, data.tooltip);
    ImGui::SetCursorPos(original_position);
    
    return clicked;
}