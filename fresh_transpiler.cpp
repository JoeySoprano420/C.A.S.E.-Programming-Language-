#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <iomanip>
#include <memory>

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
// SYMBOL TABLE (Add this before Lexer)
// ----------------------------------------------------------------------------

struct SymbolInfo {
    std::string type;
    int line;
    int column;
    bool isInitialized;
};

class SymbolTable {
public:
    SymbolTable() : errorCount(0) {
        enterScope(); // Global scope
    }

    void enterScope() {
        scopes.push_back(std::unordered_map<std::string, SymbolInfo>());
    }

    void exitScope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    bool declare(const std::string& name, const std::string& type, int line = 0, int column = 0) {
        if (scopes.empty()) {
            return false;
        }
        auto& currentScope = scopes.back();
        if (currentScope.count(name)) {
            return false; // Already declared in current scope
        }
        currentScope[name] = {type, line, column, false};
        return true;
    }

    bool lookup(const std::string& name, std::string& type) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) {
                type = it->at(name).type;
                return true;
            }
        }
        return false;
    }

    bool lookupWithInfo(const std::string& name, SymbolInfo& info) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) {
                info = it->at(name);
                return true;
            }
        }
        return false;
    }

    void markInitialized(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) {
                (*it)[name].isInitialized = true;
                return;
            }
        }
    }

    bool isInitialized(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) {
                return it->at(name).isInitialized;
            }
        }
        return false;
    }

    void incrementErrorCount() { ++errorCount; }
    int getErrorCount() const { return errorCount; }
    bool hasErrors() const { return errorCount > 0; }

private:
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
    int errorCount;
};

class Lexer {
public:
    explicit Lexer(const std::string& src)
        : source(src), pos(0), line(1), column(1) {
    }

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
        // Core
        "Print","ret","loop","if","else","Fn","call","let","while","break","continue",
        "switch","case","default","overlay","open","write","writeln","read","close",
        "mutate","scale","bounds","checkpoint","vbreak","channel","send","recv","sync",
        "schedule","input","true","false","struct","enum","union","typedef","const",
        "volatile","static","extern","inline","auto","void","int","float","double",
        "char","bool","string","array","list","dict","thread","async","batch","parallel",
        "serialize","deserialize","compress","decompress","obfuscate","deobfuscate",
        "ping","resource","environment","deadcode","dce","fold","peephole","unroll",
        "vectorize","profile","lookahead","index","derive","chain","complex","routine",
        "synchronized","deepparallel","syncopated","routed","ciam","contract","nonneg",
        "audit","replay","inspect","evolve","dictionary","ambiguous","logic","schedules",
        "derivatives","concepts","compares","ranges","scales","coroutines","shelves",
        "overlays","CIAMS","vertices","locations","assignments","temporaries","temps",
        "announcements","directories","visitors","resolvers","checkpoints","breaks",
        "generics","namespaces","triggers","permanents","fixers","simplifiers","summaries",
        "quantifiers","tieins","children","branches","tuples","nests","batches","groups",
        "pairings","pattern","matching","chains","levels","containers","scaling","frequency",
        "definers","durations","loops","conditionals","booleans","bools","pools",
        "thread_quantities","shaders","edges","capsules","packets","ping_ruleset","queries",
        "quarry","sequester","task","allow","disallow","block","accept","collect","merge",
        "pulse","assume","ask","integers","locks","truths","tables","labels","pinch",
        "bind","wrap","glue","distance","temperature","import","export","render","compile",
        "isolate","quarantine","delete","include","comment","active","potential","dormant",
        "make","build","pressure","gauge","matrix","axis","access","assess","create",
        "disable","enable","restrict","grant","empty","dismiss","employ","explore",
        "role","job","release","retain","status","leverages","events","tracers","trackers",
        "tags","mutations","rendering","weights"
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
        return std::string("(){}[];,.:").find(c) != std::string::npos;
    }

    void skipWhitespaceAndComments() {
        while (!isAtEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
            if (c == '\n') { line++; column = 1; advance(); continue; }

            if (c == '/' && peekNext() == '/') {
                while (!isAtEnd() && peek() != '\n') advance();
            }
            else if (c == '/' && peekNext() == '*') {
                advance(); advance();
                while (!isAtEnd() && !(peek() == '*' && peekNext() == '/')) {
                    if (peek() == '\n') { line++; column = 1; }
                    advance();
                }
                if (!isAtEnd()) { advance(); advance(); }
            }
            else break;
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
        advance();
        std::string value;
        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\n') { line++; column = 1; }
            value += advance();
        }
        if (peek() == '"') advance();
        else reportError("Unterminated string literal");
        return { TokenType::String, value, line, (int)startCol };
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

static std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return content;
}

