//=============================================================================
//  🌌 Violet Aura Creations — CASE Programming Language Transpiler
//=============================================================================

#pragma execution_character_set("UTF-8")

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <queue>      // For channel implementation
#include <thread>     // For thread support
#include <future>     // For async support
#include <mutex>      // For synchronization

// Forward declarations for CIAM
namespace ciam {
    class Preprocessor {
    public:
        std::string Process(const std::string& src) {
            // CIAM preprocessing logic
            return src;
        }
    };
}

// -----------------------------------------------------------------------------
// TOKEN TYPES
// -----------------------------------------------------------------------------

enum class TokenType {
    Keyword, Identifier, Number, String, Operator, Symbol, Comment,
    EndOfFile, Unknown
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

static std::string tokenTypeToString(TokenType t) {
    switch (t) {
    case TokenType::Keyword:   return "Keyword";
    case TokenType::Identifier:return "Identifier";
    case TokenType::Number:    return "Number";
    case TokenType::String:    return "String";
    case TokenType::Operator:  return "Operator";
    case TokenType::Symbol:    return "Symbol";
    case TokenType::Comment:   return "Comment";
    case TokenType::EndOfFile: return "EndOfFile";
    default:                   return "Unknown";
    }
}

// -----------------------------------------------------------------------------
// SYMBOL TABLE
// -----------------------------------------------------------------------------

enum class SymbolKind {
    Variable,
    Function,
    Parameter,
    Constant,
    Type
};

struct SymbolInfo {
    std::string name;
    std::string type;
    SymbolKind kind;
    int line;
    int column;
    bool isInitialized;
    bool isUsed;
    int scopeLevel;

    SymbolInfo()
        : kind(SymbolKind::Variable), line(0), column(0),
        isInitialized(false), isUsed(false), scopeLevel(0) {}

    SymbolInfo(const std::string& n, const std::string& t, SymbolKind k,
        int l = 0, int c = 0, int scope = 0)
        : name(n), type(t), kind(k), line(l), column(c),
        isInitialized(false), isUsed(false), scopeLevel(scope) {}
};

class SymbolTable {
public:
    SymbolTable() : currentScope(0), errorCount(0) {
        enterScope(); // Global scope
    }

    void enterScope() {
        scopes.push_back(std::unordered_map<std::string, SymbolInfo>());
        currentScope++;
    }

    void exitScope() {
        if (!scopes.empty()) {
            const auto& scope = scopes.back();
            for (const auto& pair : scope) {
                if (!pair.second.isUsed && pair.second.kind == SymbolKind::Variable) {
                    reportWarning("Unused variable '" + pair.first + "' declared at line "
                        + std::to_string(pair.second.line));
                }
            }
            scopes.pop_back();
            currentScope--;
        }
    }

    bool declare(const std::string& name, const std::string& type,
        int line = 0, int column = 0, SymbolKind kind = SymbolKind::Variable) {
        if (scopes.empty()) return false;
        auto& currentScopeMap = scopes.back();
        if (currentScopeMap.count(name)) return false;
        currentScopeMap[name] = SymbolInfo(name, type, kind, line, column, currentScope);
        return true;
    }

    bool lookup(const std::string& name, std::string& type) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                type = found->second.type;
                return true;
            }
        }
        return false;
    }

    void markUsed(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second.isUsed = true;
                return;
            }
        }
    }

    void markInitialized(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second.isInitialized = true;
                return;
            }
        }
    }

    void incrementErrorCount() { ++errorCount; }
    int getErrorCount() const { return errorCount; }
    bool hasErrors() const { return errorCount > 0; }

    std::vector<std::string> getAllSymbolNames() const {
        std::vector<std::string> names;
        std::unordered_set<std::string> seen;
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            for (const auto& pair : *it) {
                if (seen.find(pair.first) == seen.end()) {
                    names.push_back(pair.first);
                    seen.insert(pair.first);
                }
            }
        }
        return names;
    }

private:
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
    int currentScope;
    int errorCount;

    void reportWarning(const std::string& msg) const {
        std::cerr << "\033[1;33m[Warning]\033[0m " << msg << "\n";
    }
};

// -----------------------------------------------------------------------------
// LEXER - Updated for C.A.S.E. syntax
// -----------------------------------------------------------------------------

class Lexer {
public:
    explicit Lexer(const std::string& src)
        : source(src), pos(0), line(1), column(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (!isAtEnd()) {
            skipWhitespaceAndComments();
            if (isAtEnd()) break;

            size_t startCol = column;
            char c = peek();

            if (std::isalpha(c) || c == '_')
                tokens.push_back(identifier(startCol));
            else if (std::isdigit(c))
                tokens.push_back(number(startCol));
            else if (c == '"')
                tokens.push_back(string(startCol));
            else if (c == '(' || c == ')')
                tokens.push_back(parenthesis(startCol));
            else if (isOperatorChar(c))
                tokens.push_back(operate(startCol));
            else if (isSymbolChar(c))
                tokens.push_back(symbol(startCol));
            else {
                reportError("Unexpected character", c);
                tokens.push_back({ TokenType::Unknown, std::string(1, advance()), line, (int)startCol });
            }
        }
        tokens.push_back({ TokenType::EndOfFile, "", line, (int)column });
        return tokens;
    }

private:
    std::string source;
    size_t pos;
    int line;
    int column;

    const std::unordered_set<std::string> keywords = {
        "Print","ret","loop","if","else","Fn","call","let","while","break","continue",
        "switch","case","default","overlay","open","write","writeln","read","close",
        "mutate","scale","bounds","checkpoint","vbreak","channel","send","recv","sync",
        "schedule","input","true","false","struct","enum","union","typedef","const",
        "volatile","static","extern","inline","auto","void","int","float","double",
        "char","bool","string","array","list","dict","thread","async","batch","parallel",
        "serialize","deserialize","compress","decompress","obfuscate","deobfuscate",
        "ping","resource","environment","CIAM","end"
    };

