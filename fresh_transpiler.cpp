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
// SYMBOL TABLE - Complete Implementation
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

    // Scope management
    void enterScope() {
        scopes.push_back(std::unordered_map<std::string, SymbolInfo>());
        currentScope++;
    }

    void exitScope() {
        if (!scopes.empty()) {
            // Check for unused variables before exiting scope
            const auto& scope = scopes.back();
            for (const auto& pair : scope) {
                if (!pair.second.isUsed && pair.second.kind == SymbolKind::Variable) {
                    reportWarning("Unused variable '" + pair.first + "' declared at line " 
                                + std::to_string(pair.second.line));
                }
                if (!pair.second.isInitialized && pair.second.kind == SymbolKind::Variable) {
                    reportWarning("Variable '" + pair.first + "' declared but never initialized at line " 
                                + std::to_string(pair.second.line));
                }
            }
            scopes.pop_back();
            currentScope--;
        }
    }

    // Symbol declaration
    bool declare(const std::string& name, const std::string& type, 
                 int line = 0, int column = 0, SymbolKind kind = SymbolKind::Variable) {
        if (scopes.empty()) {
            return false;
        }

        auto& currentScopeMap = scopes.back();
        
        // Check for redeclaration in current scope
        if (currentScopeMap.count(name)) {
            return false;
        }

        // Add symbol to current scope
        currentScopeMap[name] = SymbolInfo(name, type, kind, line, column, currentScope);
        return true;
    }

    // Symbol lookup - searches from innermost to outermost scope
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

    // Lookup with full symbol info
    bool lookupWithInfo(const std::string& name, SymbolInfo& info) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                info = found->second;
                return true;
            }
        }
        return false;
    }

    // Check if symbol exists in current scope only
    bool existsInCurrentScope(const std::string& name) const {
        if (scopes.empty()) return false;
        return scopes.back().count(name) > 0;
    }

    // Mark symbol as initialized
    void markInitialized(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second.isInitialized = true;
                return;
            }
        }
    }

    // Mark symbol as used
    void markUsed(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second.isUsed = true;
                return;
            }
        }
    }

    // Check if symbol is initialized
    bool isInitialized(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second.isInitialized;
            }
        }
        return false;
    }

    // Get symbol kind
    SymbolKind getKind(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second.kind;
            }
        }
        return SymbolKind::Variable;
    }

    // Error tracking
    void incrementErrorCount() { ++errorCount; }
    int getErrorCount() const { return errorCount; }
    bool hasErrors() const { return errorCount > 0; }
    void resetErrorCount() { errorCount = 0; }

    // Diagnostic output
    void printSymbolTable() const {
        std::cout << "\n\033[1;33m=== Symbol Table ===\033[0m\n";
        int scopeNum = 0;
        for (const auto& scope : scopes) {
            std::cout << "Scope " << scopeNum++ << ":\n";
            for (const auto& pair : scope) {
                const auto& sym = pair.second;
                std::cout << "  " << sym.name << " : " << sym.type 
                         << " (kind: " << kindToString(sym.kind) 
                         << ", line: " << sym.line
                         << ", init: " << (sym.isInitialized ? "yes" : "no")
                         << ", used: " << (sym.isUsed ? "yes" : "no") << ")\n";
            }
        }
    }

    // Get all symbols in current scope
    std::vector<std::string> getSymbolsInCurrentScope() const {
        std::vector<std::string> symbols;
        if (!scopes.empty()) {
            for (const auto& pair : scopes.back()) {
                symbols.push_back(pair.first);
            }
        }
        return symbols;
    }

    // Get all symbols (all scopes)
    std::vector<std::string> getAllSymbols() const {
        std::vector<std::string> symbols;
        std::unordered_set<std::string> seen;
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            for (const auto& pair : *it) {
                if (seen.find(pair.first) == seen.end()) {
                    symbols.push_back(pair.first);
                    seen.insert(pair.first);
                }
            }
        }
        return symbols;
    }
    
    // Get all symbol names in all scopes
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
    
    // Get symbols in current scope only
    std::vector<std::string> getCurrentScopeSymbols() const {
        std::vector<std::string> names;
        if (!scopes.empty()) {
            for (const auto& pair : scopes.back()) {
                names.push_back(pair.first);
            }
        }
        return names;
    }

    // Get current scope level
    int getCurrentScopeLevel() const { return currentScope; }

