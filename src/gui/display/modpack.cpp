#include "args/generate.h"
#include "args/merger.h"
#include "args/pack.h"
#include "args/splitter.h"
#include "args/ftbq_converter.h"
#include "gui/display/settings.h"
#include "gui/elements/button.h"
#include "parser/arguments.h"
#include <gui/display/modpack.h>
#include <imgui.h>
#include <thread>
#include <atomic>
#include <tinyfiledialogs.h>

// https://github.com/FTBTeam/FTB-Quests/blob/1.20.1/main/common/src/main/java/dev/ftb/mods/ftbquests/quest/Quest.java

// For 1.21.1 there is no need since you have the lang files

void createModpackMenu(std::vector<Button>& buttons)
{
    static std::string pendingPath;
    static std::atomic<bool> path_ready{ false };

    static std::string pendingModPath;
    static std::atomic<bool> mod_path_ready{ false };

    if(path_ready)
    {
        settings::modpack.path = pendingPath;
        path_ready = false;
    }
    if(mod_path_ready)
    {
        settings::modpack.mod_path = pendingModPath;
        mod_path_ready = false;
    }

    ImGui::Begin("Modpack Settings");

    ImGui::Text("Global Settings:");

    ImGui::Checkbox("Input Format", &settings::modpack.input_as_json);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Target Output Format (--json):\n\n"
            "- Enabled: Outputs generated files as .json. Ideal for translation websites and similar\n"
            "- Disabled: Outputs generated files as .snbt .\n\n"
            "Note: FTBQ only reads the final assembled file if it is in .snbt.");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Output Format", &settings::modpack.output_as_json);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Target Input Format (--convert ):\n"
            "- If splitting: It will look for .json in the lang folder if enabled, or .snbt if disabled.\n"
            "- If merging: It will look for chunks in .json to convert to .snbt.\n"
            "Note: FTBQ only reads the final assembled file if it's a .snbt.");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Manifest", &settings::modpack.using_cf);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Use manifest.json instead of mods.json (Prism): \n"
                          "- manifest.json comes from the CF launcher\n"
                          "- mods.json comes from the Prism Launcher using the export command on JSON\n"
                          "Note: manifest.json has the exact file you download, leading to better results\n"
                          "In custom instances, Prism Launcher uses PackWiz on a hidden folder inside mods folder (.index)");
    }
    ImGui::SameLine();

    ImGui::Spacing();

    generateSlowedButton(buttons[5], [&]() {
        if(path_ready) return;
        std::thread([]() {
            const char* selected = tinyfd_selectFolderDialog("Select Working Directory", settings::modpack.path.c_str());
            if(selected)
            {
                pendingPath = selected;
                path_ready = true;
            }
        }).detach();
    });
    ImGui::SameLine();
    ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled],
        " [%s]", settings::modpack.path.empty() ? "Default: Current" : settings::modpack.path.c_str());

    generateSlowedButton(buttons[4], [&]() {
        if(mod_path_ready) return;
        std::thread([]() {
            const char* selected = tinyfd_selectFolderDialog("Select Minecraft Instance Folder", settings::modpack.mod_path.c_str());
            if(selected)
            {
                pendingModPath = selected;
                mod_path_ready = true;
            }
        }).detach();
    });

    ImGui::SameLine();
    ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled],
        " [%s]", settings::modpack.mod_path.empty() ? "Not set" : settings::modpack.mod_path.c_str());

    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Text("Execute Operations:");

    generateSlowedButton(buttons[0], [&]() {
        args::parser temp_args;
        if(settings::modpack.input_as_json) temp_args.result["--json"] = {};
        if(settings::modpack.output_as_json) temp_args.result["--convert"] = {};
        if(!settings::modpack.path.empty()) temp_args.result["--path"] = { settings::modpack.path };
        splitter::split(temp_args);
    });
    ImGui::SameLine();

    generateSlowedButton(buttons[1], [&]() {
        args::parser temp_args;
        if(settings::modpack.input_as_json) temp_args.result["--json"] = {};
        if(settings::modpack.output_as_json) temp_args.result["--convert"] = {};
        if(!settings::modpack.path.empty()) temp_args.result["--path"] = { settings::modpack.path };
        merger::merge(temp_args);
    });
    ImGui::SameLine();

    generateSlowedButton(buttons[2], [&]() {
        std::thread([]() {
            args::parser temp_args;
            if(settings::modpack.using_cf) temp_args.result["--curseforge"] = {};
            if(!settings::modpack.path.empty()) temp_args.result["--path"] = { settings::modpack.path };
            gen::generate(temp_args);
        }).detach();
    });
    ImGui::SameLine();

    generateSlowedButton(buttons[3], [&]() {
        std::thread([]() {
            args::parser temp_args;
            if(settings::modpack.using_cf) temp_args.result["--curseforge"] = {};
            if(!settings::modpack.path.empty()) temp_args.result["--path"] = { settings::modpack.path };
            if(!settings::modpack.mod_path.empty()) temp_args.result["--mods"] = { settings::modpack.mod_path };
            pack::pack(temp_args);
        }).detach();
    });

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("FTB Quests Conversion:");

    if(settings::current.mc_version < settings::MC_1_21_1)
    {
        generateSlowedButton(buttons[6], [&]() {
            args::parser temp_args;
            if(!settings::modpack.path.empty()) temp_args.result["--path"] = { settings::modpack.path };
            ftbq_converter::convert_1_20_to_1_21(temp_args);
        });
        ImGui::SameLine();

        generateSlowedButton(buttons[7], [&]() {
            args::parser temp_args;
            if(!settings::modpack.path.empty()) temp_args.result["--path"] = { settings::modpack.path };
            ftbq_converter::extract_to_lang(temp_args);
        });
    }

    ImGui::End();
}