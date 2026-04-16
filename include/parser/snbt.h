#pragma once

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>

namespace snbt
{

    class Tag;

    using Byte = int8_t;
    using Boolean = bool;
    using Short = int16_t;
    using Int = int32_t;
    using Long = int64_t;
    using Float = float;
    using Double = double;
    using String = std::string;
    using ByteArray = std::vector<Byte>;
    using IntArray = std::vector<Int>;
    using LongArray = std::vector<Long>;
    
    using List = std::vector<Tag>;
    using Compound = std::map<String, Tag>;

    class Tag {
    public:
        enum class Type {
            End = 0,
            Byte, Short, Int, Long, Boolean,
            Float, Double,
            String,
            ByteArray, IntArray, LongArray,
            List, Compound
        };


        using Value = std::variant<
            std::monostate, // End
            Byte,           // Type::Byte
            Short,          // Type::Short
            Int,            // Type::Int
            Long,           // Type::Long
            Boolean,        // Type::Boolean
            Float,          // Type::Float
            Double,         // Type::Double
            String,         // Type::String
            ByteArray,      // Type::ByteArray
            IntArray,       // Type::IntArray
            LongArray,      // Type::LongArray
            List,           // Type::List
            Compound        // Type::Compound
        >;

        Type type = Type::End;
        Value value = std::monostate{};

        explicit Tag() {};
        explicit Tag(Byte v) : type(Type::Byte), value(v) {}
        explicit Tag(Short v) : type(Type::Short), value(v) {}
        explicit Tag(Int v) : type(Type::Int), value(v) {}
        explicit Tag(Long v) : type(Type::Long), value(v) {}
        explicit Tag(Boolean v) : type(Type::Boolean), value(v) {}
        explicit Tag(Float v) : type(Type::Float), value(v) {}
        explicit Tag(Double v) : type(Type::Double), value(v) {}
        explicit Tag(const String& v) : type(Type::String), value(v) {}
        explicit Tag(const ByteArray& v) : type(Type::ByteArray), value(v) {}
        explicit Tag(const IntArray& v) : type(Type::IntArray), value(v) {}
        explicit Tag(const LongArray& v) : type(Type::LongArray), value(v) {}
        explicit Tag(const List& v) : type(Type::List), value(v) {}
        explicit Tag(const Compound& v) : type(Type::Compound), value(v) {}

        void print(std::ostream& os = std::cout, int indent = 0, bool insertComma = false) const
        {
            std::string pad(indent, ' ');
            switch(type)
            {
                case Type::Compound:
                {
                    const auto& map = std::get<Compound>(value);
                    os << "{\n";
                    size_t counter = 0;
                    for(const auto& [key, tag] : map)
                    {
                        os << pad << key << ": ";
                        tag.print(os, indent, insertComma);
                        counter++;
                        if(insertComma)
                        {
                            if(counter < map.size())
                            {
                                os << ",";
                            }
                        }
                        os << "\n";
                    }
                    os << "}";
                    break;
                }

                case Type::List:
                {
                    const auto& list = std::get<List>(value);
                    os << "[";
                    size_t counter = 0;

                    if(list.size() > 1)
                    {
                        os << "\n";
                    }

                    for(const auto& tag : list)
                    {
                        if(list.size() > 1)
                        {
                            os << pad << pad;
                        }

                        tag.print(os, indent);
                        if(insertComma) 
                        {
                            if(counter < list.size())
                            {
                                os << ",";
                            }
                        }

                        if(list.size() > 1)
                        {
                            os << "\n";
                        }
                        os.flush();
                    }
                    if(list.size() > 1)
                    {
                        os << pad;
                    }
                    os << "]";
                    break;
                }

                case Type::String: 
                {
                    os << "\"";
                    std::string raw = std::get<String>(value);
                    for(char c : raw)
                    {
                        if(c == '"')
                        {
                            os << "\\\"";
                        }
                        else if(c == '\\')
                        {
                            os << "\\\\";
                        }
                        else if(c == '\n')
                        {
                            os << "\\n";
                        }
                        else
                        {
                            os << c;
                        }
                    }
                    os << "\""; 
                    break;
                }
                case Type::Byte: os << (int)std::get<Byte>(value) << "b"; break;
                case Type::Short: os << std::get<Short>(value) << "s"; break;
                case Type::Int: os << std::get<Int>(value); break;
                case Type::Long: os << std::get<Long>(value) << "L"; break;
                case Type::Float: os << std::get<Float>(value) << "f"; break;
                case Type::Double: os << std::get<Double>(value) << "d"; break;
                case Type::Boolean: os << (std::get<Boolean>(value) ? "true" : "false"); break;
                case Type::ByteArray: os << "[B; ...]"; break; 
                case Type::IntArray: os << "[I; ...]"; break;
                case Type::LongArray: os << "[L; ...]"; break;
                default: os << "Unknown"; break;
            }
        }

