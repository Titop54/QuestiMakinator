#pragma once

#include "gui/display/textfield_selection.h"
#include <functional>
#include <imgui.h>

//Internal use
struct Tooltip
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

//in pixels
struct Size
{
    float x = 0;
    float y = 0;

    ImVec2 vector() const
    {
        return {x, y};
    }

    void fill()
    {
        x = -1;
    }
};

struct Button
{
    const char* label;
    const char* prefix; //prefix
    const char* tooltip;
    const char* ending = "\"";
    Position pos = {};
    Size size = {};
    Tooltip state = {};
};

bool ShowDelayedTooltip(Tooltip& state, const char* desc);

bool generateSlowedButton(TextField& editorState, Button& data);

bool generateSlowedButton(Button& data, std::function<void()> function = nullptr);