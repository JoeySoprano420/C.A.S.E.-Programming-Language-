//=============================================================================
//  🌌 Violet Aura Creations — C.A.S.E. AST Node Definitions
//=============================================================================

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <iostream>

// Forward declarations
struct Node;
using NodePtr = std::shared_ptr<Node>;

static void indent(int depth) { 
    for (int i = 0; i < depth; i++) std::cout << "  "; 
}

// Base node types
struct Node { 
    virtual ~Node() = default; 
    virtual void print(int depth = 0) const = 0; 
};

struct Expr : Node {};
struct Stmt : Node {};

// Core AST Nodes
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

// Control Flow
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
    std::vector<std::pair<std::string, NodePtr>> cases;
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

// Concurrency
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
        if (value) value->print(d + 1);
    }
};

struct RecvStmt : Stmt {
    std::string channel;
    std::string targetVar;
    void print(int d = 0) const override {
        indent(d); std::cout << "RecvStmt <- " << channel << " => " << targetVar << "\n";
    }
};

// Type System
struct StructDecl : Stmt {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields;
    void print(int d = 0) const override {
        indent(d); std::cout << "StructDecl: " << name << "\n";
        for (auto& f : fields) {
            indent(d+1); std::cout << f.first << " " << f.second << "\n";
        }
    }
};

struct EnumDecl : Stmt {
    std::string name;
    std::vector<std::string> values;
    void print(int d = 0) const override {
        indent(d); std::cout << "EnumDecl: " << name << "\n";
        for (auto& v : values) {
            indent(d + 1); std::cout << v << "\n";
        }
    }
};

struct UnionDecl : Stmt {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields;
    void print(int d = 0) const override {
        indent(d); std::cout << "UnionDecl: " << name << "\n";
        for (auto& f : fields) {
            indent(d + 1); std::cout << f.first << " " << f.second << "\n";
        }
    }
};

struct TypedefStmt : Stmt {
    std::string newName;
    std::string existingType;
    void print(int d = 0) const override {
        indent(d); std::cout << "TypedefStmt: " << newName << " = " << existingType << "\n";
    }
};

// File I/O
struct OpenStmt : Stmt {
    std::string filename;
    std::string mode;
    std::string handle;
    void print(int d = 0) const override {
        indent(d); std::cout << "OpenStmt: " << handle << " = open(\"" << filename << "\", \"" << mode << "\")\n";
    }
};

struct WriteStmt : Stmt {
    std::string handle;
    NodePtr expr;
    bool newline;
    void print(int d = 0) const override {
        indent(d); std::cout << (newline ? "WritelnStmt: " : "WriteStmt: ") << handle << "\n";
        if (expr) expr->print(d + 1);
    }
};

struct ReadStmt : Stmt {
    std::string handle;
    std::string varName;
    void print(int d = 0) const override {
        indent(d); std::cout << "ReadStmt: " << varName << " from " << handle << "\n";
    }
};

struct CloseStmt : Stmt {
    std::string handle;
    void print(int d = 0) const override {
        indent(d); std::cout << "CloseStmt: " << handle << "\n";
    }
};

struct InputStmt : Stmt {
    std::string prompt;
    std::string varName;
    void print(int d = 0) const override {
        indent(d); std::cout << "InputStmt: " << varName << " (prompt: \"" << prompt << "\")\n";
    }
};

// Data Processing
struct SerializeStmt : Stmt {
    NodePtr data;
    std::string format;
    void print(int d = 0) const override {
        indent(d); std::cout << "SerializeStmt: format=" << format << "\n";
        if (data) data->print(d + 1);
    }
};

struct DeserializeStmt : Stmt {
    NodePtr source;
    std::string format;
    void print(int d = 0) const override {
        indent(d); std::cout << "DeserializeStmt: format=" << format << "\n";
        if (source) source->print(d + 1);
    }
};

struct CompressStmt : Stmt {
    NodePtr data;
    std::string algorithm;
    void print(int d = 0) const override {
        indent(d); std::cout << "CompressStmt: algorithm=" << algorithm << "\n";
        if (data) data->print(d + 1);
    }
};

struct DecompressStmt : Stmt {
    NodePtr data;
    std::string algorithm;
    void print(int d = 0) const override {
        indent(d); std::cout << "DecompressStmt: algorithm=" << algorithm << "\n";
        if (data) data->print(d + 1);
    }
};

// Security & Monitoring
struct SanitizeStmt : Stmt {
    std::string type;
    NodePtr target;
    void print(int d = 0) const override {
        indent(d); std::cout << "SanitizeStmt: type=" << type << "\n";
        if (target) target->print(d + 1);
    }
};

struct PingStmt : Stmt {
    std::string target;
    std::string protocol;
    void print(int d = 0) const override {
        indent(d); std::cout << "PingStmt: " << target << " via " << protocol << "\n";
    }
};

struct AuditStmt : Stmt {
    std::string auditType;
    std::vector<std::string> targets;
    void print(int d = 0) const override {
        indent(d); std::cout << "AuditStmt: type=" << auditType << "\n";
    }
};

