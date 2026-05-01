#pragma once

#include <imgui.h>
#include <string>
#include <vector>

namespace settings
{

    enum MCVersion {
        MC_1_19_2,
        MC_1_20_1,
        MC_1_20_4,
        MC_1_21_1,
        MC_1_21_4,
        MC_1_21_5
    };

    struct settings
    {
        float colors[ImGuiCol_COUNT][4];

        int mc_version = MC_1_21_1;

        float font_scale = 1.0f;
        std::string font_path = "";

        float alpha = 1.0f;
        float disabled_alpha = 0.60f;

        float window_rounding = 0.0f;
        float child_rounding = 0.0f;
        float frame_rounding = 0.0f;
        float popup_rounding = 0.0f;
        float scrollbar_rounding = 0.0f;
        float grab_rounding = 0.0f;
        float tab_rounding = 0.0f;

        float window_border_size = 1.0f;
        float child_border_size = 1.0f;
        float popup_border_size = 1.0f;
        float frame_border_size = 1.0f;
        float tab_border_size = 1.0f;

        float window_min_size[2] = { 32.0f, 32.0f };
        float scrollbar_size = 14.0f;
        float grab_min_size = 12.0f;

        float window_padding[2] = { 8.0f, 8.0f };
        float frame_padding[2] = { 4.0f, 3.0f };
        float item_spacing[2] = { 8.0f, 4.0f };
        float item_inner_spacing[2] = { 4.0f, 4.0f };
        float cell_padding[2] = { 4.0f, 2.0f };
        float display_window_padding[2] = { 19.0f, 19.0f };
        float display_safe_area_padding[2] = { 3.0f, 3.0f };

        float window_title_align[2] = { 0.0f, 0.5f };
        float button_text_align[2] = { 0.5f, 0.5f };
        float selectable_text_align[2] = { 0.0f, 0.0f };
        float font_size = 16.0f;

        //window
        bool show_image = true;
        bool show_colors = true;
        bool textanimator = true;
        bool show_modpack = true;
    };

    struct modpack
    {
        bool input_as_json = false;
        bool output_as_json = false;
        bool using_cf = true;
        bool to_lang_files = false;
        std::string path = ".";
        std::string mod_path = ".";
    };

    extern struct settings current;
    extern struct settings saved;
    extern struct modpack modpack;
    extern bool show_menu;

    void load();
    void save();
    void apply_to_imgui(const struct settings& s);
    void draw_menu();

    void reset_to_defaults(struct settings& s);

    void apply_preset(struct settings& s, int preset_id);

    std::vector<std::string> get_system_fonts();
} // namespace settings