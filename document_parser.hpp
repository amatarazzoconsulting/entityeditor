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
#include <type_traits>

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
    static TypedValue detect(const std::string& str) {
        // Try integer
        try {
            size_t pos;
            long long intVal = std::stoll(str, &pos);
            if (pos == str.length()) {
                return TypedValue(intVal);
            }
        } catch (...) {}
        
        // Try double
        try {
            size_t pos;
            double doubleVal = std::stod(str, &pos);
            if (pos == str.length()) {
                return TypedValue(doubleVal);
            }
        } catch (...) {}
        
        // Try date/time
        auto dt = DateTimeParser::parse(str);
        if (dt.valid) {
            if (dt.isDateOnly) {
                return TypedValue(dt.date);
            } else if (dt.isTimeOnly) {
                return TypedValue(dt.time);
            } else {
                return TypedValue(dt.value);
            }
        }
        
        // Default to string
        return TypedValue(str);
    }
    
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
    
    std::string asString() const {
        if (m_type == ValueType::String) return m_string;
        if (m_type == ValueType::Integer) return std::to_string(m_int);
        if (m_type == ValueType::Double) {
            std::ostringstream oss;
            oss << std::fixed << m_double;
            return oss.str();
        }
        if (m_type == ValueType::Boolean) return m_bool ? "true" : "false";
        if (m_type == ValueType::DateTime) {
            return DateTimeParser::format(m_timePoint, "%Y-%m-%d %H:%M:%S");
        }
        if (m_type == ValueType::Date) {
            return DateTimeParser::format(m_date, "%Y-%m-%d");
        }
        if (m_type == ValueType::Time) {
            return DateTimeParser::formatTime(m_timeOfDay);
        }
        return "";
    }
    
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
            int amount = std::stoi(match[1]);
            std::string unit = match[2];
            
            if (unit.find("day") != std::string::npos) {
                result.value = now - std::chrono::hours(24 * amount);
            } else if (unit.find("hour") != std::string::npos) {
                result.value = now - std::chrono::hours(amount);
            } else if (unit.find("minute") != std::string::npos) {
                result.value = now - std::chrono::minutes(amount);
            } else if (unit.find("second") != std::string::npos) {
                result.value = now - std::chrono::seconds(amount);
            }
            
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
// CSVParser - Complete CSV support with type detection
//=============================================================================
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
    
    static CSVParseResult parse(const std::string& csv, const ParseOptions& options = {}) {
        CSVParseResult result;
        result.document = std::make_shared<DocumentNode>(EntityType::CSVDocument);
        result.document->setName("csv");
        
        std::istringstream stream(csv);
        std::string line;
        bool firstRow = true;
        size_t lineNum = 0;
        
        while (std::getline(stream, line)) {
            lineNum++;
            
            // Handle different line endings
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            // Skip empty lines
            if (options.skipEmptyLines && line.empty()) {
                continue;
            }
            
            // Parse fields with quote handling
            auto fields = parseCSVLine(line, options);
            
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
    
    static std::string serialize(const CSVParseResult& result, const ParseOptions& options = {}) {
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
    
private:
    static std::vector<std::string> parseCSVLine(const std::string& line, const ParseOptions& options) {
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;
        bool escapeNext = false;
        
        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];
            
            if (escapeNext) {
                field += c;
                escapeNext = false;
                continue;
            }
            
            if (c == options.escape && inQuotes) {
                // Check if this is an escaped quote
                if (i + 1 < line.length() && line[i + 1] == options.quote) {
                    field += options.quote;
                    i++; // Skip next quote
                    continue;
                }
                escapeNext = true;
                continue;
            }
            
            if (c == options.quote) {
                inQuotes = !inQuotes;
                continue;
            }
            
            if (c == options.delimiter && !inQuotes) {
                fields.push_back(field);
                field.clear();
                continue;
            }
            
            field += c;
        }
        
        fields.push_back(field);
        return fields;
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
            if (c == options.quote) {
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
        auto it = m_children.begin();
        while (it != m_children.end()) {
            if ((*it)->type() == EntityType::Text) {
                it = m_children.erase(it);
            } else {
                ++it;
            }
        }
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
// DocumentEditor - Main editing interface with CSV and type support
//=============================================================================
class DocumentEditor {
public:
    DocumentEditor() = default;
    
    // Load from file with auto-detection
    bool loadFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        std::string ext = filename.substr(filename.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "html" || ext == "htm") {
            return parseHTML(content);
        } else if (ext == "xml") {
            return parseXML(content);
        } else if (ext == "xsl" || ext == "xslt") {
            return parseXSL(content);
        } else if (ext == "json") {
            return parseJSON(content);
        } else if (ext == "yaml" || ext == "yml") {
            return parseYAML(content);
        } else if (ext == "csv") {
            return parseCSV(content);
        } else {
            return false;
        }
    }
    
    // Parse CSV with options
    bool parseCSV(const std::string& csv, const CSVParser::ParseOptions& options = {}) {
        auto result = CSVParser::parse(csv, options);
        if (result.success) {
            m_root = result.document;
            m_csvData = result.data;
            m_csvHeaders = result.headers;
            m_currentFormat = "csv";
            return true;
        }
        return false;
    }
    
    // Parse HTML
    bool parseHTML(const std::string& html) {
        m_root = std::make_shared<DocumentNode>(EntityType::HTMLDocument);
        m_root->setName("html");
        m_currentFormat = "html";
        // Simplified HTML parsing - in production, use a proper parser
        m_root->setText(html);
        return true;
    }
    
    // Parse XML
    bool parseXML(const std::string& xml) {
        m_root = std::make_shared<DocumentNode>(EntityType::XMLDocument);
        m_root->setName("xml");
        m_currentFormat = "xml";
        m_root->setText(xml);
        return true;
    }
    
    // Parse XSL
    bool parseXSL(const std::string& xsl) {
        m_root = std::make_shared<DocumentNode>(EntityType::XSLDocument);
        m_root->setName("xsl:stylesheet");
        m_currentFormat = "xsl";
        m_root->setText(xsl);
        return true;
    }
    
    // Parse JSON
    bool parseJSON(const std::string& json) {
        m_root = std::make_shared<DocumentNode>(EntityType::JSONDocument);
        m_currentFormat = "json";
        m_root->setText(json);
        return true;
    }
    
    // Parse YAML
    bool parseYAML(const std::string& yaml) {
        m_root = std::make_shared<DocumentNode>(EntityType::YAMLDocument);
        m_currentFormat = "yaml";
        m_root->setText(yaml);
        return true;
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
            return toHTMLString(m_root);
        } else if (m_currentFormat == "xml") {
            return toXMLString(m_root);
        } else if (m_currentFormat == "xsl") {
            return toXSLString(m_root);
        } else if (m_currentFormat == "json") {
            return toJSONString(m_root);
        } else if (m_currentFormat == "yaml") {
            return toYAMLString(m_root);
        } else if (m_currentFormat == "csv") {
            return toCSVString();
        }
        return "";
    }
    
    std::string toCSVString(const CSVParser::ParseOptions& options = {}) const {
        if (m_csvData.empty()) return "";
        
        std::stringstream ss;
        
        // Write headers
        if (!m_csvHeaders.empty()) {
            for (size_t i = 0; i < m_csvHeaders.size(); ++i) {
                if (i > 0) ss << options.delimiter;
                ss << quoteIfNeeded(m_csvHeaders[i], options);
            }
            ss << options.lineEnding;
        }
        
        // Write data
        for (const auto& row : m_csvData) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) ss << options.delimiter;
                std::string value = row[i].asString();
                ss << quoteIfNeeded(value, options);
            }
            ss << options.lineEnding;
        }
        
        return ss.str();
    }
    
    bool saveFile(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        file << toString();
        return true;
    }
    
