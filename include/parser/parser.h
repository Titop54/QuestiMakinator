#ifndef SNBT_PARSER_H
#define SNBT_PARSER_H

#include <cctype>
#include <charconv>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>
#include <limits>
#include <cmath>


namespace snbt
{

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
    using List = std::vector<class Tag>;
    using Compound = std::map<String, Tag>;

    class Tag {
    public:
        // Supported tag types
        enum class Type {
            Byte, Short, Int, Long, Boolean,
            Float, Double,
            String,
            ByteArray, IntArray, LongArray,
            List, Compound
        };

        // Value storage using std::variant, aka union
        using Value = std::variant<
            Byte, Short, Int, Long, Boolean,
            Float, Double,
            String,
            ByteArray, IntArray, LongArray,
            List, Compound
        >;

        // Constructors
        Tag() : value_(Byte(0)) {}
        
        // Copy/move constructors
        Tag(const Tag&) = default;
        Tag(Tag&&) = default;
        
        // Constructor for supported types (excludes Tag and its references)
        template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Tag>>>
        Tag(T&& val) : value_(std::forward<T>(val)) {}
        
        // Assignment operators
        Tag& operator=(const Tag&) = default;
        Tag& operator=(Tag&&) = default;

        // Type access
        Type type() const noexcept {
            return static_cast<Type>(value_.index());
        }

        // Value access
        template <typename T>
        const T& as() const {
            return std::get<T>(value_);
        }

        template <typename T>
        T& as() {
            return std::get<T>(value_);
        }

        bool operator==(Tag const& obj) const
        {
            return this->type() == obj.type() && this->value_ == obj.value_;
        }

    private:
        Value value_;
    };

    // Parser class
    class Parser {
    public:
        explicit Parser(std::string_view input) 
            : input_(input), pos_(0) {}

        Tag parse() {
            auto tag = parseValue();
            skipWhitespace();
            if (!atEnd()) {
                throw ParseError("Unexpected trailing characters");
            }
            return tag;
        }

    private:
        // Token types for the lexer
        enum class TokenType {
            LeftBrace, RightBrace,   // { }
            LeftBracket, RightBracket, // [ ]
            Colon, Comma, Semicolon,  // : , ;
            String, Number, Boolean,  // "text", 123, true/false
            End, Error
        };

        struct Token {
            TokenType type;
            std::string_view lexeme;
        };

        std::string_view input_;
        size_t pos_;

        // Utility functions
        char current() const noexcept {
            return pos_ < input_.size() ? input_[pos_] : '\0';
        }

        char advance() noexcept {
            return pos_ < input_.size() ? input_[pos_++] : '\0';
        }

        bool atEnd() const noexcept {
            return pos_ >= input_.size();
        }

        void skipWhitespace() noexcept {
            while (!atEnd() && std::isspace(static_cast<unsigned char>(current()))) {
                advance();
            }
        }

        bool match(char c) noexcept {
            if (!atEnd() && current() == c) {
                advance();
                return true;
            }
            return false;
        }

        // Lexer functions
        Token nextToken() {
            skipWhitespace();
            if (atEnd()) return {TokenType::End, ""};

            char c = current();
            switch (c) {
                case '{': advance(); return {TokenType::LeftBrace, "{"};
                case '}': advance(); return {TokenType::RightBrace, "}"};
                case '[': advance(); return {TokenType::LeftBracket, "["};
                case ']': advance(); return {TokenType::RightBracket, "]"};
                case ':': advance(); return {TokenType::Colon, ":"};
                case ',': advance(); return {TokenType::Comma, ","};
                case ';': advance(); return {TokenType::Semicolon, ";"};
                case '"': return parseQuotedString('"');
                case '\'': return parseQuotedString('\'');
                default:
                    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.') {
                        return parseNumber();
                    }
                    if (std::isalpha(static_cast<unsigned char>(c))) {
                        return parseIdentifier();
                    }
                    return {TokenType::Error, "Invalid token"};
            }
        }

        Token parseQuotedString(char quote) {
            advance(); // consume opening quote
            size_t start = pos_;
            while (!atEnd() && current() != quote) {
                if (current() == '\\') advance(); // handle escape
                advance();
            }
            if (atEnd()) return {TokenType::Error, "Unterminated string"};

            std::string_view lexeme = input_.substr(start, pos_ - start);
            advance(); // consume closing quote
            return {TokenType::String, lexeme};
        }

