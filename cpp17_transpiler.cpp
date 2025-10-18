// Compiler.cpp - Single translation unit for a CASE Programming Language -> C++ transpiler (C++14)

#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
// Note: <filesystem> is C++17; for C++14 compatibility, consider using boost::filesystem or manual directory creation.
// For now, assuming C++17 support or replacing with manual mkdir if needed.
#include <filesystem>

namespace ciam {

class MacroRegistry {
public:
    static void registerMacro(const std::string& name, const std::string& body);
    static std::string getMacro(const std::string& name);
    static void persist();
    static void load();

private:
    static std::unordered_map<std::string, std::string> macros;
    static const std::string registryPath;
};

} // namespace ciam

// Implementation of MacroRegistry
namespace ciam {

std::unordered_map<std::string, std::string> MacroRegistry::macros;
const std::string MacroRegistry::registryPath = "build/macro_registry.case";

void MacroRegistry::registerMacro(const std::string& name, const std::string& body) {
    macros[name] = body;
}

std::string MacroRegistry::getMacro(const std::string& name) {
    return macros.count(name) ? macros[name] : "macro " + name + "() { ret -1 }";
}

void MacroRegistry::persist() {
    std::filesystem::create_directories("build");  // Ensure directory exists
    std::ofstream out(registryPath);
    for (const auto& [name, body] : macros) {
        out << "macro " << name << "() { " << body << " }\n";
    }
    out.close();
}

void MacroRegistry::load() {
    std::ifstream in(registryPath);
    std::string line;
    while (std::getline(in, line)) {
        auto start = line.find("macro ") + 6;
        auto end = line.find("()");
        auto bodyStart = line.find("{") + 1;
        auto bodyEnd = line.find("}");
        if (start != std::string::npos && end != std::string::npos) {
            std::string name = line.substr(start, end - start);
            std::string body = line.substr(bodyStart, bodyEnd - bodyStart);
            macros[name] = body;
        }
    }
    in.close();
}

} // namespace ciam

// Additional functions
namespace ciam {

void emitFixerMacros(int errorCode) {
    switch (errorCode) {
        case 1:
            MacroRegistry::registerMacro("fix_error_1", "ret 42");
            break;
        case 2:
            MacroRegistry::registerMacro("fix_error_2", "ret x * 2");
            break;
        case 404:
            MacroRegistry::registerMacro("fix_missing", "ret 0");
            break;
        default:
            MacroRegistry::registerMacro("fix_unknown", "ret -1");
            break;
    }
    MacroRegistry::persist();
}

} // namespace ciam

// Utility functions
void invokeMacroFix(const std::string& macroName, int arg = 0) {
    std::string body = ciam::MacroRegistry::getMacro(macroName);
    std::string substituted = substituteArg(body, arg);
    std::cout << "Invoking macro " << macroName << ": " << substituted << "\n";
    // Optional: parse and execute macro body with arg substitution
}

std::string substituteArg(const std::string& body, int arg) {
    std::string result = body;
    size_t pos = result.find("x");
    if (pos != std::string::npos) {
        result.replace(pos, 1, std::to_string(arg));
    }
    return result;
}

void emitMacroFromOverlay(const std::string& overlay) {
    if (overlay == "audit") {
        ciam::MacroRegistry::registerMacro("audit_trace", "ret 'AST audited'");
    } else if (overlay == "mutate") {
        ciam::MacroRegistry::registerMacro("mutate_patch", "ret 'mutation applied'");
    }
    ciam::MacroRegistry::persist();
}

// Existing includes and code follow...
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <condition_variable>

#include "intelligence.hpp" // CIAM preprocessor (write_stdout, overlays/inspect, sandbox/audit, base-12)

// ------------------------ Lexer ------------------------

enum class TokenType { IDENT, STRING, NUMBER, KEYWORD, SYMBOL, END };

struct Token {
    TokenType type;
    std::string value;
    int line;
};

static bool isIdentStart(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) || c == '_';
}
static bool isIdentChar(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_';
}
static bool isSymbolChar(char c) {
    switch (c) {
    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    case '=':
    case ';':
    case ',':
    case '+':
    case '-':
    case '*':
    case '/':
    case '<':
    case '>':
    case '!':
    case '&':
    case '|':
    case '%':
    case ':':
    case '.':
    case '?':
    case '^':
    case '~':
    case '@':
    case '#':
    case '$':
        return true;
    default:
        return false;
    }
}

static std::string readSymbolToken(const std::string& src, size_t& idx) {
    static const std::unordered_set<std::string> multiChar = {
        "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "++", "--", "->", "::", "<<", ">>",
        "<<=", ">>=", "&=", "|=", "^=", "?:", "->*", ".*", "@=", "#=", "$="
    };
    char c = src[idx];
    std::string one(1, c);
    if (idx + 1 < src.size()) {
        std::string two = one + src[idx + 1];
        if (multiChar.count(two)) { idx += 2; return two; }
        if (idx + 2 < src.size()) {
            std::string three = two + src[idx + 2];
            if (multiChar.count(three)) { idx += 3; return three; }
        }
    }
    ++idx;
    return one;
}

std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1;

    auto push = [&](TokenType t, const std::string& v) {
        tokens.push_back(Token{ t, v, line });
    };

    auto isKeyword = [](const std::string& s) {
        static const std::unordered_set<std::string> keywords = {
            "Print", "ret", "loop", "if", "else", "Fn", "call", "let", "while", "break", "continue",
            "switch", "case", "default", "overlay", "open", "write", "writeln", "read", "close",
            "mutate", "scale", "bounds", "checkpoint", "vbreak", "channel", "send", "recv", "sync",
            "schedule", "input", "true", "false", "struct", "enum", "union", "typedef", "const",
            "volatile", "static", "extern", "inline", "auto", "void", "int", "float", "double",
            "char", "bool", "string", "array", "list", "dict", "thread", "async", "batch", "sync",
            "parallel", "serialize", "deserialize", "compress", "decompress", "obfuscate", "deobfuscate",
            "ping", "resource", "environment", "deadcode", "dce", "fold", "peephole", "unroll",
            "vectorize", "profile", "lookahead", "index", "derive", "chain", "complex", "routine",
            "synchronized", "deepparallel", "syncopated", "routed", "ciam", "contract", "nonneg",
            "audit", "replay", "inspect", "mutate", "evolve", "dictionary", "ambiguous", "logic"
        };
        return keywords.count(s);
    };

    auto readNumber = [&](size_t& idx) {
        size_t start = idx++;
        bool hasDot = false;
        bool hasExp = false;
        while (idx < src.size()) {
            char ch = src[idx];
            if (std::isdigit(static_cast<unsigned char>(ch))) { ++idx; continue; }
            if (!hasDot && ch == '.') { hasDot = true; ++idx; continue; }
            if (!hasExp && (ch == 'e' || ch == 'E')) {
                hasExp = true; ++idx;
                if (idx < src.size() && (src[idx] == '+' || src[idx] == '-')) ++idx;
                while (idx < src.size() && std::isdigit(static_cast<unsigned char>(src[idx]))) ++idx;
                break;
            }
            break;
        }
        return src.substr(start, idx - start);
    };

    while (i < src.size()) {
        char c = src[i];

        // Newlines
        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r') {
            ++i;
            continue;
        }

        // Line comments: // ...
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            i += 2;
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }

        // Identifiers / keywords
        if (isIdentStart(c)) {
            size_t start = i++;
            while (i < src.size() && isIdentChar(src[i])) ++i;
            std::string ident = src.substr(start, i - start);
            if (isKeyword(ident)) {
                push(TokenType::KEYWORD, ident);
            }
            else {
                push(TokenType::IDENT, ident);
            }
            continue;
        }

        // Numbers (integer/decimal/scientific)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string num = readNumber(i);
            push(TokenType::NUMBER, num);
            continue;
        }

        // Strings: " ... " with simple escapes
        if (c == '"') {
            ++i; // consume opening quote
            std::string acc;
            bool closed = false;
            while (i < src.size()) {
                char ch = src[i++];
                if (ch == '"') {
                    closed = true;
                    break;
                }
                if (ch == '\\' && i < src.size()) {
                    char esc = src[i++];
                    switch (esc) {
                    case 'n': acc.push_back('\n'); break;
                    case 't': acc.push_back('\t'); break;
                    case 'r': acc.push_back('\r'); break;
                    case '\\': acc.push_back('\\'); break;
                    case '"': acc.push_back('"'); break;
                    default: acc.push_back(esc); break;
                    }
                }
                else {
                    acc.push_back(ch);
                }
            }
            if (!closed) throw std::runtime_error("Unterminated string literal at line " + std::to_string(line));
            push(TokenType::STRING, acc);
            continue;
        }

        // Symbols (single char)
        if (isSymbolChar(c)) {
            push(TokenType::SYMBOL, std::string(1, c));
            ++i;
            continue;
        }

        // Unknown character
        throw std::runtime_error(std::string("Unexpected character '") + c + "' at line " + std::to_string(line));
    }

    tokens.push_back(Token{ TokenType::END, "", line });
    return tokens;
}

