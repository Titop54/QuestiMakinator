#pragma once

#include <imgui.h>
#include <string>

struct TextEditorState {
    std::string text;
    bool hasSelection = false;
    int selectionStart = 0;
    int selectionEnd = 0;
    std::string selectedText;
    
    void updateSelection() {
        if (hasSelection && selectionStart != selectionEnd) {
            int start = std::min(selectionStart, selectionEnd);
            int end = std::max(selectionStart, selectionEnd);
            selectedText = text.substr(start, end - start);
        } else {
            selectedText.clear();
            hasSelection = false;
        }
    }
    
    std::string wrapSelection(const std::string& prefix, const std::string& suffix = "") {
        if (hasSelection && selectionStart != selectionEnd) {
            int start = std::min(selectionStart, selectionEnd);
            int end = std::max(selectionStart, selectionEnd);
            
            std::string result = text;
            result.replace(start, end - start, prefix + selectedText + suffix);
            
            // Update positions after replacement
            selectionStart = start + prefix.length();
            selectionEnd = selectionStart + selectedText.length();
            
            return result;
        } else {
            // If no selection, add at the end
            return text + prefix + suffix;
        }
    }
};

int InputTextCallback(ImGuiInputTextCallbackData* data);