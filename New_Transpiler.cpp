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
               s == "else"  || s == "Fn"   || s == "call"|| s == "let" ||
               s == "while"|| s == "break"|| s == "continue" ||
               s == "switch"|| s == "case"|| s == "default" ||
               s == "overlay" || s == "open" || s == "write" || s == "writeln" ||
               s == "read" || s == "close" || s == "mutate" ||
               s == "scale" || s == "bounds" || s == "checkpoint" || s == "vbreak" ||
               s == "channel" || s == "send" || s == "recv" || s == "sync" ||
               s == "schedule" || s == "input" || s == "true" || s == "false" ||
               s == "class" || s == "extends" || s == "public" || s == "private" || s == "protected";
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
        // Try to read multi-character operators first
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
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0; // [ADD] Source line for diagnostics and smart overlays
};

// ------------------------ Parser ------------------------

// [FWD] class parsing
static Node* parseClass();
static Node* parseMethodInClass();

// [ADD] parse a method (Fn) inside a class into a 'Method' node
static Node* parseMethodInClass() {
    advanceTok(); // 'Fn'
    Node* m = new Node{"Method",""};
    m->line = previous().line;
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected method name after 'Fn' at line " + std::to_string(peek().line));
    m->value = advanceTok().value;

    // Parameters: same syntax as top-level Fn
    std::vector<std::string> paramStrFrags;
    bool sawParenParams = false;
    if (checkValue("(")) {
        int depth = 0;
        if (matchValue("(")) {
            ++depth;
            while (peek().type != TokenType::END && depth > 0) {
                if (matchValue("(")) { ++depth; continue; }
                if (matchValue(")")) { --depth; continue; }
                advanceTok(); // ignore inside, we don't parse types by tokens here
            }
            sawParenParams = true;
        }
    } else if (peek().type == TokenType::STRING) {
        paramStrFrags.push_back(advanceTok().value);
        while (matchValue(",")) {
            if (peek().type == TokenType::STRING) paramStrFrags.push_back(advanceTok().value);
            else break;
        }
    }
    if (sawParenParams || !paramStrFrags.empty()) {
        Node* params = new Node{"Params",""};
        auto pairs = parseParamListFromStringFragments(paramStrFrags);
        for (auto& pr : pairs) {
            Node* p = new Node{"Param", pr.second};
            Node* ty = new Node{"Type", pr.first};
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        m->children.push_back(params);
    }

    // Attach pending overlays to method
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov.first};
        o->line = m->line;
        for (auto* a : ov.second) o->children.push_back(a);
        m->children.push_back(o);
    }
    gPendingOverlays.clear();

    // Body
    Node* body = nullptr;
    if (checkValue("{"))      body = parseBlock();
    else if (checkValue("(")) body = parseParenBlock();
    else throw std::runtime_error("Expected '{' or '(' to start method body at line " + std::to_string(peek().line));
    m->children.push_back(body);
    skipBracketBlockIfPresent();
    return m;
}

// [ADD] parse class with optional inheritance and access sections
static Node* parseClass() {
    advanceTok(); // 'class'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected class name at line " + std::to_string(peek().line));
    Node* cls = new Node{"Class",""};
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{"Bases",""};
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{"Base", advanceTok().value};
            b->line = previous().line;
            bases->children.push_back(b);
            if (matchValue(",")) continue;
            break;
        }
        cls->children.push_back(bases);
    }

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start class body at line " + std::to_string(peek().line));

    std::string curAccess = "private";
    while (!checkValue("}") && peek().type != TokenType::END) {
        // access label: public: / private: / protected:
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        // overlay directives apply to the next method
        if (checkValue("overlay")) {
            parseOverlay();
            continue;
        }
        // methods
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{"Access", curAccess};
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}

