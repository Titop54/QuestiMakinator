#include "gui/display/textfield_selection.h"

int InputTextCallback(ImGuiInputTextCallbackData* data) {
    TextEditorState* state = static_cast<TextEditorState*>(data->UserData);
    if (state) {
        state->selectionStart = data->SelectionStart;
        state->selectionEnd = data->SelectionEnd;
        state->hasSelection = (data->SelectionStart != data->SelectionEnd);
        state->updateSelection();
    }
    return 0;
}