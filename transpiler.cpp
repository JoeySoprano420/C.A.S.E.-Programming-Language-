// Compiler.cpp - Single translation unit for a CASE Programming Language -> C++ transpiler (C++14)

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
#include <cstdlib>

#include "Intelligence.hpp" // CIAM preprocessor (write_stdout, overlays/inspect, sandbox/audit, base-12)
#include "MacroRegistry.hpp" // Macro registry integration

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
    // Single-char symbols sufficient for this DSL
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
    case '?':
    case '.':
        return true;
    default:
        return false;
    }
}

static void trimInPlace(std::string& s) {
    auto notspace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}

std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> tokens;
    tokens.reserve(src.size() / 2); // heuristic to reduce reallocations
    size_t i = 0;
    int line = 1;

    auto push = [&](TokenType t, const std::string& v) {
        tokens.push_back(Token{ t, v, line });
        };

    auto isKeyword = [](const std::string& s) {
        // Extended keywords for richer control flow, memory, I/O, overlays and systems
        return s == "Print" || s == "ret" || s == "loop" || s == "if" ||
            s == "else" || s == "Fn" || s == "call" || s == "let" ||
            s == "while" || s == "break" || s == "continue" ||
            s == "switch" || s == "case" || s == "default" ||
            s == "overlay" || s == "open" || s == "write" || s == "writeln" ||
            s == "read" || s == "close" || s == "mutate" ||
            s == "scale" || s == "bounds" || s == "checkpoint" || s == "vbreak" ||
            s == "channel" || s == "send" || s == "recv" || s == "sync" ||
            s == "schedule" || s == "input" || s == "true" || s == "false";
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

    auto readSymbol = [&](size_t& idx) {
        // Try to read multi-char operators first
        static const std::unordered_set<std::string> twoChar = {
            "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "++", "--", "->", "::", "<<", ">>"
        };
        char c = src[idx];
        std::string one(1, c);
        if (idx + 1 < src.size()) {
            std::string two = one + src[idx + 1];
            if (twoChar.count(two)) {
                idx += 2;
                return two;
            }
        }
        ++idx;
        return one;
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

        // Line comments: // ... or # ...
        if ((c == '/' && i + 1 < src.size() && src[i + 1] == '/') || c == '#') {
            if (c == '/') i += 2; else ++i;
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

        // Symbols (support multi-char first)
        if (isSymbolChar(c)) {
            std::string sym = readSymbol(i);
            push(TokenType::SYMBOL, sym);
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
    std::string type;           // e.g., Program, Print, Let, Fn, While, Scale, Bounds, Checkpoint, VBreak, Channel, Send, Recv, Schedule, Sync, Member, Index, Invoke, Ternary
    std::string value;          // payload (e.g., name, operator, string literal, loop header, label, priority, function name or member name)
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

static void skipOptionalSemicolon() {
    if (checkValue(";")) advanceTok();
}

static Node* parseStatement(); // fwd
static Node* parseExpression(); // fwd
static Node* parseUnary(); // fwd

// Overlay directive buffer (applies to next Fn)
static std::vector<std::string> gPendingOverlays;

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

// Delimited blocks: support both {...} and (...)
static Node* parseDelimitedBlock(const std::string& open, const std::string& close) {
    if (!matchValue(open)) throw std::runtime_error("Expected '" + open + "' to start a block at line " + std::to_string(peek().line));
    Node* body = new Node{ "Body", "" };
    while (peek().type != TokenType::END && !checkValue(close)) {
        body->children.push_back(parseStatement());
    }
    if (!matchValue(close)) throw std::runtime_error("Expected '" + close + "' to close a block at line " + std::to_string(peek().line));
    return body;
}

static Node* parseBlock() { return parseDelimitedBlock("{", "}"); }
static Node* parseParenBlock() { return parseDelimitedBlock("(", ")"); }

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
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseRet() {
    advanceTok(); // 'ret'
    Node* n = new Node{ "Ret", "" };
    if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}") && !checkValue("]")) {
        Node* expr = parseExpression();
        if (expr) n->children.push_back(expr);
    }
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// Shared: parse expression argument list inside '(' ... ')'
static void parseArgList(std::vector<Node*>& args) {
    if (!matchValue("(")) throw std::runtime_error("Expected '(' starting arguments at line " + std::to_string(peek().line));
    if (matchValue(")")) return; // empty
    while (true) {
        Node* e = parseExpression();
        if (e) args.push_back(e);
        if (matchValue(")")) break;
        if (matchValue(",")) continue;
        // tolerate whitespace-separated args without comma
        if (checkValue("]") || checkValue("{") || checkValue("}") || checkType(TokenType::END)) break;
    }
}

// Statement-form call
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
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// Expression-form call: `call name args...`
static Node* parseCallExprFromKeyword() {
    advanceTok(); // 'call'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected function name after 'call' at line " + std::to_string(peek().line));
    std::string fname = advanceTok().value;
    std::vector<Node*> args;
    while (peek().type != TokenType::END && !checkValue("[") && !checkValue("{") && !checkValue("}")) {
        if (checkValue(",")) { advanceTok(); continue; }
        Node* e = parseExpression();
        if (e) args.push_back(e);
        if (checkValue(",")) { advanceTok(); continue; }
        if (checkValue("{") || checkValue("}") || checkValue("[")) break;
    }
    Node* c = new Node{ "CallExpr", fname };
    for (auto* a : args) c->children.push_back(a);
    skipBracketBlockIfPresent();
    return c;
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
    skipOptionalSemicolon();
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

static Node* parseBreak() { advanceTok(); skipOptionalSemicolon(); return new Node{ "Break","" }; }
static Node* parseContinue() { advanceTok(); skipOptionalSemicolon(); return new Node{ "Continue","" }; }

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
        gPendingOverlays.push_back(advanceTok().value);
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
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseClose() {
    advanceTok(); // 'close'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected stream variable after 'close' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Close", advanceTok().value };
    skipOptionalSemicolon();
    return n;
}

// mutate (compiler-introspection hint)
static Node* parseMutate() {
    advanceTok(); // 'mutate'
    Node* n = new Node{ "Mutate","" };
    if (peek().type == TokenType::IDENT) n->value = advanceTok().value;
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// checkpoint label
static Node* parseCheckpoint() {
    advanceTok(); // 'checkpoint'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected label after 'checkpoint' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Checkpoint", advanceTok().value };
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// vbreak label
static Node* parseVBreak() {
    advanceTok(); // 'vbreak'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected label after 'vbreak' at line " + std::to_string(peek().line));
    Node* n = new Node{ "VBreak", advanceTok().value };
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// schedule priority { body }
static Node* parseSchedule() {
    advanceTok(); // 'schedule'
    Node* n = new Node{ "Schedule","" };
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
    skipOptionalSemicolon();
    return new Node{ "Sync","" };
}

static std::vector<std::pair<std::string, std::string>> parseParamListFromStringFragments(const std::vector<std::string>& frags) {
    // Join all fragments with ',' then split by ','
    std::string joined;
    for (size_t i = 0; i < frags.size(); ++i) {
        if (i) joined += ",";
        joined += frags[i];
    }
    std::vector<std::pair<std::string, std::string>> params;
    size_t start = 0;
    while (start < joined.size()) {
        size_t p = joined.find(',', start);
        std::string part = joined.substr(start, p == std::string::npos ? std::string::npos : (p - start));
        trimInPlace(part);
        if (!part.empty()) {
            size_t sp = part.find_last_of(" \t");
            if (sp != std::string::npos) {
                std::string ty = part.substr(0, sp);
                std::string nm = part.substr(sp + 1);
                trimInPlace(ty); trimInPlace(nm);
                if (!nm.empty()) params.emplace_back(ty, nm);
            }
            else {
                params.emplace_back(std::string("auto"), part);
            }
        }
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return params;
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

    // Optional parameters: () or one/more STRING fragments
    std::vector<std::string> paramStrFrags;
    bool sawParenParams = false;
    if (checkValue("(")) {
        int depth = 0;
        if (matchValue("(")) {
            ++depth;
            while (peek().type != TokenType::END && depth > 0) {
                if (matchValue("(")) { ++depth; continue; }
                if (matchValue(")")) { --depth; continue; }
                advanceTok(); // ignore inside
            }
            sawParenParams = true;
        }
    }
    else if (peek().type == TokenType::STRING) {
        paramStrFrags.push_back(advanceTok().value);
        while (matchValue(",")) {
            if (peek().type == TokenType::STRING) paramStrFrags.push_back(advanceTok().value);
            else break;
        }
    }

    if (sawParenParams || !paramStrFrags.empty()) {
        Node* params = new Node{ "Params","" };
        auto pairs = parseParamListFromStringFragments(paramStrFrags);
        for (auto& pr : pairs) {
            Node* p = new Node{ "Param", pr.second }; // value=name
            Node* ty = new Node{ "Type", pr.first };
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        n->children.push_back(params);
    }

    // Attach pending overlays as children (Overlay nodes)
    for (const auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov };
        n->children.push_back(o);
    }
    gPendingOverlays.clear();

    // Function body can be { ... } or ( ... )
    Node* body = nullptr;
    if (checkValue("{")) body = parseBlock();
    else if (checkValue("(")) body = parseParenBlock();
    else throw std::runtime_error("Expected '{' or '(' to start function body at line " + std::to_string(peek().line));

    n->children.push_back(body);
    skipBracketBlockIfPresent();
    return n;
}

// ----- Expression parsing (precedence) -----
//  || : 1, && : 2, == != : 3, < > <= >= : 4, + - : 5, * / % : 6, ?: ternary

static int precedenceOf(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "==" || op == "!=") return 3;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
    if (op == "+" || op == "-") return 5;
    if (op == "*" || op == "/" || op == "%") return 6;
    return -1;
}

// Parse an lvalue with postfix chains: var, var[idx], var.member, nested combinations
static Node* parseLValue() {
    if (peek().type != TokenType::IDENT) return nullptr;
    Node* base = new Node{ "Var", advanceTok().value };
    while (true) {
        if (checkValue("[")) {
            advanceTok();
            Node* idx = parseExpression();
            if (!matchValue("]")) throw std::runtime_error("Expected ']' in index expression at line " + std::to_string(peek().line));
            Node* idxNode = new Node{ "Index","" };
            idxNode->children.push_back(base);
            idxNode->children.push_back(idx);
            base = idxNode;
            continue;
        }
        if (checkValue(".")) {
            advanceTok();
            if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected member name after '.' at line " + std::to_string(peek().line));
            std::string member = advanceTok().value;
            Node* memNode = new Node{ "Member", member };
            memNode->children.push_back(base);
            base = memNode;
            continue;
        }
        break;
    }
    return base;
}

static Node* parsePrimary() {
    if (peek().type == TokenType::KEYWORD && peek().value == "call") {
        return parseCallExprFromKeyword();
    }
    if (peek().type == TokenType::NUMBER) {
        return new Node{ "Num", advanceTok().value };
    }
    if (peek().type == TokenType::STRING) {
        return new Node{ "Str", advanceTok().value };
    }
    if (peek().type == TokenType::KEYWORD && (peek().value == "true" || peek().value == "false")) {
        std::string v = advanceTok().value == "true" ? "1" : "0";
        return new Node{ "Num", v };
    }
    if (peek().type == TokenType::IDENT) {
        Node* acc = new Node{ "Var", advanceTok().value };
        // Postfix chains: calls, indexes, members
        while (true) {
            if (checkValue("(")) {
                std::vector<Node*> args;
                parseArgList(args);
                Node* inv = new Node{ "Invoke","" };
                inv->children.push_back(acc); // target expression
                for (auto* a : args) inv->children.push_back(a);
                acc = inv;
                continue;
            }
            if (checkValue("[")) {
                advanceTok();
                Node* idx = parseExpression();
                if (!matchValue("]")) throw std::runtime_error("Expected ']' in index at line " + std::to_string(peek().line));
                Node* idxNode = new Node{ "Index","" };
                idxNode->children.push_back(acc);
                idxNode->children.push_back(idx);
                acc = idxNode;
                continue;
            }
            if (checkValue(".")) {
                advanceTok();
                if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected member after '.' at line " + std::to_string(peek().line));
                std::string member = advanceTok().value;
                Node* mem = new Node{ "Member", member };
                mem->children.push_back(acc);
                acc = mem;
                continue;
            }
            break;
        }
        return acc;
    }
    if (checkValue("(")) {
        advanceTok(); // (
        Node* inner = parseExpression();
        if (!matchValue(")")) throw std::runtime_error("Expected ')' in expression at line " + std::to_string(peek().line));
        return inner;
    }
    throw std::runtime_error("Expected expression at line " + std::to_string(peek().line));
}

static Node* parseUnary() {
    if (checkValue("!") || checkValue("-")) {
        std::string op = advanceTok().value;
        Node* rhs = parseUnary();
        Node* u = new Node{ "Unary", op };
        u->children.push_back(rhs);
        return u;
    }
    return parsePrimary();
}

static Node* parseBinOpRHS(int minPrec, Node* lhs) {
    while (true) {
        const Token& t = peek();
        if (t.type != TokenType::SYMBOL) break;
        std::string op = t.value;
        int prec = precedenceOf(op);
        if (prec < minPrec) break;

        advanceTok(); // consume operator
        Node* rhs = parseUnary();
        int nextPrec = precedenceOf(peek().value);
        if (nextPrec > prec) {
            rhs = parseBinOpRHS(prec + 1, rhs);
        }

        Node* bin = new Node{ "BinOp", op };
        bin->children.push_back(lhs);
        bin->children.push_back(rhs);
        lhs = bin;
    }
    return lhs;
}

static Node* parseExpression() {
    Node* lhs = parseUnary();
    lhs = parseBinOpRHS(0, lhs);
    // Ternary ?: (lowest precedence, right-associative)
    if (matchValue("?")) {
        Node* thenE = parseExpression();
        if (!matchValue(":")) throw std::runtime_error("Expected ':' in ternary at line " + std::to_string(peek().line));
        Node* elseE = parseExpression();
        Node* t = new Node{ "Ternary","" };
        t->children.push_back(lhs);
        t->children.push_back(thenE);
        t->children.push_back(elseE);
        return t;
    }
    return lhs;
}

static Node* parseInput() {
    advanceTok(); // 'input'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected variable after 'input' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Input", advanceTok().value };
    skipOptionalSemicolon();
    return n;
}

static bool isAssignOp(const std::string& s) {
    return s == "=" || s == "+=" || s == "-=" || s == "*=" || s == "/=" || s == "%=" || s == "++" || s == "--";
}

static Node* parseAssignmentOrIncDec() {
    // Prefix ++/--
    if (peek().type == TokenType::SYMBOL && (checkValue("++") || checkValue("--"))) {
        std::string op = advanceTok().value;
        Node* lv = parseLValue();
        if (!lv) throw std::runtime_error("Expected lvalue after '" + op + "' at line " + std::to_string(peek().line));
        Node* n = new Node{ "Assign", op };
        n->children.push_back(lv);
        skipOptionalSemicolon();
        return n;
    }
    // lvalue (op) [expr];
    if (peek().type == TokenType::IDENT) {
        size_t save = pos;
        Node* lv = parseLValue();
        if (!lv) return nullptr;
        if (peek().type == TokenType::SYMBOL && isAssignOp(peek().value)) {
            std::string op = advanceTok().value; // assignment operator
            Node* n = new Node{ "Assign", op };
            n->children.push_back(lv);
            if (op != "++" && op != "--") {
                Node* expr = parseExpression();
                n->children.push_back(expr);
            }
            skipOptionalSemicolon();
            return n;
        }
        // Postfix ++/-- only for simple identifier (fallback)
        pos = save; // rollback if not assignment
        if (peek().type == TokenType::IDENT) {
            std::string name = advanceTok().value;
            if (peek().type == TokenType::SYMBOL && (checkValue("++") || checkValue("--"))) {
                std::string op = advanceTok().value;
                Node* n = new Node{ "Assign", op };
                Node* var = new Node{ "Var", name };
                n->children.push_back(var);
                skipOptionalSemicolon();
                return n;
            }
            // rollback complete
            pos = save;
        }
    }
    return nullptr;
}

static Node* parseStatement() {
    // Assignment and inc/dec fast-path
    if (peek().type == TokenType::SYMBOL || peek().type == TokenType::IDENT) {
        Node* asn = parseAssignmentOrIncDec();
        if (asn) return asn;
    }

    const std::string v = peek().value;
    if (v == "Print")    return parsePrint();
    if (v == "ret")      return parseRet();
    if (v == "loop")     return parseLoop();
    if (v == "if")       return parseIf();
    if (v == "while")    return parseWhile();
    if (v == "break")    return parseBreak();
    if (v == "continue") return parseContinue();
    if (v == "switch")   return parseSwitch();
    if (v == "Fn")       return parseFn();
    if (v == "call")     return parseCall();
    if (v == "let")      return parseLet();
    if (v == "overlay")  return parseOverlay();
    if (v == "open")     return parseOpen();
    if (v == "write")    return parseWriteLike("Write");
    if (v == "writeln")  return parseWriteLike("Writeln");
    if (v == "read")     return parseRead();
    if (v == "close")    return parseClose();
    if (v == "mutate")   return parseMutate();
    if (v == "scale")    return parseScale();
    if (v == "bounds")   return parseBounds();
    if (v == "checkpoint") return parseCheckpoint();
    if (v == "vbreak")   return parseVBreak();
    if (v == "channel")  return parseChannel();
    if (v == "send")     return parseSend();
    if (v == "recv")     return parseRecv();
    if (v == "schedule") return parseSchedule();
    if (v == "sync")     return parseSync();
    if (v == "input")    return parseInput();

    // Fallback: treat as unknown token - consume and produce placeholder node
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

enum class TypeKind { Unknown, Number, String };

static TypeKind inferExpr(Node* e, const std::vector<std::unordered_map<std::string, TypeKind>>& scopes) {
    if (!e) return TypeKind::Unknown;
    if (e->type == "Num") return TypeKind::Number;
    if (e->type == "Str") return TypeKind::String;
    if (e->type == "Var") {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto f = it->find(e->value);
            if (f != it->end()) return f->second;
        }
        return TypeKind::Unknown;
    }
    if (e->type == "Unary") {
        return TypeKind::Number;
    }
    if (e->type == "CallExpr" || e->type == "Invoke") {
        return TypeKind::Number;
    }
    if (e->type == "Member" || e->type == "Index") {
        return TypeKind::Unknown;
    }
    if (e->type == "Ternary") {
        TypeKind c1 = inferExpr(e->children[1], scopes);
        TypeKind c2 = inferExpr(e->children[2], scopes);
        if (c1 == TypeKind::String || c2 == TypeKind::String) return TypeKind::String;
        if (c1 == TypeKind::Number || c2 == TypeKind::Number) return TypeKind::Number;
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
    if (n->type == "Fn") {
        scopes.push_back({});
        // Seed parameters
        for (auto* ch : n->children) {
            if (ch->type == "Params") {
                for (auto* p : ch->children) {
                    std::string name = p->value;
                    std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                    TypeKind tk = TypeKind::Number;
                    if (ty.find("string") != std::string::npos) tk = TypeKind::String;
                    scopes.back()[name] = tk;
                }
            }
        }
        for (auto* ch : n->children) if (ch->type == "Body") analyze(ch, scopes);
        scopes.pop_back();
        return;
    }
    if (n->type == "If" || n->type == "While" || n->type == "Switch" || n->type == "Schedule") {
        for (auto* c : n->children) analyze(c, scopes);
        return;
    }
    if (n->type == "Let") {
        TypeKind t = inferExpr(n->children.empty() ? nullptr : n->children[0], scopes);
        if (scopes.empty()) scopes.push_back({});
        scopes.back()[n->value] = t;
    }
    if (n->type == "Assign") {
        // generic lvalue; conservatively mark number when simple var
        if (scopes.empty()) scopes.push_back({});
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            if (n->value == "++" || n->value == "--") {
                scopes.back()[lhs->value] = TypeKind::Number;
            }
            else {
                TypeKind t = inferExpr(n->children.size() > 1 ? n->children[1] : nullptr, scopes);
                scopes.back()[lhs->value] = t;
            }
        }
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

// Snapshot buffer for symbolic replay
struct Snapshot {
    std::string phase;
    std::string payload; // text or AST dump
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

// Built-in Introspection sink: no-op (extend to log externally if needed)
static void builtinAstSink(const char* /*phase*/, const Node* /*root*/) {
}

static void builtinTextSink(const char* /*phase*/, const char* /*text*/, size_t /*len*/) {
}

// Forward declaration for internal optimizer (defined later in this file)
static void optimize(Node*& root);

// Built-in mutation: evolutionary re-optimization (re-visit and re-run optimize a few rounds)
static bool mutateEvolve(Node*& root, const char* passPoint) {
    if (std::string(passPoint) == "before-opt" || std::string(passPoint) == "after-opt") {
        optimize(root);
        optimize(root);
        return true;
    }
    return false;
}

// Collects overlays to enable features (mutate/inspect/replay)
static void collectOverlaysFlags(Node* n) {
    if (!n) return;
    if (n->type == "Overlay") {
        if (n->value == "mutate") gEnableMutate = true;
        if (n->value == "inspect") gEnableInspect = true;
        if (n->value == "replay") gEnableReplay = true;
        if (n->value == "audit") gEnableInspect = true;

        // Register overlay-driven macros
        ciam::emitMacroFromOverlay(n->value);
    }
    if (n->type == "Mutate") gEnableMutate = true;
    for (auto* c : n->children) collectOverlaysFlags(c);
}

static void initPluginsOnce() {
    static bool inited = false;
    if (inited) return;
    inited = true;
    // Register built-ins
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
        if (n->value == "%") return makeNum(static_cast<long long>(a) % static_cast<long long>(b));
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
    if (n->type == "Ternary") {
        n->children[0] = fold(n->children[0]);
        n->children[1] = fold(n->children[1]);
        n->children[2] = fold(n->children[2]);
        // if cond is number, pick a branch
        double c; if (isNum(n->children[0]) && toDouble(n->children[0]->value, c)) {
            return clone(c != 0 ? n->children[1] : n->children[2]);
        }
        return n;
    }
    if (n->type == "BinOp") return foldBinop(n);
    for (size_t i = 0; i < n->children.size(); ++i) n->children[i] = fold(n->children[i]);
    return n;
}

static void dce(Node* n) {
    if (!n) return;
    if (n->type == "Body") {
        std::vector<Node*> keep;
        bool seenRet = false;
        keep.reserve(n->children.size());
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
    root = fold(root);
    dce(root);
    root = fold(root);
    dce(root);
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
    if (e->type == "Str") {
        return std::string("\"") + escapeCppString(e->value) + "\"";
    }
    if (e->type == "Unary") {
        std::string rhs = emitExpr(e->children.empty() ? nullptr : e->children[0]);
        return "(" + e->value + rhs + ")";
    }
    if (e->type == "Member") {
        // children[0] = base, value=member
        return "(" + emitExpr(e->children[0]) + "." + e->value + ")";
    }
    if (e->type == "Index") {
        // children[0] = base, children[1] = idx
        return "(" + emitExpr(e->children[0]) + "[" + emitExpr(e->children[1]) + "]" + ")";
    }
    if (e->type == "Invoke") {
        // children[0] = target expression, then args...
        std::ostringstream os;
        os << "(" << emitExpr(e->children[0]) << "(";
        for (size_t i = 1; i < e->children.size(); i++) {
            if (i > 1) os << ", ";
            os << emitExpr(e->children[i]);
        }
        os << "))";
        return os.str();
    }
    if (e->type == "CallExpr") {
        std::ostringstream os;
        os << e->value << "(";
        for (size_t i = 0; i < e->children.size(); ++i) {
            if (i) os << ", ";
            os << emitExpr(e->children[i]);
        }
        os << ")";
        return os.str();
    }
    if (e->type == "Ternary") {
        return "(" + emitExpr(e->children[0]) + " ? " + emitExpr(e->children[1]) + " : " + emitExpr(e->children[2]) + ")";
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
    // Stream-print expression; flatten '+' into stream chain
    if (!expr) { out << "std::cout << std::endl;\n"; return; }
    std::vector<Node*> parts;
    parts.reserve(8);
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
    if (mode.empty()) return "std::ios::out";
    std::string m = mode;
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

static void processLoopHeaderHints(const std::string& raw, std::string& pragmas, std::string& cleaned) {
    cleaned = raw;
    pragmas.clear();
    auto erase_all = [&](const std::string& pat) {
        size_t pos = 0;
        while ((pos = cleaned.find(pat, pos)) != std::string::npos) {
            cleaned.erase(pos, pat.size());
        }
        };
    // @omp
    if (cleaned.find("@omp") != std::string::npos) {
        pragmas += "#if defined(_OPENMP)\n#pragma omp parallel for\n#endif\n";
        erase_all("@omp");
    }
    // @vectorize
    if (cleaned.find("@vectorize") != std::string::npos) {
        pragmas += "#ifdef __clang__\n#pragma clang loop vectorize(enable)\n#endif\n";
        pragmas += "#ifdef __GNUC__\n#pragma GCC ivdep\n#endif\n";
        pragmas += "#ifdef _MSC_VER\n#pragma loop(ivdep)\n#endif\n";
        erase_all("@vectorize");
    }
    // @unroll(N)
    size_t up = cleaned.find("@unroll(");
    if (up != std::string::npos) {
        size_t lp = cleaned.find("(", up);
        size_t rp = cleaned.find(")", lp);
        if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
            std::string n = cleaned.substr(lp + 1, rp - lp - 1);
            trimInPlace(n);
            pragmas += "#pragma unroll " + n + "\n";
            cleaned.erase(up, (rp - up) + 1);
        }
        else {
            erase_all("@unroll");
        }
    }
    trimInPlace(cleaned);
}

static std::string mapTypeToCpp(const std::string& ty) {
    std::string low = ty;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (low == "int") return "int";
    if (low == "double") return "double";
    if (low == "float") return "float";
    if (low == "bool") return "bool";
    if (low.find("string") != std::string::npos) return "std::string";
    if (low == "auto") return "auto";
    return "auto";
}

static void emitScheduler(std::ostringstream& out, Node* scheduleNode) {
    std::string pr = scheduleNode->value.empty() ? "0" : scheduleNode->value;
    out << "{ struct __Task{int pr; std::function<void()> fn;}; std::vector<__Task> __sched;\n";
    out << "__sched.push_back(__Task{" << pr << ", [=](){\n";
    if (!scheduleNode->children.empty()) emitChildren(scheduleNode->children[0]->children, out);
    out << "}});\n";
    out << "std::sort(__sched.begin(), __sched.end(), [](const __Task&a,const __Task&b){return a.pr>b.pr;});\n";
    out << "for (auto& t: __sched) t.fn(); }\n";
}

static void emitPrelude(std::ostringstream& out) {
    // Metadata banner (JSON) embedded as comment + global string
    const std::string meta = getMetadataJson();
    out << "/* CASE metadata: " << meta << " */\n";
    out << "static const char* __CASE_METADATA = R\"(" << meta << ")\";\n";

    out << "#include <iostream>\n";
    out << "#include <fstream>\n";
    out << "#include <cmath>\n";
    out << "#include <vector>\n";
    out << "#include <deque>\n";
    out << "#include <mutex>\n";
    out << "#include <condition_variable>\n";
    out << "#include <functional>\n";
    out << "#include <algorithm>\n";
    out << "#if defined(_OPENMP)\n#include <omp.h>\n#endif\n";
    // Simple channel template
    out << "template<typename T>\n";
    out << "struct Channel {\n";
    out << "  std::mutex m; std::condition_variable cv; std::deque<T> q;\n";
    out << "  void send(const T& v){ std::lock_guard<std::mutex> lk(m); q.push_back(v); cv.notify_one(); }\n";
    out << "  void recv(T& out){ std::unique_lock<std::mutex> lk(m); cv.wait(lk,[&]{return !q.empty();}); out = std::move(q.front()); q.pop_front(); }\n";
    out << "};\n\n";
}

static ProcResult runProcessCapture(const std::string& cmd) {
#if defined(_WIN32)
    std::string full = cmd + " 2>&1";
    FILE* pipe = _popen(full.c_str(), "r");
    if (!pipe) return { -1, "failed to start: " + cmd };
    std::string out;
    char buf[4096];
    while (size_t n = std::fread(buf, 1, sizeof(buf), pipe)) out.append(buf, n);
    int rc = _pclose(pipe);
    return { rc, out };
#else
    std::string full = cmd + " 2>&1";
    FILE* pipe = popen(full.c_str(), "r");
    if (!pipe) return { -1, "failed to start: " + cmd };
    std::string out;
    char buf[4096];
    while (size_t n = std::fread(buf, 1, sizeof(buf), pipe)) out.append(buf, n);
    int rc = pclose(pipe);
    return { rc, out };
#endif
}

static bool tryNativeBuild(const std::string& outExe, const BuildConfig& cfg, const std::string& sourceCpp) {
    const char* noCompile = std::getenv("CASEC_NO_COMPILE");
    if (noCompile && std::string(noCompile) == "1") return true;

    auto fmtStd = [&]()->std::string {
        if (cfg.stdver == "c++17") return "-std=c++17";
        if (cfg.stdver == "c++20") return "-std=c++20";
        return "-std=c++14";
    };

#if defined(_WIN32)
    // Prefer clang-cl, allow override via cfg.cc
    std::string cc = cfg.cc.empty() ? "clang-cl" : cfg.cc;
    ProcResult pr{0,""};
    if (cc == "clang-cl") {
        std::ostringstream clir;
        clir << "clang-cl /EHsc /DNDEBUG /openmp "
             << "/O" << (cfg.opt.empty() ? "2" : cfg.opt.substr(1)) << " "
             << "/std:" << (cfg.stdver.empty() ? "c++14" : cfg.stdver) << " "
             << quote(sourceCpp) << " /link /OUT:" << quote(outExe);
        pr = runProcessCapture(clir.str());
        if (pr.exitCode != 0) {
            // Fallback to clang++
            std::ostringstream cc2;
            cc2 << "clang++ " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
                << " -fopenmp -DNDEBUG " << quote(sourceCpp) << " -o " << quote(outExe);
            pr = runProcessCapture(cc2.str());
        }
    } else if (cc == "cl") {
        std::ostringstream clir;
        clir << "cl /EHsc /nologo /DNDEBUG /openmp "
             << "/O" << (cfg.opt.empty() ? "2" : cfg.opt.substr(1)) << " "
             << "/std:" << (cfg.stdver.empty() ? "c++14" : cfg.stdver) << " "
             << quote(sourceCpp) << " /Fe" << quote(outExe);
        pr = runProcessCapture(clir.str());
    } else {
        // Use provided cc directly
        std::ostringstream any;
        any << cc << " " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
            << " -fopenmp -DNDEBUG " << quote(sourceCpp) << " -o " << quote(outExe);
        pr = runProcessCapture(any.str());
    }
    std::cout << pr.output;
    return pr.exitCode == 0;
#else
    std::string cc = cfg.cc.empty() ? "clang++" : cfg.cc;
    std::ostringstream cmd;
    cmd << cc << " " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
        << " -flto -march=native -fopenmp -DNDEBUG "
        << quote(sourceCpp) << " -o " << quote(outExe);
    ProcResult pr = runProcessCapture(cmd.str());
    std::cout << pr.output;
    return pr.exitCode == 0;
#endif
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: transpiler <input.case> [-o out_exe] [--std=c++14|c++17|c++20] [--opt=O0|O1|O2|O3] [--cc=<compiler>] [--tag key=value]\n";
        return 1;
    }
    std::string outExe = "program.exe";
#if !defined(_WIN32)
    outExe = "program.out";
#endif
    gBuild.stdver = "c++14";
    gBuild.opt = "O2";
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            outExe = argv[++i];
        } else if (a.rfind("--std=", 0) == 0) {
            gBuild.stdver = a.substr(6);
        } else if (a.rfind("--opt=", 0) == 0) {
            gBuild.opt = a.substr(6);
        } else if (a.rfind("--cc=", 0) == 0) {
            gBuild.cc = a.substr(5);
        } else if (a == "--tag" && i + 1 < argc) {
            std::string kv = argv[++i];
            size_t eq = kv.find('=');
            if (eq != std::string::npos) setMeta(kv.substr(0, eq), kv.substr(eq + 1));
        }
    }

    gSourcePath = argv[1]; // used for #line mapping
    setMeta("source", gSourcePath);
    setMeta("std", gBuild.stdver);
    setMeta("opt", gBuild.opt);

    std::ifstream f(argv[1], std::ios::binary);
    if (!f) {
        std::cerr << "Failed to open input file: " << argv[1] << "\n";
        return 1;
    }
    std::string src((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    try {
        ciam::MacroRegistry::load();

        ciam::Preprocessor ciamPre;
        src = ciamPre.Process(src);

        initPluginsOnce();

        // Lex
        auto tokens = tokenize(src);
        {
            std::ostringstream ts;
            ts << "tokens=" << tokens.size();
            emitEventText("tokens", ts.str());
        }

        // Parse
        Node* ast = parseProgram(tokens);
        hookInspect("parsed", ast);

        // Collect overlays (and emit overlay macros)
        collectOverlaysFlags(ast);

        // Basic analyze
        std::vector<std::unordered_map<std::string, TypeKind>> scopes;
        analyze(ast, scopes);
        hookInspect("analyzed", ast);

        // Allow mutators to run early (instrumentation, rewrites)
        hookMutate("pre-opt", ast);

        // Optimize
        optimize(ast);
        hookInspect("optimized", ast);

        // Allow mutators post-opt (final shaping before emit)
        hookMutate("post-opt", ast);

        // Emit C++
        hookInspect("before-emit", ast);
        std::string cpp = emitCPP(ast);
        emitEventText("emitted-cpp", cpp);

        std::ofstream out("compiler.cpp", std::ios::binary);
        if (!out) {
            std::cerr << "Failed to write compiler.cpp\n";
            return 1;
        }
        out << cpp;
        out.close();
        std::cout << "[OK] Generated compiler.cpp\n";
        hookInspect("after-emit", ast);

        ciam::MacroRegistry::persist();

        // Compile native with desired flags, capture diagnostics
        if (!tryNativeBuild(outExe, gBuild, "compiler.cpp")) {
            std::cerr << "[warn] native build failed\n";
        }

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
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}


// [ADDED] Stronger type system + overlay-driven semantic constraints

// ---- Type system extensions ----
enum class TypeKindEx { Unknown, Number, String, Boolean };

static TypeKindEx mapDeclaredTypeToKindEx(const std::string& ty) {
    std::string low = ty;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (low == "int" || low == "double" || low == "float" || low == "number" || low == "auto") return TypeKindEx::Number;
    if (low == "bool" || low == "boolean") return TypeKindEx::Boolean;
    if (low.find("string") != std::string::npos) return TypeKindEx::String;
    return TypeKindEx::Unknown;
}

static TypeKindEx inferExprStrong(Node* e,
    const std::vector<std::unordered_map<std::string, TypeKindEx>>& scopes) {
    if (!e) return TypeKindEx::Unknown;
    if (e->type == "Num") return TypeKindEx::Number;
    if (e->type == "Str") return TypeKindEx::String;
    if (e->type == "Unary") return TypeKindEx::Number; // treat as numeric context
    if (e->type == "Ternary") {
        TypeKindEx a = inferExprStrong(e->children[1], scopes);
        TypeKindEx b = inferExprStrong(e->children[2], scopes);
        if (a == TypeKindEx::String || b == TypeKindEx::String) return TypeKindEx::String;
        if (a == TypeKindEx::Number || b == TypeKindEx::Number) return TypeKindEx::Number;
        return TypeKindEx::Unknown;
    }
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], scopes);
        TypeKindEx R = inferExprStrong(e->children[1], scopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Function model + overlay constraints ----
struct FunctionInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKindEx>> params; // {name, kind}
    bool pure = false;                                       // overlay: pure
    std::unordered_set<std::string> nonnegParams;            // overlay: nonneg_<param>
};

static std::unordered_map<std::string, FunctionInfo> gFunctions;

// Extract function signature and overlay constraints (supports: "pure" and "nonneg_<paramName>")
static void collectFunctionsAndConstraints(Node* root) {
    gFunctions.clear();
    if (!root) return;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Fn") {
            FunctionInfo fi;
            fi.name = n->value;
            for (auto* ch : n->children) {
                if (ch->type == "Params") {
                    for (auto* p : ch->children) {
                        std::string pname = p->value;
                        std::string pty = (!p->children.empty() ? p->children[0]->value : "auto");
                        fi.params.emplace_back(pname, mapDeclaredTypeToKindEx(pty));
                    }
                }
                else if (ch->type == "Overlay") {
                    std::string ov = ch->value;
                    std::string low = ov;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                    if (low == "pure") {
                        fi.pure = true;
                    }
                    else if (low.size() > 7 && low.rfind("nonneg_", 0) == 0) {
                        // overlay syntax: nonneg_<paramName>
                        std::string pname = low.substr(7);
                        if (!pname.empty()) fi.nonnegParams.insert(pname);
                    }
                    else if (low == "nonnegative") {
                        // If user added a generic nonnegative overlay without param, apply to all numeric params
                        for (auto& pr : fi.params) fi.nonnegParams.insert(pr.first);
                    }
                }
            }
            gFunctions[fi.name] = std::move(fi);
        }
        for (auto* c : n->children) walk(c);
        };
    walk(root);
}

// ---- Side-effect classification for "pure" functions ----
static bool isSideEffecting(Node* n) {
    if (!n) return false;
    static const std::unordered_set<std::string> se = {
        "Print","Open","Write","Writeln","Read","Close","Send","Recv","Schedule","Sync","Mutate","Input","Checkpoint","VBreak"
    };
    return se.count(n->type) > 0;
}

// ---- Simple range/non-negativity facts ----
enum class NonNeg { Unknown, False, True };

struct Facts {
    std::vector<std::unordered_map<std::string, NonNeg>> stack;
    Facts() { stack.push_back({}); }
    void push() { stack.push_back({}); }
    void pop() { if (stack.size() > 1) stack.pop_back(); }
    NonNeg get(const std::string& v) const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto f = it->find(v);
            if (f != it->end()) return f->second;
        }
        return NonNeg::Unknown;
    }
    void set(const std::string& v, NonNeg nn) { stack.back()[v] = nn; }
};

static NonNeg isExprNonNeg(Node* e, const Facts& facts) {
    if (!e) return NonNeg::Unknown;
    if (e->type == "Num") {
        double d = 0; if (toDouble(e->value, d)) return d >= 0 ? NonNeg::True : NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "Var") return facts.get(e->value);
    if (e->type == "Ternary") {
        NonNeg a = isExprNonNeg(e->children[1], facts);
        NonNeg b = isExprNonNeg(e->children[2], facts);
        if (a == NonNeg::True && b == NonNeg::True) return NonNeg::True;
        if (a == NonNeg::False && b == NonNeg::False) return NonNeg::False;
        return NonNeg::Unknown;
    }
    if (e->type == "BinOp") {
        NonNeg L = isExprNonNeg(e->children[0], facts);
        NonNeg R = isExprNonNeg(e->children[1], facts);
        const std::string& op = e->value;
        if (op == "+") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            if (L == NonNeg::False || R == NonNeg::False) return NonNeg::Unknown;
            return NonNeg::Unknown;
        }
        if (op == "*") {
            if (L == NonNeg::True && R == NonNeg::True) return NonNeg::True;
            // negative * negative could be non-neg, but we don't prove it here
            return NonNeg::Unknown;
        }
        // Other ops unknown
        return NonNeg::Unknown;
    }
    return NonNeg::Unknown;
}

static int gSemanticErrors = 0;
static void reportSemError(const std::string& msg) {
    ++gSemanticErrors;
    std::cerr << "[semantic error] " << msg << "\n";
}

static void learnFactsFromStmt(Node* n, Facts& facts) {
    if (!n) return;
    if (n->type == "Let" && !n->children.empty()) {
        NonNeg nn = isExprNonNeg(n->children[0], facts);
        if (nn != NonNeg::Unknown) facts.set(n->value, nn);
    }
    if (n->type == "Assign" && !n->children.empty()) {
        Node* lhs = n->children[0];
        if (lhs && lhs->type == "Var") {
            const std::string& name = lhs->value;
            if (n->value == "=" && n->children.size() > 1) {
                NonNeg nn = isExprNonNeg(n->children[1], facts);
                if (nn != NonNeg::Unknown) facts.set(name, nn);
            }
            else if (n->value == "+=" && n->children.size() > 1) {
                NonNeg a = facts.get(name);
                NonNeg b = isExprNonNeg(n->children[1], facts);
                if (a == NonNeg::True && b == NonNeg::True) facts.set(name, NonNeg::True);
            }
            else if (n->value == "-=") {
                // may break non-neg; mark unknown
                facts.set(name, NonNeg::Unknown);
            }
        }
    }
    if (n->type == "Bounds" && n->children.size() == 3) {
        const std::string& name = n->children[0]->value;
        NonNeg mn = isExprNonNeg(n->children[1], facts);
        if (mn == NonNeg::True) facts.set(name, NonNeg::True);
    }
}

// Validate expressions for strong typing (basic checks)
static void strongTypeCheckExpr(Node* e,
    std::vector<std::unordered_map<std::string, TypeKindEx>>& typeScopes) {
    if (!e) return;
    if (e->type == "BinOp") {
        TypeKindEx L = inferExprStrong(e->children[0], typeScopes);
        TypeKindEx R = inferExprStrong(e->children[1], typeScopes);
        const std::string& op = e->value;
        auto isArith = [&](const std::string& s) { return s == "-" || s == "*" || s == "/" || s == "%"; };
        auto isRel = [&](const std::string& s) { return s == "<" || s == ">" || s == "<=" || s == ">=" || s == "==" || s == "!="; };
        if (op == "+") {
            // ok for number+number or string+string; else flag
            if (!((L == TypeKindEx::Number && R == TypeKindEx::Number) ||
                (L == TypeKindEx::String && R == TypeKindEx::String))) {
                reportSemError("Operator '+' requires both Number or both String operands");
            }
        }
        else if (isArith(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Arithmetic operator '" + op + "' requires Number operands");
            }
        }
        else if (isRel(op)) {
            if (!(L == TypeKindEx::Number && R == TypeKindEx::Number)) {
                reportSemError("Relational operator '" + op + "' requires Number operands");
            }
        }
    }
    for (auto* c : e->children) strongTypeCheckExpr(c, typeScopes);
}

// ---- Enforce function purity and nonneg param constraints ----
static void enforceFunctionBodyConstraints(Node* fnNode,
    const FunctionInfo& fi,
    const std::unordered_map<std::string, FunctionInfo>& fnMap) {
    // Gather parameter set for quick lookup and track locals from let
    std::unordered_set<std::string> paramSet;
    for (auto& p : fi.params) paramSet.insert(p.first);

    Facts facts; // non-negativity facts
    // Start with nonneg facts for parameters that are declared nonneg (assume true)
    for (const auto& p : fi.nonnegParams) facts.set(p, NonNeg::True);

    // Strong type scopes seeded with parameters
    std::vector<std::unordered_map<std::string, TypeKindEx>> typeScopes(1);
    for (auto& pr : fi.params) typeScopes.back()[pr.first] = pr.second;

    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (fi.pure && isSideEffecting(n)) {
            reportSemError("Function '" + fi.name + "' is pure but performs side-effect: " + n->type);
        }
        if (fi.pure && n->type == "Assign" && !n->children.empty() && n->children[0]->type == "Var") {
            const std::string& target = n->children[0]->value;
            if (paramSet.count(target)) {
                reportSemError("Function '" + fi.name + "' is pure; must not reassign parameter '" + target + "'");
            }
        }
        if ((n->type == "Call" || n->type == "CallExpr") && fi.pure) {
            // impure callee in pure context
            std::string callee = (n->type == "Call") ? n->value : n->value;
            auto it = fnMap.find(callee);
            if (it != fnMap.end() && !it->second.pure) {
                reportSemError("Function '" + fi.name + "' is pure but calls impure function '" + callee + "'");
            }
        }

        // Type checks on expressions
        if (n->type == "Print") {
            if (!n->children.empty()) strongTypeCheckExpr(n->children[0], typeScopes);
        }
        else if (n->type == "Let") {
            if (!n->children.empty()) {
                strongTypeCheckExpr(n->children[0], typeScopes);
                // If it's clearly string/number, update local type
                TypeKindEx k = inferExprStrong(n->children[0], typeScopes);
                if (k != TypeKindEx::Unknown) typeScopes.back()[n->value] = k;
            }
        }
        else if (n->type == "Assign") {
            if (n->children.size() > 1) strongTypeCheckExpr(n->children[1], typeScopes);
        }
        else if (n->type == "Scale" || n->type == "Bounds") {
            // All expressions should be numeric
            for (size_t i = 1; i < n->children.size(); ++i) {
                TypeKindEx k = inferExprStrong(n->children[i], typeScopes);
                if (k == TypeKindEx::String) {
                    reportSemError("'" + n->type + "' expects Number expressions, got String");
                }
            }
        }

        // Learn nonneg facts from simple statements
        learnFactsFromStmt(n, facts);

        // Recurse
        for (auto* c : n->children) walk(c);
        };

    // Find function body and walk it
    Node* body = nullptr;
    for (auto* ch : fnNode->children) if (ch->type == "Body") { body = ch; break; }
    if (body) walk(body);
}

static void enforceCallSiteConstraints(Node* root,
    const std::unordered_map<std::string, FunctionInfo>& fnMap) {
    Facts facts;
    std::function<void(Node*)> walk = [&](Node* n) {
        if (!n) return;
        if (n->type == "Body" || n->type == "Program") {
            facts.push();
            for (auto* c : n->children) {
                // learn facts in sequence
                learnFactsFromStmt(c, facts);
                walk(c);
            }
            facts.pop();
            return;
        }
        if (n->type == "Call" || n->type == "CallExpr") {
            const std::string callee = n->value;
            auto it = fnMap.find(callee);
            if (it != fnMap.end()) {
                const FunctionInfo& fi = it->second;
                // Build param index map
                std::unordered_map<std::string, size_t> idx;
                for (size_t i = 0; i < fi.params.size(); ++i) idx[fi.params[i].first] = i;
                for (const auto& p : fi.nonnegParams) {
                    auto posIt = idx.find(p);
                    if (posIt != idx.end()) {
                        size_t argIdx = posIt->second;
                        if (argIdx < n->children.size()) {
                            NonNeg res = isExprNonNeg(n->children[argIdx], facts);
                            if (res == NonNeg::False) {
                                reportSemError("Call to '" + callee + "' violates nonneg constraint for parameter '" + p + "'");
                            }
                            else if (res == NonNeg::Unknown) {
                                // Warning could be implemented; we keep strictness to errors only when proven negative
                            }
                        }
                    }
                }
            }
        }
        for (auto* c : n->children) walk(c);
    };
    walk(root);
}

static void enforceSemanticConstraints(Node* root) {
    gSemanticErrors = 0;
    collectFunctionsAndConstraints(root);
    // Enforce function-level constraints
    for (auto& kv : gFunctions) {
        // Locate the function node
        Node* fnNode = nullptr;
        std::function<void(Node*)> findFn = [&](Node* n) {
            if (!n || fnNode) return;
            if (n->type == "Fn" && n->value == kv.first) { fnNode = n; return; }
            for (auto* c : n->children) findFn(c);
            };
        findFn(root);
        if (fnNode) enforceFunctionBodyConstraints(fnNode, kv.second, gFunctions);
    }
    // Enforce call-site parameter constraints across program
    enforceCallSiteConstraints(root, gFunctions);

    if (gSemanticErrors > 0) {
        throw std::runtime_error("Semantic validation failed with " + std::to_string(gSemanticErrors) + " error(s).");
    }
}

// [CHANGED] Call the new semantic enforcement right after basic analysis
// ...
        // Analyze types
std::vector<std::unordered_map<std::string, TypeKind>> scopes;
analyze(ast, scopes);
emitEventAst("analyzed", ast);

// Enforce strong typing and overlay-driven semantic constraints
enforceSemanticConstraints(ast);

// Plugin transforms before optimize
runTransforms("before-opt", ast);

// [ADD] at top includes
#include <cstdio>   // popen/_popen
// [ADD] Build options, metadata & hooks utilities (place after emitEventText/runTransforms section)

// Build configuration parsed from CLI
struct BuildConfig {
    std::string cc;       // compiler (clang++, clang-cl, g++, cl)
    std::string stdver;   // c++14, c++17, c++20
    std::string opt;      // O0/O1/O2/O3
    std::string diagFmt;  // msvc or clang
    bool color = true;
};

static BuildConfig gBuild;
static std::string gSourcePath; // .case path used for #line mapping

// Metadata bag to embed into generated output
static std::unordered_map<std::string, std::string> gMeta;

static void setMeta(const std::string& k, const std::string& v) { gMeta[k] = v; }
static std::string getMetadataJson() {
    std::ostringstream os;
    os << "{";
    bool first = true;
    for (auto& kv : gMeta) {
        if (!first) os << ",";
        os << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    os << "}";
    return os.str();
}

// Simple process runner with output capture
struct ProcResult { int exitCode; std::string output; };
static ProcResult runProcessCapture(const std::string& cmd) {
#if defined(_WIN32)
    std::string full = cmd + " 2>&1";
    FILE* pipe = _popen(full.c_str(), "r");
    if (!pipe) return { -1, "failed to start: " + cmd };
    std::string out;
    char buf[4096];
    while (size_t n = std::fread(buf, 1, sizeof(buf), pipe)) out.append(buf, n);
    int rc = _pclose(pipe);
    return { rc, out };
#else
    std::string full = cmd + " 2>&1";
    FILE* pipe = popen(full.c_str(), "r");
    if (!pipe) return { -1, "failed to start: " + cmd };
    std::string out;
    char buf[4096];
    while (size_t n = std::fread(buf, 1, sizeof(buf), pipe)) out.append(buf, n);
    int rc = pclose(pipe);
    return { rc, out };
#endif
}

// Hook helpers for consistent phase names
static void hookInspect(const char* phase, Node* root) { emitEventAst(phase, root); }
static void hookMutate(const char* phase, Node*& root) { runTransforms(phase, root); }
// [MODIFY] emitPrelude to embed metadata banner
static void emitPrelude(std::ostringstream& out) {
    // Metadata banner (JSON) embedded as comment + global string
    const std::string meta = getMetadataJson();
    out << "/* CASE metadata: " << meta << " */\n";
    out << "static const char* __CASE_METADATA = R\"(" << meta << ")\";\n";

    out << "#include <iostream>\n";
    out << "#include <fstream>\n";
    out << "#include <cmath>\n";
    out << "#include <vector>\n";
    out << "#include <deque>\n";
    out << "#include <mutex>\n";
    out << "#include <condition_variable>\n";
    out << "#include <functional>\n";
    out << "#include <algorithm>\n";
    out << "#if defined(_OPENMP)\n#include <omp.h>\n#endif\n";
    // Simple channel template
    out << "template<typename T>\n";
    out << "struct Channel {\n";
    out << "  std::mutex m; std::condition_variable cv; std::deque<T> q;\n";
    out << "  void send(const T& v){ std::lock_guard<std::mutex> lk(m); q.push_back(v); cv.notify_one(); }\n";
    out << "  void recv(T& out){ std::unique_lock<std::mutex> lk(m); cv.wait(lk,[&]{return !q.empty();}); out = std::move(q.front()); q.pop_front(); }\n";
    out << "};\n\n";
}
// [REPLACE] tryNativeBuild with configurable flags + captured diagnostics
static bool tryNativeBuild(const std::string& outExe, const BuildConfig& cfg, const std::string& sourceCpp) {
    const char* noCompile = std::getenv("CASEC_NO_COMPILE");
    if (noCompile && std::string(noCompile) == "1") return true;

    auto fmtStd = [&]()->std::string {
        if (cfg.stdver == "c++17") return "-std=c++17";
        if (cfg.stdver == "c++20") return "-std=c++20";
        return "-std=c++14";
    };

#if defined(_WIN32)
    // Prefer clang-cl, allow override via cfg.cc
    std::string cc = cfg.cc.empty() ? "clang-cl" : cfg.cc;
    ProcResult pr{0,""};
    if (cc == "clang-cl") {
        std::ostringstream clir;
        clir << "clang-cl /EHsc /DNDEBUG /openmp "
             << "/O" << (cfg.opt.empty() ? "2" : cfg.opt.substr(1)) << " "
             << "/std:" << (cfg.stdver.empty() ? "c++14" : cfg.stdver) << " "
             << quote(sourceCpp) << " /link /OUT:" << quote(outExe);
        pr = runProcessCapture(clir.str());
        if (pr.exitCode != 0) {
            // Fallback to clang++
            std::ostringstream cc2;
            cc2 << "clang++ " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
                << " -fopenmp -DNDEBUG " << quote(sourceCpp) << " -o " << quote(outExe);
            pr = runProcessCapture(cc2.str());
        }
    } else if (cc == "cl") {
        std::ostringstream clir;
        clir << "cl /EHsc /nologo /DNDEBUG /openmp "
             << "/O" << (cfg.opt.empty() ? "2" : cfg.opt.substr(1)) << " "
             << "/std:" << (cfg.stdver.empty() ? "c++14" : cfg.stdver) << " "
             << quote(sourceCpp) << " /Fe" << quote(outExe);
        pr = runProcessCapture(clir.str());
    } else {
        // Use provided cc directly
        std::ostringstream any;
        any << cc << " " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
            << " -fopenmp -DNDEBUG " << quote(sourceCpp) << " -o " << quote(outExe);
        pr = runProcessCapture(any.str());
    }
    std::cout << pr.output;
    return pr.exitCode == 0;
#else
    std::string cc = cfg.cc.empty() ? "clang++" : cfg.cc;
    std::ostringstream cmd;
    cmd << cc << " " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
        << " -flto -march=native -fopenmp -DNDEBUG "
        << quote(sourceCpp) << " -o " << quote(outExe);
    ProcResult pr = runProcessCapture(cmd.str());
    std::cout << pr.output;
    return pr.exitCode == 0;
#endif
}
