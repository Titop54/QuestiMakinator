#pragma once
#include <cstddef>
#include <functional>
#include <imgui.h>
#include <string>
#include <vector>

//Based on this: https://gist.github.com/harold-b/7dcc02557c2b15d76c61fde1186e31d0#file-dear-imgui-selectable-popup-example

struct AutoCompleteState
{
    bool isPopupOpen = false;
    int  activeIdx = -1; // Arrows
    int  clickedIdx = -1; // Mouse
    bool selectionChanged = false;
};

struct CallbackData
{
    AutoCompleteState* state;
    std::vector<std::string>* candidates;
};

inline void SetInputFromActiveIndex(ImGuiInputTextCallbackData* data, const std::string& text)
{
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text.c_str());
    data->BufDirty = true;
}

inline int InputCallback(ImGuiInputTextCallbackData* data)
{
    CallbackData* cbData = (CallbackData*)data->UserData;
    if (!cbData || !cbData->state || !cbData->candidates) return 0;

    AutoCompleteState& state = *cbData->state;
    std::vector<std::string>& candidates = *cbData->candidates;

    switch (data->EventFlag)
    {
        case ImGuiInputTextFlags_CallbackCompletion: // Tab
            if (state.isPopupOpen && state.activeIdx >= 0 && static_cast<size_t>(state.clickedIdx) < candidates.size())
            {
                SetInputFromActiveIndex(data, candidates[state.activeIdx]);
                state.isPopupOpen = false;
                state.activeIdx = -1;
            }
            break;

        case ImGuiInputTextFlags_CallbackHistory:
            if (candidates.empty()) break;
            
            state.isPopupOpen = true;

            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (state.activeIdx > 0) {
                    state.activeIdx--;
                    state.selectionChanged = true;
                }
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (state.activeIdx < (int)candidates.size() - 1) {
                    state.activeIdx++;
                    state.selectionChanged = true;
                } else if (state.activeIdx == -1) {
                    state.activeIdx = 0;
                    state.selectionChanged = true;
                }
            }
            break;

        case ImGuiInputTextFlags_CallbackAlways:
            if (state.clickedIdx != -1 && static_cast<size_t>(state.clickedIdx) < candidates.size())
            {
                SetInputFromActiveIndex(data, candidates[state.clickedIdx]);
                state.isPopupOpen = false;
                state.activeIdx = -1;
                state.clickedIdx = -1;
            }
            if(state.isPopupOpen && ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                state.isPopupOpen = false;
                state.activeIdx = -1;
                //data->EventChar = 0;
            }
            break;
    }
    return 0;
}

inline std::vector<std::string> filterResults(const std::string& text, 
                                              const std::vector<std::string> validOptions,
                                              const int& limit,
                                              const AutoCompleteState& state
                                            )
{
    if(ImGui::IsWindowFocused() || state.isPopupOpen) 
    {
        std::vector<std::string> options;

        if (!text.empty()) {
            int count = 0;
            for (const auto& id : validOptions) {
                if (id.find(text) != std::string::npos) {
                    options.emplace_back(id);
                    count++;
                    if(count > limit) break;
                }
            }
            return options;
        }
    }
    return {};
}


inline void drawMenu(AutoCompleteState& state,
                     const std::string& name,
                     const std::vector<std::string>& list_candidates,
                     std::string& text_field,
                     const bool& isInputActive,
                     std::function<void(std::string&)> function_to_call
                    )
{
    if(state.isPopupOpen)
    {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 200));
            
        ImGuiWindowFlags popupFlags = 
        ImGuiWindowFlags_NoTitleBar          | 
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_HorizontalScrollbar |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_NoFocusOnAppearing  |
        ImGuiWindowFlags_Tooltip;

        bool isPopupHovered = false;
        if (!ImGui::IsPopupOpen(name.data())) {
            ImGui::OpenPopup(name.data());
        }

        if(ImGui::BeginPopup(name.data(), popupFlags))
        {
            isPopupHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
            //ImGui::PushAllowKeyboardFocus(false);

            for(int i = 0; i < (int)list_candidates.size(); i++)
            {
                bool isIndexActive = (state.activeIdx == i);

                if (isIndexActive) {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.31f));
                }

                if (ImGui::Selectable(list_candidates[i].c_str(), isIndexActive))
                {
                    text_field = list_candidates[i];
                    state.clickedIdx = i;
                    state.isPopupOpen = false;
                    function_to_call(text_field);
                    ImGui::SetKeyboardFocusHere(-1);
                    ImGui::CloseCurrentPopup();
                }

                if (isIndexActive) {
                    ImGui::PopStyleColor();
                    if (state.selectionChanged) {
                        ImGui::SetScrollHereY();
                        state.selectionChanged = false;
                    }
                }
            }
            state.isPopupOpen = true;
            //ImGui::PopAllowKeyboardFocus();
            ImGui::EndPopup();
        }
        else
        {
            state.isPopupOpen = false;
            state.activeIdx = -1;
        }

        if (ImGui::IsMouseClicked(0)) 
        {
            if (!isPopupHovered) 
            {
                if (!isInputActive) 
                {
                    state.isPopupOpen = false;
                    state.activeIdx = -1;
                    if (ImGui::IsPopupOpen(name.data())) {
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }
    }
    
    if (state.isPopupOpen && !isInputActive && !ImGui::IsWindowFocused()) {
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
            state.isPopupOpen = false;
        }
    }
}