    bool isAtEnd() const { return pos >= source.size(); }
    char peek() const { return isAtEnd() ? '\0' : source[pos]; }
    char peekNext() const { return (pos + 1 < source.size()) ? source[pos + 1] : '\0'; }

    char advance() {
        char c = source[pos++];
        column++;
        return c;
    }

    static bool isOperatorChar(char c) {
        return std::string("+-*/%=!<>|&^~").find(c) != std::string::npos;
    }

    static bool isSymbolChar(char c) {
        return std::string("{}[];,.:").find(c) != std::string::npos;
    }

    void skipWhitespaceAndComments() {
        while (!isAtEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
            if (c == '\n') { line++; column = 1; advance(); continue; }
            if (c == '#') {
                while (!isAtEnd() && peek() != '\n') advance();
                continue;
            }
            break;
        }
    }

    Token identifier(size_t startCol) {
        std::string value;
        while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_'))
            value += advance();

        if (keywords.count(value))
            return { TokenType::Keyword, value, line, (int)startCol };
        return { TokenType::Identifier, value, line, (int)startCol };
    }

    Token number(size_t startCol) {
        std::string value;
        while (!isAtEnd() && std::isdigit(peek())) value += advance();
        if (peek() == '.' && std::isdigit(peekNext())) {
            value += advance();
            while (!isAtEnd() && std::isdigit(peek())) value += advance();
        }
        return { TokenType::Number, value, line, (int)startCol };
    }

    Token string(size_t startCol) {
        advance(); // consume opening quote
        std::string value;
        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\n') { line++; column = 1; }
            if (peek() == '\\' && peekNext() != '\0') {
                advance();
                char esc = advance();
                switch (esc) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                default: value += esc; break;
                }
            }
            else {
                value += advance();
            }
        }
        if (peek() == '"') advance();
        else reportError("Unterminated string literal");
        return { TokenType::String, value, line, (int)startCol };
    }

    Token parenthesis(size_t startCol) {
        return { TokenType::Symbol, std::string(1, advance()), line, (int)startCol };
    }

    Token operate(size_t startCol) {
        std::string op;
        op += advance();
        if (!isAtEnd() && isOperatorChar(peek())) op += advance();
        return { TokenType::Operator, op, line, (int)startCol };
    }

    Token symbol(size_t startCol) {
        return { TokenType::Symbol, std::string(1, advance()), line, (int)startCol };
    }

    void reportError(const std::string& msg, char c = '\0') {
        std::cerr << "\033[1;31m[Lexer Error]\033[0m Line " << line
            << ", Col " << column << ": " << msg;
        if (c != '\0') std::cerr << " ('" << c << "')";
        std::cerr << "\n";
    }
};

// -----------------------------------------------------------------------------
// AST NODES
// -----------------------------------------------------------------------------

struct Node { 
    virtual ~Node() = default; 
    virtual void print(int depth = 0) const = 0; 
};
using NodePtr = std::shared_ptr<Node>;

static void indent(int depth) { 
    for (int i = 0; i < depth; i++) std::cout << "  "; 
}

struct Expr : Node {};
struct Stmt : Node {};

struct Block : Node { 
    std::vector<NodePtr> statements; 
    void print(int d = 0) const override {
        indent(d); std::cout << "Block\n";
        for (auto& s : statements) s->print(d + 1);
    }
};

struct PrintStmt : Stmt { 
    NodePtr expr; 
    void print(int d = 0) const override {
        indent(d); std::cout << "PrintStmt\n";
        if (expr) expr->print(d + 1);
    }
};

struct IfStmt : Stmt { 
    NodePtr condition, thenBlock, elseBlock; 
    void print(int d = 0) const override {
        indent(d); std::cout << "IfStmt\n";
        indent(d + 1); std::cout << "Condition:\n";
        condition->print(d + 2);
        indent(d + 1); std::cout << "Then:\n";
        thenBlock->print(d + 2);
        if (elseBlock) {
            indent(d + 1); std::cout << "Else:\n";
            elseBlock->print(d + 2);
        }
    }
};

struct LoopStmt : Stmt {
    std::string loopHeader;
    NodePtr block;
    void print(int d = 0) const override {
        indent(d); std::cout << "LoopStmt [" << loopHeader << "]\n";
        if (block) block->print(d + 1);
    }
};

struct FunctionDecl : Stmt { 
    std::string name; 
    std::string params;
    NodePtr body; 
    void print(int d = 0) const override {
        indent(d); std::cout << "FunctionDecl: " << name;
        if (!params.empty()) std::cout << " (" << params << ")";
        std::cout << "\n";
        body->print(d + 1);
    }
};

struct Literal : Expr { 
    std::string value; 
    void print(int d = 0) const override {
        indent(d); std::cout << "Literal: " << value << "\n";
    }
};

struct Identifier : Expr { 
    std::string name; 
    void print(int d = 0) const override {
        indent(d); std::cout << "Identifier: " << name << "\n";
    }
};

struct ReturnStmt : Stmt {
    NodePtr value;
    void print(int d = 0) const override {
        indent(d); std::cout << "Return\n";
        if (value) value->print(d + 1);
    }
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<NodePtr> args;
    void print(int d = 0) const override {
        indent(d); std::cout << "Call " << callee << "\n";
        for (auto& a : args) a->print(d + 1);
    }
};

struct BinaryExpr : Expr {
    NodePtr left, right;
    std::string op;
    void print(int d = 0) const override {
        indent(d); std::cout << "BinaryExpr " << op << "\n";
        left->print(d + 1);
        right->print(d + 1);
    }
};