        Token parseNumber() {
            size_t start = pos_;

            // Optional sign
            if (current() == '-' || current() == '+') advance();

            // Integer part
            while (std::isdigit(static_cast<unsigned char>(current()))) advance();

            // Fraction part
            if (current() == '.') {
                advance();
                while (std::isdigit(static_cast<unsigned char>(current()))) advance();
            }

            // Exponent part
            if (current() == 'e' || current() == 'E') {
                advance();
                if (current() == '+' || current() == '-') advance();
                while (std::isdigit(static_cast<unsigned char>(current()))) advance();
            }

            // Suffix (type specifier) - case insensitive
            char suffix = current();
            if (suffix == 'b' || suffix == 'B' || 
                suffix == 's' || suffix == 'S' || 
                suffix == 'l' || suffix == 'L' || 
                suffix == 'f' || suffix == 'F' || 
                suffix == 'd' || suffix == 'D') {
                advance();
            }

            return {TokenType::Number, input_.substr(start, pos_ - start)};
        }

        Token parseIdentifier() {
            size_t start = pos_;
            while (std::isalnum(static_cast<unsigned char>(current())) || 
                   current() == '_' || current() == '-' || current() == '+')
            {
                advance();
            }
            std::string_view lexeme = input_.substr(start, pos_ - start);
            
            if (lexeme == "true" || lexeme == "false") {
                return {TokenType::Boolean, lexeme};
            }
            return {TokenType::String, lexeme}; // unquoted string
        }

        // Parser functions
        Tag parseValue() {
            Token token = nextToken();
            switch (token.type) {
                case TokenType::LeftBrace:   return parseCompound();
                case TokenType::LeftBracket: return parseList();
                case TokenType::String:      return parseString(token.lexeme);
                case TokenType::Number:      return parseNumber(token.lexeme);
                case TokenType::Boolean:     return parseBoolean(token.lexeme);
                default:
                    throw ParseError("Unexpected token in value");
            }
        }

        Tag parseString(std::string_view lexeme) {
            return Tag{String(lexeme)};
        }

        Tag parseNumber(std::string_view lexeme) {
            // Determine suffix if present (case insensitive)
            char suffix = lexeme.empty() ? '\0' : lexeme.back();
            char lowerSuffix = std::tolower(static_cast<unsigned char>(suffix));
            std::string_view numStr = lexeme;
            
            if (lowerSuffix == 'b' || lowerSuffix == 's' || lowerSuffix == 'l' || 
                lowerSuffix == 'f' || lowerSuffix == 'd') {
                numStr = lexeme.substr(0, lexeme.size() - 1);
                suffix = lowerSuffix;
            } else {
                suffix = '\0';
            }

            // Parse based on suffix
            switch (suffix) {
                case 'b': return parseInteger<Byte>(numStr);
                case 's': return parseInteger<Short>(numStr);
                case 'l': return parseInteger<Long>(numStr);
                case 'f': return parseFloat<Float>(numStr);
                case 'd': return parseFloat<Double>(numStr);
                default: {
                    // No suffix - determine type automatically
                    if (lexeme.find_first_of(".eE") != std::string_view::npos) {
                        return parseFloat<Double>(numStr);
                    }
                    try {
                        return parseInteger<Int>(numStr);
                    } catch (const std::range_error&) {
                        return parseInteger<Long>(numStr);
                    }
                }
            }
        }

        template <typename T>
        Tag parseInteger(std::string_view str) {
            T value;
            auto result = std::from_chars(str.data(), str.data() + str.size(), value);
            if (result.ec != std::errc() || result.ptr != str.data() + str.size()) {
                throw ParseError("Invalid integer format: " + std::string(str));
            }
            return Tag{value};
        }

        template <typename T>
        Tag parseFloat(std::string_view str) {
            try {
                if constexpr (std::is_same_v<T, Float>) {
                    return Tag{std::stof(std::string(str))};
                } else {
                    return Tag{std::stod(std::string(str))};
                }
            } catch (...) {
                throw ParseError("Invalid float format: " + std::string(str));
            }
        }

        Tag parseBoolean(std::string_view lexeme) {
            if (lexeme == "true") return Tag{true};
            if (lexeme == "false") return Tag{false};
            throw ParseError("Invalid boolean: " + std::string(lexeme));
        }

        Tag parseCompound() {
            Compound comp;
            while (true) {
                skipWhitespace();
                if (match('}')) break;

                // Parse key
                Token keyToken = nextToken();
                if (keyToken.type != TokenType::String) {
                    throw ParseError("Expected string key in compound");
                }
                std::string key(keyToken.lexeme);

                // Parse colon
                if (nextToken().type != TokenType::Colon) {
                    throw ParseError("Expected colon after key");
                }

                // Parse value
                comp.emplace(std::move(key), parseValue());

                // Check for comma or terminator
                skipWhitespace();
                if (match('}')) break;
                
                // Handle optional comma
                if (match(',')) {
                    skipWhitespace();
                }
            }
            return Tag{std::move(comp)};
        }

