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
    case '.':
        return true;
    default:
        return false;
    }
}

std::vector<Token> tokenize(const std::string& src) {
    std::vector<Token> tokens;
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
            s == "schedule";
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
    for (const auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov };
        n->children.push_back(o);
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

static Node* parseStatement() {
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

    // Unknown token - consume and produce placeholder node
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
        // visit children (conditions and bodies)
        for (auto* c : n->children) analyze(c, scopes);
        return;
    }
    if (n->type == "Let") {
        TypeKind t = inferExpr(n->children.empty() ? nullptr : n->children[0], scopes);
        if (scopes.empty()) scopes.push_back({});
        scopes.back()[n->value] = t;
    }
    if (n->type == "Scale" || n->type == "Bounds") {
        // Expect numeric operands
        for (size_t i = 1; i < n->children.size(); ++i) {
            TypeKind tk = inferExpr(n->children[i], scopes);
            if (tk == TypeKind::String) {
                std::cerr << "[warn] string used where number expected in '" << n->type << "'\n";
            }
        }
    }
    // Walk generic children
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
        // Run all transforms; pass point allows plugin to branch internally if needed
        if (p.second) {
            bool changed = p.second(root, passPoint);
            (void)changed;
        }
    }
}

// Built-in Introspection sink: no-op (extend to log externally if needed)
static void builtinAstSink(const char* /*phase*/, const Node* /*root*/) {
    // Hook for CIAM overlays or external bridges.
}

static void builtinTextSink(const char* /*phase*/, const char* /*text*/, size_t /*len*/) {
    // Hook for CIAM overlays or external bridges.
}

// Forward declaration for internal optimizer (defined later in this file)
static void optimize(Node*& root);

