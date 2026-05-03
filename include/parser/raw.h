#pragma once

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <gui/display/colors.h>
#include <gui/display/settings.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ostream>
#include <parser/snbt.h>
#include <string>
#include <vector>

namespace raw
{

    static const std::vector<std::string> IGNORED_COMMANDS = {
        "l", "o", "n", "m", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "a", "b", "c", "d", "e", "f", "r", "k", "@url:\"", "@in:\"", "@file:\"",
        "@command:\"", "@copy:\"", "@change:\"", "@page", "&text:\"", "&item:\"", "&shadow:\"",
        "&@custom:\""
    };

    inline std::string get_color(std::string hex)
    {
        char hex_code = hex[0];
        if((hex_code >= '0' && hex_code <= '9') ||
            (hex_code >= 'a' && hex_code <= 'f') ||
            (hex_code >= 'A' && hex_code <= 'F'))
        {
            int index;
            if(hex_code >= '0' && hex_code <= '9')
            {
                index = hex_code - '0';
            }
            else if(hex_code >= 'a' && hex_code <= 'f')
            {
                index = 10 + (hex_code - 'a');
            }
            else
            {
                index = 10 + (hex_code - 'A');
            }
            return COLOR_CODES[index];
        }
        return "#ffffff";
    }

    inline bool check_non_ftb(const std::string& text, bool force)
    {
        if(force) return true;

        if(text.contains("&@url:") ||
            text.contains("&@in:") ||
            text.contains("&@file:") ||
            text.contains("&@command:") ||
            text.contains("&@copy:") ||
            text.contains("&@change:") ||
            text.contains("&@custom:") ||
            text.contains("&&text:") ||
            text.contains("&&item:") ||
            text.contains("&&shadow:")) return true;

        if(settings::current.mc_version < settings::MCVersion::MC_1_21_1 && text.contains("&#")) return true;
        return false;
    }

