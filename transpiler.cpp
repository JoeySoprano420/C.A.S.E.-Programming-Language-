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

#include "intelligence.hpp" // CIAM preprocessor (write_stdout, overlays/inspect, sandbox/audit, base-12)
#include "MacroRegistry.hpp" // [ADDED] Include the macro registry to enable macro persistence/integration

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
               s == "else"  || s == "Fn"   || s == "call"|| s == "let" ||
               s == "while"|| s == "break"|| s == "continue" ||
               s == "switch"|| s == "case"|| s == "default" ||
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
            "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "++", "--"
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
            } else {
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
                } else {
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
    if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
        Node* expr = parseExpression();
        if (expr) n->children.push_back(expr);
    }
    skipOptionalSemicolon();
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
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseLet() {
    advanceTok(); // 'let'
    Node* n = new Node{ "Let", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // variable name
    } else {
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
    // condition expression until '{'
    if (!checkValue("{")) {
        Node* condExpr = parseExpression();
        Node* cond = new Node{"Cond",""};
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
    Node* n = new Node{"While",""};
    Node* condExpr = parseExpression();
    Node* cond = new Node{"Cond",""};
    cond->children.push_back(condExpr);
    n->children.push_back(cond);
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
}

static Node* parseBreak() { advanceTok(); skipOptionalSemicolon(); return new Node{"Break",""}; }
static Node* parseContinue() { advanceTok(); skipOptionalSemicolon(); return new Node{"Continue",""}; }

static Node* parseSwitch() {
    advanceTok(); // 'switch'
    Node* n = new Node{"Switch",""};
    Node* condExpr = parseExpression();
    Node* cond = new Node{"Cond",""};
    cond->children.push_back(condExpr);
    n->children.push_back(cond);
    if (!matchValue("{")) throw std::runtime_error("Expected '{' after switch at line " + std::to_string(peek().line));
    while (!checkValue("}") && peek().type != TokenType::END) {
        if (matchValue("case")) {
            Node* caseNode = new Node{"Case",""};
            if (peek().type == TokenType::NUMBER || peek().type == TokenType::STRING || peek().type == TokenType::IDENT) {
                caseNode->value = advanceTok().value;
            } else {
                throw std::runtime_error("Expected case value at line " + std::to_string(peek().line));
            }
            Node* body = parseBlock();
            caseNode->children.push_back(body);
            n->children.push_back(caseNode);
        } else if (matchValue("default")) {
            Node* def = new Node{"Default",""};
            Node* body = parseBlock();
            def->children.push_back(body);
            n->children.push_back(def);
        } else {
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
    return new Node{"OverlayDecl",""};
}

// File I/O: open name "path" ["mode"], write name expr, writeln name expr, read name var, close name
static Node* parseOpen() {
    advanceTok(); // 'open'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected variable after 'open' at line " + std::to_string(peek().line));
    Node* n = new Node{"Open",""};
    n->value = advanceTok().value; // var name
    if (peek().type == TokenType::STRING) {
        Node* path = new Node{"Str", advanceTok().value};
        n->children.push_back(path);
    } else {
        throw std::runtime_error("Expected path string in open at line " + std::to_string(peek().line));
    }
    if (peek().type == TokenType::STRING) {
        Node* mode = new Node{"Str", advanceTok().value};
        n->children.push_back(mode);
    }
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseWriteLike(const std::string& kind) {
    advanceTok(); // 'write' or 'writeln'
    Node* n = new Node{kind, ""};
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
    Node* n = new Node{"Read",""};
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected stream variable after 'read' at line " + std::to_string(peek().line));
    n->value = advanceTok().value; // stream var
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected target variable in 'read' at line " + std::to_string(peek().line));
    Node* target = new Node{"Var", advanceTok().value};
    n->children.push_back(target);
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}
static Node* parseClose() {
    advanceTok(); // 'close'
    if (peek().type != TokenType::IDENT) throw std::runtime_error("Expected stream variable after 'close' at line " + std::to_string(peek().line));
    Node* n = new Node{"Close", advanceTok().value};
    skipOptionalSemicolon();
    return n;
}

// mutate (compiler-introspection hint)
static Node* parseMutate() {
    advanceTok(); // 'mutate'
    Node* n = new Node{"Mutate",""};
    if (peek().type == TokenType::IDENT) n->value = advanceTok().value;
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// scale x a b c d
static Node* parseScale() {
    advanceTok(); // 'scale'
    Node* n = new Node{"Scale",""};
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected identifier after 'scale' at line " + std::to_string(peek().line));
    Node* target = new Node{"Var", advanceTok().value};
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
    Node* n = new Node{"Bounds",""};
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected identifier after 'bounds' at line " + std::to_string(peek().line));
    Node* var = new Node{"Var", advanceTok().value};
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
    Node* n = new Node{"Checkpoint", advanceTok().value};
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// vbreak label
static Node* parseVBreak() {
    advanceTok(); // 'vbreak'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected label after 'vbreak' at line " + std::to_string(peek().line));
    Node* n = new Node{"VBreak", advanceTok().value};
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// channel name "Type"
static Node* parseChannel() {
    advanceTok(); // 'channel'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected channel name after 'channel' at line " + std::to_string(peek().line));
    Node* n = new Node{"Channel",""};
    n->value = advanceTok().value;
    if (!checkType(TokenType::STRING)) throw std::runtime_error("Expected type string for channel at line " + std::to_string(peek().line));
    Node* ty = new Node{"Str", advanceTok().value};
    n->children.push_back(ty);
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// send ch expr
static Node* parseSend() {
    advanceTok(); // 'send'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected channel name after 'send' at line " + std::to_string(peek().line));
    Node* n = new Node{"Send",""};
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
    Node* n = new Node{"Recv",""};
    n->value = advanceTok().value;
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected target variable for recv at line " + std::to_string(peek().line));
    Node* var = new Node{"Var", advanceTok().value};
    n->children.push_back(var);
    skipOptionalSemicolon();
    skipBracketBlockIfPresent();
    return n;
}

// schedule priority { body }
static Node* parseSchedule() {
    advanceTok(); // 'schedule'
    Node* n = new Node{"Schedule",""};
    // optional numeric priority
    if (checkType(TokenType::NUMBER)) {
        n->value = advanceTok().value;
    } else {
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
    return new Node{"Sync",""};
}

static Node* parseFn() {
    advanceTok(); // 'Fn'
    Node* n = new Node{ "Fn", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // function name
    } else {
        throw std::runtime_error("Expected function name after 'Fn' at line " + std::to_string(peek().line));
    }
    // Attach pending overlays as children (Overlay nodes)
    for (const auto& ov : gPendingOverlays) {
        Node* o = new Node{"Overlay", ov};
        n->children.push_back(o);
    }
    gPendingOverlays.clear();

    // Use braces for function body
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
}

// ----- Expression parsing (precedence) -----
//  || : 1, && : 2, == != : 3, < > <= >= : 4, + - : 5, * / % : 6

static int precedenceOf(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "==" || op == "!=") return 3;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
    if (op == "+" || op == "-") return 5;
    if (op == "*" || op == "/" || op == "%") return 6;
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
        std::string v = advanceTok().value == "true" ? "1" : "0";
        return new Node{ "Num", v };
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

static Node* parseUnary() {
    if (checkValue("!") || checkValue("-")) {
        std::string op = advanceTok().value;
        Node* rhs = parseUnary();
        Node* u = new Node{"Unary", op};
        u->children.push_back(rhs);
        return u;
    }
    return parsePrimary();
}

static Node* parseBinOpRHS(int minPrec, Node* lhs) {
    while (true) {
        const Token& t = peek();
        if (t.type != TokenType::SYMBOL) return lhs;
        std::string op = t.value;
        int prec = precedenceOf(op);
        if (prec < minPrec) return lhs;

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
}

static Node* parseExpression() {
    Node* lhs = parseUnary();
    return parseBinOpRHS(0, lhs);
}

static Node* parseInput() {
    advanceTok(); // 'input'
    if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected variable after 'input' at line " + std::to_string(peek().line));
    Node* n = new Node{"Input", advanceTok().value};
    skipOptionalSemicolon();
    return n;
}

static bool isAssignOp(const std::string& s) {
    return s == "=" || s == "+=" || s == "-=" || s == "*=" || s == "/=" || s == "%=" || s == "++" || s == "--";
}

static Node* parseAssignmentOrIncDec() {
    // Prefix ++/-- ident;
    if (peek().type == TokenType::SYMBOL && (checkValue("++") || checkValue("--"))) {
        std::string op = advanceTok().value;
        if (!checkType(TokenType::IDENT)) throw std::runtime_error("Expected identifier after '" + op + "' at line " + std::to_string(peek().line));
        Node* n = new Node{"Assign", op};
        Node* var = new Node{"Var", advanceTok().value};
        n->children.push_back(var);
        skipOptionalSemicolon();
        return n;
    }
    // ident (op) [expr];
    if (peek().type == TokenType::IDENT) {
        Token idTok = peek();
        if (pos + 1 < toks.size() && toks[pos + 1].type == TokenType::SYMBOL && isAssignOp(toks[pos + 1].value)) {
            advanceTok(); // ident
            std::string op = advanceTok().value; // assignment operator
            Node* n = new Node{"Assign", op};
            Node* var = new Node{"Var", idTok.value};
            n->children.push_back(var);
            if (op != "++" && op != "--") {
                Node* expr = parseExpression();
                n->children.push_back(expr);
            }
            skipOptionalSemicolon();
            return n;
        }
        // Postfix ++/--
        if (pos + 1 < toks.size() && toks[pos + 1].type == TokenType::SYMBOL && (toks[pos + 1].value == "++" || toks[pos + 1].value == "--")) {
            std::string name = advanceTok().value; // ident
            std::string op = advanceTok().value;   // ++/--
            Node* n = new Node{"Assign", op};
            Node* var = new Node{"Var", name};
            n->children.push_back(var);
            skipOptionalSemicolon();
            return n;
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
        std::cerr << "[warn] use of undefined variable '" << e->value << "'\n";
        return TypeKind::Unknown;
    }
    if (e->type == "Unary") {
        // '!' or unary '-' => numeric (0/1 or negation)
        return TypeKind::Number;
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
    if (n->type == "Assign") {
        // children: [0]=Var, [1]=expr? (missing for ++/--)
        if (scopes.empty()) scopes.push_back({});
        if (n->value == "++" || n->value == "--") {
            scopes.back()[n->children[0]->value] = TypeKind::Number;
        } else {
            TypeKind t = inferExpr(n->children.size() > 1 ? n->children[1] : nullptr, scopes);
            scopes.back()[n->children[0]->value] = t;
        }
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

// Built-in mutation: evolutionary re-optimization (re-visit and re-run optimize a few rounds)
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
    std::ostringstream ss; ss << v; return new Node{"Num", ss.str()};
}
static Node* clone(Node* n) {
    if (!n) return nullptr;
    Node* c = new Node{n->type, n->value};
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
        return new Node{"Str", L->value + R->value};
    }

    double a, b;
    if (isNum(L) && isNum(R) && toDouble(L->value, a) && toDouble(R->value, b)) {
        if (n->value == "+") return makeNum(a + b);
        if (n->value == "-") return makeNum(a - b);
        if (n->value == "*") return makeNum(a * b);
        if (n->value == "/") return makeNum(b == 0 ? 0 : (a / b));
        if (n->value == "%") return makeNum(static_cast<long long>(a) % static_cast<long long>(b));
        if (n->value == "<")  return makeNum((a <  b) ? 1 : 0);
        if (n->value == ">")  return makeNum((a >  b) ? 1 : 0);
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
    if (e->type == "Unary") {
        std::string rhs = emitExpr(e->children.empty() ? nullptr : e->children[0]);
        return "(" + e->value + rhs + ")";
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
    std::function<void(Node*)> flatten = [&](Node* e){
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
    m.erase(std::remove_if(m.begin(), m.end(), [](unsigned char ch){ return std::isspace(ch); }), m.end());
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
        } else {
            out << "std::cout << \"" << escapeCppString(n->value) << "\" << std::endl;\n";
        }
    }
    else if (n->type == "Input") {
        out << "std::cin >> " << n->value << ";\n";
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
            } else if (c->type == "Default") {
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
        } else {
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
        } else {
            out << "auto " << n->value << " = " << emitExpr(n->children[0]) << ";\n";
        }
    }
    else if (n->type == "Assign") {
        std::string name = n->children[0]->value;
        if (n->value == "++" || n->value == "--") {
            out << name << n->value << ";\n";
        } else {
            std::string rhs = n->children.size() > 1 ? emitExpr(n->children[1]) : "0";
            out << name << " " << n->value << " " << rhs << ";\n";
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
        // [ADDED] Load previously persisted macros
        ciam::MacroRegistry::load();

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

        // Collect overlays to enable CIAM-like behaviors (also registers overlay macros)
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

        // [ADDED] Persist macros after this run (captures overlay-driven or fixer macros)
        ciam::MacroRegistry::persist();

        // Write symbolic replay if enabled
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

