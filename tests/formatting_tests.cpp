#include "../document_parser.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void expectEqual(const std::string& actual, const std::string& expected,
                 const std::string& description) {
    if (actual != expected) {
        std::cerr << "FAIL: " << description << "\n  expected: " << expected
                  << "\n  actual:   " << actual << '\n';
        ++failures;
    }
}

template <typename Callable>
void expectThrows(Callable&& callable, const std::string& description) {
    try {
        callable();
        std::cerr << "FAIL: " << description << " did not throw\n";
        ++failures;
    } catch (const std::invalid_argument&) {
    }
}

void testNumbersAndCurrency() {
    expectEqual(ValueFormatter::number(1234567.896L), "1,234,567.90",
                "grouped decimal number");
    expectEqual(ValueFormatter::number(-12345.6L, 1, false), "-12345.6",
                "ungrouped negative number");
    expectEqual(ValueFormatter::number(42, 0), "42",
                "integer precision");
    expectEqual(ValueFormatter::number(
                    std::numeric_limits<long double>::infinity()),
                "inf", "positive infinity");
    expectEqual(ValueFormatter::number(
                    -std::numeric_limits<long double>::infinity()),
                "-inf", "negative infinity");
    expectEqual(ValueFormatter::number(
                    std::numeric_limits<long double>::quiet_NaN()),
                "nan", "not-a-number");

    expectEqual(ValueFormatter::currency(1234.5L), "$1,234.50",
                "USD currency");
    expectEqual(ValueFormatter::currency(-1234.5L, "eur"), "-€1,234.50",
                "case-insensitive currency code and negative value");
    expectEqual(ValueFormatter::currency(1250, "XYZ", 0), "XYZ 1,250",
                "unknown currency code");
    expectThrows([] { ValueFormatter::number(1, 16); },
                 "number precision validation");
    expectThrows([] { ValueFormatter::currency(1, "USD", -1); },
                 "currency precision validation");
}

void testMeasurementsAndByteSizes() {
    expectEqual(ValueFormatter::measurement(1500, "m"), "1.50 km",
                "large SI measurement");
    expectEqual(ValueFormatter::measurement(0.0015L, "m"), "1.50 mm",
                "small SI measurement");
    expectEqual(ValueFormatter::measurement(1.5e30L, "W"), "1.50 QW",
                "largest SI prefix");
    expectEqual(ValueFormatter::measurement(12, ""), "12.00",
                "measurement without a unit");
    expectEqual(ValueFormatter::measurement(999.999L, "m"), "1.00 km",
                "SI prefix changes when rounding crosses a unit boundary");

    expectEqual(ValueFormatter::byteSize(1500), "1.5 kB",
                "decimal byte size");
    expectEqual(ValueFormatter::byteSize(1536, 1, true), "1.5 KiB",
                "binary byte size");
    expectEqual(ValueFormatter::byteSize(-12), "-12.0 B",
                "negative byte size");
    expectEqual(ValueFormatter::byteSize(999.95L, 1), "1.0 kB",
                "byte unit changes when rounding crosses a boundary");
    expectEqual(ValueFormatter::byteSize(
                    std::numeric_limits<long double>::infinity()),
                "inf B", "non-finite byte size");
}

void testDateTimeAndDuration() {
    const auto epoch = std::chrono::system_clock::time_point{};
    expectEqual(ValueFormatter::dateTime(epoch), "1970-01-01 00:00:00 UTC",
                "UTC date and time");
    expectEqual(ValueFormatter::dateTime(epoch, "%Y-%m-%dT%H:%M:%SZ"),
                "1970-01-01T00:00:00Z", "custom UTC date-time format");

    const auto leapDay = std::chrono::sys_days{
        std::chrono::year{2024} / std::chrono::month{2} / std::chrono::day{29}};
    expectEqual(ValueFormatter::date(leapDay), "2024-02-29",
                "calendar date formatting");
    expectEqual(ValueFormatter::duration(std::chrono::duration<double>{3661}),
                "1h 1m 1s", "duration with hours, minutes, and seconds");
    expectEqual(ValueFormatter::duration(std::chrono::duration<double>{-2.5}, 1),
                "-2.5s", "negative fractional duration");
    expectEqual(ValueFormatter::duration(std::chrono::duration<double>{0}),
                "0s", "zero duration");
}

void testTypedValueIntegration() {
    expectEqual(TypedValue::detect("123").asString(), "123",
                "integer type detection");
    expectEqual(TypedValue::detect("hello").asString(), "hello",
                "string type detection");
    expectEqual(TypedValue(std::chrono::duration<double>{90}).asString(),
                "1m 30s", "duration typed-value formatting");
}

void testCsvParserIntegration() {
    const auto parsed = CSVParser::parse("name,value\nitem,42\n");
    if (!parsed.success || parsed.rowCount != 1 || parsed.columnCount != 2 ||
        parsed.headers.size() != 2) {
        std::cerr << "FAIL: CSV parser defaults and node integration\n";
        ++failures;
    }
}

} // namespace

int main() {
    testNumbersAndCurrency();
    testMeasurementsAndByteSizes();
    testDateTimeAndDuration();
    testTypedValueIntegration();
    testCsvParserIntegration();

    if (failures != 0) {
        std::cerr << failures << " formatting test(s) failed\n";
        return 1;
    }
    std::cout << "All formatting tests passed\n";
    return 0;
}
