#ifndef INTEGRATED_DOCUMENT_PARSER_H
#define INTEGRATED_DOCUMENT_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>
#include <memory>
#include <optional>
#include <any>
#include <functional>
#include <algorithm>
#include <regex>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stack>
#include <queue>
#include <set>
#include <chrono>
#include <iomanip>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <locale>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <string_view>
#include <unordered_set>

//=============================================================================
// Entity Type Enumeration - Expanded for all formats
//=============================================================================
enum class EntityType {
    // Core types
    Null, Bool, Int, Double, String, Array, Object,
    
    // Document types
    Document, Root, Element, Attribute, Text, Comment, CDATA,
    
    // XML specific
    XMLDocument, XMLElement, XMLAttribute, XMLComment, XMLCDATA,
    XMLProcessingInstruction, XMLDoctype, XMLNamespace,
    
    // XSL specific
    XSLDocument, XSLTemplate, XSLApply, XSLForEach, XSLIf, XSLChoose,
    XSLWhen, XSLOtherwise, XSLValueOf, XSLCopy, XSLCopyOf,
    XSLVariable, XSLParam, XSLCallTemplate, XSLWithParam,
    XSLAttribute, XSLElement, XSLText, XSLComment,
    
    // HTML specific
    HTMLDocument, HTMLDoctype, HTMLElement, HTMLAttribute, HTMLComment,
    HTMLScript, HTMLStyle, HTMLAnchor, HTMLImage, HTMLForm, HTMLInput,
    HTMLButton, HTMLTable, HTMLDiv, HTMLSpan, HTMLHeading, HTMLParagraph,
    HTMLList, HTMLLink,
    
    // JSON specific
    JSONDocument, JSONObject, JSONArray, JSONKey, JSONValue,
    
    // YAML specific
    YAMLDocument, YAMLMapping, YAMLSequence, YAMLScalar, YAMLKey,
    
    // CSV specific
    CSVDocument, CSVRow, CSVField, CSVHeader,

    // CSS specific
    CSSDocument, CSSRule, CSSDeclaration,
    
    // Date/Time specific
    DateTime, Date, Time, Timestamp, Duration,
    
    // Numeric types
    Integer, Floating, Scientific,
    
    // Common attributes
    ID, Class, Style, HRef, Src, Alt, Title,
    
    // Custom
    Custom, Unknown
};

//=============================================================================
// Typed Value - Supports automatic type detection and conversion
//=============================================================================
class TypedValue {
public:
    enum class ValueType {
        Null, Boolean, Integer, Double, String, DateTime, Date, Time, Duration
    };
    
    TypedValue() : m_type(ValueType::Null) {}
    TypedValue(bool value) : m_type(ValueType::Boolean), m_bool(value) {}
    TypedValue(int value) : m_type(ValueType::Integer), m_int(value) {}
    TypedValue(long long value) : m_type(ValueType::Integer), m_int(value) {}
    TypedValue(double value) : m_type(ValueType::Double), m_double(value) {}
    TypedValue(const std::string& value) : m_type(ValueType::String), m_string(value) {}
    TypedValue(const char* value) : m_type(ValueType::String), m_string(value) {}
    
    // Date/Time constructors
    TypedValue(const std::chrono::system_clock::time_point& value) 
        : m_type(ValueType::DateTime), m_timePoint(value) {}
    TypedValue(const std::chrono::sys_days& value)
        : m_type(ValueType::Date), m_date(value) {}
    TypedValue(const std::chrono::duration<double>& value)
        : m_type(ValueType::Duration), m_duration(value) {}
    
    // Type detection from string
    static TypedValue detect(const std::string& str);
    
    // Type conversion methods
    bool asBool() const {
        if (m_type == ValueType::Boolean) return m_bool;
        if (m_type == ValueType::Integer) return m_int != 0;
        if (m_type == ValueType::Double) return m_double != 0.0;
        if (m_type == ValueType::String) {
            std::string lower = m_string;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return lower == "true" || lower == "yes" || lower == "1";
        }
        return false;
    }
    
    long long asInt() const {
        if (m_type == ValueType::Integer) return m_int;
        if (m_type == ValueType::Double) return static_cast<long long>(m_double);
        if (m_type == ValueType::Boolean) return m_bool ? 1 : 0;
        if (m_type == ValueType::String) {
            try {
                return std::stoll(m_string);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }
    
    double asDouble() const {
        if (m_type == ValueType::Double) return m_double;
        if (m_type == ValueType::Integer) return static_cast<double>(m_int);
        if (m_type == ValueType::Boolean) return m_bool ? 1.0 : 0.0;
        if (m_type == ValueType::String) {
            try {
                return std::stod(m_string);
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }
    
    std::string asString() const;
    
    std::chrono::system_clock::time_point asDateTime() const {
        if (m_type == ValueType::DateTime) return m_timePoint;
        if (m_type == ValueType::Date) return m_date;
        return std::chrono::system_clock::now();
    }
    
    ValueType type() const { return m_type; }
    
private:
    ValueType m_type;
    bool m_bool = false;
    long long m_int = 0;
    double m_double = 0.0;
    std::string m_string;
    std::chrono::system_clock::time_point m_timePoint;
    std::chrono::sys_days m_date;
    std::chrono::duration<double> m_duration;
    std::chrono::duration<double> m_timeOfDay;
    
    friend class DateTimeParser;
};

//=============================================================================
// DateTimeParser - Comprehensive date/time parsing
//=============================================================================
class DateTimeParser {
public:
    struct DateTimeResult {
        std::chrono::system_clock::time_point value;
        std::chrono::sys_days date;
        std::chrono::duration<double> time;
        bool valid = false;
        bool isDateOnly = false;
        bool isTimeOnly = false;
        std::string format;
    };
    
    static DateTimeResult parse(const std::string& text) {
        DateTimeResult result;
        std::string trimmed = trim(text);
        
        // Try ISO 8601 format first
        if (parseISO8601(trimmed, result)) return result;
        
        // Try common date formats
        std::vector<std::pair<std::string, std::string>> dateFormats = {
            {"%Y-%m-%d", R"(\d{4}-\d{2}-\d{2})"},
            {"%Y/%m/%d", R"(\d{4}/\d{2}/\d{2})"},
            {"%m/%d/%Y", R"(\d{2}/\d{2}/\d{4})"},
            {"%d/%m/%Y", R"(\d{2}/\d{2}/\d{4})"},
            {"%B %d, %Y", R"([A-Za-z]+ \d{1,2}, \d{4})"},
            {"%b %d, %Y", R"([A-Za-z]{3} \d{1,2}, \d{4})"},
            {"%d %B %Y", R"(\d{1,2} [A-Za-z]+ \d{4})"},
            {"%d %b %Y", R"(\d{1,2} [A-Za-z]{3} \d{4})"},
            {"%Y-%m-%d", R"(\d{4}-\d{2}-\d{2})"},
            {"%Y%m%d", R"(\d{8})"}
        };
        
        // Try date + time formats
        std::vector<std::pair<std::string, std::string>> dateTimeFormats = {
            {"%Y-%m-%d %H:%M:%S", R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})"},
            {"%Y-%m-%d %H:%M", R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2})"},
            {"%Y-%m-%dT%H:%M:%S", R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})"},
            {"%Y-%m-%dT%H:%M:%SZ", R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)"},
            {"%m/%d/%Y %H:%M:%S", R"(\d{2}/\d{2}/\d{4} \d{2}:\d{2}:\d{2})"},
            {"%d/%m/%Y %H:%M:%S", R"(\d{2}/\d{2}/\d{4} \d{2}:\d{2}:\d{2})"}
        };
        
        std::tm tm = {};
        
        // Try date only
        for (const auto& [format, pattern] : dateFormats) {
            std::regex regex(pattern);
            if (std::regex_match(trimmed, regex)) {
                std::istringstream ss(trimmed);
                ss >> std::get_time(&tm, format.c_str());
                if (!ss.fail()) {
                    result.date = std::chrono::sys_days(
                        std::chrono::year_month_day(
                            std::chrono::year(tm.tm_year + 1900),
                            std::chrono::month(tm.tm_mon + 1),
                            std::chrono::day(tm.tm_mday)
                        )
                    );
                    result.value = result.date;
                    result.valid = true;
                    result.isDateOnly = true;
                    result.format = format;
                    return result;
                }
            }
        }
        
        // Try date + time
        for (const auto& [format, pattern] : dateTimeFormats) {
            std::regex regex(pattern);
            if (std::regex_match(trimmed, regex)) {
                std::istringstream ss(trimmed);
                ss >> std::get_time(&tm, format.c_str());
                if (!ss.fail()) {
                    result.value = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                    result.valid = true;
                    result.isDateOnly = false;
                    result.format = format;
                    return result;
                }
            }
        }
        
        // Try time only formats
        if (parseTimeOnly(trimmed, result)) return result;
        
        // Try relative dates
        if (parseRelativeDate(trimmed, result)) return result;
        
        return result;
    }
    
    static bool parseTimeOnly(const std::string& text, DateTimeResult& result) {
        std::vector<std::pair<std::string, std::regex>> timeFormats = {
            {"%H:%M:%S", std::regex(R"(\d{2}:\d{2}:\d{2})")},
            {"%H:%M", std::regex(R"(\d{2}:\d{2})")},
            {"%I:%M:%S %p", std::regex(R"(\d{1,2}:\d{2}:\d{2}\s*(AM|PM))", std::regex::icase)},
            {"%I:%M %p", std::regex(R"(\d{1,2}:\d{2}\s*(AM|PM))", std::regex::icase)}
        };
        
        for (const auto& [format, regex] : timeFormats) {
            std::smatch match;
            if (std::regex_match(text, match, regex)) {
                std::tm tm = {};
                std::istringstream ss(text);
                ss >> std::get_time(&tm, format.c_str());
                if (!ss.fail()) {
                    result.time = std::chrono::hours(tm.tm_hour) + 
                                  std::chrono::minutes(tm.tm_min) + 
                                  std::chrono::seconds(tm.tm_sec);
                    result.valid = true;
                    result.isTimeOnly = true;
                    result.format = format;
                    return true;
                }
            }
        }
        
        return false;
    }
    
    static bool parseRelativeDate(const std::string& text, DateTimeResult& result) {
        std::string lower = text;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        auto now = std::chrono::system_clock::now();
        
        if (lower == "today") {
            result.value = now;
            result.valid = true;
            result.isDateOnly = true;
            return true;
        }
        
        if (lower == "yesterday") {
            result.value = now - std::chrono::hours(24);
            result.valid = true;
            result.isDateOnly = true;
            return true;
        }
        
        if (lower == "tomorrow") {
            result.value = now + std::chrono::hours(24);
            result.valid = true;
            result.isDateOnly = true;
            return true;
        }
        
        // Parse "X days ago", "X hours ago", etc.
        std::regex relativeRegex(R"((\d+)\s*(day|days|hour|hours|minute|minutes|second|seconds)\s+ago)", std::regex::icase);
        std::smatch match;
        if (std::regex_match(lower, match, relativeRegex)) {
            long long amount = 0;
            try {
                amount = std::stoll(match[1]);
            } catch (const std::out_of_range&) {
                return false;
            }
            std::string unit = match[2];
            const long double unitSeconds =
                unit.find("day") != std::string::npos ? 86400.0L :
                unit.find("hour") != std::string::npos ? 3600.0L :
                unit.find("minute") != std::string::npos ? 60.0L : 1.0L;
            const long double deltaSeconds =
                static_cast<long double>(amount) * unitSeconds;
            using ClockDuration = std::chrono::system_clock::duration;
            const long double nowSeconds =
                std::chrono::duration<long double>(now.time_since_epoch()).count();
            const long double minimumSeconds =
                std::chrono::duration<long double>(ClockDuration::min()).count();
            if (!std::isfinite(deltaSeconds) ||
                deltaSeconds > nowSeconds - minimumSeconds) {
                return false;
            }
            const auto delta = std::chrono::duration_cast<ClockDuration>(
                std::chrono::duration<long double>(deltaSeconds));
            result.value = now - delta;
            
            result.valid = true;
            return true;
        }
        
        return false;
    }
    
    static bool parseISO8601(const std::string& text, DateTimeResult& result) {
        // ISO 8601: YYYY-MM-DDThh:mm:ss±hh:mm
        std::regex isoRegex(R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?(?:Z|([+-])(\d{2}):(\d{2}))?)");
        std::smatch match;
        
        if (std::regex_match(text, match, isoRegex)) {
            std::tm tm = {};
            tm.tm_year = std::stoi(match[1]) - 1900;
            tm.tm_mon = std::stoi(match[2]) - 1;
            tm.tm_mday = std::stoi(match[3]);
            tm.tm_hour = std::stoi(match[4]);
            tm.tm_min = std::stoi(match[5]);
            tm.tm_sec = std::stoi(match[6]);
            
            auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            
            // Apply timezone offset if present
            if (match[7].matched) {
                int offsetHours = std::stoi(match[8]);
                int offsetMins = std::stoi(match[9]);
                int offsetSeconds = (offsetHours * 3600 + offsetMins * 60);
                if (match[7].str() == "+") {
                    tp -= std::chrono::seconds(offsetSeconds);
                } else {
                    tp += std::chrono::seconds(offsetSeconds);
                }
            }
            
            result.value = tp;
            result.valid = true;
            result.format = "ISO8601";
            return true;
        }
        
        return false;
    }
    
    static std::string format(const std::chrono::system_clock::time_point& tp, const std::string& format) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), format.c_str());
        return ss.str();
    }
    
    static std::string format(const std::chrono::sys_days& date, const std::string& format) {
        std::chrono::year_month_day ymd(date);
        std::tm tm = {};
        tm.tm_year = static_cast<int>(ymd.year()) - 1900;
        tm.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
        tm.tm_mday = static_cast<unsigned>(ymd.day());
        
        std::stringstream ss;
        ss << std::put_time(&tm, format.c_str());
        return ss.str();
    }
    
    static std::string formatTime(const std::chrono::duration<double>& time) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(time);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(time - hours);
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time - hours - minutes);
        
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(2) << hours.count() << ":"
           << std::setw(2) << minutes.count() << ":"
           << std::setw(2) << seconds.count();
        return ss.str();
    }
    
