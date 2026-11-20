#include "../document_parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testCsv() {
    DocumentEditor editor;
    const std::string source =
        "name,note\r\n"
        "item,\"two lines\nwith \"\"quotes\"\"\"\r\n";
    check(editor.parseCSV(source), "parse multiline, escaped-quote CSV");
    const auto cell = editor.getCell(0, 1);
    check(cell && cell->asString() == "two lines\nwith \"quotes\"",
          "decode CSV quoted field");
    const std::string serialized = editor.toString();
    DocumentEditor reparsed;
    check(reparsed.parseCSV(serialized), "reparse serialized CSV");
    const auto roundTrip = reparsed.getCell(0, 1);
    check(roundTrip && roundTrip->asString() == cell->asString(),
          "CSV multiline round trip");
    check(!editor.parseCSV("a\n\"unterminated"), "reject unterminated CSV quote");
    check(!editor.lastError().empty(), "report CSV parse error");

    CSVParser::ParseOptions custom;
    custom.delimiter = ';';
    custom.escape = '\\';
    custom.hasHeader = false;
    custom.detectTypes = false;
    const auto customParsed = CSVParser::parse("\"a\\\"b\";c\n", custom);
    check(customParsed.success && customParsed.data.size() == 1 &&
          customParsed.data[0][0].asString() == "a\"b",
          "support custom CSV delimiter and escape");
    const auto oversizedRelativeDate =
        DateTimeParser::parse("999999999999999999 days ago");
    check(!oversizedRelativeDate.valid,
          "reject overflowing relative date without throwing");
}

void testHtmlAndXhtml() {
    DocumentEditor html;
    check(html.parseHTML(
        "<!doctype html><html><body><p id='intro'>Hello &amp; "
        "<b>world</b> &copy;</p><img src=x></body></html>"),
        "parse forgiving HTML");
    auto paragraph = html.findById("intro");
    check(paragraph && paragraph->getTextContent() == "Hello & world ©",
          "HTML tree and entity decoding");
    check(html.setTextById("intro", "Updated <title>"),
          "edit HTML element text by ID");
    check(html.toString().find("Updated &lt;title&gt;") != std::string::npos,
          "escape updated HTML text when serializing");
    check(html.setInnerHTMLById("intro", "<i>nested</i>"),
          "replace HTML contents with parsed fragment");
    check(html.findByTagName("i").size() == 1,
          "query element inserted by innerHTML");
    check(html.parseHTML(
        "<ul><li>one<li>two</ul><script>const text = '</scripture>';</script>"),
        "recover optional HTML closing tags and raw-text elements");
    check(html.findByTagName("li").size() == 2 &&
          html.toString().find("</scripture>") != std::string::npos,
          "preserve HTML list items and raw script content");
    check(html.parseHTML("<p>&#x110000;</p>"),
          "recover invalid HTML numeric character reference");

    DocumentEditor xhtml;
    check(xhtml.parseXHTML(
        "<?xml version=\"1.0\"?><html xmlns=\"http://www.w3.org/1999/xhtml\">"
        "<body><p id=\"x\">A &amp; B</p></body></html>"),
        "parse well-formed XHTML");
    check(xhtml.findById("x") != nullptr, "query XHTML nodes");
    const auto originalRoot = xhtml.root();
    check(!xhtml.parseXHTML("<root><broken></root>"),
          "reject mismatched XHTML tags");
    check(xhtml.root() == originalRoot, "failed parse preserves prior document");
    check(!xhtml.lastError().empty(), "report XML parse error");
}