        Tag parseList() {
            skipWhitespace();
            // Check for typed arrays ([B; ...], [I; ...], [L; ...])
            if (!atEnd() && input_[pos_] == 'B') {
                return parseByteArray();
            } else if (!atEnd() && input_[pos_] == 'I') {
                return parseIntArray();
            } else if (!atEnd() && input_[pos_] == 'L') {
                return parseLongArray();
            }

            // Regular list
            List list;
            while (true) {
                skipWhitespace();
                if (match(']')) break;
                list.push_back(parseValue());
                skipWhitespace();
                if (match(']')) break;
                
                // Handle optional comma
                if (match(',')) {
                    skipWhitespace();
                }
            }
            return Tag{std::move(list)};
        }

        Tag parseByteArray() {
            advance(); // consume 'B'
            if (nextToken().type != TokenType::Semicolon) {
                throw ParseError("Expected semicolon in byte array");
            }

            ByteArray arr;
            while (true) {
                skipWhitespace();
                if (match(']')) break;

                Token token = nextToken();
                if (token.type != TokenType::Number) {
                    throw ParseError("Expected number in byte array");
                }
                arr.push_back(parseNumber(token.lexeme).as<Byte>());

                skipWhitespace();
                if (match(']')) break;
                
                // Handle optional comma
                if (match(',')) {
                    skipWhitespace();
                }
            }
            return Tag{std::move(arr)};
        }

        Tag parseIntArray() {
            advance(); // consume 'I'
            if (nextToken().type != TokenType::Semicolon) {
                throw ParseError("Expected semicolon in int array");
            }

            IntArray arr;
            while (true) {
                skipWhitespace();
                if (match(']')) break;

                Token token = nextToken();
                if (token.type != TokenType::Number) {
                    throw ParseError("Expected number in int array");
                }
                
                try {
                    // Parse as 32-bit integer
                    Int value = parseIntegerValue<Int>(token.lexeme);
                    arr.push_back(value);
                } catch (const ParseError& e) {
                    throw ParseError(std::string(e.what()) + " in int array");
                }

                skipWhitespace();
                if (match(']')) break;
                
                // Handle optional comma
                if (match(',')) {
                    skipWhitespace();
                }
            }
            return Tag{std::move(arr)};
        }

        Tag parseLongArray() {
            advance(); // consume 'L'
            if (nextToken().type != TokenType::Semicolon) {
                throw ParseError("Expected semicolon in long array");
            }

            LongArray arr;
            while (true) {
                skipWhitespace();
                if (match(']')) break;

                Token token = nextToken();
                if (token.type != TokenType::Number) {
                    throw ParseError("Expected number in long array");
                }
                
                try {
                    // Parse as 64-bit integer
                    Long value = parseIntegerValue<Long>(token.lexeme);
                    arr.push_back(value);
                } catch (const ParseError& e) {
                    throw ParseError(std::string(e.what()) + " in long array");
                }

                skipWhitespace();
                if (match(']')) break;
                
                // Handle optional comma
                if (match(',')) {
                    skipWhitespace();
                }
            }
            return Tag{std::move(arr)};
        }

        // Helper for safe integer parsing
        template <typename T>
        T parseIntegerValue(std::string_view str) {
            int64_t value;
            auto result = std::from_chars(str.data(), str.data() + str.size(), value);
            
            if (result.ec == std::errc::invalid_argument) {
                throw ParseError("Invalid integer: " + std::string(str));
            } else if (result.ec == std::errc::result_out_of_range) {
                throw ParseError("Integer out of range: " + std::string(str));
            } else if (result.ptr != str.data() + str.size()) {
                throw ParseError("Unexpected characters in integer: " + std::string(str));
            }
            
            if (value < std::numeric_limits<T>::min() || 
                value > std::numeric_limits<T>::max()) {
                throw ParseError("Integer out of range for type");
            }
            
            return static_cast<T>(value);
        }

