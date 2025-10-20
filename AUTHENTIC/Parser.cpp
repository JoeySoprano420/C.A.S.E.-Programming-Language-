//=============================================================================
//  🌌 Violet Aura Creations — C.A.S.E. Parser Implementation
//=============================================================================

#include "Parser.hpp"
#include <memory>
#include <unordered_set>

Parser::Parser(const std::vector<Token>& t) : tokens(t), pos(0) {}

const Token& Parser::peek() const { return tokens[pos]; }
const Token& Parser::advance() { return tokens[pos++]; }

bool Parser::match(const std::string& kw) {
    if (peek().lexeme == kw) { advance(); return true; }
    return false;
}

bool Parser::check(const std::string& kw) const {
    return peek().lexeme == kw;
}

bool Parser::isAtEnd() const { 
    return peek().type == TokenType::EndOfFile; 
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
    if (match("Fn")) {
        auto fnDecl = std::make_shared<FunctionDecl>();
        if (peek().type == TokenType::Identifier) {
            fnDecl->name = advance().lexeme;
        }
        if (peek().type == TokenType::String) {
            fnDecl->params = advance().lexeme;
        }
        if (match("(")) {
            auto body = std::make_shared<Block>();
            while (!check(")") && !isAtEnd()) {
                auto stmt = parseStatement();
                if (stmt) body->statements.push_back(stmt);
            }
            match(")");
            fnDecl->body = body;
        }
        matchEnd();
        return fnDecl;
    }

    if (match("Print")) {
        auto stmt = std::make_shared<PrintStmt>();
        stmt->expr = parseExpression();
        matchEnd();
        return stmt;
    }

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

    if (match("loop")) {
        auto loopStmt = std::make_shared<LoopStmt>();
        if (peek().type == TokenType::String) {
            loopStmt->loopHeader = advance().lexeme;
        }
        loopStmt->block = parseBlock();
        matchEnd();
        return loopStmt;
    }

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

    if (match("ret")) {
        auto retStmt = std::make_shared<ReturnStmt>();
        if (!check("[") && !isAtEnd()) {
            retStmt->value = parseExpression();
        }
        return retStmt;
    }

    if (match("call")) {
        auto callExpr = std::make_shared<CallExpr>();
        if (peek().type == TokenType::Identifier) {
            callExpr->callee = advance().lexeme;
        }
        while (!check("[") && !isAtEnd() && peek().type != TokenType::EndOfFile) {
            callExpr->args.push_back(parseExpression());
            if (check(",")) advance();
        }
        matchEnd();
        return callExpr;
    }

    // Control flow
    if (match("while")) return parseWhile();
    if (match("break")) return parseBreak();
    if (match("continue")) return parseContinue();
    if (match("switch")) return parseSwitch();
    
    // Concurrency
    if (match("thread")) return parseThread();
    if (match("async")) return parseAsync();
    if (match("channel")) return parseChannel();
    if (match("send")) return parseSend();
    if (match("recv")) return parseRecv();
    
    // Type system
    if (match("struct")) return parseStruct();
    if (match("enum")) return parseEnum();
    if (match("union")) return parseUnion();
    if (match("typedef")) return parseTypedef();
    
    // File I/O
    if (match("open")) return parseOpen();
    if (match("write")) return parseWrite();
    if (match("writeln")) return parseWriteln();
    if (match("read")) return parseRead();
    if (match("close")) return parseClose();
    if (match("input")) return parseInput();
    if (match("serialize")) return parseSerialize();
    if (match("deserialize")) return parseDeserialize();
    if (match("compress")) return parseCompress();
    if (match("decompress")) return parseDecompress();
    
    // Security & Monitoring
    if (match("sanitize_mem") || match("san_mem") || 
        match("sanitize_code") || match("san_code")) return parseSanitize();
    if (match("ping")) return parsePing();
    if (match("audit")) return parseAudit();
    if (match("temperature")) return parseTemperature();
    if (match("pressure")) return parsePressure();
    if (match("gauge")) return parseGauge();
    if (match("matrix")) return parseMatrix();
    
    // BATCH 4: Data Manipulation
    if (match("mutate")) return parseMutate();
    if (match("scale")) return parseScale();
    if (match("bounds")) return parseBounds();
    if (match("checkpoint")) return parseCheckpoint();
    
    // BATCH 5: Advanced Concurrency
    if (match("sync")) return parseSync();
    if (match("parallel")) return parseParallel();
    if (match("batch")) return parseBatch();
    if (match("schedule")) return parseSchedule();
    
    // BATCH 6: Graphics/UI
    if (match("window")) return parseWindow();
    if (match("draw")) return parseDraw();
    if (match("render")) return parseRender();
    if (match("color")) return parseColor();
    if (match("event")) return parseEvent();
    if (match("widget")) return parseWidget();
    if (match("layout")) return parseLayout();
    
    // BATCH 7: Database
    if (match("connect")) return parseConnect();
    if (match("query")) return parseQuery();
    if (match("insert")) return parseInsert();
    if (match("update")) return parseUpdate();
    if (match("delete")) return parseDelete();
    if (match("transaction")) return parseTransaction();
    
    // BATCH 8: Networking
    if (match("http")) return parseHttp();
    if (match("socket")) return parseSocket();
    if (match("websocket")) return parseWebSocket();
    if (match("listen")) return parseListen();
    if (match("sendnet")) return parseSendNet();
    if (match("receive")) return parseReceive();

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
    
    // Check for standard library functions
    const std::unordered_set<std::string> mathFuncs = {
        "sin", "cos", "tan", "sqrt", "pow", "abs", "floor", "ceil", "round", "min", "max", "random"
    };
    const std::unordered_set<std::string> stringFuncs = {
        "length", "substr", "concat", "split", "upper", "lower", "trim", "replace", "find", "join"
    };
    const std::unordered_set<std::string> collectionFuncs = {
        "push", "pop", "shift", "unshift", "slice", "map", "filter", "reduce", "sort", "reverse", "size"
    };
    
    if (peek().type == TokenType::Keyword) {
        std::string kw = peek().lexeme;
        
        // Math functions
        if (mathFuncs.count(kw)) {
            advance();
            auto mathCall = std::make_shared<MathCallExpr>();
            mathCall->function = kw;
            // Parse arguments
            while (!check("[") && !isAtEnd() && peek().type != TokenType::EndOfFile) {
                mathCall->args.push_back(parsePrimary());
                if (check(",")) advance();
                else break;
            }
            return mathCall;
        }
        
        // String functions
        if (stringFuncs.count(kw)) {
            advance();
            auto strCall = std::make_shared<StringCallExpr>();
            strCall->function = kw;
            // Parse arguments
            while (!check("[") && !isAtEnd() && peek().type != TokenType::EndOfFile) {
                strCall->args.push_back(parsePrimary());
                if (check(",")) advance();
                else break;
            }
            return strCall;
        }
        
        // Collection functions
        if (collectionFuncs.count(kw)) {
            advance();
            auto collCall = std::make_shared<CollectionCallExpr>();
            collCall->function = kw;
            // First arg is the collection
            if (!check("[") && !isAtEnd()) {
                collCall->collection = parsePrimary();
            }
            // Additional arguments
            while (!check("[") && !isAtEnd() && peek().type != TokenType::EndOfFile) {
                if (check(",")) advance();
                else break;
                collCall->args.push_back(parsePrimary());
            }
            return collCall;
        }
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
    return std::make_shared<Literal>();
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

// === Control Flow Implementations ===
NodePtr Parser::parseWhile() {
    auto stmt = std::make_shared<WhileStmt>();
    stmt->condition = parseExpression();
    stmt->block = parseBlock();
    matchEnd();
    return stmt;
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
    auto stmt = std::make_shared<SwitchStmt>();
    stmt->condition = parseExpression();
    
    match("{");
    while (!check("}") && !isAtEnd()) {
        if (match("case")) {
            std::string caseValue;
            if (peek().type == TokenType::Number || peek().type == TokenType::String) {
                caseValue = advance().lexeme;
            }
            auto caseBlock = parseBlock();
            stmt->cases.push_back({caseValue, caseBlock});
        }
        else if (match("default")) {
            stmt->defaultBlock = parseBlock();
        }
        else {
            advance();
        }
    }
    match("}");
    matchEnd();
    return stmt;
}

// === Concurrency Implementations ===
NodePtr Parser::parseThread() {
    auto stmt = std::make_shared<ThreadStmt>();
    stmt->block = parseBlock();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseAsync() {
    auto stmt = std::make_shared<AsyncStmt>();
    stmt->expr = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseChannel() {
    auto decl = std::make_shared<ChannelDecl>();
    if (peek().type == TokenType::Identifier) {
        decl->name = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        decl->channelType = advance().lexeme;
    }
    matchEnd();
    return decl;
}

NodePtr Parser::parseSend() {
    auto stmt = std::make_shared<SendStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->channel = advance().lexeme;
    }
    stmt->value = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseRecv() {
    auto stmt = std::make_shared<RecvStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->channel = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->targetVar = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

// === Type System Implementations ===
NodePtr Parser::parseStruct() {
    auto decl = std::make_shared<StructDecl>();
    if (peek().type == TokenType::Identifier) {
        decl->name = advance().lexeme;
    }
    
    match("{");
    while (!check("}") && !isAtEnd()) {
        std::string type, name;
        if (peek().type == TokenType::Identifier) type = advance().lexeme;
        if (peek().type == TokenType::Identifier) name = advance().lexeme;
        if (!type.empty() && !name.empty()) {
            decl->fields.push_back({type, name});
        }
    }
    match("}");
    matchEnd();
    return decl;
}

NodePtr Parser::parseEnum() {
    auto decl = std::make_shared<EnumDecl>();
    if (peek().type == TokenType::Identifier) {
        decl->name = advance().lexeme;
    }
    
    match("{");
    while (!check("}") && !isAtEnd()) {
        if (peek().type == TokenType::Identifier) {
            decl->values.push_back(advance().lexeme);
        }
        if (check(",")) advance();
    }
    match("}");
    matchEnd();
    return decl;
}

NodePtr Parser::parseUnion() {
    auto decl = std::make_shared<UnionDecl>();
    if (peek().type == TokenType::Identifier) {
        decl->name = advance().lexeme;
    }
    
    match("{");
    while (!check("}") && !isAtEnd()) {
        std::string type, name;
        if (peek().type == TokenType::Identifier) type = advance().lexeme;
        if (peek().type == TokenType::Identifier) name = advance().lexeme;
        if (!type.empty() && !name.empty()) {
            decl->fields.push_back({type, name});
        }
    }
    match("}");
    matchEnd();
    return decl;
}

NodePtr Parser::parseTypedef() {
    auto stmt = std::make_shared<TypedefStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->existingType = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->newName = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

// === File I/O Implementations ===
NodePtr Parser::parseOpen() {
    auto stmt = std::make_shared<OpenStmt>();
    if (peek().type == TokenType::String) {
        stmt->filename = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        stmt->mode = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseWrite() {
    auto stmt = std::make_shared<WriteStmt>();
    stmt->newline = false;
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    stmt->expr = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseWriteln() {
    auto stmt = std::make_shared<WriteStmt>();
    stmt->newline = true;
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    stmt->expr = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseRead() {
    auto stmt = std::make_shared<ReadStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->varName = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseClose() {
    auto stmt = std::make_shared<CloseStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseInput() {
    auto stmt = std::make_shared<InputStmt>();
    if (peek().type == TokenType::String) {
        stmt->prompt = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->varName = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseSerialize() {
    auto stmt = std::make_shared<SerializeStmt>();
    stmt->format = "json";
    if (peek().type == TokenType::String) {
        stmt->format = advance().lexeme;
    }
    stmt->data = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseDeserialize() {
    auto stmt = std::make_shared<DeserializeStmt>();
    stmt->format = "json";
    if (peek().type == TokenType::String) {
        stmt->format = advance().lexeme;
    }
    stmt->source = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseCompress() {
    auto stmt = std::make_shared<CompressStmt>();
    stmt->algorithm = "zlib";
    if (peek().type == TokenType::String) {
        stmt->algorithm = advance().lexeme;
    }
    stmt->data = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseDecompress() {
    auto stmt = std::make_shared<DecompressStmt>();
    stmt->algorithm = "zlib";
    if (peek().type == TokenType::String) {
        stmt->algorithm = advance().lexeme;
    }
    stmt->data = parseExpression();
    matchEnd();
    return stmt;
}

// === Security & Monitoring Implementations ===
NodePtr Parser::parseSanitize() {
    auto stmt = std::make_shared<SanitizeStmt>();
    std::string keyword = tokens[pos-1].lexeme;
    if (keyword.find("mem") != std::string::npos) {
        stmt->type = "mem";
    } else {
        stmt->type = "code";
    }
    stmt->target = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parsePing() {
    auto stmt = std::make_shared<PingStmt>();
    if (peek().type == TokenType::String) {
        stmt->target = advance().lexeme;
    }
    stmt->protocol = "icmp";
    if (peek().type == TokenType::String) {
        stmt->protocol = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseAudit() {
    auto stmt = std::make_shared<AuditStmt>();
    stmt->auditType = "replay";
    if (peek().type == TokenType::String) {
        stmt->auditType = advance().lexeme;
    }
    while (!check("[") && !isAtEnd()) {
        if (peek().type == TokenType::Identifier || peek().type == TokenType::String) {
            stmt->targets.push_back(advance().lexeme);
        }
        if (check(",")) advance();
        else break;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseTemperature() {
    auto stmt = std::make_shared<TemperatureStmt>();
    stmt->sensor = "cpu";
    if (peek().type == TokenType::String) {
        stmt->sensor = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parsePressure() {
    auto stmt = std::make_shared<PressureStmt>();
    stmt->resource = "cpu";
    if (peek().type == TokenType::String) {
        stmt->resource = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseGauge() {
    auto stmt = std::make_shared<GaugeStmt>();
    if (peek().type == TokenType::String) {
        stmt->metric = advance().lexeme;
    }
    stmt->value = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseMatrix() {
    auto stmt = std::make_shared<MatrixStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->matrixName = advance().lexeme;
    }
    stmt->rows = 3;
    stmt->cols = 3;
    if (peek().type == TokenType::Number) {
        stmt->rows = std::stoi(advance().lexeme);
    }
    if (peek().type == TokenType::Number) {
        stmt->cols = std::stoi(advance().lexeme);
    }
    matchEnd();
    return stmt;
}

// === BATCH 4: Data Manipulation Implementations ===
NodePtr Parser::parseMutate() {
    auto stmt = std::make_shared<MutateStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->varName = advance().lexeme;
    }
    stmt->transformation = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseScale() {
    auto stmt = std::make_shared<ScaleStmt>();
    stmt->target = parseExpression();
    if (peek().type == TokenType::Number || peek().type == TokenType::Identifier) {
        stmt->factor = parseExpression();
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseBounds() {
    auto stmt = std::make_shared<BoundsStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->varName = advance().lexeme;
    }
    // Parse min and max
    stmt->min = parseExpression();
    stmt->max = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseCheckpoint() {
    auto stmt = std::make_shared<CheckpointStmt>();
    if (peek().type == TokenType::String) {
        stmt->name = advance().lexeme;
    }
    // Parse variable list
    while (!check("[") && !isAtEnd()) {
        if (peek().type == TokenType::Identifier) {
            stmt->variables.push_back(advance().lexeme);
        }
        if (check(",")) advance();
        else break;
    }
    matchEnd();
    return stmt;
}

// === BATCH 5: Advanced Concurrency Implementations ===
NodePtr Parser::parseSync() {
    auto stmt = std::make_shared<SyncStmt>();
    // Parse resource list before block
    while (!check("{") && !isAtEnd()) {
        if (peek().type == TokenType::Identifier) {
            stmt->resources.push_back(advance().lexeme);
        }
        if (check(",")) advance();
        else break;
    }
    stmt->block = parseBlock();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseParallel() {
    auto stmt = std::make_shared<ParallelStmt>();
    match("{");
    while (!check("}") && !isAtEnd()) {
        auto task = parseBlock();
        if (task) stmt->tasks.push_back(task);
    }
    match("}");
    matchEnd();
    return stmt;
}

NodePtr Parser::parseBatch() {
    auto stmt = std::make_shared<BatchStmt>();
    stmt->batchSize = 100; // default
    if (peek().type == TokenType::Identifier) {
        stmt->dataSource = advance().lexeme;
    }
    if (peek().type == TokenType::Number) {
        stmt->batchSize = std::stoi(advance().lexeme);
    }
    stmt->operation = parseBlock();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseSchedule() {
    auto stmt = std::make_shared<ScheduleStmt>();
    stmt->when = "immediate";
    if (peek().type == TokenType::String) {
        stmt->when = advance().lexeme;
    }
    stmt->task = parseBlock();
    matchEnd();
    return stmt;
}

// === BATCH 6: Graphics/UI Implementations ===
NodePtr Parser::parseWindow() {
    auto stmt = std::make_shared<WindowStmt>();
    stmt->width = 800;
    stmt->height = 600;
    if (peek().type == TokenType::String) {
        stmt->title = advance().lexeme;
    }
    if (peek().type == TokenType::Number) {
        stmt->width = std::stoi(advance().lexeme);
    }
    if (peek().type == TokenType::Number) {
        stmt->height = std::stoi(advance().lexeme);
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseDraw() {
    auto stmt = std::make_shared<DrawStmt>();
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->shape = advance().lexeme;
    }
    // Parse parameters
    while (!check("[") && !isAtEnd()) {
        stmt->params.push_back(parseExpression());
        if (check(",")) advance();
        else break;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseRender() {
    auto stmt = std::make_shared<RenderStmt>();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseColor() {
    auto stmt = std::make_shared<ColorStmt>();
    stmt->r = stmt->g = stmt->b = 0;
    stmt->a = 255;
    if (peek().type == TokenType::Number) {
        stmt->r = std::stoi(advance().lexeme);
    }
    if (peek().type == TokenType::Number) {
        stmt->g = std::stoi(advance().lexeme);
    }
    if (peek().type == TokenType::Number) {
        stmt->b = std::stoi(advance().lexeme);
    }
    if (peek().type == TokenType::Number) {
        stmt->a = std::stoi(advance().lexeme);
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseEvent() {
    auto stmt = std::make_shared<EventStmt>();
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->eventType = advance().lexeme;
    }
    stmt->handler = parseBlock();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseWidget() {
    auto stmt = std::make_shared<WidgetStmt>();
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->widgetType = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->name = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseLayout() {
    auto stmt = std::make_shared<LayoutStmt>();
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->layoutType = advance().lexeme;
    }
    match("{");
    while (!check("}") && !isAtEnd()) {
        stmt->children.push_back(parseStatement());
    }
    match("}");
    matchEnd();
    return stmt;
}

// === BATCH 7: Database Operations Implementations ===
NodePtr Parser::parseConnect() {
    auto stmt = std::make_shared<ConnectStmt>();
    if (peek().type == TokenType::String) {
        stmt->dbType = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        stmt->connectionString = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseQuery() {
    auto stmt = std::make_shared<QueryStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        stmt->sql = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->resultVar = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseInsert() {
    auto stmt = std::make_shared<InsertStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->table = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseUpdate() {
    auto stmt = std::make_shared<UpdateStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->table = advance().lexeme;
    }
    stmt->condition = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseDelete() {
    auto stmt = std::make_shared<DeleteStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->table = advance().lexeme;
    }
    stmt->condition = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseTransaction() {
    auto stmt = std::make_shared<TransactionStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    stmt->block = parseBlock();
    matchEnd();
    return stmt;
}

// === BATCH 8: Networking Implementations ===
NodePtr Parser::parseHttp() {
    auto stmt = std::make_shared<HttpStmt>();
    if (peek().type == TokenType::String || peek().type == TokenType::Identifier) {
        stmt->method = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        stmt->url = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->resultVar = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseSocket() {
    auto stmt = std::make_shared<SocketStmt>();
    stmt->port = 8080;
    if (peek().type == TokenType::String) {
        stmt->socketType = advance().lexeme;
    }
    if (peek().type == TokenType::String) {
        stmt->address = advance().lexeme;
    }
    if (peek().type == TokenType::Number) {
        stmt->port = std::stoi(advance().lexeme);
    }
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseWebSocket() {
    auto stmt = std::make_shared<WebSocketStmt>();
    if (peek().type == TokenType::String) {
        stmt->url = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    matchEnd();
    return stmt;
}

NodePtr Parser::parseListen() {
    auto stmt = std::make_shared<ListenStmt>();
    stmt->port = 8080;
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::Number) {
        stmt->port = std::stoi(advance().lexeme);
    }
    stmt->handler = parseBlock();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseSendNet() {
    auto stmt = std::make_shared<SendNetStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    stmt->data = parseExpression();
    matchEnd();
    return stmt;
}

NodePtr Parser::parseReceive() {
    auto stmt = std::make_shared<ReceiveStmt>();
    if (peek().type == TokenType::Identifier) {
        stmt->handle = advance().lexeme;
    }
    if (peek().type == TokenType::Identifier) {
        stmt->resultVar = advance().lexeme;
    }
    matchEnd();
    return stmt;
}
