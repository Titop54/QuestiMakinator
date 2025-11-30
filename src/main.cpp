#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_stdlib.h>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/WindowEnums.hpp>

#include <parser/raw.h>
#include <integration/kubejs.h>
#include <gui/display/Image.h>

#include <string>

// Structure to handle text selection state
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

// Improved tooltip function with delay
struct TooltipState {
    float hoverTime = 0.0f;
    bool isHovering = false;
};

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

// Callback to capture text selection
static int InputTextCallback(ImGuiInputTextCallbackData* data) {
    TextEditorState* state = static_cast<TextEditorState*>(data->UserData);
    if (state) {
        state->selectionStart = data->SelectionStart;
        state->selectionEnd = data->SelectionEnd;
        state->hasSelection = (data->SelectionStart != data->SelectionEnd);
        state->updateSelection();
    }
    return 0;
}

int main()
{
    sf::RenderWindow window(
                     sf::VideoMode(sf::Vector2u(800, 600)),
                    "QuestiMakinator",
                    sf::Style::Default,
                    sf::State::Windowed,
                    {}
    );

    auto desktop = sf::VideoMode::getDesktopMode();
    int x = desktop.size.x/2 - window.getSize().x/2;
    int y = desktop.size.y/2 - window.getSize().y/2;
    window.setPosition(sf::Vector2(x, y));

    if(!ImGui::SFML::Init(window)) return -1;

    window.setFramerateLimit(60);
    
    // Use our custom state
    TextEditorState editorState;
    std::string inputText2 = "";
    
    // Variables for effects with parameters
    std::string effectParam1 = "1.0";
    std::string effectParam2 = "1.0";
    std::string effectParam3 = "1.0";
    std::string customText = "";
    std::string colorHex = "FFFFFF";
    
    // State for radio buttons
    static int selected_option = 0;

    // Tooltip states for buttons
    TooltipState tooltipStates[100] = {}; // Enough for all buttons
    int tooltipIndex = 0;

    // After calling init, we need to pass the style
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.1f, 0.9f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.5f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.6f, 0.8f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.WindowRounding = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.FrameRounding = 0.0f;

    float zoom_factor = 1.0f;
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = zoom_factor;
    sf::Clock deltaClock;

    while(window.isOpen())
    {
        while(std::optional<sf::Event> event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            if(event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }
        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(window, dt);
        ImVec2 center = ImVec2(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        ImGui::Begin("Text Formatter");
        float width = ImGui::GetContentRegionAvail().x;

        // Reset tooltip index for this frame
        tooltipIndex = 0;

        // ===== GROUP 1: BASIC FORMATS =====
        ImGui::Text("Basic Formats:");
        
        if (ImGui::Button("Bold")) {
            editorState.text = editorState.wrapSelection("&l");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Bold text (&l)");
        ImGui::SameLine();
        
        if (ImGui::Button("Italic")) {
            editorState.text = editorState.wrapSelection("&o");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Italic text (&o)");
        ImGui::SameLine();

        if (ImGui::Button("Underline")) {
            editorState.text = editorState.wrapSelection("&n");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Underlined text (&n)");
        ImGui::SameLine();

        if (ImGui::Button("Strikethrough")) {
            editorState.text = editorState.wrapSelection("&m");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Strikethrough text (&m)");
        ImGui::SameLine();

        if (ImGui::Button("Reset")) {
            editorState.text = editorState.wrapSelection("&r");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Reset formatting (&r)");
        ImGui::SameLine();

        if (ImGui::Button("Obfuscated")) {
            editorState.text = editorState.wrapSelection("&k");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Obfuscated text (&k)");
        ImGui::SameLine();
        ImGui::NewLine();

        // ===== GROUP 2: BASIC COLORS =====
        ImGui::Text("Basic Colors:");
        
        // First row of colors
        if (ImGui::Button("Black (&0)")) {
            editorState.text = editorState.wrapSelection("&0");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Black color (&0)");
        ImGui::SameLine();

        if (ImGui::Button("Dark Blue (&1)")) {
            editorState.text = editorState.wrapSelection("&1");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Dark blue color (&1)");
        ImGui::SameLine();

        if (ImGui::Button("Dark Green (&2)")) {
            editorState.text = editorState.wrapSelection("&2");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Dark green color (&2)");
        ImGui::SameLine();

        if (ImGui::Button("Light Blue (&3)")) {
            editorState.text = editorState.wrapSelection("&3");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Light blue color (&3)");
        ImGui::SameLine();

        if (ImGui::Button("Red (&4)")) {
            editorState.text = editorState.wrapSelection("&4");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Red color (&4)");
        ImGui::SameLine();

        if (ImGui::Button("Purple (&5)")) {
            editorState.text = editorState.wrapSelection("&5");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Purple color (&5)");
        ImGui::SameLine();

        // Second row of colors
        if (ImGui::Button("Orange (&6)")) {
            editorState.text = editorState.wrapSelection("&6");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Orange color (&6)");
        ImGui::SameLine();

        if (ImGui::Button("Light Gray (&7)")) {
            editorState.text = editorState.wrapSelection("&7");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Light gray color (&7)");
        ImGui::NewLine();

        if (ImGui::Button("Gray (&8)")) {
            editorState.text = editorState.wrapSelection("&8");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Gray color (&8)");
        ImGui::SameLine();

        if (ImGui::Button("Blue (&9)")) {
            editorState.text = editorState.wrapSelection("&9");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Blue color (&9)");
        ImGui::SameLine();

        if (ImGui::Button("Light Green (&a)")) {
            editorState.text = editorState.wrapSelection("&a");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Light green color (&a)");
        ImGui::SameLine();

        if (ImGui::Button("Aqua (&b)")) {
            editorState.text = editorState.wrapSelection("&b");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Aqua color (&b)");
        ImGui::SameLine();

        // Third row of colors
        if (ImGui::Button("Light Red (&c)")) {
            editorState.text = editorState.wrapSelection("&c");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Light red color (&c)");
        ImGui::SameLine();

        if (ImGui::Button("Pink (&d)")) {
            editorState.text = editorState.wrapSelection("&d");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Pink color (&d)");
        ImGui::SameLine();

        if (ImGui::Button("Yellow (&e)")) {
            editorState.text = editorState.wrapSelection("&e");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Yellow color (&e)");
        ImGui::SameLine();

        if (ImGui::Button("White (&f)")) {
            editorState.text = editorState.wrapSelection("&f");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "White color (&f)");
        ImGui::SameLine();
        ImGui::NewLine();

        // ===== GROUP 3: SPECIAL ACTIONS =====
        ImGui::Text("Special Actions:");
        
        if (ImGui::Button("Insert URL")) {
            editorState.text = editorState.wrapSelection("&@url:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Insert URL: &@url:\"url\"");
        ImGui::SameLine();

        if (ImGui::Button("Insert Text")) {
            editorState.text = editorState.wrapSelection("&@in:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Insert chat text: &@in:\"text\"");
        ImGui::SameLine();

        if (ImGui::Button("Open File")) {
            editorState.text = editorState.wrapSelection("&@file:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Open file: &@file:\"path\"");
        ImGui::SameLine();

        if (ImGui::Button("Run Command")) {
            editorState.text = editorState.wrapSelection("&@command:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Execute command: &@command:\"command\"");
        ImGui::SameLine();

        if (ImGui::Button("Copy Text")) {
            editorState.text = editorState.wrapSelection("&@copy:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Copy text: &@copy:\"text\"");
        ImGui::SameLine();

        if (ImGui::Button("Change Quest")) {
            editorState.text = editorState.wrapSelection("&@change:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Change quest: &@change:\"text\"");
        ImGui::SameLine();

        if (ImGui::Button("New Page")) {
            editorState.text = editorState.wrapSelection("&@page");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "New page: &@page");
        ImGui::SameLine();
        ImGui::NewLine();

        // ===== GROUP 4: HOVER EFFECTS / EFFECTS =====
        ImGui::Text("Hover Effects:");
        
        if (ImGui::Button("Show Text Hover")) {
            editorState.text = editorState.wrapSelection("&&text:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Show text on hover: &&text:\"text\"");
        ImGui::SameLine();

        if (ImGui::Button("Show Item Hover")) {
            editorState.text = editorState.wrapSelection("&&item:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Show item on hover: &&item:\"item\"");
        ImGui::SameLine();
        
        if (ImGui::Button("Shadow Text")) {
            editorState.text = editorState.wrapSelection("&&shadow:\"", "\"");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Put a shadow on the text (Not working) (#AARRGGBB)");
        ImGui::SameLine();
        ImGui::NewLine();

        // ===== GROUP 5: MOD EFFECTS =====
        ImGui::Text("Mod Effects:");
        
        // First row of effects
        if (ImGui::Button("Typewriter")) {
            editorState.text = editorState.wrapSelection("<typewriter>", "</typewriter>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Typewriter effect");
        ImGui::SameLine();

        if (ImGui::Button("Bounce")) {
            editorState.text = editorState.wrapSelection("<bounce a=1.0 f=1.0 w=1.0>", "</bounce>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Vertical bounce effect");
        ImGui::SameLine();

        if (ImGui::Button("Fade")) {
            editorState.text = editorState.wrapSelection("<fade a=0.3 f=1.0 w=0.0>", "</fade>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Fade effect");
        ImGui::SameLine();
        
        if (ImGui::Button("Glitch")) {
            editorState.text = editorState.wrapSelection("<glitch f=1.0 j=0.015 b=0.003 s=0.08>", "</glitch>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Glitch effect");
        ImGui::SameLine();

        // Second row of effects
        if (ImGui::Button("Gradient")) {
            editorState.text = editorState.wrapSelection("<grad from=#7FFFD4 to=#1E90FF hue=false f=0.0 sp=20.0 uni=false>", "</grad>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Color gradient effect");
        ImGui::SameLine();

        if (ImGui::Button("Neon")) {
            editorState.text = editorState.wrapSelection("<neon p=10 r=2 a=0.12>", "</neon>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Neon effect");
        ImGui::SameLine();

        if (ImGui::Button("Pendulum")) {
            editorState.text = editorState.wrapSelection("<pend f=1.0 a=30 r=0.0>", "</pend>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Circular pendulum effect");
        ImGui::SameLine();

        if (ImGui::Button("Pulse")) {
            editorState.text = editorState.wrapSelection("<pulse base=0.75 a=1.0 f=1.0 w=0.0>", "</pulse>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Brightness pulse effect");
        ImGui::SameLine();

        // Third row of effects
        if (ImGui::Button("Rainbow")) {
            editorState.text = editorState.wrapSelection("<rainb f=1.0 w=1.0>", "</rainb>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Rainbow color effect");
        ImGui::SameLine();

        if (ImGui::Button("Shadow")) {
            editorState.text = editorState.wrapSelection("<shadow x=0.0 y=0.0 c=000000 a=1.0>", "</shadow>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Modify text shadow");
        ImGui::SameLine();

        if (ImGui::Button("Shake")) {
            editorState.text = editorState.wrapSelection("<shake a=1.0 f=1.0>", "</shake>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Random shake effect");
        ImGui::SameLine();

        if (ImGui::Button("Swing")) {
            editorState.text = editorState.wrapSelection("<swing a=1.0 f=1.0 w=0.0>", "</swing>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Character swing effect");
        ImGui::NewLine();

        // Fourth row of effects
        if (ImGui::Button("Turbulence")) {
            editorState.text = editorState.wrapSelection("<turb a=1.0 f=1.0>", "</turb>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Turbulence effect");
        ImGui::SameLine();

        if (ImGui::Button("Wave")) {
            editorState.text = editorState.wrapSelection("<wave a=1.0 f=1.0 w=1.0>", "</wave>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Wave-like undulation effect");
        ImGui::SameLine();

        if (ImGui::Button("Wiggle")) {
            editorState.text = editorState.wrapSelection("<wiggle a=1.0 f=1.0 w=1.0>", "</wiggle>");
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Per-character random movement");
        ImGui::NewLine();
        ImGui::Separator();

        // Show selection information (debug)
        if (editorState.hasSelection) {
            ImGui::Text("Selection: '%s' (%d-%d)", 
                       editorState.selectedText.c_str(), 
                       editorState.selectionStart, 
                       editorState.selectionEnd);
        } else {
            ImGui::Text("No selection");
        }

        // First textfield with callback to capture selection
        ImVec2 size(width, ImGui::GetTextLineHeight() * 8);
        ImGui::InputTextMultiline("##input1", &editorState.text, size, 
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways,
            InputTextCallback, &editorState);

        // Second textfield
        ImGui::InputTextMultiline("##input2", &inputText2, size, 
            ImGuiInputTextFlags_ReadOnly);

        // Radio buttons (only one selected)
        ImGui::Text("Options:");
        ImGui::RadioButton("Array", &selected_option, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Extra", &selected_option, 1);
        ImGui::SameLine();

        // Action buttons
        if(ImGui::Button("Convert"))
        {
            inputText2 = raw::to_json(editorState.text, selected_option ? true : false);
            ImGui::SetClipboardText(inputText2.c_str());
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Convert text to JSON format and copy to clipboard");
        ImGui::SameLine();

        // Button to copy text
        if(ImGui::Button("Copy Text Output"))
        {
            ImGui::SetClipboardText(inputText2.c_str());
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Copy converted text to clipboard");
        ImGui::SameLine();
        
        if(ImGui::Button("Reload Minecraft (Not yet)"))
        {
            
            if(client.connect())
            {
                if(client.sendReloadCommand(ReloadType::SERVER)) 
                {
                    inputText2 = "Reload successful!";
                }
                else
                {
                    inputText2 = "Error executing reload";
                }
            }
            else
            {
                inputText2 = "Could not connect to server";
            }
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Reload Minecraft scripts (requires KubeJS)");
        ImGui::SameLine();

        if(ImGui::Button("Change zoom"))
        {
            zoom_factor += 1.0f;
            if(zoom_factor >= 4.0f)
            {
                zoom_factor = 1.0f;
            }
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = zoom_factor;
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Changes zoom level from 1 to 4");
        ImGui::SameLine();

        if(ImGui::Button("Exit"))
        {
            window.close();
        }
        ShowDelayedTooltip(tooltipStates[tooltipIndex++], "Close the application");
        ImGui::SameLine();


        ImGui::End();
        //browser.update(deltaClock.getElapsedTime().asSeconds());
        createKubejsImageBrowser(dt.asSeconds());

        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();

    return 0;
}