private:
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
    int currentScope;
    int errorCount;

    static std::string kindToString(SymbolKind kind) {
        switch (kind) {
            case SymbolKind::Variable:  return "Variable";
            case SymbolKind::Function:  return "Function";
            case SymbolKind::Parameter: return "Parameter";
            case SymbolKind::Constant:  return "Constant";
            case SymbolKind::Type:      return "Type";
            default:                    return "Unknown";
        }
    }

    void reportWarning(const std::string& msg) const {
        std::cerr << "\033[1;33m[Warning]\033[0m " << msg << "\n";
    }
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
// SEMANTIC ANALYZER WITH ENHANCED SYMBOLTABLE INTEGRATION
// ----------------------------------------------------------------------------

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(SymbolTable& st) : symTable(st) {}
    void analyze(NodePtr node);
    bool hasErrors() const { return symTable.hasErrors(); }
    void printErrorSummary() const;

private:
    SymbolTable& symTable;
    std::vector<std::string> errorMessages;
    std::vector<std::string> warningMessages;
    
    void analyzeBlock(const Block& blk);
    void analyzeStmt(NodePtr stmt);
    void analyzeExpr(NodePtr expr, std::string& type);
    
    // Enhanced error reporting methods
    void reportError(const std::string& msg, int line = -1, int column = -1);
    void reportWarning(const std::string& msg, int line = -1, int column = -1);
    void reportUndefinedVariable(const std::string& varName, int line = -1, int column = -1);
    void reportRedeclaration(const std::string& name, const SymbolInfo& existing);
    void reportTypeMismatch(const std::string& expected, const std::string& actual, int line = -1);
    void reportUninitializedUse(const std::string& varName, int line = -1);
    
    // Fuzzy matching for suggestions
    std::vector<std::string> getSimilarIdentifiers(const std::string& name) const;
    int levenshteinDistance(const std::string& s1, const std::string& s2) const;
};

// Enhanced error reporting with location tracking
void SemanticAnalyzer::reportError(const std::string& msg, int line, int column) {
    std::string fullMsg = "\033[1;31m[Semantic Error]\033[0m ";
    if (line != -1) {
        fullMsg += "Line " + std::to_string(line);
        if (column != -1) {
            fullMsg += ", Col " + std::to_string(column);
        }
        fullMsg += ": ";
    }
    fullMsg += msg;
    
    std::cerr << fullMsg << "\n";
    errorMessages.push_back(msg);
    symTable.incrementErrorCount();
}

void SemanticAnalyzer::reportWarning(const std::string& msg, int line, int column) {
    std::string fullMsg = "\033[1;33m[Warning]\033[0m ";
    if (line != -1) {
        fullMsg += "Line " + std::to_string(line);
        if (column != -1) {
            fullMsg += ", Col " + std::to_string(column);
        }
        fullMsg += ": ";
    }
    fullMsg += msg;
    
    std::cerr << fullMsg << "\n";
    warningMessages.push_back(msg);
}

// Levenshtein distance algorithm for fuzzy matching
int SemanticAnalyzer::levenshteinDistance(const std::string& s1, const std::string& s2) const {
    const size_t m = s1.size();
    const size_t n = s2.size();
    
    if (m == 0) return n;
    if (n == 0) return m;
    
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    
    for (size_t i = 0; i <= m; i++) dp[i][0] = i;
    for (size_t j = 0; j <= n; j++) dp[0][j] = j;
    
    for (size_t i = 1; i <= m; i++) {
        for (size_t j = 1; j <= n; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,      // deletion
                dp[i][j - 1] + 1,      // insertion
                dp[i - 1][j - 1] + cost // substitution
            });
        }
    }
    
    return dp[m][n];
}

// Find similar identifiers using fuzzy matching
std::vector<std::string> SemanticAnalyzer::getSimilarIdentifiers(const std::string& name) const {
    std::vector<std::pair<std::string, int>> candidates;
    
    // Get all symbols from symbol table
    // This requires adding a method to SymbolTable to get all symbol names
    // For now, we'll use a simplified version
    
    std::vector<std::string> suggestions;
    const int MAX_DISTANCE = 3; // Maximum edit distance for suggestions
    
    // You would iterate through all symbols in the symbol table here
    // For demonstration, this is a placeholder
    // In a real implementation, you'd need to add a method to SymbolTable
    // to retrieve all current symbol names
    
    return suggestions;
}