// ------------------------ AST ------------------------

struct Node {
    std::string type;           // e.g., Program, Print, Let, Fn, While, ... Scale, Bounds, Checkpoint, VBreak, Channel, Send, Recv, Schedule, Sync
    std::string value;          // payload (e.g., name, operator, string literal, loop header, label, priority)
    std::vector<Node*> children;// generic children
};

// ------------------------ Parser ------------------------

static std::vector<Token> toks;
static size_t pos = 0;

static const Token& peek() {
    if (pos < toks.size()) return toks[pos];
    return toks.back(); // should be END
}
static const Token& previous() {
    if (pos == 0) return toks[0];
    return toks[pos - 1];
}
static const Token& advanceTok() {
    if (pos < toks.size()) ++pos;
    return previous();
}
static bool checkValue(const std::string& v) {
    return peek().value == v;
}
static bool checkType(TokenType t) { return peek().type == t; }
static bool matchValue(const std::string& v) {
    if (checkValue(v)) {
        advanceTok();
        return true;
    }
    return false;
}

static Node* parseStatement(); // fwd
static Node* parseExpression(); // fwd
static Node* parseType(); // fwd
static Node* parseStruct(); // fwd
static Node* parseArray(); // fwd
static Node* parseList(); // fwd
static Node* parseDict(); // fwd
static Node* parseThread(); // fwd
static Node* parseAsync(); // fwd
static Node* parseBatch(); // fwd
static Node* parseSerialize(); // fwd
static Node* parseCompress(); // fwd
static Node* parseObfuscate(); // fwd
static Node* parsePing(); // fwd
static Node* parseDerive(); // fwd
static Node* parseChain(); // fwd
static Node* parseContract(); // fwd

// Overlay directive buffer (applies to next Fn)
static std::vector<Node*> gPendingOverlays;

static void skipBracketBlockIfPresent() {
    if (matchValue("[")) {
        // Consume until the matching closing ']'
        int depth = 1;
        while (peek().type != TokenType::END && depth > 0) {
            if (matchValue("[")) {
                ++depth;
                continue;
            }
            if (matchValue("]")) {
                --depth;
                continue;
            }
            advanceTok();
        }
    }
}

static Node* parseBlock() {
    // Expects current token to be '{'
    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start a block at line " + std::to_string(peek().line));
    Node* body = new Node{ "Body", "" };
    while (peek().type != TokenType::END && !checkValue("}")) {
        body->children.push_back(parseStatement());
    }
    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close a block at line " + std::to_string(peek().line));
    return body;
}

