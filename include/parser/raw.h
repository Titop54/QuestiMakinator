#ifndef RAW_JSON_HPP
#define RAW_JSON_HPP

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace raw {

inline std::string json_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1F) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                        << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

inline std::string to_json(const std::string& input, bool use_extra = false) {
    // Tabla de códigos de color hexadecimales
    static const std::string COLOR_CODES[16] = {
        "#000000", "#0000AA", "#00AA00", "#00AAAA", "#AA0000", "#AA00AA", 
        "#FFAA00", "#AAAAAA", "#555555", "#5555FF", "#55FF55", "#55FFFF", 
        "#FF5555", "#FF55FF", "#FFFF55", "#FFFFFF"
    };

    struct State {
        std::string color;  // Vacío por defecto en lugar de "#FFFFFF"
        bool bold = false;
        bool italic = false;
        bool underlined = false;
        bool strikethrough = false;
        bool obfuscated = false;
        std::string click_action;
        std::string click_value;
        std::string hover_action;
        std::string hover_value;
    };

    std::vector<std::string> components;
    State current_state;
    std::string current_text;
    bool has_formatting = false;

    auto flush_text = [&]() {
        if (current_text.empty()) return;

        std::ostringstream oss;
        oss << "{";
        oss << "\"text\":\"" << json_escape(current_text) << "\"";
        
        auto has_extra_properties = [](const State& s) -> bool {
            return (s.color != "#FFFFFF") ||
                s.bold ||
                s.italic ||
                s.underlined ||
                s.strikethrough ||
                s.obfuscated ||
                !s.click_action.empty() ||
                !s.hover_action.empty();
        };

        // Verificar si es el primer componente y tiene propiedades extra
        if (components.empty() && has_extra_properties(current_state)) {
            components.push_back("{\"text\":\"\"}");
        }
        // Solo añadir propiedades si no son valores por defecto
        if (!current_state.color.empty() && current_state.color != "#FFFFFF") {
            oss << ",\"color\":\"" << current_state.color << "\"";
        }

        if (current_state.bold) oss << ",\"bold\":true";
        if (current_state.italic) oss << ",\"italic\":true";
        if (current_state.underlined) oss << ",\"underlined\":true";
        if (current_state.strikethrough) oss << ",\"strikethrough\":true";
        if (current_state.obfuscated) oss << ",\"obfuscated\":true";
        
        if (!current_state.click_action.empty()) {
            oss << ",\"clickEvent\":{\"action\":\"" << current_state.click_action 
                << "\",\"value\":\"" << json_escape(current_state.click_value) << "\"}";
        }
        
        if (!current_state.hover_action.empty()) {
            oss << ",\"hoverEvent\":{\"action\":\"" << current_state.hover_action << "\"";
            if (current_state.hover_action == "show_item") {
                oss << ",\"contents\":{\"id\":\"" << json_escape(current_state.hover_value) 
                    << "\",\"count\":1}";
            } else {
                oss << ",\"contents\":{\"text\":\"" << json_escape(current_state.hover_value) << "\"}";
            }
            oss << "}";
        }
        
        oss << "}";
        components.push_back(oss.str());
        current_text.clear();
        current_state.hover_action.clear();
        current_state.hover_value.clear();
        current_state.click_action.clear();
        current_state.click_value.clear();
    };

    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '&' && (i + 1) < input.size()) {
            char code = input[i + 1];
            if (code == 'r') {
                flush_text();
                // Reset completo del estado (incluyendo eventos)
                current_state = State();
                has_formatting = true;
                i += 2;
                continue;
            } else if (code == 'l' || code == 'o' || code == 'n' || code == 'm' || code == 'k') {
                flush_text();
                if (code == 'l') current_state.bold = true;
                else if (code == 'o') current_state.italic = true;
                else if (code == 'n') current_state.underlined = true;
                else if (code == 'm') current_state.strikethrough = true;
                else if (code == 'k') current_state.obfuscated = true;
                has_formatting = true;
                i += 2;
                continue;
            }
            else if((code >= '0' && code <= '9') || 
                    (code >= 'a' && code <= 'f') || 
                    (code >= 'A' && code <= 'F'))
            {
                flush_text();
                int index;
                if (code >= '0' && code <= '9') index = code - '0';
                else if (code >= 'a' && code <= 'f') index = 10 + (code - 'a');
                else index = 10 + (code - 'A');
                current_state.color = COLOR_CODES[index];
                has_formatting = true;
                i += 2;
                continue;
            }
            else if(code == '#' && (i + 7) < input.size())
            {
                flush_text();
                current_state.color = "#" + input.substr(i + 2, 6);
                has_formatting = true;
                i += 8;
                continue;
            }
            else if(code == '@' && (i + 2) < input.size())
            {
                size_t start = i + 2;
                size_t pos = start;
                std::string cmd;
                
                // Extraer el nombre del comando
                while (pos < input.size() && (std::isalpha(input[pos]) || input[pos] == '_')) {
                    cmd += input[pos];
                    pos++;
                }
                
                // Manejar comando de página sin parámetros
                if (cmd == "page") {
                    flush_text();
                    components.emplace_back("{\"text\":\"\\n{@pagebreak}\\n\"}");
                    has_formatting = true;
                    i = pos;
                    continue;
                }
                
                // Manejar comandos con parámetros entre comillas
                if(pos < input.size() && input[pos+1] == '"') 
                {
                    size_t end_quote = pos + 2;
                    while (end_quote < input.size() && input[end_quote] != '"') {
                        if (input[end_quote] == '\\' && end_quote + 1 < input.size()) {
                            end_quote++; // Saltar caracter de escape
                        }
                        end_quote++;
                    }
                    
                    if (end_quote < input.size()) {
                        std::string value = input.substr(pos + 2, end_quote - pos - 2);
                        flush_text();
                        has_formatting = true;
                        
                        if (cmd == "url") {
                            current_state.click_action = "open_url";
                            current_state.click_value = value;
                        } else if (cmd == "in") {
                            current_state.click_action = "suggest_command";
                            current_state.click_value = value;
                        } else if (cmd == "file") {
                            current_state.click_action = "open_file";
                            current_state.click_value = value;
                        } else if (cmd == "command") {
                            current_state.click_action = "run_command";
                            current_state.click_value = value;
                        } else if (cmd == "copy") {
                            current_state.click_action = "copy_to_clipboard";
                            current_state.click_value = value;
                        } else if (cmd == "change") {
                            current_state.click_action = "change_page";
                            current_state.click_value = value;
                        }
                        
                        i = end_quote + 1;
                        continue;
                    }
                }
            }
            else if (code == '&' && (i + 2) < input.size()) {
                // Manejo de eventos hover
                size_t start = i + 2;
                size_t pos = start;
                std::string cmd;

                while (pos < input.size() && (std::isalpha(input[pos]) || input[pos] == '_')) {
                    cmd += input[pos];
                    pos++;
                }

                if (pos < input.size() && input[pos+1] == '"') {
                    size_t end_quote = pos + 2;
                    while (end_quote < input.size()) {
                        if (input[end_quote] == '\\' && end_quote + 1 < input.size()) {
                            end_quote += 2;
                        } else if (input[end_quote] == '"') {
                            break;
                        } else {
                            end_quote++;
                        }
                    }

                    if (end_quote < input.size() && input[end_quote] == '"') {
                        std::string value = input.substr(pos + 2, end_quote - pos - 2);
                        has_formatting = true;

                        if (cmd == "text") {
                            current_state.hover_action = "show_text";
                            current_state.hover_value = value;
                        } else if (cmd == "item") {
                            current_state.hover_action = "show_item";
                            current_state.hover_value = value;
                        }

                        i = end_quote + 1;
                        continue;
                    }
                }
            }
        }
        current_text += input[i];
        i++;
    }
    flush_text();

    if(!has_formatting)
    {
        return "\"" + json_escape(input) + "\"";
    }
    else if(components.empty())
    {
        return "\"\"";
    }
    else if(components.size() == 1)
    {
        return "[" + components[0] + "]";
    }
    else
    {
        if(use_extra)
        {
            std::string first_component = components[0];
            
            if(!first_component.empty() && first_component.back() == '}')
            {
                first_component.pop_back();
            }

            first_component += ",\"extra\":[";
            for(size_t j = 1; j < components.size(); ++j)
            {
                if(j > 1) first_component += ",";
                first_component += components[j];
            }
            first_component += "]}";
            
            return first_component;
        }
        else //original
        {
            
            std::string result = "[";
            for(size_t j = 0; j < components.size(); ++j)
            {
                if(j > 0) result += ",";
                result += components[j];
            }
            result += "]";
            return result;
        }
    }
}

