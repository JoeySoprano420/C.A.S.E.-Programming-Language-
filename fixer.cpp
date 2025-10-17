#pragma once
#include <string>
#include <unordered_map>
#include <fstream>

namespace ciam {

class MacroRegistry {
public:
    static void registerMacro(const std::string& name, const std::string& body);
    static std::string getMacro(const std::string& name);
    static void persist();
    static void load();

private:
    static std::unordered_map<std::string, std::string> macros;
    static const std::string registryPath;
};

} // namespace ciam

#include "MacroRegistry.hpp"

namespace ciam {

std::unordered_map<std::string, std::string> MacroRegistry::macros;
const std::string MacroRegistry::registryPath = "build/macro_registry.case";

void MacroRegistry::registerMacro(const std::string& name, const std::string& body) {
    macros[name] = body;
}

std::string MacroRegistry::getMacro(const std::string& name) {
    return macros.count(name) ? macros[name] : "macro " + name + "() { ret -1 }";
}

void MacroRegistry::persist() {
    std::ofstream out(registryPath);
    for (const auto& [name, body] : macros) {
        out << "macro " << name << "() { " << body << " }\n";
    }
    out.close();
}

void MacroRegistry::load() {
    std::ifstream in(registryPath);
    std::string line;
    while (std::getline(in, line)) {
        auto start = line.find("macro ") + 6;
        auto end = line.find("()");
        auto bodyStart = line.find("{") + 1;
        auto bodyEnd = line.find("}");
        if (start != std::string::npos && end != std::string::npos) {
            std::string name = line.substr(start, end - start);
            std::string body = line.substr(bodyStart, bodyEnd - bodyStart);
            macros[name] = body;
        }
    }
    in.close();
}

} // namespace ciam

#include "MacroRegistry.hpp"

namespace ciam {

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

}

#include "MacroRegistry.hpp"

void invokeMacroFix(const std::string& macroName, int arg = 0) {
    std::string body = ciam::MacroRegistry::getMacro(macroName);
    std::cout << "Invoking macro " << macroName << ": " << body << "\n";
    // Optional: parse and execute macro body with arg substitution
}

int main() {
    ciam::MacroRegistry::load();  // Load macros from previous builds
    ciam::emitFixerMacros(2);     // Emit macro for error code 2
   
std::string substituteArg(const std::string& body, int arg) {
    std::string result = body;
    size_t pos = result.find("x");
    if (pos != std::string::npos) {
        result.replace(pos, 1, std::to_string(arg));
    }
    return result;
}

std::string body = ciam::MacroRegistry::getMacro(macroName);
std::string substituted = substituteArg(body, arg);
std::cout << "Invoking macro " << macroName << ": " << substituted << "\n";

#include <filesystem>
void ensureRegistryPath() {
    std::filesystem::create_directories("build");
}

static std::vector<std::string> macroHistory;

void MacroRegistry::registerMacro(const std::string& name, const std::string& body) {
    macros[name] = body;
    macroHistory.push_back(name);
}

void emitMacroFromOverlay(const std::string& overlay) {
    if (overlay == "audit") {
        MacroRegistry::registerMacro("audit_trace", "ret 'AST audited'");
    } else if (overlay == "mutate") {
        MacroRegistry::registerMacro("mutate_patch", "ret 'mutation applied'");
    }
    MacroRegistry::persist();
} 