// Built-in mutation: evolutionary re-optimization (re-run optimize a few rounds)
static bool mutateEvolve(Node*& root, const char* passPoint) {
    // Apply extra simplification rounds at known points
    if (std::string(passPoint) == "before-opt" || std::string(passPoint) == "after-opt") {
        // very lightweight "evolution": two extra folds and DCE sweeps
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
        // children: [0]=Var x, [1]=a, [2]=b, [3]=c, [4]=d
        std::string x = n->children[0]->value;
        std::string a = emitExpr(n->children[1]);
        std::string b = emitExpr(n->children[2]);
        std::string c = emitExpr(n->children[3]);
        std::string d = emitExpr(n->children[4]);
        out << "{ auto __t = ((" << x << ") - (" << a << ")) / ((" << b << ") - (" << a << ")); "
            << x << " = (" << c << ") + __t * ((" << d << ") - (" << c << ")); }\n";
    }
    else if (n->type == "Bounds") {
        // children: [0]=Var x, [1]=min, [2]=max
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
        // value=name, child[0]=type string (e.g., "int")
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
        // Preprocess with CIAM features (enabled only when source contains: call CIAM[on])
        ciam::Preprocessor ciamPre;
        src = ciamPre.Process(src);

        // Init plugins/overlays system
        initPluginsOnce();

        // Lex
        auto tokens = tokenize(src);
        {
            // tokens snapshot as a single-line summary
            std::ostringstream ts;
            ts << "tokens=" << tokens.size();
            emitEventText("tokens", ts.str());
        }

        // Parse
        Node* ast = parseProgram(tokens);
        emitEventAst("parsed", ast);

        // Collect overlays to enable CIAM-like behaviors
        collectOverlaysFlags(ast);

        // Analyze types
        std::vector<std::unordered_map<std::string, TypeKind>> scopes;
        analyze(ast, scopes);
        emitEventAst("analyzed", ast);

        // Plugin transforms before optimize
        runTransforms("before-opt", ast);

        // Optimize
        optimize(ast);
        emitEventAst("optimized", ast);

        // Plugin transforms after optimize
        runTransforms("after-opt", ast);

        // Emit C++
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

        // Write symbolic replay if enabled
        if (gEnableReplay) {
            std::ofstream replay("replay.txt", std::ios::binary);
            if (replay) {
                for (auto& s : gReplay) {
                    replay << "=== " << s.phase << " ===\n";
                    replay << s.payload << "\n";
                }
                std::cout << "[OK] Wrote symbolic replay to replay.txt\n";
            }
            else {
                std::cerr << "[warn] failed to write replay.txt\n";
            }
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}


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
               s == "class" || s == "extends" || s == "public" || s == "private" || s == "protected" ||
               /* [ADD] */ s == "routine" || s == "struct" || s == "splice" || s == "duration" || s == "derivative";
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
    Node* m = new Node{ "Method","" };
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
            Node* p = new Node{ "Param", pr.second };
            Node* ty = new Node{ "Type", pr.first };
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        m->children.push_back(params);
    }

    // Attach pending overlays to method
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov.first };
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
    Node* cls = new Node{ "Class","" };
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{ "Bases","" };
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{ "Base", advanceTok().value };
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
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
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
                    Node* p = new Node{ "Str", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{ "Num", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{ "Var", advanceTok().value };
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
        Node* o = new Node{ "Overlay", ov.first };
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
    Node* cls = new Node{ "Class","" };
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{ "Bases","" };
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{ "Base", advanceTok().value };
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
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
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
                    Node* p = new Node{ "Str", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{ "Num", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{ "Var", advanceTok().value };
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
        Node* o = new Node{ "Overlay", ov.first };
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
    Node* cls = new Node{ "Class","" };
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{ "Bases","" };
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{ "Base", advanceTok().value };
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
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
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
                    Node* p = new Node{ "Str", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{ "Num", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{ "Var", advanceTok().value };
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
        Node* o = new Node{ "Overlay", ov.first };
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
    Node* cls = new Node{ "Class","" };
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{ "Bases","" };
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{ "Base", advanceTok().value };
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
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
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
                    Node* p = new Node{ "Str", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{ "Num", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{ "Var", advanceTok().value };
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
        Node* o = new Node{ "Overlay", ov.first };
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
    Node* cls = new Node{ "Class","" };
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{ "Bases","" };
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{ "Base", advanceTok().value };
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
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
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
                    Node* p = new Node{ "Str", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{ "Num", advanceTok().value };
                    p->line = previous().line;
                    args.push_back(p);
                }
                else if (peek().type == TokenType::IDENT) {
                    Node* p = new Node{ "Var", advanceTok().value };
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
    Node* m = new Node{ "Method","" };
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
            Node* p = new Node{ "Param", pr.second };
            Node* ty = new Node{ "Type", pr.first };
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        m->children.push_back(params);
    }

    // Attach pending overlays to method
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov.first };
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
    Node* cls = new Node{ "Class","" };
    cls->value = advanceTok().value;
    cls->line = previous().line;

    // optional extends Base1, Base2
    if (checkValue("extends")) {
        advanceTok();
        Node* bases = new Node{ "Bases","" };
        while (peek().type == TokenType::IDENT) {
            Node* b = new Node{ "Base", advanceTok().value };
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
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            cls->children.push_back(acc);
            continue;
        }
        // fields (let with required initializer for type inference)
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field"; // mark as field instead of statement
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            cls->children.push_back(acc);
            continue;
        }

        // Fallback to generic statement inside class (rare)
        Node* st = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
        acc->children.push_back(st);
        cls->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close class '" + cls->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return cls;
}

// ------------------------ Parser ------------------------

// add these near other forwards
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
                advanceTok();
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
            Node* p = new Node{ "Param", pr.second };
            Node* ty = new Node{ "Type", pr.first };
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        n->children.push_back(params);
    }

    // Optional return type: '->' IDENT|STRING
    if (checkValue("->")) {
        advanceTok();
        std::string rty;
        if (peek().type == TokenType::IDENT || peek().type == TokenType::STRING) {
            rty = advanceTok().value;
        }
        else {
            throw std::runtime_error("Expected type after '->' at line " + std::to_string(peek().line));
        }
        Node* rt = new Node{ "ReturnType", rty };
        n->children.push_back(rt);
    }

    // Attach pending overlays as children
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov.first };
        o->line = n->line;
        for (auto* a : ov.second) o->children.push_back(a);
        n->children.push_back(o);
    }
    gPendingOverlays.clear();

    Node* body = nullptr;
    if (checkValue("{")) body = parseBlock();
    else if (checkValue("(")) body = parseParenBlock();
    else throw std::runtime_error("Expected '{' or '(' to start function body at line " + std::to_string(peek().line));

    n->children.push_back(body);
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseStruct() {
    advanceTok(); // 'struct'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected struct name at line " + std::to_string(peek().line));
    Node* st = new Node{ "Struct","" };
    st->value = advanceTok().value;
    st->line = previous().line;

    if (!matchValue("{")) throw std::runtime_error("Expected '{' to start struct body at line " + std::to_string(peek().line));

    std::string curAccess = "public"; // struct defaults to public
    while (!checkValue("}") && peek().type != TokenType::END) {
        if (checkType(TokenType::KEYWORD) && (checkValue("public") || checkValue("private") || checkValue("protected"))) {
            std::string acc = advanceTok().value;
            if (!matchValue(":")) throw std::runtime_error("Expected ':' after access specifier at line " + std::to_string(peek().line));
            curAccess = acc;
            continue;
        }
        if (checkValue("overlay")) { parseOverlay(); continue; }
        if (checkValue("Fn")) {
            Node* method = parseMethodInClass();
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(method);
            st->children.push_back(acc);
            continue;
        }
        if (checkValue("let")) {
            Node* field = parseLet();
            field->type = "Field";
            Node* acc = new Node{ "Access", curAccess };
            acc->children.push_back(field);
            st->children.push_back(acc);
            continue;
        }
        Node* stmnt = parseStatement();
        Node* acc = new Node{ "Access", curAccess };
        acc->children.push_back(stmnt);
        st->children.push_back(acc);
    }

    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close struct '" + st->value + "' at line " + std::to_string(peek().line));
    skipBracketBlockIfPresent();
    return st;
}
static Node* parseRoutine() {
    advanceTok(); // 'routine'
    // Reuse parseFn logic but tag as 'Routine'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected routine name at line " + std::to_string(peek().line));
    // Build via a temporary Fn, then retag
    size_t savePos = pos - 1; // 'routine' already consumed
    // Implement inline similar to parseFn
    Node* n = new Node{ "Routine", "" };
    n->line = previous().line;
    n->value = advanceTok().value;

    // params
    std::vector<std::string> paramStrFrags;
    bool sawParenParams = false;
    if (checkValue("(")) {
        int depth = 0;
        if (matchValue("(")) {
            ++depth;
            while (peek().type != TokenType::END && depth > 0) {
                if (matchValue("(")) { ++depth; continue; }
                if (matchValue(")")) { --depth; continue; }
                advanceTok();
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
            Node* p = new Node{ "Param", pr.second };
            Node* ty = new Node{ "Type", pr.first };
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        n->children.push_back(params);
    }
    // optional return type
    if (checkValue("->")) {
        advanceTok();
        std::string rty;
        if (peek().type == TokenType::IDENT || peek().type == TokenType::STRING) rty = advanceTok().value;
        else throw std::runtime_error("Expected type after '->' at line " + std::to_string(peek().line));
        Node* rt = new Node{ "ReturnType", rty };
        n->children.push_back(rt);
    }
    // overlays
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov.first };
        o->line = n->line;
        for (auto* a : ov.second) o->children.push_back(a);
        n->children.push_back(o);
    }
    gPendingOverlays.clear();
    // body
    Node* body = nullptr;
    if (checkValue("{")) body = parseBlock();
    else if (checkValue("(")) body = parseParenBlock();
    else throw std::runtime_error("Expected '{' or '(' to start routine body at line " + std::to_string(peek().line));
    n->children.push_back(body);
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseClass();
static Node* parseStruct();
static Node* parseRoutine();
static Node* parseMethodInClass();
static Node* parseSplice();
static Node* parseDuration();
static Node* parseDerivative();
static std::string mapTypeToCpp(const std::string& ty) {
    std::string low = ty;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (low == "void") return "void";               // [ADD]
    if (low == "int") return "int";
    if (low == "double") return "double";
    if (low == "float") return "float";
    if (low == "bool") return "bool";
    if (low.find("string") != std::string::npos) return "std::string";
    if (low == "auto") return "auto";
    return "auto";
}

// Parse a method inside a class: supports parameters and optional return type via '-> Type'
static Node* parseMethodInClass() {
    advanceTok(); // 'Fn'
    Node* m = new Node{ "Method","" };
    m->line = previous().line;

    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected method name after 'Fn' at line " + std::to_string(peek().line));
    m->value = advanceTok().value;

    // Parameters: same options as parseFn
    std::vector<std::string> paramStrFrags;
    bool sawParenParams = false;
    if (checkValue("(")) {
        int depth = 0;
        if (matchValue("(")) {
            ++depth;
            while (peek().type != TokenType::END && depth > 0) {
                if (matchValue("(")) { ++depth; continue; }
                if (matchValue(")")) { --depth; continue; }
                advanceTok();
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
            Node* p = new Node{ "Param", pr.second };
            Node* ty = new Node{ "Type", pr.first };
            p->children.push_back(ty);
            params->children.push_back(p);
        }
        m->children.push_back(params);
    }

    // Optional return type: '->' IDENT|STRING
    if (checkValue("->")) {
        advanceTok();
        std::string rty;
        if (peek().type == TokenType::IDENT || peek().type == TokenType::STRING) {
            rty = advanceTok().value;
        }
        else {
            throw std::runtime_error("Expected type after '->' at line " + std::to_string(peek().line));
        }
        Node* rt = new Node{ "ReturnType", rty };
        m->children.push_back(rt);
    }

    // Attach pending overlays to method
    for (auto& ov : gPendingOverlays) {
        Node* o = new Node{ "Overlay", ov.first };
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

// splice "<raw-cpp>" ; inject raw C++ directly
static Node* parseSplice() {
    advanceTok(); // 'splice'
    if (peek().type != TokenType::STRING) throw std::runtime_error("Expected string after 'splice' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Splice", advanceTok().value };
    n->line = previous().line;
    skipOptionalSemicolon();
    return n;
}

// duration <expr> "unit"
static Node* parseDuration() {
    advanceTok(); // 'duration'
    Node* e = parseExpression();
    if (!e) throw std::runtime_error("Expected expression in 'duration' at line " + std::to_string(peek().line));
    if (peek().type != TokenType::STRING) throw std::runtime_error("Expected unit string in 'duration' at line " + std::to_string(peek().line));
    Node* unit = new Node{ "Str", advanceTok().value };
    Node* n = new Node{ "Duration","" };
    n->children.push_back(e);
    n->children.push_back(unit);
    n->line = e->line;
    skipOptionalSemicolon();
    return n;
}

// derivative <expr> [wrt IDENT] (minimal form accepts: derivative <expr>)
static Node* parseDerivative() {
    advanceTok(); // 'derivative'
    Node* e = parseExpression();
    Node* n = new Node{ "Derivative","" };
    n->children.push_back(e);
    // optional: 'wrt' var
    if (peek().type == TokenType::IDENT && peek().value == "wrt") {
        advanceTok();
        if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected identifier after 'wrt' at line " + std::to_string(peek().line));
        Node* var = new Node{ "Var", advanceTok().value };
        n->children.push_back(var);
    }
    n->line = e ? e->line : previous().line;
    skipOptionalSemicolon();
    return n;
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
    if (v == "routine")  return parseRoutine();   // [ADD]
    if (v == "class")    return parseClass();
    if (v == "struct")   return parseStruct();    // [ADD]
    if (v == "splice")   return parseSplice();    // [ADD]
    if (v == "duration") return parseDuration();  // [ADD]
    if (v == "derivative") return parseDerivative(); // [ADD]
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

    advanceTok();
    return new Node{ "Unknown", v };
}

static void emitNode(Node* n, std::ostringstream& out) {
    if (n->type == "Program") {
        emitPrelude(out);
        for (auto* c : n->children) {
            if (c->type == "Class" || c->type == "Struct" || c->type == "Fn" || c->type == "Routine") emitNode(c, out);
        }
        out << "int main(){\n";
        if (!gSourcePath.empty()) { out << "#line 1 \"" << gSourcePath << "\"\n"; }
        for (auto* c : n->children) {
            if (c->type != "Fn" && c->type != "Routine" && c->type != "Class" && c->type != "Struct") emitNode(c, out);
        }
        out << "return 0;\n}\n";
    }
    else if (n->type == "Class" || n->type == "Struct") {
        const bool isStruct = (n->type == "Struct");
        out << (isStruct ? "struct " : "class ") << n->value;

        // bases only for class
        if (!isStruct) {
            Node* bases = nullptr;
            for (auto* ch : n->children) if (ch->type == "Bases") { bases = ch; break; }
            if (bases && !bases->children.empty()) {
                out << " : ";
                for (size_t i = 0; i < bases->children.size(); ++i) {
                    if (i) out << ", ";
                    out << "public " << bases->children[i]->value;
                }
            }
        }
        out << " {\n";
        std::string lastAcc;
        for (auto* ch : n->children) {
            if (ch->type != "Access") continue;
            std::string acc = ch->value;
            if (acc != lastAcc) { out << acc << ":\n"; lastAcc = acc; }
            if (!ch->children.empty()) {
                Node* mem = ch->children[0];
                if (mem->type == "Field") {
                    Node* init = (!mem->children.empty() ? mem->children[0] : nullptr);
                    std::string ty = inferFieldCppType(init);
                    out << ty << " " << mem->value;
                    if (init) out << " = " << emitExpr(init);
                    out << ";\n";
                }
                else if (mem->type == "Method") {
                    // overlays, params, body, return type
                    std::vector<std::string> overlays;
                    Node* params = nullptr; Node* body = nullptr; std::string ret = "auto";
                    for (auto* c2 : mem->children) {
                        if (c2->type == "Overlay") overlays.push_back(c2->value);
                        else if (c2->type == "Params") params = c2;
                        else if (c2->type == "Body") body = c2;
                        else if (c2->type == "ReturnType") ret = c2->value; // [USE]
                    }
                    out << mapTypeToCpp(ret) << " " << mem->value << "(";
                    if (params) {
                        for (size_t i = 0; i < params->children.size(); ++i) {
                            if (i) out << ",";
                            Node* p = params->children[i];
                            std::string name = p->value;
                            std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                            out << mapTypeToCpp(ty) << " " << name;
                        }
                    }
                    out << ")";
                    out << "{\n";
                    for (const auto& ov : overlays) out << "/* overlay: " << ov << " */\n";
                    if (body) emitChildren(body->children, out);
                    out << "}\n";
                }
                else {
                    emitNode(mem, out);
                }
            }
        }
        out << "};\n";
    }
    else if (n->type == "Routine" || n->type == "Fn") {
        std::vector<std::string> overlays;
        Node* params = nullptr; Node* body = nullptr; std::string ret = "auto";
        for (auto* ch : n->children) {
            if (ch->type == "Overlay") overlays.push_back(ch->value);
            else if (ch->type == "Params") params = ch;
            else if (ch->type == "Body") body = ch;
            else if (ch->type == "ReturnType") ret = ch->value; // [USE]
        }
        out << mapTypeToCpp(ret) << " " << n->value << "(";
        if (params) {
            for (size_t i = 0; i < params->children.size(); ++i) {
                if (i) out << ",";
                Node* p = params->children[i];
                std::string name = p->value;
                std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                out << mapTypeToCpp(ty) << " " << name;
            }
        }
        out << ")";
        out << "{\n";
        for (const auto& ov : overlays) out << "/* overlay: " << ov << " */\n";
        if (body) emitChildren(body->children, out);
        out << "}\n";
    }
    else if (n->type == "Splice") {
        // raw C++ injection
        out << n->value << "\n";
    }
    else if (n->type == "Duration") {
        // Placeholder emission; extend as needed to bind to variables
        out << "/* duration(" << emitExpr(n->children[0]) << ", \"" << escapeCppString(n->children[1]->value) << "\") */\n";
    }
    else if (n->type == "Derivative") {
        out << "/* derivative(" << emitExpr(n->children[0]) << (n->children.size() > 1 ? (", wrt " + n->children[1]->value) : "") << ") */\n";
    }
    else if (n->type == "Print") {
        if (!n->children.empty()) { emitPrintChain(out, n->children[0]); }
        else { out << "std::cout << \"" << escapeCppString(n->value) << "\" << std::endl;\n"; }
    }
    // ... keep existing branches for other node types ...
}

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
    ProcResult pr{ 0,"" };
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
    }
    else if (cc == "cl") {
        std::ostringstream clir;
        clir << "cl /EHsc /nologo /DNDEBUG /openmp "
            << "/O" << (cfg.opt.empty() ? "2" : cfg.opt.substr(1)) << " "
            << "/std:" << (cfg.stdver.empty() ? "c++14" : cfg.stdver) << " "
            << quote(sourceCpp) << " /Fe" << quote(outExe);
        pr = runProcessCapture(clir.str());
    }
    else {
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
        }
        else if (a.rfind("--std=", 0) == 0) {
            gBuild.stdver = a.substr(6);
        }
        else if (a.rfind("--opt=", 0) == 0) {
            gBuild.opt = a.substr(6);
        }
        else if (a.rfind("--cc=", 0) == 0) {
            gBuild.cc = a.substr(5);
        }
        else if (a == "--tag" && i + 1 < argc) {
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
            }
            else {
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
    ProcResult pr{ 0,"" };
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
    }
    else if (cc == "cl") {
        std::ostringstream clir;
        clir << "cl /EHsc /nologo /DNDEBUG /openmp "
            << "/O" << (cfg.opt.empty() ? "2" : cfg.opt.substr(1)) << " "
            << "/std:" << (cfg.stdver.empty() ? "c++14" : cfg.stdver) << " "
            << quote(sourceCpp) << " /Fe" << quote(outExe);
        pr = runProcessCapture(clir.str());
    }
    else {
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

// [ADD] ------- Helpers used by emitter and parser (C++14-safe) -------

static std::string escapeCppString(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out.push_back(c); break;
        }
    }
    return out;
}

static bool isNumberLiteral(const std::string& s) {
    if (s.empty()) return false;
    bool hasDigit = false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            if (c >= '0' && c <= '9') hasDigit = true;
            continue;
        }
        return false;
    }
    return hasDigit;
}

// Map CASE I/O mode strings to std::ios flags
static const char* toIosMode(const std::string& mode) {
    std::string m = mode;
    std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (m == "in")  return "std::ios::in";
    if (m == "out") return "std::ios::out";
    if (m == "app") return "std::ios::app";
    if (m == "bin" || m == "binary") return "std::ios::binary";
    // default to output
    return "std::ios::out";
}

// Infer a C++ type for class/struct field from initializer
static std::string inferFieldCppType(Node* init) {
    if (!init) return "double";
    if (init->type == "Str") return "std::string";
    if (init->type == "Num") {
        if (init->value.find_first_of(".eE") != std::string::npos) return "double";
        return "int";
    }
    return "double";
}

// Forward decl for emitExpr
static std::string emitExpr(Node* e);

// Emit a list of statements
static void emitChildren(const std::vector<Node*>& kids, std::ostringstream& out) {
    for (auto* c : kids) if (c) emitNode(c, out);
}

// Minimal print chain (can be extended to multi-arg later)
static void emitPrintChain(std::ostringstream& out, Node* e) {
    out << "std::cout << " << emitExpr(e) << " << std::endl;\n";
}

// Expression codegen
static std::string emitExpr(Node* e) {
    if (!e) return "/*null*/0";
    if (e->type == "Num") return e->value;
    if (e->type == "Str") return std::string("\"") + escapeCppString(e->value) + "\"";
    if (e->type == "Var") return e->value;
    if (e->type == "Unary") {
        return "(" + e->value + "(" + emitExpr(e->children.empty() ? nullptr : e->children[0]) + "))";
    }
    if (e->type == "BinOp") {
        std::string L = emitExpr(e->children[0]);
        std::string R = emitExpr(e->children[1]);
        return "(" + L + " " + e->value + " " + R + ")";
    }
    if (e->type == "Ternary") {
        // children: cond, then, else
        if (e->children.size() >= 3) {
            return "(" + emitExpr(e->children[0]) + " ? " + emitExpr(e->children[1]) + " : " + emitExpr(e->children[2]) + ")";
        }
    }
    if (e->type == "Member") {
        // children[0]=base, value=member
        return "(" + emitExpr(e->children[0]) + "." + e->value + ")";
    }
    if (e->type == "Index") {
        // children[0]=base, children[1]=idx
        return "(" + emitExpr(e->children[0]) + "[" + emitExpr(e->children[1]) + "])";
    }
    if (e->type == "Invoke") {
        // children[0]=target expr, rest args
        std::string s = emitExpr(e->children[0]) + "(";
        for (size_t i = 1; i < e->children.size(); ++i) {
            if (i > 1) s += ", ";
            s += emitExpr(e->children[i]);
        }
        s += ")";
        return s;
    }
    if (e->type == "CallExpr" || e->type == "Call") {
        std::string s = e->value + "(";
        for (size_t i = 0; i < e->children.size(); ++i) {
            if (i) s += ", ";
            s += emitExpr(e->children[i]);
        }
        s += ")";
        return s;
    }
    // Fallback to value if any
    if (!e->value.empty()) return e->value;
    return "0";
}

// Build prelude with includes and channel
static void emitPrelude(std::ostringstream& out) {
    out << "#include <iostream>\n";
    out << "#include <fstream>\n";
    out << "#include <algorithm>\n";
    out << "#include <vector>\n";
    out << "#include <deque>\n";
    out << "#include <mutex>\n";
    out << "#include <condition_variable>\n";
    out << "#include <chrono>\n";
    out << "#include <atomic>\n";
    out << "#include <cmath>\n";
    out << "template<typename T>\n";
    out << "struct Channel {\n";
    out << "  std::mutex m; std::condition_variable cv; std::deque<T> q;\n";
    out << "  void send(const T& v){ std::lock_guard<std::mutex> lk(m); q.push_back(v); cv.notify_one(); }\n";
    out << "  void recv(T& out){ std::unique_lock<std::mutex> lk(m); cv.wait(lk,[&]{return !q.empty();}); out = std::move(q.front()); q.pop_front(); }\n";
    out << "};\n\n";
}

// [ADD] ------- Parser helpers for assignments/inc/dec and input -------

static Node* parseAssignmentOrIncDec() {
    size_t save = pos;
    // lvalue start
    if (!(peek().type == TokenType::IDENT)) return nullptr;

    // Parse lvalue (supports member/index chains)
    Node* lhs = nullptr;
    {
        // Reuse parseLValue path without consuming beyond assignment operators
        // We duplicate minimal logic here to avoid reentrancy issues.
        Node* base = new Node{ "Var", advanceTok().value };
        while (true) {
            if (checkValue("[")) {
                advanceTok();
                Node* idx = parseExpression();
                if (!matchValue("]")) { pos = save; return nullptr; }
                Node* idxNode = new Node{ "Index","" };
                idxNode->children.push_back(base);
                idxNode->children.push_back(idx);
                base = idxNode;
                continue;
            }
            if (checkValue(".")) {
                advanceTok();
                if (!checkType(TokenType::IDENT)) { pos = save; return nullptr; }
                std::string member = advanceTok().value;
                Node* memNode = new Node{ "Member", member };
                memNode->children.push_back(base);
                base = memNode;
                continue;
            }
            break;
        }
        lhs = base;
    }

    // Operators: =, +=, -=, *=, /=, %=, ++, --
    if (peek().type == TokenType::SYMBOL) {
        std::string op = peek().value;
        if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=") {
            advanceTok(); // consume op
            Node* rhs = parseExpression();
            if (!rhs) { pos = save; return nullptr; }
            skipOptionalSemicolon();
            Node* asn = new Node{ "Assign", op };
            asn->children.push_back(lhs);
            asn->children.push_back(rhs);
            return asn;
        }
        if (op == "++" || op == "--") {
            // Postfix inc/dec on lvalue
            advanceTok();
            skipOptionalSemicolon();
            Node* one = new Node{ "Num","1" };
            std::string aop = (op == "++") ? "+=" : "-=";
            Node* asn = new Node{ "Assign", aop };
            asn->children.push_back(lhs);
            asn->children.push_back(one);
            return asn;
        }
    }

    // Not an assignment; revert
    pos = save;
    return nullptr;
}

static Node* parseInput() {
    // input <ident>
    advanceTok(); // 'input'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected identifier after 'input' at line " + std::to_string(peek().line));
    Node* n = new Node{ "Input", advanceTok().value };
    skipOptionalSemicolon();
    return n;
}

// [REPLACE] Emit function: complete all branches (no placeholders left)
static void emitNode(Node* n, std::ostringstream& out) {
    if (!n) return;

    if (n->type == "Program") {
        emitPrelude(out);
        // Decls first
        for (auto* c : n->children) {
            if (c->type == "Class" || c->type == "Struct" || c->type == "Fn" || c->type == "Routine") emitNode(c, out);
        }
        out << "int main(){\n";
        if (!gSourcePath.empty()) { out << "#line 1 \"" << gSourcePath << "\"\n"; }
        for (auto* c : n->children) {
            if (c->type != "Fn" && c->type != "Routine" && c->type != "Class" && c->type != "Struct") emitNode(c, out);
        }
        out << "return 0;\n}\n";
        return;
    }

    if (n->type == "Class" || n->type == "Struct") {
        const bool isStruct = (n->type == "Struct");
        out << (isStruct ? "struct " : "class ") << n->value;

        if (!isStruct) {
            Node* bases = nullptr;
            for (auto* ch : n->children) if (ch->type == "Bases") { bases = ch; break; }
            if (bases && !bases->children.empty()) {
                out << " : ";
                for (size_t i = 0; i < bases->children.size(); ++i) {
                    if (i) out << ", ";
                    out << "public " << bases->children[i]->value;
                }
            }
        }
        out << " {\n";
        std::string lastAcc;
        for (auto* ch : n->children) {
            if (ch->type != "Access") continue;
            std::string acc = ch->value;
            if (acc != lastAcc) { out << acc << ":\n"; lastAcc = acc; }
            if (!ch->children.empty()) {
                Node* mem = ch->children[0];
                if (mem->type == "Field") {
                    Node* init = (!mem->children.empty() ? mem->children[0] : nullptr);
                    std::string ty = inferFieldCppType(init);
                    out << ty << " " << mem->value;
                    if (init) out << " = " << emitExpr(init);
                    out << ";\n";
                }
                else if (mem->type == "Method") {
                    std::vector<std::string> overlays;
                    Node* params = nullptr; Node* body = nullptr; std::string ret = "auto";
                    for (auto* c2 : mem->children) {
                        if (c2->type == "Overlay") overlays.push_back(c2->value);
                        else if (c2->type == "Params") params = c2;
                        else if (c2->type == "Body") body = c2;
                        else if (c2->type == "ReturnType") ret = c2->value;
                    }
                    out << mapTypeToCpp(ret) << " " << mem->value << "(";
                    if (params) {
                        for (size_t i = 0; i < params->children.size(); ++i) {
                            if (i) out << ",";
                            Node* p = params->children[i];
                            std::string name = p->value;
                            std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                            out << mapTypeToCpp(ty) << " " << name;
                        }
                    }
                    out << "){\n";
                    for (const auto& ov : overlays) out << "/* overlay: " << ov << " */\n";
                    if (body) emitChildren(body->children, out);
                    out << "}\n";
                }
                else {
                    emitNode(mem, out);
                }
            }
        }
        out << "};\n";
        return;
    }

    if (n->type == "Routine" || n->type == "Fn") {
        std::vector<std::string> overlays;
        Node* params = nullptr; Node* body = nullptr; std::string ret = "auto";
        for (auto* ch : n->children) {
            if (ch->type == "Overlay") overlays.push_back(ch->value);
            else if (ch->type == "Params") params = ch;
            else if (ch->type == "Body") body = ch;
            else if (ch->type == "ReturnType") ret = ch->value;
        }
        out << mapTypeToCpp(ret) << " " << n->value << "(";
        if (params) {
            for (size_t i = 0; i < params->children.size(); ++i) {
                if (i) out << ",";
                Node* p = params->children[i];
                std::string name = p->value;
                std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                out << mapTypeToCpp(ty) << " " << name;
            }
        }
        out << "){\n";
        for (const auto& ov : overlays) out << "/* overlay: " << ov << " */\n";
        if (body) emitChildren(body->children, out);
        out << "}\n";
        return;
    }

    if (n->type == "Splice") {
        out << n->value << "\n";
        return;
    }

    if (n->type == "Duration") {
        // children[0]=expr, children[1]=unit string
        std::string unit = n->children.size() > 1 ? n->children[1]->value : "ms";
        std::string u = unit; std::transform(u.begin(), u.end(), u.begin(), ::tolower);
        const char* chronoUnit =
            (u == "ns" || u == "nanoseconds") ? "std::chrono::nanoseconds" :
            (u == "us" || u == "microseconds") ? "std::chrono::microseconds" :
            (u == "ms" || u == "milliseconds") ? "std::chrono::milliseconds" :
            (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
            (u == "m" || u == "min" || u == "minutes") ? "std::chrono::minutes" :
            (u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "std::chrono::milliseconds";
        out << "{ using namespace std::chrono; auto __dur_val = " << emitExpr(n->children[0]) << "; "
            << chronoUnit << " __dur = duration_cast<" << chronoUnit << ">(duration<double>(__dur_val)); "
            << "(void)__dur; }\n";
        return;
    }

    if (n->type == "Derivative") {
        // children[0]=expr, optional children[1]=Var wrt
        if (n->children.size() >= 2 && n->children[1]->type == "Var") {
            const std::string var = n->children[1]->value;
            const std::string f = emitExpr(n->children[0]);
            out << "{ auto __eps = 1e-6; auto __orig = (" << var << "); "
                "auto __f0 = (double)(" << f << "); "
                << var << " = __orig + __eps; "
                "auto __f1 = (double)(" << f << "); "
                << var << " = __orig; "
                "double __deriv = (__f1 - __f0) / __eps; (void)__deriv; }\n";
        }
        else {
            // No wrt variable; compute nothing but keep valid code
            out << "{ /* derivative: missing 'wrt' variable */ double __deriv = 0.0; (void)__deriv; }\n";
        }
        return;
    }

    if (n->type == "Print") {
        if (!n->children.empty()) emitPrintChain(out, n->children[0]);
        else out << "std::cout << \"" << escapeCppString(n->value) << "\" << std::endl;\n";
        return;
    }

    if (n->type == "Let") {
        // auto var = expr;
        out << "auto " << n->value << " = " << (n->children.empty() ? "0" : emitExpr(n->children[0])) << ";\n";
        return;
    }

    if (n->type == "Assign") {
        // children[0]=lhs, children[1]=rhs, value=op
        std::string lhs = emitExpr(n->children[0]);
        if (n->children.size() > 1) {
            std::string rhs = emitExpr(n->children[1]);
            out << lhs << " " << n->value << " " << rhs << ";\n";
        }
        else {
            // Defensive
            out << "/* invalid assign */\n";
        }
        return;
    }

    if (n->type == "If") {
        Node* cond = nullptr; Node* ifBody = nullptr; Node* elseBody = nullptr;
        for (auto* c : n->children) {
            if (c->type == "Cond") cond = c;
            else if (c->type == "Body") ifBody = (ifBody ? ifBody : c);
            else if (c->type == "Else") elseBody = c;
        }
        out << "if (" << (cond && !cond->children.empty() ? emitExpr(cond->children[0]) : "0") << ") {\n";
        if (ifBody) emitChildren(ifBody->children, out);
        out << "}";
        if (elseBody) {
            out << " else {\n";
            emitChildren(elseBody->children, out);
            out << "}";
        }
        out << "\n";
        return;
    }

    if (n->type == "While") {
        Node* cond = (!n->children.empty() ? n->children[0] : nullptr);
        Node* body = (n->children.size() > 1 ? n->children[1] : nullptr);
        // cond is "Cond"
        out << "while (" << (cond && !cond->children.empty() ? emitExpr(cond->children[0]) : "0") << ") {\n";
        if (body) emitChildren(body->children, out);
        out << "}\n";
        return;
    }

    if (n->type == "Loop") {
        // value holds raw for(...) header if provided by user
        std::string hdr = n->value;
        // Accept 'i=0; i<N; i++' or full 'for(...)' header
        if (hdr.find("for") == std::string::npos) {
            out << "for (" << hdr << ") {\n";
        }
        else {
            out << hdr << " {\n";
        }
        if (!n->children.empty()) emitChildren(n->children[0]->children, out);
        out << "}\n";
        return;
    }

    if (n->type == "Break") { out << "break;\n"; return; }
    if (n->type == "Continue") { out << "continue;\n"; return; }

    if (n->type == "Switch") {
        Node* cond = (!n->children.empty() ? n->children[0] : nullptr);
        out << "switch (" << (cond && !cond->children.empty() ? emitExpr(cond->children[0]) : "0") << ") {\n";
        for (size_t i = 1; i < n->children.size(); ++i) {
            Node* c = n->children[i];
            if (c->type == "Case") {
                const std::string& v = c->value;
                if (isNumberLiteral(v)) out << "case " << v << ":\n";
                else out << "case /*id*/ " << v << ":\n";
                if (!c->children.empty()) { emitChildren(c->children[0]->children, out); }
                out << "break;\n";
            }
            else if (c->type == "Default") {
                out << "default:\n";
                if (!c->children.empty()) { emitChildren(c->children[0]->children, out); }
                out << "break;\n";
            }
        }
        out << "}\n";
        return;
    }

    if (n->type == "Open") {
        std::string var = n->value;
        std::string path = (n->children.size() > 0 ? n->children[0]->value : "");
        std::string mode = (n->children.size() > 1 ? n->children[1]->value : "out");
        out << "std::fstream " << var << "(\"" << escapeCppString(path) << "\", " << toIosMode(mode) << ");\n";
        return;
    }
    if (n->type == "Write") {
        std::string var = n->value;
        std::string e = (n->children.empty() ? "\"\"" : emitExpr(n->children[0]));
        out << var << " << " << e << ";\n";
        return;
    }
    if (n->type == "Writeln") {
        std::string var = n->value;
        std::string e = (n->children.empty() ? "\"\"" : emitExpr(n->children[0]));
        out << var << " << " << e << " << std::endl;\n";
        return;
    }
    if (n->type == "Read") {
        std::string var = n->value;
        std::string target = (!n->children.empty() ? n->children[0]->value : "");
        out << var << " >> " << target << ";\n";
        return;
    }
    if (n->type == "Close") {
        out << n->value << ".close();\n";
        return;
    }

    if (n->type == "Scale") {
        // children: [Var x, a, b, c, d]
        if (n->children.size() == 5 && n->children[0]->type == "Var") {
            std::string x = n->children[0]->value;
            std::string a = emitExpr(n->children[1]);
            std::string b = emitExpr(n->children[2]);
            std::string c = emitExpr(n->children[3]);
            std::string d = emitExpr(n->children[4]);
            out << x << " = ((" << x << " - " << a << ")/(" << b << " - " << a << "))*(" << d << " - " << c << ") + " << c << ";\n";
        }
        return;
    }

    if (n->type == "Bounds") {
        // children: [Var x, min, max]
        if (n->children.size() == 3 && n->children[0]->type == "Var") {
            std::string x = n->children[0]->value;
            std::string mn = emitExpr(n->children[1]);
            std::string mx = emitExpr(n->children[2]);
            out << x << " = std::min(std::max(" << x << ", " << mn << "), " << mx << ");\n";
        }
        return;
    }

    if (n->type == "Checkpoint") {
        out << "std::cerr << \"[checkpoint] " << escapeCppString(n->value) << "\\n\";\n";
        return;
    }
    if (n->type == "VBreak") {
        out << "std::cerr << \"[vbreak] " << escapeCppString(n->value) << "\\n\";\n";
        return;
    }

    if (n->type == "Channel") {
        // value=name, children[0]=Str type
        std::string nm = n->value;
        std::string ty = (n->children.size() ? mapTypeToCpp(n->children[0]->value) : "double");
        out << "Channel<" << ty << "> " << nm << ";\n";
        return;
    }
    if (n->type == "Send") {
        out << n->value << ".send(" << emitExpr(n->children[0]) << ");\n";
        return;
    }
    if (n->type == "Recv") {
        // children[0]=Var target
        out << n->value << ".recv(" << (n->children.empty() ? "/*?*/x" : n->children[0]->value) << ");\n";
        return;
    }

    if (n->type == "Schedule") {
        // children[0]=Body, value=priority (unused)
        if (!n->children.empty()) {
            out << "{ /* schedule priority=" << escapeCppString(n->value) << " */\n";
            emitChildren(n->children[0]->children, out);
            out << "}\n";
        }
        return;
    }

    if (n->type == "Sync") {
        out << "std::atomic_thread_fence(std::memory_order_seq_cst);\n";
        return;
    }

    if (n->type == "Call") {
        out << emitExpr(n) << ";\n";
        return;
    }

    if (n->type == "Ret") {
        if (!n->children.empty()) out << "return " << emitExpr(n->children[0]) << ";\n";
        else out << "return;\n";
        return;
    }

    if (n->type == "Mutate") {
        out << "/* mutate " << escapeCppString(n->value) << " */\n";
        return;
    }

    // Unknown/fallback
    out << "/* Unknown: " << n->type << " " << escapeCppString(n->value) << " */\n";
}

// [REPLACE] Map CASE types to C++ types
static std::string mapTypeToCpp(const std::string& ty) {
    if (ty == "int") return "int";
    if (ty == "float" || ty == "double") return "double";
    if (ty == "str" || ty == "string") return "std::string";
    if (ty == "bool") return "bool";
    // Default to double
    return "double";
}

// [ADD] ------- Additional global metadata storage -------
static std::map<std::string, std::string> gMeta;
static void setMetadata(const std::string& key, const std::string& value) {
    gMeta[key] = value;
}
static std::string getMetadataJson() {
    std::ostringstream os;
    os << "{";
    bool first = true;
    for (const auto& kv : gMeta) {
        if (!first) os << ", ";
		os << "\"" << kv.first << "\": \"" << kv.second << "\"";
        first = false;
    }
    os << "}";
	return os.str();
}

// [REPLACE] runProcessCapture to capture stdout/stderr
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
	while (size_t n = fread(buf, 1, sizeof(buf), pipe)) out.append(buf, n);
    int rc = pclose(pipe);
    return { rc, out };
#endif
}

struct OverlayContext {
    std::string func;
    std::string srcFile;
    int line;
    std::string timeUTC;
    std::string dateUTC;
};

// [REPLACE] Emit standard header with metadata banner
static void emitStandardHeader(std::ostringstream& out) {
	std::string meta = getMetadataJson();

    void Function::emit(std::ostream & o, int depth, const OverlayContext & ctx) {
        o << "auto " << name << "(" << params << "){\n";
        if (!overlayTag.empty() && overlays.count(overlayTag)) {
            std::string code = overlays[overlayTag].injectCode;
            std::unordered_map<std::string, std::string> sub{
                {"${FUNC}",ctx.func},
                {"${FILE}",ctx.srcFile},
                {"${LINE}",std::to_string(ctx.line)},
                {"${TIME}",ctx.timeUTC}
            };
            for (auto& [k, v] : sub) {
                size_t p; while ((p = code.find(k)) != std::string::npos) code.replace(p, k.size(), v);
            }
            o << "    // Overlay: " << overlayTag << "\n" << code << "\n";
        }
        for (auto& s : body) s->emit(o, 4);
        o << "}\n\n";
    }

	out <<
		"//
		// Generated by CASE Compiler\n"

#include <filesystem>
#include <chrono>
#include <thread>
        bool watchAndRebuild(const std::vector<std::string>& files, int intervalSec) {
        using clock = std::filesystem::file_time_type;
        std::unordered_map<std::string, clock> last;
        for (auto& f : files) if (std::filesystem::exists(f))
            last[f] = std::filesystem::last_write_time(f);

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
            bool changed = false;
            for (auto& f : files) {
                if (!std::filesystem::exists(f)) continue;
                auto now = std::filesystem::last_write_time(f);
                if (!last.count(f) || now != last[f]) { changed = true; last[f] = now; }
            }
            if (changed) {
                std::cout << color::yellow << "🔄  Change detected — rebuilding…" << color::reset << "\n";
                return true; // signal caller to rebuild once, then exit
            }
        }
    }

	"// Metadata: " << meta << "\n"
		"// ------------------------\n\n";
}
}
static bool compileCppSource(const std::string& sourceCpp, const std::string& outExe, const Config& cfg) {
    #if defined(_WIN32)
    std::string cc = cfg.cc.empty() ? "cl" : cfg.cc;
    ProcResult pr;
    if (cc == "g++" || cc == "gcc") {
        std::ostringstream gcir;
        gcir << "g++ " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
            << " -fopenmp -DNDEBUG " << quote(sourceCpp) << " -o " << quote(outExe);
        pr = runProcessCapture(gcir.str());
        if (pr.exitCode != 0) {
            // Fallback to clang++
            std::ostringstream cc2;
            cc2 << "clang++ " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
                << " -fopenmp -DNDEBUG " << quote(sourceCpp) << " -o " << quote(outExe);
            pr = runProcessCapture(cc2.str());
        }
    }
    else if (cc == "clang++") {
        std::ostringstream clir;
        clir << "clang++ " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
			<< " -fopenmp -DNDEBUG "
            << quote(sourceCpp) << " -o " << quote(outExe);
        pr = runProcessCapture(clir.str());
	}
    else {
        std::ostringstream any;
		any << cc << " " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
            << " -fopenmp -DNDEBUG "
			<< quote(sourceCpp) << " -o " << quote(outExe);
        pr = runProcessCapture(any.str());
    }
	std::cout << pr.output;
    return pr.exitCode == 0;
    #else
	std::string cc = cfg.cc.empty() ? "g++" : cfg.cc;
    std::ostringstream cmd;
	cmd << cc << " " << fmtStd() << " -" << (cfg.opt.empty() ? "O2" : cfg.opt)
		<< " -fopenmp -DNDEBUG "
		<< quote(sourceCpp) << " -o " << quote(outExe);
	ProcResult pr = runProcessCapture(cmd.str());
	std::cout << pr.output;
	return pr.exitCode == 0;
#endif
}
static std::string fmtStd() {
    #if defined(__cplusplus)
    #if __cplusplus >= 202002L
        return "-std=c++20";
    #elif __cplusplus >= 201703L
        return "-std=c++17";
    #elif __cplusplus >= 201402L
        return "-std=c++14";
    #elif __cplusplus >= 201103L
        return "-std=c++11";
    #else
        return "-std=c++98";
    #endif
    #else
        return "-std=c++11";
    #endif
}
static std::string escapeCppString(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (isprint(static_cast<unsigned char>(c))) {
                    out += c;
                }
                else {
                    char buf[5];
                    snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
                    out += buf;
                }
				break;
                }
	}
	return out;
}
static bool isNumberLiteral(const std::string& s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[i] == '-' || s[i] == '+') ++i;
    bool hasDigits = false;
    bool hasDot = false;
    for (; i < s.size(); ++i) {
        if (isdigit(static_cast<unsigned char>(s[i]))) {
            hasDigits = true;
        }
        else if (s[i] == '.' && !hasDot) {
            hasDot = true;
        }
        else {
            return false;
        }
    }
    return hasDigits;
}
static std::string toIosMode(const std::string& mode) {
    if (mode == "in") return "std::ios::in";
    if (mode == "out") return "std::ios::out";
    if (mode == "app") return "std::ios::app";
    if (mode == "ate") return "std::ios::ate";
    if (mode == "trunc") return "std::ios::trunc";
    if (mode == "binary") return "std::ios::binary";
    // Combine modes if multiple provided separated by '|'
    if (mode.find('|') != std::string::npos) {
        std::istringstream ss(mode);
        std::string token;
        std::string result;
        while (std::getline(ss, token, '|')) {
            if (!result.empty()) result += " | ";
            result += toIosMode(token);
        }
        return result;
    }
	// Fallback
	return "std::ios::out";
}

