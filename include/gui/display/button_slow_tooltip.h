#pragma once

#include "gui/display/textfield_selection.h"
#include <functional>

struct TooltipState {
    float hoverTime = 0.0f;
    bool isHovering = false;
};

struct FormatButtonData {
    const char* label;
    const char* prefix; //prefix
    const char* tooltip;
    const char* ending = "\"";
    TooltipState state = {};
};

bool ShowDelayedTooltip(TooltipState& state, const char* desc);

bool generateSlowedButton(TextEditorState& editorState, FormatButtonData& data);

bool generateSlowedButton(FormatButtonData& data, std::function<void()> function = nullptr);