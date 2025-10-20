//=============================================================================
//  🌌 Violet Aura Creations — CASE Programming Language Transpiler
//  MODULAR ARCHITECTURE - Main Entry Point
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

// Modular imports
#include "AST.hpp"
#include "Parser.hpp"
#include "CodeEmitter.hpp"
#include "intelligence.hpp"  // CIAM preprocessor

// -----------------------------------------------------------------------------
// TOKEN TYPES (shared with Parser)
// -----------------------------------------------------------------------------

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
        "ping","resource","environment","CIAM","end",
        "sanitize_mem","san_mem","sanitize_code","san_code","audit","temperature",
        "pressure","gauge","matrix",
        // BATCH 6: Graphics/UI
        "window","draw","render","color","event","widget","layout",
        // BATCH 7: Database
        "connect","query","insert","update","delete","transaction",
        // BATCH 8: Networking
        "http","socket","websocket","listen","sendnet","receive",
        // STANDARD LIBRARY: Math
        "sin","cos","tan","sqrt","pow","abs","floor","ceil","round","min","max","random",
        // STANDARD LIBRARY: Strings
        "length","substr","concat","split","upper","lower","trim","replace","find","join",
        // STANDARD LIBRARY: Collections
        "push","pop","shift","unshift","slice","map","filter","reduce","sort","reverse","size"
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
        
        // CIAM preprocessing if enabled
        if (source.find("call CIAM[on]") != std::string::npos) {
            ciam::Preprocessor ciamPre;
            source = ciamPre.Process(source);
        }

        // Lexical analysis
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        std::cout << "\n\033[1;36m=== Token Stream ===\033[0m\n";
        for (const auto& t : tokens) {
            std::cout << std::setw(5) << t.line << ":" << std::setw(3) << t.column << " | "
                << std::left << std::setw(12) << tokenTypeToString(t.type)
                << " -> \"" << t.lexeme << "\"\n";
        }

        // Parsing
        Parser parser(tokens);
        NodePtr ast = parser.parse();

        std::cout << "\n\033[1;36m=== AST ===\033[0m\n";
        ast->print();

        // Code generation
        CodeEmitter emitter;
        std::string cpp = emitter.emit(ast);

        std::ofstream out("compiler.cpp");
        out << cpp;
        std::cout << "\n\033[1;32m✅ Generated compiler.cpp\033[0m\n";

        // Compile generated C++ code
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
