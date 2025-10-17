#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace ciam {

class MacroRegistry {
public:
    // Register or overwrite a macro body under a name
    static void registerMacro(const std::string& name, const std::string& body);

    // Returns stored body, or a default stub if not found
    static std::string getMacro(const std::string& name);

    // Persist registry to disk (creates "build" folder if needed)
    static void persist();

    // Load registry from disk if present
    static void load();

    // Optional: inspection of registration order
    static const std::vector<std::string>& history();

private:
    static std::unordered_map<std::string, std::string> macros;
    static std::vector<std::string> macroHistory;
    static const std::string registryPath;

    static void ensureRegistryPath();
};

// Utilities for integrating with the transpiler
void emitFixerMacros(int errorCode);
void emitMacroFromOverlay(const std::string& overlay);

// Simple helper to demonstrate invoking a macro and substituting an integer arg
// Returns the substituted macro body and also prints a trace line.
std::string invokeMacroFix(const std::string& macroName, int arg = 0);

} // namespace ciam