private:
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
};

//=============================================================================
// ValueFormatter - Locale-independent display formatting
//=============================================================================
class ValueFormatter {
public:
    static std::string number(long double value, int precision = 2,
                              bool grouping = true) {
        validatePrecision(precision);
        if (!std::isfinite(value)) {
            if (std::isnan(value)) return "nan";
            return std::signbit(value) ? "-inf" : "inf";
        }

        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::fixed << std::setprecision(precision) << value;
        std::string result = stream.str();
        if (!grouping) return result;

        const std::size_t decimal = result.find('.');
        const std::size_t integerEnd = decimal == std::string::npos
            ? result.size()
            : decimal;
        const std::size_t firstDigit = !result.empty() &&
            (result[0] == '-' || result[0] == '+') ? 1 : 0;
        for (std::size_t position = integerEnd; position > firstDigit + 3;
             position -= 3) {
            result.insert(position - 3, 1, ',');
        }
        return result;
    }

    static std::string currency(long double value, const std::string& code = "USD",
                                int precision = 2) {
        validatePrecision(precision);
        const std::string normalizedCode = uppercase(code);
        const std::string symbol = currencySymbol(normalizedCode);
        const std::string formatted = number(value, precision);
        if (symbol.empty()) return normalizedCode + " " + formatted;
        return formatted.front() == '-'
            ? "-" + symbol + formatted.substr(1)
            : symbol + formatted;
    }

    static std::string measurement(long double value, const std::string& unit,
                                   int precision = 2) {
        validatePrecision(precision);
        if (unit.empty()) return number(value, precision);
        if (!std::isfinite(value) || value == 0.0L) {
            return number(value, precision) + " " + unit;
        }

        static constexpr const char* prefixes[] = {
            "q", "r", "y", "z", "a", "f", "p", "n", "µ", "m",
            "", "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"
        };
        int exponent = std::clamp(
            static_cast<int>(std::floor(std::log10(std::fabs(value)) / 3.0L)),
            -10, 10);
        long double scaled = value / std::pow(1000.0L, exponent);
        const long double roundingMargin =
            0.5L * std::pow(10.0L, -precision);
        if (std::fabs(scaled) >= 1000.0L - roundingMargin && exponent < 10) {
            ++exponent;
            scaled = value / std::pow(1000.0L, exponent);
        }
        const std::string prefix = prefixes[exponent + 10];
        const std::string suffix = prefix + unit;
        return number(scaled, precision) +
            (suffix.empty() ? "" : " " + suffix);
    }

    static std::string byteSize(long double bytes, int precision = 1,
                                bool binary = false) {
        validatePrecision(precision);
        if (!std::isfinite(bytes)) {
            return number(bytes, precision) + " B";
        }
        const long double base = binary ? 1024.0L : 1000.0L;
        const char* const* units = binary
            ? binaryByteUnits()
            : decimalByteUnits();
        const int maximumUnit = binary ? 8 : 10;

        long double scaled = bytes;
        int unitIndex = 0;
        while (std::fabs(scaled) >= base && unitIndex < maximumUnit) {
            scaled /= base;
            ++unitIndex;
        }
        const long double roundingMargin =
            0.5L * std::pow(10.0L, -precision);
        if (unitIndex < maximumUnit &&
            std::fabs(scaled) >= base - roundingMargin) {
            scaled /= base;
            ++unitIndex;
        }
        return number(scaled, precision) + " " + units[unitIndex];
    }

    static std::string dateTime(
        const std::chrono::system_clock::time_point& value,
        const std::string& format = "%Y-%m-%d %H:%M:%S UTC") {
        const std::time_t timestamp =
            std::chrono::system_clock::to_time_t(value);
        std::tm utc = {};
#if defined(_WIN32)
        if (gmtime_s(&utc, &timestamp) != 0) {
            throw std::runtime_error("Unable to convert timestamp to UTC");
        }
#else
        if (gmtime_r(&timestamp, &utc) == nullptr) {
            throw std::runtime_error("Unable to convert timestamp to UTC");
        }
#endif
        return formatTime(&utc, format);
    }

    static std::string date(
        const std::chrono::sys_days& value,
        const std::string& format = "%Y-%m-%d") {
        const std::chrono::year_month_day ymd(value);
        std::tm dateValue = {};
        dateValue.tm_year = static_cast<int>(ymd.year()) - 1900;
        dateValue.tm_mon = static_cast<int>(static_cast<unsigned>(ymd.month())) - 1;
        dateValue.tm_mday = static_cast<int>(static_cast<unsigned>(ymd.day()));
        return formatTime(&dateValue, format);
    }

    static std::string duration(std::chrono::duration<double> value,
                                int precision = 0) {
        validatePrecision(precision);
        if (!std::isfinite(value.count())) {
            return number(value.count(), precision) + " s";
        }

        const bool negative = value.count() < 0;
        long double remaining = std::fabs(value.count());
        if (remaining / 3600.0L >=
            static_cast<long double>(
                std::numeric_limits<unsigned long long>::max())) {
            return number(value.count(), precision) + " s";
        }
        const auto hours = static_cast<unsigned long long>(remaining / 3600.0L);
        remaining -= static_cast<long double>(hours) * 3600.0L;
        const auto minutes = static_cast<unsigned int>(remaining / 60.0L);
        remaining -= static_cast<long double>(minutes) * 60.0L;

        std::ostringstream result;
        result.imbue(std::locale::classic());
        if (negative) result << '-';
        bool hasPreviousUnit = false;
        if (hours != 0) {
            result << hours << "h";
            hasPreviousUnit = true;
        }
        if (minutes != 0) {
            if (hasPreviousUnit) result << ' ';
            result << minutes << "m";
            hasPreviousUnit = true;
        }
        if (!hasPreviousUnit || remaining != 0.0L) {
            if (hasPreviousUnit) result << ' ';
            result << number(remaining, precision, false) << "s";
        }
        return result.str();
    }

private:
    static void validatePrecision(int precision) {
        if (precision < 0 || precision > 15) {
            throw std::invalid_argument("Precision must be between 0 and 15");
        }
    }

    static std::string uppercase(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::toupper(character));
            });
        return value;
    }

    static std::string currencySymbol(const std::string& code) {
        static const std::unordered_map<std::string, std::string> symbols = {
            {"USD", "$"}, {"EUR", "€"}, {"GBP", "£"}, {"JPY", "¥"},
            {"CNY", "¥"}, {"CAD", "CA$"}, {"AUD", "A$"}, {"INR", "₹"}
        };
        const auto found = symbols.find(code);
        return found == symbols.end() ? "" : found->second;
    }

    static const char* const* decimalByteUnits() {
        static const char* units[] = {
            "B", "kB", "MB", "GB", "TB", "PB",
            "EB", "ZB", "YB", "RB", "QB"
        };
        return units;
    }

    static const char* const* binaryByteUnits() {
        static const char* units[] = {
            "B", "KiB", "MiB", "GiB", "TiB",
            "PiB", "EiB", "ZiB", "YiB"
        };
        return units;
    }

    static std::string formatTime(const std::tm* value,
                                 const std::string& format) {
        std::vector<char> buffer(128);
        while (buffer.size() <= 65536) {
            const std::size_t size =
                std::strftime(buffer.data(), buffer.size(), format.c_str(), value);
            if (size != 0) return std::string(buffer.data(), size);
            if (format.empty()) return "";
            buffer.resize(buffer.size() * 2);
        }
        throw std::length_error("Formatted date/time exceeds 64 KiB");
    }
};

inline TypedValue TypedValue::detect(const std::string& str) {
    try {
        std::size_t position;
        const long long value = std::stoll(str, &position);
        if (position == str.length()) return TypedValue(value);
    } catch (const std::exception&) {
    }

    try {
        std::size_t position;
        const double value = std::stod(str, &position);
        if (position == str.length()) return TypedValue(value);
    } catch (const std::exception&) {
    }

    const auto dateTime = DateTimeParser::parse(str);
    if (dateTime.valid) {
        if (dateTime.isDateOnly) return TypedValue(dateTime.date);
        if (dateTime.isTimeOnly) return TypedValue(dateTime.time);
        return TypedValue(dateTime.value);
    }
    return TypedValue(str);
}

inline std::string TypedValue::asString() const {
    if (m_type == ValueType::String) return m_string;
    if (m_type == ValueType::Integer) return std::to_string(m_int);
    if (m_type == ValueType::Double) {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        stream << std::fixed << m_double;
        return stream.str();
    }
    if (m_type == ValueType::Boolean) return m_bool ? "true" : "false";
    if (m_type == ValueType::DateTime) {
        return ValueFormatter::dateTime(m_timePoint, "%Y-%m-%d %H:%M:%S");
    }
    if (m_type == ValueType::Date) return ValueFormatter::date(m_date);
    if (m_type == ValueType::Time) return DateTimeParser::formatTime(m_timeOfDay);
    if (m_type == ValueType::Duration) return ValueFormatter::duration(m_duration);
    return "";
}

//=============================================================================
// Forward declarations for DocumentNode and DocumentEditor
//=============================================================================
class DocumentNode;
class DocumentEditor;

//=============================================================================
// DocumentNode Implementation (continued)
//=============================================================================
class DocumentNode : public std::enable_shared_from_this<DocumentNode> {
public:
    using Ptr = std::shared_ptr<DocumentNode>;
    using List = std::vector<Ptr>;
    using AttributeMap = std::unordered_map<std::string, std::string>;
    using ChildrenList = std::vector<Ptr>;
    
    // Constructors
    DocumentNode(EntityType type = EntityType::Element) : m_type(type) {}
    DocumentNode(const std::string& name) : m_type(EntityType::Element), m_name(name) {}
    DocumentNode(const std::string& name, const std::string& text) 
        : m_type(EntityType::Element), m_name(name), m_text(text) {}
    
    virtual ~DocumentNode() = default;
    
