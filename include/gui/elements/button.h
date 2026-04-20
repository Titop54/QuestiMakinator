#pragma once

#include "gui/elements/textfield.h"
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
    std::string label = "";
    std::string prefix = ""; //prefix
    std::string tooltip = "";
    std::string ending = "";
    Position pos = {};
    Size size = {};
    Tooltip state = {};
};

bool ShowDelayedTooltip(Tooltip& state, std::string desc);

bool generateSlowedButton(TextField& editorState, Button& data);

bool generateSlowedButton(Button& data, std::function<void()> function = nullptr);