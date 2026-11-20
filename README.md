# Entity Editor

Entity Editor is a dependency-free, header-only C++20 library in [`document_parser.hpp`](document_parser.hpp). It parses and edits HTML, strict XML/XHTML, JSON, a documented YAML subset, CSS rules/declarations, and CSV data, then serializes the edited tree back to text or a file.

The parsers validate structure and report failures through `false` and `DocumentEditor::lastError()`; failed parses leave the last successfully parsed document intact. Serialization is normalized rather than source-format preserving (for example, whitespace and attribute order may change). This self-contained implementation is not a complete browser HTML5 tree builder or complete YAML/CSS semantic validator; supported syntax and limitations are called out below.

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

This produces an HTML fragment. `setInnerHTML` parses forgiving HTML markup and replaces the node's children. `getInnerHTML`/`getOuterHTML` serialize node contents; source whitespace, attribute order, and original quote style are not preserved.

### 3. Edit an HTML or XHTML file and save it

HTML is parsed with forgiving tag recovery; XHTML is parsed strictly as XML. Load by extension, edit a node, then save:

```cpp
#include "document_parser.hpp"

#include <iostream>

int main() {
    DocumentEditor editor;
    if (!editor.loadFile("page.xhtml")) {
        std::cerr << editor.lastError() << '\n';
        return 1;
    }

    if (!editor.setTextById("summary", "Updated content")) {
        std::cerr << "Element #summary was not found\n";
        return 1;
    }
    if (!editor.saveFile("page.xhtml")) {
        std::cerr << editor.lastError() << '\n';
        return 1;
    }
}
```

`loadFile` recognizes `.html`, `.htm`, `.xhtml`, `.xml`, `.xsl`, `.xslt`, `.json`, `.yaml`, `.yml`, `.css`, and `.csv` (case-insensitive). The extension determines the parser; file contents are not used to guess the format. `saveFile` writes the current document format to the requested path.

### 4. Parse and edit CSS

CSS rules and declarations are nodes too, so declarations can be located and changed before saving:

```cpp
DocumentEditor stylesheet;
if (!stylesheet.loadFile("site.css")) {
    return 1;
}

for (const auto& color : stylesheet.findByTagName("color")) {
    color->setText("#234567");
}
if (!stylesheet.saveFile("site.css")) {
    return 1;
}
```

The CSS parser recognizes comments, standard rule blocks, declarations, nested at-rules, strings, parentheses, and custom-property brace values. It checks syntax structure but does not validate property names or values against the CSS specification.

### 5. Edit JSON or YAML data

Object keys and mapping keys become node names. Find a key, change its scalar text, and serialize or save the edited document:

```cpp
DocumentEditor settings;
if (!settings.loadFile("settings.json")) {
    return 1;
}

auto retries = settings.findByTagName("retries");
if (retries.empty()) {
    return 1;
}
retries.front()->setText("5");
if (!settings.saveFile("settings.json")) {
    return 1;
}
```

The same workflow works with supported YAML mappings and sequences. JSON preserves scalar kinds: use `setText` with a valid JSON number when editing a numeric node, or replace its `kind` with `"string"` via `setAttribute` when changing it to text.

### 6. Format numbers, units, and elapsed time

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
| `trimFields` | `true` | Trims spaces and tabs around parsed fields, including headers. |
| `skipEmptyLines` | `true` | Skips physical records containing no characters; empty lines inside quoted fields are retained. |
| `lineEnding` | `"\n"` | Line ending emitted by CSV serialization; parsing recognizes LF, CRLF, and CR record separators. |
| `encoding` | `"UTF-8"` | Descriptive only; no encoding conversion is performed. Supply UTF-8 input. |

Quoted fields can contain delimiters and line breaks. With the default `escape == quote`, doubled quotes represent a quote inside a quoted field; a distinct `escape` character can be used to escape the next character. Malformed quotes return `success == false` and populate `error`. `CSVParser::CSVParseResult` includes `document`, `headers`, typed `data`, `rawData`, `rowCount`, `columnCount`, `success`, and `error`. Delimiter must differ from quote and escape; these characters cannot be line breaks.

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
- `validateOptions(options)` throws `std::invalid_argument` for conflicting delimiter/quote/escape characters or line-break characters.
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
| `loadFile(filename)` | Loads a file and dispatches by extension: `html`, `htm`, `xhtml`, `xml`, `xsl`, `xslt`, `json`, `yaml`, `yml`, `css`, or `csv`. |
| `parseCSV(csv, options = {})` | Parses CSV and stores its typed rows, headers, document nodes, and active format. |
| `parseHTML(html)`, `parseXHTML(xhtml)`, `parseXML(xml)`, `parseXSL(xsl)` | Parse HTML forgivingly or parse XML/XHTML/XSL as well-formed XML. XSL uses the XML parser and serializer. |
| `parseJSON(json)`, `parseYAML(yaml)`, `parseCSS(css)` | Parse structured JSON, the YAML subset, or CSS rules/declarations. |
| `root()` | Return the current mutable root node, or `nullptr` before a successful parse. |
| `lastError()` | Return the most recent file or parse error; successful parse/save clears it. |
| `getCSVData()`, `getCSVHeaders()` | Return const references to the stored CSV rows and headers. |
| `getCell(row, column)` | Return an optional typed cell. |
| `setCell(row, column, string)` | Update a cell in an existing row; returns false for an invalid row. |
| `addCSVRow(row)` | Append typed values as a CSV row. |
| `findById(id)`, `findByTagName(tag)`, `findByClassName(className)`, `findByAttribute(name, value = "")` | Search the parsed node tree. |
| `querySelector(selector)`, `querySelectorAll(selector)` | Run the simplified selector lookup described below. |
| `findByNumericValue(attribute, min, max)` | Find nodes whose attribute parses as a number in the inclusive range. |
| `findByDateRange(attribute, start, end)` | Find nodes whose parsed date/time attribute falls in the inclusive range. |
| `setTextById(id, text)`, `setTypedValueById(id, value)`, `setAttributeById(id, name, value)`, `setInnerHTMLById(id, html)` | Apply the operation to the first node found by ID; return whether a node was found. |
| `batchEdit(selector, callback)` | Call the callback for each node returned by `querySelectorAll`. |
| `addElement(parent, tag, id = "", text = "")` | Create and append a `DocumentNode`; `parent` must be a non-null shared node. |
| `deleteElementById(id)` | Remove a found node if it has a parent. |
| `toString()` | Serialize according to the active format; markup, JSON, YAML, and CSS are normalized, and CSV uses default options. |
| `toCSVString(options = {})` | Serialize stored CSV data using the given delimiter, quoting, and line ending settings. |
| `saveFile(filename)` | Write `toString()` to a file; returns false if open/write fails and sets `lastError()`. |