    // Basic Properties
    EntityType type() const { return m_type; }
    void setType(EntityType type) { m_type = type; }
    
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }
    
    const std::string& text() const { return m_text; }
    void setText(const std::string& text) { m_text = text; }
    
    // Typed value support
    TypedValue typedValue() const {
        // Check if we have a stored typed value
        auto typeAttr = getAttribute("type");
        if (typeAttr) {
            if (*typeAttr == "integer") {
                auto val = getAttribute("value_int");
                if (val) return TypedValue(std::stoll(*val));
            } else if (*typeAttr == "float") {
                auto val = getAttribute("value_double");
                if (val) return TypedValue(std::stod(*val));
            } else if (*typeAttr == "boolean") {
                auto val = getAttribute("value_bool");
                if (val) return TypedValue(*val == "true");
            } else if (*typeAttr == "datetime") {
                auto val = getAttribute("value_datetime");
                if (val) {
                    auto dt = DateTimeParser::parse(*val);
                    if (dt.valid) return TypedValue(dt.value);
                }
            }
        }
        
        // Try to detect from text
        return TypedValue::detect(m_text);
    }
    
    void setTypedValue(const TypedValue& value) {
        m_text = value.asString();
        
        switch (value.type()) {
            case TypedValue::ValueType::Integer:
                setAttribute("type", "integer");
                setAttribute("value_int", std::to_string(value.asInt()));
                break;
            case TypedValue::ValueType::Double:
                setAttribute("type", "float");
                setAttribute("value_double", std::to_string(value.asDouble()));
                break;
            case TypedValue::ValueType::Boolean:
                setAttribute("type", "boolean");
                setAttribute("value_bool", value.asBool() ? "true" : "false");
                break;
            case TypedValue::ValueType::DateTime:
                setAttribute("type", "datetime");
                setAttribute("value_datetime", 
                    DateTimeParser::format(value.asDateTime(), "%Y-%m-%d %H:%M:%S"));
                break;
            default:
                setAttribute("type", "string");
                break;
        }
    }
    
    // Attribute Management
    void setAttribute(const std::string& key, const std::string& value) {
        m_attributes[key] = value;
        if (key == "id" || key == "ID" || key == "Id") {
            m_id = value;
        }
    }
    
    std::optional<std::string> getAttribute(const std::string& key) const {
        auto it = m_attributes.find(key);
        if (it != m_attributes.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    template<typename T>
    std::optional<T> getAttributeAs(const std::string& key) const {
        auto attr = getAttribute(key);
        if (attr) {
            if constexpr (std::is_same_v<T, int>) {
                try { return std::stoi(*attr); } catch (...) {}
            } else if constexpr (std::is_same_v<T, double>) {
                try { return std::stod(*attr); } catch (...) {}
            } else if constexpr (std::is_same_v<T, bool>) {
                std::string lower = *attr;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower == "true" || lower == "1") return true;
                if (lower == "false" || lower == "0") return false;
            } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
                auto dt = DateTimeParser::parse(*attr);
                if (dt.valid) return dt.value;
            }
        }
        return std::nullopt;
    }
    
    bool hasAttribute(const std::string& key) const {
        return m_attributes.find(key) != m_attributes.end();
    }
    
    void removeAttribute(const std::string& key) {
        m_attributes.erase(key);
        if (key == "id" || key == "ID" || key == "Id") {
            m_id.clear();
        }
    }
    
    const AttributeMap& attributes() const { return m_attributes; }
    AttributeMap& attributes() { return m_attributes; }
    
    // ID Management
    const std::string& id() const { return m_id; }
    void setId(const std::string& id) { 
        m_id = id;
        m_attributes["id"] = id;
    }
    
    bool hasId() const { return !m_id.empty(); }
    
    // Class Management
    std::vector<std::string> getClasses() const {
        std::vector<std::string> classes;
        auto classAttr = getAttribute("class");
        if (classAttr) {
            std::stringstream ss(*classAttr);
            std::string className;
            while (ss >> className) {
                classes.push_back(className);
            }
        }
        return classes;
    }
    
    void addClass(const std::string& className) {
        auto classes = getClasses();
        if (std::find(classes.begin(), classes.end(), className) == classes.end()) {
            classes.push_back(className);
            std::string classStr;
            for (size_t i = 0; i < classes.size(); ++i) {
                if (i > 0) classStr += " ";
                classStr += classes[i];
            }
            setAttribute("class", classStr);
        }
    }
    
    void removeClass(const std::string& className) {
        auto classes = getClasses();
        classes.erase(std::remove(classes.begin(), classes.end(), className), classes.end());
        std::string classStr;
        for (size_t i = 0; i < classes.size(); ++i) {
            if (i > 0) classStr += " ";
            classStr += classes[i];
        }
        if (classStr.empty()) {
            removeAttribute("class");
        } else {
            setAttribute("class", classStr);
        }
    }
    
    // Child Management
    void addChild(Ptr child) {
        child->m_parent = shared_from_this();
        m_children.push_back(child);
    }
    
    void addChild(size_t index, Ptr child) {
        child->m_parent = shared_from_this();
        if (index < m_children.size()) {
            m_children.insert(m_children.begin() + index, child);
        } else {
            m_children.push_back(child);
        }
    }
    
    void removeChild(Ptr child) {
        auto it = std::find(m_children.begin(), m_children.end(), child);
        if (it != m_children.end()) {
            (*it)->m_parent.reset();
            m_children.erase(it);
        }
    }
    
    void removeChild(size_t index) {
        if (index < m_children.size()) {
            m_children[index]->m_parent.reset();
            m_children.erase(m_children.begin() + index);
        }
    }
    
    void clearChildren() {
        for (auto& child : m_children) {
            child->m_parent.reset();
        }
        m_children.clear();
    }
    
    const ChildrenList& children() const { return m_children; }
    ChildrenList& children() { return m_children; }
    
    Ptr parent() const { return m_parent.lock(); }
    
    // Text Content Management
    std::string getTextContent() const {
        std::string result = m_text;
        for (const auto& child : m_children) {
            result += child->getTextContent();
        }
        return result;
    }
    
    void setTextContent(const std::string& text) {
        clearChildren();
        m_text = text;
    }
    
    // Query Methods
    Ptr findById(const std::string& id) {
        if (m_id == id) return shared_from_this();
        for (auto& child : m_children) {
            auto result = child->findById(id);
            if (result) return result;
        }
        return nullptr;
    }
    
    List findByTagName(const std::string& tagName) {
        List results;
        if (m_name == tagName) results.push_back(shared_from_this());
        for (auto& child : m_children) {
            auto childResults = child->findByTagName(tagName);
            results.insert(results.end(), childResults.begin(), childResults.end());
        }
        return results;
    }
    
    List findByClassName(const std::string& className) {
        List results;
        auto classes = getClasses();
        if (std::find(classes.begin(), classes.end(), className) != classes.end()) {
            results.push_back(shared_from_this());
        }
        for (auto& child : m_children) {
            auto childResults = child->findByClassName(className);
            results.insert(results.end(), childResults.begin(), childResults.end());
        }
        return results;
    }
    
    List findByAttribute(const std::string& attrName, const std::string& attrValue = "") {
        List results;
        auto attr = getAttribute(attrName);
        if (attr && (attrValue.empty() || *attr == attrValue)) {
            results.push_back(shared_from_this());
        }
        for (auto& child : m_children) {
            auto childResults = child->findByAttribute(attrName, attrValue);
            results.insert(results.end(), childResults.begin(), childResults.end());
        }
        return results;
    }
    
    // Numeric queries
    List findByNumericValue(const std::string& attrName, double minValue, double maxValue) {
        List results;
        auto attr = getAttribute(attrName);
        if (attr) {
            try {
                double val = std::stod(*attr);
                if (val >= minValue && val <= maxValue) {
                    results.push_back(shared_from_this());
                }
            } catch (...) {}
        }
        for (auto& child : m_children) {
            auto childResults = child->findByNumericValue(attrName, minValue, maxValue);
            results.insert(results.end(), childResults.begin(), childResults.end());
        }
        return results;
    }
    
    // Date queries
    List findByDateRange(const std::string& attrName, 
                         const std::chrono::system_clock::time_point& start,
                         const std::chrono::system_clock::time_point& end) {
        List results;
        auto attr = getAttribute(attrName);
        if (attr) {
            auto dt = DateTimeParser::parse(*attr);
            if (dt.valid && dt.value >= start && dt.value <= end) {
                results.push_back(shared_from_this());
            }
        }
        for (auto& child : m_children) {
            auto childResults = child->findByDateRange(attrName, start, end);
            results.insert(results.end(), childResults.begin(), childResults.end());
        }
        return results;
    }
    
    // CSS Selector support
    List querySelectorAll(const std::string& selector);
    Ptr querySelector(const std::string& selector);
    
    // Index access
    Ptr getElementByIndex(size_t index) {
        if (index < m_children.size()) {
            return m_children[index];
        }
        return nullptr;
    }
    
    // Editing Methods
    void setInnerHTML(const std::string& html);
    std::string getInnerHTML() const;
    std::string getOuterHTML() const;
    
    // Clone
    Ptr clone(bool deep = true) const {
        auto newNode = std::make_shared<DocumentNode>(m_type);
        newNode->m_name = m_name;
        newNode->m_text = m_text;
        newNode->m_attributes = m_attributes;
        newNode->m_id = m_id;
        
        if (deep) {
            for (const auto& child : m_children) {
                newNode->addChild(child->clone(true));
            }
        }
        
        return newNode;
    }
    
private:
    EntityType m_type = EntityType::Element;
    std::string m_name;
    std::string m_text;
    std::string m_id;
    AttributeMap m_attributes;
    ChildrenList m_children;
    std::weak_ptr<DocumentNode> m_parent;
};

//=============================================================================
// CSVParser - Complete CSV support with type detection
//=============================================================================
class DocumentNode;

class CSVParser {
public:
    struct ParseOptions {
        char delimiter = ',';
        char quote = '"';
        char escape = '"';
        bool hasHeader = true;
        bool detectTypes = true;
        bool trimFields = true;
        bool skipEmptyLines = true;
        std::string lineEnding = "\n";
        std::string encoding = "UTF-8";
    };

    struct CSVParseResult {
        std::shared_ptr<DocumentNode> document;
        std::vector<std::string> headers;
        std::vector<std::vector<TypedValue>> data;
        std::vector<std::vector<std::string>> rawData;
        size_t rowCount = 0;
        size_t columnCount = 0;
        bool success = false;
        std::string error;
    };

    static void validateOptions(const ParseOptions& options) {
        if (options.delimiter == options.quote || options.escape == options.delimiter ||
            options.delimiter == '\r' || options.delimiter == '\n' ||
            options.quote == '\r' || options.quote == '\n' ||
            options.escape == '\r' || options.escape == '\n') {
            throw std::invalid_argument(
                "CSV delimiter must differ from quote and escape; these cannot be line breaks");
        }
    }

    static CSVParseResult parse(const std::string& csv, const ParseOptions& options) {
        CSVParseResult result;
        try {
            validateOptions(options);
        } catch (const std::invalid_argument& error) {
            result.error = error.what();
            return result;
        }
        result.document = std::make_shared<DocumentNode>(EntityType::CSVDocument);
        result.document->setName("csv");

        std::string parseError;
        const auto records = parseCSVRecords(csv, options, parseError);
        if (!parseError.empty()) {
            result.error = parseError;
            return result;
        }
        bool firstRow = true;
        for (const auto& fields : records) {

            if (firstRow && options.hasHeader) {
                result.headers = fields;
                firstRow = false;
                continue;
            }

            // Create row node
            auto rowNode = std::make_shared<DocumentNode>(EntityType::CSVRow);
            rowNode->setAttribute("row", std::to_string(result.rowCount));

            // Process fields
            std::vector<TypedValue> typedRow;
            std::vector<std::string> rawRow;

            for (size_t i = 0; i < fields.size(); ++i) {
                std::string field = options.trimFields ? trim(fields[i]) : fields[i];
                rawRow.push_back(field);

                TypedValue typedValue;
                if (options.detectTypes) {
                    typedValue = TypedValue::detect(field);
                } else {
                    typedValue = TypedValue(field);
                }
                typedRow.push_back(typedValue);

                // Create field node
                auto fieldNode = std::make_shared<DocumentNode>(EntityType::CSVField);
                if (!result.headers.empty() && i < result.headers.size()) {
                    fieldNode->setName(result.headers[i]);
                    fieldNode->setAttribute("column", result.headers[i]);
                } else {
                    fieldNode->setName("col_" + std::to_string(i));
                }
                fieldNode->setAttribute("column_index", std::to_string(i));

                // Store typed value as attribute
                switch (typedValue.type()) {
                    case TypedValue::ValueType::Integer:
                        fieldNode->setAttribute("type", "integer");
                        fieldNode->setAttribute("value_int", std::to_string(typedValue.asInt()));
                        fieldNode->setText(std::to_string(typedValue.asInt()));
                        break;
                    case TypedValue::ValueType::Double:
                        fieldNode->setAttribute("type", "float");
                        fieldNode->setAttribute("value_double", std::to_string(typedValue.asDouble()));
                        fieldNode->setText(std::to_string(typedValue.asDouble()));
                        break;
                    case TypedValue::ValueType::DateTime:
                        fieldNode->setAttribute("type", "datetime");
                        fieldNode->setAttribute("value_datetime",
                            DateTimeParser::format(typedValue.asDateTime(), "%Y-%m-%d %H:%M:%S"));
                        fieldNode->setText(field);
                        break;
                    case TypedValue::ValueType::Date:
                        fieldNode->setAttribute("type", "date");
                        fieldNode->setText(field);
                        break;
                    case TypedValue::ValueType::Time:
                        fieldNode->setAttribute("type", "time");
                        fieldNode->setText(field);
                        break;
                    case TypedValue::ValueType::Boolean:
                        fieldNode->setAttribute("type", "boolean");
                        fieldNode->setAttribute("value_bool", typedValue.asBool() ? "true" : "false");
                        fieldNode->setText(field);
                        break;
                    default:
                        fieldNode->setAttribute("type", "string");
                        fieldNode->setText(field);
                        break;
                }

                rowNode->addChild(fieldNode);
            }

            result.data.push_back(typedRow);
            result.rawData.push_back(rawRow);
            result.document->addChild(rowNode);
            result.rowCount++;
            result.columnCount = std::max(result.columnCount, fields.size());
        }

        result.success = true;
        return result;
    }

    static CSVParseResult parse(const std::string& csv) {
        return parse(csv, ParseOptions{});
    }

    static std::string serialize(const CSVParseResult& result, const ParseOptions& options) {
        validateOptions(options);
        std::stringstream ss;

        // Write headers
        if (!result.headers.empty()) {
            for (size_t i = 0; i < result.headers.size(); ++i) {
                if (i > 0) ss << options.delimiter;
                ss << quoteIfNeeded(result.headers[i], options);
            }
            ss << options.lineEnding;
        }

        // Write data
        for (const auto& row : result.data) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) ss << options.delimiter;
                std::string value = row[i].asString();
                ss << quoteIfNeeded(value, options);
            }
            ss << options.lineEnding;
        }

        return ss.str();
    }

    static std::string serialize(const CSVParseResult& result) {
        return serialize(result, ParseOptions{});
    }

private:
    static std::vector<std::vector<std::string>> parseCSVRecords(
        const std::string& csv, const ParseOptions& options, std::string& error) {
        std::vector<std::vector<std::string>> records;
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;
        bool afterQuote = false;
        bool recordStarted = false;

        auto finishRecord = [&] {
            fields.push_back(options.trimFields ? trim(field) : field);
            field.clear();
            records.push_back(std::move(fields));
            fields.clear();
            recordStarted = false;
        };

        size_t startOffset = csv.size() >= 3 &&
            csv.compare(0, 3, std::string("\xEF\xBB\xBF", 3)) == 0 ? 3 : 0;
        for (size_t i = startOffset; i < csv.size(); ++i) {
            const char c = csv[i];
            if (inQuotes) {
                if (options.escape != options.quote && c == options.escape) {
                    if (i + 1 == csv.size()) {
                        error = "CSV parse error: escape at end of input";
                        return {};
                    }
                    field += csv[++i];
                } else if (c == options.quote) {
                    if (options.escape == options.quote &&
                        i + 1 < csv.size() && csv[i + 1] == options.quote) {
                        field += options.quote;
                        ++i;
                    } else {
                        inQuotes = false;
                        afterQuote = true;
                    }
                } else {
                    field += c;
                }
                continue;
            }

            if (afterQuote) {
                if (c == options.delimiter) {
                    fields.push_back(options.trimFields ? trim(field) : field);
                    field.clear();
                    afterQuote = false;
                    recordStarted = true;
                    continue;
                }
                if (c == '\r' || c == '\n') {
                    finishRecord();
                    afterQuote = false;
                    if (c == '\r' && i + 1 < csv.size() && csv[i + 1] == '\n') {
                        ++i;
                    }
                    continue;
                }
                if (options.trimFields && (c == ' ' || c == '\t')) {
                    continue;
                }
                error = "CSV parse error: unexpected character after closing quote";
                return {};
            }

            if (c == options.quote) {
                if (!field.empty()) {
                    error = "CSV parse error: quote inside an unquoted field";
                    return {};
                }
                inQuotes = true;
                recordStarted = true;
            } else if (c == options.delimiter) {
                fields.push_back(options.trimFields ? trim(field) : field);
                field.clear();
                recordStarted = true;
            } else if (c == '\r' || c == '\n') {
                const bool emptyRecord = !recordStarted && fields.empty() && field.empty();
                if (!(options.skipEmptyLines && emptyRecord)) {
                    finishRecord();
                }
                if (c == '\r' && i + 1 < csv.size() && csv[i + 1] == '\n') {
                    ++i;
                }
            } else {
                field += c;
                recordStarted = true;
            }
        }

        if (inQuotes) {
            error = "CSV parse error: unterminated quoted field";
            return {};
        }
        if (recordStarted || !fields.empty() || !field.empty() || afterQuote) {
            finishRecord();
        }

        return records;
    }

    static std::string quoteIfNeeded(const std::string& field, const ParseOptions& options) {
        // Check if quoting is needed
        bool needsQuote = field.find(options.delimiter) != std::string::npos ||
                          field.find(options.quote) != std::string::npos ||
                          field.find('\n') != std::string::npos ||
                          field.find('\r') != std::string::npos;

        if (!needsQuote) return field;

        std::string quoted;
        quoted += options.quote;
        for (char c : field) {
            if (options.escape != options.quote && c == options.escape) {
                quoted += options.escape;
                quoted += c;
            } else if (c == options.quote) {
                quoted += options.escape;
                quoted += options.quote;
            } else {
                quoted += c;
            }
        }
        quoted += options.quote;
        return quoted;
    }

    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }
};