// Allow Print "..." or Print <expr>
static Node* parsePrint() {
    advanceTok(); // 'Print'
    Node* n = new Node{ "Print", "" };
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    }
    else if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
        Node* expr = parseExpression();
        if (expr) n->children.push_back(expr);
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseRet() {
    advanceTok(); // 'ret'
    Node* n = new Node{ "Ret", "" };
    if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
        Node* expr = parseExpression();
        if (expr) n->children.push_back(expr);
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseCall() {
    advanceTok(); // 'call'
    Node* n = new Node{ "Call", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
        // Optional arguments (expressions) separated by comma until bracket or block start
        while (peek().type != TokenType::END && !checkValue("[") && !checkValue("}") && !checkValue("{")) {
            if (checkValue(",")) { advanceTok(); continue; }
            if (checkValue("else")) break;
            Node* arg = parseExpression();
            if (arg) n->children.push_back(arg);
            if (checkValue(",")) { advanceTok(); continue; }
            if (checkValue("{") || checkValue("}") || checkValue("[")) break;
        }
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseLet() {
    advanceTok(); // 'let'
    Node* n = new Node{ "Let", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // variable name
    }
    else {
        throw std::runtime_error("Expected identifier after 'let' at line " + std::to_string(peek().line));
    }
    if (!matchValue("=")) {
        throw std::runtime_error("Expected '=' in let statement at line " + std::to_string(peek().line));
    }
    Node* expr = parseExpression();
    if (!expr) throw std::runtime_error("Invalid expression in let at line " + std::to_string(peek().line));
    n->children.push_back(expr);
    return n;
}

static Node* parseLoop() {
    advanceTok(); // 'loop'
    Node* n = new Node{ "Loop", "" };
    // Accept either a string containing the for(...) header or a bare identifier/expression token
    if (peek().type == TokenType::STRING || peek().type == TokenType::IDENT || peek().type == TokenType::NUMBER) {
        n->value = advanceTok().value; // e.g., "@omp @unroll(4) int i=0; i<N; i++"
    }
    Node* body = parseBlock();
    n->children.push_back(body);
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseIf() {
    advanceTok(); // 'if'
    Node* n = new Node{ "If", "" };
    // condition expression until '{'
    if (!checkValue("{")) {
        Node* condExpr = parseExpression();
        Node* cond = new Node{ "Cond","" };
        cond->children.push_back(condExpr);
        n->children.push_back(cond);
    }
    Node* ifBody = parseBlock();
    n->children.push_back(ifBody);

    if (matchValue("else")) {
        Node* elseBody = parseBlock();
        elseBody->type = "Else";
        n->children.push_back(elseBody);
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseWhile() {
    advanceTok(); // 'while'
    Node* n = new Node{ "While","" };
    Node* condExpr = parseExpression();
    Node* cond = new Node{ "Cond","" };
    cond->children.push_back(condExpr);
    n->children.push_back(cond);
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
}

static Node* parseBreak() { advanceTok(); return new Node{ "Break","" }; }
static Node* parseContinue() { advanceTok(); return new Node{ "Continue","" }; }

static Node* parseSwitch() {
    advanceTok(); // 'switch'
    Node* n = new Node{ "Switch","" };
    Node* condExpr = parseExpression();
    Node* cond = new Node{ "Cond","" };
    cond->children.push_back(condExpr);
    n->children.push_back(cond);
    if (!matchValue("{")) throw std::runtime_error("Expected '{' after switch at line " + std::to_string(peek().line));
    while (!checkValue("}") && peek().type != TokenType::END) {
        if (matchValue("case")) {
            Node* caseNode = new Node{ "Case","" };
            if (peek().type == TokenType::NUMBER || peek().type == TokenType::STRING || peek().type == TokenType::IDENT) {
                caseNode->value = advanceTok().value;
            }
            else {
                throw std::runtime_error("Expected case value at line " + std::to_string(peek().line));
            }
            Node* body = parseBlock();
            caseNode->children.push_back(body);
            n->children.push_back(caseNode);
        }
        else if (matchValue("default")) {
            Node* def = new Node{ "Default","" };
            Node* body = parseBlock();
            def->children.push_back(body);
            n->children.push_back(def);
        }
        else {
            advanceTok();
        }
    }
    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close switch at line " + std::to_string(peek().line));
    return n;
}

// overlay directive: overlay <name>[, name] ... applies to next Fn
static Node* parseOverlay() {
    advanceTok(); // 'overlay'
    while (peek().type == TokenType::IDENT) {
        std::string name = advanceTok().value;
        Node* overlay = new Node{ "Overlay", name };
        if (matchValue("(")) {
            while (!checkValue(")") && peek().type != TokenType::END) {
                if (peek().type == TokenType::IDENT) {
                    Node* v = new Node{ "Var", advanceTok().value };
                    overlay->children.push_back(v);
                }
                if (matchValue(",")) continue;
                else if (matchValue(")")) break;
                else if (peek().type != TokenType::IDENT) {
                    throw std::runtime_error("Expected identifier in overlay args at line " + std::to_string(peek().line));
                }
            }
            if (!matchValue(")")) throw std::runtime_error("Expected ')' closing overlay at line " + std::to_string(peek().line));
        }
        gPendingOverlays.push_back(overlay);
        if (matchValue(",")) continue;
        else break;
    }
    skipBracketBlockIfPresent();
    return new Node{ "OverlayDecl","" };
}

// File I/O: open name "path" ["mode"], write name expr, writeln name expr, read name var, close name
static Node* parseOpen() {
    advanceTok(); // 'open'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected variable after 'open' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Open","" };
    n->value = advanceTok().value; // var name
    if (peek().type == TokenType::STRING) {
        Node* path = new Node{ "Str", advanceTok().value };
        n->children.push_back(path);
    }
    else {
        throw std::runtime_error("Expected path string in open at line " + std::to_string(peek().line));
    }
    if (peek().type == TokenType::STRING) {
        Node* mode = new Node{ "Str", advanceTok().value };
        n->children.push_back(mode);
    }
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseWriteLike(const std::string& kind) {
    advanceTok(); // 'write' or 'writeln'
    Node* n = new Node{ kind, "" };
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected stream variable after '" + kind + "' at line " + std::to_string(peek().line));
    n->value = advanceTok().value;
    if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
        Node* expr = parseExpression();
        if (expr) n->children.push_back(expr);
    }
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseRead() {
    advanceTok(); // 'read'
    Node* n = new Node{ "Read","" };
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected stream variable after 'read' at line " + std::to_string(peek().line));
    n->value = advanceTok().value; // stream var
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected target variable in 'read' at line " + std::to_string(peek().line));
    Node* target = new Node{ "Var", advanceTok().value };
    n->children.push_back(target);
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseClose() {
    advanceTok(); // 'close'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected stream variable after 'close' at line " + std::to_string(peek().line));
    return new Node{ "Close", advanceTok().value };
}

// mutate (compiler-introspection hint)
static Node* parseMutate() {
    advanceTok(); // 'mutate'
    Node* n = new Node{ "Mutate","" };
    if (peek().type == TokenType::IDENT) n->value = advanceTok().value;
    skipBracketBlockIfPresent();
    return n;
}

// scale x a b c d
static Node* parseScale() {
    advanceTok(); // 'scale'
    Node* n = new Node{ "Scale","" };
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected identifier after 'scale' at line " + std::to_string(peek().line));
    Node* target = new Node{ "Var", advanceTok().value };
    n->children.push_back(target);
    // parse 4 expressions
    for (int i = 0; i < 4; ++i) {
        Node* e = parseExpression();
        if (!e) throw std::runtime_error("Invalid scale expression at line " + std::to_string(peek().line));
        n->children.push_back(e);
    }
    skipBracketBlockIfPresent();
    return n;
}

// bounds x min max
static Node* parseBounds() {
    advanceTok(); // 'bounds'
    Node* n = new Node{ "Bounds","" };
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected identifier after 'bounds' at line " + std::to_string(peek().line));
    Node* var = new Node{ "Var", advanceTok().value };
    n->children.push_back(var);
    Node* mn = parseExpression();
    Node* mx = parseExpression();
    if (!mn || !mx) throw std::runtime_error("Invalid bounds expressions at line " + std::to_string(peek().line));
    n->children.push_back(mn);
    n->children.push_back(mx);
    skipBracketBlockIfPresent();
    return n;
}

// checkpoint label
static Node* parseCheckpoint() {
    advanceTok(); // 'checkpoint'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected label after 'checkpoint' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Checkpoint", advanceTok().value };
    skipBracketBlockIfPresent();
    return n;
}

// vbreak label
static Node* parseVBreak() {
    advanceTok(); // 'vbreak'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected label after 'vbreak' at line " + std::to_string(peek().line));
    Node* n = new Node{ "VBreak", advanceTok().value };
    skipBracketBlockIfPresent();
    return n;
}

// channel name "Type"
static Node* parseChannel() {
    advanceTok(); // 'channel'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected channel name after 'channel' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Channel","" };
    n->value = advanceTok().value;
    if (!checkType(TokenType::STRING)) throw std::runtime_error("Expected type string for channel at line " + std::to_string(peek().line));
    Node* ty = new Node{ "Str", advanceTok().value };
    n->children.push_back(ty);
    skipBracketBlockIfPresent();
    return n;
}

// send ch expr
static Node* parseSend() {
    advanceTok(); // 'send'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected channel name after 'send' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Send","" };
    n->value = advanceTok().value;
    Node* e = parseExpression();
    if (!e) throw std::runtime_error("Expected expression in send at line " + std::to_string(peek().line));
    n->children.push_back(e);
    skipBracketBlockIfPresent();
    return n;
}

// recv ch var
static Node* parseRecv() {
    advanceTok(); // 'recv'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected channel name after 'recv' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Recv","" };
    n->value = advanceTok().value;
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected target variable for recv at line " + std::to_string(peek().line));
    Node* var = new Node{ "Var", advanceTok().value };
    n->children.push_back(var);
    skipBracketBlockIfPresent();
    return n;
}

// schedule priority { body }
static Node* parseSchedule() {
    advanceTok(); // 'schedule'
    Node* n = new Node{ "Schedule","" };
    // optional numeric priority
    if (checkType(TokenType::NUMBER)) {
        n->value = advanceTok().value;
    }
    else {
        n->value = "0";
    }
    Node* body = parseBlock();
    n->children.push_back(body);
    skipBracketBlockIfPresent();
    return n;
}

// sync barrier
static Node* parseSync() {
    advanceTok();
    return new Node{ "Sync","" };
}

static Node* parseFn() {
    advanceTok(); // 'Fn'
    Node* n = new Node{ "Fn", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // function name
    }
    else {
        throw std::runtime_error("Expected function name after 'Fn' at line " + std::to_string(peek().line));
    }
    // Attach pending overlays as children (Overlay nodes)
    for (auto* ov : gPendingOverlays) {
        n->children.push_back(ov);
    }
    gPendingOverlays.clear();
    // Use braces for function body
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
}

// ----- Expression parsing (precedence: * / > + -) -----

static int precedenceOf(const std::string& op) {
    if (op == "*" || op == "/") return 20;
    if (op == "+" || op == "-") return 10;
    return -1;
}

static Node* parsePrimary() {
    if (peek().type == TokenType::NUMBER) {
        return new Node{ "Num", advanceTok().value };
    }
    if (peek().type == TokenType::STRING) {
        return new Node{ "Str", advanceTok().value };
    }
    if (peek().type == TokenType::KEYWORD && (peek().value == "true" || peek().value == "false")) {
        return new Node{ "Bool", advanceTok().value };
    }
    if (peek().type == TokenType::IDENT) {
        return new Node{ "Var", advanceTok().value };
    }
    if (checkValue("(")) {
        advanceTok(); // (
        Node* inner = parseExpression();
        if (!matchValue(")")) throw std::runtime_error("Expected ')' in expression at line " + std::to_string(peek().line));
        return inner;
    }
    throw std::runtime_error("Expected expression at line " + std::to_string(peek().line));
}

static Node* parseBinOpRHS(int minPrec, Node* lhs) {
    while (true) {
        const Token& t = peek();
        if (t.type != TokenType::SYMBOL) return lhs;
        std::string op = t.value;
        int prec = precedenceOf(op);
        if (prec < minPrec) return lhs;

        advanceTok(); // consume operator
        Node* rhs = parsePrimary();
        int nextPrec = precedenceOf(peek().value);
        if (nextPrec > prec) {
            rhs = parseBinOpRHS(prec + 1, rhs);
        }

        Node* bin = new Node{ "BinOp", op };
        bin->children.push_back(lhs);
        bin->children.push_back(rhs);
        lhs = bin;
    }
}

static Node* parseExpression() {
    Node* lhs = parsePrimary();
    return parseBinOpRHS(0, lhs);
}

static Node* parseType() {
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected type name at line " + std::to_string(peek().line));
    std::string ty = advanceTok().value;
    if (ty == "struct") return parseStruct();
    if (ty == "array") return parseArray();
    if (ty == "list") return parseList();
    if (ty == "dict") return parseDict();
    // Basic types
    Node* n = new Node{ "Type", ty };
    return n;
}

static Node* parseStruct() {
    Node* n = new Node{ "Struct", "" };
    if (!matchValue("{")) throw std::runtime_error("Expected '{' after struct at line " + std::to_string(peek().line));
    while (!checkValue("}") && peek().type != TokenType::END) {
        if (peek().type == TokenType::IDENT) {
            std::string fieldTy = advanceTok().value;
            if (peek().type == TokenType::IDENT) {
                std::string fieldName = advanceTok().value;
                Node* field = new Node{ "Field", fieldName };
                field->children.push_back(new Node{ "Type", fieldTy });
                n->children.push_back(field);
            }
        }
        if (!matchValue(";")) break;
    }
    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close struct at line " + std::to_string(peek().line));
    return n;
}

static Node* parseArray() {
    std::string elemTy = advanceTok().value; // e.g., int
    if (!matchValue("[")) throw std::runtime_error("Expected '[' in array at line " + std::to_string(peek().line));
    Node* sizeExpr = parseExpression();
    if (!matchValue("]")) throw std::runtime_error("Expected ']' in array at line " + std::to_string(peek().line));
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected array name at line " + std::to_string(peek().line));
    std::string name = advanceTok().value;
    Node* n = new Node{ "Array", name };
    n->children.push_back(new Node{ "Type", elemTy });
    n->children.push_back(sizeExpr);
    return n;
}

static Node* parseList() {
    std::string elemTy = advanceTok().value;
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected list name at line " + std::to_string(peek().line));
    std::string name = advanceTok().value;
    Node* n = new Node{ "List", name };
    n->children.push_back(new Node{ "Type", elemTy });
    if (matchValue("=")) {
        if (!matchValue("{")) throw std::runtime_error("Expected '{' for list init at line " + std::to_string(peek().line));
        while (!checkValue("}") && peek().type != TokenType::END) {
            Node* elem = parseExpression();
            n->children.push_back(elem);
            if (!matchValue(",")) break;
        }
        if (!matchValue("}")) throw std::runtime_error("Expected '}' to close list at line " + std::to_string(peek().line));
    }
    return n;
}

static Node* parseDict() {
    std::string keyTy = advanceTok().value;
    if (!matchValue(":")) throw std::runtime_error("Expected ':' in dict type at line " + std::to_string(peek().line));
    std::string valTy = advanceTok().value;
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected dict name at line " + std::to_string(peek().line));
    std::string name = advanceTok().value;
    Node* n = new Node{ "Dict", name };
    n->children.push_back(new Node{ "Type", keyTy + ":" + valTy });
    if (matchValue("=")) {
        if (!matchValue("{")) throw std::runtime_error("Expected '{' for dict init at line " + std::to_string(peek().line));
        while (!checkValue("}") && peek().type != TokenType::END) {
            Node* key = parseExpression();
            if (!matchValue(":")) throw std::runtime_error("Expected ':' in dict pair at line " + std::to_string(peek().line));
            Node* val = parseExpression();
            Node* pair = new Node{ "Pair", "" };
            pair->children.push_back(key);
            pair->children.push_back(val);
            n->children.push_back(pair);
            if (!matchValue(",")) break;
        }
        if (!matchValue("}")) throw std::runtime_error("Expected '}' to close dict at line " + std::to_string(peek().line));
    }
    return n;
}

static Node* parseThread() {
    advanceTok(); // 'thread'
    Node* n = new Node{ "Thread", "" };
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
}

static Node* parseAsync() {
    advanceTok(); // 'async'
    Node* n = new Node{ "Async", "" };
    Node* expr = parseExpression(); // e.g., call func
    n->children.push_back(expr);
    return n;
}

static Node* parseBatch() {
    advanceTok(); // 'batch'
    Node* n = new Node{ "Batch", "" };
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
}

static Node* parseSerialize() {
    advanceTok(); // 'serialize'
    Node* n = new Node{ "Serialize", "" };
    Node* obj = parseExpression();
    if (!matchValue("to")) throw std::runtime_error("Expected 'to' in serialize at line " + std::to_string(peek().line));
    Node* file = parseExpression();
    n->children.push_back(obj);
    n->children.push_back(file);
    return n;
}

static Node* parseCompress() {
    advanceTok(); // 'compress'
    Node* n = new Node{ "Compress", "" };
    Node* data = parseExpression();
    n->children.push_back(data);
    return n;
}

static Node* parseObfuscate() {
    advanceTok(); // 'obfuscate'
    Node* n = new Node{ "Obfuscate", "" };
    Node* target = parseExpression();
    if (matchValue("with")) {
        Node* key = parseExpression();
        n->children.push_back(key);
    }
    n->children.push_back(target);
    return n;
}

static Node* parsePing() {
    advanceTok(); // 'ping'
    Node* n = new Node{ "Ping", "" };
    if (matchValue("resource")) {
        Node* res = parseExpression();
        n->children.push_back(res);
    } else if (matchValue("environment")) {
        Node* env = parseExpression();
        n->children.push_back(env);
    }
    return n;
}

static Node* parseDerive() {
    advanceTok(); // 'derive'
    Node* n = new Node{ "Derive", "" };
    Node* func = parseExpression();
    if (!matchValue("at")) throw std::runtime_error("Expected 'at' in derive at line " + std::to_string(peek().line));
    Node* point = parseExpression();
    n->children.push_back(func);
    n->children.push_back(point);
    return n;
}

static Node* parseChain() {
    advanceTok(); // 'chain'
    Node* n = new Node{ "Chain", "" };
    while (true) {
        Node* expr = parseExpression();
        n->children.push_back(expr);
        if (!matchValue("->")) break;
    }
    return n;
}

static Node* parseContract() {
    advanceTok(); // 'contract'
    Node* n = new Node{ "Contract", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // e.g., nonneg
        while (peek().type == TokenType::IDENT) {
            Node* var = new Node{ "Var", advanceTok().value };
            n->children.push_back(var);
        }
    }
    return n;
}

static Node* parseStatement() {
    const std::string v = peek().value;
    if (v == "Print") return parsePrint();
    if (v == "ret") return parseRet();
    if (v == "loop") return parseLoop();
    if (v == "if") return parseIf();
    if (v == "while") return parseWhile();
    if (v == "break") return parseBreak();
    if (v == "continue") return parseContinue();
    if (v == "switch") return parseSwitch();
    if (v == "Fn") return parseFn();
    if (v == "call") return parseCall();
    if (v == "let") return parseLet();
    if (v == "overlay") return parseOverlay();
    if (v == "open") return parseOpen();
    if (v == "write") return parseWriteLike("Write");
    if (v == "writeln") return parseWriteLike("Writeln");
    if (v == "read") return parseRead();
    if (v == "close") return parseClose();
    if (v == "mutate") return parseMutate();
    if (v == "scale") return parseScale();
    if (v == "bounds") return parseBounds();
    if (v == "checkpoint") return parseCheckpoint();
    if (v == "vbreak") return parseVBreak();
    if (v == "channel") return parseChannel();
    if (v == "send") return parseSend();
    if (v == "recv") return parseRecv();
    if (v == "schedule") return parseSchedule();
    if (v == "sync") return parseSync();
    if (v == "struct") return parseStruct();
    if (v == "array") return parseArray();
    if (v == "list") return parseList();
    if (v == "dict") return parseDict();
    if (v == "thread") return parseThread();
    if (v == "async") return parseAsync();
    if (v == "batch") return parseBatch();
    if (v == "serialize") return parseSerialize();
    if (v == "compress") return parseCompress();
    if (v == "obfuscate") return parseObfuscate();
    if (v == "ping") return parsePing();
    if (v == "derive") return parseDerive();
    if (v == "chain") return parseChain();
    if (v == "contract") return parseContract();

    advanceTok();
    return new Node{ "Unknown", v };
}

Node* parseProgram(const std::vector<Token>& t) {
    toks = t;
    pos = 0;
    Node* root = new Node{ "Program", "" };
    while (peek().type != TokenType::END) {
        root->children.push_back(parseStatement());
    }
    return root;
}

// ------------------------ Simple Semantic Analyzer (type check) ------------------------

enum class TypeKind { Unknown, Number, String, Boolean, Struct, Array, List, Dict, Void };

static TypeKind inferExpr(Node* e, const std::vector<std::unordered_map<std::string, TypeKind>>& scopes) {
    if (!e) return TypeKind::Unknown;
    if (e->type == "Num") return TypeKind::Number;
    if (e->type == "Str") return TypeKind::String;
    if (e->type == "Bool") return TypeKind::Boolean;
    if (e->type == "Var") {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto f = it->find(e->value);
            if (f != it->end()) return f->second;
        }
        std::cerr << "[warn] use of undefined variable '" << e->value << "'\n";
        return TypeKind::Unknown;
    }
    if (e->type == "BinOp") {
        TypeKind L = inferExpr(e->children[0], scopes);
        TypeKind R = inferExpr(e->children[1], scopes);
        if (e->value == "+") {
            if (L == TypeKind::String || R == TypeKind::String) return TypeKind::String;
        }
        return TypeKind::Number;
    }
    if (e->type == "Struct") return TypeKind::Struct;
    if (e->type == "Array") return TypeKind::Array;
    if (e->type == "List") return TypeKind::List;
    if (e->type == "Dict") return TypeKind::Dict;
    if (e->type == "Derive") return TypeKind::Number;
    return TypeKind::Unknown;
}

static void analyze(Node* n, std::vector<std::unordered_map<std::string, TypeKind>>& scopes) {
    if (!n) return;
    if (n->type == "Body" || n->type == "Program") {
        scopes.push_back({});
        for (auto* c : n->children) analyze(c, scopes);
        scopes.pop_back();
        return;
    }
    if (n->type == "Fn" || n->type == "If" || n->type == "While" || n->type == "Switch" || n->type == "Schedule") {
        for (auto* c : n->children) analyze(c, scopes);
        return;
    }
    if (n->type == "Let") {
        TypeKind t = inferExpr(n->children.empty() ? nullptr : n->children[0], scopes);
        if (scopes.empty()) scopes.push_back({});
        scopes.back()[n->value] = t;
    }
    if (n->type == "Scale" || n->type == "Bounds") {
        for (size_t i = 1; i < n->children.size(); ++i) {
            TypeKind tk = inferExpr(n->children[i], scopes);
            if (tk == TypeKind::String) {
                std::cerr << "[warn] string used where number expected in '" << n->type << "'\n";
            }
        }
    }
    for (auto* c : n->children) analyze(c, scopes);
}

// ------------------------ Introspection & Plugin System ------------------------

static bool gEnableInspect = false;
static bool gEnableReplay = false;
static bool gEnableMutate = false;

struct Snapshot {
    std::string phase;
    std::string payload;
};
static std::vector<Snapshot> gReplay;

static void dumpIndent(std::ostringstream& os, int n) {
    for (int i = 0; i < n; ++i) os << ' ';
}

static void dumpASTRec(Node* n, std::ostringstream& os, int depth) {
    if (!n) return;
    dumpIndent(os, depth);
    os << n->type;
    if (!n->value.empty()) os << "(" << n->value << ")";
    os << "\n";
    for (auto* c : n->children) dumpASTRec(c, os, depth + 2);
}

static std::string dumpAST(Node* root) {
    std::ostringstream os;
    dumpASTRec(root, os, 0);
    return os.str();
}

using AstTransformFn = bool(*)(Node*& root, const char* passName);
using AstSinkFn = void(*)(const char* phase, const Node* root);
using TextSinkFn = void(*)(const char* phase, const char* text, size_t len);

struct PluginRegistry {
    std::vector<std::pair<std::string, AstTransformFn>> transforms;
    std::vector<AstSinkFn> astSinks;
    std::vector<TextSinkFn> textSinks;

    void RegisterTransform(const std::string& name, AstTransformFn fn) {
        transforms.emplace_back(name, fn);
    }
    void RegisterAstSink(AstSinkFn fn) { astSinks.push_back(fn); }
    void RegisterTextSink(TextSinkFn fn) { textSinks.push_back(fn); }
};

static PluginRegistry& Registry() {
    static PluginRegistry R;
    return R;
}

static void emitEventAst(const char* phase, Node* root) {
    if (gEnableInspect || gEnableReplay) {
        for (auto& s : Registry().astSinks) s(phase, root);
    }
    if (gEnableReplay) {
        gReplay.push_back({ phase, dumpAST(root) });
    }
}

static void emitEventText(const char* phase, const std::string& text) {
    if (gEnableInspect || gEnableReplay) {
        for (auto& s : Registry().textSinks) s(phase, text.c_str(), text.size());
    }
    if (gEnableReplay) {
        gReplay.push_back({ phase, text });
    }
}

static void runTransforms(const char* passPoint, Node*& root) {
    if (!gEnableMutate) return;
    for (auto& p : Registry().transforms) {
        if (p.second) {
            bool changed = p.second(root, passPoint);
            (void)changed;
        }
    }
}

static void builtinAstSink(const char* /*phase*/, const Node* /*root*/) {}
static void builtinTextSink(const char* /*phase*/, const char* /*text*/, size_t /*len*/) {}

static bool mutateEvolve(Node*& root, const char* passPoint) {
    if (std::string(passPoint) == "before-opt" || std::string(passPoint) == "after-opt") {
        optimize(root);
        optimize(root);
        return true;
    }
    return false;
}

static void collectOverlaysFlags(Node* n) {
    if (!n) return;
    if (n->type == "Overlay") {
        if (n->value == "mutate") gEnableMutate = true;
        if (n->value == "inspect") gEnableInspect = true;
        if (n->value == "replay") gEnableReplay = true;
        if (n->value == "audit") gEnableInspect = true;
    }
    if (n->type == "Mutate") gEnableMutate = true;
    for (auto* c : n->children) collectOverlaysFlags(c);
}

static void initPluginsOnce() {
    static bool inited = false;
    if (inited) return;
    inited = true;
    Registry().RegisterAstSink(&builtinAstSink);
    Registry().RegisterTextSink(&builtinTextSink);
    Registry().RegisterTransform("mutate-evolve", &mutateEvolve);
}

// ------------------------ Optimizer (constant fold, DCE, peephole) ------------------------

static bool isNum(Node* n) { return n && n->type == "Num"; }
static bool isStr(Node* n) { return n && n->type == "Str"; }

static bool toDouble(const std::string& s, double& out) {
    char* endp = nullptr;
    out = std::strtod(s.c_str(), &endp);
    return endp && *endp == '\0';
}
static Node* makeNum(double v) {
    std::ostringstream ss; ss << v; return new Node{ "Num", ss.str() };
}
static Node* clone(Node* n) {
    if (!n) return nullptr;
    Node* c = new Node{ n->type, n->value };
    for (auto* ch : n->children) c->children.push_back(clone(ch));
    return c;
}
static Node* fold(Node* n);

static Node* peephole(Node* n) {
    if (!n) return n;
    if (n->type == "BinOp" && n->children.size() == 2) {
        Node* L = n->children[0];
        Node* R = n->children[1];
        if (n->value == "+") {
            if (isNum(L) && L->value == "0") return clone(R);
            if (isNum(R) && R->value == "0") return clone(L);
        }
        if (n->value == "-" && isNum(R) && R->value == "0") return clone(L);
        if (n->value == "*") {
            if (isNum(L) && L->value == "1") return clone(R);
            if (isNum(R) && R->value == "1") return clone(L);
            if ((isNum(L) && L->value == "0") || (isNum(R) && R->value == "0")) return makeNum(0);
        }
        if (n->value == "/" && isNum(R) && R->value == "1") return clone(L);
    }
    return n;
}

static Node* foldBinop(Node* n) {
    if (!n || n->type != "BinOp" || n->children.size() < 2) return n;
    n->children[0] = fold(n->children[0]);
    n->children[1] = fold(n->children[1]);

    Node* L = n->children[0];
    Node* R = n->children[1];

    if (n->value == "+" && isStr(L) && isStr(R)) {
        return new Node{ "Str", L->value + R->value };
    }

    double a, b;
    if (isNum(L) && isNum(R) && toDouble(L->value, a) && toDouble(R->value, b)) {
        if (n->value == "+") return makeNum(a + b);
        if (n->value == "-") return makeNum(a - b);
        if (n->value == "*") return makeNum(a * b);
        if (n->value == "/") return makeNum(b == 0 ? 0 : (a / b));
        if (n->value == "<")  return makeNum((a < b) ? 1 : 0);
        if (n->value == ">")  return makeNum((a > b) ? 1 : 0);
        if (n->value == "<=") return makeNum((a <= b) ? 1 : 0);
        if (n->value == ">=") return makeNum((a >= b) ? 1 : 0);
        if (n->value == "==") return makeNum((a == b) ? 1 : 0);
        if (n->value == "!=") return makeNum((a != b) ? 1 : 0);
    }
    return peephole(n);
}

static Node* fold(Node* n) {
    if (!n) return n;
    if (n->type == "BinOp") return foldBinop(n);
    for (size_t i = 0; i < n->children.size(); ++i) n->children[i] = fold(n->children[i]);
    return n;
}

static void dce(Node* n) {
    if (!n) return;
    if (n->type == "Body") {
        std::vector<Node*> keep;
        bool seenRet = false;
        for (auto* ch : n->children) {
            if (seenRet) continue;
            keep.push_back(ch);
            if (ch->type == "Ret") seenRet = true;
        }
        n->children.swap(keep);
    }
    for (auto* ch : n->children) dce(ch);
}

static void optimize(Node*& root) {
    if (!root) return;
    // Constant folding
    root = fold(root);
    // Dead code elimination
    dce(root);
    // Peephole
    root = peephole(root);
    // Loop unrolling (if overlay @unroll)
    // ... (implement as in previous responses)
    // Vectorization pragmas
    // ... (emit pragmas)
}

// ------------------------ Emitter ------------------------

static std::string escapeCppString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        case '\r': out += "\\r"; break;
        default:   out.push_back(c); break;
        }
    }
    return out;
}

static std::string emitExpr(Node* e);

static std::string emitExpr(Node* e) {
    if (!e) return "0";
    if (e->type == "Num") return e->value;
    if (e->type == "Var") return e->value;
    if (e->type == "Bool") return (e->value == "true" ? "true" : "false");
    if (e->type == "Str") {
        return std::string("\"") + escapeCppString(e->value) + "\"";
    }
    if (e->type == "BinOp") {
        std::string lhs = emitExpr(e->children.size() > 0 ? e->children[0] : nullptr);
        std::string rhs = emitExpr(e->children.size() > 1 ? e->children[1] : nullptr);
        return "(" + lhs + " " + e->value + " " + rhs + ")";
    }
    // Legacy nodes support
    if (e->type == "ExprNum") return e->value;
    if (e->type == "ExprIdent") return e->value;
    if (e->type == "ExprStr") return std::string("\"") + escapeCppString(e->value) + "\"";
    return "/*expr*/0";
}

static void emitNode(Node* n, std::ostringstream& out);

static void emitChildren(const std::vector<Node*>& cs, std::ostringstream& out) {
    for (auto* c : cs) emitNode(c, out);
}

static void emitPrintChain(std::ostringstream& out, Node* expr) {
    if (!expr) { out << "std::cout << std::endl;\n"; return; }
    std::vector<Node*> parts;
    std::function<void(Node*)> flatten = [&](Node* e) {
        if (!e) return;
        if (e->type == "BinOp" && e->value == "+") { flatten(e->children[0]); flatten(e->children[1]); }
        else parts.push_back(e);
    };
    flatten(expr);
    out << "std::cout";
    if (parts.empty()) {
        out << " << " << emitExpr(expr) << " << std::endl;\n";
        return;
    }
    for (auto* p : parts) out << " << " << emitExpr(p);
    out << " << std::endl;\n";
}

static std::string toIosMode(const std::string& mode) {
    // modes: "out", "in", "app", "binary" (combine separated by '|': "out|app")
    if (mode.empty()) return "std::ios::out";
    std::string m = mode;
    // remove spaces
    m.erase(std::remove_if(m.begin(), m.end(), [](unsigned char ch) { return std::isspace(ch); }), m.end());
    std::ostringstream os;
    bool first = true;
    size_t start = 0;
    while (start < m.size()) {
        size_t p = m.find('|', start);
        std::string tok = m.substr(start, p == std::string::npos ? std::string::npos : (p - start));
        std::string part = "std::ios::out";
        if (tok == "out") part = "std::ios::out";
        else if (tok == "in") part = "std::ios::in";
        else if (tok == "app") part = "std::ios::app";
        else if (tok == "binary") part = "std::ios::binary";
        else part = "std::ios::out";
        if (!first) os << " | ";
        os << part;
        first = false;
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return os.str();
}

static std::string mkCheckpointLabel(const std::string& name) {
    std::string lab = "__cp_label_" + name;
    for (auto& c : lab) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') c = '_';
    return lab;
}

static void emitScheduler(std::ostringstream& out, Node* scheduleNode) {
    std::string pr = scheduleNode->value.empty() ? "0" : scheduleNode->value;
    out << "{ struct __Task{int pr; std::function<void()> fn;}; std::vector<__Task> __sched;\n";
    // Collect one block as one task; extendable to multiple
    out << "__sched.push_back(__Task{" << pr << ", [=](){\n";
    if (!scheduleNode->children.empty()) emitChildren(scheduleNode->children[0]->children, out);
    out << "}});\n";
    out << "std::sort(__sched.begin(), __sched.end(), [](const __Task&a,const __Task&b){return a.pr>b.pr;});\n";
    out << "for (auto& t: __sched) t.fn(); }\n";
}

static void emitPrelude(std::ostringstream& out) {
    out << "#include <iostream>\n";
    out << "#include <fstream>\n";
    out << "#include <cmath>\n";
    out << "#include <vector>\n";
    out << "#include <deque>\n";
    out << "#include <mutex>\n";
    out << "#include <condition_variable>\n";
    out << "#include <functional>\n";
    out << "#include <algorithm>\n";
    out << "#include <thread>\n";
    out << "#include <future>\n";
    out << "#include <map>\n";
    out << "#if defined(_OPENMP)\n#include <omp.h>\n#endif\n";
    // Simple channel template
    out << "template<typename T>\n";
    out << "struct Channel {\n";
    out << "  std::mutex m; std::condition_variable cv; std::deque<T> q;\n";
    out << "  void send(const T& v){ std::lock_guard<std::mutex> lk(m); q.push_back(v); cv.notify_one(); }\n";
    out << "  void recv(T& out){ std::unique_lock<std::mutex> lk(m); cv.wait(lk,[&]{return !q.empty();}); out = std::move(q.front()); q.pop_front(); }\n";
    out << "};\n\n";
}

static void emitNode(Node* n, std::ostringstream& out) {
    if (n->type == "Program") {
        emitPrelude(out);
        // Emit function definitions first
        for (auto* c : n->children)
            if (c->type == "Fn") emitNode(c, out);
        out << "int main(){\n";
        for (auto* c : n->children)
            if (c->type != "Fn") emitNode(c, out);
        out << "return 0;\n}\n";
    }
    else if (n->type == "Print") {
        if (!n->children.empty()) {
            emitPrintChain(out, n->children[0]);
        }
        else {
            out << "std::cout << \"" << escapeCppString(n->value) << "\" << std::endl;\n";
        }
    }
    else if (n->type == "Loop") {
        std::string header = n->value;
        out << "for(" << header << "){\n";
        if (!n->children.empty()) emitChildren(n->children[0]->children, out);
        out << "}\n";
    }
    else if (n->type == "While") {
        std::string cond = n->children[0]->children.empty() ? "true" : emitExpr(n->children[0]->children[0]);
        out << "while(" << cond << "){\n";
        if (n->children.size() > 1) emitChildren(n->children[1]->children, out);
        out << "}\n";
    }
    else if (n->type == "Break") {
        out << "break;\n";
    }
    else if (n->type == "Continue") {
        out << "continue;\n";
    }
    else if (n->type == "If") {
        std::string cond = "/* condition */";
        size_t bodyIdx = 0;
        if (!n->children.empty() && n->children[0]->type == "Cond") {
            cond = emitExpr(n->children[0]->children[0]);
            bodyIdx = 1;
        }
        out << "if(" << cond << "){\n";
        if (n->children.size() > bodyIdx) emitChildren(n->children[bodyIdx]->children, out);
        out << "}\n";
        if (n->children.size() > bodyIdx + 1) {
            out << "else{\n";
            emitChildren(n->children[bodyIdx + 1]->children, out);
            out << "}\n";
        }
    }
    else if (n->type == "Switch") {
        std::string cond = (n->children.empty() ? "0" : emitExpr(n->children[0]->children[0]));
        out << "switch(" << cond << "){\n";
        for (size_t i = 1; i < n->children.size(); ++i) {
            Node* c = n->children[i];
            if (c->type == "Case") {
                out << "case " << c->value << ":\n";
                if (!c->children.empty()) emitChildren(c->children[0]->children, out);
                out << "break;\n";
            }
            else if (c->type == "Default") {
                out << "default:\n";
                if (!c->children.empty()) emitChildren(c->children[0]->children, out);
            }
        }
        out << "}\n";
    }
    else if (n->type == "Fn") {
        std::vector<std::string> overlays;
        Node* body = nullptr;
        for (auto* ch : n->children) {
            if (ch->type == "Overlay") overlays.push_back(ch->value);
            else if (ch->type == "Body") body = ch;
        }
        out << "void " << n->value << "(){\n";
        for (const auto& ov : overlays) {
            out << "/* overlay: " << ov << " */\n";
        }
        if (body) emitChildren(body->children, out);
        out << "}\n";
    }
    else if (n->type == "Call") {
        if (n->children.empty()) {
            out << n->value << "();\n";
        }
        else {
            out << n->value << "(";
            for (size_t i = 0; i < n->children.size(); ++i) {
                if (i) out << ", ";
                out << emitExpr(n->children[i]);
            }
            out << ");\n";
        }
    }
    else if (n->type == "Open") {
        std::string var = n->value;
        std::string path = (!n->children.empty() ? n->children[0]->value : "");
        std::string mode = (n->children.size() > 1 ? n->children[1]->value : "out");
        out << "std::fstream " << var << "(" << "\"" << escapeCppString(path) << "\"" << ", " << toIosMode(mode) << ");\n";
    }
    else if (n->type == "Write" || n->type == "Writeln") {
        std::string var = n->value;
        if (!n->children.empty()) {
            out << var << " << " << emitExpr(n->children[0]) << ";\n";
        }
        if (n->type == "Writeln") out << var << " << std::endl;\n";
    }
    else if (n->type == "Read") {
        std::string var = n->value;
        std::string target = (!n->children.empty() ? n->children[0]->value : "");
        out << var << " >> " << target << ";\n";
    }
    else if (n->type == "Close") {
        out << n->value << ".close();\n";
    }
    else if (n->type == "Let") {
        if (n->children.empty()) {
            out << "/* invalid let */\n";
        }
        else {
            out << "auto " << n->value << " = " << emitExpr(n->children[0]) << ";\n";
        }
    }
    else if (n->type == "Ret") {
        if (n->children.empty()) out << "return;\n";
        else out << "return " << emitExpr(n->children[0]) << ";\n";
    }
    else if (n->type == "Mutate") {
        out << "/* mutation requested: " << n->value << " */\n";
    }
    else if (n->type == "Scale") {
        std::string x = n->children[0]->value;
        std::string a = emitExpr(n->children[1]);
        std::string b = emitExpr(n->children[2]);
        std::string c = emitExpr(n->children[3]);
        std::string d = emitExpr(n->children[4]);
        out << "{ auto __t = ((" << x << ") - (" << a << ")) / ((" << b << ") - (" << a << ")); "
            << x << " = (" << c << ") + __t * ((" << d << ") - (" << c << ")); }\n";
    }
    else if (n->type == "Bounds") {
        std::string x = n->children[0]->value;
        std::string mn = emitExpr(n->children[1]);
        std::string mx = emitExpr(n->children[2]);
        out << "if((" << x << ")<(" << mn << ")) " << x << "=(" << mn << ");\n";
        out << "if((" << x << ")>(" << mx << ")) " << x << "=(" << mx << ");\n";
    }
    else if (n->type == "Checkpoint") {
        out << mkCheckpointLabel(n->value) << ":\n";
    }
    else if (n->type == "VBreak") {
        out << "goto " << mkCheckpointLabel(n->value) << ";\n";
    }
    else if (n->type == "Channel") {
        std::string ty = n->children[0]->value;
        out << "Channel<" << ty << "> " << n->value << ";\n";
    }
    else if (n->type == "Send") {
        out << n->value << ".send(" << emitExpr(n->children[0]) << ");\n";
    }
    else if (n->type == "Recv") {
        out << n->value << ".recv(" << n->children[0]->value << ");\n";
    }
    else if (n->type == "Schedule") {
        emitScheduler(out, n);
    }
    else if (n->type == "Sync") {
#if defined(_OPENMP)
        out << "#pragma omp barrier\n";
#else
        out << "/* sync barrier (no-op) */\n";
#endif
    }
    else if (n->type == "Struct") {
        out << "struct {\n";
        for (auto* ch : n->children) {
            if (ch->type == "Field") {
                out << emitExpr(ch->children[0]) << " " << ch->value << ";\n";
            }
        }
        out << "};\n";
    }
    else if (n->type == "Array") {
        std::string ty = n->children[0]->value;
        std::string size = emitExpr(n->children[1]);
        out << ty << " " << n->value << "[" << size << "];\n";
    }
    else if (n->type == "List") {
        std::string ty = n->children[0]->value;
        out << "std::vector<" << ty << "> " << n->value;
        if (n->children.size() > 1) {
            out << " = {";
            for (size_t i = 1; i < n->children.size(); ++i) {
                if (i > 1) out << ", ";
                out << emitExpr(n->children[i]);
            }
            out << "}";
        }
        out << ";\n";
    }
    else if (n->type == "Dict") {
        std::string tys = n->children[0]->value;
        size_t colon = tys.find(':');
        std::string keyTy = tys.substr(0, colon);
        std::string valTy = tys.substr(colon + 1);
        out << "std::map<" << keyTy << ", " << valTy << "> " << n->value;
        if (n->children.size() > 1) {
            out << " = {";
            for (size_t i = 1; i < n->children.size(); ++i) {
                auto* pair = n->children[i];
                out << "{" << emitExpr(pair->children[0]) << ", " << emitExpr(pair->children[1]) << "}";
                if (i < n->children.size() - 1) out << ", ";
            }
            out << "}";
        }
        out << ";\n";
    }
    else if (n->type == "Thread") {
        out << "std::thread([]() {\n";
        emitChildren(n->children[0]->children, out);
        out << "}).detach();\n";
    }
    else if (n->type == "Async") {
        out << "std::async(std::launch::async, []() { return " << emitExpr(n->children[0]) << "; });\n";
    }
    else if (n->type == "Batch") {
        out << "#pragma omp parallel\n{\n";
        emitChildren(n->children[0]->children, out);
        out << "}\n";
    }
    else if (n->type == "Serialize") {
        out << "// Serialize " << emitExpr(n->children[0]) << " to " << emitExpr(n->children[1]) << "\n";
        out << "std::ofstream ser_file(" << emitExpr(n->children[1]) << ");\n";
        out << "ser_file << " << emitExpr(n->children[0]) << ";\n";
    }
    else if (n->type == "Compress") {
        out << "// Compress " << emitExpr(n->children[0]) << "\n";
        out << "auto compressed = " << emitExpr(n->children[0]) << "; // TODO: implement compression\n";
    }
    else if (n->type == "Obfuscate") {
        out << "// Obfuscate " << emitExpr(n->children.back()) << "\n";
        std::string key = n->children.size() > 1 ? emitExpr(n->children[0]) : "0xAA";
        out << "for (auto& c : " << emitExpr(n->children.back()) << ") c ^= " << key << ";\n";
    }
    else if (n->type == "Ping") {
        out << "system(\"ping " << (n->children.empty() ? "localhost" : emitExpr(n->children[0])) << "\");\n";
    }
    else if (n->type == "Derive") {
        out << "((" << emitExpr(n->children[0]) << "(" << emitExpr(n->children[1]) << " + 1e-6) - " << emitExpr(n->children[0]) << "(" << emitExpr(n->children[1]) << ")) / 1e-6)\n";
    }
    else if (n->type == "Chain") {
        for (size_t i = 0; i < n->children.size(); ++i) {
            out << emitExpr(n->children[i]);
            if (i < n->children.size() - 1) out << ";\n";
        }
        out << ";\n";
    }
    else {
        out << "/* Unknown: " << n->type << " " << n->value << " */\n";
    }
}

std::string emitCPP(Node* root) {
    std::ostringstream out;
    emitNode(root, out);
    return out.str();
}

// ------------------------ Driver ------------------------

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: transpiler <input.case>\n";
        return 1;
    }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) {
        std::cerr << "Failed to open input file: " << argv[1] << "\n";
        return 1;
    }
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    try {
        ciam::MacroRegistry::load();  // Load macros from previous builds

        ciam::Preprocessor ciamPre;
        src = ciamPre.Process(src);

        initPluginsOnce();

        auto tokens = tokenize(src);
        {
            std::ostringstream ts;
            ts << "tokens=" << tokens.size();
            emitEventText("tokens", ts.str());
        }

        Node* ast = parseProgram(tokens);
        emitEventAst("parsed", ast);

        collectOverlaysFlags(ast);

        std::vector<std::unordered_map<std::string, TypeKind>> scopes;
        analyze(ast, scopes);
        emitEventAst("analyzed", ast);

        runTransforms("before-opt", ast);

        optimize(ast);
        emitEventAst("optimized", ast);

        runTransforms("after-opt", ast);

        emitEventAst("before-emit", ast);
        std::string cpp = emitCPP(ast);
        emitEventText("emitted-cpp", cpp);

        std::ofstream out("compiler.cpp", std::ios::binary);
        if (!out) {
            std::cerr << "Failed to write compiler.cpp\n";
            return 1;
        }
        out << cpp;
        std::cout << "[OK] Generated compiler.cpp\n";

        if (gEnableReplay) {
            std::ofstream replay("replay.txt", std::ios::binary);
            if (replay) {
                for (auto& s : gReplay) {
                    replay << "=== " << s.phase << " ===\n";
                    replay << s.payload << "\n";
                }
                std::cout << "[OK] Wrote symbolic replay to replay.txt\n";
            } else {
                std::cerr << "[warn] failed to write replay.txt\n";
            }
        }
    }
    catch (const std::runtime_error& e) {
        std::cerr << "[DEEP ERROR] " << e.what() << "\n";
        if (std::string(e.what()).find("unterminated") != std::string::npos) {
            std::cerr << "[SUGGESTION] Add [end] to close block.\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    std::string clangCmd = "clang++ -std=c++14 -O3 compiler.cpp -o program.exe";
    if (system(clangCmd.c_str()) != 0) {
        std::cerr << "[LINK ERROR] Failed to link to .exe\n";
    }
}