        bool empty() const
        {
            return type == Type::End;
        }

        template<typename T>
        T cast()
        {
            try
            {
                return std::get<T>(value);
            }
            catch(...)
            {
                return {};
            }
        }
    };

    class SnbtParser
    {
    private:
        std::string source = "";
        Tag tag;
        size_t cursor = 0;

    public:
        SnbtParser(const std::string& input, bool isFile = false)
        {
            tag = parse(input, isFile);
        }

        SnbtParser()
        {
        }

        SnbtParser(const std::filesystem::directory_entry entry)
        {
            tag = parse(entry.path().string(), true);
        }

        Tag parse(const std::string& input, bool isFile = false)
        {
            if(isFile)
            {
                std::ifstream file(input);
                if(!file.is_open())
                {
                    file.close();
                    close();
                    Tag t;
                    return t;

                }
                std::string content((std::istreambuf_iterator<char>(file)),
                                    (std::istreambuf_iterator<char>()));
                file.close();
                source = content;
            }
            else
            {
                source = input;
            }

            return parse();
        }

        bool has_content() const
        {
            return !tag.empty();
        }

        bool empty() const
        {
            return tag.empty();
        }

        void close()
        {
            cursor = 0;
            source.clear();
            source.shrink_to_fit();
        }

        Tag get_tag() const
        {
            return tag;
        }

    private:

        Tag parse()
        {
            skipWhitespace();
            Tag result = parseValue();
            skipWhitespace();
            if(cursor < source.length())
            {
                reportError("Unexpected trailing characters");
            }
            source.clear();
            source.shrink_to_fit();
            return result;
        }

        char peek(int offset = 0) const
        {
            if(cursor + offset >= source.length()) return '\0';
            return source[cursor + offset];
        }

        char advance()
        {
            if (cursor >= source.length()) return '\0';
            return source[cursor++];
        }

        void skipWhitespace()
        {
            while(cursor < source.length() && std::isspace(source[cursor]))
            {
                cursor++;
            }
        }

        bool match(char expected)
        {
            if(peek() == expected)
            {
                cursor++;
                return true;
            }
            return false;
        }

        [[noreturn]] void reportError(const std::string& message)
        {
            size_t lineStart = cursor;
            while(lineStart > 0 && source[lineStart - 1] != '\n')
            {
                lineStart--;
            }

            size_t lineEnd = cursor;
            while(lineEnd < source.length() && source[lineEnd] != '\n')
            {
                lineEnd++;
            }

            std::string contextLine = source.substr(lineStart, lineEnd - lineStart);
            std::string pointer(cursor - lineStart, ' ');
            pointer += "^";

            std::stringstream ss;
            ss << "\nSNBT Parsing Error: " << message << "\n";
            ss << contextLine << "\n";
            ss << pointer << "\n";

            throw std::runtime_error(ss.str());
        }

        Tag parseValue()
        {
            skipWhitespace();
            char c = peek();

            if (c == '{') return parseCompound();
            if (c == '[') return parseListOrArray();
            if (c == '"' || c == '\'') return parseQuotedString();
            
            return parseUnquotedStringOrNumber();
        }

        Tag parseCompound()
        {
            match('{');
            Compound map;
            
            while(true)
            {
                skipWhitespace();
                if(match('}')) break;

                String key;
                char c = peek();
                if(c == '"' || c == '\'')
                {
                    key = std::get<String>(parseQuotedString().value);
                }
                else
                {
                    key = readUnquotedString();
                    if(key.empty()) reportError("Expected key");
                }

                skipWhitespace();
                if(!match(':')) reportError("Expected ':' after key");
                
                // Parse Value
                map[key] = parseValue();

                skipWhitespace();
                if(match('}')) break;
                match(',');
            }
            return Tag(map);
        }

