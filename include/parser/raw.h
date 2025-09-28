#ifndef RAW_JSON_HPP
#define RAW_JSON_HPP

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <cctype>
#include <iomanip>
#include <sstream>
namespace raw
{

    // Tabla de códigos de color hexadecimales
    static const std::string COLOR_CODES[16] = {
            "#000000", "#0000AA", "#00AA00", "#00AAAA", "#AA0000", "#AA00AA", 
            "#FFAA00", "#AAAAAA", "#555555", "#5555FF", "#55FF55", "#55FFFF", 
            "#FF5555", "#FF55FF", "#FFFF55", "#FFFFFF"
    };

    static const std::vector<std::string> IGNORED_COMMANDS = {
        "l", "o", "n", "m", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", 
        "a", "b", "c", "d", "e", "f", "r", "k", "@url:\"", "@in:\"", "@file:\"", 
        "@command:\"", "@copy:\"", "@change:\"", "@page", "&text:\"", "&item:\""
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

    inline bool check_non_ftb(const std::string& text)
    {
        if(text.contains("&k") ||
           text.contains("&@url:") ||
           text.contains("&@in:") ||
           text.contains("&@file:") ||
           text.contains("&@command:") ||
           text.contains("&@copy:") ||
           text.contains("&@change:") ||
           text.contains("&&text:") ||
           text.contains("&&item:")
        ) return true;

        return false;
    }

    inline bool change_gradient(std::string& text)
    {
        bool changes_made = false;
        std::string result;
        size_t pos = 0;
        const size_t len = text.length();
        
        // Lista de comandos a ignorar (sin el & inicial)
        
        
        while(pos < len)
        {
            // Buscar inicio de gradiente
            size_t gradient_start = text.find("&@gradient:\"", pos);
            if(gradient_start == std::string::npos)
            {
                // No hay más gradientes
                result.append(text, pos, std::string::npos);
                break;
            }
            
            result.append(text, pos, gradient_start - pos);
            pos = gradient_start;
            
            // Verificar que hay suficiente texto después del gradiente
            if(pos + 12 >= len) // &@gradient:"#:"
            {
                result.append(text, pos, std::string::npos);
                break;
            }
            
            // Saltar &@gradient:"
            size_t color_start = pos + 12;
            
            // Verificar primer color
            if(text[color_start] != '#')
            {
                result.append(text, pos, 12);
                pos += 12;
                continue;
            }
            
            std::string color1;
            for(int i = 0; i < 6; ++i)
            {
                if(color_start + 1 + i >= len || 
                    !std::isxdigit(static_cast<unsigned char>(text[color_start + 1 + i])))
                {
                    result.append(text, pos, std::min(size_t(12), len - pos));
                    pos += 12;
                    continue;
                }
                color1 += std::tolower(text[color_start + 1 + i]);
            }
            
            // Verificar separador :
            size_t separator_pos = color_start + 7;
            if(separator_pos >= len || text[separator_pos] != ':')
            {
                result.append(text, pos, std::min(size_t(12), len - pos));
                pos += 12;
                continue;
            }
            
            // Verificar segundo #
            size_t second_hash_pos = separator_pos + 1;
            if (second_hash_pos >= len || text[second_hash_pos] != '#')
            {
                result.append(text, pos, std::min(size_t(12), len - pos));
                pos += 12;
                continue;
            }
            
            // Segundo color
            std::string color2;
            for(int i = 0; i < 6; ++i)
            {
                if (second_hash_pos + 1 + i >= len || 
                    !std::isxdigit(static_cast<unsigned char>(text[second_hash_pos + 1 + i])))
                {
                    result.append(text, pos, std::min(size_t(12), len - pos));
                    pos += 12;
                    continue;
                }
                color2 += std::tolower(text[second_hash_pos + 1 + i]);
            }
            
            // Verificar comilla final
            size_t quote_end_pos = second_hash_pos + 7;
            if(quote_end_pos >= len || text[quote_end_pos] != '"')
            {
                result.append(text, pos, std::min(size_t(12), len - pos));
                pos += 12;
                continue;
            }
            
            // Buscar texto hasta el próximo comando especial o &r
            size_t text_start = quote_end_pos + 1;
            size_t text_end = text_start;
            bool found_end = false;
            std::string current_command;
            
            while(text_end < len)
            {
                // Si encontramos un &, podría ser el inicio de un comando
                if(text[text_end] == '&' && text_end + 1 < len)
                {
                    // Verificar si es un comando a ignorar
                    bool is_ignored_command = false;
                    
                    for(const auto& cmd : IGNORED_COMMANDS)
                    {
                        if(text.compare(text_end + 1, cmd.length(), cmd) == 0)
                        {
                            is_ignored_command = true;
                            break;
                        }
                    }
                    
                    // También verificar códigos de color hexadecimales (&#)
                    if(text_end + 2 < len && text[text_end + 1] == '#' && 
                    std::isxdigit(static_cast<unsigned char>(text[text_end + 2])))
                    {
                        is_ignored_command = true;
                    }
                    
                    if(is_ignored_command)
                    {
                        // Si no estamos escapando este & con otro &
                        if(text_end == text_start || text[text_end - 1] != '&')
                        {
                            found_end = true;
                            break;
                        }
                        else
                        {
                            // Es un &&, saltar un & adicional
                            text_end++;
                        }
                    }
                }
                
                text_end++;
            }
            
            if(!found_end)
            {
                text_end = len;
            }
            
            std::string gradient_text = text.substr(text_start, text_end - text_start);
            
            if(!gradient_text.empty())
            {
                changes_made = true;
                const size_t text_length = gradient_text.length();
                
                for(size_t i = 0; i < text_length; ++i)
                {
                    // Si encontramos un &, verificar si es el inicio de un comando
                    if(gradient_text[i] == '&' && i + 1 < text_length)
                    {
                        bool is_command = false;
                        
                        for(const auto& cmd : IGNORED_COMMANDS)
                        {
                            if(gradient_text.compare(i + 1, cmd.length(), cmd) == 0)
                            {
                                is_command = true;
                                break;
                            }
                        }
                        
                        // También verificar códigos de color hexadecimales (&#)
                        if(i + 2 < text_length && gradient_text[i + 1] == '#' && 
                        std::isxdigit(static_cast<unsigned char>(gradient_text[i + 2])))
                        {
                            is_command = true;
                        }
                        
                        if(is_command)
                        {
                            // Si el & anterior no era otro & (no está escapado)
                            if(i == 0 || gradient_text[i - 1] != '&')
                            {
                                // Añadir el comando sin procesar
                                result += gradient_text[i];
                                continue;
                            }
                            else
                            {
                                // Eliminar el & de escape y procesar normalmente
                                result.pop_back();
                            }
                        }
                    }
                    
                    // Calcular color intermedio para esta letra
                    double ratio = static_cast<double>(i) / static_cast<double>(text_length - 1);
                    
                    // Interpolar cada componente del color
                    std::string intermediate_color;
                    for(int j = 0; j < 6; j += 2)
                    {
                        // Convertir componentes hexadecimales a decimales
                        int comp1 = std::stoi(color1.substr(j, 2), nullptr, 16);
                        int comp2 = std::stoi(color2.substr(j, 2), nullptr, 16);
                        
                        // Interpolar
                        int intermediate_comp = static_cast<int>(comp1 + ratio * (comp2 - comp1));
                        
                        // Convertir de vuelta a hexadecimal
                        char hex[3];
                        snprintf(hex, sizeof(hex), "%02x", intermediate_comp);
                        intermediate_color += hex;
                    }
                    
                    // Añadir el código de color y la letra
                    result += "&#";
                    result += intermediate_color;
                    result += gradient_text[i];
                }
            }
            
            // Saltar al final del texto del gradiente
            pos = text_end;
        }
        
        if(changes_made)
        {
            text = std::move(result);
        }
        
        return changes_made;
    }

    inline bool change_gradient2(std::string &text)
    {
        struct Text
        {
            int start; //start of the text
            int end; // end of the text
            std::string text; // text to modify
            int type; // type of text
            std::vector<std::string> colors; //colors
        };

        std::vector<Text> cases;
        size_t status = 0;

        while(status != std::string::npos)
        {
            status = text.find("&@gradient:\"", status);
            Text segment = {
                static_cast<int>(status),
                0,
                "",
                0,
                std::vector<std::string>()
            };
            status += 1;
            status = text.find("&@gradient:\"", status);
            status == std::string::npos ? 
                      segment.end = (text.size()-1) :
                      segment.end = status;
            segment.text = text.substr(segment.start, segment.end-segment.start);
            segment.type = std::stoi(segment.text.substr(12, 1)); //since &@gradient:\" has 13 words and we need 1 word
            /*
            type 0 || type -1 = default state and ignore
            type 1 = normal gradient
            type 2 = multiple gradient
            type 3 = gradient 1 but when reaching another &@gradient it blends with the start of it
            */
            switch(segment.type)
            {
                case 1: {//2 colors
                    if(segment.text.size() < 30) segment.type = -1; //&@gradient:"x:#00ff00:#00ff00"
                    segment.colors.emplace_back(segment.text.substr(15, 6));
                    segment.colors.emplace_back(segment.text.substr(23, 6));
                    // Buscar próximos códigos de color
                    //check the closing "
                    size_t content_start = segment.text.find("\"", 14) + 1;
                    if(content_start == std::string::npos) break;

                    // Buscar próximos códigos de color
                    size_t color_pos = segment.text.find("&", content_start);
                    if(color_pos != std::string::npos && color_pos < static_cast<size_t>(segment.end))
                    {
                        // Manejar &r
                        std::string code = segment.text.substr(color_pos, 2);
                        if(code == "&r")//reset color
                        {
                            segment.end = segment.end - (segment.end - color_pos);
                        }
                        // Manejar códigos hex
                        else if(code == "&#" && color_pos + 8 <= static_cast<size_t>(segment.end))
                        {
                            segment.end = segment.end - (segment.end - color_pos);
                        }
                        else if(color_pos + 1 < static_cast<size_t>(segment.end))
                        {
                            char type = code[code.size()-1];
                            if((type >= '0' && type <= '9') || 
                               (type >= 'a' && type <= 'f') || 
                               (type >= 'A' && type <= 'F'))
                            {
                                segment.end = segment.end - (segment.end - color_pos);
                            }
                        }
                    }
                }
                break;

                case 2: {
                    if(segment.text.size() < 22) segment.type = -1; //&@gradient:"x:#00ff00"
                    size_t end_pos = segment.text.find("\"", 14); //last position, if non, well, we skip it
                    if(end_pos == std::string::npos) end_pos = segment.text.size();
                    size_t pos = 15; //start of a color, without the #
                    while(pos < end_pos)
                    {
                        segment.colors.emplace_back(segment.text.substr(pos, 6));
                        pos += 8; //skipping the color and the :#
                    }
                    if(segment.colors.empty()) segment.type = -1;
                }
                break;

                case 3: {
                    if(segment.text.size() < 22)
                    {
                        segment.type = -1;
                        break;
                    }
                    //first color 
                    std::string first_color = segment.text.substr(15, 6);
                    segment.colors.emplace_back(first_color);
                    
                    //check the closing "
                    size_t content_start = segment.text.find("\"", 14) + 1;
                    if(content_start == std::string::npos) break;

                    // Buscar próximos códigos de color
                    size_t color_pos = segment.text.find("&", content_start);

                    if(color_pos != std::string::npos && color_pos < static_cast<size_t>(segment.end))
                    {
                        // Manejar &r
                        if(segment.text.substr(color_pos, 2) == "&r") //we found nothing
                        {
                            break;
                        }
                        // Manejar códigos hex
                        else if(segment.text.substr(color_pos, 2) == "&#" && color_pos + 8 <= static_cast<size_t>(segment.end))
                        {
                            segment.colors.emplace_back(segment.text.substr(color_pos + 2, 6));
                        }
                        // Manejar códigos de letra
                        else if(color_pos + 1 < static_cast<size_t>(segment.end))
                        {
                            segment.colors.emplace_back(
                                get_color(segment.text.substr(color_pos, 1))
                            );
                        }
                    }
                }
                break;

                default:
                    continue;
                break;
            }
            

            cases.emplace_back(segment);
        }

        //do the colors
        for(size_t i = cases.size()-1; i > 0; i--)
        {
            auto c = cases[i];
        }

        for(auto e : cases)
        {
            std::cout << e.start 
                      << " to " 
                      << e.end 
                      << " : " + text.substr(e.start, e.end-e.start+1)
                      << " : " + std::to_string(e.type)
                      << std::endl;
            for(auto e : e.colors)
            {
                std::cout << ", " + e << std::endl;
            }
        }
        return true;
    }

    inline bool change_gradient3(std::string &text)
    {
        struct Text {
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
            while (pos < str.size()) {
                if (str[pos] == '\\' && pos + 1 < str.size()) {
                    pos += 2;
                    continue;
                }
                
                if (str[pos] == '&') {
                    if (pos + 1 < str.size()) {
                        if (str[pos + 1] == 'r') {
                            return pos;
                        }
                        if (str[pos + 1] == '#' && pos + 8 <= str.size()) {
                            return pos;
                        }
                        std::string candidate = str.substr(pos, 2);
                        for (const auto& cmd : IGNORED_COMMANDS) {
                            if (candidate == cmd) {
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

        while (status != std::string::npos) {
            status = text.find("&@gradient:\"", status);
            if (status == std::string::npos) break;

            Text segment;
            segment.start = static_cast<int>(status);
            size_t end_pos = text.find("\"", status + 12);
            if (end_pos == std::string::npos) break;

            size_t next_gradient = text.find("&@gradient:\"", status + 1);
            segment.end = (next_gradient == std::string::npos) ? text.size() - 1 : next_gradient - 1;
            segment.text = text.substr(segment.start, segment.end - segment.start + 1);

            if (segment.text.size() < 15) {
                status = segment.end + 1;
                continue;
            }
            std::cout << segment.text.substr(12, 1) << std::flush;
            segment.type = std::stoi(segment.text.substr(12, 1));

            switch (segment.type) {
                case 1:
                    if (segment.text.size() < 30) {
                        segment.type = -1;
                        break;
                    }
                    segment.colors.emplace_back(segment.text.substr(15, 6));
                    segment.colors.emplace_back(segment.text.substr(23, 6));
                    break;
                case 2:
                    if (segment.text.size() < 22) {
                        segment.type = -1;
                        break;
                    }
                    {
                        size_t color_pos = 15;
                        while (color_pos + 6 <= segment.text.size()) {
                            segment.colors.emplace_back(segment.text.substr(color_pos, 6));
                            color_pos += 8;
                            if (color_pos >= segment.text.size() || segment.text[color_pos - 1] != '#') break;
                        }
                    }
                    break;
                case 3: {
                    if(segment.text.size() < 22)
                    {
                        segment.type = -1;
                        break;
                    }
                    //first color 
                    std::string first_color = segment.text.substr(15, 6);
                    segment.colors.emplace_back(first_color);
                    
                    //check the closing "
                    size_t content_start = segment.text.find("\"", 14) + 1;
                    if(content_start == std::string::npos) break;

                    // Buscar próximos códigos de color
                    size_t color_pos = segment.text.find("&", content_start);

                    if(color_pos != std::string::npos && color_pos < static_cast<size_t>(segment.end))
                    {
                        // Manejar &r
                        if(segment.text.substr(color_pos, 2) == "&r") //we found nothing
                        {
                            break;
                        }
                        // Manejar códigos hex
                        else if(segment.text.substr(color_pos, 2) == "&#" && color_pos + 8 <= static_cast<size_t>(segment.end))
                        {
                            segment.colors.emplace_back(segment.text.substr(color_pos + 2, 6));
                        }
                        // Manejar códigos de letra
                        else if(color_pos + 1 < static_cast<size_t>(segment.end))
                        {
                            segment.colors.emplace_back(
                                get_color(segment.text.substr(color_pos, 1))
                            );
                        }
                    }
                }
                break;
                default:
                    segment.type = -1;
                    break;
            }

            if (segment.type != -1) {
                cases.emplace_back(segment);
            }
            status = segment.end + 1;
        }

        // Procesar los casos desde el final hacia el inicio
        for (int i = cases.size() - 1; i >= 0; i--)
        {
            auto& c = cases[i];
            size_t content_start = c.text.find("\"", 12) + 1;
            if (content_start == std::string::npos) continue;

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
                for (size_t j = 0; j < length; j++) {
                    // Calculate position in gradient
                    float t = static_cast<float>(j) / (length - 1);
                    // Map to color segments
                    float segment = t * (c.colors.size() - 1);
                    int color_index = static_cast<int>(segment);
                    // Handle last color segment
                    if (static_cast<size_t>(color_index) >= c.colors.size() - 1) {
                        colored_text += "&#" + c.colors.back() + text_to_color[j];
                    } else {
                        float segment_t = segment - color_index;
                        std::string color = interpolate_color(
                            c.colors[color_index],
                            c.colors[color_index + 1],
                            segment_t
                        );
                        colored_text += "&#" + color + text_to_color[j];
                    }
                }
            }

            std::string new_segment = colored_text + remaining_text;
            text.replace(c.start, c.end - c.start + 1, new_segment);
        }

        return true;
    }

    inline std::string json_escape(const std::string& str)
    {
        std::ostringstream oss;
        for(char c : str)
        {
            switch (c)
            {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if(static_cast<unsigned char>(c) <= 0x1F)
                    {
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') 
                            << static_cast<int>(c);
                    }
                    else
                    {
                        oss << c;
                    }
            }
        }
        return oss.str();
    }

    inline std::string to_json(std::string& input, bool use_extra = false)
    {
        change_gradient3(input);
        if(!check_non_ftb(input)) return input;

        struct State
        {
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
            if(components.empty() && has_extra_properties(current_state))
            {
                components.push_back("{\"text\":\"\"}");
            }
            // Solo añadir propiedades si no son valores por defecto
            if(!current_state.color.empty() && current_state.color != "#FFFFFF") {
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

                    if(pos < input.size() && input[pos+1] == '"')
                    {
                        size_t end_quote = pos + 2;
                        while(end_quote < input.size())
                        {
                            if(input[end_quote] == '\\' && end_quote + 1 < input.size())
                            {
                                end_quote += 2;
                            }
                            else if(input[end_quote] == '"')
                            {
                                break;
                            }
                            else
                            {
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

}

#endif