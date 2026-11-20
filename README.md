# Entity Editor

Entity Editor is a header-only C++20 library in [`document_parser.hpp`](document_parser.hpp). It provides a shared node model, CSV parsing and editing, basic selector and node operations, date/time conversion, and value formatting.

> **Scope note:** the current HTML, XML, XSL, JSON, and YAML entry points store their input as raw text; they do not parse those formats into nodes. CSV is the only implemented structured parser. The API also contains simplified selector and HTML-fragment operations. The examples and reference below describe what this version actually does.

## Requirements and quick start

Use a C++20 compiler and include the header directly:

```cpp
#include "document_parser.hpp"

#include <iostream>

int main() {
    DocumentEditor editor;
    if (!editor.parseCSV("name,price\nNotebook,4.50\n")) {
        return 1;
    }

    const auto price = editor.getCell(0, 1);
    if (price) {
        std::cout << price->asDouble() << '\n';
    }
}
```

For example, compile a source file in this directory with:

```sh
g++ -std=c++20 -Wall -Wextra -pedantic example.cpp -o example
./example
```

## Tutorials

### 1. Read, update, and write CSV

`DocumentEditor` stores CSV rows as typed values. The row index does not include the header row. The following example changes a cell and writes CSV back out:

```cpp
#include "document_parser.hpp"

#include <iostream>

int main() {
    DocumentEditor editor;
    CSVParser::ParseOptions options;
    options.hasHeader = true;
    options.detectTypes = true;

    if (!editor.parseCSV(
            "item,quantity\n"
            "notebook,3\n"
            "pen,12\n",
            options)) {
        return 1;
    }

    const auto quantity = editor.getCell(0, 1);
    if (!quantity) {
        return 1;
    }
    std::cout << "First item quantity: " << quantity->asInt() << '\n';

    if (!editor.setCell(0, 1, "5")) {
        return 1;
    }
    std::cout << editor.toCSVString(options);
}
```

`getCSVHeaders()` returns the header names. `getCSVData()` returns rows of `TypedValue`. `getCell(row, column)` returns an empty `std::optional` when the requested cell is out of range. `setCell` updates a cell only when the row exists; it detects the new value's type.

### 2. Build and query a small node tree

For structured node operations, create `DocumentNode` objects directly. This example creates a fragment, queries it, and serializes the article node:

```cpp
#include "document_parser.hpp"

#include <iostream>
#include <memory>

int main() {
    auto root = std::make_shared<DocumentNode>("main");
    auto article = std::make_shared<DocumentNode>("article");
    article->setId("featured");
    article->addClass("card");

    auto title = std::make_shared<DocumentNode>("h2", "A useful item");
    article->addChild(title);
    root->addChild(article);

    auto found = root->querySelector("#featured");
    if (found) {
        found->setAttribute("data-state", "published");
        std::cout << found->getOuterHTML() << '\n';
    }
}
```

This produces an HTML-like fragment. `setInnerHTML` does **not** parse markup: it removes existing child nodes and stores the supplied string in a text node. `getInnerHTML`/`getOuterHTML` serialize node contents; they are not a full HTML parser or standards-compliant serializer.

### 3. Preserve or write raw document text

The HTML, XML, XSL, JSON, and YAML methods currently keep the supplied text unchanged. They can be used as simple text containers / file pass-throughs:

```cpp
DocumentEditor editor;
if (!editor.parseJSON(R"({"enabled":true,"count":3})")) {
    return 1;
}

const std::string originalText = editor.toString();
```

These methods do not validate or expose JSON keys, HTML elements, XML attributes, or YAML mappings. `DocumentEditor::loadFile` chooses an entry point from the extension (`html`, `htm`, `xml`, `xsl`, `xslt`, `json`, `yaml`, `yml`, or `csv`); it does not inspect the file contents to detect a format.

### 4. Format numbers, units, and elapsed time

`ValueFormatter` offers locale-independent formatting helpers:

```cpp
const auto price = ValueFormatter::currency(1234.5, "USD");       // $1,234.50
const auto size = ValueFormatter::byteSize(1536, 1, true);        // 1.5 KiB
const auto distance = ValueFormatter::measurement(1500, "m");     // 1.50 km
const auto elapsed = ValueFormatter::duration(
    std::chrono::duration<double>{3661});                         // 1h 1m 1s
```