struct VarDecl : Stmt {
    std::string name, type;
    NodePtr initializer;
    void print(int d = 0) const override {
        indent(d); std::cout << "VarDecl " << type << " " << name << "\n";
        if (initializer) initializer->print(d + 1);
    }
};

// Additional AST Nodes for extended features

struct WhileStmt : Stmt {
    NodePtr condition;
    NodePtr block;
    void print(int d = 0) const override {
        indent(d); std::cout << "WhileStmt\n";
        indent(d+1); std::cout << "Condition:\n";
        condition->print(d+2);
        if (block) block->print(d+1);
    }
};

struct BreakStmt : Stmt {
    void print(int d = 0) const override {
        indent(d); std::cout << "Break\n";
    }
};

struct ContinueStmt : Stmt {
    void print(int d = 0) const override {
        indent(d); std::cout << "Continue\n";
    }
};

struct SwitchStmt : Stmt {
    NodePtr condition;
    std::vector<std::pair<std::string, NodePtr>> cases; // (value, block)
    NodePtr defaultBlock;
    void print(int d = 0) const override {
        indent(d); std::cout << "SwitchStmt\n";
        condition->print(d+1);
        for (auto& c : cases) {
            indent(d+1); std::cout << "Case " << c.first << ":\n";
            c.second->print(d+2);
        }
        if (defaultBlock) {
            indent(d+1); std::cout << "Default:\n";
            defaultBlock->print(d+2);
        }
    }
};

struct ThreadStmt : Stmt {
    NodePtr block;
    void print(int d = 0) const override {
        indent(d); std::cout << "ThreadStmt\n";
        if (block) block->print(d+1);
    }
};

struct AsyncStmt : Stmt {
    NodePtr expr;
    void print(int d = 0) const override {
        indent(d); std::cout << "AsyncStmt\n";
        if (expr) expr->print(d+1);
    }
};

struct ChannelDecl : Stmt {
    std::string name;
    std::string channelType;
    void print(int d = 0) const override {
        indent(d); std::cout << "ChannelDecl: " << name << " <" << channelType << ">\n";
    }
};

struct SendStmt : Stmt {
    std::string channel;
    NodePtr value;
    void print(int d = 0) const override {
        indent(d); std::cout << "SendStmt -> " << channel << "\n";
        if (value) value->print(d+1);
    }
};

struct RecvStmt : Stmt {
    std::string channel;
    std::string targetVar;
    void print(int d = 0) const override {
        indent(d); std::cout << "RecvStmt <- " << channel << " => " << targetVar << "\n";
    }
};

struct StructDecl : Stmt {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields; // (type, name)
    void print(int d = 0) const override {
        indent(d); std::cout << "StructDecl: " << name << "\n";
        for (auto& f : fields) {
            indent(d+1); std::cout << f.first << " " << f.second << "\n";
        }
    }
};
// -----------------------------------------------------------------------------
// PARSER - Updated for C.A.S.E. syntax with [end]
// -----------------------------------------------------------------------------

class Parser {
public:
    explicit Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}
    NodePtr parse();

private:
    const std::vector<Token>& tokens;
    size_t pos;

    const Token& peek() const { return tokens[pos]; }
    const Token& advance() { return tokens[pos++]; }
    bool match(const std::string& kw);
    bool check(const std::string& kw) const;
    bool isAtEnd() const { return peek().type == TokenType::EndOfFile; }

    NodePtr parseStatement();

    NodePtr parseBlock();
    NodePtr parseExpression();
    NodePtr parsePrimary();
    NodePtr parseBinOpRHS(int minPrec, NodePtr lhs);
    int precedenceOf(const std::string& op);
    bool matchEnd();

    NodePtr parseWhile();
    NodePtr parseBreak();
    NodePtr parseContinue();
    NodePtr parseSwitch();
    NodePtr parseThread();
    NodePtr parseAsync();
    NodePtr parseChannel();
    NodePtr parseSend();
    NodePtr parseRecv();
    NodePtr parseStruct();
};

bool Parser::match(const std::string& kw) {
    if (peek().lexeme == kw) { advance(); return true; }
    return false;
}

bool Parser::check(const std::string& kw) const {
    return peek().lexeme == kw;
}

bool Parser::matchEnd() {
    if (check("[")) {
        advance();
        if (check("end")) {
            advance();
            if (check("]")) {
                advance();
                return true;
            }
        }
    }
    return false;
}

int Parser::precedenceOf(const std::string& op) {
    if (op == "*" || op == "/") return 20;
    if (op == "+" || op == "-") return 10;
    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") return 5;
    return -1;
}

NodePtr Parser::parse() {
    auto root = std::make_shared<Block>();
    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) root->statements.push_back(stmt);
    }
    return root;
}