namespace document_parser_detail {

inline bool hasUtf8Bom(const std::string& input) {
    return input.size() >= 3 &&
        input.compare(0, 3, std::string("\xEF\xBB\xBF", 3)) == 0;
}

inline std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

inline std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline void requireValidUtf8(const std::string& input, const std::string& format) {
    for (size_t i = 0; i < input.size();) {
        const unsigned char first = static_cast<unsigned char>(input[i]);
        if (first <= 0x7F) {
            ++i;
            continue;
        }
        size_t length = 0;
        std::uint32_t codepoint = 0;
        if (first >= 0xC2 && first <= 0xDF) {
            length = 2;
            codepoint = first & 0x1F;
        } else if (first >= 0xE0 && first <= 0xEF) {
            length = 3;
            codepoint = first & 0x0F;
        } else if (first >= 0xF0 && first <= 0xF4) {
            length = 4;
            codepoint = first & 0x07;
        } else {
            throw std::runtime_error(format + " input is not valid UTF-8 at byte " +
                                     std::to_string(i));
        }
        if (input.size() - i < length) {
            throw std::runtime_error(format + " input has truncated UTF-8 at byte " +
                                     std::to_string(i));
        }
        for (size_t j = 1; j < length; ++j) {
            const unsigned char continuation = static_cast<unsigned char>(input[i + j]);
            if ((continuation & 0xC0) != 0x80) {
                throw std::runtime_error(format + " input is not valid UTF-8 at byte " +
                                         std::to_string(i + j));
            }
            codepoint = (codepoint << 6) | (continuation & 0x3F);
        }
        if ((length == 3 && codepoint < 0x800) ||
            (length == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
            codepoint > 0x10FFFF) {
            throw std::runtime_error(format + " input contains invalid UTF-8 at byte " +
                                     std::to_string(i));
        }
        i += length;
    }
}

inline void appendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        throw std::runtime_error("Invalid Unicode code point");
    }
    if (codepoint <= 0x7F) {
        output += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        output += static_cast<char>(0xC0 | (codepoint >> 6));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        output += static_cast<char>(0xE0 | (codepoint >> 12));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        output += static_cast<char>(0xF0 | (codepoint >> 18));
        output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

inline std::string decodeEntities(const std::string& input, bool html) {
    std::string output;
    output.reserve(input.size());
    for (size_t i = 0; i < input.size();) {
        if (input[i] != '&') {
            output += input[i++];
            continue;
        }
        const size_t semicolon = input.find(';', i + 1);
        if (semicolon == std::string::npos || semicolon - i > 16) {
            if (!html) throw std::runtime_error("Unterminated entity reference");
            output += input[i++];
            continue;
        }
        const std::string entity = input.substr(i + 1, semicolon - i - 1);
        if (entity == "amp") output += '&';
        else if (entity == "lt") output += '<';
        else if (entity == "gt") output += '>';
        else if (entity == "quot") output += '"';
        else if (entity == "apos") output += '\'';
        else if (html && entity == "nbsp") appendUtf8(output, 0xA0);
        else if (html && entity == "copy") appendUtf8(output, 0xA9);
        else if (html && entity == "reg") appendUtf8(output, 0xAE);
        else if (html && entity == "trade") appendUtf8(output, 0x2122);
        else if (html && entity == "mdash") appendUtf8(output, 0x2014);
        else if (html && entity == "ndash") appendUtf8(output, 0x2013);
        else if (html && entity == "hellip") appendUtf8(output, 0x2026);
        else if (html && entity == "euro") appendUtf8(output, 0x20AC);
        else if (!entity.empty() && entity[0] == '#') {
            std::uint32_t codepoint = 0;
            const bool hexadecimal = entity.size() > 2 &&
                (entity[1] == 'x' || entity[1] == 'X');
            const size_t start = hexadecimal ? 2 : 1;
            const int base = hexadecimal ? 16 : 10;
            if (start == entity.size()) {
                if (!html) throw std::runtime_error("Empty numeric entity");
                output += input.substr(i, semicolon - i + 1);
                i = semicolon + 1;
                continue;
            }
            bool validDigits = true;
            for (size_t j = start; j < entity.size(); ++j) {
                const unsigned char ch = static_cast<unsigned char>(entity[j]);
                int digit = -1;
                if (ch >= '0' && ch <= '9') digit = ch - '0';
                else if (hexadecimal && ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
                else if (hexadecimal && ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
                if (digit < 0 || digit >= base ||
                    codepoint > (0x10FFFFu - static_cast<unsigned>(digit)) /
                        static_cast<unsigned>(base)) {
                    validDigits = false;
                    break;
                }
                codepoint = codepoint * static_cast<unsigned>(base) +
                    static_cast<unsigned>(digit);
            }
            if (!validDigits) {
                if (!html) throw std::runtime_error("Invalid numeric entity");
                output += input.substr(i, semicolon - i + 1);
                i = semicolon + 1;
                continue;
            }
            if (!html && codepoint < 0x20 && codepoint != 0x09 &&
                codepoint != 0x0A && codepoint != 0x0D) {
                throw std::runtime_error("Invalid XML character reference");
            }
            if (html && (codepoint == 0 ||
                (codepoint < 0x20 && codepoint != 0x09 && codepoint != 0x0A &&
                 codepoint != 0x0D) ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
                codepoint > 0x10FFFF)) {
                codepoint = 0xFFFD;
            }
            appendUtf8(output, codepoint);
        } else if (html) {
            output += input.substr(i, semicolon - i + 1);
        } else {
            throw std::runtime_error("Unknown XML entity: &" + entity + ";");
        }
        i = semicolon + 1;
    }
    return output;
}

inline std::string escapeMarkup(const std::string& input, bool attribute = false) {
    std::string output;
    for (char ch : input) {
        switch (ch) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '"':
                if (attribute) output += "&quot;";
                else output += ch;
                break;
            default: output += ch; break;
        }
    }
    return output;
}

inline bool isVoidHtmlTag(const std::string& tag) {
    static const std::unordered_set<std::string> tags = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };
    return tags.count(tag) != 0;
}

class XmlParser {
public:
    explicit XmlParser(const std::string& source)
        : m_source(source), m_position(hasUtf8Bom(source) ? 3 : 0) {}

    DocumentNode::Ptr parse() {
        requireValidUtf8(m_source, "XML");
        auto document = std::make_shared<DocumentNode>(EntityType::XMLDocument);
        document->setName("xml");
        skipMisc(document, true);
        if (m_position == m_source.size()) fail("Missing document element");
        document->addChild(parseElement(0));
        skipMisc(document, false);
        if (m_position != m_source.size()) fail("Content after document element");
        return document;
    }

private:
    const std::string& m_source;
    size_t m_position = 0;
    bool m_seenDoctype = false;

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error(message + " at byte " + std::to_string(m_position));
    }

    bool starts(const char* token) const {
        return m_source.compare(m_position, std::char_traits<char>::length(token), token) == 0;
    }

    void skipSpace() {
        while (m_position < m_source.size() &&
               std::isspace(static_cast<unsigned char>(m_source[m_position]))) {
            ++m_position;
        }
    }

    std::string readName() {
        const size_t start = m_position;
        if (m_position == m_source.size()) fail("Expected XML name");
        const unsigned char first = static_cast<unsigned char>(m_source[m_position]);
        if (!(std::isalpha(first) || first == '_' || first == ':' || first >= 0x80)) {
            fail("Invalid XML name start");
        }
        ++m_position;
        while (m_position < m_source.size()) {
            const unsigned char ch = static_cast<unsigned char>(m_source[m_position]);
            if (std::isalnum(ch) || ch == '_' || ch == ':' || ch == '-' ||
                ch == '.' || ch >= 0x80) {
                ++m_position;
            } else {
                break;
            }
        }
        if (start == m_position) fail("Expected XML name");
        return m_source.substr(start, m_position - start);
    }

    std::string readUntil(const std::string& terminator) {
        const size_t end = m_source.find(terminator, m_position);
        if (end == std::string::npos) fail("Unterminated XML markup");
        std::string value = m_source.substr(m_position, end - m_position);
        m_position = end + terminator.size();
        return value;
    }

    std::string readComment() {
        std::string value = readUntil("-->");
        if (value.find("--") != std::string::npos) {
            fail("'--' is not allowed inside XML comments");
        }
        return value;
    }

    void skipMisc(const DocumentNode::Ptr& parent, bool allowDoctype) {
        while (m_position < m_source.size()) {
            skipSpace();
            if (starts("<!--")) {
                m_position += 4;
                auto comment = std::make_shared<DocumentNode>(EntityType::XMLComment);
                comment->setText(readComment());
                parent->addChild(comment);
            } else if (starts("<?")) {
                m_position += 2;
                const std::string target = readName();
                auto instruction =
                    std::make_shared<DocumentNode>(EntityType::XMLProcessingInstruction);
                instruction->setName(target);
                skipSpace();
                instruction->setText(readUntil("?>"));
                parent->addChild(instruction);
            } else if (allowDoctype && !m_seenDoctype && starts("<!DOCTYPE")) {
                const size_t start = m_position;
                bool quoted = false;
                char quote = 0;
                int subsetDepth = 0;
                bool terminated = false;
                do {
                    const char ch = m_source[m_position++];
                    if (quoted) {
                        if (ch == quote) quoted = false;
                    } else if (ch == '"' || ch == '\'') {
                        quoted = true;
                        quote = ch;
                    } else if (ch == '[') {
                        ++subsetDepth;
                    } else if (ch == ']') {
                        --subsetDepth;
                    } else if (ch == '>' && subsetDepth == 0) {
                        terminated = true;
                        break;
                    }
                } while (m_position < m_source.size());
                if (!terminated) fail("Unterminated DOCTYPE");
                auto doctype = std::make_shared<DocumentNode>(EntityType::XMLDoctype);
                doctype->setText(m_source.substr(start, m_position - start));
                parent->addChild(doctype);
                m_seenDoctype = true;
            } else {
                break;
            }
        }
    }

    DocumentNode::Ptr parseElement(size_t depth) {
        if (depth > 512) fail("Maximum XML nesting depth exceeded");
        if (!starts("<") || starts("</")) fail("Expected XML start tag");
        ++m_position;
        const std::string name = readName();
        auto element = std::make_shared<DocumentNode>(EntityType::XMLElement);
        element->setName(name);
        std::unordered_set<std::string> attributeNames;

        while (true) {
            skipSpace();
            if (starts("/>")) {
                m_position += 2;
                return element;
            }
            if (starts(">")) {
                ++m_position;
                break;
            }
            const std::string key = readName();
            if (!attributeNames.insert(key).second) fail("Duplicate XML attribute");
            skipSpace();
            if (!starts("=")) fail("Expected '=' after XML attribute");
            ++m_position;
            skipSpace();
            if (m_position == m_source.size() ||
                (m_source[m_position] != '"' && m_source[m_position] != '\'')) {
                fail("XML attribute value must be quoted");
            }
            const char quote = m_source[m_position++];
            const size_t start = m_position;
            while (m_position < m_source.size() && m_source[m_position] != quote) {
                if (m_source[m_position] == '<') fail("'<' in XML attribute value");
                ++m_position;
            }
            if (m_position == m_source.size()) fail("Unterminated XML attribute");
            element->setAttribute(key, decodeEntities(
                m_source.substr(start, m_position - start), false));
            ++m_position;
        }

        while (m_position < m_source.size()) {
            if (starts("</")) {
                m_position += 2;
                const std::string closingName = readName();
                skipSpace();
                if (!starts(">")) fail("Expected '>' on XML end tag");
                ++m_position;
                if (closingName != name) fail("Mismatched XML end tag");
                return element;
            }
            if (starts("<!--")) {
                m_position += 4;
                auto comment = std::make_shared<DocumentNode>(EntityType::XMLComment);
                comment->setText(readComment());
                element->addChild(comment);
            } else if (starts("<![CDATA[")) {
                m_position += 9;
                auto cdata = std::make_shared<DocumentNode>(EntityType::XMLCDATA);
                cdata->setText(readUntil("]]>"));
                element->addChild(cdata);
            } else if (starts("<?")) {
                m_position += 2;
                auto instruction =
                    std::make_shared<DocumentNode>(EntityType::XMLProcessingInstruction);
                instruction->setName(readName());
                skipSpace();
                instruction->setText(readUntil("?>"));
                element->addChild(instruction);
            } else if (starts("<!")) {
                fail("Unsupported declaration inside XML element");
            } else if (starts("<")) {
                element->addChild(parseElement(depth + 1));
            } else {
                const size_t start = m_position;
                while (m_position < m_source.size() && m_source[m_position] != '<') {
                    ++m_position;
                }
                const std::string rawText = m_source.substr(start, m_position - start);
                if (rawText.find("]]>") != std::string::npos) {
                    fail("']]>' is only allowed in CDATA");
                }
                auto text = std::make_shared<DocumentNode>(EntityType::Text);
                text->setText(decodeEntities(rawText, false));
                element->addChild(text);
            }
        }
        fail("Unclosed XML element <" + name + ">");
    }
};

inline DocumentNode::Ptr parseHtml(const std::string& source) {
    requireValidUtf8(source, "HTML");
    auto document = std::make_shared<DocumentNode>(EntityType::HTMLDocument);
    document->setName("html");
    std::vector<DocumentNode::Ptr> stack{document};
    size_t position = hasUtf8Bom(source) ? 3 : 0;
    auto addText = [&](const std::string& text, bool raw = false) {
        if (text.empty()) return;
        auto node = std::make_shared<DocumentNode>(EntityType::Text);
        node->setText(raw ? text : decodeEntities(text, true));
        stack.back()->addChild(node);
    };
    auto tagNameChar = [](unsigned char ch) {
        return std::isalnum(ch) || ch == ':' || ch == '-' || ch == '_';
    };
    static const std::unordered_set<std::string> autoCloseOnBlock = {
        "address", "article", "aside", "blockquote", "div", "dl", "fieldset",
        "footer", "form", "h1", "h2", "h3", "h4", "h5", "h6", "header",
        "hr", "main", "nav", "ol", "p", "pre", "section", "table", "ul"
    };
    auto closeOpen = [&](const std::string& tag) {
        for (size_t i = stack.size(); i > 1; --i) {
            if (lower(stack[i - 1]->name()) == tag) {
                stack.resize(i - 1);
                return;
            }
        }
    };

    while (position < source.size()) {
        if (source[position] != '<') {
            const size_t end = source.find('<', position);
            const size_t textEnd = end == std::string::npos ? source.size() : end;
            addText(source.substr(position, textEnd - position));
            position = textEnd;
            continue;
        }
        if (source.compare(position, 4, "<!--") == 0) {
            const size_t end = source.find("-->", position + 4);
            auto comment = std::make_shared<DocumentNode>(EntityType::HTMLComment);
            comment->setText(source.substr(position + 4,
                end == std::string::npos ? std::string::npos : end - position - 4));
            stack.back()->addChild(comment);
            position = end == std::string::npos ? source.size() : end + 3;
            continue;
        }
        if (source.compare(position, 2, "<!") == 0 ||
            source.compare(position, 2, "<?") == 0) {
            const size_t end = source.find('>', position + 2);
            auto declaration = std::make_shared<DocumentNode>(
                source.compare(position, 2, "<!") == 0
                    ? EntityType::HTMLDoctype : EntityType::Comment);
            declaration->setText(source.substr(position,
                end == std::string::npos ? std::string::npos : end - position + 1));
            stack.back()->addChild(declaration);
            position = end == std::string::npos ? source.size() : end + 1;
            continue;
        }
        if (source.compare(position, 2, "</") == 0) {
            size_t cursor = position + 2;
            while (cursor < source.size() &&
                   std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;
            const size_t start = cursor;
            while (cursor < source.size() &&
                   tagNameChar(static_cast<unsigned char>(source[cursor]))) ++cursor;
            if (cursor == start) {
                addText("<");
                ++position;
                continue;
            }
            const std::string closing = lower(source.substr(start, cursor - start));
            const size_t end = source.find('>', cursor);
            position = end == std::string::npos ? source.size() : end + 1;
            closeOpen(closing);
            continue;
        }

        size_t cursor = position + 1;
        while (cursor < source.size() &&
               std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;
        const size_t nameStart = cursor;
        while (cursor < source.size() &&
               tagNameChar(static_cast<unsigned char>(source[cursor]))) ++cursor;
        if (cursor == nameStart) {
            addText("<");
            ++position;
            continue;
        }
        const std::string tag = lower(source.substr(nameStart, cursor - nameStart));
        if (stack.size() > 1 && stack.back()->name() == "p" &&
            autoCloseOnBlock.count(tag)) {
            stack.pop_back();
        }
        if (tag == "li" || tag == "option" || tag == "tr" ||
            tag == "td" || tag == "th") {
            closeOpen(tag);
        }

        auto element = std::make_shared<DocumentNode>(EntityType::HTMLElement);
        element->setName(tag);
        bool selfClosing = false;
        bool malformed = false;
        while (cursor < source.size()) {
            while (cursor < source.size() &&
                   std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;
            if (cursor == source.size()) {
                malformed = true;
                break;
            }
            if (source[cursor] == '>') {
                ++cursor;
                break;
            }
            if (source[cursor] == '/' && cursor + 1 < source.size() &&
                source[cursor + 1] == '>') {
                selfClosing = true;
                cursor += 2;
                break;
            }
            const size_t attrStart = cursor;
            while (cursor < source.size() &&
                   !std::isspace(static_cast<unsigned char>(source[cursor])) &&
                   source[cursor] != '=' && source[cursor] != '>' &&
                   source[cursor] != '/') ++cursor;
            if (cursor == attrStart) {
                ++cursor;
                continue;
            }
            const std::string attr = lower(source.substr(attrStart, cursor - attrStart));
            while (cursor < source.size() &&
                   std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;
            std::string value;
            if (cursor < source.size() && source[cursor] == '=') {
                ++cursor;
                while (cursor < source.size() &&
                       std::isspace(static_cast<unsigned char>(source[cursor]))) ++cursor;
                if (cursor < source.size() &&
                    (source[cursor] == '"' || source[cursor] == '\'')) {
                    const char quote = source[cursor++];
                    const size_t valueStart = cursor;
                    while (cursor < source.size() && source[cursor] != quote) ++cursor;
                    value = decodeEntities(source.substr(valueStart, cursor - valueStart), true);
                    if (cursor < source.size()) ++cursor;
                    else malformed = true;
                } else {
                    const size_t valueStart = cursor;
                    while (cursor < source.size() &&
                           !std::isspace(static_cast<unsigned char>(source[cursor])) &&
                           source[cursor] != '>') ++cursor;
                    value = decodeEntities(source.substr(valueStart, cursor - valueStart), true);
                }
            }
            if (!element->hasAttribute(attr)) element->setAttribute(attr, value);
        }
        if (malformed) {
            addText(source.substr(position, cursor - position));
            position = cursor;
            continue;
        }
        stack.back()->addChild(element);
        position = cursor;
        if (!selfClosing && !isVoidHtmlTag(tag)) {
            stack.push_back(element);
            if (tag == "script" || tag == "style") {
                const std::string closing = "</" + tag;
                const std::string lowered = lower(source.substr(position));
                size_t rawEnd = lowered.find(closing);
                while (rawEnd != std::string::npos) {
                    const size_t afterName = rawEnd + closing.size();
                    if (afterName == lowered.size() ||
                        std::isspace(static_cast<unsigned char>(lowered[afterName])) ||
                        lowered[afterName] == '>' || lowered[afterName] == '/') {
                        break;
                    }
                    rawEnd = lowered.find(closing, rawEnd + closing.size());
                }
                const size_t textEnd = rawEnd == std::string::npos
                    ? source.size() : position + rawEnd;
                addText(source.substr(position, textEnd - position), true);
                position = textEnd;
            }
        }
    }
    return document;
}

inline std::string serializeMarkupNode(
    const DocumentNode::Ptr& node, bool html, bool parentRaw = false) {
    if (!node) return "";
    const EntityType type = node->type();
    if (type == EntityType::Text) {
        return parentRaw ? node->text() : escapeMarkup(node->text());
    }
    if (type == EntityType::Comment || type == EntityType::HTMLComment ||
        type == EntityType::XMLComment) {
        return "<!--" + node->text() + "-->";
    }
    if (type == EntityType::HTMLDoctype || type == EntityType::XMLDoctype) {
        return node->text();
    }
    if (type == EntityType::XMLCDATA) {
        return "<![CDATA[" + node->text() + "]]>";
    }
    if (type == EntityType::XMLProcessingInstruction) {
        return "<?" + node->name() +
            (node->text().empty() ? "" : " " + node->text()) + "?>";
    }
    if (type == EntityType::HTMLDocument || type == EntityType::XMLDocument ||
        type == EntityType::XSLDocument || type == EntityType::Document ||
        type == EntityType::Root) {
        std::string result = node->text();
        for (const auto& child : node->children()) {
            result += serializeMarkupNode(child, html);
        }
        return result;
    }

    const std::string name = node->name();
    std::string result = "<" + name;
    for (const auto& [key, value] : node->attributes()) {
        result += " " + key + "=\"" + escapeMarkup(value, true) + "\"";
    }
    const bool isHtmlElement = html && isVoidHtmlTag(lower(name));
    if (isHtmlElement) {
        result += ">";
        return result;
    }
    if (node->children().empty() && node->text().empty()) {
        return result + (html ? "></" + name + ">" : "/>");
    }
    result += ">";
    const bool raw = html && (lower(name) == "script" || lower(name) == "style");
    if (!node->text().empty()) {
        result += raw ? node->text() : escapeMarkup(node->text());
    }
    for (const auto& child : node->children()) {
        result += serializeMarkupNode(child, html, raw);
    }
    return result + "</" + name + ">";
}

class JsonParser {
public:
    explicit JsonParser(const std::string& source)
        : m_source(source), m_position(hasUtf8Bom(source) ? 3 : 0) {}

    DocumentNode::Ptr parse() {
        requireValidUtf8(m_source, "JSON");
        auto document = std::make_shared<DocumentNode>(EntityType::JSONDocument);
        skipSpace();
        document->addChild(parseValue(0));
        skipSpace();
        if (m_position != m_source.size()) fail("Trailing characters");
        return document;
    }

private:
    const std::string& m_source;
    size_t m_position = 0;

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("JSON parse error: " + message +
                                 " at byte " + std::to_string(m_position));
    }

    void skipSpace() {
        while (m_position < m_source.size() &&
               (m_source[m_position] == ' ' || m_source[m_position] == '\t' ||
                m_source[m_position] == '\r' || m_source[m_position] == '\n')) {
            ++m_position;
        }
    }

    char take() {
        if (m_position == m_source.size()) fail("Unexpected end of input");
        return m_source[m_position++];
    }

    static int hexDigit(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    }

    std::uint32_t readHexCodepoint() {
        if (m_source.size() - m_position < 4) fail("Incomplete Unicode escape");
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const int digit = hexDigit(m_source[m_position++]);
            if (digit < 0) fail("Invalid Unicode escape");
            value = (value << 4) | static_cast<unsigned>(digit);
        }
        return value;
    }

    std::string parseString() {
        if (take() != '"') fail("Expected string");
        std::string result;
        while (m_position < m_source.size()) {
            const unsigned char ch = static_cast<unsigned char>(take());
            if (ch == '"') return result;
            if (ch < 0x20) fail("Control character in string");
            if (ch != '\\') {
                result += static_cast<char>(ch);
                continue;
            }
            const char escaped = take();
            switch (escaped) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    std::uint32_t codepoint = readHexCodepoint();
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        if (m_source.size() - m_position < 6 ||
                            m_source[m_position] != '\\' ||
                            m_source[m_position + 1] != 'u') {
                            fail("Unpaired high surrogate");
                        }
                        m_position += 2;
                        const std::uint32_t low = readHexCodepoint();
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail("Invalid low surrogate");
                        }
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) +
                            (low - 0xDC00);
                    } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                        fail("Unpaired low surrogate");
                    }
                    appendUtf8(result, codepoint);
                    break;
                }
                default: fail("Invalid string escape");
            }
        }
        fail("Unterminated string");
    }

    DocumentNode::Ptr parseValue(size_t depth) {
        if (depth > 512) fail("Maximum nesting depth exceeded");
        skipSpace();
        if (m_position == m_source.size()) fail("Expected value");
        auto value = std::make_shared<DocumentNode>(EntityType::JSONValue);
        const char ch = m_source[m_position];
        if (ch == '{') {
            value->setAttribute("kind", "object");
            ++m_position;
            skipSpace();
            if (m_position < m_source.size() && m_source[m_position] == '}') {
                ++m_position;
                return value;
            }
            while (true) {
                skipSpace();
                if (m_position == m_source.size() || m_source[m_position] != '"') {
                    fail("Expected object key");
                }
                const std::string key = parseString();
                skipSpace();
                if (take() != ':') fail("Expected ':' after object key");
                auto child = parseValue(depth + 1);
                child->setName(key);
                value->addChild(child);
                skipSpace();
                const char separator = take();
                if (separator == '}') break;
                if (separator != ',') fail("Expected ',' or '}'");
            }
        } else if (ch == '[') {
            value->setAttribute("kind", "array");
            ++m_position;
            skipSpace();
            if (m_position < m_source.size() && m_source[m_position] == ']') {
                ++m_position;
                return value;
            }
            while (true) {
                value->addChild(parseValue(depth + 1));
                skipSpace();
                const char separator = take();
                if (separator == ']') break;
                if (separator != ',') fail("Expected ',' or ']'");
            }
        } else if (ch == '"') {
            value->setAttribute("kind", "string");
            value->setText(parseString());
        } else if (m_source.compare(m_position, 4, "true") == 0) {
            m_position += 4;
            value->setAttribute("kind", "boolean");
            value->setText("true");
        } else if (m_source.compare(m_position, 5, "false") == 0) {
            m_position += 5;
            value->setAttribute("kind", "boolean");
            value->setText("false");
        } else if (m_source.compare(m_position, 4, "null") == 0) {
            m_position += 4;
            value->setAttribute("kind", "null");
        } else {
            const size_t start = m_position;
            if (m_source[m_position] == '-') ++m_position;
            if (m_position == m_source.size()) fail("Incomplete number");
            if (m_source[m_position] == '0') {
                ++m_position;
                if (m_position < m_source.size() &&
                    std::isdigit(static_cast<unsigned char>(m_source[m_position]))) {
                    fail("Leading zero in number");
                }
            } else {
                const size_t integerStart = m_position;
                while (m_position < m_source.size() &&
                       std::isdigit(static_cast<unsigned char>(m_source[m_position]))) {
                    ++m_position;
                }
                if (integerStart == m_position) fail("Invalid number");
            }
            if (m_position < m_source.size() && m_source[m_position] == '.') {
                ++m_position;
                const size_t fractionStart = m_position;
                while (m_position < m_source.size() &&
                       std::isdigit(static_cast<unsigned char>(m_source[m_position]))) {
                    ++m_position;
                }
                if (fractionStart == m_position) fail("Missing fraction digits");
            }
            if (m_position < m_source.size() &&
                (m_source[m_position] == 'e' || m_source[m_position] == 'E')) {
                ++m_position;
                if (m_position < m_source.size() &&
                    (m_source[m_position] == '+' || m_source[m_position] == '-')) {
                    ++m_position;
                }
                const size_t exponentStart = m_position;
                while (m_position < m_source.size() &&
                       std::isdigit(static_cast<unsigned char>(m_source[m_position]))) {
                    ++m_position;
                }
                if (exponentStart == m_position) fail("Missing exponent digits");
            }
            value->setAttribute("kind", "number");
            value->setText(m_source.substr(start, m_position - start));
        }
        return value;
    }
};

