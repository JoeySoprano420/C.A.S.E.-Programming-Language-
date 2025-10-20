//=============================================================================
//  🌌 Violet Aura Creations — C.A.S.E. Code Emitter Header
//=============================================================================

#pragma once
#include "AST.hpp"
#include <sstream>
#include <string>

class CodeEmitter {
public:
    std::string emit(NodePtr root);

private:
    void emitNode(NodePtr node, std::ostringstream& out);
    void emitExpr(NodePtr expr, std::ostringstream& out);
};