        Tag parseListOrArray()
        {
            match('[');
            skipWhitespace();

            // Check for Array types: B; I; L;
            if(source.length() > cursor + 2 && source[cursor + 1] == ';')
            {
                char typeChar = std::toupper(peek());
                if(typeChar == 'B')
                { 
                    cursor += 2;
                    return parseByteArray();
                }
                if(typeChar == 'I')
                { 
                    cursor += 2;
                    return parseIntArray();
                }
                if(typeChar == 'L')
                { 
                    cursor += 2;
                    return parseLongArray();
                }
            }

            List list;
            while(true)
            {
                skipWhitespace();
                if(match(']')) break;

                list.push_back(parseValue());

                skipWhitespace();
                if(match(']')) break;
                match(',');
            }
            return Tag(list);
        }

        Tag parseByteArray()
        {
            ByteArray arr;
            while(true)
            {
                skipWhitespace();
                if(match(']')) break;

                Tag valTag = parseValue();
                if(valTag.type != Tag::Type::Byte) reportError("Expected Byte (suffix 'b') in Byte Array");
                arr.push_back(std::get<Byte>(valTag.value));

                skipWhitespace();
                if(match(']')) break;
                match(',');
            }
            return Tag(arr);
        }

        Tag parseIntArray()
        {
            IntArray arr;
            while(true)
            {
                skipWhitespace();
                if(match(']')) break;

                Tag valTag = parseValue();
                if(valTag.type != Tag::Type::Int) reportError("Expected Int in Int Array");
                arr.push_back(std::get<Int>(valTag.value));

                skipWhitespace();
                if(match(']')) break;
                match(',');
            }
            return Tag(arr);
        }

        Tag parseLongArray()
        {
            LongArray arr;
            while(true)
            {
                skipWhitespace();
                if(match(']')) break;

                Tag valTag = parseValue();
                if(valTag.type != Tag::Type::Long) reportError("Expected Long (suffix 'L') in Long Array");
                arr.push_back(std::get<Long>(valTag.value));

                skipWhitespace();
                if(match(']')) break;
                match(',');
            }
            return Tag(arr);
        }

        Tag parseQuotedString()
        {
            char quoteType = advance(); // " or '
            String res;
            while(cursor < source.length())
            {
                char c = advance();
                if(c == '\\')
                {
                    if(cursor >= source.length()) reportError("Unexpected end of string in escape");
                    char esc = advance();
                    if(esc == 'n') res += '\n';
                    else if(esc == '"') res += '"';
                    else if(esc == '\'') res += '\'';
                    else if(esc == '\\') res += '\\';
                    else res += esc; 
                }
                else if (c == quoteType)
                {
                    return Tag(res);
                }
                else
                {
                    res += c;
                }
            }
            reportError("Unterminated string");
        }

        String readUnquotedString()
        {
            size_t start = cursor;
            while(cursor < source.length())
            {
                char c = peek();
                if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                   (c >= '0' && c <= '9') || c == '_' || c == '-' || 
                    c == '.' || c == '+')
                {
                    cursor++;
                }
                else
                {
                    break;
                }
            }
            return source.substr(start, cursor - start);
        }

        Tag parseUnquotedStringOrNumber()
        {
            String raw = readUnquotedString();
            if(raw.empty()) reportError("Expected value");

            if(raw == "true") return Tag(true);
            if(raw == "false") return Tag(false);

            try
            {
                char suffix = raw.back();
                char lowerSuffix = std::tolower(suffix);
                bool hasSuffix = (lowerSuffix == 'b' || lowerSuffix == 's' || 
                                  lowerSuffix == 'l' || lowerSuffix == 'f' || 
                                  lowerSuffix == 'd');
                
                String numPart = hasSuffix ? raw.substr(0, raw.size() - 1) : raw;
                
                bool isNumeric = true;
                bool hasDecimal = false;
                for(char c : numPart)
                {
                    if(!isdigit(c) && c != '-' && c != '+' && c != '.' && c != 'e' && c != 'E')
                    {
                        isNumeric = false;
                        break;
                    }
                    if(c == '.') hasDecimal = true;
                }

                if(!isNumeric) return Tag(raw);

                if(lowerSuffix == 'b') return Tag((Byte)std::stoi(numPart));
                if(lowerSuffix == 's') return Tag((Short)std::stoi(numPart));
                if(lowerSuffix == 'l') return Tag((Long)std::stoll(numPart));
                if(lowerSuffix == 'f') return Tag((Float)std::stof(numPart));
                if(lowerSuffix == 'd') return Tag((Double)std::stod(numPart));

                if(hasDecimal)
                {
                    return Tag((Double)std::stod(numPart));
                }
                else
                {
                    return Tag((Int)std::stoi(numPart));
                }
            }
            catch(...)
            {
                return Tag(raw);
            }
        }
    };

}