NodePtr Parser::parseStatement() {
    // Fn declaration: Fn name "params" ( body ) [end]
    if (match("Fn")) {
        auto fnDecl = std::make_shared<FunctionDecl>();
        if (peek().type == TokenType::Identifier) {
            fnDecl->name = advance().lexeme;
        }

        // Optional parameters in quotes
        if (peek().type == TokenType::String) {
            fnDecl->params = advance().lexeme;
        }

        // Parse body in parentheses
        if (match("(")) {
            auto body = std::make_shared<Block>();
            while (!check(")") && !isAtEnd()) {
                auto stmt = parseStatement();
                if (stmt) body->statements.push_back(stmt);
            }
            match(")");
            fnDecl->body = body;
        }

        matchEnd(); // consume [end]
        return fnDecl;
    }

    // Print statement: Print "text" [end] or Print expr [end]
    if (match("Print")) {
        auto stmt = std::make_shared<PrintStmt>();
        stmt->expr = parseExpression();
        matchEnd();
        return stmt;
    }

    // if statement: if cond { ... } else { ... } [end]
    if (match("if")) {
        auto ifStmt = std::make_shared<IfStmt>();
        ifStmt->condition = parseExpression();
        ifStmt->thenBlock = parseBlock();
        if (match("else")) {
            ifStmt->elseBlock = parseBlock();
        }
        matchEnd();
        return ifStmt;
    }

    // loop statement: loop "header" { body } [end]
    if (match("loop")) {
        auto loopStmt = std::make_shared<LoopStmt>();
        if (peek().type == TokenType::String) {
            loopStmt->loopHeader = advance().lexeme;
        }
        loopStmt->block = parseBlock();
        matchEnd();
        return loopStmt;
    }

    // let statement: let var = expr
    if (match("let")) {
        auto varDecl = std::make_shared<VarDecl>();
        if (peek().type == TokenType::Identifier) {
            varDecl->name = advance().lexeme;
        }
        if (match("=")) {
            varDecl->initializer = parseExpression();
        }
        varDecl->type = "auto";
        return varDecl;
    }

    // ret statement: ret expr
    if (match("ret")) {
        auto retStmt = std::make_shared<ReturnStmt>();
        if (!check("[") && !isAtEnd()) {
            retStmt->value = parseExpression();
        }
        return retStmt;
    }

    // call statement: call funcName args [end]
    if (match("call")) {
        auto callExpr = std::make_shared<CallExpr>();
        if (peek().type == TokenType::Identifier) {
            callExpr->callee = advance().lexeme;
        }
        // Parse arguments until [end]
        while (!check("[") && !isAtEnd() && peek().type != TokenType::EndOfFile) {
            callExpr->args.push_back(parseExpression());
            if (check(",")) advance();
        }
        matchEnd();
        return callExpr;
    }

    // Additional statements for extended features

    if (match("while")) {
        return parseWhile();
    }

    if (match("break")) {
        return parseBreak();
    }

    if (match("continue")) {
        return parseContinue();
    }

    if (match("switch")) {
        return parseSwitch();
    }

    if (match("thread")) {
        return parseThread();
    }

    if (match("async")) {
        return parseAsync();
    }

    // channel declaration: channel chName <type>
    if (match("channel")) {
        return parseChannel();
    }

    // send statement: send channelName, value
    if (match("send")) {
        return parseSend();
    }

    // receive statement: recv channelName => varName
    if (match("recv")) {
        return parseRecv();
    }

    return parseExpression();
}

NodePtr Parser::parseBlock() {
    match("{");
    auto blk = std::make_shared<Block>();
    while (!check("}") && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) blk->statements.push_back(stmt);
    }
    match("}");
    return blk;
}

NodePtr Parser::parseExpression() {
    NodePtr lhs = parsePrimary();
    return parseBinOpRHS(0, lhs);
}

NodePtr Parser::parsePrimary() {
    if (peek().type == TokenType::String || peek().type == TokenType::Number) {
        auto lit = std::make_shared<Literal>();
        lit->value = advance().lexeme;
        return lit;
    }
    if (peek().type == TokenType::Identifier) {
        auto id = std::make_shared<Identifier>();
        id->name = advance().lexeme;
        return id;
    }
    if (match("(")) {
        NodePtr inner = parseExpression();
        match(")");
        return inner;
    }
    return std::make_shared<Literal>(); // fallback
}

NodePtr Parser::parseBinOpRHS(int minPrec, NodePtr lhs) {
    while (true) {
        if (peek().type != TokenType::Operator) return lhs;
        std::string op = peek().lexeme;
        int prec = precedenceOf(op);
        if (prec < minPrec) return lhs;

        advance();
        NodePtr rhs = parsePrimary();
        int nextPrec = precedenceOf(peek().lexeme);
        if (nextPrec > prec) {
            rhs = parseBinOpRHS(prec + 1, rhs);
        }

        auto bin = std::make_shared<BinaryExpr>();
        bin->left = lhs;
        bin->right = rhs;
        bin->op = op;
        lhs = bin;
    }
}

NodePtr Parser::parseWhile() {
    auto whileStmt = std::make_shared<WhileStmt>();
    whileStmt->condition = parseExpression();
    whileStmt->block = parseBlock();
    matchEnd();
    return whileStmt;
}

NodePtr Parser::parseBreak() {
    matchEnd();
    return std::make_shared<BreakStmt>();
}

NodePtr Parser::parseContinue() {
    matchEnd();
    return std::make_shared<ContinueStmt>();
}

NodePtr Parser::parseSwitch() {
    auto switchStmt = std::make_shared<SwitchStmt>();
    switchStmt->condition = parseExpression();
    
    match("{");
    while (!check("}") && !isAtEnd()) {
        if (match("case")) {
            std::string caseValue;
            if (peek().type == TokenType::Number || peek().type == TokenType::String) {
                caseValue = advance().lexeme;
            }
            auto caseBlock = parseBlock();
            switchStmt->cases.push_back({caseValue, caseBlock});
        }
        else if (match("default")) {
            switchStmt->defaultBlock = parseBlock();
        }
        else {
            advance();
        }
    }
    match("}");
    matchEnd();
    return switchStmt;
}

NodePtr Parser::parseThread() {
    auto threadStmt = std::make_shared<ThreadStmt>();
    threadStmt->block = parseBlock();
    matchEnd();
    return threadStmt;
}

NodePtr Parser::parseAsync() {
    auto asyncStmt = std::make_shared<AsyncStmt>();
    asyncStmt->expr = parseExpression();
    matchEnd();
    return asyncStmt;
}

NodePtr Parser::parseChannel() {
    auto channelDecl = std::make_shared<ChannelDecl>();
    if (peek().type == TokenType::Identifier) {
        channelDecl->name = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        channelDecl->channelType = advance().lexeme;
    }
    matchEnd();
    return channelDecl;
}

