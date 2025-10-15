// Compiler.cpp - Single translation unit for a tiny DSL -> C++ transpiler (C++14)

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
        tokens.push_back(Token{t, v, line});
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
            } else {
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
                } else {
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

    tokens.push_back(Token{TokenType::END, "", line});
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
    Node* body = new Node{"Body", ""};
    while (peek().type != TokenType::END && !checkValue("}")) {
        body->children.push_back(parseStatement());
    }
    if (!matchValue("}")) throw std::runtime_error("Expected '}' to close a block at line " + std::to_string(peek().line));
    return body;
}

static Node* parsePrint() {
    advanceTok(); // consume 'Print'
    Node* n = new Node{"Print", ""};
    if (peek().type == TokenType::STRING) {
        n->value = advanceTok().value;
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseRet() {
    advanceTok(); // consume 'ret'
    Node* n = new Node{"Ret", ""};
    if (peek().type != TokenType::END && peek().type != TokenType::SYMBOL && !checkValue("[")) {
        n->value = advanceTok().value;
    }
    return n;
}

static Node* parseCall() {
    advanceTok(); // consume 'call'
    Node* n = new Node{"Call", ""};
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value;
    }
    skipBracketBlockIfPresent();
    return n;
}

static Node* parseLet() {
    advanceTok(); // consume 'let'
    Node* n = new Node{"Let", ""};
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // variable name
    }
    if (!matchValue("=")) {
        throw std::runtime_error("Expected '=' in let statement at line " + std::to_string(peek().line));
    }
    if (peek().type == TokenType::NUMBER) {
        n->children.push_back(new Node{"ExprNum", advanceTok().value});
    } else if (peek().type == TokenType::STRING) {
        n->children.push_back(new Node{"ExprStr", advanceTok().value});
    } else if (peek().type == TokenType::IDENT) {
        n->children.push_back(new Node{"ExprIdent", advanceTok().value});
    } else {
        throw std::runtime_error("Invalid expression in let statement at line " + std::to_string(peek().line));
    }
    return n;
}

static Node* parseLoop() {
    advanceTok(); // consume 'loop'
    Node* n = new Node{"Loop", ""};
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
    Node* n = new Node{"If", ""};
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
    Node* n = new Node{"Fn", ""};
    if (peek().type == TokenType::IDENT) {
        n->value = advanceTok().value; // function name
    } else {
        throw std::runtime_error("Expected function name after 'Fn' at line " + std::to_string(peek().line));
    }
    // Use braces for function body
    Node* body = parseBlock();
    n->children.push_back(body);
    return n;
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
    return new Node{"Unknown", v};
}

Node* parseProgram(const std::vector<Token>& t) {
    toks = t;
    pos = 0;
    Node* root = new Node{"Program", ""};
    while (peek().type != TokenType::END) {
        root->children.push_back(parseStatement());
    }
    return root;
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

static void emitNode(Node* n, std::ostringstream& out);

static void emitChildren(const std::vector<Node*>& cs, std::ostringstream& out) {
    for (auto* c : cs) emitNode(c, out);
}

static void emitNode(Node* n, std::ostringstream& out) {
    if (n->type == "Program") {
        out << "#include <iostream>\n";
        out << "\n";
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
        out << "for(" << n->value << "){\n";
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
        } else {
            Node* expr = n->children[0];
            out << "auto " << n->value << " = ";
            if (expr->type == "ExprStr") {
                out << "\"" << escapeCppString(expr->value) << "\"";
            } else {
                out << expr->value;
            }
            out << ";\n";
        }
    }
    else if (n->type == "Ret") {
        if (n->value.empty()) out << "return;\n";
        else out << "return " << n->value << ";\n";
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
        auto tokens = tokenize(src);
        Node* ast = parseProgram(tokens);
        std::string cpp = emitCPP(ast);

        std::ofstream out("compiler.cpp", std::ios::binary);
        if (!out) {
            std::cerr << "Failed to write compiler.cpp\n";
            return 1;
        }
        out << cpp;
        std::cout << "✅ Generated compiler.cpp\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