        // Exception class
        class ParseError : public std::runtime_error {
        public:
            using std::runtime_error::runtime_error;
        };
    };

    namespace detail {
        inline std::string escapeString(std::string_view str) {
            std::string result;
            result.reserve(str.length() + 2);
            result += '"';
            for (char c : str) {
                switch (c) {
                    case '"':  result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
                            char buf[7];
                            std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned char>(c));
                            result += buf;
                        } else {
                            result += c;
                        }
                }
            }
            result += '"';
            return result;
        }

        inline std::string formatDouble(double value) {
            char buffer[100];
            int len = std::snprintf(buffer, sizeof(buffer), "%.17g", value);
            if (len < 0 || static_cast<size_t>(len) >= sizeof(buffer)) {
                return std::to_string(value);
            }
            std::string s = buffer;
            if (s.find('.') == std::string::npos && 
                s.find('e') == std::string::npos && 
                s.find('E') == std::string::npos) {
                s += ".0";
            }
            return s;
        }

        inline std::string makeIndent(int level) {
            return std::string(static_cast<size_t>(level) * 4, ' ');
        }
    } // namespace detail

    inline std::string to_string(const Tag& tag, int indent = 0) {
        using Type = Tag::Type;
        
        switch (tag.type()) {
            case Type::Byte:
                return std::to_string(tag.as<Byte>()) + "b";
                
            case Type::Short:
                return std::to_string(tag.as<Short>()) + "s";
                
            case Type::Int:
                return std::to_string(tag.as<Int>());
                
            case Type::Long:
                return std::to_string(tag.as<Long>()) + "l";
                
            case Type::Boolean:
                return tag.as<Boolean>() ? "true" : "false";
                
            case Type::Float: {
                char buffer[100];
                int len = std::snprintf(buffer, sizeof(buffer), "%.9g", tag.as<Float>());
                if (len < 0 || static_cast<size_t>(len) >= sizeof(buffer)) {
                    return std::to_string(tag.as<Float>()) + "f";
                }
                std::string s = buffer;
                if (s.find('.') == std::string::npos && 
                    s.find('e') == std::string::npos && 
                    s.find('E') == std::string::npos) {
                    s += ".0";
                }
                return s + "f";
            }
                
            case Type::Double:
                return detail::formatDouble(tag.as<Double>());
                
            case Type::String:
                return detail::escapeString(tag.as<String>());
                
            case Type::ByteArray: {
                const auto& arr = tag.as<ByteArray>();
                if (arr.empty()) return "[B;]";
                
                std::string result = "[B;";
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i > 0) result += ',';
                    result += std::to_string(arr[i]) + 'b';
                }
                result += ']';
                return result;
            }
                
            case Type::IntArray: {
                const auto& arr = tag.as<IntArray>();
                if (arr.empty()) return "[I;]";
                
                std::string result = "[I;";
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i > 0) result += ',';
                    result += std::to_string(arr[i]);
                }
                result += ']';
                return result;
            }
                
            case Type::LongArray: {
                const auto& arr = tag.as<LongArray>();
                if (arr.empty()) return "[L;]";
                
                std::string result = "[L;";
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i > 0) result += ',';
                    result += std::to_string(arr[i]) + 'l';
                }
                result += ']';
                return result;
            }
                
            case Type::List: {
                const auto& list = tag.as<List>();
                if (list.empty()) return "[]";
                
                if (indent == 0) {
                    std::string result = "[";
                    for (size_t i = 0; i < list.size(); ++i) {
                        if (i > 0) result += ',';
                        result += to_string(list[i], 0);
                    }
                    result += ']';
                    return result;
                } else {
                    std::string result = "[\n";
                    std::string outerIndent = detail::makeIndent(indent);
                    std::string innerIndent = detail::makeIndent(indent + 1);
                    
                    for (size_t i = 0; i < list.size(); ++i) {
                        if (i > 0) result += ",\n";
                        result += innerIndent;
                        result += to_string(list[i], indent + 1);
                    }
                    result += '\n' + outerIndent + ']';
                    return result;
                }
            }
                
            case Type::Compound: {
                const auto& comp = tag.as<Compound>();
                if (comp.empty()) return "{}";
                
                if (indent == 0) {
                    std::string result = "{";
                    for (const auto& [key, value] : comp) {
                        result += detail::escapeString(key);
                        result += ':';
                        result += to_string(value, 0);
                    }
                    result += '}';
                    return result;
                } else {
                    std::string result = "{\n";
                    std::string outerIndent = detail::makeIndent(indent);
                    std::string innerIndent = detail::makeIndent(indent + 1);
                    size_t count = 0;
                    
                    for (const auto& [key, value] : comp) {
                        if (count++ > 0) result += "\n";
                        result += innerIndent;
                        result += detail::escapeString(key);
                        result += ": ";
                        result += to_string(value, indent + 1);
                    }
                    result += '\n' + outerIndent + '}';
                    return result;
                }
            }
                
            default:
                return "unknown";
        }
    }


} // namespace snbt


#endif