## Options reference

### `CSVParser::ParseOptions`

Pass this struct to `CSVParser::parse`, `CSVParser::serialize`, `DocumentEditor::parseCSV`, or `DocumentEditor::toCSVString`.

| Option | Default | Effect |
| --- | --- | --- |
| `delimiter` | `','` | Field separator used by CSV parsing and serialization. |
| `quote` | `'"'` | Quote character used by parsing and serialization. |
| `escape` | `'"'` | Escape character used by the parser and emitted before quotes in serialized fields. |
| `hasHeader` | `true` | Treats the first non-skipped line as headers during parsing. Serialization writes headers whenever the result/editor has them. |
| `detectTypes` | `true` | Uses `TypedValue::detect` for fields. When false, parsed fields are strings. |
| `trimFields` | `true` | Trims spaces and tabs around parsed data fields. Header strings are not trimmed by this option. |
| `skipEmptyLines` | `true` | Skips physical lines that are exactly empty (after removing a trailing `\r`). |
| `lineEnding` | `"\n"` | Line ending emitted by CSV serialization. Parsing uses `std::getline` and does not consult this option. |
| `encoding` | `"UTF-8"` | Reserved option; encoding conversion is not currently implemented. |

CSV input is processed one physical line at a time. Quoted fields spanning multiple lines are not supported. The parser's quote/escape handling is simplified; test input containing quoted fields with your chosen options before relying on round-trip fidelity. `CSVParser::CSVParseResult` includes `document`, `headers`, typed `data`, `rawData`, `rowCount`, `columnCount`, `success`, and `error`; the current parser sets `success` on completion and does not populate detailed parse errors.

### `ValueFormatter` options

All precision arguments accept integers from 0 through 15 inclusive. Values outside that range throw `std::invalid_argument`.

| Function | Parameters and behavior |
| --- | --- |
| `number(value, precision = 2, grouping = true)` | Fixed-point decimal formatting; grouping inserts commas every three integer digits. Non-finite values are returned as `nan`, `inf`, or `-inf`. |
| `currency(value, code = "USD", precision = 2)` | Recognizes USD, EUR, GBP, JPY, CNY, CAD, AUD, and INR (case-insensitive). Other codes are uppercased and prefixed with a space. |
| `measurement(value, unit, precision = 2)` | Scales by powers of 1000 using SI prefixes from `q` through `Q`; an empty unit returns a formatted number without a suffix. |
| `byteSize(bytes, precision = 1, binary = false)` | Uses decimal units (kB through QB) by default, or IEC units (KiB through YiB) when `binary` is true. |
| `dateTime(timePoint, format = "%Y-%m-%d %H:%M:%S UTC")` | Formats a `system_clock::time_point` in UTC. The format is a `strftime` format string. |
| `date(sysDays, format = "%Y-%m-%d")` | Formats a `std::chrono::sys_days` date using a `strftime` format string. |
| `duration(seconds, precision = 0)` | Formats elapsed seconds as compact hours/minutes/seconds, e.g. `1h 1m 1s`. |

### `TypedValue` type detection and conversion

`TypedValue::detect(string)` attempts a complete integer parse, then a complete floating-point parse, then `DateTimeParser::parse`; unmatched input remains a string. It does not infer booleans from text. `TypedValue` can also be constructed explicitly from `bool`, `int`, `long long`, `double`, `std::string`, `const char*`, `system_clock::time_point`, `sys_days`, and `duration<double>`.

`type()` returns one of `ValueType::{Null, Boolean, Integer, Double, String, DateTime, Date, Time, Duration}`. Conversion methods are `asString()`, `asBool()`, `asInt()`, `asDouble()`, and `asDateTime()`. Failed string-to-number conversions in `asInt`/`asDouble` return zero; `asDateTime()` returns the stored date/time when available and otherwise returns the current time. Use `type()` to check before converting.

`DateTimeParser::parse` recognizes:

- ISO-like timestamps `YYYY-MM-DDThh:mm:ss`, optional fractional seconds, and optional `Z` or `±hh:mm` timezone suffix.
- Dates `YYYY-MM-DD`, `YYYY/MM/DD`, `MM/DD/YYYY`, `DD/MM/YYYY`, `Month D, YYYY`, `Mon D, YYYY`, `D Month YYYY`, `D Mon YYYY`, and `YYYYMMDD`.
- Date-times `YYYY-MM-DD HH:mm[:ss]`, `YYYY-MM-DDThh:mm:ss[Z]`, `MM/DD/YYYY HH:mm:ss`, and `DD/MM/YYYY HH:mm:ss`.
- Times `HH:mm[:ss]` and 12-hour `h:mm[:ss] AM/PM`.
- Relative text `today`, `yesterday`, `tomorrow`, and integer quantities of days, hours, minutes, or seconds followed by `ago`.

The parser returns a `DateTimeResult` with `valid`, `value`, `date`, `time`, `isDateOnly`, `isTimeOnly`, and `format` fields. Ambiguous numeric dates use the first matching pattern (month/day before day/month). The relative forms depend on the clock at parse time. Timezone conversion and format handling follow the current implementation; validate results if timezone-sensitive correctness is required.

## API reference

### `CSVParser`

- `parse(csv)` and `parse(csv, options)` return `CSVParseResult`.
- `serialize(result)` and `serialize(result, options)` return CSV text.
- `ParseOptions` and the result fields are documented above. Serialization uses typed `data` and `headers`; `rawData` is not used.

### `DateTimeParser`

- `parse(text)` returns `DateTimeResult`.
- `parseISO8601(text, result)`, `parseTimeOnly(text, result)`, and `parseRelativeDate(text, result)` are public parsing helpers that populate a supplied result and return whether that form matched.
- `format(timePoint, format)` formats a `system_clock::time_point`; `format(sysDays, format)` formats a calendar date. `formatTime(duration)` returns `HH:MM:SS`.

### `ValueFormatter`

Public static methods are `number`, `currency`, `measurement`, `byteSize`, `dateTime`, `date`, and `duration`, with parameters and defaults listed in the options reference.

### `DocumentEditor`

| Method | Description |
| --- | --- |
| `loadFile(filename)` | Loads text and dispatches by the listed filename extension; returns `false` on open failure or unknown extension. |
| `parseCSV(csv, options = {})` | Parses CSV and stores its typed rows, headers, document nodes, and active format. |
| `parseHTML(html)`, `parseXML(xml)`, `parseXSL(xsl)`, `parseJSON(json)`, `parseYAML(yaml)` | Store the input text as-is and set the active output format. These are not structured parsers. |
| `getCSVData()`, `getCSVHeaders()` | Return const references to the stored CSV rows and headers. |
| `getCell(row, column)` | Return an optional typed cell. |
| `setCell(row, column, string)` | Update a cell in an existing row; returns false for an invalid row. |
| `addCSVRow(row)` | Append typed values as a CSV row. |
| `findById(id)`, `findByTagName(tag)`, `findByClassName(className)`, `findByAttribute(name, value = "")` | Search the internal node tree; raw HTML/XML/etc. text is not parsed into searchable nodes. |
| `querySelector(selector)`, `querySelectorAll(selector)` | Run the simplified selector lookup described below. |
| `findByNumericValue(attribute, min, max)` | Find nodes whose attribute parses as a number in the inclusive range. |
| `findByDateRange(attribute, start, end)` | Find nodes whose parsed date/time attribute falls in the inclusive range. |
| `setTextById(id, text)`, `setTypedValueById(id, value)`, `setAttributeById(id, name, value)`, `setInnerHTMLById(id, html)` | Apply the operation to the first node found by ID; return whether a node was found. |
| `batchEdit(selector, callback)` | Call the callback for each node returned by `querySelectorAll`. |
| `addElement(parent, tag, id = "", text = "")` | Create and append a `DocumentNode`; `parent` must be a non-null shared node. |
| `deleteElementById(id)` | Remove a found node if it has a parent. |
| `toString()` | Serialize according to the active format; CSV uses default CSV options. |
| `toCSVString(options = {})` | Serialize stored CSV data using the given delimiter, quoting, and line ending settings. |
| `saveFile(filename)` | Write `toString()` to a file; returns false if the output file could not be opened. |

### `DocumentNode`