private:
    DocumentNode::Ptr m_root;
    std::string m_currentFormat;
    std::vector<std::vector<TypedValue>> m_csvData;
    std::vector<std::string> m_csvHeaders;
    
    std::string quoteIfNeeded(const std::string& field, const CSVParser::ParseOptions& options) const {
        bool needsQuote = field.find(options.delimiter) != std::string::npos ||
                          field.find(options.quote) != std::string::npos ||
                          field.find('\n') != std::string::npos;
        
        if (!needsQuote) return field;
        
        std::string quoted;
        quoted += options.quote;
        for (char c : field) {
            if (c == options.quote) {
                quoted += options.escape;
                quoted += options.quote;
            } else {
                quoted += c;
            }
        }
        quoted += options.quote;
        return quoted;
    }
    
    std::string toHTMLString(DocumentNode::Ptr node, int indent = 0) const {
        if (!node) return "";
        std::string indentStr(indent, ' ');
        std::string result;
        
        if (node->type() == EntityType::HTMLElement || node->type() == EntityType::Element) {
            result += indentStr + "<" + node->name();
            for (const auto& [key, value] : node->attributes()) {
                result += " " + key + "=\"" + value + "\"";
            }
            
            if (node->children().empty() && node->text().empty()) {
                result += " />\n";
            } else {
                result += ">";
                if (!node->text().empty()) {
                    result += node->text();
                }
                for (const auto& child : node->children()) {
                    result += "\n" + toHTMLString(child, indent + 2);
                }
                if (!node->children().empty()) result += "\n" + indentStr;
                result += "</" + node->name() + ">\n";
            }
        } else {
            result = node->text();
        }
        
        return result;
    }
    
    std::string toXMLString(DocumentNode::Ptr node, int indent = 0) const {
        // Similar to HTML but with XML declaration
        return toHTMLString(node, indent);
    }
    
    std::string toXSLString(DocumentNode::Ptr node, int indent = 0) const {
        return toHTMLString(node, indent);
    }
    
    std::string toJSONString(DocumentNode::Ptr node, int indent = 0) const {
        return node ? node->text() : "";
    }
    
    std::string toYAMLString(DocumentNode::Ptr node, int indent = 0) const {
        return node ? node->text() : "";
    }
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
    // Simplified - in production, parse HTML and set children
    clearChildren();
    auto textNode = std::make_shared<DocumentNode>(EntityType::Text);
    textNode->setText(html);
    addChild(textNode);
}

inline std::string DocumentNode::getInnerHTML() const {
    std::string result;
    for (const auto& child : m_children) {
        result += child->getOuterHTML();
    }
    return result;
}

inline std::string DocumentNode::getOuterHTML() const {
    if (m_type == EntityType::Text) {
        return m_text;
    }
    
    std::string result = "<" + m_name;
    for (const auto& [key, value] : m_attributes) {
        result += " " + key + "=\"" + value + "\"";
    }
    
    if (m_children.empty() && m_text.empty()) {
        result += " />";
    } else {
        result += ">";
        result += m_text;
        for (const auto& child : m_children) {
            result += child->getOuterHTML();
        }
        result += "</" + m_name + ">";
    }
    
    return result;
}

#endif // INTEGRATED_DOCUMENT_PARSER_H
