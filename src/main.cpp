#include "args/generate.h"
#include "args/merger.h"
#include "args/splitter.h"
#include "args/pack.h"
#include "gui/display/settings.h"
#include "gui/elements/button.h"
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
#include <tinyfiledialogs/tinyfiledialogs.h>

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
        io.Fonts->AddFontFromFileTTF(settings::current.font_path.c_str(), settings::current.font_size);
    }
    else
    {
        ImFontConfig config;
        config.SizePixels = settings::current.font_size;
        io.Fonts->AddFontDefault(&config);
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
        { "Rainbows", "&z", "Gradient using the rainbow (1.21.1+ only) (&z)", "&r" }
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
        { "Reload Minecraft (1.21.1+)", "", "Reload Minecraft scripts (requires KubeJS)" },
        { "Settings", "", "Open appearance settings" },
        { "Exit", "", "Close the application" }
    };

    std::vector<Button> argsButtons = {
        { "Deconstruct", "", "Breaks down large .lang files into modular chunks.\nIdeal for localized editing and clean Git version control." },
        { "Rebuild", "", "Reassembles modular chunks back into a single .lang file.\nSynchronizes all your translations into the final game format." },
        { "Setup Project", "", "Initializes a fresh workspace with folders and base scripts.\nRun this when starting a new modpack development." },
        { "Ship Modpack", "", "Compiles the project into final distribution ZIPs.\nCreates separate Client and Server packages for your players." },
        { "Set Client Root", "", "Points to your Minecraft instance folder.\nUsed to extract mod metadata for the server-side generation." },
        { "Set Project Path", "", "Defines the working directory of your quest project.\nWhere the 'config/ftbquests' folder is located." }
    };

    int win_w, win_h;
    glfwGetWindowSize(window, &win_w, &win_h);
    ImVec2 center = ImVec2(win_w / 2.0f, win_h / 2.0f);
    ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    {
        KubeJSImageBrowser browser;
        bool browserFirstRun = true;

        bool is_new = true;

        bool convert = false;
        bool json = false;
        bool cf = false;
        std::string mod_path = "";
        std::string path = "";

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

            int drawn_basic = 0;
            for(size_t i = 0; i < basicFormats.size(); ++i)
            {
                auto& data = basicFormats[i];

                if(!is_new && data.label == "Rainbows")
                {
                    continue;
                }

                if(drawn_basic > 0)
                {
                    ImGui::SameLine();
                }

                generateSlowedButton(field, data);
                drawn_basic++;
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

            for(size_t i = 0; i < modEffects.size(); i++)
            {
                Button data = modEffects[i];

                if(!is_new)
                {
                    size_t space_pos = data.prefix.find(' ');

                    if(space_pos != std::string::npos && data.prefix.front() == '<')
                    {
                        data.prefix = data.prefix.substr(0, space_pos) + ">";
                    }
                }

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
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Uses an array of JSON objects, it can be more readable (1.21.1+)");
            }
            ImGui::SameLine();
            ImGui::RadioButton("Extra", &selected_option, 1);
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Uses only an JSON object with extra field (1.19.2+)");
            }
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
            ImGui::Text("Global Settings:");

            ImGui::Checkbox("Input Format", &json);
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Target Output Format (--json):\n\n"
                    "- Enabled: Outputs generated files as .json. Ideal for translation websites and similar\n"
                    "- Disabled: Outputs generated files as .snbt .\n\n"
                    "Note: FTBQ only reads the final assembled file if it is in .snbt.");
            }

            ImGui::SameLine();
            ImGui::Checkbox("Output Format", &convert);
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Target Input Format (--convert ):\n"
                    "- If splitting: It will look for .json in the lang folder if enabled, or .snbt if disabled.\n"
                    "- If merging: It will look for chunks in .json to convert to .snbt.\n"
                    "Note: FTBQ only reads the final assembled file if it's a .snbt.");
            }

            ImGui::SameLine();
            ImGui::Checkbox("Manifest", &cf);
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Use manifest.json instead of mods.json (Prism): \n"
                                  "- manifest.json comes from the CF launcher\n"
                                  "- mods.json comes from the Prism Launcher using the export command on JSON\n"
                                  "Note: manifest.json has the exact file you download, leading to better results");
            }
            ImGui::SameLine();

            ImGui::Checkbox("1.21.1+ format", &is_new);
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Toggles more effects details (1.21.1+).\n"
                                  "- Enabled: Shows '&z' and uses detailed modEffects (e.g., <pulse base=...>).\n"
                                  "- Disabled: Hides '&z' and uses simple tags (e.g., <pulse>).");
            }

            ImGui::Spacing();

            generateSlowedButton(argsButtons[5], [&]() {
                const char* selected = tinyfd_selectFolderDialog("Select Working Directory", path.c_str());
                if(selected) path = selected;
            });
            ImGui::SameLine();
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled],
                " [%s]", path.empty() ? "Default: Current" : path.c_str());

            generateSlowedButton(argsButtons[4], [&]() {
                const char* selected = tinyfd_selectFolderDialog("Select Minecraft Instance Folder", mod_path.c_str());
                if(selected) mod_path = selected;
            });
            ImGui::SameLine();
            ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled],
                " [%s]", mod_path.empty() ? "Not set" : mod_path.c_str());

            ImGui::Spacing();

            ImGui::Separator();
            ImGui::Text("Execute Operations:");

            generateSlowedButton(argsButtons[0], [&]() {
                args::parser temp_args;
                if(json) temp_args.result["--json"] = {};
                if(convert) temp_args.result["--convert"] = {};
                if(!path.empty()) temp_args.result["--path"] = { path };
                splitter::split(temp_args);
            });
            ImGui::SameLine();

            generateSlowedButton(argsButtons[1], [&]() {
                args::parser temp_args;
                if(json) temp_args.result["--json"] = {};
                if(convert) temp_args.result["--convert"] = {};
                if(!path.empty()) temp_args.result["--path"] = { path };
                merger::merge(temp_args);
            });
            ImGui::SameLine();

            generateSlowedButton(argsButtons[2], [&]() {
                args::parser temp_args;
                if(cf) temp_args.result["--curseforge"] = {};
                if(!path.empty()) temp_args.result["--path"] = { path };
                gen::generate(temp_args);
            });
            ImGui::SameLine();

            generateSlowedButton(argsButtons[3], [&]() {
                args::parser temp_args;
                if(cf) temp_args.result["--curseforge"] = {};
                if(!path.empty()) temp_args.result["--path"] = { path };
                if(!mod_path.empty()) temp_args.result["--mods"] = { mod_path };
                pack::pack(temp_args);
            });

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