// -----------------------------------------------------------------------------
// AST NODES
// ----------------------------------------------------------------------------

struct Node { virtual ~Node() = default; virtual void print(int depth = 0) const = 0; };
using NodePtr = std::shared_ptr<Node>;

static void indent(int depth) { for (int i = 0; i < depth; i++) std::cout << "  "; }

struct Expr : Node {};
struct Stmt : Node {};
struct Block : Node { std::vector<NodePtr> statements; void print(int d = 0) const override; };
struct PrintStmt : Stmt { NodePtr expr; void print(int d = 0) const override; };
struct IfStmt : Stmt { NodePtr condition, thenBlock, elseBlock; void print(int d = 0) const override; };
struct LoopStmt : Stmt { NodePtr block; void print(int d = 0) const override; };
struct FunctionDecl : Stmt { std::string name; NodePtr body; void print(int d = 0) const override; };
struct Literal : Expr { std::string value; void print(int d = 0) const override; };
struct Identifier : Expr { std::string name; void print(int d = 0) const override; };

void Block::print(int d) const {
    indent(d); std::cout << "Block\n";
    for (auto& s : statements) s->print(d + 1);
}
void PrintStmt::print(int d) const {
    indent(d); std::cout << "PrintStmt\n";
    if (expr) expr->print(d + 1);
}
void IfStmt::print(int d) const {
    indent(d); std::cout << "IfStmt\n";
    indent(d + 1); std::cout << "Condition:\n"; condition->print(d + 2);
    indent(d + 1); std::cout << "Then:\n"; thenBlock->print(d + 2);
    if (elseBlock) { indent(d + 1); std::cout << "Else:\n"; elseBlock->print(d + 2); }
}
void LoopStmt::print(int d) const { indent(d); std::cout << "LoopStmt\n"; block->print(d + 1); }
void FunctionDecl::print(int d) const {
    indent(d); std::cout << "FunctionDecl: " << name << "\n"; body->print(d + 1);
}
void Literal::print(int d) const { indent(d); std::cout << "Literal: " << value << "\n"; }
void Identifier::print(int d) const { indent(d); std::cout << "Identifier: " << name << "\n"; }

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

// -----------------------------------------------------------------------------
// PARSER
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
};

bool Parser::match(const std::string& kw) {
    if (peek().lexeme == kw) { advance(); return true; }
    return false;
}
bool Parser::check(const std::string& kw) const {
    return peek().lexeme == kw;
}

int Parser::precedenceOf(const std::string& op) {
    if (op == "*" || op == "/") return 20;
    if (op == "+" || op == "-") return 10;
    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") return 5;
    return -1;
}

NodePtr Parser::parse() {
    auto root = std::make_shared<Block>();
    while (!isAtEnd()) root->statements.push_back(parseStatement());
    return root;
}