inline std::string escapeJson(const std::string& input) {
    static const char hex[] = "0123456789abcdef";
    std::string output = "\"";
    for (unsigned char ch : input) {
        switch (ch) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (ch < 0x20) {
                    output += "\\u00";
                    output += hex[ch >> 4];
                    output += hex[ch & 0x0F];
                } else {
                    output += static_cast<char>(ch);
                }
        }
    }
    output += '"';
    return output;
}

inline std::string serializeJsonValue(const DocumentNode::Ptr& value) {
    if (!value) return "null";
    const auto kind = value->getAttribute("kind").value_or("string");
    if (kind == "object") {
        std::string result = "{";
        for (size_t i = 0; i < value->children().size(); ++i) {
            if (i) result += ',';
            const auto& child = value->children()[i];
            result += escapeJson(child->name()) + ":" + serializeJsonValue(child);
        }
        return result + "}";
    }
    if (kind == "array") {
        std::string result = "[";
        for (size_t i = 0; i < value->children().size(); ++i) {
            if (i) result += ',';
            result += serializeJsonValue(value->children()[i]);
        }
        return result + "]";
    }
    if (kind == "string") return escapeJson(value->text());
    if (kind == "number") {
        static const std::regex numberPattern(
            R"(-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
        if (!std::regex_match(value->text(), numberPattern)) {
            throw std::runtime_error("Invalid JSON number in edited document");
        }
        return value->text();
    }
    if (kind == "boolean") {
        if (value->text() != "true" && value->text() != "false") {
            throw std::runtime_error("Invalid JSON boolean in edited document");
        }
        return value->text();
    }
    if (kind == "null") return "null";
    throw std::runtime_error("Unknown JSON value kind: " + kind);
}

class YamlParser {
public:
    explicit YamlParser(const std::string& source) {
        requireValidUtf8(source, "YAML");
        std::istringstream input(source);
        std::string line;
        size_t number = 0;
        bool contentSeen = false;
        bool documentEnded = false;
        while (std::getline(input, line)) {
            ++number;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (number == 1 && hasUtf8Bom(line)) line.erase(0, 3);
            size_t indent = 0;
            while (indent < line.size() && line[indent] == ' ') ++indent;
            if (indent < line.size() && line[indent] == '\t') {
                failAt(number, "Tabs are not allowed for indentation");
            }
            std::string content = stripComment(line.substr(indent));
            content = trim(content);
            if (content.empty()) continue;
            if (content == "---") {
                if (contentSeen) failAt(number, "Multiple documents are unsupported");
                continue;
            }
            if (content == "...") {
                documentEnded = true;
                continue;
            }
            if (documentEnded) failAt(number, "Content after document end marker");
            contentSeen = true;
            m_lines.push_back({indent, std::move(content), number});
        }
    }

    DocumentNode::Ptr parse() {
        auto document = std::make_shared<DocumentNode>(EntityType::YAMLDocument);
        if (m_lines.empty()) return document;
        size_t index = 0;
        auto value = parseBlock(index, m_lines.front().indent, 0);
        if (index != m_lines.size()) failLine(m_lines[index], "Unexpected indentation");
        document->addChild(value);
        return document;
    }

private:
    struct Line {
        size_t indent;
        std::string text;
        size_t number;
    };

    std::vector<Line> m_lines;

    [[noreturn]] static void failAt(size_t line, const std::string& message) {
        throw std::runtime_error("YAML parse error at line " +
                                 std::to_string(line) + ": " + message);
    }

    [[noreturn]] static void failLine(const Line& line, const std::string& message) {
        failAt(line.number, message);
    }

    static std::string stripComment(const std::string& input) {
        char quote = 0;
        bool escaped = false;
        for (size_t i = 0; i < input.size(); ++i) {
            const char ch = input[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (quote == '"' && ch == '\\') {
                escaped = true;
                continue;
            }
            if (quote != 0) {
                if (ch == quote) {
                    if (quote == '\'' && i + 1 < input.size() &&
                        input[i + 1] == '\'') {
                        ++i;
                    } else {
                        quote = 0;
                    }
                }
                continue;
            }
            if (ch == '"' || ch == '\'') quote = ch;
            else if (ch == '#' && (i == 0 ||
                     std::isspace(static_cast<unsigned char>(input[i - 1])))) {
                return input.substr(0, i);
            }
        }
        return input;
    }

    static size_t mappingColon(const std::string& text) {
        char quote = 0;
        bool escaped = false;
        int nested = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            const char ch = text[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (quote == '"' && ch == '\\') {
                escaped = true;
                continue;
            }
            if (quote != 0) {
                if (ch == quote) quote = 0;
                continue;
            }
            if (ch == '"' || ch == '\'') quote = ch;
            else if (ch == '[' || ch == '{') ++nested;
            else if (ch == ']' || ch == '}') --nested;
            else if (ch == ':' && nested == 0 &&
                     (i + 1 == text.size() ||
                      std::isspace(static_cast<unsigned char>(text[i + 1])))) {
                return i;
            }
        }
        return std::string::npos;
    }

    static bool isSequenceLine(const std::string& text) {
        return text == "-" || (text.size() > 1 && text[0] == '-' &&
                               std::isspace(static_cast<unsigned char>(text[1])));
    }

    std::vector<std::string> splitFlowItems(const std::string& source) {
        std::vector<std::string> items;
        size_t start = 0;
        std::vector<char> closing;
        char quote = 0;
        bool escaped = false;
        for (size_t i = 0; i < source.size(); ++i) {
            const char ch = source[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (quote == '"' && ch == '\\') {
                escaped = true;
                continue;
            }
            if (quote != 0) {
                if (ch == quote) {
                    if (quote == '\'' && i + 1 < source.size() &&
                        source[i + 1] == '\'') {
                        ++i;
                    } else {
                        quote = 0;
                    }
                }
                continue;
            }
            if (ch == '"' || ch == '\'') quote = ch;
            else if (ch == '[') closing.push_back(']');
            else if (ch == '{') closing.push_back('}');
            else if (ch == ']' || ch == '}') {
                if (closing.empty() || closing.back() != ch) {
                    throw std::runtime_error(
                        "YAML parse error: mismatched flow collection delimiter");
                }
                closing.pop_back();
            } else if (ch == ',' && closing.empty()) {
                items.push_back(trim(source.substr(start, i - start)));
                start = i + 1;
            }
        }
        if (quote != 0 || escaped || !closing.empty()) {
            throw std::runtime_error("YAML parse error: unclosed flow collection");
        }
        const std::string last = trim(source.substr(start));
        if (!last.empty()) items.push_back(last);
        return items;
    }

    static size_t flowColon(const std::string& source) {
        char quote = 0;
        bool escaped = false;
        int nesting = 0;
        for (size_t i = 0; i < source.size(); ++i) {
            const char ch = source[i];
            if (escaped) escaped = false;
            else if (quote == '"' && ch == '\\') escaped = true;
            else if (quote != 0) {
                if (ch == quote) quote = 0;
            } else if (ch == '"' || ch == '\'') quote = ch;
            else if (ch == '[' || ch == '{') ++nesting;
            else if (ch == ']' || ch == '}') --nesting;
            else if (ch == ':' && nesting == 0) return i;
        }
        return std::string::npos;
    }

    DocumentNode::Ptr makeScalar(const std::string& source, size_t depth = 0) {
        if (depth > 128) {
            throw std::runtime_error("YAML parse error: maximum flow nesting depth exceeded");
        }
        const std::string value = trim(source);
        auto node = std::make_shared<DocumentNode>(EntityType::YAMLScalar);
        if (value.empty() || value == "~" || value == "null" ||
            value == "Null" || value == "NULL") {
            node->setAttribute("kind", "null");
            return node;
        }
        const std::string normalized = lower(value);
        if (normalized == "true" || normalized == "false") {
            node->setAttribute("kind", "boolean");
            node->setText(normalized);
            return node;
        }
        if (value.front() == '"') {
            auto parsed = JsonParser(value).parse();
            if (parsed->children().empty() ||
                parsed->children()[0]->getAttribute("kind").value_or("") != "string") {
                throw std::runtime_error("YAML parse error: invalid quoted string");
            }
            node->setAttribute("kind", "string");
            node->setText(parsed->children()[0]->text());
            return node;
        }
        if (value.front() == '\'') {
            if (value.size() < 2 || value.back() != '\'') {
                throw std::runtime_error("YAML parse error: unterminated single-quoted string");
            }
            std::string decoded;
            for (size_t i = 1; i + 1 < value.size(); ++i) {
                if (value[i] == '\'' && i + 1 < value.size() - 1 &&
                    value[i + 1] == '\'') {
                    decoded += '\'';
                    ++i;
                } else {
                    decoded += value[i];
                }
            }
            node->setAttribute("kind", "string");
            node->setText(decoded);
            return node;
        }
        if (value == "[]") {
            auto sequence = std::make_shared<DocumentNode>(EntityType::YAMLSequence);
            sequence->setAttribute("kind", "sequence");
            return sequence;
        }
        if (value == "{}") {
            auto mapping = std::make_shared<DocumentNode>(EntityType::YAMLMapping);
            mapping->setAttribute("kind", "mapping");
            return mapping;
        }
        if (value.front() == '[' && value.back() == ']') {
            auto sequence = std::make_shared<DocumentNode>(EntityType::YAMLSequence);
            sequence->setAttribute("kind", "sequence");
            const auto items = splitFlowItems(value.substr(1, value.size() - 2));
            for (const auto& item : items) {
                if (item.empty()) {
                    throw std::runtime_error("YAML parse error: empty flow sequence item");
                }
                sequence->addChild(makeScalar(item, depth + 1));
            }
            return sequence;
        }
        if (value.front() == '{' && value.back() == '}') {
            auto mapping = std::make_shared<DocumentNode>(EntityType::YAMLMapping);
            mapping->setAttribute("kind", "mapping");
            const auto items = splitFlowItems(value.substr(1, value.size() - 2));
            for (const auto& item : items) {
                const size_t colon = flowColon(item);
                if (colon == std::string::npos) {
                    throw std::runtime_error("YAML parse error: expected ':' in flow mapping");
                }
                auto key = makeScalar(trim(item.substr(0, colon)), depth + 1);
                if (key->getAttribute("kind").value_or("") != "string") {
                    throw std::runtime_error("YAML parse error: mapping keys must be strings");
                }
                const std::string keyName = key->text();
                if (std::any_of(mapping->children().begin(), mapping->children().end(),
                        [&](const DocumentNode::Ptr& child) {
                            return child->name() == keyName;
                        })) {
                    throw std::runtime_error("YAML parse error: duplicate flow mapping key");
                }
                auto child = makeScalar(trim(item.substr(colon + 1)), depth + 1);
                child->setName(keyName);
                mapping->addChild(child);
            }
            return mapping;
        }
        if (value.front() == '[' || value.front() == '{' ||
            value.front() == '&' || value.front() == '*' || value.front() == '!') {
            throw std::runtime_error(
                "YAML parse error: flow collections, anchors, aliases, and tags are unsupported");
        }
        static const std::regex numberPattern(
            R"([-+]?(?:0|[1-9](?:_?[0-9])*)(?:\.[0-9](?:_?[0-9])*)?(?:[eE][-+]?[0-9]+)?)");
        if (std::regex_match(value, numberPattern)) {
            std::string numeric = value;
            numeric.erase(std::remove(numeric.begin(), numeric.end(), '_'), numeric.end());
            if (!numeric.empty() && numeric[0] == '+') numeric.erase(numeric.begin());
            node->setAttribute("kind", "number");
            node->setText(numeric);
            return node;
        }
        node->setAttribute("kind", "string");
        node->setText(value);
        return node;
    }

    DocumentNode::Ptr parseBlock(size_t& index, size_t indent, size_t depth) {
        if (depth > 256) failLine(m_lines[index], "Maximum nesting depth exceeded");
        if (index >= m_lines.size() || m_lines[index].indent != indent) {
            if (index < m_lines.size()) failLine(m_lines[index], "Invalid indentation");
            throw std::runtime_error("YAML parse error: expected block");
        }
        if (isSequenceLine(m_lines[index].text)) {
            auto sequence = std::make_shared<DocumentNode>(EntityType::YAMLSequence);
            sequence->setAttribute("kind", "sequence");
            while (index < m_lines.size() && m_lines[index].indent == indent &&
                   isSequenceLine(m_lines[index].text)) {
                const Line line = m_lines[index++];
                const std::string rest = line.text == "-"
                    ? "" : trim(line.text.substr(1));
                if (rest.empty()) {
                    if (index < m_lines.size() && m_lines[index].indent > indent) {
                        sequence->addChild(parseBlock(index, m_lines[index].indent, depth + 1));
                    } else {
                        sequence->addChild(makeScalar("null"));
                    }
                } else {
                    const size_t colon = mappingColon(rest);
                    if (colon != std::string::npos) {
                        auto mapping = std::make_shared<DocumentNode>(EntityType::YAMLMapping);
                        mapping->setAttribute("kind", "mapping");
                        parsePair(mapping, trim(rest.substr(0, colon)),
                                  trim(rest.substr(colon + 1)), index, indent + 2, depth + 1,
                                  line);
                        while (index < m_lines.size() &&
                               m_lines[index].indent == indent + 2 &&
                               !isSequenceLine(m_lines[index].text)) {
                            parseMappingEntry(mapping, index, indent + 2, depth + 1);
                        }
                        sequence->addChild(mapping);
                    } else {
                        sequence->addChild(makeScalar(rest));
                        if (index < m_lines.size() && m_lines[index].indent > indent) {
                            failLine(m_lines[index], "Scalar sequence item cannot have nested content");
                        }
                    }
                }
            }
            return sequence;
        }

        if (mappingColon(m_lines[index].text) == std::string::npos) {
            const Line scalarLine = m_lines[index++];
            auto scalar = makeScalar(scalarLine.text);
            if (index < m_lines.size() && m_lines[index].indent >= indent) {
                failLine(m_lines[index], "A scalar document cannot have sibling content");
            }
            return scalar;
        }

        auto mapping = std::make_shared<DocumentNode>(EntityType::YAMLMapping);
        mapping->setAttribute("kind", "mapping");
        while (index < m_lines.size() && m_lines[index].indent == indent &&
               !isSequenceLine(m_lines[index].text)) {
            parseMappingEntry(mapping, index, indent, depth);
        }
        return mapping;
    }

    void parseMappingEntry(const DocumentNode::Ptr& mapping, size_t& index,
                           size_t indent, size_t depth) {
        const Line line = m_lines[index++];
        const size_t colon = mappingColon(line.text);
        if (colon == std::string::npos) failLine(line, "Expected mapping key followed by ':'");
        parsePair(mapping, trim(line.text.substr(0, colon)),
                  trim(line.text.substr(colon + 1)), index, indent, depth, line);
    }

    void parsePair(const DocumentNode::Ptr& mapping, const std::string& keySource,
                   const std::string& valueSource, size_t& index, size_t indent,
                   size_t depth, const Line& line) {
        if (keySource.empty()) failLine(line, "Empty mapping key");
        auto key = makeScalar(keySource);
        if (key->getAttribute("kind").value_or("") != "string") {
            failLine(line, "Mapping keys must be strings");
        }
        const std::string name = key->text();
        if (std::any_of(mapping->children().begin(), mapping->children().end(),
                [&](const DocumentNode::Ptr& child) { return child->name() == name; })) {
            failLine(line, "Duplicate mapping key");
        }

        DocumentNode::Ptr value;
        if (valueSource.empty()) {
            if (index < m_lines.size() && m_lines[index].indent > indent) {
                value = parseBlock(index, m_lines[index].indent, depth + 1);
            } else {
                value = makeScalar("null");
            }
        } else if (valueSource == "|" || valueSource == ">") {
            std::string block;
            const size_t blockIndent = index < m_lines.size() &&
                m_lines[index].indent > indent ? m_lines[index].indent : indent + 1;
            size_t previousLine = line.number;
            while (index < m_lines.size() && m_lines[index].indent >= blockIndent) {
                if (m_lines[index].number != previousLine + 1) {
                    failLine(m_lines[index],
                        "Blank or comment lines inside block scalars are unsupported");
                }
                if (!block.empty()) block += valueSource == "|" ? "\n" : " ";
                block += m_lines[index].text;
                previousLine = m_lines[index].number;
                ++index;
            }
            value = std::make_shared<DocumentNode>(EntityType::YAMLScalar);
            value->setAttribute("kind", "string");
            value->setText(block);
        } else {
            value = makeScalar(valueSource);
            if (index < m_lines.size() && m_lines[index].indent > indent) {
                failLine(m_lines[index], "Scalar mapping value cannot have nested content");
            }
        }
        value->setName(name);
        mapping->addChild(value);
    }
};

inline std::string yamlQuote(const std::string& value, bool key = false) {
    static const std::regex safe(
        R"([A-Za-z_][A-Za-z0-9_.-]*|[0-9]+(?:\.[0-9]+)?)");
    static const std::regex numeric(
        R"([-+]?(?:0|[1-9](?:_?[0-9])*)(?:\.[0-9](?:_?[0-9])*)?(?:[eE][-+]?[0-9]+)?)");
    const std::string normalized = lower(value);
    const bool ambiguous = normalized == "null" || normalized == "true" ||
        normalized == "false" || value == "~" ||
        std::regex_match(value, numeric);
    if (!value.empty() && !ambiguous && std::regex_match(value, safe) &&
        value.front() != '-') return value;
    (void)key;
    return escapeJson(value);
}

inline std::string serializeYamlValue(
    const DocumentNode::Ptr& value, size_t indent = 0) {
    if (!value) return "null";
    const std::string kind = value->getAttribute("kind").value_or("string");
    if (kind == "mapping") {
        if (value->children().empty()) return "{}";
        std::string output;
        for (const auto& child : value->children()) {
            const std::string childKind = child->getAttribute("kind").value_or("string");
            output += std::string(indent, ' ') + yamlQuote(child->name(), true) + ":";
            if ((childKind == "mapping" || childKind == "sequence") &&
                !child->children().empty()) {
                output += "\n";
                output += serializeYamlValue(child, indent + 2);
                if (output.back() != '\n') output += '\n';
            } else {
                output += " " + serializeYamlValue(child, indent + 2);
                output += '\n';
            }
        }
        return output;
    }
    if (kind == "sequence") {
        if (value->children().empty()) return "[]";
        std::string output;
        for (const auto& child : value->children()) {
            const std::string childKind = child->getAttribute("kind").value_or("string");
            output += std::string(indent, ' ') + "-";
            if ((childKind == "mapping" || childKind == "sequence") &&
                !child->children().empty()) {
                output += "\n";
                output += serializeYamlValue(child, indent + 2);
                if (output.back() != '\n') output += '\n';
            } else {
                output += " " + serializeYamlValue(child, indent + 2);
                output += '\n';
            }
        }
        return output;
    }
    if (kind == "null") return "null";
    if (kind == "boolean") {
        if (value->text() != "true" && value->text() != "false") {
            throw std::runtime_error("Invalid YAML boolean in edited document");
        }
        return value->text();
    }
    if (kind == "number") {
        static const std::regex numberPattern(
            R"([-+]?(?:0|[1-9](?:_?[0-9])*)(?:\.[0-9](?:_?[0-9])*)?(?:[eE][-+]?[0-9]+)?)");
        if (!std::regex_match(value->text(), numberPattern)) {
            throw std::runtime_error("Invalid YAML number in edited document");
        }
        return value->text();
    }
    return yamlQuote(value->text());
}

class CssParser {
public:
    explicit CssParser(const std::string& source)
        : m_source(source), m_position(hasUtf8Bom(source) ? 3 : 0) {}

    DocumentNode::Ptr parse() {
        requireValidUtf8(m_source, "CSS");
        auto document = std::make_shared<DocumentNode>(EntityType::CSSDocument);
        document->setName("css");
        parseRules(document, false, 0);
        skipSpaceAndComments(document);
        if (m_position != m_source.size()) fail("Unexpected trailing input");
        return document;
    }

private:
    const std::string& m_source;
    size_t m_position = 0;

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("CSS parse error at byte " +
                                 std::to_string(m_position) + ": " + message);
    }

    void skipSpaceAndComments(const DocumentNode::Ptr& parent) {
        while (m_position < m_source.size()) {
            if (std::isspace(static_cast<unsigned char>(m_source[m_position]))) {
                ++m_position;
            } else if (m_source.compare(m_position, 2, "/*") == 0) {
                const size_t end = m_source.find("*/", m_position + 2);
                if (end == std::string::npos) fail("Unterminated comment");
                auto comment = std::make_shared<DocumentNode>(EntityType::Comment);
                comment->setName("css");
                comment->setText(m_source.substr(m_position + 2, end - m_position - 2));
                parent->addChild(comment);
                m_position = end + 2;
            } else {
                break;
            }
        }
    }

    std::string readPrelude(char& terminator, bool inBlock) {
        const size_t start = m_position;
        char quote = 0;
        bool escaped = false;
        int parentheses = 0;
        int brackets = 0;
        int customValueBraces = 0;
        while (m_position < m_source.size()) {
            const char ch = m_source[m_position];
            if (escaped) {
                escaped = false;
                ++m_position;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                ++m_position;
                continue;
            }
            if (quote != 0) {
                if (ch == quote) quote = 0;
                ++m_position;
                continue;
            }
            if (ch == '"' || ch == '\'') {
                quote = ch;
                ++m_position;
                continue;
            }
            if (m_source.compare(m_position, 2, "/*") == 0) {
                const size_t end = m_source.find("*/", m_position + 2);
                if (end == std::string::npos) fail("Unterminated comment");
                m_position = end + 2;
                continue;
            }
            if (ch == '(') ++parentheses;
            else if (ch == ')') {
                if (--parentheses < 0) fail("Unexpected ')'");
            } else if (ch == '[') ++brackets;
            else if (ch == ']') {
                if (--brackets < 0) fail("Unexpected ']'");
            } else if (inBlock && ch == '{' && parentheses == 0 && brackets == 0) {
                const std::string before = trim(m_source.substr(start, m_position - start));
                const size_t colon = declarationColon(before);
                if (customValueBraces > 0 ||
                    (!before.empty() && before.rfind("--", 0) == 0 &&
                     colon != std::string::npos)) {
                    ++customValueBraces;
                } else {
                    terminator = ch;
                    std::string prelude = m_source.substr(start, m_position - start);
                    ++m_position;
                    return trim(prelude);
                }
            } else if (ch == '}' && customValueBraces > 0) {
                --customValueBraces;
            } else if (parentheses == 0 && brackets == 0 &&
                       customValueBraces == 0 &&
                       (ch == '{' || ch == ';' || ch == '}')) {
                terminator = ch;
                std::string prelude = m_source.substr(start, m_position - start);
                ++m_position;
                return trim(prelude);
            }
            ++m_position;
        }
        if (quote != 0 || escaped || parentheses != 0 || brackets != 0 ||
            customValueBraces != 0) {
            fail("Unclosed string or delimiter");
        }
        terminator = '\0';
        return trim(m_source.substr(start));
    }

    static size_t declarationColon(const std::string& declaration) {
        char quote = 0;
        bool escaped = false;
        int nesting = 0;
        for (size_t i = 0; i < declaration.size(); ++i) {
            const char ch = declaration[i];
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (quote != 0) {
                if (ch == quote) quote = 0;
            } else if (ch == '"' || ch == '\'') {
                quote = ch;
            } else if (ch == '(' || ch == '[') {
                ++nesting;
            } else if (ch == ')' || ch == ']') {
                --nesting;
            } else if (ch == ':' && nesting == 0) {
                return i;
            }
        }
        return std::string::npos;
    }

    void parseRules(const DocumentNode::Ptr& parent, bool inBlock, size_t depth) {
        if (depth > 256) fail("Maximum CSS nesting depth exceeded");
        while (true) {
            skipSpaceAndComments(parent);
            if (m_position == m_source.size()) {
                if (inBlock) fail("Unclosed rule block");
                return;
            }
            if (m_source[m_position] == '}') {
                if (!inBlock) fail("Unexpected '}'");
                ++m_position;
                return;
            }
            char terminator = '\0';
            const std::string prelude = readPrelude(terminator, inBlock);
            if (terminator == '}') {
                if (prelude.empty()) {
                    if (!inBlock) fail("Unexpected '}'");
                    return;
                }
                parseDeclaration(parent, prelude);
                if (!inBlock) fail("Declaration outside a rule");
                return;
            }
            if (prelude.empty()) {
                if (terminator == ';') continue;
                if (terminator == '\0') {
                    if (inBlock) fail("Unclosed rule block");
                    return;
                }
                fail("Empty CSS rule");
            }
            if (terminator == '{') {
                auto rule = std::make_shared<DocumentNode>(EntityType::CSSRule);
                rule->setName(prelude);
                rule->setAttribute("block", "true");
                if (prelude.front() == '@') rule->setAttribute("at_rule", "true");
                parent->addChild(rule);
                parseRules(rule, true, depth + 1);
                continue;
            }
            if (terminator == ';') {
                if (prelude.front() == '@') {
                    auto atRule = std::make_shared<DocumentNode>(EntityType::CSSRule);
                    atRule->setName(prelude);
                    atRule->setAttribute("at_rule", "true");
                    parent->addChild(atRule);
                } else {
                    if (!inBlock) fail("Declaration outside a rule");
                    parseDeclaration(parent, prelude);
                }
                continue;
            }
            if (terminator == '\0') {
                if (inBlock) fail("Unclosed rule block");
                if (prelude.front() == '@') {
                    auto atRule = std::make_shared<DocumentNode>(EntityType::CSSRule);
                    atRule->setName(prelude);
                    atRule->setAttribute("at_rule", "true");
                    parent->addChild(atRule);
                    return;
                }
                fail("Missing ';' or rule block");
            }
        }
    }

    void parseDeclaration(const DocumentNode::Ptr& parent,
                         const std::string& declaration) {
        const size_t colon = declarationColon(declaration);
        if (colon == std::string::npos) fail("Expected ':' in declaration");
        const std::string property = trim(declaration.substr(0, colon));
        const std::string value = trim(declaration.substr(colon + 1));
        if (property.empty() || value.empty()) fail("Empty property or value");
        auto node = std::make_shared<DocumentNode>(EntityType::CSSDeclaration);
        node->setName(property);
        node->setText(value);
        parent->addChild(node);
    }
};

inline std::string serializeCss(const DocumentNode::Ptr& node, size_t indent = 0) {
    if (!node) return "";
    std::string output;
    const std::string padding(indent, ' ');
    if (node->type() == EntityType::Comment && node->name() == "css") {
        return padding + "/*" + node->text() + "*/\n";
    }
    if (node->type() == EntityType::CSSDeclaration) {
        return padding + node->name() + ": " + node->text() + ";\n";
    }
    if (node->type() == EntityType::CSSRule) {
        if (node->children().empty() &&
            node->getAttribute("block").value_or("") != "true") {
            return padding + node->name() + ";\n";
        }
        output = padding + node->name() + " {\n";
        for (const auto& child : node->children()) {
            output += serializeCss(child, indent + 2);
        }
        return output + padding + "}\n";
    }
    for (const auto& child : node->children()) {
        output += serializeCss(child, indent);
    }
    return output;
}

} // namespace document_parser_detail


//=============================================================================
// DocumentEditor - Main parsing, editing, and serialization interface
//=============================================================================
class DocumentEditor {
public:
    DocumentEditor() = default;

    // Load and dispatch by filename extension
    bool loadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Unable to open input file: " + filename;
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        if (file.bad()) {
            m_lastError = "Failed while reading input file: " + filename;
            return false;
        }

        const size_t pathSeparator = filename.find_last_of("/\\");
        const size_t dot = filename.find_last_of('.');
        std::string ext = dot == std::string::npos ||
            (pathSeparator != std::string::npos && dot < pathSeparator)
            ? "" : filename.substr(dot + 1);
        ext = document_parser_detail::lower(ext);

        if (ext == "html" || ext == "htm") {
            return parseHTML(content);
        } else if (ext == "xhtml") {
            return parseXHTML(content);
        } else if (ext == "xml") {
            return parseXML(content);
        } else if (ext == "xsl" || ext == "xslt") {
            return parseXSL(content);
        } else if (ext == "json") {
            return parseJSON(content);
        } else if (ext == "yaml" || ext == "yml") {
            return parseYAML(content);
        } else if (ext == "css") {
            return parseCSS(content);
        } else if (ext == "csv") {
            return parseCSV(content);
        } else {
            m_lastError = "Unsupported file extension: " + ext;
            return false;
        }
    }

    const std::string& lastError() const { return m_lastError; }
    DocumentNode::Ptr root() const { return m_root; }

    // Parse CSV with options
    bool parseCSV(const std::string& csv, const CSVParser::ParseOptions& options = {}) {
        auto result = CSVParser::parse(csv, options);
        if (result.success) {
            m_root = result.document;
            m_csvData = result.data;
            m_csvHeaders = result.headers;
            m_currentFormat = "csv";
            m_lastError.clear();
            return true;
        }
        m_lastError = result.error;
        return false;
    }

    bool parseHTML(const std::string& html) {
        return parseDocument("html", [&] {
            return document_parser_detail::parseHtml(html);
        });
    }

    bool parseXHTML(const std::string& xhtml) {
        return parseDocument("xhtml", [&] {
            return document_parser_detail::XmlParser(xhtml).parse();
        });
    }

    bool parseXML(const std::string& xml) {
        return parseDocument("xml", [&] {
            return document_parser_detail::XmlParser(xml).parse();
        });
    }

    bool parseXSL(const std::string& xsl) {
        return parseDocument("xsl", [&] {
            auto document = document_parser_detail::XmlParser(xsl).parse();
            document->setType(EntityType::XSLDocument);
            return document;
        });
    }

    bool parseJSON(const std::string& json) {
        return parseDocument("json", [&] {
            return document_parser_detail::JsonParser(json).parse();
        });
    }

    bool parseYAML(const std::string& yaml) {
        return parseDocument("yaml", [&] {
            return document_parser_detail::YamlParser(yaml).parse();
        });
    }

    bool parseCSS(const std::string& css) {
        return parseDocument("css", [&] {
            return document_parser_detail::CssParser(css).parse();
        });
    }

    //=========================================================================
    // CSV-Specific Operations
    //=========================================================================

    // Get CSV data as typed values
    const std::vector<std::vector<TypedValue>>& getCSVData() const { return m_csvData; }
    const std::vector<std::string>& getCSVHeaders() const { return m_csvHeaders; }

    // Get cell as typed value
    std::optional<TypedValue> getCell(size_t row, size_t col) const {
        if (row < m_csvData.size() && col < m_csvData[row].size()) {
            return m_csvData[row][col];
        }
        return std::nullopt;
    }

    // Set cell value with type detection
    bool setCell(size_t row, size_t col, const std::string& value) {
        if (row < m_csvData.size()) {
            if (col >= m_csvData[row].size()) {
                m_csvData[row].resize(col + 1);
            }
            m_csvData[row][col] = TypedValue::detect(value);

            // Update the document node
            if (m_root && row < m_root->children().size()) {
                auto rowNode = m_root->children()[row];
                if (rowNode && col < rowNode->children().size()) {
                    auto cellNode = rowNode->children()[col];
                    if (cellNode) {
                        cellNode->setTypedValue(m_csvData[row][col]);
                    }
                }
            }
            return true;
        }
        return false;
    }

    // Add row to CSV
    void addCSVRow(const std::vector<TypedValue>& row) {
        m_csvData.push_back(row);

        // Update document node
        if (m_root) {
            auto rowNode = std::make_shared<DocumentNode>(EntityType::CSVRow);
            rowNode->setAttribute("row", std::to_string(m_csvData.size() - 1));

            for (size_t i = 0; i < row.size(); ++i) {
                auto fieldNode = std::make_shared<DocumentNode>(EntityType::CSVField);
                if (i < m_csvHeaders.size()) {
                    fieldNode->setName(m_csvHeaders[i]);
                }
                fieldNode->setTypedValue(row[i]);
                rowNode->addChild(fieldNode);
            }

            m_root->addChild(rowNode);
        }
    }

    //=========================================================================
    // Query Methods
    //=========================================================================

    DocumentNode::Ptr findById(const std::string& id) {
        if (m_root) return m_root->findById(id);
        return nullptr;
    }

    DocumentNode::List findByTagName(const std::string& tagName) {
        if (m_root) return m_root->findByTagName(tagName);
        return {};
    }

    DocumentNode::List findByClassName(const std::string& className) {
        if (m_root) return m_root->findByClassName(className);
        return {};
    }
    
    DocumentNode::List findByAttribute(const std::string& attrName, const std::string& attrValue = "") {
        if (m_root) return m_root->findByAttribute(attrName, attrValue);
        return {};
    }
    
    DocumentNode::List querySelectorAll(const std::string& selector) {
        if (m_root) return m_root->querySelectorAll(selector);
        return {};
    }
    
    DocumentNode::Ptr querySelector(const std::string& selector) {
        if (m_root) return m_root->querySelector(selector);
        return nullptr;
    }
    
    // Numeric range queries
    DocumentNode::List findByNumericValue(const std::string& attrName, double minValue, double maxValue) {
        if (m_root) return m_root->findByNumericValue(attrName, minValue, maxValue);
        return {};
    }
    
    // Date range queries
    DocumentNode::List findByDateRange(const std::string& attrName,
                                       const std::chrono::system_clock::time_point& start,
                                       const std::chrono::system_clock::time_point& end) {
        if (m_root) return m_root->findByDateRange(attrName, start, end);
        return {};
    }
    
    //=========================================================================
    // Editing Methods
    //=========================================================================
    
    bool setTextById(const std::string& id, const std::string& newText) {
        auto element = findById(id);
        if (element) {
            element->setTextContent(newText);
            return true;
        }
        return false;
    }
    
    bool setTypedValueById(const std::string& id, const TypedValue& value) {
        auto element = findById(id);
        if (element) {
            element->setTypedValue(value);
            return true;
        }
        return false;
    }
    
    bool setAttributeById(const std::string& id, const std::string& attrName, const std::string& attrValue) {
        auto element = findById(id);
        if (element) {
            element->setAttribute(attrName, attrValue);
            return true;
        }
        return false;
    }
    
    bool setInnerHTMLById(const std::string& id, const std::string& html) {
        auto element = findById(id);
        if (element) {
            element->setInnerHTML(html);
            return true;
        }
        return false;
    }
    
    void batchEdit(const std::string& selector, std::function<void(DocumentNode::Ptr)> editor) {
        auto elements = querySelectorAll(selector);
        for (auto& element : elements) {
            editor(element);
        }
    }
    
    DocumentNode::Ptr addElement(DocumentNode::Ptr parent, const std::string& tagName, 
                                  const std::string& id = "", const std::string& text = "") {
        if (!parent) {
            throw std::invalid_argument("addElement requires a non-null parent");
        }
        auto element = std::make_shared<DocumentNode>(tagName, text);
        if (!id.empty()) {
            element->setId(id);
        }
        parent->addChild(element);
        return element;
    }
    
    bool deleteElementById(const std::string& id) {
        auto element = findById(id);
        if (element && element->parent()) {
            element->parent()->removeChild(element);
            return true;
        }
        return false;
    }
    
    //=========================================================================
    // Output Methods
    //=========================================================================
    
    std::string toString() const {
        if (m_currentFormat == "html") {
            return document_parser_detail::serializeMarkupNode(m_root, true);
        } else if (m_currentFormat == "xhtml" || m_currentFormat == "xml" ||
                   m_currentFormat == "xsl") {
            return document_parser_detail::serializeMarkupNode(m_root, false);
        } else if (m_currentFormat == "css") {
            return document_parser_detail::serializeCss(m_root);
        } else if (m_currentFormat == "json") {
            return m_root && !m_root->children().empty()
                ? document_parser_detail::serializeJsonValue(m_root->children().front())
                : "";
        } else if (m_currentFormat == "yaml") {
            if (!m_root || m_root->children().empty()) return "";
            std::string serialized =
                document_parser_detail::serializeYamlValue(m_root->children().front());
            if (!serialized.empty() && serialized.back() != '\n') serialized += '\n';
            return serialized;
        } else if (m_currentFormat == "csv") {
            return toCSVString();
        }
        return "";
    }
    
    std::string toCSVString(const CSVParser::ParseOptions& options = {}) const {
        CSVParser::CSVParseResult result;
        result.headers = m_csvHeaders;
        result.data = m_csvData;
        return CSVParser::serialize(result, options);
    }
    
    bool saveFile(const std::string& filename) {
        if (!m_root || m_currentFormat.empty()) {
            m_lastError = "No parsed document to save";
            return false;
        }
        std::string serialized;
        try {
            serialized = toString();
            document_parser_detail::requireValidUtf8(serialized, "Output");
            validateSerializedDocument(serialized);
        } catch (const std::runtime_error& error) {
            m_lastError = error.what();
            return false;
        }
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Unable to open output file: " + filename;
            return false;
        }
        file << serialized;
        if (!file) {
            m_lastError = "Failed while writing output file: " + filename;
            return false;
        }
        m_lastError.clear();
        return true;
    }
    
