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
    struct modpack modpack;

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
        ImVec4 accent, darkAcc, bg, btn, text, tab_select, tab;

        switch(preset_id)
        {
        case 0:
            accent = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
            darkAcc = ImVec4(0.00f, 0.30f, 0.50f, 1.00f);
            bg = ImVec4(0.02f, 0.02f, 0.04f, 0.95f);
            btn = ImVec4(0.05f, 0.10f, 0.15f, 1.00f);
            text = ImVec4(0.00f, 0.80f, 1.00f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.00f, 0.20f, 0.30f, 1.00f);
            break;
        case 1:
            accent = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            darkAcc = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
            bg = ImVec4(0.90f, 0.90f, 0.90f, 0.94f);
            btn = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
            text = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
            break;
        case 2:
            accent = ImVec4(1.00f, 0.13f, 0.61f, 1.00f);
            darkAcc = ImVec4(0.61f, 0.05f, 0.36f, 1.00f);
            bg = ImVec4(0.05f, 0.01f, 0.05f, 0.95f);
            btn = ImVec4(0.30f, 0.04f, 0.20f, 1.00f);
            text = ImVec4(0.00f, 1.00f, 0.90f, 1.00f);
            tab_select = accent;
            tab = darkAcc;
            break;
        case 3:
            accent = ImVec4(0.50f, 0.80f, 0.50f, 1.00f);
            darkAcc = ImVec4(0.20f, 0.50f, 0.20f, 1.00f);
            bg = ImVec4(0.05f, 0.10f, 0.05f, 0.94f);
            btn = ImVec4(0.10f, 0.30f, 0.10f, 1.00f);
            text = ImVec4(0.80f, 0.95f, 0.80f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.15f, 0.40f, 0.15f, 1.00f);
            break;
        case 4:
            accent = ImVec4(1.00f, 0.40f, 0.20f, 1.00f);
            darkAcc = ImVec4(0.70f, 0.20f, 0.10f, 1.00f);
            bg = ImVec4(0.10f, 0.05f, 0.05f, 0.94f);
            btn = ImVec4(0.30f, 0.10f, 0.05f, 1.00f);
            text = ImVec4(0.95f, 0.80f, 0.80f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.40f, 0.15f, 0.08f, 1.00f);
            break;
        case 5:
            accent = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
            darkAcc = ImVec4(0.00f, 0.40f, 0.60f, 1.00f);
            bg = ImVec4(0.02f, 0.05f, 0.10f, 0.95f);
            btn = ImVec4(0.05f, 0.15f, 0.25f, 1.00f);
            text = ImVec4(0.80f, 0.95f, 1.00f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.05f, 0.20f, 0.35f, 1.00f);
            break;
        case 6:
            accent = ImVec4(0.00f, 1.00f, 0.00f, 1.00f);
            darkAcc = ImVec4(0.00f, 0.40f, 0.00f, 1.00f);
            bg = ImVec4(0.00f, 0.00f, 0.00f, 0.98f);
            btn = ImVec4(0.00f, 0.20f, 0.00f, 1.00f);
            text = ImVec4(0.00f, 0.80f, 0.00f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.00f, 0.25f, 0.00f, 1.00f);
            break;
        case 7:
            accent = ImVec4(1.00f, 0.00f, 0.60f, 1.00f);
            darkAcc = ImVec4(0.50f, 0.00f, 0.40f, 1.00f);
            bg = ImVec4(0.10f, 0.05f, 0.15f, 0.95f);
            btn = ImVec4(0.30f, 0.10f, 0.40f, 1.00f);
            text = ImVec4(1.00f, 0.80f, 0.20f, 1.00f);
            tab_select = accent;
            tab = darkAcc;
            break;
        case 8:
            accent = ImVec4(0.15f, 0.55f, 0.82f, 1.00f);
            darkAcc = ImVec4(0.10f, 0.40f, 0.60f, 1.00f);
            bg = ImVec4(0.00f, 0.17f, 0.21f, 0.95f);
            btn = ImVec4(0.03f, 0.21f, 0.26f, 1.00f);
            text = ImVec4(0.51f, 0.58f, 0.59f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.07f, 0.30f, 0.35f, 1.00f);
            break;
        case 9:
            accent = ImVec4(0.80f, 0.55f, 0.40f, 1.00f);
            darkAcc = ImVec4(0.60f, 0.40f, 0.30f, 1.00f);
            bg = ImVec4(0.15f, 0.12f, 0.10f, 0.95f);
            btn = ImVec4(0.30f, 0.20f, 0.15f, 1.00f);
            text = ImVec4(0.95f, 0.90f, 0.85f, 1.00f);
            tab_select = accent;
            tab = ImVec4(0.40f, 0.30f, 0.25f, 1.00f);
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
        set_color(ImGuiCol_ButtonActive, darkAcc);
        set_color(ImGuiCol_FrameBg, btn);
        set_color(ImGuiCol_FrameBgHovered, darkAcc);
        set_color(ImGuiCol_FrameBgActive, darkAcc);
        set_color(ImGuiCol_TitleBg, darkAcc);
        set_color(ImGuiCol_TitleBgActive, accent);
        set_color(ImGuiCol_Separator, accent);
        set_color(ImGuiCol_CheckMark, accent);
        set_color(ImGuiCol_SliderGrab, accent);
        set_color(ImGuiCol_SliderGrabActive, text);
        set_color(ImGuiCol_TextSelectedBg, darkAcc);
        set_color(ImGuiCol_Tab, tab);
        set_color(ImGuiCol_TabActive, tab_select);
        set_color(ImGuiCol_TabHovered, darkAcc);

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
        s = settings();
        ImGuiStyle style;
        ImGui::StyleColorsDark(&style);
        for(int i = 0; i < ImGuiCol_COUNT; i++)
        {
            s.colors[i][0] = style.Colors[i].x;
            s.colors[i][1] = style.Colors[i].y;
            s.colors[i][2] = style.Colors[i].z;
            s.colors[i][3] = style.Colors[i].w;
        }

        apply_preset(s, 0);
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

    template <typename T>
    void sync(json& j, const char* key, T& var, bool loading)
    {
        if(loading)
        {
            if(j.contains(key))
            {
                try
                {
                    var = j.at(key).get<T>();
                }
                catch(...)
                {}
            }
        }
        else
        {
            j[key] = var;
        }
    }

    void sync_vec2(json& j, const char* key, float var[2], bool loading)
    {
        if(loading)
        {
            if(j.contains(key) && j[key].is_array() && j[key].size() >= 2)
            {
                var[0] = j[key][0];
                var[1] = j[key][1];
            }
        }
        else
        {
            j[key] = { var[0], var[1] };
        }
    }

    void sync_all(json& j, struct settings& s, bool loading)
    {
        // Colors
        if(loading)
        {
            if(j.contains("colors"))
            {
                auto& jc = j["colors"];
                for(int i = 0; i < ImGuiCol_COUNT && (size_t)i < jc.size(); ++i)
                {
                    for(int c = 0; c < 4; ++c) s.colors[i][c] = jc[i][c];
                }
            }
        }
        else
        {
            j["colors"] = json::array();
            for(int i = 0; i < ImGuiCol_COUNT; i++)
            {
                j["colors"].push_back({ s.colors[i][0], s.colors[i][1], s.colors[i][2], s.colors[i][3] });
            }
        }

        // Floats
        sync(j, "font_scale", s.font_scale, loading);
        sync(j, "alpha", s.alpha, loading);
        sync(j, "disabled_alpha", s.disabled_alpha, loading);
        sync(j, "window_rounding", s.window_rounding, loading);
        sync(j, "child_rounding", s.child_rounding, loading);
        sync(j, "frame_rounding", s.frame_rounding, loading);
        sync(j, "popup_rounding", s.popup_rounding, loading);
        sync(j, "scrollbar_rounding", s.scrollbar_rounding, loading);
        sync(j, "grab_rounding", s.grab_rounding, loading);
        sync(j, "tab_rounding", s.tab_rounding, loading);
        sync(j, "window_border_size", s.window_border_size, loading);
        sync(j, "child_border_size", s.child_border_size, loading);
        sync(j, "popup_border_size", s.popup_border_size, loading);
        sync(j, "frame_border_size", s.frame_border_size, loading);
        sync(j, "tab_border_size", s.tab_border_size, loading);
        sync(j, "scrollbar_size", s.scrollbar_size, loading);
        sync(j, "grab_min_size", s.grab_min_size, loading);
        sync(j, "font_size", s.font_size, loading);

        // Strings
        sync(j, "font_path", s.font_path, loading);

        // Bools
        sync(j, "show_image", s.show_image, loading);
        sync(j, "show_colors", s.show_colors, loading);
        sync(j, "textanimator", s.textanimator, loading);
        sync(j, "show_modpack", s.show_modpack, loading);
        sync(j, "show_quest_editor", s.show_quest_editor, loading);

        // Vec2
        sync_vec2(j, "window_min_size", s.window_min_size, loading);
        sync_vec2(j, "window_padding", s.window_padding, loading);
        sync_vec2(j, "frame_padding", s.frame_padding, loading);
        sync_vec2(j, "item_spacing", s.item_spacing, loading);
        sync_vec2(j, "item_inner_spacing", s.item_inner_spacing, loading);
        sync_vec2(j, "cell_padding", s.cell_padding, loading);
        sync_vec2(j, "display_window_padding", s.display_window_padding, loading);
        sync_vec2(j, "display_safe_area_padding", s.display_safe_area_padding, loading);
        sync_vec2(j, "window_title_align", s.window_title_align, loading);
        sync_vec2(j, "button_text_align", s.button_text_align, loading);
        sync_vec2(j, "selectable_text_align", s.selectable_text_align, loading);

        // Version
        sync(j, "mc_version", s.mc_version, loading);
    }

    void load()
    {
        reset_to_defaults(saved);
        std::ifstream file("settings.json");
        if(file.is_open())
        {
            try
            {
                json j;
                file >> j;
                sync_all(j, saved, true);
            }
            catch(...)
            {}
        }
        else
        {
            apply_preset(saved, 0);
        }
        current = saved;
    }

    void save()
    {
        json j;
        sync_all(j, saved, false);
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
        apply_to_imgui(saved);
        ImGui::SetNextWindowSizeConstraints(ImVec2(550, 450), ImVec2(900, 900));
        ImGui::Begin("Settings", &show_menu);

        ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_NoCloseWithMiddleMouseButton |
                                     ImGuiTabBarFlags_FittingPolicyScroll;

        if(ImGui::BeginTabBar("SettingsTabs", tab_flags))
        {

            if(ImGui::BeginTabItem("Configuration"))
            {
                ImGui::Separator();

                ImGui::BeginChild("PresetsScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);
                ImGui::Checkbox("Show images", &current.show_image);
                ImGui::Checkbox("Show colors", &current.show_colors);
                ImGui::Checkbox("Show TextAnimator", &current.textanimator);
                ImGui::Checkbox("Show Modpack Tools", &current.show_modpack);
                ImGui::Checkbox("Show Quest Node Editor", &current.show_quest_editor);

                ImGui::Dummy(ImVec2(0, 10));
                ImGui::Separator();
                ImGui::Text("Minecraft Version:");
                
                ImGui::RadioButton("1.19.2", &current.mc_version, MC_1_19_2); ImGui::SameLine();
                ImGui::RadioButton("1.20.1", &current.mc_version, MC_1_20_1); ImGui::SameLine();
                ImGui::RadioButton("1.20.4", &current.mc_version, MC_1_20_4);
                
                ImGui::RadioButton("1.21.1", &current.mc_version, MC_1_21_1); ImGui::SameLine();
                ImGui::RadioButton("1.21.4", &current.mc_version, MC_1_21_4); ImGui::SameLine();
                ImGui::RadioButton("1.21.5+", &current.mc_version, MC_1_21_5);

                ImGui::EndChild();
                ImGui::EndTabItem();
            }
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