struct TemperatureStmt : Stmt {
    std::string sensor;
    void print(int d = 0) const override {
        indent(d); std::cout << "TemperatureStmt: sensor=" << sensor << "\n";
    }
};

struct PressureStmt : Stmt {
    std::string resource;
    void print(int d = 0) const override {
        indent(d); std::cout << "PressureStmt: resource=" << resource << "\n";
    }
};

struct GaugeStmt : Stmt {
    std::string metric;
    NodePtr value;
    void print(int d = 0) const override {
        indent(d); std::cout << "GaugeStmt: metric=" << metric << "\n";
        if (value) value->print(d + 1);
    }
};

struct MatrixStmt : Stmt {
    std::string matrixName;
    int rows, cols;
    std::vector<NodePtr> elements;
    void print(int d = 0) const override {
        indent(d); std::cout << "MatrixStmt: " << matrixName << " [" << rows << "x" << cols << "]\n";
    }
};

// BATCH 4: Data Manipulation
struct MutateStmt : Stmt {
    std::string varName;
    NodePtr transformation;
    void print(int d = 0) const override {
        indent(d); std::cout << "MutateStmt: " << varName << "\n";
        if (transformation) transformation->print(d + 1);
    }
};

struct ScaleStmt : Stmt {
    NodePtr target;
    NodePtr factor;
    void print(int d = 0) const override {
        indent(d); std::cout << "ScaleStmt\n";
        if (target) target->print(d + 1);
        if (factor) {
            indent(d + 1); std::cout << "Factor:\n";
            factor->print(d + 2);
        }
    }
};

struct BoundsStmt : Stmt {
    std::string varName;
    NodePtr min, max;
    void print(int d = 0) const override {
        indent(d); std::cout << "BoundsStmt: " << varName << "\n";
        if (min) {
            indent(d + 1); std::cout << "Min:\n";
            min->print(d + 2);
        }
        if (max) {
            indent(d + 1); std::cout << "Max:\n";
            max->print(d + 2);
        }
    }
};

struct CheckpointStmt : Stmt {
    std::string name;
    std::vector<std::string> variables;
    void print(int d = 0) const override {
        indent(d); std::cout << "CheckpointStmt: " << name << "\n";
        for (auto& v : variables) {
            indent(d + 1); std::cout << v << "\n";
        }
    }
};

// BATCH 5: Advanced Concurrency
struct SyncStmt : Stmt {
    std::vector<std::string> resources;
    NodePtr block;
    void print(int d = 0) const override {
        indent(d); std::cout << "SyncStmt: ";
        for (size_t i = 0; i < resources.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << resources[i];
        }
        std::cout << "\n";
        if (block) block->print(d + 1);
    }
};

struct ParallelStmt : Stmt {
    std::vector<NodePtr> tasks;
    void print(int d = 0) const override {
        indent(d); std::cout << "ParallelStmt\n";
        for (auto& task : tasks) {
            task->print(d + 1);
        }
    }
};

struct BatchStmt : Stmt {
    std::string dataSource;
    NodePtr operation;
    int batchSize;
    void print(int d = 0) const override {
        indent(d); std::cout << "BatchStmt: " << dataSource << " (size=" << batchSize << ")\n";
        if (operation) operation->print(d + 1);
    }
};

struct ScheduleStmt : Stmt {
    NodePtr task;
    std::string when; // "immediate", "deferred", time expression
    void print(int d = 0) const override {
        indent(d); std::cout << "ScheduleStmt: " << when << "\n";
        if (task) task->print(d + 1);
    }
};

// BATCH 6: Graphics/UI
struct WindowStmt : Stmt {
    std::string title;
    int width, height;
    void print(int d = 0) const override {
        indent(d); std::cout << "WindowStmt: \"" << title << "\" [" << width << "x" << height << "]\n";
    }
};

struct DrawStmt : Stmt {
    std::string shape; // "rect", "circle", "line", "text"
    std::vector<NodePtr> params;
    void print(int d = 0) const override {
        indent(d); std::cout << "DrawStmt: " << shape << "\n";
        for (auto& p : params) p->print(d + 1);
    }
};

struct RenderStmt : Stmt {
    void print(int d = 0) const override {
        indent(d); std::cout << "RenderStmt\n";
    }
};

struct ColorStmt : Stmt {
    int r, g, b, a;
    void print(int d = 0) const override {
        indent(d); std::cout << "ColorStmt: RGBA(" << r << "," << g << "," << b << "," << a << ")\n";
    }
};

struct EventStmt : Stmt {
    std::string eventType; // "click", "keypress", "hover"
    NodePtr handler;
    void print(int d = 0) const override {
        indent(d); std::cout << "EventStmt: " << eventType << "\n";
        if (handler) handler->print(d + 1);
    }
};

struct WidgetStmt : Stmt {
    std::string widgetType; // "button", "label", "textbox", "slider"
    std::string name;
    std::vector<std::pair<std::string, NodePtr>> properties;
    void print(int d = 0) const override {
        indent(d); std::cout << "WidgetStmt: " << widgetType << " (" << name << ")\n";
    }
};