private:
    template <typename Parser>
    bool parseDocument(const std::string& format, Parser&& parser) {
        try {
            auto parsed = parser();
            if (!parsed) {
                m_lastError = "Parser returned an empty document";
                return false;
            }
            m_root = std::move(parsed);
            m_currentFormat = format;
            m_csvData.clear();
            m_csvHeaders.clear();
            m_lastError.clear();
            return true;
        } catch (const std::runtime_error& error) {
            m_lastError = error.what();
            return false;
        }
    }

    void validateSerializedDocument(const std::string& serialized) const {
        if (m_currentFormat == "xml" || m_currentFormat == "xhtml" ||
            m_currentFormat == "xsl") {
            document_parser_detail::XmlParser(serialized).parse();
        } else if (m_currentFormat == "json") {
            document_parser_detail::JsonParser(serialized).parse();
        } else if (m_currentFormat == "yaml") {
            document_parser_detail::YamlParser(serialized).parse();
        } else if (m_currentFormat == "css") {
            document_parser_detail::CssParser(serialized).parse();
        } else if (m_currentFormat == "csv") {
            const auto result = CSVParser::parse(serialized);
            if (!result.success) {
                throw std::runtime_error(result.error);
            }
        } else if (m_currentFormat == "html") {
            document_parser_detail::parseHtml(serialized);
        }
    }

    DocumentNode::Ptr m_root;
    std::string m_currentFormat;
    std::string m_lastError;
    std::vector<std::vector<TypedValue>> m_csvData;
    std::vector<std::string> m_csvHeaders;
    
};