NodePtr Parser::parseSend() {
    auto sendStmt = std::make_shared<SendStmt>();
    if (peek().type == TokenType::Identifier) {
        sendStmt->channel = advance().lexeme;
    }
    sendStmt->value = parseExpression();
    matchEnd();
    return sendStmt;
}

NodePtr Parser::parseRecv() {
    auto recvStmt = std::make_shared<RecvStmt>();
    if (peek().type == TokenType::Identifier) {
        recvStmt->channel = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        recvStmt->targetVar = advance().lexeme;
    }
    matchEnd();
    return recvStmt;
}

NodePtr Parser::parseStruct() {
    auto structDecl = std::make_shared<StructDecl>();
    if (peek().type == TokenType::Identifier) {
        structDecl->name = advance().lexeme;
    }
    
    match("{");
    while (!check("}") && !isAtEnd()) {
        std::string fieldType, fieldName;
        if (peek().type == TokenType::Identifier) {
            fieldType = advance().lexeme;
        }
        if (peek().type == TokenType::Identifier) {
            fieldName = advance().lexeme;
        }
        if (!fieldType.empty() && !fieldName.empty()) {
            structDecl->fields.push_back({fieldType, fieldName});
        }
    }
    match("}");
    matchEnd();
    return structDecl;
}

// -----------------------------------------------------------------------------
// CODE EMITTER - Generates C++20 from AST
// -----------------------------------------------------------------------------

class CodeEmitter {
public:
    std::string emit(NodePtr root) {
        std::ostringstream out;
        out << "#include <iostream>\n";
        out << "#include <string>\n";
        out << "#include <cmath>\n";
        out << "#include <queue>\n";
        out << "#include <thread>\n";
        out << "#include <future>\n\n";
        
        emitNode(root, out);
        return out.str();
    }

private:
    void emitNode(NodePtr node, std::ostringstream& out) {
        if (auto block = std::dynamic_pointer_cast<Block>(node)) {
            for (auto& stmt : block->statements) {
                emitNode(stmt, out);
            }
        }
        else if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(node)) {
            out << "auto " << fn->name << "(";
            if (!fn->params.empty()) {
                out << fn->params;
            }
            out << ") {\n";
            emitNode(fn->body, out);
            out << "}\n\n";
        }
        else if (auto print = std::dynamic_pointer_cast<PrintStmt>(node)) {
            out << "std::cout << ";
            emitExpr(print->expr, out);
            out << " << std::endl;\n";
        }
        else if (auto varDecl = std::dynamic_pointer_cast<VarDecl>(node)) {
            out << "auto " << varDecl->name;
            if (varDecl->initializer) {
                out << " = ";
                emitExpr(varDecl->initializer, out);
            }
            out << ";\n";
        }
        else if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(node)) {
            out << "return";
            if (ret->value) {
                out << " ";
                emitExpr(ret->value, out);
            }
            out << ";\n";
        }
        else if (auto loop = std::dynamic_pointer_cast<LoopStmt>(node)) {
            out << "for (" << loop->loopHeader << ") {\n";
            emitNode(loop->block, out);
            out << "}\n";
        }
        else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(node)) {
            out << "if (";
            emitExpr(ifStmt->condition, out);
            out << ") {\n";
            emitNode(ifStmt->thenBlock, out);
            out << "}";
            if (ifStmt->elseBlock) {
                out << " else {\n";
                emitNode(ifStmt->elseBlock, out);
                out << "}";
            }
            out << "\n";
        }
        else if (auto call = std::dynamic_pointer_cast<CallExpr>(node)) {
            out << call->callee << "(";
            for (size_t i = 0; i < call->args.size(); ++i) {
                if (i > 0) out << ", ";
                emitExpr(call->args[i], out);
            }
            out << ");\n";
        }
        else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(node)) {
            out << "while (";
            emitExpr(whileStmt->condition, out);
            out << ") {\n";
            emitNode(whileStmt->block, out);
            out << "}\n";
        }
        else if (std::dynamic_pointer_cast<BreakStmt>(node)) {
            out << "break;\n";
        }
        else if (std::dynamic_pointer_cast<ContinueStmt>(node)) {
            out << "continue;\n";
        }
        else if (auto switchStmt = std::dynamic_pointer_cast<SwitchStmt>(node)) {
            out << "switch (";
            emitExpr(switchStmt->condition, out);
            out << ") {\n";
            for (auto& c : switchStmt->cases) {
                out << "case " << c.first << ":\n";
                emitNode(c.second, out);
                out << "break;\n";
            }
            if (switchStmt->defaultBlock) {
                out << "default:\n";
                emitNode(switchStmt->defaultBlock, out);
            }
            out << "}\n";
        }
        else if (auto threadStmt = std::dynamic_pointer_cast<ThreadStmt>(node)) {
            out << "std::thread([&]() {\n";
            emitNode(threadStmt->block, out);
            out << "}).detach();\n";
        }
        else if (auto asyncStmt = std::dynamic_pointer_cast<AsyncStmt>(node)) {
            out << "std::async(std::launch::async, [&]() { return ";
            emitExpr(asyncStmt->expr, out);
            out << "; });\n";
        }
        else if (auto channelDecl = std::dynamic_pointer_cast<ChannelDecl>(node)) {
            out << "std::queue<" << channelDecl->channelType << "> " 
                << channelDecl->name << ";\n";
        }
        else if (auto sendStmt = std::dynamic_pointer_cast<SendStmt>(node)) {
            out << sendStmt->channel << ".push(";
            emitExpr(sendStmt->value, out);
            out << ");\n";
        }
        else if (auto recvStmt = std::dynamic_pointer_cast<RecvStmt>(node)) {
            out << recvStmt->targetVar << " = " << recvStmt->channel << ".front();\n";
            out << recvStmt->channel << ".pop();\n";
        }
        else if (auto structDecl = std::dynamic_pointer_cast<StructDecl>(node)) {
            out << "struct " << structDecl->name << " {\n";
            for (auto& f : structDecl->fields) {
                out << "  " << f.first << " " << f.second << ";\n";
            }
            out << "};\n";
        }
    }

    void emitExpr(NodePtr expr, std::ostringstream& out) {
        if (auto lit = std::dynamic_pointer_cast<Literal>(expr)) {
            out << "\"" << lit->value << "\"";
        }
        else if (auto id = std::dynamic_pointer_cast<Identifier>(expr)) {
            out << id->name;
        }
        else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
            out << "(";
            emitExpr(bin->left, out);
            out << " " << bin->op << " ";
            emitExpr(bin->right, out);
            out << ")";
        }
    }
};