#include "Transpiler.h"
#include <sstream>
#include <map>
#include <string>
#include <cctype>
#include <stdexcept>
#include <cstdio>
static std::string emitExpr(Node* e) {
	if (!e) return "0";

    static void applyContextSubstitutions(std::string & text,
        const std::string & func,
        const std::string & file,
        int line) {
        const std::unordered_map<std::string, std::string> map{
            {"${FUNC}", func},
            {"${FILE}", file},
            {"${LINE}", std::to_string(line)},
            {"${TIME}", std::format("{:%H:%M:%S}", std::chrono::system_clock::now())}
        };
        for (auto& [k, v] : map) {
            size_t pos;
            while ((pos = text.find(k)) != std::string::npos)
                text.replace(pos, k.size(), v);
        }
    }

    if (e->type == "Num" || e->type == "Str" || e->type == "Bool" || e->type == "Var") {
        return e->value;
	}
    if (e->type == "UnaryOp") {
        // value=op, children[0]=expr
        return "(" + e->value + emitExpr(e->children[0]) + ")";
    }
    if (e->type == "BinaryOp") {
        // value=op, children[0]=left, children[1]=right
        return "(" + emitExpr(e->children[0]) + " " + e->value + " " + emitExpr(e->children[1]) + ")";
    }
    if (e->type == "TernaryOp") {
        // children[0]=cond, children[1]=true expr, children[2]=false expr
        return "(" + emitExpr(e->children[0]) + " ? " + emitExpr(e->children[1]) + " : " + emitExpr(e->children[2]) + ")";
    }
    if (e->type == "Paren") {
        // children[0]=expr
        return "(" + emitExpr(e->children[0]) + ")";
    }
    if (e->type == "DotInvoke") {
        // children[0]=base, value=method, rest args
        std::string s = "(" + emitExpr(e->children[0]) + ")." + e->value + "(";
        for (size_t i = 1; i < e->children.size(); ++i) {
            if (i > 1) s += ", ";
            s += emitExpr(e->children[i]);
        }
        s += ")";
		return s;
        }
    if (e->type == "Call") {
        // value=func, children=args
        std::string s = e->value + "(";
        for (size_t i = 0; i < e->children.size(); ++i) {
            if (i > 0) s += ", ";
            s += emitExpr(e->children[i]);
        }
		s += ")";
        return s;
    }
    if (e->type == "Index") {
        // children[0]=base, children[1]=index
        return emitExpr(e->children[0]) + "[" + emitExpr(e->children[1]) + "]";
    }
    if (e->type == "Member") {
        // children[0]=base
        return emitExpr(e->children[0]) + "." + e->value;
    }
    // Fallback
	return "/*expr:" + e->type + "*/0";
}
static void emitPrelude(std::ostringstream& out) {
    out << "#include <iostream>\n";
    out << "#include <fstream>\n";
    out << "#include <string>\n";
    out << "#include <deque>\n";
    out << "#include <mutex>\n";
    out << "#include <condition_variable>\n";
    out << "#include <atomic>\n";
    out << "#include <algorithm>\n";
    out << "#include <chrono>\n";
    out << "#include <thread>\n\n";
    // Channel class
    out << "template<typename T>\n";
    out << "class Channel {\n";
    out << "  std::deque<T> q;\n";
    out << "  std::mutex m;\n";
    out << "  std::condition_variable cv;\n";
    out << "public:\n";
    out << "  void send(const T& val){ std::lock_guard<std::mutex> lk(m); q.push_back(val); cv.notify_one(); }\n";
	out << "  void send(T&& val){ std::lock_guard<std::mutex> lk(m); q.push_back(std::move(val)); cv.notify_one(); }\n";
    out << "  void recv(T& val){ std::unique_lock<std::mutex> lk(m); cv.wait(lk, [this]{ return !q.empty(); }); val = std::move(q.front()); q.pop_front(); }\n";
	out << "};\n\n";
}
static Node* parseAssignment(Node*& lhs) {
    size_t save = pos;
    // Parse LHS: Var, Member, or Index
    {
        Node* base = nullptr;
        if (peek().type == TokenType::IDENT) {
            base = new Node{ "Var", advanceTok().value };
        }
        else {
            pos = save; return nullptr;
        }
        // Handle Member and Index chains
        while (true) {
            if (peek().type == TokenType::SYMBOL && peek().value == "[") {
                advanceTok(); // consume '['
                Node* indexExpr = parseExpression();
                if (!indexExpr || peek().type != TokenType::SYMBOL || peek().value != "]") {
                    pos = save; return nullptr;
                }
                advanceTok(); // consume ']'
                Node* indexNode = new Node{ "Index", "" };
                indexNode->children.push_back(base);
                indexNode->children.push_back(indexExpr);
                base = indexNode;
                continue;
            }
            if (peek().type == TokenType::SYMBOL && peek().value == ".") {
                advanceTok(); // consume '.'
                if (peek().type != TokenType::IDENT) {
                    pos = save; return nullptr;
				}
                std::string memberName = advanceTok().value;
                Node* memberNode = new Node{ "Member", memberName };
                memberNode->children.push_back(base);
                base = memberNode;
                continue;
            }
            break;
        }
        lhs = base;
	}
    // Parse assignment operator
    if (peek().type == TokenType::SYMBOL &&
        (peek().value == "=" || peek().value == "+=" || peek().value == "-=" ||
         peek().value == "*=" || peek().value == "/=" || peek().value == "%=")) {
        std::string op = advanceTok().value;
        Node* assignNode = new Node{ "Assign", op };
        assignNode->children.push_back(lhs);
        Node* rhs = parseExpression();
        if (!rhs) {
            pos = save; return nullptr;
        }
        assignNode->children.push_back(rhs);
        return assignNode;
    }
	// No assignment found
	pos = save; return nullptr;
}
static void emitNode(Node* n, std::ostringstream& out) {
    if (!n) return;
    if (n->type == "Program") {
        emitPrelude(out);
        for (auto& inc : gIncludes) {
            out << "#include " << inc << "\n";
        }
        out << "\n";
        for (auto* c : n->children) {
            if (c->type == "Splice") emitNode(c, out);
        }
		out << "\n";
        out << "int main() {\n";
		for (auto* c : n->children) {
            if (c->type != "Splice") emitNode(c, out);
		}

		out << "\n";
        out << "    return 0;\n";
		out << "}\n";
        return;
    }
	if (n->type == "Struct" || n->type == "Class") {
		bool isStruct = (n->type == "Struct");
		out << (isStruct ? "struct " : "class ") << n->value;
        // Handle bases
        {
			Node* bases = nullptr;
            for (auto* ch : n->children) {
                if (ch->type == "Bases") { bases = ch; break; }
			}
            if (bases && !bases->children.empty()) {
				out << " : ";
                for (size_t i = 0; i < bases->children.size(); ++i) {
					// Assume public inheritance
                    if (i > 0) out << ", ";
					out << "public " << bases->children[i]->value;
				}
			}
            }
        out << " {\n";
        std::string lastAcc = "";
		for (auto* ch : n->children) {
            // Access specifiers
            if (ch->type == "Access") {
                std::string acc = ch->value;
                if (acc != lastAcc) {
                    out << acc << ":\n";
                    lastAcc = acc;
                }
                continue;
            }
			// Members
            if (ch->type == "Member") {
                Node* mem = ch;
                if (mem->type == "Field") {
                    Node* init = nullptr;
                    for (auto* c2 : mem->children) {
                        if (c2->type == "Init") init = c2;
                    }
                    // Infer type from init expression if available
					// Otherwise default to double
                    std::string ty = "double";
                    if (init && !init->children.empty()) {
                        // Simple heuristic: if init is Num with '.', use double; else int
                        Node* expr = init->children[0];
                        if (expr->type == "Num") {
                            if (expr->value.find('.') != std::string::npos) ty = "double";
                            else ty = "int";
                        }
                    }
                    out << mapTypeToCpp(ty) << " " << mem->value;
                    if (init && !init->children.empty()) {
                        out << " = " << emitExpr(init->children[0]);
                    }
                    out << ";\n";
                }
                else if (mem->type == "Routine" || mem->type == "Fn") {
                    std::vector<std::string> overlays;
                    Node* params = nullptr; Node* body = nullptr; std::string ret = "auto";
                    for (auto* c2 : mem->children) {
                        if (c2->type == "Overlay") overlays.push_back(c2->value);
                        else if (c2->type == "Params") params = c2;
                        else if (c2->type == "Body") body = c2;
                        else if (c2->type == "ReturnType") ret = c2->value;
                    }
                    out << mapTypeToCpp(ret) << " " << mem->value << "(";
                    if (params) {
                        for (size_t i = 0; i < params->children.size(); ++i) {
                            if (i) out << ",";
                            Node* p = params->children[i];
                            std::string name = p->value;
                            std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                            out << mapTypeToCpp(ty) << " " << name;
                        }
                    }
                    out << "){\n";
                    for (const auto& ov : overlays) out << "/* overlay: " << ov << " */\n";
                    if (body) emitChildren(body->children, out);
                    out << "}\n";
                }
                else {
                    emitNode(mem, out);
                }
            }
        }
        out << "};\n";
		return;
        }
    if (n->type == "Routine" || n->type == "Fn") {
        std::vector<std::string> overlays;
        Node* params = nullptr; Node* body = nullptr; std::string ret = "auto";
        for (auto* c : n->children) {
            if (c->type == "Overlay") overlays.push_back(c->value);
            else if (c->type == "Params") params = c;
            else if (c->type == "Body") body = c;
            else if (c->type == "ReturnType") ret = c->value;
        }
        out << mapTypeToCpp(ret) << " " << n->value << "(";
        if (params) {
            for (size_t i = 0; i < params->children.size(); ++i) {
                if (i) out << ",";
                Node* p = params->children[i];
				{
                    std::string name = p->value;
                    std::string ty = (!p->children.empty() ? p->children[0]->value : "auto");
                    out << mapTypeToCpp(ty) << " " << name;
                }
            }
        }
        out << "){\n";
        for (const auto& ov : overlays) out << "/* overlay: " << ov << " */\n";
        if (body) emitChildren(body->children, out);
        out << "}\n\n";
        return;
	}
    if (n->type == "Duration") {
        // children[0]=expr, value=unit
        std::string u = n->value;
        std::string chronoUnit =
            (u == "ms" || u == "msec" || u == "millis" || u == "milliseconds") ? "std::chrono::milliseconds" :
            (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
            (u == "us" || u == "usec" || u == "microseconds") ? "std::chrono::microseconds" :
            (u == "ns" || u == "nsec" || u == "nanoseconds") ? "std::chrono::nanoseconds" :
            (u == "min" || u == "minute" || u == "minutes") ? "std::chrono::minutes" :
            (u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "std::chrono::milliseconds";
        out << "{ using namespace std::chrono; auto __dur_val = " << emitExpr(n->children[0]) << "; "
            << chronoUnit << " __dur = duration_cast<" << chronoUnit << ">(duration<double>(__dur_val)); "
            << "(void)__dur; }\n";
        return;
    }
    if (n->type == "TimeDuration") {
        // children[0]=expr, value=unit
        std::string u = n->value;
        std::string chronoUnit =
            (u == "ms" || u == "msec" || u == "millis" || u == "milliseconds") ? "std::chrono::milliseconds" :
            (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
            (u == "us" || u == "usec" || u == "microseconds") ? "std::chrono::microseconds" :
			(u == "ns" || u == "nsec" || u == "nanoseconds") ? "std::chrono::nanoseconds" :
            (u == "min" || u == "minute" || u == "minutes") ? "std::chrono::minutes" :
			(u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "std::chrono::milliseconds";
        out << "std::this_thread::sleep_for(" << chronoUnit << "(" << emitExpr(n->children[0]) << "));\n";
        return;
    }
	if (n->type == "Assign") {
        // children[0]=LHS, children[1]=RHS
        Node* lhsNode = n->children[0];
        Node* rhsNode = n->children[1];
        std::string lhs = emitExpr(lhsNode);
        std::string rhs = emitExpr(rhsNode);
        if (n->value == "=" || n->value == "+=" || n->value == "-=" ||
            n->value == "*=" || n->value == "/=" || n->value == "%=") {
            out << lhs << " " << n->value << " " << rhs << ";\n";
        }
        else if (n->value == "++" || n->value == "--") {
            out << lhs << n->value << ";\n";
        }
        else if (n->value == "+=1") {
            out << lhs << " += 1;\n";
        }
        else if (n->value == "-=1") {
            out << lhs << " -= 1;\n";
        }
        else if (n->value == "*=2") {
            out << lhs << " *= 2;\n";
        }
        else if (n->value == "/=2") {
            out << lhs << " /= 2;\n";
        }
        else if (n->value == "%=2") {
            out << lhs << " %= 2;\n";
        }
        else if (n->value == "**=") {
            out << lhs << " = pow(" << lhs << ", " << rhs << ");\n";
        }
        else if (n->value == "//=") {
            out << lhs << " = floor(" << lhs << " / " << rhs << ");\n";
        }
        else if (n->value == "&=" || n->value == "|=" || n->value == "^=" ||
			n->value == "<<=" || n->value == ">>=") {
            out << lhs << " " << n->value << " " << rhs << ";\n";
        }
        else {
            out << "/* Unknown assign op: " << n->value << " */\n";
        }
        return;
    }
    if (n->type == "If") {
        // children[0]=cond, children[1]=then, children[2]=else (optional)
        out << "if (" << emitExpr(n->children[0]) << ") {\n";
        emitChildren(n->children[1]->children, out);
        out << "}\n";
        if (n->children.size() > 2) {
            out << "else {\n";
            emitChildren(n->children[2]->children, out);
            out << "}\n";
        }
        return;
    }
    if (n->type == "While") {
        // children[0]=cond, children[1]=body
        out << "while (" << emitExpr(n->children[0]) << ") {\n";
        emitChildren(n->children[1]->children, out);
        out << "}\n";
        return;
    }
    if (n->type == "For") {
        // value=header, children[0]=body
		std::string hdr = n->value;
        out << "for (" << hdr << ") {\n";
        emitChildren(n->children[0]->children, out);
        out << "}\n";
        return;
    }
    if (n->type == "Switch") {
        // children[0]=expr, children[1]=cases
        out << "switch (" << emitExpr(n->children[0]) << ") {\n";
        Node* casesNode = n->children[1];
        for (auto* c : casesNode->children) {
            if (c->type == "Case") {
                std::string v = emitExpr(c->children[0]);
				if (isNumberLiteral(v)) out << "case " << v << ":\n";
                else out << "case " << v << ":\n";
                emitChildren(c->children[1]->children, out);
                out << "    break;\n";
            }
            else if (c->type == "Default") {
                out << "default:\n";
                emitChildren(c->children[0]->children, out);
                out << "    break;\n";
            }
        }
        out << "}\n";
        return;
	}
    if (n->type == "Print") {
		std::string var = n->value;
		std::string e = (n->children.empty() ? "\"\"" : emitExpr(n->children[0]));
        out << var << " << " << e << ";\n";
        return;
	}
    if (n->type == "PrintLn") {
		std::string var = n->value;
		std::string e = (n->children.empty() ? "\"\"" : emitExpr(n->children[0]));
		out << var << " << " << e << " << std::endl;\n";
        return;
    }
    if (n->type == "Input") {
        // children[0]=Var target
		std::string var = n->value;
        std::string target = (n->children.empty() ? "/*?*/x" : n->children[0]->value);
		out << var << " >> " << target << ";\n";
        return;
	}
    if (n->type == "MapRange") {
		// value=unused
		// children: [Var x, a, b, c, d]
		if (n->children.size() == 5 && n->children[0]->type == "Var") {
			std::string x = n->children[0]->value;
			std::string a = emitExpr(n->children[1]);
			std::string b = emitExpr(n->children[2]);
			std::string c = emitExpr(n->children[3]);
			std::string d = emitExpr(n->children[4]);
			out << x << " = ((" << x << " - " << a << ") * (" << d << " - " << c << ") / (" << b << " - " << a << ")) + " << c << ";\n";
		}
        return;
    }
    if (n->type == "Clamp") {
		// value=unused
		// children: [Var x, min, max]
		if (n->children.size() == 3 && n->children[0]->type == "Var") {
            std::string x = n->children[0]->value;
			std::string mn = emitExpr(n->children[1]);
			std::string mx = emitExpr(n->children[2]);
			out << x << " = std::min(std::max(" << x << ", " << mn << "), " << mx << ");\n";
            }
        return;
    }
    if (n->type == "Debug") {
		out << "std::cerr << \"[debug] " << escapeCppString(n->value) << "\\n\";\n";

        bool watchAndRebuild(const std::vector<std::string>& files,
            std::chrono::seconds poll, std::chrono::seconds debounce) {
            namespace fs = std::filesystem;
            std::unordered_map<std::string, fs::file_time_type> last;
            auto lastTrigger = std::chrono::steady_clock::now();

            for (auto& f : files)
                if (fs::exists(f)) last[f] = fs::last_write_time(f);

            while (true) {
                std::this_thread::sleep_for(poll);
                bool changed = false;
                for (auto& f : files) {
                    if (!fs::exists(f)) continue;
                    auto t = fs::last_write_time(f);
                    if (t != last[f]) { changed = true; last[f] = t; }
                }
                auto now = std::chrono::steady_clock::now();
                if (changed && now - lastTrigger > debounce) {
                    lastTrigger = now;
                    std::cout << color::yellow << "🔄 change detected\n" << color::reset;
                    return true;
                }
            }
        }

		return;
        }
    if (n->type == "Parallel") {
		// children[0]=body

#include <openssl/sha.h>   // or any portable hash lib
#include <iomanip>

        static std::string fileChecksum(const std::string& path) {
            std::ifstream f(path, std::ios::binary);
            if (!f) return "";
            std::ostringstream ss; unsigned char buf[SHA256_DIGEST_LENGTH];
            SHA256_CTX ctx; SHA256_Init(&ctx);
            std::array<char, 8192> block;
            while (f.read(block.data(), block.size()) || f.gcount())
                SHA256_Update(&ctx, block.data(), f.gcount());
            SHA256_Final(buf, &ctx);
            for (unsigned char c : buf) ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
            return ss.str();
        }

		out << "{ std::vector<std::thread> __threads;\n";
		out << "  std::mutex __mtx;\n";
		out << "  auto __worker = [&__mtx]() {\n";
		emitChildren(n->children[0]->children, out);
		out << "  };\n";
		out << "  const int __numThreads = std::thread::hardware_concurrency();\n";
		out << "  for (int __i = 0; __i < __numThreads; ++__i) {\n";
		out << "    __threads.emplace_back(__worker);\n";
		out << "  }\n";
		out << "  for (auto& t : __threads) t.join();\n";
        out << "}\n";
        return;
    }
    // Fallback
	out << "/* unhandled node type: " << n->type << " */\n";
	}
#include <unordered_map>
	static std::unordered_map<std::string, std::string> gMeta;
    static std::string getMetadataJson() {
		std::ostringstream os;
        os << "{";
    bool first = true;
	for (const auto& kv : gMeta) {
        if (!first) os << ",";
		os << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    os << "}";
	return os.str();
	}

    void writeManifest(const std::vector<std::string>& built) {
        std::filesystem::create_directories("build");
        std::ofstream j("build/manifest.json");
        j << "{\n  \"artifacts\": [\n";
        for (size_t i = 0; i < built.size(); ++i) {
            const auto& f = built[i];
            auto cs = fileChecksum(f);
            auto sz = std::filesystem::file_size(f);
            j << "    {\"file\":\"" << f << "\",\"size\":" << sz << ",\"checksum\":\"" << cs << "\"}";
            if (i + 1 < built.size()) j << ",";
            j << "\n";
        }
        j << "  ]\n}\n";
    }

	return os.str();
	}
    #if defined(_WIN32)
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out.append(buf);
    int rc = _pclose(pipe);
	return { rc, out };
    #else
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) out.append(buf);
	int rc = pclose(pipe);
    return { rc, out };
#endif
	}
	}
	}
	out << "auto " << name << "(" << params << ") {\n";
	for (auto& s : body) s->emit(out, 4);
	out << "}\n\n";
    void emit(std::ostringstream& o, const std::string& name,
                  const std::string& params,
                  const std::vector<Stmt*>& body,
                  const std::string& overlayTag,
		const EmitContext& ctx) override {
        o << "auto " << name << "(" << params << ") {\n";
        if (!overlayTag.empty()) {
            std::string code = gOverlays[overlayTag];
			std::unordered_map<std::string, std::string> sub = {
                "${FUNC}", name},
                {"${FILE}", ctx.file},
                {"${LINE}", std::to_string(ctx.line)},
                {"${TIME}", std::format("{:%H:%M:%S}", std::chrono::system_clock::now())}
            };
		for (auto& [k, v] : sub) {
            size_t pos;
            while ((pos = code.find(k)) != std::string::npos)
                code.replace(pos, k.size(), v);
        }
            o << code << "\n";
	}
	for (auto& s : body) s->emit(o, 4, ctx);
    o << "}\n\n";
	}
    static bool watchFilesForChanges(const std::vector<std::string>& files, int intervalSec) {
    if (files.empty()) return false;
	else {
        std::unordered_map<std::string, std::filesystem::file_time_type> last;
        for (auto& f : files)
			if (std::filesystem::exists(f))
				last[f] = std::filesystem::last_write_time(f);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
			bool changed = false;
            for (auto& f : files) {
                if (!std::filesystem::exists(f)) continue;
				auto now = std::filesystem::last_write_time(f);
                if (now != last[f]) {
                    changed = true;
                    last[f] = now;
                }
            }
			if (!changed) continue;
            else {
				std::cout << color::yellow << "🔄 change detected, rebuilding...\n" << color::reset;
                return true;
            }
        }
	}
	return false;

    // Simple keyword set
    auto isKeyword = [](const std::string& s) {
        return s == "Print" || s == "ret" || s == "loop" || s == "if" ||
            s == "else" || s == "Fn" || s == "call" || s == "let" ||
            s == "while" || s == "break" || s == "continue" ||
            s == "switch" || s == "case" || s == "default" ||
            s == "overlay" || s == "open" || s == "write" || s == "writeln" ||
            s == "read" || s == "close" || s == "mutate" ||
            s == "scale" || s == "bounds" || s == "checkpoint" || s == "vbreak" ||
            s == "channel" || s == "send" || s == "recv" || s == "sync" ||
            s == "schedule" || s == "input" ||
            s == "true" || s == "false" ||
            s == "match"; // <-- added
        };

    // Extended keyword set
    auto isKeyword = [](const std::string& s) {
        return s == "Print" || s == "ret" || s == "loop" || s == "if" ||
            s == "else" || s == "Fn" || s == "call" || s == "let" ||
            s == "while" || s == "break" || s == "continue" ||
            s == "switch" || s == "case" || s == "default" ||
            s == "overlay" || s == "open" || s == "write" || s == "writeln" ||
            s == "read" || s == "close" || s == "mutate" ||
            s == "scale" || s == "bounds" || s == "checkpoint" || s == "vbreak" ||
            s == "channel" || s == "send" || s == "recv" || s == "sync" ||
            s == "schedule" || s == "input" || s == "true" || s == "false" ||
            s == "class" || s == "extends" || s == "public" || s == "private" || s == "protected" ||
            s == "routine" || s == "struct" || s == "splice" || s == "duration" || s == "derivative" ||
            s == "match"; // <-- added
        };

    // match <expr> { case <pat> [| <pat>]* { body } ... default { body } }
    static Node* parseMatch() {
        advanceTok(); // 'match'

        // Scrutinee expression
        Node* scrutExpr = parseExpression();
        if (!scrutExpr) throw std::runtime_error("Expected expression after 'match' at line " + std::to_string(peek().line));

        Node* matchNode = new Node{ "Match","" };
        Node* scrut = new Node{ "Scrut","" };
        scrut->children.push_back(scrutExpr);
        matchNode->children.push_back(scrut);

        if (!matchValue("{")) throw std::runtime_error("Expected '{' after match expression at line " + std::to_string(peek().line));

        auto parsePatternList = [&]() -> Node* {
            // Returns a 'Pats' node with children of type: 'PatNum' / 'PatStr' / 'PatWildcard'
            Node* pats = new Node{ "Pats","" };
            bool gotAny = false;
            while (true) {
                if (peek().type == TokenType::NUMBER) {
                    Node* p = new Node{ "PatNum", advanceTok().value };
                    pats->children.push_back(p);
                    gotAny = true;
                }
                else if (peek().type == TokenType::STRING) {
                    Node* p = new Node{ "PatStr", advanceTok().value };
                    pats->children.push_back(p);
                    gotAny = true;
                }
                else if (peek().type == TokenType::IDENT && peek().value == "_") {
                    advanceTok();
                    Node* p = new Node{ "PatWildcard","_" };
                    pats->children.push_back(p);
                    gotAny = true;
                }
                else {
                    break;
                }
                if (matchValue("|") || matchValue(",")) continue;
                break;
            }
            if (!gotAny) throw std::runtime_error("Expected pattern after 'case' at line " + std::to_string(peek().line));
            return pats;
            };

        while (!checkValue("}") && peek().type != TokenType::END) {
            if (matchValue("case")) {
                Node* caseNode = new Node{ "Case","" };
                Node* pats = parsePatternList();
                caseNode->children.push_back(pats);

                // Body as a block
                Node* body = parseBlock();
                caseNode->children.push_back(body);
                matchNode->children.push_back(caseNode);
            }
            else if (matchValue("default")) {
                Node* def = new Node{ "Default","" };
                Node* body = parseBlock();
                def->children.push_back(body);
                matchNode->children.push_back(def);
            }
            else {
                // Skip unexpected tokens within match
                advanceTok();
            }
        }

        if (!matchValue("}")) throw std::runtime_error("Expected '}' to close match at line " + std::to_string(peek().line));
        return matchNode;
    }

    C.A.S.E. - Programming - Language - \Transpiler.cpp
        if (v == "match")    return parseMatch();

    static void emitMatch(std::ostringstream& out, Node* n) {
        // n: Match(Scrut -> expr, Case(Pats..., Body), ..., Default(Body)?)
        static int __match_counter = 0;
        int id = __match_counter++;

        if (n->children.empty() || n->children[0]->type != "Scrut" || n->children[0]->children.empty()) {
            out << "/* invalid match */\n";
            return;
        }

        std::string scrut = emitExpr(n->children[0]->children[0]);
        out << "{ auto __match_tmp_" << id << " = " << scrut << "; bool __matched_" << id << " = false;\n";

        for (size_t i = 1; i < n->children.size(); ++i) {
            Node* c = n->children[i];
            if (!c) continue;

            if (c->type == "Case") {
                // c->children[0] = Pats, c->children[1] = Body
                Node* pats = (c->children.size() > 0 ? c->children[0] : nullptr);
                Node* body = (c->children.size() > 1 ? c->children[1] : nullptr);

                // Build condition: (true) for wildcard, or equality chain
                std::ostringstream cond;
                bool first = true;
                bool hasWildcard = false;

                if (pats) {
                    for (auto* p : pats->children) {
                        if (!p) continue;
                        if (p->type == "PatWildcard") {
                            hasWildcard = true;
                        }
                        else if (p->type == "PatNum") {
                            if (!first) cond << " || ";
                            cond << "(__match_tmp_" << id << " == " << p->value << ")";
                            first = false;
                        }
                        else if (p->type == "PatStr") {
                            if (!first) cond << " || ";
                            cond << "(__match_tmp_" << id << " == " << "\"" << escapeCppString(p->value) << "\"" << ")";
                            first = false;
                        }
                    }
                }

                if (hasWildcard) {
                    out << "if (!__matched_" << id << ") {\n";
                }
                else {
                    out << "if (!__matched_" << id << " && (" << (first ? "false" : cond.str()) << ")) {\n";
                }
                out << "__matched_" << id << " = true;\n";
                if (body) emitChildren(body->children, out);
                out << "}\n";
            }
            else if (c->type == "Default") {
                Node* body = (c->children.size() > 0 ? c->children[0] : nullptr);
                out << "if (!__matched_" << id << ") {\n";
                if (body) emitChildren(body->children, out);
                out << "}\n";
            }
        }

        out << "}\n";
    }