NodePtr Parser::parseStatement() {
    if (match("Fn")) {
        auto name = advance();
        match("("); match(")");
        auto body = parseBlock();
        auto f = std::make_shared<FunctionDecl>();
        f->name = name.lexeme; f->body = body; return f;
    }
    if (match("Print")) {
        auto stmt = std::make_shared<PrintStmt>();
        stmt->expr = parseExpression();
        match(";"); return stmt;
    }
    if (match("if")) {
        match("(");
        auto cond = parseExpression();
        match(")");
        auto thenBlk = parseBlock();
        std::shared_ptr<Block> elseBlk = nullptr;
        if (match("else")) elseBlk = std::static_pointer_cast<Block>(parseBlock());
        auto s = std::make_shared<IfStmt>();
        s->condition = cond; s->thenBlock = thenBlk; s->elseBlock = elseBlk;
        return s;
    }
    if (match("loop")) {
        auto l = std::make_shared<LoopStmt>();
        l->block = parseBlock();
        return l;
    }
    return parseExpression();
}

NodePtr Parser::parseBlock() {
    match("{");
    auto blk = std::make_shared<Block>();
    while (!check("}") && !isAtEnd()) blk->statements.push_back(parseStatement());
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
    advance(); return std::make_shared<Literal>();
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

// -----------------------------------------------------------------------------
// SEMANTIC ANALYZER WITH ENHANCED ERROR HANDLING
// ----------------------------------------------------------------------------

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(SymbolTable& st) : symTable(st) {}
    void analyze(NodePtr node);
    bool hasErrors() const { return symTable.hasErrors(); }

private:
    SymbolTable& symTable;
    void analyzeBlock(const Block& blk);
    void analyzeStmt(NodePtr stmt);
    void analyzeExpr(NodePtr expr, std::string& type);
    void reportError(const std::string& msg);
    void reportUndefinedVariable(const std::string& varName);
    void reportRedeclaration(const std::string& name, const SymbolInfo& existing);
    void reportTypeMismatch(const std::string& expected, const std::string& actual);
};

void SemanticAnalyzer::reportError(const std::string& msg) {
    std::cerr << "\033[1;31m[Semantic Error]\033[0m " << msg << "\n";
    symTable.incrementErrorCount();
}

void SemanticAnalyzer::reportUndefinedVariable(const std::string& varName) {
    reportError("Undefined variable '" + varName + "'");
    
    // Suggest similar variables (simple Levenshtein-like suggestion)
    std::cerr << "\033[1;33m[Hint]\033[0m Did you mean: ";
    // TODO: Implement fuzzy matching for suggestions
    std::cerr << "(no suggestions available)\n";
}

void SemanticAnalyzer::reportRedeclaration(const std::string& name, const SymbolInfo& existing) {
    reportError("Redeclaration of '" + name + "'");
    std::cerr << "\033[1;33m[Note]\033[0m Previously declared at line " 
              << existing.line << ", column " << existing.column << "\n";
}

void SemanticAnalyzer::reportTypeMismatch(const std::string& expected, const std::string& actual) {
    reportError("Type mismatch: expected '" + expected + "', got '" + actual + "'");
}

void SemanticAnalyzer::analyze(NodePtr node) {
    if (auto blk = std::dynamic_pointer_cast<Block>(node)) {
        analyzeBlock(*blk);
    } else {
        analyzeStmt(node);
    }
}

void SemanticAnalyzer::analyzeBlock(const Block& blk) {
    symTable.enterScope();
    for (auto& stmt : blk.statements) {
        analyzeStmt(stmt);
    }
    symTable.exitScope();
}

