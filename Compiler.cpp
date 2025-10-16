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
        return s == "Print" || s == "ret" || s == "loop" || s == "if" ||
            s == "else" || s == "Fn" || s == "call" || s == "let";
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

        // Numbers (simple integer or decimal)
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = i++;
            bool hasDot = false;
            while (i < src.size() &&
                (std::isdigit(static_cast<unsigned char>(src[i])) || (!hasDot && src[i] == '.'))) {
                if (src[i] == '.') hasDot = true;
                ++i;
            }
            push(TokenType::NUMBER, src.substr(start, i - start));
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
    std::string type;           // e.g., "Program", "Print", "Let", ...
    std::string value;          // payload (e.g., string contents, identifier, condition, etc.)
    std::vector<Node*> children;
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
static bool matchValue(const std::string& v) {
    if (checkValue(v)) {
        advanceTok();
        return true;
    }
    return false;
}

static Node* parseStatement(); // fwd
static Node* parseExpression(); // fwd

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

static Node* parsePrint() {
    advanceTok(); // consume 'Print'
    Node* n = new Node{ "Print", "" };
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseRet() {
    advanceTok(); // consume 'ret'
    Node* n = new Node{ "Ret", "" };
    if (peek().type != TokenType::END && !checkValue("[") && !checkValue("}")) {
        Node* expr = parseExpression();
        if (expr) n->children.push_back(expr);
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseCall() {
    advanceTok(); // consume 'call'
    Node* n = new Node{ "Call", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseLet() {
    advanceTok(); // consume 'let'
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
    advanceTok(); // consume 'loop'
    Node* n = new Node{ "Loop", "" };
    // Accept either a string containing the for(...) header or a bare identifier/expression token
    if (peek().type == TokenType::STRING || peek().type == TokenType::IDENT || peek().type == TokenType::NUMBER) {
        n->value = advanceTok().value; // e.g., "int i=0; i<10; i++"
    }
    Node* body = parseBlock();
    n->children.push_back(body);
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseIf() {
    advanceTok(); // consume 'if'
    Node* n = new Node{ "If", "" };
    // Very simple condition capture: next non-symbol token as condition
    if (peek().type != TokenType::SYMBOL && peek().type != TokenType::END && !checkValue("{")) {
        n->value = advanceTok().value;
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

static Node* parseFn() {
    advanceTok(); // consume 'Fn'
    Node* n = new Node{ "Fn", "" };
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // function name
    }
    else {
        throw std::runtime_error("Expected function name after 'Fn' at line " + std::to_string(peek().line));
    }
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
    if (v == "Print") return parsePrint();
    if (v == "ret") return parseRet();
    if (v == "loop") return parseLoop();
    if (v == "if") return parseIf();
    if (v == "Fn") return parseFn();
    if (v == "call") return parseCall();
    if (v == "let") return parseLet();

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

// ------------------------ Optimizer (constant fold, DCE, inlining, peephole) ------------------------

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
        // x + 0 -> x | 0 + x -> x
        if (n->value == "+") {
            if (isNum(L) && L->value == "0") return clone(R);
            if (isNum(R) && R->value == "0") return clone(L);
        }
        // x - 0 -> x
        if (n->value == "-" && isNum(R) && R->value == "0") return clone(L);
        // x * 1 -> x | 1 * x -> x | x * 0 -> 0 | 0 * x -> 0
        if (n->value == "*") {
            if (isNum(L) && L->value == "1") return clone(R);
            if (isNum(R) && R->value == "1") return clone(L);
            if ((isNum(L) && L->value == "0") || (isNum(R) && R->value == "0")) return makeNum(0);
        }
        // x / 1 -> x
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

    // String concatenation folding for '+'
    if (n->value == "+" && isStr(L) && isStr(R)) {
        return new Node{"Str", L->value + R->value};
    }

    // Numeric folding
    double a, b;
    if (isNum(L) && isNum(R) && toDouble(L->value, a) && toDouble(R->value, b)) {
        if (n->value == "+") return makeNum(a + b);
        if (n->value == "-") return makeNum(a - b);
        if (n->value == "*") return makeNum(a * b);
        if (n->value == "/") return makeNum(b == 0 ? 0 : (a / b));
        if (n->value == "<")  return makeNum((a <  b) ? 1 : 0);
        if (n->value == ">")  return makeNum((a >  b) ? 1 : 0);
        if (n->value == "<=") return makeNum((a <= b) ? 1 : 0);
        if (n->value == ">=") return makeNum((a >= b) ? 1 : 0);
        if (n->value == "==") return makeNum((a == b) ? 1 : 0);
        if (n->value == "!=") return makeNum((a != b) ? 1 : 0);
    }
    // Peephole algebraic
    return peephole(n);
}

static Node* fold(Node* n) {
    if (!n) return n;
    if (n->type == "BinOp") return foldBinop(n);
    for (size_t i = 0; i < n->children.size(); ++i) n->children[i] = fold(n->children[i]);
    // If we had expression conditions, we could fold if-branches here.
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

// Inline: single-expression-return, non-recursive
struct InlineFn {
    std::vector<std::string> params;
    Node* expr = nullptr;
    bool recursive = false;
};

static void collectInlineFns(Node* root, std::unordered_map<std::string, InlineFn>& table) {
    if (!root || root->type != "Program") return;
    for (auto* fn : root->children) {
        if (fn->type != "Fn") continue;
        InlineFn cand;
        Node* body = nullptr;
        std::vector<std::string> params;
        for (auto* ch : fn->children) {
            if (ch->type == "Param") {
                if (!ch->children.empty()) params.push_back(ch->children[0]->value);
            } else if (ch->type == "Body") {
                body = ch;
            }
        }
        if (!body) continue;
        if (body->children.size() == 1 && body->children[0]->type == "Ret" && !body->children[0]->children.empty()) {
            cand.params = params;
            cand.expr = clone(body->children[0]->children[0]);
            // simple recursion scan
            std::function<void(Node*)> scan = [&](Node* n) {
                if (!n) return;
                if (n->type == "CallExpr" && n->value == fn->value) cand.recursive = true;
                for (auto* c : n->children) scan(c);
            };
            scan(cand.expr);
            if (!cand.recursive) table[fn->value] = cand;
        }
    }
}

static Node* subst(Node* n, const std::unordered_map<std::string, Node*>& env) {
    if (!n) return nullptr;
    if (n->type == "Var") {
        auto it = env.find(n->value);
        if (it != env.end()) return clone(it->second);
        return new Node{"Var", n->value};
    }
    Node* c = new Node{n->type, n->value};
    for (auto* ch : n->children) c->children.push_back(subst(ch, env));
    return c;
}

static Node* inlineCalls(Node* n, const std::unordered_map<std::string, InlineFn>& table) {
    if (!n) return n;
    for (size_t i = 0; i < n->children.size(); ++i) n->children[i] = inlineCalls(n->children[i], table);
    if (n->type == "CallExpr") {
        auto it = table.find(n->value);
        if (it != table.end()) {
            const InlineFn& f = it->second;
            std::unordered_map<std::string, Node*> env;
            for (size_t i = 0; i < f.params.size() && i < n->children.size(); ++i) env[f.params[i]] = n->children[i];
            Node* inlined = subst(f.expr, env);
            return fold(inlined);
        }
    }
    return n;
}

static void optimize(Node*& root) {
    if (!root) return;
    root = fold(root);   // constant folding + peephole
    dce(root);           // dead code elimination
    // inline after fold
    std::unordered_map<std::string, InlineFn> table;
    collectInlineFns(root, table);
    root = inlineCalls(root, table);
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

// Loop hints: parse header markers like "@unroll(4) @vectorize @parallel @omp"
struct LoopHints { int unroll = 0; bool vectorize = false; bool ivdep = false; bool omp = false; };
static LoopHints extractLoopHints(std::string& header) {
    LoopHints h{};
    auto eraseAll = [&](const std::string& tag) {
        for (;;) { auto p = header.find(tag); if (p == std::string::npos) break; header.erase(p, tag.size()); }
    };
    // @unroll(N)
    {
        auto p = header.find("@unroll(");
        if (p != std::string::npos) {
            auto q = header.find(")", p + 8);
            if (q != std::string::npos) {
                std::string n = header.substr(p + 8, q - (p + 8));
                try { h.unroll = std::stoi(n); } catch (...) {}
                header.erase(p, (q - p) + 1);
            }
        }
    }
    if (header.find("@vectorize") != std::string::npos) { h.vectorize = true; eraseAll("@vectorize"); }
    if (header.find("@ivdep")     != std::string::npos) { h.ivdep     = true; eraseAll("@ivdep"); }
    if (header.find("@omp")       != std::string::npos) { h.omp       = true; eraseAll("@omp"); }
    if (header.find("@parallel")  != std::string::npos) { h.omp       = true; eraseAll("@parallel"); } // treat as omp
    return h;
}

static void emitNode(Node* n, std::ostringstream& out) {
    if (n->type == "Program") {
        out << "#include <iostream>\n";
        out << "#if defined(_OPENMP)\n#include <omp.h>\n#endif\n\n";
        // Emit function declarations/definitions first
        for (auto* c : n->children)
            if (c->type == "Fn") emitNode(c, out);
        out << "int main(){\n";
        for (auto* c : n->children)
            if (c->type != "Fn") emitNode(c, out);
        out << "return 0;\n}\n";
    }
    else if (n->type == "Print") {
        out << "std::cout << \"" << escapeCppString(n->value) << "\" << std::endl;\n";
    }
    else if (n->type == "Loop") {
        std::string header = n->value;
        LoopHints hints = extractLoopHints(header);
        // Vectorization and unrolling pragmas (vendor-guarded)
#if defined(__clang__)
        if (hints.vectorize) out << "#pragma clang loop vectorize(enable)\n";
        if (hints.unroll > 1) out << "#pragma clang loop unroll(enable) unroll_count(" << hints.unroll << ")\n";
#elif defined(__GNUG__)
        if (hints.vectorize) out << "#pragma GCC ivdep\n";
        if (hints.unroll > 1) out << "#pragma GCC unroll " << hints.unroll << "\n";
#elif defined(_MSC_VER)
        if (hints.ivdep || hints.vectorize) out << "#pragma loop(ivdep)\n";
        // MSVC unroll pragma not standardized; rely on /O2 auto-unroll.
#endif
#if defined(_OPENMP)
        if (hints.omp) out << "#pragma omp parallel for\n";
#endif
        out << "for(" << header << "){\n";
        if (!n->children.empty()) emitChildren(n->children[0]->children, out);
        out << "}\n";
    }
    else if (n->type == "If") {
        std::string cond = n->value.empty() ? "/* condition */" : n->value;
        out << "if(" << cond << "){\n";
        if (!n->children.empty()) emitChildren(n->children[0]->children, out);
        out << "}\n";
        if (n->children.size() > 1) {
            out << "else{\n";
            emitChildren(n->children[1]->children, out);
            out << "}\n";
        }
    }
    else if (n->type == "Fn") {
        out << "void " << n->value << "(){\n";
        if (!n->children.empty()) emitChildren(n->children[0]->children, out);
        out << "}\n";
    }
    else if (n->type == "Call") {
        out << n->value << "();\n";
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

        auto tokens = tokenize(src);
        Node* ast = parseProgram(tokens);

        // AOT optimizations (constant fold, peephole, DCE, inlining)
        optimize(ast);

        std::string cpp = emitCPP(ast);

        std::ofstream out("compiler.cpp", std::ios::binary);
        if (!out) {
            std::cerr << "Failed to write compiler.cpp\n";
            return 1;
        }
        out << cpp;
        std::cout << "[OK] Generated compiler.cpp\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