// -----------------------------------------------------------------------------
// LLVM IR GENERATOR (Complete Implementation)
// ----------------------------------------------------------------------------/

#ifdef ENABLE_LLVM

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

class LLVMCodeGenerator {
public:
    LLVMCodeGenerator() 
        : context(std::make_unique<llvm::LLVMContext>()),
          module(std::make_unique<llvm::Module>("CASE_Module", *context)),
          builder(std::make_unique<llvm::IRBuilder<>>(*context)) {
        
        initializeBuiltins();
    }

    void generate(NodePtr root) {
        generateNode(root);
    }

    void writeToFile(const std::string& filename) {
        std::error_code EC;
        llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
        
        if (EC) {
            std::cerr << "Could not open file: " << EC.message() << "\n";
            return;
        }
        
        module->print(dest, nullptr);
        dest.flush();
    }

    void optimize() {
        llvm::legacy::FunctionPassManager FPM(module.get());
        
        // Add optimization passes
        FPM.add(llvm::createInstructionCombiningPass());
        FPM.add(llvm::createReassociatePass());
        FPM.add(llvm::createGVNPass());
        FPM.add(llvm::createCFGSimplificationPass());
        
        FPM.doInitialization();
        
        for (auto &F : *module) {
            FPM.run(F);
        }
    }

private:
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::map<std::string, llvm::AllocaInst*> namedValues;
    std::map<std::string, llvm::Function*> functionTable;
    llvm::Function* currentFunction;
    llvm::BasicBlock* currentBreakBlock;
    llvm::BasicBlock* currentContinueBlock;

    void initializeBuiltins() {
        // Create printf declaration for Print statements
        std::vector<llvm::Type*> printfArgs;
        printfArgs.push_back(llvm::Type::getInt8PtrTy(*context));
        
        llvm::FunctionType* printfType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            printfArgs,
            true
        );
        
