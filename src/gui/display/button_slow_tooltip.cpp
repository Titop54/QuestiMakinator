#include "gui/display/button_slow_tooltip.h"
#include <functional>
#include <imgui.h>

bool ShowDelayedTooltip(TooltipState& state, const char* desc) {
    bool showTooltip = false;
    if (ImGui::IsItemHovered()) {
        if (!state.isHovering) {
            state.isHovering = true;
            state.hoverTime = 0.0f;
        }
        state.hoverTime += ImGui::GetIO().DeltaTime;
        
        if (state.hoverTime >= 1.0f) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
            showTooltip = true;
        }
    } else {
        state.isHovering = false;
        state.hoverTime = 0.0f;
    }
    
    return showTooltip;
}

bool generateSlowedButton(TextEditorState& editorState, FormatButtonData& data) {
    bool clicked = false;
    if (ImGui::Button(data.label)) {
        editorState.text = editorState.wrapSelection(data.prefix, data.ending);
        clicked = true;
    }
    ShowDelayedTooltip(data.state, data.tooltip);
    
    return clicked;
}

bool generateSlowedButton(FormatButtonData& data, std::function<void()> onClickAction) {
    bool clicked = false;
    
    if (ImGui::Button(data.label)) {
        if (onClickAction) {
            onClickAction();
        }
        clicked = true;
    }
    ShowDelayedTooltip(data.state, data.tooltip);
    
    return clicked;
}