#pragma once

#include "gui/display/textfield_selection.h"
#include <functional>
#include <imgui.h>

//Internal use
struct TooltipState
{
    float hoverTime = 0.0f;
    bool isHovering = false;
};

//When any is -1, then it doesnt work
struct Position
{
    float x = -1.0f;
    float y = -1.0f;

    bool does_work() const
    {
        return x != -1.0f && y != -1.0f;
    }

    ImVec2 vector() const
    {
        return {x,y};
    }
};

struct FormatButtonData
{
    const char* label;
    const char* prefix; //prefix
    const char* tooltip;
    const char* ending = "\"";
    Position pos = {};
    TooltipState state = {};
};

bool ShowDelayedTooltip(TooltipState& state, const char* desc);

bool generateSlowedButton(TextEditorState& editorState, FormatButtonData& data);

bool generateSlowedButton(FormatButtonData& data, std::function<void()> function = nullptr);