#include "args/generate.h"
#include "args/merger.h"
#include "args/splitter.h"
#include "args/pack.h"
#include "gui/display/settings.h"
#include "parser/arguments.h"

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
#include <gui/display/colors.h>

int main(int argc, char* argv[])
{
    backward::SignalHandling sh;
    args::parser args;
    args.parse_data(argc, argv);

    if(args.result.contains("--generate")) gen::generate(args);
    if(args.result.contains("--splitter")) splitter::split(args);
    if(args.result.contains("--merger")) merger::merge(args);
    if(args.result.contains("--pack")) pack::pack(args);

    if(args.result.contains("--pack") || args.result.contains("--generate") ||
        args.result.contains("--merger") || args.result.contains("--splitter"))
    {
        return 0; // if we are using these, well, exit
    }

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

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    TextField field;
    std::string inputText2 = "";

    int selected_option = 0;

    settings::load();

    ImGuiIO& io = ImGui::GetIO();
    if(!settings::current.font_path.empty())
    {
        io.Fonts->AddFontFromFileTTF(settings::current.font_path.c_str(), 18.0f);
    }
    else
    {
        io.Fonts->AddFontDefault(); 
    }

    settings::apply_to_imgui(settings::saved);

    double lastTime = glfwGetTime();

    std::vector<Button> basicFormats = {
        { "Bold", "&l", "Bold text (&l)", "&r" },
        { "Italic", "&o", "Italic text (&o)", "&r" },
        { "Underline", "&n", "Underlined text (&n)", "&r" },
        { "Strikethrough", "&m", "Strikethrough text (&m)", "&r" },
        { "Reset", "&r", "Reset formatting (&r)" },
        { "Obfuscated", "&k", "Obfuscated text (&k)", "&r" },
        { "Rainbow", "&z", "Gradient using the rainbow (1.21.1+ only) (&z)", "&r" }
    };

    std::vector<Button> specialActions = {
        { "Insert URL", "&@url:\"", "Insert URL: &@url:\"url\"", "\"&r" },
        { "Insert Text", "&@in:\"", "Insert chat text: &@in:\"text\"", "\"&r" },
        { "Open File", "&@file:\"", "Open file: &@file:\"path\"", "\"&r" },
        { "Run Command", "&@command:\"", "Execute command: &@command:\"command\"", "\"&r" },
        { "Copy Text", "&@copy:\"", "Copy text: &@copy:\"text\"", "\"&r" },
        { "Change Quest", "&@change:\"", "Change quest: &@change:\"text\"", "\"&r" },
        { "New Page", "&@page", "New page: &@page", "" }
    };

    std::vector<Button> hoverEffects = {
        { "Show Text Hover", "&&text:\"", "Needs to be after the text.\nShow text on hover: <some text to show tooltip>&&text:\"text\"", "\"&r" },
        { "Show Item Hover", "&&item:\"", "Needs to be after the text.\nShow item on hover: <some text to show tooltip>&&item:\"item\"", "\"&r" },
        //{"Shadow Text", "&&shadow:\"", "Needs to be after the text.\n Put a shadow on the text (Not working) (#AARRGGBB)", "&r"}
    };

    std::vector<Button> modEffects = {
        // row 1
        { "Typewriter", "<typewriter>", "Typewriter effect", "</typewriter>" },
        { "Bounce", "<bounce a=1.0 f=1.0 w=1.0>", "Vertical bounce effect", "</bounce>" },
        { "Fade", "<fade a=0.3 f=1.0 w=0.0>", "Fade effect", "</fade>" },
        { "Glitch", "<glitch f=1.0 j=0.015 b=0.003 s=0.08>", "Glitch effect", "</glitch>" },
        // row 2
        { "Gradient", "<grad from=#7FFFD4 to=#1E90FF hue=false f=0.0 sp=20.0 uni=false>", "Color gradient effect", "</grad>" },
        { "Neon", "<neon p=10 r=2 a=0.12>", "Neon effect", "</neon>" },
        { "Pendulum", "<pend f=1.0 a=30 r=0.0>", "Circular pendulum effect", "</pend>" },
        { "Pulse", "<pulse base=0.75 a=1.0 f=1.0 w=0.0>", "Brightness pulse effect", "</pulse>" },
        // row 3
        { "Rainbow", "<rainb f=1.0 w=1.0>", "Rainbow color effect", "</rainb>" },
        { "Shadow", "<shadow x=0.0 y=0.0 c=000000 a=1.0>", "Modify text shadow", "</shadow>" },
        { "Shake", "<shake a=1.0 f=1.0>", "Random shake effect", "</shake>" },
        { "Swing", "<swing a=1.0 f=1.0 w=0.0>", "Character swing effect", "</swing>" },
        // row 4
        { "Turbulence", "<turb a=1.0 f=1.0>", "Turbulence effect", "</turb>" },
        { "Wave", "<wave a=1.0 f=1.0 w=1.0>", "Wave-like undulation effect", "</wave>" },
        { "Wiggle", "<wiggle a=1.0 f=1.0 w=1.0>", "Per-character random movement", "</wiggle>" }
    };

    std::vector<Button> actionButtons = {
        { "Convert", "", "Convert text to JSON format and copy to clipboard" },
        { "Copy Text Output", "", "Copy converted text to clipboard" },
        { "Reload Minecraft (Not yet)", "", "Reload Minecraft scripts (requires KubeJS)" },
        { "Settings", "", "Open appearance settings" },
        { "Exit", "", "Close the application" }
    };

    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    ImVec2 center = ImVec2(win_w / 2.0f, win_h / 2.0f);
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    {
        KubeJSImageBrowser browser;
        bool browserFirstRun = true;
        while(!glfwWindowShouldClose(window))
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

            for(size_t i = 0; i < basicFormats.size(); ++i)
            {
                auto& data = basicFormats[i];

                generateSlowedButton(field, data);

                if(i < basicFormats.size() - 1)
                {
                    ImGui::SameLine();
                }
            }
            ImGui::NewLine();

            ImGui::Text("Special Actions:");
            for(size_t i = 0; i < specialActions.size(); ++i)
            {
                auto& data = specialActions[i];

                generateSlowedButton(field, data);

                if(i < specialActions.size() - 1)
                {
                    ImGui::SameLine();
                }
            }
            ImGui::NewLine();

            ImGui::Text("Hover Effects:");
            for(size_t i = 0; i < hoverEffects.size(); ++i)
            {
                auto& data = hoverEffects[i];

                generateSlowedButton(field, data);

                if(i < hoverEffects.size() - 1)
                {
                    ImGui::SameLine();
                }
            }
            ImGui::NewLine();

            ImGui::Text("Mod Effects:");
            for(size_t i = 0; i < modEffects.size(); i++)
            {
                auto& data = modEffects[i];

                generateSlowedButton(field, data);

                if((i + 1) % 5 != 0 && i < modEffects.size() - 1)
                {
                    ImGui::SameLine();
                }
                else if(i < modEffects.size() - 1)
                {
                    ImGui::NewLine();
                }
            }
            ImGui::NewLine();
            ImGui::Separator();

            // Show selection information (debug)
            if(field.hasSelection)
            {
                ImGui::Text("Selection: '%s' (%d-%d)",
                    field.getSelected().c_str(),
                    field.selectionStart,
                    field.selectionEnd);
            }
            else
            {
                ImGui::Text("No selection");
            }

            // First textfield with callback to capture selection
            ImVec2 size(width, ImGui::GetTextLineHeight() * 8);
            ImGui::InputTextMultiline("##input1", &field.text, size,
                ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_WordWrap,
                InputTextCallback, &field);

            // Second textfield
            ImGui::InputTextMultiline("##input2", &inputText2, size,
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);

            // Radio buttons (only one selected)
            ImGui::Text("Options:");
            ImGui::RadioButton("Array", &selected_option, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Extra", &selected_option, 1);
            ImGui::SameLine();

            // Action buttons
            size_t action_idx = 0;
            generateSlowedButton(actionButtons[action_idx++], [&]() {
                inputText2 = raw::to_json(field.text, selected_option ? true : false);
                ImGui::SetClipboardText(inputText2.c_str());
            });
            ImGui::SameLine();

            generateSlowedButton(actionButtons[action_idx++], [&]() {
                ImGui::SetClipboardText(inputText2.c_str());
            });
            ImGui::SameLine();

            generateSlowedButton(actionButtons[action_idx++], [&]() {
                if(!client.connect()) inputText2 = "Couldn't connect to server";
                if(!client.sendReloadCommand(ReloadType::SERVER)) inputText2 = "Error executing reload";
                inputText2 = "Reload successful!";
            });
            ImGui::SameLine();

            generateSlowedButton(actionButtons[action_idx++], [&]() {
                settings::show_menu = !settings::show_menu;
            });
            ImGui::SameLine();

            generateSlowedButton(actionButtons[action_idx++], [&]() {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            });

            ImGui::NewLine();
            ImGui::Separator();

            settings::draw_menu();
            ImGui::SameLine();

            ImGui::End();

            createKubejsImageBrowser(browser, browserFirstRun, dt, window);
            createColorWheel(field);

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