// CSS Selector implementation
inline DocumentNode::List DocumentNode::querySelectorAll(const std::string& selector) {
    List results;
    
    // Simple CSS selector parsing for demonstration
    std::regex idRegex(R"(#([a-zA-Z][a-zA-Z0-9_-]*))");
    std::regex classRegex(R"(\.([a-zA-Z][a-zA-Z0-9_-]*))");
    std::regex tagRegex(R"(^([a-zA-Z][a-zA-Z0-9]*))");
    
    std::smatch match;
    std::string remaining = selector;
    
    if (std::regex_search(remaining, match, idRegex)) {
        // Find by ID
        auto element = findById(match[1].str());
        if (element) results.push_back(element);
    } else if (std::regex_search(remaining, match, classRegex)) {
        // Find by class
        results = findByClassName(match[1].str());
    } else if (std::regex_search(remaining, match, tagRegex)) {
        // Find by tag name
        results = findByTagName(match[1].str());
    }
    
    return results;
}

inline DocumentNode::Ptr DocumentNode::querySelector(const std::string& selector) {
    auto results = querySelectorAll(selector);
    return results.empty() ? nullptr : results[0];
}

inline void DocumentNode::setInnerHTML(const std::string& html) {
    auto fragment = document_parser_detail::parseHtml(html);
    clearChildren();
    m_text.clear();
    for (const auto& child : fragment->children()) {
        addChild(child->clone(true));
    }
}

