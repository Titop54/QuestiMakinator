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
        { "Bold", "Bold text (&l)", "&l", "&r" },
        { "Italic", "Italic text (&o)", "&o", "&r" },
        { "Underline", "Underlined text (&n)", "&n", "&r" },
        { "Strikethrough", "Strikethrough text (&m)", "&m", "&r" },
        { "Reset", "Reset formatting (&r)", "&r" },
        { "Obfuscated", "Obfuscated text (&k)", "&k", "&r" },
        { "Rainbows", "Gradient using the rainbow (1.21.1+ only) (&z)", "&z", "&r" }
    };

    std::vector<Button> specialActions = {
        { "Insert URL", "Insert URL: &@url:\"url\"", "&@url:\"", "\"&r" },
        { "Insert Text", "Insert chat text: &@in:\"text\"", "&@in:\"", "\"&r" },
        { "Open File", "Open file: &@file:\"path\"", "&@file:\"", "\"&r" },
        { "Run Command", "Execute command: &@command:\"/<command>\"", "&@command:\"", "\"&r" },
        { "Copy Text", "Copy text: &@copy:\"text\"", "&@copy:\"", "\"&r" },
        { "Change Quest", "Change quest: &@change:\"text\"", "&@change:\"", "\"&r" },
        { "New Page", "New page: &@page", "&@page", "" }
    };

    std::vector<Button> hoverEffects = {
        { "Show Text Hover", "Needs to be after the text.\nShow text on hover: <some text to show tooltip>&&text:\"text\"", "&&text:\"", "\"&r" },
        { "Show Item Hover", "Needs to be after the text.\nShow item on hover: <some text to show tooltip>&&item:\"item\"", "&&item:\"", "\"&r" },
        { "Shadow Text", "&&shadow:\"", "Needs to be after the text.\n Put a shadow on the text (1.21.4) (#AARRGGBB)", "&r" }
    };

    std::vector<Button> modEffects = {
        // row 1
        { "Typewriter",
            "Simulates real-time typing by rendering characters one by one.\n\n"
            "Usage:\n- Must be placed at the very beginning of the text.\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<typewriter>", "</typewriter>" },

        { "Bounce",
            "Makes characters bounce vertically.\n\n"
            "Parameters:\n"
            "- a: Amplitude (Height of the bounce)\n"
            "- f: Frequency (Speed of movement)\n"
            "- w: Wave Size (Distance between character peaks)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<bounce a=1.0 f=1.0 w=1.0>", "</bounce>" },

        { "Fade",
            "Periodically changes text opacity (breathing effect).\n\n"
            "Parameters:\n"
            "- a: Minimum Opacity (Lower bound)\n"
            "- f: Frequency (Speed of the fade)\n"
            "- w: Wave Size (Offset between characters)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<fade a=0.3 f=1.0 w=0.0>", "</fade>" },

        { "Glitch",
            "Creates slicing, flickering, and random jitter layers.\n\n"
            "Parameters:\n"
            "- f: Frequency (Speed of glitching)\n"
            "- j: Jitter Chance (Random character displacement)\n"
            "- b: Blink Chance (Random visibility flicker)\n"
            "- s: Slicing Chance (Horizontal layer shifting)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<glitch f=1.0 j=0.015 b=0.003 s=0.08>", "</glitch>" },

        // row 2
        { "Gradient",
            "Smooth color transition in RGB or HSV space.\n\n"
            "Parameters:\n"
            "- from/to: Start and End colors (Hex code)\n"
            "- hue: Use HSV interpolation (True/False)\n"
            "- f: Flow Speed (Movement speed)\n"
            "- sp: Span (Color distribution distance)\n"
            "- uni: Unidirectional flow (True/False)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<grad from=#7FFFD4 to=#1E90FF hue=false f=0.0 sp=20.0 uni=false>", "</grad>" },

        { "Neon",
            "Generates multiple glowing outlines around characters.\n\n"
            "Parameters:\n"
            "- p: Sampling Count (Quality of glow, min 4)\n"
            "- r: Radius (Thickness of the glow)\n"
            "- a: Opacity Multiplier (Glow intensity)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<neon p=10 r=2 a=0.12>", "</neon>" },

        { "Pendulum",
            "Characters swing back and forth from the top.\n\n"
            "Parameters:\n"
            "- f: Frequency (Swing speed)\n"
            "- a: Maximum Angle (Degrees of rotation)\n"
            "- r: Circular Radius (Optional circular path)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<pend f=1.0 a=30 r=0.0>", "</pend>" },

        { "Pulse",
            "Changes the overall brightness over time.\n\n"
            "Parameters:\n"
            "- base: Minimum Brightness multiplier\n"
            "- a: Amplitude (Intensity of the pulse)\n"
            "- f: Frequency (Speed of fluctuation)\n"
            "- w: Wave Size (Offset between characters)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<pulse base=0.75 a=1.0 f=1.0 w=0.0>", "</pulse>" },

        // row 3
        { "Rainbow",
            "Cycles through the full HSV color spectrum.\n\n"
            "Parameters:\n"
            "- f: Frequency (Color cycle speed)\n"
            "- w: Wave Size (Color distribution width)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<rainb f=1.0 w=1.0>", "</rainb>" },

        { "Shadow",
            "Customizes the text shadow properties.\n\n"
            "Parameters:\n"
            "- x/y: Offset Delta (Horizontal and vertical position)\n"
            "- c: Shadow Color (Hex code)\n"
            "- a: Opacity (Shadow transparency)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<shadow x=0.0 y=0.0 c=000000 a=1.0>", "</shadow>" },

        { "Shake",
            "High-energy random jitter in all directions.\n\n"
            "Parameters:\n"
            "- a: Amplitude (Strength of the shake)\n"
            "- f: Frequency (Speed of the jitter)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<shake a=1.0 f=1.0>", "</shake>" },

        { "Swing",
            "Characters spin back and forth on their axis.\n\n"
            "Parameters:\n"
            "- a: Amplitude (Rotation intensity)\n"
            "- f: Frequency (Spin speed)\n"
            "- w: Wave Size (Phase offset per character)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<swing a=1.0 f=1.0 w=0.0>", "</swing>" },

        // row 4
        { "Turbulence",
            "Applies noise-based random displacement.\n\n"
            "Parameters:\n"
            "- a: Amplitude (Strength of the wind effect)\n"
            "- f: Frequency (Complexity of the noise)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<turb a=1.0 f=1.0>", "</turb>" },

        { "Wave",
            "Smooth vertical undulation like ocean waves.\n\n"
            "Parameters:\n"
            "- a: Amplitude (Height of the wave)\n"
            "- f: Frequency (Speed of the wave)\n"
            "- w: Wave Size (Horizontal wave length)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<wave a=1.0 f=1.0 w=1.0>", "</wave>" },

        { "Wiggle",
            "Each character moves along its own random fixed path.\n\n"
            "Parameters:\n"
            "- a: Amplitude (Distance of movement)\n"
            "- f: Frequency (Speed of the wiggle)\n"
            "- w: Wave Size (Phase offset)\n\n"
            "Note: 1.21.1+ for the extra parameters",
            "<wiggle a=1.0 f=1.0 w=1.0>", "</wiggle>" }
    };

    std::vector<Button> actionButtons = {
        { "Convert", "Convert text to JSON format and copy to clipboard" },
        { "Copy Text Output", "Copy converted text to clipboard" },
        { "Reload Minecraft (1.21.1+)", "Reload Minecraft scripts (requires KubeJS)" },
        { "Settings", "Open appearance settings" },
        { "Exit", "Close the application" }
    };

    std::vector<Button> argsButtons = {
        { "Deconstruct", "Breaks down large .lang files into modular chunks.\nIdeal for localized editing and clean Git version control." },
        { "Rebuild", "Reassembles modular chunks back into a single .lang file.\nSynchronizes all your translations into the final game format." },
        { "Setup Project", "Initializes a fresh workspace with folders and base scripts.\nRun this when starting a new modpack development." },
        { "Ship Modpack", "Compiles the project into final distribution ZIPs.\nCreates separate Client and Server packages for your players." },
        { "Set Client Root", "Points to your Minecraft instance folder.\nUsed to extract mod metadata for the server-side generation." },
        { "Set Project Path", "Defines the working directory of your quest project.\nWhere the 'config/ftbquests' folder is located." }
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

            if(settings::current.textanimator) ImGui::Text("TextAnimator:");
            for(size_t i = 0; i < modEffects.size() && settings::current.textanimator; i++)
            {
                Button& data = modEffects[i];

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
            if(settings::current.show_packing)
            {
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
            }

            settings::draw_menu();
            ImGui::SameLine();

            ImGui::End();

            if(settings::current.show_image) createKubejsImageBrowser(browser, browserFirstRun, dt, window);
            if(settings::current.show_colors) createColorWheel(field);

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