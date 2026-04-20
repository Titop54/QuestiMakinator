#pragma once

#include <imgui.h>
#include <string>

struct TextField
{
    std::string text;
    bool hasSelection = false;
    int selectionStart = 0;
    int selectionEnd = 0;
    int cursor = 0;

    std::string getSelected() const
    {
        if (hasSelection && selectionStart != selectionEnd)
        {
            int start = std::min(selectionStart, selectionEnd);
            int end = std::max(selectionStart, selectionEnd);
            return text.substr(start, end - start);
        }
        return "";
    }

    void wrapSelection(const std::string& prefix, const std::string& suffix = "")
    {
        if(hasSelection && selectionStart != selectionEnd)
        {
            int start = std::min(selectionStart, selectionEnd);
            int end = std::max(selectionStart, selectionEnd);
            std::string selected = text.substr(start, end - start);
            std::string sub = prefix + selected + suffix;

            text.replace(start, end - start, sub);

            selectionStart = start + prefix.length();
            selectionEnd = selectionStart + selected.length();
            cursor = selectionEnd;
            hasSelection = true;
        }
        else
        {
            int pos = cursor;
            text.insert(pos, prefix + suffix);

            cursor = pos + prefix.length();
            selectionStart = cursor;
            selectionEnd = cursor;
            hasSelection = false;
        }
    }
};

inline int InputTextCallback(ImGuiInputTextCallbackData* data)
{
    TextField* state = static_cast<TextField*>(data->UserData);
    if(state)
    {
        state->cursor = data->CursorPos;
        state->selectionStart = data->SelectionStart;
        state->selectionEnd = data->SelectionEnd;
        state->hasSelection = (data->SelectionStart != data->SelectionEnd);
        state->getSelected();
    }
    return 0;
}