inline void hola()
{
    std::cout << "Program made by Titop54 - https://github.com/Titop54\n"
              << "You can't use this program unless explicit permission by Titop54 which has being granted with screenshots proof\n"
              << "If you are found using this without permission, you are granting me to remove your content :)\n"
              << "\nBase FTB setup:\n"
              << "- Bold (&l)\n"
              << "- Italic (&o)\n"
              << "- Underline (&n)\n"
              << "- Strikethrough (&m)\n"
              << "- Colors (&0 to &f)\n"
              << "- Reset to default (&r)\n"
              << "\nCustom commands:\n"
              << "- Custom colors (&#<6-digit hex>), goes before anything else\n"
              << "- Obfuscated (&k)\n"
              << "- Insert URL (&@url:\"<url>\"), words after this will be affected\n"
              << "- Insert chat text (&@in:\"<text>\"), words after this will be affected\n"
              << "- Open file (&@file:\"<file>\"), words after this will be affected\n"
              << "- Run command (&@command:\"<command>\"), words after this will be affected\n"
              << "- Copy to clipboard (&@copy:\"<text>\"), words after this will be affected\n"
              << "- Change page (&@change:\"<value>\"), words after this will be affected\n"
              << "- New page (&@page), acts as the new page on ftb\n"
              << "- Show text on hover (&&text:\"<text>\"), words before this will be affected\n"
              << "- Show item on hover (&&item:\"<item_id>\"), words before this will be affected\n\n";
}

inline void interactive_converter()
{
    hola();
    std::string input;
    bool use_extra_mode = false;
    
    while(true)
    {
        std::cout << "Use mode <extra | array> to change mode\n";
        std::cout << "> (Mode: " << (use_extra_mode ? "EXTRA" : "ARRAY") << ") ";
        std::getline(std::cin, input);
        
        if(input == "F" || input == "f")
        {
            std::cout << "Exiting converter...\n";
            break;
        }
        
        if(input == "mode extra")
        {
            use_extra_mode = true;
            std::cout << "Extra mode\n";
            continue;
        }
        else if(input == "mode array")
        {
            use_extra_mode = false;
            std::cout << "Array mode\n";
            continue;
        }
        
        std::string json = to_json(input, use_extra_mode);
        std::cout << "\nJSON Result:\n";
        std::cout << json << "\n\n";
    }
}

} // namespace raw

#endif // RAW_JSON_HPP