    inline std::string change_gradient(std::string& text)
    {
        struct Text
        {
            int start;
            int end;
            std::string text;
            int type;
            std::vector<std::string> colors;
        };

        // Función para interpolar colores
        auto interpolate_color = [](const std::string& color1, const std::string& color2, float t) -> std::string {
            int r1 = std::stoi(color1.substr(0, 2), nullptr, 16);
            int g1 = std::stoi(color1.substr(2, 2), nullptr, 16);
            int b1 = std::stoi(color1.substr(4, 2), nullptr, 16);

            int r2 = std::stoi(color2.substr(0, 2), nullptr, 16);
            int g2 = std::stoi(color2.substr(2, 2), nullptr, 16);
            int b2 = std::stoi(color2.substr(4, 2), nullptr, 16);

            int r = static_cast<int>(r1 + (r2 - r1) * t);
            int g = static_cast<int>(g1 + (g2 - g1) * t);
            int b = static_cast<int>(b1 + (b2 - b1) * t);

            char hex_color[7];
            snprintf(hex_color, sizeof(hex_color), "%02X%02X%02X", r, g, b);
            return std::string(hex_color);
        };

        // Función para encontrar el límite del texto a modificar
        auto find_limit = [](const std::string& str) -> size_t {
            size_t pos = 0;
            while(pos < str.size())
            {
                if(str[pos] == '\\' && pos + 1 < str.size())
                {
                    pos += 2;
                    continue;
                }

                if(str[pos] == '&')
                {
                    if(pos + 1 < str.size())
                    {
                        if(str[pos + 1] == 'r')
                        {
                            return pos;
                        }
                        if(str[pos + 1] == '#' && pos + 8 <= str.size())
                        {
                            return pos;
                        }
                        std::string candidate = str.substr(pos, 2);
                        for(const auto& cmd : IGNORED_COMMANDS)
                        {
                            if(candidate == cmd)
                            {
                                return pos;
                            }
                        }
                    }
                }
                pos++;
            }
            return str.size();
        };

        std::vector<Text> cases;
        size_t status = 0;

        while(status != std::string::npos)
        {
            status = text.find("&@gradient:\"", status);
            if(status == std::string::npos) break;

            Text segment;
            segment.start = static_cast<int>(status);
            size_t end_pos = text.find("\"", status + 12);
            if(end_pos == std::string::npos) break;

            size_t next_gradient = text.find("&@gradient:\"", status + 1);
            segment.end = (next_gradient == std::string::npos) ? text.size() - 1 : next_gradient - 1;
            segment.text = text.substr(segment.start, segment.end - segment.start + 1);

            if(segment.text.size() < 15)
            {
                status = segment.end + 1;
                continue;
            }
            std::cout << segment.text.substr(12, 1) << std::flush;
            segment.type = std::stoi(segment.text.substr(12, 1));

            switch(segment.type)
            {
            case 1:
                if(segment.text.size() < 30)
                {
                    segment.type = -1;
                    break;
                }
                segment.colors.emplace_back(segment.text.substr(15, 6));
                segment.colors.emplace_back(segment.text.substr(23, 6));
                break;
            case 2:
                if(segment.text.size() < 22)
                {
                    segment.type = -1;
                    break;
                }
                {
                    size_t color_pos = 15;
                    while(color_pos + 6 <= segment.text.size())
                    {
                        segment.colors.emplace_back(segment.text.substr(color_pos, 6));
                        color_pos += 8;
                        if(color_pos >= segment.text.size() || segment.text[color_pos - 1] != '#') break;
                    }
                }
                break;
            case 3: {
                if(segment.text.size() < 22)
                {
                    segment.type = -1;
                    break;
                }
                // first color
                std::string first_color = segment.text.substr(15, 6);
                segment.colors.emplace_back(first_color);

                // check the closing "
                size_t content_start = segment.text.find("\"", 14) + 1;
                if(content_start == std::string::npos) break;

                // Buscar próximos códigos de color
                size_t color_pos = segment.text.find("&", content_start);

                if(color_pos != std::string::npos && color_pos < static_cast<size_t>(segment.end))
                {
                    if(segment.text.substr(color_pos, 2) == "&r") // we found nothing
                    {
                        break;
                    }
                    // hex colors
                    else if(segment.text.substr(color_pos, 2) == "&#" && color_pos + 8 <= static_cast<size_t>(segment.end))
                    {
                        segment.colors.emplace_back(segment.text.substr(color_pos + 2, 6));
                    }
                    // letters
                    else if(color_pos + 1 < static_cast<size_t>(segment.end))
                    {
                        segment.colors.emplace_back(
                            get_color(segment.text.substr(color_pos, 1)));
                    }
                }
            }
            break;
            default:
                segment.type = -1;
                break;
            }

            if(segment.type != -1)
            {
                cases.emplace_back(segment);
            }
            status = segment.end + 1;
        }

        // last to start, why? because we will change stuff
        std::string text_input = text;
        for(int i = cases.size() - 1; i >= 0; i--)
        {
            auto& c = cases[i];
            size_t content_start = c.text.find("\"", 12) + 1;
            if(content_start == std::string::npos) continue;

            std::string content = c.text.substr(content_start);
            size_t limit = find_limit(content);
            std::string text_to_color = content.substr(0, limit);
            std::string remaining_text = content.substr(limit);

            std::string colored_text;
            if(c.colors.size() == 1)
            {
                colored_text = "&#" + c.colors[0] + text_to_color;
            }
            else
            {
                size_t length = text_to_color.size();
                for(size_t j = 0; j < length; j++)
                {
                    float t = static_cast<float>(j) / (length - 1);
                    float segment = t * (c.colors.size() - 1);
                    int color_index = static_cast<int>(segment);
                    if(static_cast<size_t>(color_index) >= c.colors.size() - 1)
                    {
                        colored_text += "&#" + c.colors.back() + text_to_color[j];
                    }
                    else
                    {
                        float segment_t = segment - color_index;
                        std::string color = interpolate_color(
                            c.colors[color_index],
                            c.colors[color_index + 1],
                            segment_t);
                        colored_text += "&#" + color + text_to_color[j];
                    }
                }
            }

            std::string new_segment = colored_text + remaining_text;
            text_input.replace(c.start, c.end - c.start + 1, new_segment);
        }

        return text_input;
    }