void SemanticAnalyzer::analyzeStmt(NodePtr stmt) {
    if (auto fn = std::dynamic_pointer_cast<FunctionDecl>(stmt)) {
        SymbolInfo existing;
        if (symTable.lookupWithInfo(fn->name, existing)) {
            reportRedeclaration(fn->name, existing);
        } else {
            symTable.declare(fn->name, "function", 0, 0);
        }
        symTable.enterScope();
        analyzeBlock(*std::static_pointer_cast<Block>(fn->body));
        symTable.exitScope();
    } 
    else if (auto var = std::dynamic_pointer_cast<VarDecl>(stmt)) {
        SymbolInfo existing;
        if (symTable.lookupWithInfo(var->name, existing)) {
            reportRedeclaration(var->name, existing);
        } else {
            symTable.declare(var->name, var->type, 0, 0);
        }
        if (var->initializer) {
            std::string initType;
            analyzeExpr(var->initializer, initType);
            if (initType != var->type && initType != "unknown") {
                reportTypeMismatch(var->type, initType);
            }
            symTable.markInitialized(var->name);
        }
    } 
    else if (auto ifs = std::dynamic_pointer_cast<IfStmt>(stmt)) {
        std::string condType;
        analyzeExpr(ifs->condition, condType);
        if (condType != "bool" && condType != "unknown") {
            reportError("Condition must be boolean, got '" + condType + "'");
        }
        analyzeBlock(*std::static_pointer_cast<Block>(ifs->thenBlock));
        if (ifs->elseBlock) {
            analyzeBlock(*std::static_pointer_cast<Block>(ifs->elseBlock));
        }
    } 
    else if (auto loop = std::dynamic_pointer_cast<LoopStmt>(stmt)) {
        analyzeBlock(*std::static_pointer_cast<Block>(loop->block));
    } 
    else if (auto print = std::dynamic_pointer_cast<PrintStmt>(stmt)) {
        std::string exprType;
        analyzeExpr(print->expr, exprType);
    }
}

void SemanticAnalyzer::analyzeExpr(NodePtr expr, std::string& type) {
    if (auto lit = std::dynamic_pointer_cast<Literal>(expr)) {
        if (lit->value.find('.') != std::string::npos || lit->value.find('e') != std::string::npos) {
            type = "double";
        } else if (lit->value == "true" || lit->value == "false") {
            type = "bool";
        } else {
            type = "int";
        }
    } 
    else if (auto id = std::dynamic_pointer_cast<Identifier>(expr)) {
        if (!symTable.lookup(id->name, type)) {
            reportUndefinedVariable(id->name);
            type = "unknown";
        } else if (!symTable.isInitialized(id->name)) {
            reportError("Variable '" + id->name + "' used before initialization");
        }
    } 
    else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
        std::string leftType, rightType;
        analyzeExpr(bin->left, leftType);
        analyzeExpr(bin->right, rightType);
        
        if (leftType != rightType && leftType != "unknown" && rightType != "unknown") {
            reportTypeMismatch(leftType, rightType);
        }
        
        if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
            type = (leftType == "double" || rightType == "double") ? "double" : "int";
        } else if (bin->op == "==" || bin->op == "!=" || bin->op == "<" || 
                   bin->op == ">" || bin->op == "<=" || bin->op == ">=") {
            type = "bool";
        } else {
            type = "unknown";
        }
    } 
    else {
        type = "unknown";
    }
}

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------

int main(int argc, char** argv) {
    try {
        std::string source;
        if (argc > 1) source = readFile(argv[1]);
        else {
            std::cout << "Enter code (Ctrl+Z or Ctrl+D to end):\n";
            std::string line;
            while (std::getline(std::cin, line)) source += line + "\n";
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
        NodePtr tree = parser.parse();

        std::cout << "\n\033[1;36m=== AST ===\033[0m\n";
        tree->print();

        SymbolTable symTable;
        SemanticAnalyzer analyzer(symTable);
        analyzer.analyze(tree);

        if (analyzer.hasErrors()) {
            std::cerr << "\n\033[1;31m=== Semantic Analysis Failed ===\033[0m\n";
            std::cerr << "Total errors: " << symTable.getErrorCount() << "\n";
            return 1;
        }

        std::cout << "\n\033[1;32m=== Semantic Analysis Complete ===\033[0m\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\033[1;31m[Error]\033[0m " << e.what() << "\n";
        return 1;
    }
}

