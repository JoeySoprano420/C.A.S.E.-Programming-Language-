//=============================================================================
//  ?? Violet Aura Creations — C.A.S.E. Code Emitter Implementation
//=============================================================================

#include "CodeEmitter.hpp"
#include <fstream>

std::string CodeEmitter::emit(NodePtr root) {
    std::ostringstream out;
    out << "#include <iostream>\n";
    out << "#include <string>\n";
    out << "#include <cmath>\n";
    out << "#include <fstream>\n";
    out << "#include <queue>\n";
    out << "#include <thread>\n";
    out << "#include <future>\n";
    out << "#include <vector>\n\n";
    
    emitNode(root, out);
    return out.str();
}

void CodeEmitter::emitNode(NodePtr node, std::ostringstream& out) {
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
    // BATCH 1: Type System Extensions
    else if (auto enumDecl = std::dynamic_pointer_cast<EnumDecl>(node)) {
        out << "enum class " << enumDecl->name << " {\n";
        for (size_t i = 0; i < enumDecl->values.size(); ++i) {
            out << "    " << enumDecl->values[i];
            if (i < enumDecl->values.size() - 1) out << ",";
            out << "\n";
        }
        out << "};\n";
    }
    else if (auto unionDecl = std::dynamic_pointer_cast<UnionDecl>(node)) {
        out << "union " << unionDecl->name << " {\n";
        for (auto& field : unionDecl->fields) {
            out << "    " << field.first << " " << field.second << ";\n";
        }
        out << "};\n";
    }
    else if (auto typedefStmt = std::dynamic_pointer_cast<TypedefStmt>(node)) {
        out << "using " << typedefStmt->newName << " = " << typedefStmt->existingType << ";\n";
    }
    // BATCH 2: File I/O & Input
    else if (auto openStmt = std::dynamic_pointer_cast<OpenStmt>(node)) {
        out << "std::fstream " << openStmt->handle << "(\"" << openStmt->filename << "\", ";
        if (openStmt->mode == "r") out << "std::ios::in";
        else if (openStmt->mode == "w") out << "std::ios::out";
        else if (openStmt->mode == "rw") out << "std::ios::in | std::ios::out";
        else out << "std::ios::out";
        out << ");\n";
    }
    else if (auto writeStmt = std::dynamic_pointer_cast<WriteStmt>(node)) {
        out << writeStmt->handle << " << ";
        emitExpr(writeStmt->expr, out);
        if (writeStmt->newline) out << " << std::endl";
        out << ";\n";
    }
    else if (auto readStmt = std::dynamic_pointer_cast<ReadStmt>(node)) {
        out << readStmt->handle << " >> " << readStmt->varName << ";\n";
    }
    else if (auto closeStmt = std::dynamic_pointer_cast<CloseStmt>(node)) {
        out << closeStmt->handle << ".close();\n";
    }
    else if (auto inputStmt = std::dynamic_pointer_cast<InputStmt>(node)) {
        out << "std::cout << \"" << inputStmt->prompt << "\";\n";
        out << "std::cin >> " << inputStmt->varName << ";\n";
    }
    else if (auto serializeStmt = std::dynamic_pointer_cast<SerializeStmt>(node)) {
        out << "// Serialize to " << serializeStmt->format << ": ";
        emitExpr(serializeStmt->data, out);
        out << "\n";
    }
    else if (auto deserializeStmt = std::dynamic_pointer_cast<DeserializeStmt>(node)) {
        out << "// Deserialize from " << deserializeStmt->format << ": ";
        emitExpr(deserializeStmt->source, out);
        out << "\n";
    }
    else if (auto compressStmt = std::dynamic_pointer_cast<CompressStmt>(node)) {
        out << "// Compress with " << compressStmt->algorithm << ": ";
        emitExpr(compressStmt->data, out);
        out << "\n";
    }
    else if (auto decompressStmt = std::dynamic_pointer_cast<DecompressStmt>(node)) {
        out << "// Decompress with " << decompressStmt->algorithm << ": ";
        emitExpr(decompressStmt->data, out);
        out << "\n";
    }
    // BATCH 3: Security & Monitoring
    else if (auto sanitizeStmt = std::dynamic_pointer_cast<SanitizeStmt>(node)) {
        if (sanitizeStmt->type == "mem") {
            out << "// Memory sanitization: ";
            emitExpr(sanitizeStmt->target, out);
            out << "\nstd::memset(&";
            emitExpr(sanitizeStmt->target, out);
            out << ", 0, sizeof(";
            emitExpr(sanitizeStmt->target, out);
            out << "));\n";
        } else {
            out << "// Code sanitization (static analysis pass)\n";
        }
    }
    else if (auto pingStmt = std::dynamic_pointer_cast<PingStmt>(node)) {
        out << "// Network ping: " << pingStmt->target << " via " << pingStmt->protocol << "\n";
    }
    else if (auto auditStmt = std::dynamic_pointer_cast<AuditStmt>(node)) {
        out << "// Audit " << auditStmt->auditType << ": ";
        for (size_t i = 0; i < auditStmt->targets.size(); ++i) {
            if (i > 0) out << ", ";
            out << auditStmt->targets[i];
        }
        out << "\n";
    }
    else if (auto tempStmt = std::dynamic_pointer_cast<TemperatureStmt>(node)) {
        out << "// Read " << tempStmt->sensor << " temperature\n";
        out << "auto temp = 0.0; // Platform-specific sensor reading\n";
    }
    else if (auto pressureStmt = std::dynamic_pointer_cast<PressureStmt>(node)) {
        out << "// Monitor " << pressureStmt->resource << " pressure/load\n";
        out << "auto pressure = 0.0; // Platform-specific monitoring\n";
    }
    else if (auto gaugeStmt = std::dynamic_pointer_cast<GaugeStmt>(node)) {
        out << "// Gauge metric: " << gaugeStmt->metric << " = ";
        emitExpr(gaugeStmt->value, out);
        out << "\n";
    }
    else if (auto matrixStmt = std::dynamic_pointer_cast<MatrixStmt>(node)) {
        out << "std::vector<std::vector<double>> " << matrixStmt->matrixName;
        out << "(" << matrixStmt->rows << ", std::vector<double>(" << matrixStmt->cols << "));\n";
    }
    // BATCH 4: Data Manipulation
    else if (auto mutateStmt = std::dynamic_pointer_cast<MutateStmt>(node)) {
        out << "// Mutate " << mutateStmt->varName << " with transformation\n";
        out << mutateStmt->varName << " = ";
        emitExpr(mutateStmt->transformation, out);
        out << ";\n";
    }
    else if (auto scaleStmt = std::dynamic_pointer_cast<ScaleStmt>(node)) {
        out << "// Scale operation\n";
        emitExpr(scaleStmt->target, out);
        out << " *= ";
        emitExpr(scaleStmt->factor, out);
        out << ";\n";
    }
    else if (auto boundsStmt = std::dynamic_pointer_cast<BoundsStmt>(node)) {
        out << "// Bounds check for " << boundsStmt->varName << "\n";
        out << "if (" << boundsStmt->varName << " < ";
        emitExpr(boundsStmt->min, out);
        out << ") " << boundsStmt->varName << " = ";
        emitExpr(boundsStmt->min, out);
        out << ";\n";
        out << "if (" << boundsStmt->varName << " > ";
        emitExpr(boundsStmt->max, out);
        out << ") " << boundsStmt->varName << " = ";
        emitExpr(boundsStmt->max, out);
        out << ";\n";
    }
    else if (auto checkpointStmt = std::dynamic_pointer_cast<CheckpointStmt>(node)) {
        out << "// Checkpoint: " << checkpointStmt->name << "\n";
        out << "struct checkpoint_" << checkpointStmt->name << " {\n";
        for (auto& var : checkpointStmt->variables) {
            out << "    auto " << var << "_saved = " << var << ";\n";
        }
        out << "};\n";
    }
    // BATCH 5: Advanced Concurrency
    else if (auto syncStmt = std::dynamic_pointer_cast<SyncStmt>(node)) {
        out << "{\n";
        out << "    std::lock_guard<std::mutex> lock(global_mutex);\n";
        out << "    // Synchronized block for: ";
        for (size_t i = 0; i < syncStmt->resources.size(); ++i) {
            if (i > 0) out << ", ";
            out << syncStmt->resources[i];
        }
        out << "\n";
        emitNode(syncStmt->block, out);
        out << "}\n";
    }
    else if (auto parallelStmt = std::dynamic_pointer_cast<ParallelStmt>(node)) {
        out << "{\n";
        out << "    std::vector<std::thread> threads;\n";
        for (size_t i = 0; i < parallelStmt->tasks.size(); ++i) {
            out << "    threads.emplace_back([&]() {\n";
            emitNode(parallelStmt->tasks[i], out);
            out << "    });\n";
        }
        out << "    for (auto& t : threads) t.join();\n";
        out << "}\n";
    }
    else if (auto batchStmt = std::dynamic_pointer_cast<BatchStmt>(node)) {
        out << "// Batch processing: " << batchStmt->dataSource << " (size=" << batchStmt->batchSize << ")\n";
        out << "for (size_t batch_start = 0; batch_start < " << batchStmt->dataSource << ".size(); batch_start += " << batchStmt->batchSize << ") {\n";
        out << "    size_t batch_end = std::min(batch_start + " << batchStmt->batchSize << ", " << batchStmt->dataSource << ".size());\n";
        emitNode(batchStmt->operation, out);
        out << "}\n";
    }
    else if (auto scheduleStmt = std::dynamic_pointer_cast<ScheduleStmt>(node)) {
        out << "// Schedule task: " << scheduleStmt->when << "\n";
        out << "std::async(std::launch::deferred, [&]() {\n";
        emitNode(scheduleStmt->task, out);
        out << "});\n";
    }
    // BATCH 6: Graphics/UI
    else if (auto windowStmt = std::dynamic_pointer_cast<WindowStmt>(node)) {
        out << "// Create window: \"" << windowStmt->title << "\" " << windowStmt->width << "x" << windowStmt->height << "\n";
        out << "auto window = createWindow(\"" << windowStmt->title << "\", " << windowStmt->width << ", " << windowStmt->height << ");\n";
    }
    else if (auto drawStmt = std::dynamic_pointer_cast<DrawStmt>(node)) {
        out << "// Draw " << drawStmt->shape << "\n";
        out << "draw_" << drawStmt->shape << "(";
        for (size_t i = 0; i < drawStmt->params.size(); ++i) {
            if (i > 0) out << ", ";
            emitExpr(drawStmt->params[i], out);
        }
        out << ");\n";
    }
    else if (std::dynamic_pointer_cast<RenderStmt>(node)) {
        out << "// Render frame\n";
        out << "render();\n";
    }
    else if (auto colorStmt = std::dynamic_pointer_cast<ColorStmt>(node)) {
        out << "// Set color: RGBA(" << colorStmt->r << "," << colorStmt->g << "," << colorStmt->b << "," << colorStmt->a << ")\n";
        out << "setColor(" << colorStmt->r << ", " << colorStmt->g << ", " << colorStmt->b << ", " << colorStmt->a << ");\n";
    }
    else if (auto eventStmt = std::dynamic_pointer_cast<EventStmt>(node)) {
        out << "// Event handler: " << eventStmt->eventType << "\n";
        out << "on_" << eventStmt->eventType << "([&]() {\n";
        emitNode(eventStmt->handler, out);
        out << "});\n";
    }
    else if (auto widgetStmt = std::dynamic_pointer_cast<WidgetStmt>(node)) {
        out << "// Widget: " << widgetStmt->widgetType << " (" << widgetStmt->name << ")\n";
        out << "auto " << widgetStmt->name << " = create_" << widgetStmt->widgetType << "();\n";
    }
    else if (auto layoutStmt = std::dynamic_pointer_cast<LayoutStmt>(node)) {
        out << "// Layout: " << layoutStmt->layoutType << "\n";
        out << "{\n";
        out << "    auto layout = create_" << layoutStmt->layoutType << "_layout();\n";
        for (auto& child : layoutStmt->children) {
            out << "    layout.add(";
            emitNode(child, out);
            out << ");\n";
        }
        out << "}\n";
    }
    // BATCH 7: Database Operations
    else if (auto connectStmt = std::dynamic_pointer_cast<ConnectStmt>(node)) {
        out << "// Database connection: " << connectStmt->dbType << "\n";
        out << "auto " << connectStmt->handle << " = db::connect(\"" << connectStmt->dbType << "\", \"" << connectStmt->connectionString << "\");\n";
    }
    else if (auto queryStmt = std::dynamic_pointer_cast<QueryStmt>(node)) {
        out << "// Query database\n";
        out << "auto " << queryStmt->resultVar << " = " << queryStmt->handle << ".query(\"" << queryStmt->sql << "\");\n";
    }
    else if (auto insertStmt = std::dynamic_pointer_cast<InsertStmt>(node)) {
        out << "// Insert into " << insertStmt->table << "\n";
        out << insertStmt->handle << ".insert(\"" << insertStmt->table << "\", data);\n";
    }
    else if (auto updateStmt = std::dynamic_pointer_cast<UpdateStmt>(node)) {
        out << "// Update " << updateStmt->table << "\n";
        out << updateStmt->handle << ".update(\"" << updateStmt->table << "\", data, ";
        emitExpr(updateStmt->condition, out);
        out << ");\n";
    }
    else if (auto deleteStmt = std::dynamic_pointer_cast<DeleteStmt>(node)) {
        out << "// Delete from " << deleteStmt->table << "\n";
        out << deleteStmt->handle << ".del(\"" << deleteStmt->table << "\", ";
        emitExpr(deleteStmt->condition, out);
        out << ");\n";
    }
    else if (auto txStmt = std::dynamic_pointer_cast<TransactionStmt>(node)) {
        out << "// Transaction\n";
        out << txStmt->handle << ".beginTransaction();\n";
        out << "try {\n";
        emitNode(txStmt->block, out);
        out << "    " << txStmt->handle << ".commit();\n";
        out << "} catch(...) {\n";
        out << "    " << txStmt->handle << ".rollback();\n";
        out << "}\n";
    }
    // BATCH 8: Networking
    else if (auto httpStmt = std::dynamic_pointer_cast<HttpStmt>(node)) {
        out << "// HTTP " << httpStmt->method << " request\n";
        out << "auto " << httpStmt->resultVar << " = http::" << httpStmt->method << "(\"" << httpStmt->url << "\");\n";
    }
    else if (auto socketStmt = std::dynamic_pointer_cast<SocketStmt>(node)) {
        out << "// Socket: " << socketStmt->socketType << "\n";
        out << "auto " << socketStmt->handle << " = socket::create(\"" << socketStmt->socketType << "\", \"" << socketStmt->address << "\", " << socketStmt->port << ");\n";
    }
    else if (auto wsStmt = std::dynamic_pointer_cast<WebSocketStmt>(node)) {
        out << "// WebSocket connection\n";
        out << "auto " << wsStmt->handle << " = websocket::connect(\"" << wsStmt->url << "\");\n";
    }
    else if (auto listenStmt = std::dynamic_pointer_cast<ListenStmt>(node)) {
        out << "// Listen on port " << listenStmt->port << "\n";
        out << listenStmt->handle << ".listen(" << listenStmt->port << ", [&](auto request) {\n";
        emitNode(listenStmt->handler, out);
        out << "});\n";
    }
    else if (auto sendNetStmt = std::dynamic_pointer_cast<SendNetStmt>(node)) {
        out << "// Send network data\n";
        out << sendNetStmt->handle << ".send(";
        emitExpr(sendNetStmt->data, out);
        out << ");\n";
    }
    else if (auto recvStmt = std::dynamic_pointer_cast<ReceiveStmt>(node)) {
        out << "// Receive network data\n";
        out << "auto " << recvStmt->resultVar << " = " << recvStmt->handle << ".receive();\n";
    }
}