    inline std::string to_json(std::string& input, bool use_extra = false, bool force = false)
    {
        std::string text_input = change_gradient(input);
        bool is_1_21_5 = settings::current.mc_version >= settings::MC_1_21_5;

        if(!check_non_ftb(text_input, force))
        {
            std::string json_str = nlohmann::json(text_input).dump();
            return is_1_21_5 ? snbt::json_to_tag(json_str) : json_str;
        }

        struct State
        {
            std::string color;
            bool bold = false;
            bool italic = false;
            bool underlined = false;
            bool strikethrough = false;
            bool obfuscated = false;
            std::string click_action;
            std::string click_value;
            std::string hover_action;
            std::string hover_value;
            std::string shadow;
        };

        std::vector<nlohmann::json> components;
        State current_state;
        std::string current_text;
        bool has_formatting = false;

        auto flush_text = [&]() {
            if(current_text.empty()) return;

            nlohmann::json component;
            component["text"] = current_text;

            auto has_extra_properties = [](const State& s) -> bool {
                return (!s.color.empty() && s.color != "#FFFFFF") || s.bold || s.italic || s.underlined || s.strikethrough || s.obfuscated || !s.click_action.empty() || !s.hover_action.empty() || !s.shadow.empty();
            };

            if(components.empty() && has_extra_properties(current_state))
            {
                components.push_back({ { "text", "" } });
            }

            if(!current_state.color.empty() && current_state.color != "#FFFFFF")
            {
                component["color"] = current_state.color;
            }

            if(current_state.bold) component["bold"] = true;
            if(current_state.italic) component["italic"] = true;
            if(current_state.underlined) component["underlined"] = true;
            if(current_state.strikethrough) component["strikethrough"] = true;
            if(current_state.obfuscated) component["obfuscated"] = true;

            if(!current_state.shadow.empty())
            {
                try
                {
                    component["shadow_color"] = std::stoll(current_state.shadow);
                }
                catch(...)
                {
                }
            }

            if(!current_state.click_action.empty())
            {
                if(current_state.click_action == "custom" && is_1_21_5)
                {
                    nlohmann::json click;
                    click["action"] = "custom";
                    std::string val = current_state.click_value;
                    size_t colon = val.find(':');
                    if(colon != std::string::npos)
                    {
                        click["id"] = val.substr(0, colon);
                        click["payload"] = val.substr(colon + 1);
                    }
                    else
                    {
                        click["id"] = val;
                    }
                    component["click_event"] = click;
                }
                else if(current_state.click_action == "custom")
                {
                    // Ignore custom action for versions < 1.21.5
                }
                else if(is_1_21_5)
                {
                    nlohmann::json click;
                    click["action"] = current_state.click_action;
                    if(current_state.click_action == "open_url")
                        click["url"] = current_state.click_value;
                    else if(current_state.click_action == "run_command" || current_state.click_action == "suggest_command")
                        click["command"] = current_state.click_value;
                    else if(current_state.click_action == "change_page")
                    {
                        try
                        {
                            click["page"] = std::stoi(current_state.click_value);
                        }
                        catch(...)
                        {
                            click["page"] = 1;
                        }
                    }
                    else
                        click["value"] = current_state.click_value;
                    component["click_event"] = click;
                }
                else
                {
                    component["clickEvent"] = { { "action", current_state.click_action }, { "value", current_state.click_value } };
                }
            }

            if(!current_state.hover_action.empty())
            {
                if(is_1_21_5)
                {
                    nlohmann::json hover;
                    hover["action"] = current_state.hover_action;
                    if(current_state.hover_action == "show_text")
                        hover["text"] = current_state.hover_value;
                    else if(current_state.hover_action == "show_item")
                    {
                        hover["id"] = current_state.hover_value;
                        hover["count"] = 1;
                    }
                    else
                        hover["contents"] = { { "text", current_state.hover_value } };
                    component["hover_event"] = hover;
                }
                else
                {
                    nlohmann::json hover;
                    hover["action"] = current_state.hover_action;
                    if(current_state.hover_action == "show_item")
                    {
                        hover["contents"] = { { "id", current_state.hover_value }, { "count", 1 } };
                    }
                    else
                    {
                        hover["contents"] = { { "text", current_state.hover_value } };
                    }
                    component["hoverEvent"] = hover;
                }
            }

            components.push_back(component);
            current_text.clear();
            current_state.hover_action.clear();
            current_state.hover_value.clear();
            current_state.click_action.clear();
            current_state.click_value.clear();
            current_state.shadow.clear();
        };

        size_t i = 0;
        while(i < text_input.size())
        {
            if(text_input[i] == '&' && (i + 1) < text_input.size())
            {
                char code = text_input[i + 1];
                if(code == 'r')
                {
                    flush_text();
                    current_state = State();
                    has_formatting = true;
                    i += 2;
                    continue;
                }
                else if(code == 'l' || code == 'o' || code == 'n' || code == 'm' || code == 'k')
                {
                    flush_text();
                    if(code == 'l')
                        current_state.bold = true;
                    else if(code == 'o')
                        current_state.italic = true;
                    else if(code == 'n')
                        current_state.underlined = true;
                    else if(code == 'm')
                        current_state.strikethrough = true;
                    else if(code == 'k')
                        current_state.obfuscated = true;
                    has_formatting = true;
                    i += 2;
                    continue;
                }
                else if((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F'))
                {
                    flush_text();
                    int index;
                    if(code >= '0' && code <= '9')
                        index = code - '0';
                    else if(code >= 'a' && code <= 'f')
                        index = 10 + (code - 'a');
                    else
                        index = 10 + (code - 'A');
                    current_state.color = COLOR_CODES[index];
                    has_formatting = true;
                    i += 2;
                    continue;
                }
                else if(code == '#' && (i + 7) < text_input.size())
                {
                    flush_text();
                    current_state.color = "#" + text_input.substr(i + 2, 6);
                    has_formatting = true;
                    i += 8;
                    continue;
                }
                else if(code == '@' && (i + 2) < text_input.size())
                {
                    size_t start = i + 2;
                    size_t pos = start;
                    std::string cmd;

                    while(pos < text_input.size() && (std::isalpha(text_input[pos]) || text_input[pos] == '_'))
                    {
                        cmd += text_input[pos];
                        pos++;
                    }

                    if(cmd == "page")
                    {
                        flush_text();
                        components.push_back({ { "text", "\n{@pagebreak}\n" } });
                        has_formatting = true;
                        i = pos;
                        continue;
                    }

                    if(pos < text_input.size() && text_input[pos + 1] == '"')
                    {
                        size_t end_quote = pos + 2;
                        while(end_quote < text_input.size() && text_input[end_quote] != '"')
                        {
                            if(text_input[end_quote] == '\\' && end_quote + 1 < text_input.size())
                            {
                                end_quote++;
                            }
                            end_quote++;
                        }

                        if(end_quote < text_input.size())
                        {
                            std::string value = text_input.substr(pos + 2, end_quote - pos - 2);
                            flush_text();
                            has_formatting = true;

                            if(cmd == "url")
                                current_state.click_action = "open_url";
                            else if(cmd == "in")
                                current_state.click_action = "suggest_command";
                            else if(cmd == "file")
                                current_state.click_action = "open_file";
                            else if(cmd == "command")
                                current_state.click_action = "run_command";
                            else if(cmd == "copy")
                                current_state.click_action = "copy_to_clipboard";
                            else if(cmd == "change")
                                current_state.click_action = "change_page";
                            else if(cmd == "custom")
                                current_state.click_action = "custom";

                            current_state.click_value = value;
                            i = end_quote + 1;
                            continue;
                        }
                    }
                }
                else if(code == '&' && (i + 2) < text_input.size())
                {
                    size_t start = i + 2;
                    size_t pos = start;
                    std::string cmd;

                    while(pos < text_input.size() && (std::isalpha(text_input[pos]) || text_input[pos] == '_'))
                    {
                        cmd += text_input[pos];
                        pos++;
                    }

                    if(pos < text_input.size() && text_input[pos + 1] == '"')
                    {
                        size_t end_quote = pos + 2;
                        while(end_quote < text_input.size())
                        {
                            if(text_input[end_quote] == '\\' && end_quote + 1 < text_input.size())
                            {
                                end_quote += 2;
                            }
                            else if(text_input[end_quote] == '"')
                            {
                                break;
                            }
                            else
                            {
                                end_quote++;
                            }
                        }

                        if(end_quote < text_input.size() && text_input[end_quote] == '"')
                        {
                            std::string value = text_input.substr(pos + 2, end_quote - pos - 2);
                            has_formatting = true;

                            if(cmd == "text")
                            {
                                current_state.hover_action = "show_text";
                                current_state.hover_value = value;
                            }
                            else if(cmd == "item")
                            {
                                current_state.hover_action = "show_item";
                                current_state.hover_value = value;
                            }
                            else if(cmd == "shadow")
                            {
                                if(value.length() >= 1 && value[0] == '#' && value.length() == 9)
                                {
                                    std::string argb_hex = value.substr(1);
                                    unsigned int shadow_decimal = argb_hex_to_decimal(argb_hex);
                                    current_state.shadow = std::to_string(shadow_decimal);
                                }
                            }

                            i = end_quote + 1;
                            continue;
                        }
                    }
                }
            }
            current_text += text_input[i];
            i++;
        }
        flush_text();

        nlohmann::json final_json;
        if(!has_formatting)
        {
            final_json = text_input;
        }
        else if(components.empty())
        {
            final_json = "";
        }
        else if(components.size() == 1)
        {
            final_json = components;
        }
        else
        {
            if(use_extra)
            {
                nlohmann::json result = components[0];
                nlohmann::json extra = nlohmann::json::array();
                for(size_t j = 1; j < components.size(); ++j)
                {
                    extra.push_back(components[j]);
                }
                result["extra"] = extra;
                final_json = result;
            }
            else
            {
                final_json = components;
            }
        }

        std::string json_str = final_json.dump();
        return is_1_21_5 ? snbt::json_to_tag(json_str) : json_str;
    }

} // namespace raw