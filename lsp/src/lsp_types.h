#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace LspTypes {

using json = nlohmann::json;

struct Position {
    int line = 0;
    int character = 0;
};

inline void to_json(json &j, const Position &p) {
    j = json{{"line", p.line}, {"character", p.character}};
}

inline void from_json(const json &j, Position &p) {
    j.at("line").get_to(p.line);
    j.at("character").get_to(p.character);
}

struct Range {
    Position start;
    Position end;
};

inline void to_json(json &j, const Range &r) {
    j = json{{"start", r.start}, {"end", r.end}};
}

inline void from_json(const json &j, Range &r) {
    j.at("start").get_to(r.start);
    j.at("end").get_to(r.end);
}

struct Location {
    std::string uri;
    Range range;
};

inline void to_json(json &j, const Location &l) {
    j = json{{"uri", l.uri}, {"range", l.range}};
}

enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

struct Diagnostic {
    Range range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string source = "skald";
};

inline void to_json(json &j, const Diagnostic &d) {
    j = json{{"range", d.range},
             {"severity", static_cast<int>(d.severity)},
             {"message", d.message},
             {"source", d.source}};
}

enum class CompletionItemKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Field = 6,
    Variable = 6,
    Class = 7,
    Module = 9,
    Property = 10,
    File = 17,
    Reference = 18,
    Keyword = 14
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind = CompletionItemKind::Text;
    std::string detail;
};

inline void to_json(json &j, const CompletionItem &c) {
    j = json{{"label", c.label}, {"kind", static_cast<int>(c.kind)}};
    if (!c.detail.empty()) {
        j["detail"] = c.detail;
    }
}

enum class SymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Variable = 13,
    Constant = 14,
    String = 15,
    Function = 12,
    Key = 20
};

struct DocumentSymbol {
    std::string name;
    SymbolKind kind;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children;
};

inline void to_json(json &j, const DocumentSymbol &s) {
    j = json{{"name", s.name},
             {"kind", static_cast<int>(s.kind)},
             {"range", s.range},
             {"selectionRange", s.selectionRange}};
    if (!s.children.empty()) {
        j["children"] = s.children;
    }
}

struct TextDocumentIdentifier {
    std::string uri;
};

inline void from_json(const json &j, TextDocumentIdentifier &t) {
    j.at("uri").get_to(t.uri);
}

struct TextDocumentItem {
    std::string uri;
    std::string languageId;
    int version = 0;
    std::string text;
};

inline void from_json(const json &j, TextDocumentItem &t) {
    j.at("uri").get_to(t.uri);
    j.at("text").get_to(t.text);
    if (j.contains("version"))
        j.at("version").get_to(t.version);
    if (j.contains("languageId"))
        j.at("languageId").get_to(t.languageId);
}

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;
};

inline void from_json(const json &j, TextDocumentPositionParams &p) {
    j.at("textDocument").get_to(p.textDocument);
    j.at("position").get_to(p.position);
}

} // namespace LspTypes