else if (n->type == "Match") {
    emitMatch(out, n);
}

    // Replace isSymbolChar and add readSymbol, extend isKeyword; then use readSymbol in tokenize()

    static bool isSymbolChar(char c) {
        switch (c) {
        case '(': case ')': case '{': case '}': case '[': case ']':
        case '=': case ';': case ',': case '+': case '-': case '*':
        case '/': case '<': case '>': case '!': case '&': case '|':
        case '%': case ':': case '?': case '.':
            return true;
        default:
            return false;
        }
    }

    static std::string readSymbolToken(const std::string& src, size_t& idx) {
        static const std::unordered_set<std::string> twoChar = {
            "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=",
            "++", "--", "->", "::", "<<", ">>", "<<=", ">>=", "&=", "|=", "^="
        };
        char c = src[idx];
        std::string one(1, c);
        if (idx + 1 < src.size()) {
            std::string two = one + src[idx + 1];
            if (twoChar.count(two)) { idx += 2; return two; }
        }
        ++idx;
        return one;
    }

    std::vector<Token> tokenize(const std::string& src) {
        std::vector<Token> tokens;
        size_t i = 0;
        int line = 1;

        auto push = [&](TokenType t, const std::string& v) { tokens.push_back(Token{ t, v, line }); };

        auto isKeyword = [](const std::string& s) {
            return
                // control + functions
                s == "Print" || s == "ret" || s == "return" || s == "loop" || s == "if" || s == "else" ||
                s == "Fn" || s == "routine" || s == "call" || s == "let" ||
                s == "while" || s == "break" || s == "continue" ||
                s == "switch" || s == "case" || s == "default" || s == "match" ||
                s == "try" || s == "catch" || s == "throw" ||
                // overlays + IO + effects
                s == "overlay" || s == "open" || s == "write" || s == "writeln" ||
                s == "read" || s == "close" || s == "mutate" ||
                s == "scale" || s == "bounds" || s == "checkpoint" || s == "vbreak" ||
                s == "channel" || s == "send" || s == "recv" || s == "sync" ||
                s == "schedule" ||
                // types + aggregates
                s == "class" || s == "extends" || s == "public" || s == "private" || s == "protected" ||
                s == "struct" || s == "enum" || s == "namespace" ||
                // meta
                s == "splice" || s == "duration" || s == "derivative" ||
                // literals
                s == "true" || s == "false" || s == "null";
            };

        auto readNumber = [&](size_t& idx) {
            size_t start = idx;
            bool hasDot = false, hasExp = false;
            if (idx + 1 < src.size() && src[idx] == '0' && (src[idx + 1] == 'x' || src[idx + 1] == 'X')) {
                idx += 2; while (idx < src.size() && std::isxdigit(static_cast<unsigned char>(src[idx]))) ++idx;
                return src.substr(start, idx - start);
            }
            if (idx + 1 < src.size() && src[idx] == '0' && (src[idx + 1] == 'b' || src[idx + 1] == 'B')) {
                idx += 2; while (idx < src.size() && (src[idx] == '0' || src[idx] == '1' || src[idx] == '_')) ++idx;
                return src.substr(start, idx - start);
            }
            while (idx < src.size()) {
                char ch = src[idx];
                if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '_') { ++idx; continue; }
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
            if (c == '\n') { ++line; ++i; continue; }
            if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }

            // // or # comments
            if ((c == '/' && i + 1 < src.size() && src[i + 1] == '/') || c == '#') {
                if (c == '/') i += 2; else ++i;
                while (i < src.size() && src[i] != '\n') ++i;
                continue;
            }

            if (isIdentStart(c)) {
                size_t start = i++;
                while (i < src.size() && isIdentChar(src[i])) ++i;
                std::string ident = src.substr(start, i - start);
                push(isKeyword(ident) ? TokenType::KEYWORD : TokenType::IDENT, ident);
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c))) {
                std::string num = readNumber(i);
                push(TokenType::NUMBER, num);
                continue;
            }

            if (c == '"') {
                ++i;
                std::string acc; bool closed = false;
                while (i < src.size()) {
                    char ch = src[i++];
                    if (ch == '"') { closed = true; break; }
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
                    else acc.push_back(ch);
                }
                if (!closed) throw std::runtime_error("Unterminated string literal at line " + std::to_string(line));
                push(TokenType::STRING, acc);
                continue;
            }

            if (isSymbolChar(c)) {
                std::string sym = readSymbolToken(src, i);
                push(TokenType::SYMBOL, sym);
                continue;
            }

            throw std::runtime_error(std::string("Unexpected character '") + c + "' at line " + std::to_string(line));
        }

        tokens.push_back(Token{ TokenType::END, "", line });
        return tokens;
    }

    // Add forward decls near the other fwd declarations
    static Node* parseAssignmentOrIncDec();
    static Node* parseMatch();
    static Node* parseTry();
    static Node* parseThrow();
    static Node* parseSplice();
    static Node* parseDuration();
    static Node* parseDerivative();

    // Upgrade precedence
    static int precedenceOf(const std::string& op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=") return 3;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
        if (op == "+" || op == "-") return 5;
        if (op == "*" || op == "/" || op == "%") return 6;
        return -1;
    }

    // Unary supports ! and -
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

    // Optional: booleans in primary
    static Node* parsePrimary() {
        if (peek().type == TokenType::NUMBER) return new Node{ "Num", advanceTok().value };
        if (peek().type == TokenType::STRING) return new Node{ "Str", advanceTok().value };
        if (peek().type == TokenType::KEYWORD && (peek().value == "true" || peek().value == "false")) {
            std::string v = advanceTok().value == "true" ? "1" : "0";
            return new Node{ "Num", v };
        }
        if (peek().type == TokenType::IDENT) return new Node{ "Var", advanceTok().value };
        if (checkValue("(")) {
            advanceTok();
            Node* inner = parseExpression();
            if (!matchValue(")")) throw std::runtime_error("Expected ')' in expression at line " + std::to_string(peek().line));
            return inner;
        }
        throw std::runtime_error("Expected expression at line " + std::to_string(peek().line));
    }

    // Assignment and ++/-- (statement-form)
    static Node* parseAssignmentOrIncDec() {
        size_t save = pos;
        if (peek().type != TokenType::IDENT) return nullptr;
        Node* lhs = new Node{ "Var", advanceTok().value }; // keep simple lvalue

        // ++ or -- (postfix)
        if (peek().type == TokenType::SYMBOL && (peek().value == "++" || peek().value == "--")) {
            std::string op = advanceTok().value;
            Node* one = new Node{ "Num", "1" };
            Node* asn = new Node{ "Assign", op == "++" ? "+=" : "-=" };
            asn->children.push_back(lhs);
            asn->children.push_back(one);
            return asn;
        }

        // =, +=, -=, *=, /=, %=
        if (peek().type == TokenType::SYMBOL &&
            (peek().value == "=" || peek().value == "+=" || peek().value == "-=" ||
                peek().value == "*=" || peek().value == "/=" || peek().value == "%=")) {
            std::string op = advanceTok().value;
            Node* rhs = parseExpression();
            Node* asn = new Node{ "Assign", op };
            asn->children.push_back(lhs);
            asn->children.push_back(rhs);
            return asn;
        }

        pos = save;
        return nullptr;
    }

	static void emitStmt(std::ostringstream& out, Node* n) {    
		if (n->type == "Sleep") {
			// children[0]=duration, value=unit
			std::string u = n->value;
            std::string chronoUnit =
            (u == "ms" || u == "msec" || u == "millis" || u == "milliseconds") ? "std::chrono::milliseconds" :
            (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
				(u == "us" || u == "usec" || u == "microseconds") ? "std::chrono::microseconds" :

                // match <expr> { case <pat>[|<pat>]* {body} ... default {body} }
                static Node* parseMatch() {
                advanceTok(); // 'match'
                Node* scrutExpr = parseExpression();
                if (!scrutExpr) throw std::runtime_error("Expected expression after 'match' at line " + std::to_string(peek().line));
                Node* n = new Node{ "Match","" };
                Node* scrut = new Node{ "Scrut","" };
                scrut->children.push_back(scrutExpr);
                n->children.push_back(scrut);
                if (!matchValue("{")) throw std::runtime_error("Expected '{' after match expression at line " + std::to_string(peek().line));

                auto parsePatternList = [&]() -> Node* {
                    Node* pats = new Node{ "Pats","" };
                    bool got = false;
                    while (true) {
                        if (peek().type == TokenType::NUMBER) { pats->children.push_back(new Node{ "PatNum", advanceTok().value }); got = true; }
                        else if (peek().type == TokenType::STRING) { pats->children.push_back(new Node{ "PatStr", advanceTok().value }); got = true; }
                        else if (peek().type == TokenType::IDENT && peek().value == "_") { advanceTok(); pats->children.push_back(new Node{ "PatWildcard","_" }); got = true; }
                        else break;
                        if (matchValue("|") || matchValue(",")) continue;
                        break;
                    }
                    if (!got) throw std::runtime_error("Expected pattern after 'case' at line " + std::to_string(peek().line));
                    return pats;
                    };

                while (!checkValue("}") && peek().type != TokenType::END) {
                    if (matchValue("case")) {
                        Node* c = new Node{ "Case","" };
                        c->children.push_back(parsePatternList());
                        Node* body = parseBlock();
                        c->children.push_back(body);
                        n->children.push_back(c);
                    }
                    else if (matchValue("default")) {
                        Node* d = new Node{ "Default","" };
                        d->children.push_back(parseBlock());
                        n->children.push_back(d);
                    }
                    else {
                        advanceTok();
                    }
                }
                if (!matchValue("}")) throw std::runtime_error("Expected '}' to close match at line " + std::to_string(peek().line));
                return n;
            }

            // try { ... } catch (<ident>){ ... }
            static Node* parseTry() {
                advanceTok(); // 'try'
                Node* t = new Node{ "Try","" };
                Node* body = parseBlock();
                t->children.push_back(body);
                if (matchValue("catch")) {
                    std::string var;
                    if (matchValue("(")) {
                        if (peek().type == TokenType::IDENT) var = advanceTok().value;
                        if (!matchValue(")")) throw std::runtime_error("Expected ')' after catch parameter at line " + std::to_string(peek().line));
                    }
                    Node* c = new Node{ "Catch", var };
                    c->children.push_back(parseBlock());
                    t->children.push_back(c);
                }
                return t;
            }

            // throw <expr|string>
            static Node* parseThrow() {
                advanceTok(); // 'throw'
                Node* n = new Node{ "Throw","" };
                if (peek().type == TokenType::STRING) {
                    n->children.push_back(new Node{ "Str", advanceTok().value });
                }
                else {
                    n->children.push_back(parseExpression());
                }
                return n;
            }

            // splice "raw C++"
            static Node* parseSplice() {
                advanceTok(); // 'splice'
                if (peek().type != TokenType::STRING) throw std::runtime_error("Expected string after 'splice' at line " + std::to_string(peek().line));
                Node* n = new Node{ "Splice", advanceTok().value };
                return n;
            }

            // duration <expr> "unit"
            static Node* parseDuration() {
                advanceTok(); // 'duration'
                Node* e = parseExpression();
                if (!e) throw std::runtime_error("Expected expression in 'duration' at line " + std::to_string(peek().line));
                if (peek().type != TokenType::STRING) throw std::runtime_error("Expected unit string in 'duration' at line " + std::to_string(peek().line));
                Node* unit = new Node{ "Str", advanceTok().value };
                Node* n = new Node{ "Duration","" };
                n->children.push_back(e);
                n->children.push_back(unit);
                return n;
            }

            // derivative <expr> [wrt IDENT]
            static Node* parseDerivative() {
                advanceTok(); // 'derivative'
                Node* e = parseExpression();
                Node* n = new Node{ "Derivative","" };
                n->children.push_back(e);
                if (peek().type == TokenType::IDENT && peek().value == "wrt") {
                    advanceTok();
                    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected identifier after 'wrt' at line " + std::to_string(peek().line));
                    n->children.push_back(new Node{ "Var", advanceTok().value });
                }
                return n;
            }

            // Hook new statements early in parseStatement()
            static Node* parseStatement() {
                // First, assignment/inc/dec if it fits
                if (peek().type == TokenType::IDENT) {
                    if (Node* asn = parseAssignmentOrIncDec()) return asn;
                }
                const std::string v = peek().value;
                if (v == "Print")    return parsePrint();
                if (v == "ret" || v == "return") return parseRet();
                if (v == "loop")     return parseLoop();
                if (v == "if")       return parseIf();
                if (v == "while")    return parseWhile();
                if (v == "break") { advanceTok(); return new Node{ "Break","" }; }
                if (v == "continue") { advanceTok(); return new Node{ "Continue","" }; }
                if (v == "switch")   return parseSwitch();
                if (v == "match")    return parseMatch();
                if (v == "try")      return parseTry();
                if (v == "throw")    return parseThrow();
                if (v == "Fn")       return parseFn();
                if (v == "routine") { advanceTok(); return parseFn(); } // alias to Fn
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
                if (v == "splice")   return parseSplice();
                if (v == "duration") return parseDuration();
                if (v == "derivative") return parseDerivative();

                advanceTok();
                return new Node{ "Unknown", v };
            }

            // Extend emitExpr to support Unary and Ternary (safe no-ops if not used)
            static std::string emitExpr(Node* e) {
                if (!e) return "0";
                if (e->type == "Num") return e->value;
                if (e->type == "Var") return e->value;
                if (e->type == "Str") return std::string("\"") + escapeCppString(e->value) + "\"";
                if (e->type == "Unary") return "(" + e->value + "(" + emitExpr(e->children[0]) + "))";
                if (e->type == "BinOp") {
                    std::string lhs = emitExpr(e->children.size() > 0 ? e->children[0] : nullptr);
                    std::string rhs = emitExpr(e->children.size() > 1 ? e->children[1] : nullptr);
                    return "(" + lhs + " " + e->value + " " + rhs + ")";
                }
                if (e->type == "Ternary") {
                    return "(" + emitExpr(e->children[0]) + " ? " + emitExpr(e->children[1]) + " : " + emitExpr(e->children[2]) + ")";
                }
                return "/*expr*/0";
            }

            // Add branches in emitNode()

else if (n->type == "Assign") {
                std::string lhs = emitExpr(n->children[0]);
                std::string rhs = emitExpr(n->children[1]);
                out << lhs << " " << n->value << " " << rhs << ";\n";
                }
else if (n->type == "Match") {
                    static int __match_counter = 0;
                    int id = __match_counter++;
                    std::string scrut = emitExpr(n->children[0]->children[0]);
                    out << "{ auto __match_tmp_" << id << " = " << scrut << "; bool __matched_" << id << " = false;\n";
                    for (size_t i = 1; i < n->children.size(); ++i) {
                        Node* c = n->children[i];
                        if (c->type == "Case") {
                            Node* pats = c->children[0];
                            Node* body = c->children[1];
                            std::ostringstream cond; bool first = true; bool wild = false;
                            for (auto* p : pats->children) {
                                if (p->type == "PatWildcard") { wild = true; continue; }
                                if (!first) cond << " || ";
                                if (p->type == "PatNum") cond << "(__match_tmp_" << id << " == " << p->value << ")";
                                else if (p->type == "PatStr") cond << "(__match_tmp_" << id << " == \"" << escapeCppString(p->value) << "\")";
                                first = false;
                            }
                            if (wild) { out << "if (!__matched_" << id << ") {\n"; }
                            else { out << "if (!__matched_" << id << " && (" << (first ? "false" : cond.str()) << ")) {\n"; }
                            out << "__matched_" << id << " = true;\n";
                            emitChildren(body->children, out);
                            out << "}\n";
                        }
                        else if (c->type == "Default") {
                            Node* body = c->children[0];
                            out << "if (!__matched_" << id << ") {\n";
                            emitChildren(body->children, out);
                            out << "}\n";
                        }
                    }
                    out << "}\n";
                    }
else if (n->type == "Try") {
                        out << "try{\n";
                        if (!n->children.empty()) emitChildren(n->children[0]->children, out);
                        out << "}\n";
                        if (n->children.size() > 1 && n->children[1]->type == "Catch") {
                            Node* c = n->children[1];
                            std::string var = c->value.empty() ? "e" : c->value;
                            out << "catch(const std::exception& " << var << "){\n";
                            if (!c->children.empty()) emitChildren(c->children[0]->children, out);
                            out << "}\n";
                        }
                        }
else if (n->type == "Throw") {
                            if (!n->children.empty() && n->children[0]->type == "Str") {
                                out << "throw std::runtime_error(" << "\"" << escapeCppString(n->children[0]->value) << "\"" << ");\n";
                            }
                            else {
                                out << "throw " << emitExpr(n->children.empty() ? nullptr : n->children[0]) << ";\n";
                            }
                            }
else if (n->type == "Splice") {
                                out << n->value << "\n";
                                }
else if (n->type == "Duration") {
                                    std::string unit = n->children[1]->value;
                                    std::string u = unit; std::transform(u.begin(), u.end(), u.begin(), ::tolower);
                                    const char* chronoUnit =
                                        (u == "ns" || u == "nanoseconds") ? "std::chrono::nanoseconds" :
                                        (u == "us" || u == "microseconds") ? "std::chrono::microseconds" :
                                        (u == "ms" || u == "milliseconds") ? "std::chrono::milliseconds" :
                                        (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
                                        (u == "m" || u == "min" || u == "minutes") ? "std::chrono::minutes" :
                                        (u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "std::chrono::milliseconds";
                                    out << "{ using namespace std::chrono; auto __dur_val = " << emitExpr(n->children[0]) << "; "
                                        << chronoUnit << " __dur = duration_cast<" << chronoUnit << ">(duration<double>(__dur_val)); (void)__dur; }\n";
                                        }
else if (n->type == "Derivative") {
                                            if (n->children.size() >= 2 && n->children[1]->type == "Var") {
                                                const std::string var = n->children[1]->value;
                                                const std::string f = emitExpr(n->children[0]);
                                                out << "{ auto __eps=1e-6; auto __orig=(" << var << "); "
                                                    "auto __f0=(double)(" << f << "); " << var << " = __orig + __eps; "
                                                    "auto __f1=(double)(" << f << "); " << var << " = __orig; "
                                                    "double __deriv = (__f1-__f0)/__eps; (void)__deriv; }\n";
                                            }
                                            else {
                                                out << "{ /* derivative missing wrt */ double __deriv = 0.0; (void)__deriv; }\n";
                                            }
                                            }

                                            // In emitPrelude(), add chrono and stdexcept
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
                                                out << "#include <chrono>\n";
                                                out << "#include <stdexcept>\n";
                                                out << "#if defined(_OPENMP)\n#include <omp.h>\n#endif\n";
                                                // channel template unchanged...
                                            }

                                            // ciam_registry.hpp
#pragma once
#include <functional>
#include <string>
#include <unordered_map>

                                            namespace ciam {

                                                struct OverlayContext {
                                                    std::string funcName;
                                                    std::string overlayName;
                                                    int line;
                                                    std::string file;
                                                };

                                                using PlaceholderFn = std::function<std::string(const OverlayContext&)>;

                                                class PlaceholderRegistry {
                                                public:
                                                    static void registerPlaceholder(const std::string& key, PlaceholderFn fn);
                                                    static std::string resolve(const std::string& key, const OverlayContext& ctx);

                                                private:
                                                    static std::unordered_map<std::string, PlaceholderFn>& table();
                                                };

                                            } // namespace ciam

// ciam_registry.cpp
#include "ciam_registry.hpp"

                                            namespace ciam {

                                                static std::unordered_map<std::string, PlaceholderFn> registry;

                                                void PlaceholderRegistry::registerPlaceholder(const std::string& key, PlaceholderFn fn) {
                                                    registry[key] = std::move(fn);
                                                }

                                                std::string PlaceholderRegistry::resolve(const std::string& key, const OverlayContext& ctx) {
                                                    auto it = registry.find(key);
                                                    return (it != registry.end()) ? it->second(ctx) : "${" + key + "}";
                                                }

                                                std::unordered_map<std::string, PlaceholderFn>& PlaceholderRegistry::table() {
                                                    return registry;
                                                }

                                            } // namespace ciam


                                            ciam_ext_system.cpp


#include "ciam_registry.hpp"
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#define SYS_OS "Windows"
#elif __APPLE__
#define SYS_OS "macOS"
#elif __linux__
#define SYS_OS "Linux"
#else
#define SYS_OS "UnknownOS"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define SYS_ARCH "x86_64"
#elif defined(__aarch64__)
#define SYS_ARCH "ARM64"
#else
#define SYS_ARCH "UnknownArch"
#endif

                                                namespace {

                                                std::string currentCompiler() {
#ifdef __clang__
                                                    return "Clang";
#elif defined(__GNUC__)
                                                    return "GCC";
#elif defined(_MSC_VER)
                                                    return "MSVC";
#else
                                                    return "UnknownCompiler";
#endif
                                                }

                                                std::string currentCPUCount() {
                                                    unsigned int n = std::thread::hardware_concurrency();
                                                    return std::to_string(n ? n : 1);
                                                }

                                                void registerSystemPlaceholders() {
                                                    using namespace ciam;

                                                    PlaceholderRegistry::registerPlaceholder("OS", [](const OverlayContext&) {
                                                        return '"' + std::string(SYS_OS) + '"';
                                                        });

                                                    PlaceholderRegistry::registerPlaceholder("ARCH", [](const OverlayContext&) {
                                                        return '"' + std::string(SYS_ARCH) + '"';
                                                        });

                                                    PlaceholderRegistry::registerPlaceholder("CPU", [](const OverlayContext&) {
                                                        return currentCPUCount();
                                                        });

                                                    PlaceholderRegistry::registerPlaceholder("COMPILER", [](const OverlayContext&) {
                                                        return '"' + currentCompiler() + '"';
                                                        });
                                                }

                                                // Auto-run on program start
                                                struct AutoInit {
                                                    AutoInit() { registerSystemPlaceholders(); }
                                                } _autoInit;

                                            } // anonymous namespace


// ciam_plugin_loader.hpp
#pragma once
#include <string>

                                            namespace ciam {
                                                // Load one plugin (shared library) from disk.
                                                bool loadPlugin(const std::string& path);

                                                // Load all plugins from a directory (non-recursive).
                                                void loadPluginsFromDir(const std::string& directory);
                                            }


#include "ciam_plugin_loader.hpp"
#include <filesystem>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#define OPEN_LIB(name)   LoadLibraryA(name)
#define GET_SYM(handle,sym) GetProcAddress((HMODULE)handle, sym)
#define CLOSE_LIB(handle)   FreeLibrary((HMODULE)handle)
#else
#include <dlfcn.h>
#define OPEN_LIB(name)   dlopen(name, RTLD_NOW)
#define GET_SYM(handle,sym) dlsym(handle, sym)
#define CLOSE_LIB(handle)   dlclose(handle)
#endif

                                            namespace ciam {

                                                using InitFn = void(*)();        // every plugin must export this symbol

                                                bool loadPlugin(const std::string& path) {
                                                    void* handle = OPEN_LIB(path.c_str());
                                                    if (!handle) {
                                                        std::cerr << "[CIAM] Could not open " << path << std::endl;
                                                        return false;
                                                    }
                                                    auto init = reinterpret_cast<InitFn>(GET_SYM(handle, "ciam_register"));
                                                    if (init) {
                                                        init();
                                                        std::cout << "[CIAM] Loaded plugin " << path << std::endl;
                                                        return true;
                                                    }
                                                    std::cerr << "[CIAM] " << path << " missing symbol ciam_register" << std::endl;
                                                    CLOSE_LIB(handle);
                                                    return false;
                                                }

                                                void loadPluginsFromDir(const std::string& dir) {
                                                    namespace fs = std::filesystem;
                                                    for (auto& entry : fs::directory_iterator(dir)) {
                                                        const auto& p = entry.path();
                                                        if (!entry.is_regular_file()) continue;
#if defined(_WIN32)
                                                        if (p.extension() == ".dll")
#elif defined(__APPLE__)
                                                        if (p.extension() == ".dylib")
#else
                                                        if (p.extension() == ".so")
#endif
                                                            loadPlugin(p.string());
                                                    }
                                                }

                                            } // namespace ciam


#include "ciam_plugin_loader.hpp"

                                            int main(int argc, char** argv) {
                                                // 1️⃣  Load built-in CIAM placeholders
                                                loadCoreCIAMRegistry();

                                                // 2️⃣  Discover external plugin modules
                                                ciam::loadPluginsFromDir("plugins");

                                                // 3️⃣  Continue with normal transpilation
                                                runTranspiler(argc, argv);
                                            }


                                            // ciam_plugin_manifest.hpp
#pragma once
#include <string>
#include <unordered_map>

                                            namespace ciam {
                                                struct PluginManifest {
                                                    std::string id, name, version, author, description;
                                                    std::vector<std::string> placeholders;
                                                };

                                                PluginManifest readManifest(const std::string& path);
                                                void printManifestSummary(const PluginManifest& m);
                                            }


                                            // ciam_plugin_manifest.cpp
#include "ciam_plugin_manifest.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

                                            namespace ciam {

                                                PluginManifest readManifest(const std::string& path) {
                                                    PluginManifest m;
                                                    std::ifstream in(path);
                                                    if (!in) return m;

                                                    std::string key, val;
                                                    while (in >> key) {
                                                        std::getline(in, val);
                                                        if (!val.empty() && val[0] == ' ') val.erase(0, 1);
                                                        if (key == "NAME")        m.name = val;
                                                        else if (key == "ID")     m.id = val;
                                                        else if (key == "VERSION")m.version = val;
                                                        else if (key == "AUTHOR") m.author = val;
                                                        else if (key == "DESCRIPTION") m.description = val;
                                                        else if (key == "PLACEHOLDERS") {
                                                            std::stringstream ss(val);
                                                            std::string tok;
                                                            while (std::getline(ss, tok, ',')) {
                                                                tok.erase(remove_if(tok.begin(), tok.end(), ::isspace), tok.end());
                                                                if (!tok.empty()) m.placeholders.push_back(tok);
                                                            }
                                                        }
                                                    }
                                                    return m;
                                                }

                                                void printManifestSummary(const PluginManifest& m) {
                                                    std::cout << "▶ " << (m.name.empty() ? m.id : m.name)
                                                        << " v" << (m.version.empty() ? "?" : m.version)
                                                        << "  by " << (m.author.empty() ? "unknown" : m.author) << "\n";
                                                    if (!m.placeholders.empty()) {
                                                        std::cout << "   Exports: ";
                                                        for (size_t i = 0; i < m.placeholders.size(); ++i) {
                                                            std::cout << "${" << m.placeholders[i] << "}";
                                                            if (i + 1 < m.placeholders.size()) std::cout << ", ";
                                                        }
                                                        std::cout << "\n";
                                                    }
                                                }

                                            } // namespace ciam


											(u == "ms" || u == "milliseconds") ? "std::chrono::milliseconds" :
												(u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
												(u == "m" || u == "min" || u == "minutes") ? "std::chrono::minutes" :
												(u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "std::chrono::milliseconds";
                                            out << "{ using namespace std::chrono; auto __dur_val = " << emitExpr(n->children[0]) << "; "
												<< chronoUnit << " __dur = duration_cast<" << chronoUnit << ">(duration<double>(__dur_val)); (void)__dur; }\n";

#include "ciam_plugin_manifest.hpp"

                                            void loadPluginsFromDir(const std::string& dir) {
                                                namespace fs = std::filesystem;
                                                for (auto& entry : fs::directory_iterator(dir)) {
                                                    const auto& p = entry.path();
                                                    if (!entry.is_regular_file()) continue;

#if defined(_WIN32)
                                                    bool islib = (p.extension() == ".dll");
#elif defined(__APPLE__)
                                                    bool islib = (p.extension() == ".dylib");
#else
                                                    bool islib = (p.extension() == ".so");
#endif
                                                    if (!islib) continue;

                                                    // look for matching manifest
                                                    std::string manifestPath = p;
                                                    manifestPath.replace(manifestPath.find_last_of('.'), std::string::npos, ".caseinfo");
                                                    if (fs::exists(manifestPath)) {
                                                        auto m = ciam::readManifest(manifestPath);
                                                        ciam::printManifestSummary(m);
                                                    }

                                                    loadPlugin(p.string());
                                                }
                                            }

											} // namespace ciam

                                            if (argc >= 2 && std::string(argv[1]) == "list-plugins") {
                                                ciam::loadPluginsFromDir("plugins");
                                                return 0;
                                            }


                                            // cmod_pack.cpp
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstdlib>

                                            namespace fs = std::filesystem;

                                            int main(int argc, char** argv) {
                                                if (argc < 3) {
                                                    std::cout << "Usage: cmod pack <plugin_dir>\n";
                                                    return 0;
                                                }
                                                fs::path src = argv[2];
                                                fs::path out = src.filename().string() + ".cmod";

                                                std::string cmd =
                                                    "zip -r " + out.string() + " " +
                                                    (fs::is_directory(src) ? src.string() : ".");
                                                std::system(cmd.c_str());
                                                std::cout << "Created " << out << "\n";
                                            }


#include <filesystem>
#include <iostream>
#include <cstdlib>

                                            namespace fs = std::filesystem;

                                            int installCmod(const std::string& path) {
                                                if (!fs::exists(path)) {
                                                    std::cerr << "File not found: " << path << "\n";
                                                    return 1;
                                                }

                                                fs::path pluginDir = "plugins";
                                                fs::create_directories(pluginDir);

                                                // unzip
                                                std::string cmd = "unzip -o " + path + " -d " + pluginDir.string();
                                                if (std::system(cmd.c_str()) != 0) {
                                                    std::cerr << "Failed to unpack " << path << "\n";
                                                    return 1;
                                                }

                                                std::cout << "[CIAM] Installed " << path << " to " << pluginDir << "\n";
                                                return 0;
                                            }

                                            if (argc >= 3 && std::string(argv[1]) == "install")
                                                return installCmod(argv[2]);


                                            void installCmod(const std::string& path) {
                                                namespace fs = std::filesystem;
                                                fs::create_directories("plugins");
                                                fs::create_directories("themes");
                                                fs::create_directories("overlays");
                                                fs::create_directories("snippets");

                                                std::string unzip = "unzip -o " + path + " -d temp_install";
                                                if (std::system(unzip.c_str()) != 0) {
                                                    std::cerr << "Failed to extract " << path << "\n"; return;
                                                }

                                                // Move contents by folder
                                                for (auto& entry : fs::directory_iterator("temp_install")) {
                                                    auto name = entry.path().filename().string();
                                                    if (name == "lib") fs::copy(entry, "plugins", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                                    else if (name == "themes") fs::copy(entry, "themes", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                                    else if (name == "overlays") fs::copy(entry, "overlays", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                                    else if (name == "snippets") fs::copy(entry, "snippets", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                                    else if (name == "doc") fs::copy(entry, "plugins/docs", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                                                }
                                                fs::remove_all("temp_install");
                                                std::cout << "[CIAM] Installed extended module " << path << "\n";
                                            }


                                            // Replace the existing Duration/Derivative placeholders and add Input emission
                                            // in the first emitNode(...) implementation you actually use (the one used by emitCPP).

                                            // Find: static void emitNode(Node* n, std::ostringstream& out) { ... }
                                            // And update these branches:

                                            else if (n->type == "Duration") {
                                                // Before: placeholder comment
                                                // After: convert to std::chrono duration, keep it in a scoped variable to avoid name clashes.
                                                std::string unit = (n->children.size() > 1 ? n->children[1]->value : "ms");
                                                std::string u = unit; std::transform(u.begin(), u.end(), u.begin(), ::tolower);
                                                const char* chronoUnit =
                                                    (u == "ns" || u == "nanoseconds") ? "std::chrono::nanoseconds" :
                                                    (u == "us" || u == "microseconds") ? "std::chrono::microseconds" :
                                                    (u == "ms" || u == "milliseconds") ? "std::chrono::milliseconds" :
                                                    (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
                                                    (u == "m" || u == "min" || u == "minutes") ? "std::chrono::minutes" :
                                                    (u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "std::chrono::milliseconds";
                                                out << "{ using namespace std::chrono; auto __dur_val = " << emitExpr(n->children[0]) << "; "
                                                    << chronoUnit << " __dur = duration_cast<" << chronoUnit << ">(duration<double>(__dur_val)); "
                                                    << "(void)__dur; }\n";
                                            }
                                            else if (n->type == "Derivative") {
                                                // Before: placeholder comment
                                                // After: numeric forward-difference derivative; requires 'wrt <var>' to be present.
                                                if (n->children.size() >= 2 && n->children[1]->type == "Var") {
                                                    const std::string var = n->children[1]->value;
                                                    const std::string f = emitExpr(n->children[0]);
                                                    out << "{ auto __eps = 1e-6; auto __orig = (" << var << "); "
                                                        "auto __f0 = (double)(" << f << "); "
                                                        << var << " = __orig + __eps; "
                                                        "auto __f1 = (double)(" << f << "); "
                                                        << var << " = __orig; "
                                                        "double __deriv = (__f1 - __f0) / __eps; (void)__deriv; }\n";
                                                }
                                                else {
                                                    out << "{ /* derivative missing 'wrt <var>' */ double __deriv = 0.0; (void)__deriv; }\n";
                                                }
                                            }
                                            else if (n->type == "Input") {
                                                // Emit a simple read from stdin into a variable
                                                out << "std::cin >> " << n->value << ";\n";
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
                                                out << "#include <chrono>\n"; // added for duration support
                                                out << "#if defined(_OPENMP)\n#include <omp.h>\n#endif\n";
                                                // Simple channel template
                                                out << "template<typename T>\n";
                                                out << "struct Channel {\n";
                                                out << "  std::mutex m; std::condition_variable cv; std::deque<T> q;\n";
                                                out << "  void send(const T& v){ std::lock_guard<std::mutex> lk(m); q.push_back(v); cv.notify_one(); }\n";
                                                out << "  void recv(T& out){ std::unique_lock<std::mutex> lk(m); cv.wait(lk,[&]{return !q.empty();}); out = std::move(q.front()); q.pop_front(); }\n";
                                                out << "};\n";
                                                // Define default unknown duration unit alias
                                                out << "using quantum_epochs = std::chrono::duration<double>;\n\n";
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
                                                            // Emit string constants with quotes; else raw
                                                            bool isStr = false;
                                                            if (!c->value.empty() && (c->value.find_first_not_of("0123456789.-") != std::string::npos)) isStr = true;
                                                            out << "case " << (isStr ? ("/*id*/ " + c->value) : c->value) << ":\n";
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
                                                else if (n->type == "Assign") {
                                                    // children[0]=lhs, [1]=rhs (optional for ++/--)
                                                    std::string lhs = emitExpr(n->children.empty() ? nullptr : n->children[0]);
                                                    if (n->value == "++" || n->value == "--") {
                                                        // Expand as compound assign by 1 to keep simple C++14 target
                                                        out << lhs << (n->value == "++" ? " += 1" : " -= 1") << ";\n";
                                                    }
                                                    else {
                                                        std::string rhs = emitExpr(n->children.size() > 1 ? n->children[1] : nullptr);
                                                        out << lhs << " " << n->value << " " << rhs << ";\n";
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
                                                    // children: [0]=Var x, [1]=a, [2]=b, [3]=c, [4]=d
                                                    std::string x = n->children[0]->value;
                                                    std::string a = emitExpr(n->children[1]);
                                                    std::string b = emitExpr(n->children[2]);
                                                    std::string c = emitExpr(n->children[3]);
                                                    std::string d = emitExpr(n->children[4]);
                                                    out << "{ auto __t = ((" << x << ") - (" << a << ")) / ((" << b << ") - (" << a << ")); "
                                                        << x << " = (" << c << ") + __t * ((" << d << ") - (" << c << ")); }\n";
                                                }
                                                else if (n->type == "Bounds") {
                                                    // children: [0]=Var x, [1]=min, [2]=max
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
                                                    // value=name, child[0]=type string (e.g., "int")
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
                                                else if (n->type == "Input") {
                                                    // read from stdin into a variable
                                                    out << "std::cin >> " << n->value << ";\n";
                                                }
                                                else if (n->type == "Duration") {
                                                    // Lower to std::chrono with default to quantum_epochs for unknown units
                                                    std::string unit = (n->children.size() > 1 ? n->children[1]->value : "ms");
                                                    std::string u = unit;
                                                    std::transform(u.begin(), u.end(), u.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                                                    const char* chronoUnit =
                                                        (u == "ns" || u == "nanoseconds") ? "std::chrono::nanoseconds" :
                                                        (u == "us" || u == "microseconds") ? "std::chrono::microseconds" :
                                                        (u == "ms" || u == "milliseconds") ? "std::chrono::milliseconds" :
                                                        (u == "s" || u == "sec" || u == "seconds") ? "std::chrono::seconds" :
                                                        (u == "m" || u == "min" || u == "minutes") ? "std::chrono::minutes" :
                                                        (u == "h" || u == "hour" || u == "hours") ? "std::chrono::hours" : "quantum_epochs";
                                                    out << "{ using namespace std::chrono; auto __dur_val = " << emitExpr(n->children[0]) << "; "
                                                        << "auto __dur = duration_cast<" << chronoUnit << ">(duration<double>(__dur_val)); "
                                                        << "(void)__dur; }\n";
                                                }
                                                else if (n->type == "Derivative") {
                                                    // Numeric forward difference: derivative <expr> wrt <var>
                                                    if (n->children.size() >= 2 && n->children[1]->type == "Var") {
                                                        const std::string var = n->children[1]->value;
                                                        const std::string f = emitExpr(n->children[0]);
                                                        out << "{ auto __eps = 1e-6; auto __orig = (" << var << "); "
                                                            "auto __f0 = (double)(" << f << "); "
                                                            << var << " = __orig + __eps; "
                                                            "auto __f1 = (double)(" << f << "); "
                                                            << var << " = __orig; "
                                                            "double __deriv = (__f1 - __f0) / __eps; (void)__deriv; }\n";
                                                    }
                                                    else {
                                                        out << "{ /* derivative missing 'wrt <var>' */ double __deriv = 0.0; (void)__deriv; }\n";
                                                    }
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