void CodeEmitter::emitExpr(NodePtr node, std::ostringstream& out) {
    if (auto lit = std::dynamic_pointer_cast<Literal>(node)) {
        out << lit->value;
    }
    else if (auto id = std::dynamic_pointer_cast<Identifier>(node)) {
        out << id->name;
    }
    else if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(node)) {
        out << "(";
        emitExpr(bin->left, out);
        out << " " << bin->op << " ";
        emitExpr(bin->right, out);
        out << ")";
    }
    else if (auto callExpr = std::dynamic_pointer_cast<CallExpr>(node)) {
        out << callExpr->callee << "(";
        for (size_t i = 0; i < callExpr->args.size(); ++i) {
            if (i > 0) out << ", ";
            emitExpr(callExpr->args[i], out);
        }
        out << ")";
    }
    // Standard Library: Math
    else if (auto mathCall = std::dynamic_pointer_cast<MathCallExpr>(node)) {
        out << "std::" << mathCall->function << "(";
        for (size_t i = 0; i < mathCall->args.size(); ++i) {
            if (i > 0) out << ", ";
            emitExpr(mathCall->args[i], out);
        }
        out << ")";
    }
    // Standard Library: Strings
    else if (auto strCall = std::dynamic_pointer_cast<StringCallExpr>(node)) {
        if (strCall->function == "length" && !strCall->args.empty()) {
            emitExpr(strCall->args[0], out);
            out << ".length()";
        } else {
            out << "str_" << strCall->function << "(";
            for (size_t i = 0; i < strCall->args.size(); ++i) {
                if (i > 0) out << ", ";
                emitExpr(strCall->args[i], out);
            }
            out << ")";
        }
    }
    // Standard Library: Collections
    else if (auto collCall = std::dynamic_pointer_cast<CollectionCallExpr>(node)) {
        if (collCall->function == "size" && collCall->collection) {
            emitExpr(collCall->collection, out);
            out << ".size()";
        } else if (collCall->function == "push" && collCall->collection) {
            emitExpr(collCall->collection, out);
            out << ".push_back(";
            if (!collCall->args.empty()) emitExpr(collCall->args[0], out);
            out << ")";
        } else {
            out << "coll_" << collCall->function << "(";
            if (collCall->collection) {
                emitExpr(collCall->collection, out);
                if (!collCall->args.empty()) out << ", ";
            }
            for (size_t i = 0; i < collCall->args.size(); ++i) {
                if (i > 0) out << ", ";
                emitExpr(collCall->args[i], out);
            }
            out << ")";
        }
    }
}