inline std::string DocumentNode::getInnerHTML() const {
    const bool raw = m_type == EntityType::HTMLElement &&
        (document_parser_detail::lower(m_name) == "script" ||
         document_parser_detail::lower(m_name) == "style");
    std::string result = raw ? m_text : document_parser_detail::escapeMarkup(m_text);
    for (const auto& child : m_children) {
        result += raw && child->type() == EntityType::Text
            ? child->text() : child->getOuterHTML();
    }
    return result;
}

inline std::string DocumentNode::getOuterHTML() const {
    if (m_type == EntityType::Text) {
        return document_parser_detail::escapeMarkup(m_text);
    }
    if (m_type == EntityType::Comment || m_type == EntityType::HTMLComment ||
        m_type == EntityType::XMLComment) {
        return "<!--" + m_text + "-->";
    }
    if (m_type == EntityType::XMLCDATA) {
        return "<![CDATA[" + m_text + "]]>";
    }
    if (m_type == EntityType::HTMLDoctype || m_type == EntityType::XMLDoctype) {
        return m_text;
    }
    if (m_type == EntityType::XMLProcessingInstruction) {
        return "<?" + m_name +
            (m_text.empty() ? "" : " " + m_text) + "?>";
    }
    if (m_type == EntityType::HTMLDocument || m_type == EntityType::XMLDocument ||
        m_type == EntityType::XSLDocument || m_type == EntityType::Document ||
        m_type == EntityType::Root) {
        std::string result = m_text;
        for (const auto& child : m_children) result += child->getOuterHTML();
        return result;
    }
    
    std::string result = "<" + m_name;
    for (const auto& [key, value] : m_attributes) {
        result += " " + key + "=\"" +
            document_parser_detail::escapeMarkup(value, true) + "\"";
    }
    
    if (m_type == EntityType::HTMLElement &&
        document_parser_detail::isVoidHtmlTag(document_parser_detail::lower(m_name))) {
        return result + ">";
    }
    if (m_children.empty() && m_text.empty()) {
        result += m_type == EntityType::HTMLElement ? "></" + m_name + ">" : "/>";
    } else {
        result += ">";
        const bool raw = m_type == EntityType::HTMLElement &&
            (document_parser_detail::lower(m_name) == "script" ||
             document_parser_detail::lower(m_name) == "style");
        result += raw ? m_text : document_parser_detail::escapeMarkup(m_text);
        for (const auto& child : m_children) {
            result += raw && child->type() == EntityType::Text
                ? child->text() : child->getOuterHTML();
        }
        result += "</" + m_name + ">";
    }
    
    return result;
}

#endif // INTEGRATED_DOCUMENT_PARSER_H