// ------------------------ AST ------------------------

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0; // [ADD] Source line for diagnostics and smart overlays
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
// [MODIFY] Overlay buffer to hold names + params
static std::vector<std::pair<std::string, std::vector<Node*>>> gPendingOverlays;

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
    n->line = previous().line;
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    } else if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
// [REPLACE] parseOverlay to support parameters: overlay name(|, name ...), each may have (args)
static Node* parseOverlay() {
    advanceTok(); // 'overlay'
    while (peek().type == TokenType::IDENT) {
        std::string name = advanceTok().value;
        std::vector<Node*> args;
        if (checkValue("(")) {
            advanceTok();
            while (!checkValue(")") && peek().type != TokenType::END) {
                if (peek().type == TokenType::STRING) {
                    Node* p = new Node{"Str", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{"Num", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{"Var", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                }
                if (checkValue(",")) advanceTok();
            }
            if (!matchValue(")")) throw std::runtime_error("Expected ')' to close overlay args at line " + std::to_string(peek().line));
        }
        gPendingOverlays.emplace_back(std::move(name), std::move(args));
        if (matchValue(",")) continue;
        else break;
    }
    skipBracketBlockIfPresent();
    return new Node{"OverlayDecl",""};
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
    n->line = previous().line;
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
    n->line = previous().line;
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
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

    // Attach pending overlays (name + param nodes)
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov.first};
        o->line = n->line;
        for (auto* a : ov.second) o->children.push_back(a);
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

static Node* parseClass() {
    advanceTok(); // 'class'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected class name at line " + std::to_string(peek().line));
    Node* cls = new Node{"Class",""};
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{"Bases",""};
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{"Base", advanceTok().value};
            b->line = previous().line;
            bases->children.push_back(b);
            if (matchValue(",")) continue;
            break;
        }
        cls->children.push_back(bases);
    }

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start class body at line " + std::to_string(peek().line));

    std::string curAccess = "private";
    while (!checkValue("}") && peek().type != TokenType::END) {
        // access label: public: / private: / protected:
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        // overlay directives apply to the next method
        if (checkValue("overlay")) {
            parseOverlay();
            continue;
        }
        // methods
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{"Access", curAccess};
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}

// ------------------------ AST ------------------------

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0; // [ADD] Source line for diagnostics and smart overlays
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
// [MODIFY] Overlay buffer to hold names + params
static std::vector<std::pair<std::string, std::vector<Node*>>> gPendingOverlays;

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
    n->line = previous().line;
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    } else if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
// [REPLACE] parseOverlay to support parameters: overlay name(|, name ...), each may have (args)
static Node* parseOverlay() {
    advanceTok(); // 'overlay'
    while (peek().type == TokenType::IDENT) {
        std::string name = advanceTok().value;
        std::vector<Node*> args;
        if (checkValue("(")) {
            advanceTok();
            while (!checkValue(")") && peek().type != TokenType::END) {
                if (peek().type == TokenType::STRING) {
                    Node* p = new Node{"Str", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{"Num", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{"Var", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                }
                if (checkValue(",")) advanceTok();
            }
            if (!matchValue(")")) throw std::runtime_error("Expected ')' to close overlay args at line " + std::to_string(peek().line));
        }
        gPendingOverlays.emplace_back(std::move(name), std::move(args));
        if (matchValue(",")) continue;
        else break;
    }
    skipBracketBlockIfPresent();
    return new Node{"OverlayDecl",""};
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
    n->line = previous().line;
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
    n->line = previous().line;
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
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
    } else if (peek().type == TokenType::STRING) {
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

    // Attach pending overlays (name + param nodes)
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov.first};
        o->line = n->line;
        for (auto* a : ov.second) o->children.push_back(a);
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

static Node* parseClass() {
    advanceTok(); // 'class'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected class name at line " + std::to_string(peek().line));
    Node* cls = new Node{"Class",""};
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{"Bases",""};
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{"Base", advanceTok().value};
            b->line = previous().line;
            bases->children.push_back(b);
            if (matchValue(",")) continue;
            break;
        }
        cls->children.push_back(bases);
    }

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start class body at line " + std::to_string(peek().line));

    std::string curAccess = "private";
    while (!checkValue("}") && peek().type != TokenType::END) {
        // access label: public: / private: / protected:
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        // overlay directives apply to the next method
        if (checkValue("overlay")) {
            parseOverlay();
            continue;
        }
        // methods
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{"Access", curAccess};
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}

// ------------------------ AST ------------------------

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0; // [ADD] Source line for diagnostics and smart overlays
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
// [MODIFY] Overlay buffer to hold names + params
static std::vector<std::pair<std::string, std::vector<Node*>>> gPendingOverlays;

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
    n->line = previous().line;
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    } else if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
// [REPLACE] parseOverlay to support parameters: overlay name(|, name ...), each may have (args)
static Node* parseOverlay() {
    advanceTok(); // 'overlay'
    while (peek().type == TokenType::IDENT) {
        std::string name = advanceTok().value;
        std::vector<Node*> args;
        if (checkValue("(")) {
            advanceTok();
            while (!checkValue(")") && peek().type != TokenType::END) {
                if (peek().type == TokenType::STRING) {
                    Node* p = new Node{"Str", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{"Num", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{"Var", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                }
                if (checkValue(",")) advanceTok();
            }
            if (!matchValue(")")) throw std::runtime_error("Expected ')' to close overlay args at line " + std::to_string(peek().line));
        }
        gPendingOverlays.emplace_back(std::move(name), std::move(args));
        if (matchValue(",")) continue;
        else break;
    }
    skipBracketBlockIfPresent();
    return new Node{"OverlayDecl",""};
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
    n->line = previous().line;
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
    n->line = previous().line;
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
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
    } else if (peek().type == TokenType::STRING) {
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

    // Attach pending overlays (name + param nodes)
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov.first};
        o->line = n->line;
        for (auto* a : ov.second) o->children.push_back(a);
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

static Node* parseClass() {
    advanceTok(); // 'class'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected class name at line " + std::to_string(peek().line));
    Node* cls = new Node{"Class",""};
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{"Bases",""};
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{"Base", advanceTok().value};
            b->line = previous().line;
            bases->children.push_back(b);
            if (matchValue(",")) continue;
            break;
        }
        cls->children.push_back(bases);
    }

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start class body at line " + std::to_string(peek().line));

    std::string curAccess = "private";
    while (!checkValue("}") && peek().type != TokenType::END) {
        // access label: public: / private: / protected:
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        // overlay directives apply to the next method
        if (checkValue("overlay")) {
            parseOverlay();
            continue;
        }
        // methods
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{"Access", curAccess};
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}

// ------------------------ AST ------------------------

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0; // [ADD] Source line for diagnostics and smart overlays
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
// [MODIFY] Overlay buffer to hold names + params
static std::vector<std::pair<std::string, std::vector<Node*>>> gPendingOverlays;

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
    n->line = previous().line;
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    } else if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
// [REPLACE] parseOverlay to support parameters: overlay name(|, name ...), each may have (args)
static Node* parseOverlay() {
    advanceTok(); // 'overlay'
    while (peek().type == TokenType::IDENT) {
        std::string name = advanceTok().value;
        std::vector<Node*> args;
        if (checkValue("(")) {
            advanceTok();
            while (!checkValue(")") && peek().type != TokenType::END) {
                if (peek().type == TokenType::STRING) {
                    Node* p = new Node{"Str", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{"Num", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{"Var", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                }
                if (checkValue(",")) advanceTok();
            }
            if (!matchValue(")")) throw std::runtime_error("Expected ')' to close overlay args at line " + std::to_string(peek().line));
        }
        gPendingOverlays.emplace_back(std::move(name), std::move(args));
        if (matchValue(",")) continue;
        else break;
    }
    skipBracketBlockIfPresent();
    return new Node{"OverlayDecl",""};
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
    n->line = previous().line;
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
    n->line = previous().line;
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
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
    } else if (peek().type == TokenType::STRING) {
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

    // Attach pending overlays (name + param nodes)
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov.first};
        o->line = n->line;
        for (auto* a : ov.second) o->children.push_back(a);
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

static Node* parseClass() {
    advanceTok(); // 'class'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected class name at line " + std::to_string(peek().line));
    Node* cls = new Node{"Class",""};
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{"Bases",""};
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{"Base", advanceTok().value};
            b->line = previous().line;
            bases->children.push_back(b);
            if (matchValue(",")) continue;
            break;
        }
        cls->children.push_back(bases);
    }

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start class body at line " + std::to_string(peek().line));

    std::string curAccess = "private";
    while (!checkValue("}") && peek().type != TokenType::END) {
        // access label: public: / private: / protected:
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        // overlay directives apply to the next method
        if (checkValue("overlay")) {
            parseOverlay();
            continue;
        }
        // methods
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{"Access", curAccess};
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}

// ------------------------ AST ------------------------

struct Node {
    std::string type;
    std::string value;
    std::vector<Node*> children;
    int line = 0; // [ADD] Source line for diagnostics and smart overlays
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
// [MODIFY] Overlay buffer to hold names + params
static std::vector<std::pair<std::string, std::vector<Node*>>> gPendingOverlays;

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
    n->line = previous().line;
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    } else if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
    n->line = previous().line;
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
// [REPLACE] parseOverlay to support parameters: overlay name(|, name ...), each may have (args)
static Node* parseOverlay() {
    advanceTok(); // 'overlay'
    while (peek().type == TokenType::IDENT) {
        std::string name = advanceTok().value;
        std::vector<Node*> args;
        if (checkValue("(")) {
            advanceTok();
            while (!checkValue(")") && peek().type != TokenType::END) {
                if (peek().type == TokenType::STRING) {
                    Node* p = new Node{"Str", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{"Num", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                } else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{"Var", advanceTok().value};
                    p->line = previous().line;
                    args.push_back(p);
                }
                if (checkValue(",")) advanceTok();
            }
            if (!matchValue(")")) throw std::runtime_error("Expected ')' to close overlay args at line " + std::to_string(peek().line));
        }
        gPendingOverlays.emplace_back(std::move(name), std::move(args));
        if (matchValue(",")) continue;
        else break;
    }
    skipBracketBlockIfPresent();
    return new Node{"OverlayDecl",""};
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
    n->line = previous().line;
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

// [FWD] class parsing
static Node* parseClass();
static Node* parseMethodInClass();

// [ADD] parse a method (Fn) inside a class into a 'Method' node
static Node* parseMethodInClass() {
    advanceTok(); // 'Fn'
    Node* m = new Node{"Method",""};
    m->line = previous().line;
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected method name after 'Fn' at line " + std::to_string(peek().line));
    m->value = advanceTok().value;

    // Parameters: same syntax as top-level Fn
    std::vector<std::string> paramStrFrags;
    bool sawParenParams = false;
    if (checkValue("(")) {
        int depth = 0;
        if (matchValue("(")) {
            ++depth;
            while (peek().type != TokenType::END && depth > 0) {
                if (matchValue("(")) { ++depth; continue; }
                if (matchValue(")")) { --depth; continue; }
                advanceTok(); // ignore inside, we don't parse types by tokens here
            }
            sawParenParams = true;
        }
    } else if (peek().type == TokenType::STRING) {
        paramStrFrags.push_back(advanceTok().value);
        while (matchValue(",")) {
            if (peek().type == TokenType::STRING) paramStrFrags.push_back(advanceTok().value);
            else break;
        }
    }
    if (sawParenParams || !paramStrFrags.empty()) {
        Node* params = new Node{"Params",""};
        auto pairs = parseParamListFromStringFragments(paramStrFrags);
        for (auto& pr : pairs) {
            Node* p = new Node{"Param", pr.second};
            Node* ty = new Node{"Type", pr.first};
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        m->children.push_back(params);
    }

    // Attach pending overlays to method
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov.first};
        o->line = m->line;
        for (auto* a : ov.second) o->children.push_back(a);
        m->children.push_back(o);
    }
    gPendingOverlays.clear();

    // Body
    Node* body = nullptr;
    if (checkValue("{"))      body = parseBlock();
    else if (checkValue("(")) body = parseParenBlock();
    else throw std::runtime_error("Expected '{' or '(' to start method body at line " + std::to_string(peek().line));
    m->children.push_back(body);
    skipBracketBlockIfPresent();
    return m;
}

// [ADD] parse class with optional inheritance and access sections
static Node* parseClass() {
    advanceTok(); // 'class'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected class name at line " + std::to_string(peek().line));
    Node* cls = new Node{"Class",""};
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{"Bases",""};
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{"Base", advanceTok().value};
            b->line = previous().line;
            bases->children.push_back(b);
            if (matchValue(",")) continue;
            break;
        }
        cls->children.push_back(bases);
    }

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start class body at line " + std::to_string(peek().line));

    std::string curAccess = "private";
    while (!checkValue("}") && peek().type != TokenType::END) {
        // access label: public: / private: / protected:
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        // overlay directives apply to the next method
        if (checkValue("overlay")) {
            parseOverlay();
            continue;
        }
        // methods
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{"Access", curAccess};
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{"Access", curAccess};
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}