### `DocumentNode`

`DocumentNode` is shared-pointer managed. Its aliases are `Ptr` (`std::shared_ptr<DocumentNode>`), `List`, `AttributeMap`, and `ChildrenList`. Constructors accept an `EntityType`, a name, or a name and text.

Public operations:

- Properties: `type`/`setType`, `name`/`setName`, `text`/`setText`.
- Typed values: `typedValue`, `setTypedValue`.
- Attributes: `setAttribute`, `getAttribute` (optional string), `getAttributeAs<T>`, `hasAttribute`, `removeAttribute`, `attributes`.
- IDs and classes: `id`, `setId`, `hasId`, `getClasses`, `addClass`, `removeClass`.
- Tree: `addChild(child)`, `addChild(index, child)`, `removeChild(child)`, `removeChild(index)`, `clearChildren`, `children`, `parent`.
- Text: `getTextContent` recursively joins this node's text and descendant text; `setTextContent` replaces the node's text and clears all children.
- Searches: `findById`, `findByTagName`, `findByClassName`, `findByAttribute`, `findByNumericValue`, `findByDateRange`.
- Other: `querySelector`, `querySelectorAll`, `getElementByIndex`, `setInnerHTML`, `getInnerHTML`, `getOuterHTML`, `clone(deep = true)`.

`getAttributeAs<T>` supports `int`, `double`, `bool`, and `system_clock::time_point`; unsupported types return `nullopt`. Boolean attributes recognize `true`/`false` and `1`/`0`. `findByAttribute(name)` with the default empty value matches attributes whose value is empty **or** any present value, so use an explicit value to request equality. Child insertion expects the node to be managed by `std::shared_ptr` because it sets a `weak_ptr` parent link. `setInnerHTML` replaces the children with a forgivingly parsed HTML fragment.

### Parser coverage and limits

- **HTML (`.html`, `.htm`):** tokenizes elements, attributes, text, comments, and doctype; accepts UTF-8 BOMs; decodes common and numeric character references; tolerates unclosed/mismatched tags and common optional closing tags. It is not the complete WHATWG HTML5 tree-construction algorithm, and the named-entity table and malformed-table recovery are limited.
- **XHTML/XML/XSL (`.xhtml`, `.xml`, `.xsl`, `.xslt`):** accepts UTF-8 BOMs and requires well-formed XML, one document element, matching tag names, quoted attributes, and built-in/numeric entities. Comments, CDATA, processing instructions, and a DOCTYPE token are retained. External entity resolution is deliberately not performed; DTD-defined entities and namespace constraint validation are not implemented. XHTML is strict XML, unlike HTML.
- **JSON (`.json`):** validates JSON syntax, numbers, escapes, nesting, and UTF-8; accepts a UTF-8 BOM and decodes Unicode surrogate pairs in `\\u` escapes. Comments and trailing commas are rejected.
- **YAML (`.yaml`, `.yml`):** accepts UTF-8 BOMs and supports indentation-based mappings/sequences, plain and quoted scalars, comments, nested blocks, flow sequences/mappings, and simple `|`/`>` block scalars without blank or comment-only content lines. Anchors, aliases, tags, complex keys, multiple documents, and advanced block-scalar chomping/indent indicators are unsupported.
- **CSS (`.css`):** accepts UTF-8 BOMs and parses comments, declarations, rule blocks, nested at-rules, strings, parentheses, and custom-property brace values. It performs structural parsing, not browser-level CSS semantic validation.
- **CSV (`.csv`):** accepts a UTF-8 BOM and parses quoted delimiters, escaped quotes, CR/LF/CRLF records, and multiline quoted fields. It has no encoding conversion; `ParseOptions::encoding` is informational.

All tree serializers normalize output; they preserve parsed text and comments where represented but not the original spacing or attribute order. Parser failures return `false`, report a message through `lastError()`, and do not replace the previous document.

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
- CSS: `CSSDocument`, `CSSRule`, `CSSDeclaration`
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

The parser integration tests can be built and run with:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -pedantic tests/document_parser_tests.cpp -o document_parser_tests
./document_parser_tests
```