void testXml() {
    DocumentEditor editor;
    check(editor.parseXML(
        "<?xml version=\"1.0\"?><root><value>A &amp; B</value>"
        "<![CDATA[x < y]]><!-- note --></root>"),
        "parse XML with declaration, entity, CDATA, and comment");
    check(editor.findByTagName("value").size() == 1, "find XML element");
    check(editor.toString().find("<![CDATA[x < y]]>") != std::string::npos,
          "serialize XML CDATA");
    check(editor.parseXML("<!DOCTYPE root [<!ELEMENT root ANY>]><root/>"),
          "parse XML internal-subset doctype");
    check(editor.toString().find("<!DOCTYPE root") != std::string::npos,
          "serialize XML doctype");
    check(!editor.parseXML("<root>&unknown;</root>"),
          "reject unknown XML entity");
}

void testJson() {
    DocumentEditor editor;
    check(editor.parseJSON(
        R"({"name":"caf\u00e9","active":true,"values":[1,null,{"ok":false}]})"),
        "parse nested JSON");
    auto name = editor.findByTagName("name");
    check(name.size() == 1 && name[0]->text() == "café",
          "decode JSON Unicode escapes");
    name[0]->setText("edited");
    const std::string serialized = editor.toString();
    DocumentEditor reparsed;
    check(reparsed.parseJSON(serialized), "reparse serialized JSON");
    check(reparsed.findByTagName("name")[0]->text() == "edited",
          "serialize JSON node edits");
    check(!editor.parseJSON(R"({"x":01})"), "reject invalid JSON number");
    const auto path = std::filesystem::temp_directory_path() /
        "entityeditor_invalid_json_save.json";
    {
        std::ofstream existing(path, std::ios::binary);
        existing << "sentinel";
    }
    editor.findByTagName("values")[0]->children()[0]->setText("not a number");
    check(!editor.saveFile(path.string()), "reject invalid edited JSON number");
    std::ifstream saved(path, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(saved)),
                        std::istreambuf_iterator<char>());
    check(contents == "sentinel", "failed serialization does not truncate output");
    std::error_code error;
    std::filesystem::remove(path, error);
    check(!error, "remove temporary invalid JSON output");
}

void testYaml() {
    DocumentEditor editor;
    check(editor.parseYAML("\"root scalar\""),
          "parse scalar YAML document");
    check(editor.toString() == "\"root scalar\"\n",
          "serialize scalar YAML document");
    check(editor.parseYAML(
        "name: Demo\n"
        "items:\n"
        "  - one\n"
        "  - two\n"
        "metadata:\n"
        "  enabled: true\n"
        "  count: 2\n"),
        "parse block YAML mappings and sequences");
    check(editor.findByTagName("name").size() == 1, "find YAML mapping value");
    check(editor.parseYAML("empty: []\n"), "parse empty flow sequence");
    check(editor.toString().find("empty: []") != std::string::npos,
          "serialize empty YAML sequence");
    check(editor.parseYAML("items: [one, 2, {active: true}]\n"),
          "parse YAML flow collections");
    check(editor.toString().find("active: true") != std::string::npos,
          "serialize nested YAML flow mapping");
    check(editor.parseYAML("message: |\n  line one\n  line two\n"),
          "parse simple YAML literal block scalar");
    check(editor.findByTagName("message")[0]->text() == "line one\nline two",
          "preserve lines in YAML block scalar");
    check(!editor.parseYAML("message: |\n  line one\n\n  line two\n"),
          "reject unsupported blank line inside YAML block scalar");
    check(editor.parseYAML(
        "name: Demo\n"
        "items:\n"
        "  - one\n"
        "  - two\n"
        "metadata:\n"
        "  enabled: true\n"
        "  count: 2\n"),
        "restore YAML mapping after flow collection check");
    check(editor.toString().find("  - one") != std::string::npos,
          "serialize nested YAML sequence");
    DocumentEditor reparsed;
    check(reparsed.parseYAML(editor.toString()), "reparse serialized YAML");
    check(!editor.parseYAML("items:\n  - one\n   - bad\n"),
          "reject invalid YAML indentation");
    check(!editor.parseYAML("a: one\n---\nb: two\n"),
          "reject unsupported multiple YAML documents");
}

