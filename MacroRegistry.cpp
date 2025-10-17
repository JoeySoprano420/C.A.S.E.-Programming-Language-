#include "MacroRegistry.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
  #include <direct.h>
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <errno.h>
#endif

namespace ciam {

// Static storage
std::unordered_map<std::string, std::string> MacroRegistry::macros;
std::vector<std::string> MacroRegistry::macroHistory;
const std::string MacroRegistry::registryPath = "build/macro_registry.case";

void MacroRegistry::ensureRegistryPath() {
#ifdef _WIN32
    _mkdir("build"); // returns 0 on success, -1 on error (e.g., already exists)
#else
    // 0755, ignore EEXIST
    ::mkdir("build", 0755);
#endif
}

void MacroRegistry::registerMacro(const std::string& name, const std::string& body) {
    macros[name] = body;
    macroHistory.push_back(name);
}

std::string MacroRegistry::getMacro(const std::string& name) {
    auto it = macros.find(name);
    if (it != macros.end()) return it->second;
    // Default stub body in DSL style
    return "ret -1";
}

void MacroRegistry::persist() {
    ensureRegistryPath();
    std::ofstream out(registryPath);
    if (!out) return;
    for (const auto& kv : macros) {
        const auto& name = kv.first;
        const auto& body = kv.second;
        out << "macro " << name << "() { " << body << " }\n";
    }
}

void MacroRegistry::load() {
    macros.clear();
    macroHistory.clear();

    std::ifstream in(registryPath);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        // Expected format: macro <name>() { <body> }
        auto mpos = line.find("macro ");
        if (mpos == std::string::npos) continue;
        auto nameStart = mpos + 6;
        auto nameEnd = line.find("()", nameStart);
        auto braceL = line.find('{', nameEnd);
        auto braceR = line.rfind('}');
        if (nameEnd == std::string::npos || braceL == std::string::npos || braceR == std::string::npos || braceR <= braceL) continue;

        std::string name = line.substr(nameStart, nameEnd - nameStart);
        // Trim spaces around name
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.erase(name.begin());
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();

        std::string body = line.substr(braceL + 1, braceR - braceL - 1);
        // Trim body
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.front()))) body.erase(body.begin());
        while (!body.empty() && std::isspace(static_cast<unsigned char>(body.back()))) body.pop_back();

        macros[name] = body;
        macroHistory.push_back(name);
    }
}

const std::vector<std::string>& MacroRegistry::history() {
    return macroHistory;
}

// ---------------- Utilities ----------------

void emitFixerMacros(int errorCode) {
    switch (errorCode) {
        case 1:
            MacroRegistry::registerMacro("fix_error_1", "ret 42");
            break;
        case 2:
            MacroRegistry::registerMacro("fix_error_2", "ret x * 2");
            break;
        case 404:
            MacroRegistry::registerMacro("fix_missing", "ret 0");
            break;
        default:
            MacroRegistry::registerMacro("fix_unknown", "ret -1");
            break;
    }
    MacroRegistry::persist();
}

void emitMacroFromOverlay(const std::string& overlay) {
    if (overlay == "audit") {
        MacroRegistry::registerMacro("audit_trace", "ret 'AST audited'");
    } else if (overlay == "mutate") {
        MacroRegistry::registerMacro("mutate_patch", "ret 'mutation applied'");
    } else if (overlay == "replay") {
        MacroRegistry::registerMacro("replay_note", "ret 'replay active'");
    }
    MacroRegistry::persist();
}

static std::string substituteArgAllX(const std::string& body, int arg) {
    // Naive substitution of 'x' with integer arg; improve with a tokenizer if needed.
    std::ostringstream oss;
    for (char c : body) {
        if (c == 'x') oss << arg;
        else oss << c;
    }
    return oss.str();
}

std::string invokeMacroFix(const std::string& macroName, int arg) {
    std::string body = MacroRegistry::getMacro(macroName);
    std::string substituted = substituteArgAllX(body, arg);
    std::cout << "Invoking macro " << macroName << ": " << substituted << "\n";
    return substituted;
}

} // namespace ciam
