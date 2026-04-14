#include "gui/display/textfield_selection.h"

int InputTextCallback(ImGuiInputTextCallbackData* data) {
    TextField* state = static_cast<TextField*>(data->UserData);
    if (state) {
        state->selectionStart = data->SelectionStart;
        state->selectionEnd = data->SelectionEnd;
        state->hasSelection = (data->SelectionStart != data->SelectionEnd);
        state->updateSelection();
    }
    return 0;
}