void testCss() {
    DocumentEditor editor;
    check(editor.parseCSS(
        "/* theme */\n"
        "body, .card:hover { color: red; margin: calc(2px + 1px) }\n"
        ".card { --theme: {color: red}; }\n"
        "@media screen and (min-width: 10px) { .card { display: block; } }\n"),
        "parse CSS rules, comments, functions, and nested at-rule");
    auto color = editor.findByTagName("color");
    check(color.size() == 1 && color[0]->text() == "red",
          "find CSS declaration");
    color[0]->setText("blue");
    DocumentEditor reparsed;
    check(reparsed.parseCSS(editor.toString()), "reparse serialized CSS");
    check(reparsed.findByTagName("color")[0]->text() == "blue",
          "serialize CSS declaration edit");
    check(editor.parseCSS("a {} @layer reset;"),
          "parse empty CSS rule and semicolon at-rule");
    check(editor.toString().find("a {\n}") != std::string::npos &&
          editor.toString().find("@layer reset;") != std::string::npos,
          "distinguish empty CSS blocks from semicolon at-rules");
    check(!editor.parseCSS("a { color: red;"),
          "reject unclosed CSS block");
}

void testFileSaveAndLoad() {
    const auto path = std::filesystem::temp_directory_path() /
        "entityeditor_parser_test.xhtml";
    DocumentEditor editor;
    check(editor.parseXHTML("<root><item id=\"saved\">before</item></root>"),
          "parse XHTML before saving");
    check(editor.setTextById("saved", "after"), "edit XHTML before saving");
    check(editor.saveFile(path.string()), "save edited XHTML");
    DocumentEditor loaded;
    check(loaded.loadFile(path.string()), "load saved XHTML by extension");
    check(loaded.findById("saved") &&
          loaded.findById("saved")->getTextContent() == "after",
          "saved XHTML preserves edit");
    std::error_code error;
    std::filesystem::remove(path, error);
    check(!error, "remove temporary parser test file");

    const auto cssPath = std::filesystem::temp_directory_path() /
        "entityeditor_parser_test.css";
    DocumentEditor css;
    check(css.parseCSS(".card { color: red; }"), "parse CSS before saving");
    check(css.saveFile(cssPath.string()), "save CSS file");
    DocumentEditor loadedCss;
    check(loadedCss.loadFile(cssPath.string()), "load CSS by extension");
    check(loadedCss.findByTagName("color").size() == 1,
          "loaded CSS exposes declaration nodes");
    std::filesystem::remove(cssPath, error);
    check(!error, "remove temporary CSS test file");
}

void testUtf8Bom() {
    const std::string bom = "\xEF\xBB\xBF";
    DocumentEditor editor;
    check(editor.parseHTML(bom + "<p>text</p>"), "accept UTF-8 BOM in HTML");
    check(editor.parseXHTML(bom + "<root/>"), "accept UTF-8 BOM in XML");
    check(editor.parseJSON(bom + "{\"ok\":true}"), "accept UTF-8 BOM in JSON");
    check(editor.parseYAML(bom + "key: value\n"), "accept UTF-8 BOM in YAML");
    check(editor.parseCSS(bom + "a { color: red; }"), "accept UTF-8 BOM in CSS");
    check(editor.parseCSV(bom + "key\nvalue\n"), "accept UTF-8 BOM in CSV");
    check(editor.getCSVHeaders().size() == 1 &&
          editor.getCSVHeaders()[0] == "key",
          "remove CSV BOM from first header");
}

} // namespace

int main() {
    testCsv();
    testHtmlAndXhtml();
    testXml();
    testJson();
    testYaml();
    testCss();
    testFileSaveAndLoad();
    testUtf8Bom();

    if (failures != 0) {
        std::cerr << failures << " parser test(s) failed\n";
        return 1;
    }
    std::cout << "All parser tests passed\n";
}