        llvm::Function::Create(
            printfType,
            llvm::Function::ExternalLinkage,
            "printf",
            module.get()
        );
    }

    llvm::Type* getLLVMType(const std::string& typeStr) {
        if (typeStr == "int") return llvm::Type::getInt32Ty(*context);
        if (typeStr == "double" || typeStr == "float") return llvm::Type::getDoubleTy(*context);
        if (typeStr == "bool") return llvm::Type::getInt1Ty(*context);
        if (typeStr == "string") return llvm::Type::getInt8PtrTy(*context);
        if (typeStr == "void") return llvm::Type::getVoidTy(*context);
        return llvm::Type::getInt32Ty(*context); // default to int
    }

    llvm::Value* generateNode(NodePtr node) {
        if (auto block = std::dynamic_pointer_cast<Block>(node)) {
            return generateBlock(block);
        }
        else if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(node)) {
            return generateFunction(fn);
        }
        else if (auto print = std::dynamic_pointer_cast<PrintStmt>(node)) {
            return generatePrint(print);
        }
        else if (auto varDecl = std::dynamic_pointer_cast<VarDecl>(node)) {
            return generateVarDecl(varDecl);
        }
        else if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(node)) {
            return generateReturn(ret);
        }
        else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(node)) {
            return generateIf(ifStmt);
        }
        else if (auto loop = std::dynamic_pointer_cast<LoopStmt>(node)) {
            return generateLoop(loop);
        }
        else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(node)) {
            return generateWhile(whileStmt);
        }
        else if (auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(node)) {
            return builder->CreateBr(currentBreakBlock);
        }
        else if (auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(node)) {
            return builder->CreateBr(currentContinueBlock);
        }
        else if (auto switchStmt = std::dynamic_pointer_cast<SwitchStmt>(node)) {
            return generateSwitch(switchStmt);
        }
        else if (auto call = std::dynamic_pointer_cast<CallExpr>(node)) {
            return generateCall(call);
        }
        
        return nullptr;
    }

    llvm::Value* generateBlock(std::shared_ptr<Block> block) {
        llvm::Value* last = nullptr;
        for (auto& stmt : block->statements) {
            last = generateNode(stmt);
        }
        return last;
    }

    llvm::Value* generateFunction(std::shared_ptr<FunctionDecl> fn) {
        // Parse parameters
        std::vector<llvm::Type*> paramTypes;
        std::vector<std::string> paramNames;
        
        if (!fn->params.empty()) {
            // Simple parsing: "int a, double b" -> [(int, a), (double, b)]
            std::stringstream ss(fn->params);
            std::string token;
            while (std::getline(ss, token, ',')) {
                std::istringstream tokenStream(token);
                std::string type, name;
                tokenStream >> type >> name;
                paramTypes.push_back(getLLVMType(type));
                paramNames.push_back(name);
            }
        }
        
        // Create function type (default return: int)
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            paramTypes,
            false
        );
        
        llvm::Function* function = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            fn->name,
            module.get()
        );
        
        functionTable[fn->name] = function;
        currentFunction = function;
        
        // Create entry block
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*context, "entry", function);
        builder->SetInsertPoint(entryBB);
        
        // Allocate parameters
        auto argIt = function->arg_begin();
        for (size_t i = 0; i < paramNames.size(); ++i, ++argIt) {
            llvm::AllocaInst* alloca = builder->CreateAlloca(paramTypes[i], nullptr, paramNames[i]);
            builder->CreateStore(&*argIt, alloca);
            namedValues[paramNames[i]] = alloca;
        }
        
        // Generate body
        generateNode(fn->body);
        
        // Ensure return if not present
        if (builder->GetInsertBlock()->getTerminator() == nullptr) {
            builder->CreateRet(llvm::ConstantInt::get(*context, llvm::APInt(32, 0)));
        }
        
        // Verify function
        std::string errorStr;
        llvm::raw_string_ostream errorStream(errorStr);
        if (llvm::verifyFunction(*function, &errorStream)) {
            std::cerr << "Function verification failed: " << errorStr << "\n";
        }
        
        namedValues.clear();
        return function;
    }

    llvm::Value* generatePrint(std::shared_ptr<PrintStmt> print) {
        llvm::Function* printfFunc = module->getFunction("printf");
        
        llvm::Value* exprVal = generateExpression(print->expr);
        
        // Create format string based on type
        llvm::Value* formatStr;
        if (exprVal->getType()->isDoubleTy()) {
            formatStr = builder->CreateGlobalStringPtr("%f\n");
        } else if (exprVal->getType()->isIntegerTy()) {
            formatStr = builder->CreateGlobalStringPtr("%d\n");
        } else {
            formatStr = builder->CreateGlobalStringPtr("%s\n");
        }
        
        std::vector<llvm::Value*> args = { formatStr, exprVal };
        return builder->CreateCall(printfFunc, args);
    }

    llvm::Value* generateVarDecl(std::shared_ptr<VarDecl> varDecl) {
        llvm::Type* type = getLLVMType(varDecl->type);
        llvm::AllocaInst* alloca = builder->CreateAlloca(type, nullptr, varDecl->name);
        
        if (varDecl->initializer) {
            llvm::Value* initVal = generateExpression(varDecl->initializer);
            builder->CreateStore(initVal, alloca);
        }
        
        namedValues[varDecl->name] = alloca;
        return alloca;
    }

    llvm::Value* generateReturn(std::shared_ptr<ReturnStmt> ret) {
        if (ret->value) {
            llvm::Value* retVal = generateExpression(ret->value);
            return builder->CreateRet(retVal);
        }
        return builder->CreateRetVoid();
    }

    llvm::Value* generateIf(std::shared_ptr<IfStmt> ifStmt) {
        llvm::Value* condVal = generateExpression(ifStmt->condition);
        
        llvm::Function* function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "then", function);
        llvm::BasicBlock* elseBB = ifStmt->elseBlock ? 
            llvm::BasicBlock::Create(*context, "else") : nullptr;
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "ifcont");
        
        if (elseBB) {
            builder->CreateCondBr(condVal, thenBB, elseBB);
        } else {
            builder->CreateCondBr(condVal, thenBB, mergeBB);
        }
        
        // Then block
        builder->SetInsertPoint(thenBB);
        generateNode(ifStmt->thenBlock);
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(mergeBB);
        }
        
        // Else block
        if (elseBB) {
            function->getBasicBlockList().push_back(elseBB);
            builder->SetInsertPoint(elseBB);
            generateNode(ifStmt->elseBlock);
            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(mergeBB);
            }
        }
        
        // Merge block
        function->getBasicBlockList().push_back(mergeBB);
        builder->SetInsertPoint(mergeBB);
        
        return nullptr;
    }

    llvm::Value* generateLoop(std::shared_ptr<LoopStmt> loop) {
        llvm::Function* function = builder->GetInsertBlock()->getParent();
        
        llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(*context, "loop", function);
        llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "afterloop");
        
        // Save break/continue blocks
        llvm::BasicBlock* savedBreak = currentBreakBlock;
        llvm::BasicBlock* savedContinue = currentContinueBlock;
        currentBreakBlock = afterBB;
        currentContinueBlock = loopBB;
        
        builder->CreateBr(loopBB);
        builder->SetInsertPoint(loopBB);
        
        generateNode(loop->block);
        
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(loopBB);
        }
        
        function->getBasicBlockList().push_back(afterBB);
        builder->SetInsertPoint(afterBB);
        
        // Restore break/continue blocks
        currentBreakBlock = savedBreak;
        currentContinueBlock = savedContinue;
        
        return nullptr;
    }

    llvm::Value* generateWhile(std::shared_ptr<WhileStmt> whileStmt) {
        llvm::Function* function = builder->GetInsertBlock()->getParent();
        
        llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context, "whilecond", function);
        llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(*context, "whileloop");
        llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "afterwhile");
        
        builder->CreateBr(condBB);
        builder->SetInsertPoint(condBB);
        
        llvm::Value* condVal = generateExpression(whileStmt->condition);
        builder->CreateCondBr(condVal, loopBB, afterBB);
        
        function->getBasicBlockList().push_back(loopBB);
        builder->SetInsertPoint(loopBB);
        
        // Save break/continue blocks
        llvm::BasicBlock* savedBreak = currentBreakBlock;
        llvm::BasicBlock* savedContinue = currentContinueBlock;
        currentBreakBlock = afterBB;
        currentContinueBlock = condBB;
        
        generateNode(whileStmt->block);
        
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(condBB);
        }
        
        function->getBasicBlockList().push_back(afterBB);
        builder->SetInsertPoint(afterBB);
        
        // Restore break/continue blocks
        currentBreakBlock = savedBreak;
        currentContinueBlock = savedContinue;
        
        return nullptr;
    }

    llvm::Value* generateSwitch(std::shared_ptr<SwitchStmt> switchStmt) {
        llvm::Value* condVal = generateExpression(switchStmt->condition);
        llvm::Function* function = builder->GetInsertBlock()->getParent();
        
        llvm::BasicBlock* defaultBB = switchStmt->defaultBlock ?
            llvm::BasicBlock::Create(*context, "default") :
            llvm::BasicBlock::Create(*context, "switchcont");
        
        llvm::SwitchInst* switchInst = builder->CreateSwitch(condVal, defaultBB, switchStmt->cases.size());
        
        for (auto& caseEntry : switchStmt->cases) {
            llvm::BasicBlock* caseBB = llvm::BasicBlock::Create(*context, "case", function);
            
            // Parse case value
            int caseVal = std::stoi(caseEntry.first);
            switchInst->addCase(
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), caseVal),
                caseBB
            );
            
            builder->SetInsertPoint(caseBB);
            generateNode(caseEntry.second);
            
            if (!builder->GetInsertBlock()->getTerminator()) {
                builder->CreateBr(defaultBB);
            }
        }
        
        if (switchStmt->defaultBlock) {
            function->getBasicBlockList().push_back(defaultBB);
            builder->SetInsertPoint(defaultBB);
            generateNode(switchStmt->defaultBlock);
        }
        
        llvm::BasicBlock* contBB = llvm::BasicBlock::Create(*context, "switchcont");
        function->getBasicBlockList().push_back(contBB);
        
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(contBB);
        }
        
        builder->SetInsertPoint(contBB);
        return nullptr;
    }

    llvm::Value* generateCall(std::shared_ptr<CallExpr> call) {
        llvm::Function* callee = functionTable[call->callee];
        if (!callee) {
            std::cerr << "Unknown function: " << call->callee << "\n";
            return nullptr;
        }
        
        std::vector<llvm::Value*> args;
        for (auto& arg : call->args) {
            args.push_back(generateExpression(arg));
        }
        
        return builder->CreateCall(callee, args);
    }

    llvm::Value* generateExpression(NodePtr expr) {
        if (auto lit = std::dynamic_pointer_cast<Literal>(expr)) {
            // Try to parse as number
            try {
                if (lit->value.find('.') != std::string::npos) {
                    double val = std::stod(lit->value);
                    return llvm::ConstantFP::get(*context, llvm::APFloat(val));
                } else {
                    int val = std::stoi(lit->value);
                    return llvm::ConstantInt::get(*context, llvm::APInt(32, val));
                }
            } catch (...) {
                // It's a string literal
                return builder->CreateGlobalStringPtr(lit->value);
            }
        }
        else if (auto id = std::dynamic_pointer_cast<Identifier>(expr)) {
            llvm::AllocaInst* var = namedValues[id->name];
            if (!var) {
                std::cerr << "Unknown variable: " << id->name << "\n";
                return nullptr;
            }
            return builder->CreateLoad(var->getAllocatedType(), var, id->name);
        }
        else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
            llvm::Value* L = generateExpression(bin->left);
            llvm::Value* R = generateExpression(bin->right);
            
            if (bin->op == "+") return builder->CreateAdd(L, R, "addtmp");
            if (bin->op == "-") return builder->CreateSub(L, R, "subtmp");
            if (bin->op == "*") return builder->CreateMul(L, R, "multmp");
            if (bin->op == "/") return builder->CreateSDiv(L, R, "divtmp");
            if (bin->op == "==") return builder->CreateICmpEQ(L, R, "cmptmp");
            if (bin->op == "!=") return builder->CreateICmpNE(L, R, "cmptmp");
            if (bin->op == "<") return builder->CreateICmpSLT(L, R, "cmptmp");
            if (bin->op == ">") return builder->CreateICmpSGT(L, R, "cmptmp");
            if (bin->op == "<=") return builder->CreateICmpSLE(L, R, "cmptmp");
            if (bin->op == ">=") return builder->CreateICmpSGE(L, R, "cmptmp");
        }
        
        return nullptr;
    }
};

