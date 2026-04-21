#include "gui/display/settings.h"
#include "gui/elements/menu.h"
#include <fstream>
#include <cfloat>
#include <filesystem>
#include <imgui.h>
#include <system_error>
#include <algorithm>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace settings
{

    struct settings current;
    struct settings saved;
    bool show_menu = false;
    bool was_menu_open = false;
    std::vector<std::string> cached_fonts;

    std::vector<std::string> get_system_fonts()
    {
        std::vector<std::string> fonts;
        std::vector<std::string> paths;

#ifdef _WIN32
        paths.push_back("C:\\Windows\\Fonts");
#else
        paths.push_back("/usr/share/fonts");
        paths.push_back("/usr/local/share/fonts");
        const char* home = getenv("HOME");
        if(home) paths.push_back(std::string(home) + "/.local/share/fonts");
#endif

        for(const auto& path : paths)
        {
            std::error_code ec;
            if(fs::exists(path, ec))
            {
                auto options = fs::directory_options::skip_permission_denied;
                for(auto it = fs::recursive_directory_iterator(path, options, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
                {
                    if(ec) continue;
                    if(it->is_regular_file())
                    {
                        std::string ext = it->path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if(ext == ".ttf")
                        {
                            fonts.push_back(it->path().string());
                        }
                    }
                }
            }
        }
        return fonts;
    }

    void apply_preset(struct settings& s, int preset_id)
    {
        ImVec4 accent, darkAcc, bg, btn, text;

        switch(preset_id)
        {
        case 0:
            accent = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
            darkAcc = ImVec4(0.00f, 0.30f, 0.50f, 1.00f);
            bg = ImVec4(0.02f, 0.02f, 0.04f, 0.95f);
            btn = ImVec4(0.05f, 0.10f, 0.15f, 1.00f);
            text = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
            break;
        case 1:
            accent = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            darkAcc = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
            bg = ImVec4(0.90f, 0.90f, 0.90f, 0.94f);
            btn = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
            text = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            break;
        case 2:
            accent = ImVec4(1.00f, 0.13f, 0.61f, 1.00f);
            darkAcc = ImVec4(0.61f, 0.05f, 0.36f, 1.00f);
            bg = ImVec4(0.05f, 0.01f, 0.05f, 0.95f);
            btn = ImVec4(0.30f, 0.04f, 0.20f, 1.00f);
            text = ImVec4(0.00f, 1.00f, 0.90f, 1.00f);
            break;
        case 3:
            accent = ImVec4(0.50f, 0.80f, 0.50f, 1.00f);
            darkAcc = ImVec4(0.20f, 0.50f, 0.20f, 1.00f);
            bg = ImVec4(0.05f, 0.10f, 0.05f, 0.94f);
            btn = ImVec4(0.10f, 0.30f, 0.10f, 1.00f);
            text = ImVec4(0.80f, 0.95f, 0.80f, 1.00f);
            break;
        case 4:
            accent = ImVec4(1.00f, 0.40f, 0.20f, 1.00f);
            darkAcc = ImVec4(0.70f, 0.20f, 0.10f, 1.00f);
            bg = ImVec4(0.10f, 0.05f, 0.05f, 0.94f);
            btn = ImVec4(0.30f, 0.10f, 0.05f, 1.00f);
            text = ImVec4(0.95f, 0.80f, 0.80f, 1.00f);
            break;
        case 5:
            accent = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
            darkAcc = ImVec4(0.00f, 0.40f, 0.60f, 1.00f);
            bg = ImVec4(0.02f, 0.05f, 0.10f, 0.95f);
            btn = ImVec4(0.05f, 0.15f, 0.25f, 1.00f);
            text = ImVec4(0.80f, 0.95f, 1.00f, 1.00f);
            break;
        case 6:
            accent = ImVec4(0.00f, 1.00f, 0.00f, 1.00f);
            darkAcc = ImVec4(0.00f, 0.40f, 0.00f, 1.00f);
            bg = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
            btn = ImVec4(0.00f, 0.20f, 0.00f, 1.00f);
            text = ImVec4(0.00f, 0.80f, 0.00f, 1.00f);
            break;
        case 7:
            accent = ImVec4(1.00f, 0.00f, 0.60f, 1.00f);
            darkAcc = ImVec4(0.50f, 0.00f, 0.40f, 1.00f);
            bg = ImVec4(0.10f, 0.05f, 0.15f, 0.95f);
            btn = ImVec4(0.30f, 0.10f, 0.40f, 1.00f);
            text = ImVec4(1.00f, 0.80f, 0.20f, 1.00f);
            break;
        case 8:
            accent = ImVec4(0.15f, 0.55f, 0.82f, 1.00f);
            darkAcc = ImVec4(0.10f, 0.40f, 0.60f, 1.00f);
            bg = ImVec4(0.00f, 0.17f, 0.21f, 0.95f);
            btn = ImVec4(0.03f, 0.21f, 0.26f, 1.00f);
            text = ImVec4(0.51f, 0.58f, 0.59f, 1.00f);
            break;
        case 9:
            accent = ImVec4(0.80f, 0.55f, 0.40f, 1.00f);
            darkAcc = ImVec4(0.60f, 0.40f, 0.30f, 1.00f);
            bg = ImVec4(0.15f, 0.12f, 0.10f, 0.95f);
            btn = ImVec4(0.30f, 0.20f, 0.15f, 1.00f);
            text = ImVec4(0.95f, 0.90f, 0.85f, 1.00f);
            break;
        }

        auto set_color = [&s](int idx, ImVec4 c) {
            s.colors[idx][0] = c.x;
            s.colors[idx][1] = c.y;
            s.colors[idx][2] = c.z;
            s.colors[idx][3] = c.w;
        };

        set_color(ImGuiCol_WindowBg, bg);
        set_color(ImGuiCol_Border, accent);
        set_color(ImGuiCol_Text, text);
        set_color(ImGuiCol_Button, btn);
        set_color(ImGuiCol_ButtonHovered, darkAcc);
        set_color(ImGuiCol_ButtonActive, accent);
        set_color(ImGuiCol_FrameBg, btn);
        set_color(ImGuiCol_FrameBgHovered, darkAcc);
        set_color(ImGuiCol_FrameBgActive, accent);
        set_color(ImGuiCol_TitleBg, darkAcc);
        set_color(ImGuiCol_TitleBgActive, accent);
        set_color(ImGuiCol_Separator, accent);
        set_color(ImGuiCol_CheckMark, accent);
        set_color(ImGuiCol_SliderGrab, accent);
        set_color(ImGuiCol_SliderGrabActive, text);
        set_color(ImGuiCol_TextSelectedBg, darkAcc);

        if(preset_id == 0)
        {
            set_color(ImGuiCol_TitleBgActive, ImVec4(0.00f, 0.40f, 0.70f, 1.00f));
            set_color(ImGuiCol_SliderGrabActive, ImVec4(0.50f, 0.95f, 1.00f, 1.00f));
            set_color(ImGuiCol_TextSelectedBg, ImVec4(0.00f, 0.80f, 1.00f, 0.40f));

            s.window_rounding = 0.0f;
            s.child_rounding = 0.0f;
            s.frame_rounding = 0.0f;
            s.popup_rounding = 0.0f;
            s.scrollbar_rounding = 0.0f;
            s.grab_rounding = 0.0f;
            s.tab_rounding = 0.0f;

            s.window_border_size = 1.0f;
            s.child_border_size = 1.0f;
            s.popup_border_size = 1.0f;
            s.frame_border_size = 1.0f;
            s.tab_border_size = 1.0f;
        }
    }

    void reset_to_defaults(struct settings& s)
    {
        ImGuiStyle default_style;
        for(int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            s.colors[i][0] = default_style.Colors[i].x;
            s.colors[i][1] = default_style.Colors[i].y;
            s.colors[i][2] = default_style.Colors[i].z;
            s.colors[i][3] = default_style.Colors[i].w;
        }

        apply_preset(s, 0);

        s.font_scale = 1.0f;
        s.font_path = "";
        s.alpha = 1.0f;
        s.disabled_alpha = 0.60f;

        s.window_min_size[0] = 32.0f;
        s.window_min_size[1] = 32.0f;
        s.scrollbar_size = 14.0f;
        s.grab_min_size = 12.0f;

        s.window_padding[0] = 8.0f;
        s.window_padding[1] = 8.0f;
        s.frame_padding[0] = 4.0f;
        s.frame_padding[1] = 3.0f;
        s.item_spacing[0] = 8.0f;
        s.item_spacing[1] = 4.0f;
        s.item_inner_spacing[0] = 4.0f;
        s.item_inner_spacing[1] = 4.0f;
        s.cell_padding[0] = 4.0f;
        s.cell_padding[1] = 2.0f;
        s.display_window_padding[0] = 19.0f;
        s.display_window_padding[1] = 19.0f;
        s.display_safe_area_padding[0] = 3.0f;
        s.display_safe_area_padding[1] = 3.0f;

        s.window_title_align[0] = 0.0f;
        s.window_title_align[1] = 0.5f;
        s.button_text_align[0] = 0.5f;
        s.button_text_align[1] = 0.5f;
        s.selectable_text_align[0] = 0.0f;
        s.selectable_text_align[1] = 0.0f;
    }

    void apply_to_imgui(const struct settings& s)
    {
        ImGuiStyle& style = ImGui::GetStyle();

        for(int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            style.Colors[i] = ImVec4(s.colors[i][0], s.colors[i][1], s.colors[i][2], s.colors[i][3]);
        }

        style.Alpha = s.alpha;
        style.DisabledAlpha = s.disabled_alpha;

        style.WindowRounding = s.window_rounding;
        style.ChildRounding = s.child_rounding;
        style.FrameRounding = s.frame_rounding;
        style.PopupRounding = s.popup_rounding;
        style.ScrollbarRounding = s.scrollbar_rounding;
        style.GrabRounding = s.grab_rounding;
        style.TabRounding = s.tab_rounding;

        style.WindowBorderSize = s.window_border_size;
        style.ChildBorderSize = s.child_border_size;
        style.PopupBorderSize = s.popup_border_size;
        style.FrameBorderSize = s.frame_border_size;
        style.TabBorderSize = s.tab_border_size;

        style.WindowMinSize = ImVec2(s.window_min_size[0], s.window_min_size[1]);
        style.ScrollbarSize = s.scrollbar_size;
        style.GrabMinSize = s.grab_min_size;

        style.WindowPadding = ImVec2(s.window_padding[0], s.window_padding[1]);
        style.FramePadding = ImVec2(s.frame_padding[0], s.frame_padding[1]);
        style.ItemSpacing = ImVec2(s.item_spacing[0], s.item_spacing[1]);
        style.ItemInnerSpacing = ImVec2(s.item_inner_spacing[0], s.item_inner_spacing[1]);
        style.CellPadding = ImVec2(s.cell_padding[0], s.cell_padding[1]);
        style.DisplayWindowPadding = ImVec2(s.display_window_padding[0], s.display_window_padding[1]);
        style.DisplaySafeAreaPadding = ImVec2(s.display_safe_area_padding[0], s.display_safe_area_padding[1]);

        style.WindowTitleAlign = ImVec2(s.window_title_align[0], s.window_title_align[1]);
        style.ButtonTextAlign = ImVec2(s.button_text_align[0], s.button_text_align[1]);
        style.SelectableTextAlign = ImVec2(s.selectable_text_align[0], s.selectable_text_align[1]);

        ImGui::GetIO().FontGlobalScale = s.font_scale;
    }

    void load()
    {
        reset_to_defaults(saved);

        std::ifstream file("settings.json");
        if(!file.is_open())
        {
            current = saved;
            save();
            return;
        }

        try
        {
            json j;
            file >> j;

            if(j.contains("colors"))
            {
                auto jcolors = j["colors"];
                for(size_t i = 0; i < (size_t)ImGuiCol_COUNT && i < jcolors.size(); ++i)
                {
                    for(int c = 0; c < 4; ++c) saved.colors[i][c] = jcolors[i][c];
                }
            }

            if(j.contains("font_scale")) saved.font_scale = j["font_scale"];
            if(j.contains("font_path")) saved.font_path = j["font_path"];
            if(j.contains("alpha")) saved.alpha = j["alpha"];
            if(j.contains("disabled_alpha")) saved.disabled_alpha = j["disabled_alpha"];

            if(j.contains("window_rounding")) saved.window_rounding = j["window_rounding"];
            if(j.contains("child_rounding")) saved.child_rounding = j["child_rounding"];
            if(j.contains("frame_rounding")) saved.frame_rounding = j["frame_rounding"];
            if(j.contains("popup_rounding")) saved.popup_rounding = j["popup_rounding"];
            if(j.contains("scrollbar_rounding")) saved.scrollbar_rounding = j["scrollbar_rounding"];
            if(j.contains("grab_rounding")) saved.grab_rounding = j["grab_rounding"];
            if(j.contains("tab_rounding")) saved.tab_rounding = j["tab_rounding"];

            if(j.contains("window_border_size")) saved.window_border_size = j["window_border_size"];
            if(j.contains("child_border_size")) saved.child_border_size = j["child_border_size"];
            if(j.contains("popup_border_size")) saved.popup_border_size = j["popup_border_size"];
            if(j.contains("frame_border_size")) saved.frame_border_size = j["frame_border_size"];
            if(j.contains("tab_border_size")) saved.tab_border_size = j["tab_border_size"];

            if(j.contains("scrollbar_size")) saved.scrollbar_size = j["scrollbar_size"];
            if(j.contains("grab_min_size")) saved.grab_min_size = j["grab_min_size"];
            if(j.contains("font_size")) saved.font_size = j["font_size"];

            auto load_vec2 = [&](const char* key, float arr[2]) {
                if(j.contains(key))
                {
                    arr[0] = j[key][0];
                    arr[1] = j[key][1];
                }
            };

            load_vec2("window_min_size", saved.window_min_size);
            load_vec2("window_padding", saved.window_padding);
            load_vec2("frame_padding", saved.frame_padding);
            load_vec2("item_spacing", saved.item_spacing);
            load_vec2("item_inner_spacing", saved.item_inner_spacing);
            load_vec2("cell_padding", saved.cell_padding);
            load_vec2("display_window_padding", saved.display_window_padding);
            load_vec2("display_safe_area_padding", saved.display_safe_area_padding);
            load_vec2("window_title_align", saved.window_title_align);
            load_vec2("button_text_align", saved.button_text_align);
            load_vec2("selectable_text_align", saved.selectable_text_align);
        }
        catch(const json::exception& e)
        {
        }

        current = saved;
    }

    void save()
    {
        json j;

        j["colors"] = json::array();
        for(int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            j["colors"].push_back({ saved.colors[i][0], saved.colors[i][1], saved.colors[i][2], saved.colors[i][3] });
        }

        j["font_scale"] = saved.font_scale;
        j["font_path"] = saved.font_path;
        j["alpha"] = saved.alpha;
        j["disabled_alpha"] = saved.disabled_alpha;

        j["window_rounding"] = saved.window_rounding;
        j["child_rounding"] = saved.child_rounding;
        j["frame_rounding"] = saved.frame_rounding;
        j["popup_rounding"] = saved.popup_rounding;
        j["scrollbar_rounding"] = saved.scrollbar_rounding;
        j["grab_rounding"] = saved.grab_rounding;
        j["tab_rounding"] = saved.tab_rounding;

        j["window_border_size"] = saved.window_border_size;
        j["child_border_size"] = saved.child_border_size;
        j["popup_border_size"] = saved.popup_border_size;
        j["frame_border_size"] = saved.frame_border_size;
        j["tab_border_size"] = saved.tab_border_size;

        j["scrollbar_size"] = saved.scrollbar_size;
        j["grab_min_size"] = saved.grab_min_size;

        j["window_min_size"] = { saved.window_min_size[0], saved.window_min_size[1] };
        j["window_padding"] = { saved.window_padding[0], saved.window_padding[1] };
        j["frame_padding"] = { saved.frame_padding[0], saved.frame_padding[1] };
        j["item_spacing"] = { saved.item_spacing[0], saved.item_spacing[1] };
        j["item_inner_spacing"] = { saved.item_inner_spacing[0], saved.item_inner_spacing[1] };
        j["cell_padding"] = { saved.cell_padding[0], saved.cell_padding[1] };
        j["display_window_padding"] = { saved.display_window_padding[0], saved.display_window_padding[1] };
        j["display_safe_area_padding"] = { saved.display_safe_area_padding[0], saved.display_safe_area_padding[1] };

        j["window_title_align"] = { saved.window_title_align[0], saved.window_title_align[1] };
        j["button_text_align"] = { saved.button_text_align[0], saved.button_text_align[1] };
        j["selectable_text_align"] = { saved.selectable_text_align[0], saved.selectable_text_align[1] };
        j["font_size"] = saved.font_size;

        std::ofstream file("settings.json");
        if(file.is_open())
        {
            file << j.dump(4);
        }
    }

    void draw_menu()
    {
        if(show_menu && !was_menu_open)
        {
            current = saved;
        }
        if(!show_menu && was_menu_open)
        {
            current = saved;
            apply_to_imgui(saved);
        }
        was_menu_open = show_menu;

        if(!show_menu) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(550, 450), ImVec2(900, 900));
        ImGui::Begin("Settings", &show_menu);

        ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_NoCloseWithMiddleMouseButton | ImGuiTabBarFlags_FittingPolicyScroll;

        if(ImGui::BeginTabBar("SettingsTabs", tab_flags))
        {

            if(ImGui::BeginTabItem("Presets"))
            {
                ImGui::Separator();

                ImGui::BeginChild("PresetsScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);

                if(ImGui::Button("Retro", ImVec2(-1, 30))) apply_preset(current, 0);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Classic Light", ImVec2(-1, 30))) apply_preset(current, 1);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Cyberpunk Neon", ImVec2(-1, 30))) apply_preset(current, 2);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Deep Forest", ImVec2(-1, 30))) apply_preset(current, 3);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Volcanic Red", ImVec2(-1, 30))) apply_preset(current, 4);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Oceanic Aqua", ImVec2(-1, 30))) apply_preset(current, 5);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Hacker Terminal", ImVec2(-1, 30))) apply_preset(current, 6);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Synthwave Retro", ImVec2(-1, 30))) apply_preset(current, 7);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Solarized Dark", ImVec2(-1, 30))) apply_preset(current, 8);
                ImGui::Dummy(ImVec2(0, 5));
                if(ImGui::Button("Warm Coffee", ImVec2(-1, 30))) apply_preset(current, 9);

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Colors"))
            {
                ImGui::Separator();

                ImGui::BeginChild("ColorScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);
                for(int i = 0; i < ImGuiCol_COUNT; ++i)
                {
                    ImGui::PushID(i);
                    ImGui::ColorEdit4(ImGui::GetStyleColorName(i), current.colors[i]);
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Geometry & Borders"))
            {
                ImGui::Separator();
                ImGui::SliderFloat("Window Rounding", &current.window_rounding, 0.0f, 20.0f);
                ImGui::SliderFloat("Child Rounding", &current.child_rounding, 0.0f, 20.0f);
                ImGui::SliderFloat("Frame Rounding", &current.frame_rounding, 0.0f, 20.0f);
                ImGui::SliderFloat("Popup Rounding", &current.popup_rounding, 0.0f, 20.0f);
                ImGui::SliderFloat("Scrollbar Rounding", &current.scrollbar_rounding, 0.0f, 20.0f);
                ImGui::SliderFloat("Grab Rounding", &current.grab_rounding, 0.0f, 20.0f);
                ImGui::SliderFloat("Tab Rounding", &current.tab_rounding, 0.0f, 20.0f);

                ImGui::Dummy(ImVec2(0, 10));

                ImGui::Separator();
                ImGui::SliderFloat("Window Border Size", &current.window_border_size, 0.0f, 5.0f);
                ImGui::SliderFloat("Child Border Size", &current.child_border_size, 0.0f, 5.0f);
                ImGui::SliderFloat("Popup Border Size", &current.popup_border_size, 0.0f, 5.0f);
                ImGui::SliderFloat("Frame Border Size", &current.frame_border_size, 0.0f, 5.0f);
                ImGui::SliderFloat("Tab Border Size", &current.tab_border_size, 0.0f, 5.0f);

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Spacings & Sizes"))
            {
                ImGui::Separator();
                ImGui::SliderFloat2("Window Min Size", current.window_min_size, 10.0f, 200.0f);
                ImGui::SliderFloat("Scrollbar Size", &current.scrollbar_size, 1.0f, 30.0f);
                ImGui::SliderFloat("Grab Min Size", &current.grab_min_size, 1.0f, 30.0f);

                ImGui::Dummy(ImVec2(0, 10));

                ImGui::Separator();
                ImGui::SliderFloat2("Window Padding", current.window_padding, 0.0f, 20.0f);
                ImGui::SliderFloat2("Frame Padding", current.frame_padding, 0.0f, 20.0f);
                ImGui::SliderFloat2("Item Spacing", current.item_spacing, 0.0f, 20.0f);
                ImGui::SliderFloat2("Item Inner Spacing", current.item_inner_spacing, 0.0f, 20.0f);
                ImGui::SliderFloat2("Cell Padding", current.cell_padding, 0.0f, 20.0f);
                ImGui::SliderFloat2("Display Window Padding", current.display_window_padding, 0.0f, 30.0f);
                ImGui::SliderFloat2("Display Safe Area Padding", current.display_safe_area_padding, 0.0f, 30.0f);

                ImGui::Dummy(ImVec2(0, 10));

                ImGui::Separator();
                ImGui::SliderFloat2("Window Title Align", current.window_title_align, 0.0f, 1.0f);
                ImGui::SliderFloat2("Button Text Align", current.button_text_align, 0.0f, 1.0f);
                ImGui::SliderFloat2("Selectable Text Align", current.selectable_text_align, 0.0f, 1.0f);

                ImGui::EndTabItem();
            }

            if(ImGui::BeginTabItem("Typography & Alpha"))
            {

                ImGui::Separator();
                ImGui::SliderFloat("Global Alpha", &current.alpha, 0.2f, 1.0f);
                ImGui::SliderFloat("Disabled Alpha", &current.disabled_alpha, 0.0f, 1.0f);

                ImGui::Dummy(ImVec2(0, 10));

                ImGui::Separator();
                ImGui::SliderFloat("Global Zoom", &current.font_scale, 0.5f, 4.0f);

                ImGui::SliderFloat("Font Size", &current.font_size, 10.0f, 48.0f, "%.1f px");
                
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Custom font resolution. Requires restarting the app.");
                }

                if(cached_fonts.empty())
                {
                    cached_fonts = get_system_fonts();
                }

                draw_search_bar("Select Font", cached_fonts, current.font_path, [&](std::string) {
                });

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        apply_to_imgui(current);

        ImGui::Separator();

        if(ImGui::Button("Save Configuration"))
        {
            saved = current;
            save();
        }

        ImGui::SameLine();
        if(ImGui::Button("Close"))
        {
            show_menu = false;
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20.0f, 0.0f));
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if(ImGui::Button("Reset to Defaults"))
        {
            reset_to_defaults(current);
        }
        ImGui::PopStyleColor(2);

        ImGui::End();
    }
} // namespace settings