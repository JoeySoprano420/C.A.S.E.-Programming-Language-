//=============================================================================
//  🌌 Violet Aura Creations — C.A.S.E. Parser Header
//=============================================================================

#pragma once
#include "AST.hpp"
#include <vector>
#include <string>

// Token types
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

class Parser {
public:
    explicit Parser(const std::vector<Token>& t);
    NodePtr parse();

private:
    const std::vector<Token>& tokens;
    size_t pos;

    const Token& peek() const;
    const Token& advance();
    bool match(const std::string& kw);
    bool check(const std::string& kw) const;
    bool isAtEnd() const;

    NodePtr parseStatement();
    NodePtr parseBlock();
    NodePtr parseExpression();
    NodePtr parsePrimary();
    NodePtr parseBinOpRHS(int minPrec, NodePtr lhs);
    int precedenceOf(const std::string& op);
    bool matchEnd();

    // Core statements
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
    
    // BATCH 1: Type System
    NodePtr parseEnum();
    NodePtr parseUnion();
    NodePtr parseTypedef();
    
    // BATCH 2: File I/O
    NodePtr parseOpen();
    NodePtr parseWrite();
    NodePtr parseWriteln();
    NodePtr parseRead();
    NodePtr parseClose();
    NodePtr parseInput();
    NodePtr parseSerialize();
    NodePtr parseDeserialize();
    NodePtr parseCompress();
    NodePtr parseDecompress();
    
    // BATCH 3: Security & Monitoring
    NodePtr parseSanitize();
    NodePtr parsePing();
    NodePtr parseAudit();
    NodePtr parseTemperature();
    NodePtr parsePressure();
    NodePtr parseGauge();
    NodePtr parseMatrix();
    
    // BATCH 4: Data Manipulation
    NodePtr parseMutate();
    NodePtr parseScale();
    NodePtr parseBounds();
    NodePtr parseCheckpoint();
    
    // BATCH 5: Advanced Concurrency
    NodePtr parseSync();
    NodePtr parseParallel();
    NodePtr parseBatch();
    NodePtr parseSchedule();
    
    // BATCH 6: Graphics/UI
    NodePtr parseWindow();
    NodePtr parseDraw();
    NodePtr parseRender();
    NodePtr parseColor();
    NodePtr parseEvent();
    NodePtr parseWidget();
    NodePtr parseLayout();
    
    // BATCH 7: Database Operations
    NodePtr parseConnect();
    NodePtr parseQuery();
    NodePtr parseInsert();
    NodePtr parseUpdate();
    NodePtr parseDelete();
    NodePtr parseTransaction();
    
    // BATCH 8: Advanced Networking
    NodePtr parseHttp();
    NodePtr parseSocket();
    NodePtr parseWebSocket();
    NodePtr parseListen();
    NodePtr parseSendNet();
    NodePtr parseReceive();
};