#endif // ENABLE_LLVM

// -----------------------------------------------------------------------------
// MAIN ENTRY POINT
// -----------------------------------------------------------------------------

static std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: transpiler <input.case>\n";
        return 1;
    }

    try {
        std::string source = readFile(argv[1]);
        
        if (source.find("call CIAM[on]") != std::string::npos) {
            ciam::Preprocessor ciamPre;
            source = ciamPre.Process(source);
        }

        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        std::cout << "\n\033[1;36m=== Token Stream ===\033[0m\n";
        for (const auto& t : tokens) {
            std::cout << std::setw(5) << t.line << ":" << std::setw(3) << t.column << " | "
                << std::left << std::setw(12) << tokenTypeToString(t.type)
                << " -> \"" << t.lexeme << "\"\n";
        }

        Parser parser(tokens);
        NodePtr ast = parser.parse();

        std::cout << "\n\033[1;36m=== AST ===\033[0m\n";
        ast->print();

        CodeEmitter emitter;
        std::string cpp = emitter.emit(ast);

        std::ofstream out("compiler.cpp");
        out << cpp;
        std::cout << "\n\033[1;32m✅ Generated compiler.cpp\033[0m\n";

        std::string compileCmd = "clang++ -std=c++20 -O3 compiler.cpp -o program.exe";
        if (system(compileCmd.c_str()) == 0) {
            std::cout << "\033[1;32m✅ Compiled to program.exe\033[0m\n";
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\033[1;31m[Error]\033[0m " << e.what() << "\n";
        return 1;
    }
}

#pragma execution_character_set(pop)
