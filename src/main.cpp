#include "parser/arguments.h"
#include "parser/splitter.h"
#include "parser/merger.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h> 

#include <backward.hpp>

#include <parser/raw.h>
#include <integration/kubejs.h>
#include <gui/display/Image.h>
#include <gui/display/window.h>
#include <gui/display/button_slow_tooltip.h>
#include <gui/display/textfield_selection.h>

int main(int argc, char* argv[])
{
    backward::SignalHandling sh;
    args::parser args;
    args.parse_data(argc, argv);

    if(args.result.contains("--splitter")) ftb_splitter::split(args);
    else if(args.result.contains("--merge")) ftb_merger::merge(args);

    std::ofstream crashFile("errors.txt");
    std::ofstream logFile("logs.txt");

    auto originalCerr = std::cerr.rdbuf();
    auto originalCout = std::cout.rdbuf();

    std::cerr.rdbuf(crashFile.rdbuf());
    std::cout.rdbuf(logFile.rdbuf());

    const char* glsl_version = "#version 330 core";
    GLFWwindow* window = WindowUtils::createWindow(); 
    if(!window)
    {
        std::cerr << "Failed to initialize GLFW or create window\n";
        std::cerr.rdbuf(originalCerr);
        std::cout.rdbuf(originalCout);

        logFile.close();
        crashFile.close();
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    TextEditorState editorState;
    std::string inputText2 = "";
    
    int selected_option = 0;

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
    io.FontGlobalScale = zoom_factor;

    double lastTime = glfwGetTime();

    std::vector<FormatButtonData> basicFormats = {
        {"Bold", "&l", "Bold text (&l)"},
        {"Italic", "&o", "Italic text (&o)"},
        {"Underline", "&n", "Underlined text (&n)"},
        {"Strikethrough", "&m", "Strikethrough text (&m)"},
        {"Reset", "&r", "Reset formatting (&r)"},
        {"Obfuscated", "&k", "Obfuscated text (&k)"}
    };
    
    std::vector<FormatButtonData> basicColors = {
        {"Black (&0)", "&0", "Black color (&0)"},
        {"Dark Blue (&1)", "&1", "Dark blue color (&1)"},
        {"Dark Green (&2)", "&2", "Dark green color (&2)"},
        {"Light Blue (&3)", "&3", "Light blue color (&3)"},
        {"Red (&4)", "&4", "Red color (&4)"},
        {"Purple (&5)", "&5", "Purple color (&5)"},
        {"Orange (&6)", "&6", "Orange color (&6)"},
        {"Light Gray (&7)", "&7", "Light gray color (&7)"},
        {"Gray (&8)", "&8", "Gray color (&8)"},
        {"Blue (&9)", "&9", "Blue color (&9)"},
        {"Light Green (&a)", "&a", "Light green color (&a)"},
        {"Aqua (&b)", "&b", "Aqua color (&b)"},
        {"Light Red (&c)", "&c", "Light red color (&c)"},
        {"Pink (&d)", "&d", "Pink color (&d)"},
        {"Yellow (&e)", "&e", "Yellow color (&e)"},
        {"White (&f)", "&f", "White color (&f)"}
    };

    std::vector<FormatButtonData> specialActions = {
        {"Insert URL", "&@url:\"", "Insert URL: &@url:\"url\"", "\""},
        {"Insert Text", "&@in:\"", "Insert chat text: &@in:\"text\"", "\""},
        {"Open File", "&@file:\"", "Open file: &@file:\"path\"", "\""},
        {"Run Command", "&@command:\"", "Execute command: &@command:\"command\"", "\""},
        {"Copy Text", "&@copy:\"", "Copy text: &@copy:\"text\"", "\""},
        {"Change Quest", "&@change:\"", "Change quest: &@change:\"text\"", "\""},
        {"New Page", "&@page", "New page: &@page", ""}
    };

    std::vector<FormatButtonData> hoverEffects = {
        {"Show Text Hover", "&&text:\"", "Show text on hover: &&text:\"text\"", "\""},
        {"Show Item Hover", "&&item:\"", "Show item on hover: &&item:\"item\"", "\""},
        {"Shadow Text", "&&shadow:\"", "Put a shadow on the text (Not working) (#AARRGGBB)", "\""}
    };

    std::vector<FormatButtonData> modEffects = {
        //row 1
        {"Typewriter", "<typewriter>", "Typewriter effect", "</typewriter>"},
        {"Bounce", "<bounce a=1.0 f=1.0 w=1.0>", "Vertical bounce effect", "</bounce>"},
        {"Fade", "<fade a=0.3 f=1.0 w=0.0>", "Fade effect", "</fade>"},
        {"Glitch", "<glitch f=1.0 j=0.015 b=0.003 s=0.08>", "Glitch effect", "</glitch>"},
        //row 2
        {"Gradient", "<grad from=#7FFFD4 to=#1E90FF hue=false f=0.0 sp=20.0 uni=false>", "Color gradient effect", "</grad>"},
        {"Neon", "<neon p=10 r=2 a=0.12>", "Neon effect", "</neon>"},
        {"Pendulum", "<pend f=1.0 a=30 r=0.0>", "Circular pendulum effect", "</pend>"},
        {"Pulse", "<pulse base=0.75 a=1.0 f=1.0 w=0.0>", "Brightness pulse effect", "</pulse>"},
        //row 3
        {"Rainbow", "<rainb f=1.0 w=1.0>", "Rainbow color effect", "</rainb>"},
        {"Shadow", "<shadow x=0.0 y=0.0 c=000000 a=1.0>", "Modify text shadow", "</shadow>"},
        {"Shake", "<shake a=1.0 f=1.0>", "Random shake effect", "</shake>"},
        {"Swing", "<swing a=1.0 f=1.0 w=0.0>", "Character swing effect", "</swing>"},
        //row 4
        {"Turbulence", "<turb a=1.0 f=1.0>", "Turbulence effect", "</turb>"},
        {"Wave", "<wave a=1.0 f=1.0 w=1.0>", "Wave-like undulation effect", "</wave>"},
        {"Wiggle", "<wiggle a=1.0 f=1.0 w=1.0>", "Per-character random movement", "</wiggle>"}
    };

    std::vector<FormatButtonData> actionButtons = {
        {"Convert", "", "Convert text to JSON format and copy to clipboard"}, 
        {"Copy Text Output", "", "Copy converted text to clipboard"}, 
        {"Reload Minecraft (Not yet)", "", "Reload Minecraft scripts (requires KubeJS)"},
        {"Change zoom", "", "Changes zoom level from 1 to 4"},
        {"Exit", "", "Close the application"}
    };

    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    ImVec2 center = ImVec2(win_w / 2.0f, win_h / 2.0f);
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    {
    KubeJSImageBrowser browser;
    bool browserFirstRun = true;
    while (!glfwWindowShouldClose(window))
    {
        // Procesar eventos
        glfwPollEvents();

        // Calcular Delta Time
        double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        // Iniciar nuevo frame de ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Text Formatter");
        float width = ImGui::GetContentRegionAvail().x;

        ImGui::Text("Basic Formats:");
        
        for (size_t i = 0; i < basicFormats.size(); ++i) {
            auto& data = basicFormats[i];
            
            generateSlowedButton(editorState, data);

            if (i < basicFormats.size() - 1) {
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();

        ImGui::Text("Basic Colors:");
        for (size_t i = 0; i < basicColors.size(); ++i) {
            auto& data = basicColors[i];
            
            generateSlowedButton(editorState, data);
            
            if ((i + 1) % 6 != 0 && i < basicColors.size() - 1) {
                ImGui::SameLine();
            } else if (i < basicColors.size() - 1) {
                ImGui::NewLine();
            }
        }
        
        ImGui::NewLine();

        ImGui::Text("Special Actions:");
        for (size_t i = 0; i < specialActions.size(); ++i) {
            auto& data = specialActions[i];
            
            generateSlowedButton(editorState, data);

            if (i < specialActions.size() - 1) {
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();

        ImGui::Text("Hover Effects:");
        for (size_t i = 0; i < hoverEffects.size(); ++i) {
            auto& data = hoverEffects[i];
            
            generateSlowedButton(editorState, data);
            
            if (i < hoverEffects.size() - 1) {
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();

        ImGui::Text("Mod Effects:");
        for (size_t i = 0; i < modEffects.size(); ++i) {
            auto& data = modEffects[i];
            
            generateSlowedButton(editorState, data);

            if ((i + 1) % 9 != 0 && i < modEffects.size() - 1) {
                ImGui::SameLine();
            }
        }
        ImGui::NewLine();
        ImGui::Separator();

        // Show selection information (debug)
        if(editorState.hasSelection)
        {
            ImGui::Text("Selection: '%s' (%d-%d)", 
                       editorState.selectedText.c_str(), 
                       editorState.selectionStart, 
                       editorState.selectionEnd);
        }
        else
        {
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
        size_t action_idx = 0;
        generateSlowedButton(actionButtons[action_idx++], [&](){
            inputText2 = raw::to_json(editorState.text, selected_option ? true : false);
            ImGui::SetClipboardText(inputText2.c_str());
        });
        ImGui::SameLine();

        generateSlowedButton(actionButtons[action_idx++], [&](){
            ImGui::SetClipboardText(inputText2.c_str());
        });
        ImGui::SameLine();
        
        generateSlowedButton(actionButtons[action_idx++], [&](){
            if(!client.connect()) inputText2 = "Couldn't connect to server";
            if(!client.sendReloadCommand(ReloadType::SERVER)) inputText2 = "Error executing reload";
            inputText2 = "Reload successful!";
        });
        ImGui::SameLine();

        generateSlowedButton(actionButtons[action_idx++], [&](){
            zoom_factor += 1.0f;
            if(zoom_factor >= 4.0f)
            {
                zoom_factor = 1.0f;
            }
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = zoom_factor;
        });
        ImGui::SameLine();

        generateSlowedButton(actionButtons[action_idx++], [&](){
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        });
        ImGui::SameLine();


        ImGui::End();

        createKubejsImageBrowser(browser, browserFirstRun, dt, window);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cerr.rdbuf(originalCerr);
    std::cout.rdbuf(originalCout);

    logFile.close();
    crashFile.close();

    return 0;
}