void SemanticAnalyzer::reportUndefinedVariable(const std::string& varName, int line, int column) {
    std::string msg = "Undefined variable '" + varName + "'";
    reportError(msg, line, column);
    
    // Try to suggest similar variables
    auto suggestions = getSimilarIdentifiers(varName);
    if (!suggestions.empty()) {
        std::cerr << "\033[1;33m[Hint]\033[0m Did you mean: ";
        for (size_t i = 0; i < suggestions.size() && i < 3; i++) {
            if (i > 0) std::cerr << ", ";
            std::cerr << "'" << suggestions[i] << "'";
        }
        std::cerr << "?\n";
    }
}

void SemanticAnalyzer::reportRedeclaration(const std::string& name, const SymbolInfo& existing) {
    reportError("Redeclaration of '" + name + "'");
    std::cerr << "\033[1;33m[Note]\033[0m Previously declared at line " 
              << existing.line << ", column " << existing.column << "\n";
}

void SemanticAnalyzer::reportTypeMismatch(const std::string& expected, const std::string& actual, int line) {
    std::string msg = "Type mismatch: expected '" + expected + "', got '" + actual + "'";
    reportError(msg, line);
}

void SemanticAnalyzer::reportUninitializedUse(const std::string& varName, int line) {
    std::string msg = "Variable '" + varName + "' used before initialization";
    reportError(msg, line);
}

void SemanticAnalyzer::printErrorSummary() const {
    if (errorMessages.empty() && warningMessages.empty()) {
        return;
    }
    
    std::cout << "\n\033[1;36m=== Analysis Summary ===\033[0m\n";
    
    if (!errorMessages.empty()) {
        std::cout << "\033[1;31mErrors: " << errorMessages.size() << "\033[0m\n";
    }
    
    if (!warningMessages.empty()) {
        std::cout << "\033[1;33mWarnings: " << warningMessages.size() << "\033[0m\n";
    }
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
        } else if (!lit->value.empty()) {
            type = "int";
        } else {
            type = "string"; // Empty literal is treated as string
        }
    } 
    else if (auto id = std::dynamic_pointer_cast<Identifier>(expr)) {
        if (!symTable.lookup(id->name, type)) {
            reportUndefinedVariable(id->name);
            type = "unknown";
        } else if (!symTable.isInitialized(id->name)) {
            reportUninitializedUse(id->name);
            // Still return the type even if uninitialized
            symTable.lookup(id->name, type);
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
            // Arithmetic operators
            if (leftType == "string" || rightType == "string") {
                if (bin->op == "+") {
                    type = "string"; // String concatenation
                } else {
                    reportError("Cannot perform arithmetic operation on strings");
                    type = "unknown";
                }
            } else {
                type = (leftType == "double" || rightType == "double") ? "double" : "int";
            }
        } else if (bin->op == "==" || bin->op == "!=" || bin->op == "<" || 
                   bin->op == ">" || bin->op == "<=" || bin->op == ">=") {
            // Comparison operators
            type = "bool";
        } else if (bin->op == "&&" || bin->op == "||") {
            // Logical operators
            if (leftType != "bool" && leftType != "unknown") {
                reportError("Logical operator requires boolean operands, got '" + leftType + "'");
            }
            if (rightType != "bool" && rightType != "unknown") {
                reportError("Logical operator requires boolean operands, got '" + rightType + "'");
            }
            type = "bool";
        } else {
            reportError("Unknown operator '" + bin->op + "'");
            type = "unknown";
        }
    } 
    else {
        reportWarning("Unknown expression type");
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
                << R"( -> ")" << t.lexeme << "\"\n";
        }

        Parser parser(tokens);
        NodePtr tree = parser.parse();

        std::cout << "\n\033[1;36m=== AST ===\033[0m\n";
        tree->print();

        SymbolTable symTable;
        SemanticAnalyzer analyzer(symTable);
        analyzer.analyze(tree);

        // Print symbol table for debugging
        symTable.printSymbolTable();

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

// -----------------------------------------------------------------------------
// DRIVER
// ----------------------------------------------------------------------------

int main_driver() {
    std::string code = R"(
        Fn main() {
            let x = 5 + 3 * 2;
            Print x;
            if (x > 5) {
                Print "Greater";
            } else {
                loop { Print "Looping"; }
            }
        }
    )";

    Lexer lexer(code);
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    NodePtr tree = parser.parse();

    std::cout << "\n\033[1;36m=== AST ===\033[0m\n";
    tree->print();

    SymbolTable symTable;
    SemanticAnalyzer analyzer(symTable);
    analyzer.analyze(tree);

    symTable.printSymbolTable();

    std::cout << "\n\033[1;32m=== Semantic Analysis Complete ===\033[0m\n";

    return 0;
}