`DocumentNode` is shared-pointer managed. Its aliases are `Ptr` (`std::shared_ptr<DocumentNode>`), `List`, `AttributeMap`, and `ChildrenList`. Constructors accept an `EntityType`, a name, or a name and text.

Public operations:

- Properties: `type`/`setType`, `name`/`setName`, `text`/`setText`.
- Typed values: `typedValue`, `setTypedValue`.
- Attributes: `setAttribute`, `getAttribute` (optional string), `getAttributeAs<T>`, `hasAttribute`, `removeAttribute`, `attributes`.
- IDs and classes: `id`, `setId`, `hasId`, `getClasses`, `addClass`, `removeClass`.
- Tree: `addChild(child)`, `addChild(index, child)`, `removeChild(child)`, `removeChild(index)`, `clearChildren`, `children`, `parent`.
- Text: `getTextContent` recursively joins this node's text and descendant text; `setTextContent` replaces this node's text and removes child nodes of type `Text`.
- Searches: `findById`, `findByTagName`, `findByClassName`, `findByAttribute`, `findByNumericValue`, `findByDateRange`.
- Other: `querySelector`, `querySelectorAll`, `getElementByIndex`, `setInnerHTML`, `getInnerHTML`, `getOuterHTML`, `clone(deep = true)`.

`getAttributeAs<T>` supports `int`, `double`, `bool`, and `system_clock::time_point`; unsupported types return `nullopt`. Boolean attributes recognize `true`/`false` and `1`/`0`. `findByAttribute(name)` with the default empty value matches attributes whose value is empty **or** any present value, so use an explicit value to request equality. Child insertion expects the node to be managed by `std::shared_ptr` because it sets a `weak_ptr` parent link.

### Selector behavior

`querySelectorAll` currently recognizes one simple ID (`#name`), class (`.name`), or tag (`name`) pattern and uses the first matching pattern type in that order. ID searches return at most one node; class and tag searches return matches from the current node and descendants. It does not implement CSS combinators, attribute selectors, comma groups, or compound selectors. `querySelector` returns the first result or `nullptr`.

### `EntityType`

The public enum contains these values, grouped by intended use:

- Core: `Null`, `Bool`, `Int`, `Double`, `String`, `Array`, `Object`
- Document: `Document`, `Root`, `Element`, `Attribute`, `Text`, `Comment`, `CDATA`
- XML: `XMLDocument`, `XMLElement`, `XMLAttribute`, `XMLComment`, `XMLCDATA`, `XMLProcessingInstruction`, `XMLDoctype`, `XMLNamespace`
- XSL: `XSLDocument`, `XSLTemplate`, `XSLApply`, `XSLForEach`, `XSLIf`, `XSLChoose`, `XSLWhen`, `XSLOtherwise`, `XSLValueOf`, `XSLCopy`, `XSLCopyOf`, `XSLVariable`, `XSLParam`, `XSLCallTemplate`, `XSLWithParam`, `XSLAttribute`, `XSLElement`, `XSLText`, `XSLComment`
- HTML: `HTMLDocument`, `HTMLDoctype`, `HTMLElement`, `HTMLAttribute`, `HTMLComment`, `HTMLScript`, `HTMLStyle`, `HTMLAnchor`, `HTMLImage`, `HTMLForm`, `HTMLInput`, `HTMLButton`, `HTMLTable`, `HTMLDiv`, `HTMLSpan`, `HTMLHeading`, `HTMLParagraph`, `HTMLList`, `HTMLLink`
- JSON: `JSONDocument`, `JSONObject`, `JSONArray`, `JSONKey`, `JSONValue`
- YAML: `YAMLDocument`, `YAMLMapping`, `YAMLSequence`, `YAMLScalar`, `YAMLKey`
- CSV: `CSVDocument`, `CSVRow`, `CSVField`, `CSVHeader`
- Date/time: `DateTime`, `Date`, `Time`, `Timestamp`, `Duration`
- Numeric: `Integer`, `Floating`, `Scientific`
- Common attributes: `ID`, `Class`, `Style`, `HRef`, `Src`, `Alt`, `Title`
- Other: `Custom`, `Unknown`

## Tests

The repository's formatting regression tests can be built and run with:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic tests/formatting_tests.cpp -o formatting_tests
./formatting_tests
```