struct LayoutStmt : Stmt {
    std::string layoutType; // "vertical", "horizontal", "grid"
    std::vector<NodePtr> children;
    void print(int d = 0) const override {
        indent(d); std::cout << "LayoutStmt: " << layoutType << "\n";
        for (auto& child : children) child->print(d + 1);
    }
};

// BATCH 7: Database Operations
struct ConnectStmt : Stmt {
    std::string dbType; // "sqlite", "mysql", "postgres"
    std::string connectionString;
    std::string handle;
    void print(int d = 0) const override {
        indent(d); std::cout << "ConnectStmt: " << dbType << " -> " << handle << "\n";
    }
};

struct QueryStmt : Stmt {
    std::string handle;
    std::string sql;
    std::string resultVar;
    void print(int d = 0) const override {
        indent(d); std::cout << "QueryStmt: " << resultVar << " = query(" << handle << ")\n";
        indent(d + 1); std::cout << "SQL: " << sql << "\n";
    }
};

struct InsertStmt : Stmt {
    std::string handle;
    std::string table;
    std::vector<std::pair<std::string, NodePtr>> values;
    void print(int d = 0) const override {
        indent(d); std::cout << "InsertStmt: " << table << "\n";
    }
};

struct UpdateStmt : Stmt {
    std::string handle;
    std::string table;
    std::vector<std::pair<std::string, NodePtr>> values;
    NodePtr condition;
    void print(int d = 0) const override {
        indent(d); std::cout << "UpdateStmt: " << table << "\n";
    }
};

struct DeleteStmt : Stmt {
    std::string handle;
    std::string table;
    NodePtr condition;
    void print(int d = 0) const override {
        indent(d); std::cout << "DeleteStmt: " << table << "\n";
    }
};

struct TransactionStmt : Stmt {
    std::string handle;
    NodePtr block;
    void print(int d = 0) const override {
        indent(d); std::cout << "TransactionStmt\n";
        if (block) block->print(d + 1);
    }
};

// BATCH 8: Advanced Networking
struct HttpStmt : Stmt {
    std::string method; // "GET", "POST", "PUT", "DELETE"
    std::string url;
    NodePtr data;
    std::string resultVar;
    void print(int d = 0) const override {
        indent(d); std::cout << "HttpStmt: " << method << " " << url << " -> " << resultVar << "\n";
    }
};

struct SocketStmt : Stmt {
    std::string socketType; // "tcp", "udp"
    std::string address;
    int port;
    std::string handle;
    void print(int d = 0) const override {
        indent(d); std::cout << "SocketStmt: " << socketType << " " << address << ":" << port << " -> " << handle << "\n";
    }
};

struct WebSocketStmt : Stmt {
    std::string url;
    std::string handle;
    void print(int d = 0) const override {
        indent(d); std::cout << "WebSocketStmt: " << url << " -> " << handle << "\n";
    }
};

struct ListenStmt : Stmt {
    std::string handle;
    int port;
    NodePtr handler;
    void print(int d = 0) const override {
        indent(d); std::cout << "ListenStmt: port " << port << "\n";
        if (handler) handler->print(d + 1);
    }
};

struct SendNetStmt : Stmt {
    std::string handle;
    NodePtr data;
    void print(int d = 0) const override {
        indent(d); std::cout << "SendNetStmt: " << handle << "\n";
    }
};

struct ReceiveStmt : Stmt {
    std::string handle;
    std::string resultVar;
    void print(int d = 0) const override {
        indent(d); std::cout << "ReceiveStmt: " << resultVar << " <- " << handle << "\n";
    }
};

// STANDARD LIBRARY: Math Functions
struct MathCallExpr : Expr {
    std::string function; // "sin", "cos", "sqrt", "pow", "abs", "floor", "ceil", "round", "min", "max", "random"
    std::vector<NodePtr> args;
    void print(int d = 0) const override {
        indent(d); std::cout << "MathCall: " << function << "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "arg" << i;
        }
        std::cout << ")\n";
        for (auto& arg : args) arg->print(d + 1);
    }
};

// STANDARD LIBRARY: String Functions
struct StringCallExpr : Expr {
    std::string function; // "length", "substr", "concat", "split", "upper", "lower", "trim", "replace", "find"
    std::vector<NodePtr> args;
    void print(int d = 0) const override {
        indent(d); std::cout << "StringCall: " << function << "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "arg" << i;
        }
        std::cout << ")\n";
        for (auto& arg : args) arg->print(d + 1);
    }
};

// STANDARD LIBRARY: Collection Functions
struct CollectionCallExpr : Expr {
    std::string function; // "push", "pop", "shift", "unshift", "slice", "map", "filter", "reduce", "sort", "reverse"
    NodePtr collection;
    std::vector<NodePtr> args;
    void print(int d = 0) const override {
        indent(d); std::cout << "CollectionCall: " << function << "\n";
        if (collection) {
            indent(d + 1); std::cout << "On:\n";
            collection->print(d + 2);
        }
        for (auto& arg : args) arg->print(d + 1);
    }
};
