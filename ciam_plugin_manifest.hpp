// ==========================================================
//  CIAM PLUGIN MANIFEST INTERFACE (v1.0)
//  ----------------------------------------------------------
//  Used by the C.A.S.E. Compiler to register and integrate
//  external CIAM modules (.ciam or .cpp plugins).
//
//  Author: Violet Systems
//  Spec Revision: 1.0.7
//  License: S.U.E.T. License (Standard Universal Engineering Terms)
// ==========================================================
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <iostream>
#include <sstream>
#include <type_traits>

// ==========================================================
//  CIAM Version Information
// ==========================================================
#define CIAM_PLUGIN_API_VERSION "1.0.7"
#define CIAM_PLUGIN_COMPILER_ID "C.A.S.E."
#define CIAM_PLUGIN_ABI_LEVEL   4

// ==========================================================
//  Type and Promotion System
// ==========================================================
namespace ciam {

    enum class BaseType {
        AUTO,
        INT,
        DOUBLE,
        RATIONAL,
        COMPLEX,
        STRING,
        UNKNOWN
    };

    inline std::string toString(BaseType t) {
        switch (t) {
        case BaseType::AUTO: return "auto";
        case BaseType::INT: return "int";
        case BaseType::DOUBLE: return "double";
        case BaseType::RATIONAL: return "rational";
        case BaseType::COMPLEX: return "complex";
        case BaseType::STRING: return "string";
        default: return "unknown";
        }
    }

    // Promotion Matrix
    inline bool canPromote(BaseType from, BaseType to) {
        if (to == BaseType::AUTO || to == BaseType::UNKNOWN) return true;
        if (to == BaseType::INT)       return from == BaseType::INT;
        if (to == BaseType::DOUBLE)    return from == BaseType::DOUBLE || from == BaseType::INT || from == BaseType::RATIONAL;
        if (to == BaseType::RATIONAL)  return from == BaseType::RATIONAL || from == BaseType::INT || from == BaseType::DOUBLE;
        if (to == BaseType::COMPLEX)   return from == BaseType::COMPLEX || from == BaseType::DOUBLE || from == BaseType::INT || from == BaseType::RATIONAL;
        if (to == BaseType::STRING)    return from == BaseType::STRING;
        return false;
    }

    // ==========================================================
    //  CIAM Parameter and Macro Definition
    // ==========================================================
    struct Parameter {
        std::string name;
        BaseType type = BaseType::AUTO;
        Parameter() = default;
        Parameter(std::string n, BaseType t) : name(std::move(n)), type(t) {}
    };

    struct Macro {
        std::string name;
        std::vector<Parameter> params;
        std::string body;
        bool inlineable = true;
        bool exported = false;
    };

    // ==========================================================
    //  Plugin Manifest Declaration
    // ==========================================================
    struct Manifest {
        std::string pluginName;
        std::string pluginAuthor;
        std::string pluginVersion;
        std::string pluginDescription;
        std::vector<Macro> macros;

        // Optional runtime callbacks
        std::function<void()> onLoad;
        std::function<void()> onUnload;
        std::function<void(const Macro&)> onRegister;

        Manifest() = default;
    };

    // ==========================================================
    //  Global Plugin Registry
    // ==========================================================
    inline std::vector<std::shared_ptr<Manifest>>& registry() {
        static std::vector<std::shared_ptr<Manifest>> reg;
        return reg;
    }

    // Register a new plugin manifest into the global registry
    inline void registerManifest(std::shared_ptr<Manifest> m) {
        registry().push_back(std::move(m));
    }

    // ==========================================================
    //  Registration Macros
    // ==========================================================
#define CIAM_BEGIN_PLUGIN(NAME, AUTHOR, VERSION, DESC)                     \
    namespace {                                                            \
    std::shared_ptr<ciam::Manifest> __ciam_manifest =                      \
        std::make_shared<ciam::Manifest>();                                \
    struct __CiamInit {                                                    \
        __CiamInit() {                                                     \
            __ciam_manifest->pluginName = NAME;                            \
            __ciam_manifest->pluginAuthor = AUTHOR;                        \
            __ciam_manifest->pluginVersion = VERSION;                      \
            __ciam_manifest->pluginDescription = DESC;                     \
            ciam::registerManifest(__ciam_manifest);                       \
        }                                                                  \
    } __ciam_initializer;                                                  \
    } /* end anon */

#define CIAM_REGISTER_MACRO(NAME, BODY, ...)                               \
    {                                                                      \
        ciam::Macro m;                                                     \
        m.name = NAME;                                                     \
        m.body = BODY;                                                     \
        m.inlineable = true;                                               \
        m.exported = true;                                                 \
        m.params = { __VA_ARGS__ };                                        \
        __ciam_manifest->macros.push_back(m);                              \
        if (__ciam_manifest->onRegister) __ciam_manifest->onRegister(m);   \
    }

#define CIAM_END_PLUGIN()                                                  \
    namespace { struct __ciam_dummy_end__ {}; } /* plugin terminator */

// ==========================================================
//  Helper Utilities
// ==========================================================
    inline BaseType inferBaseType(const std::string& token) {
        if (token.find('"') != std::string::npos) return BaseType::STRING;
        if (token.find('.') != std::string::npos) return BaseType::DOUBLE;
        if (token.find('/') != std::string::npos) return BaseType::RATIONAL;
        return BaseType::INT;
    }

    inline bool validateArgs(const Macro& m, const std::vector<BaseType>& args) {
        if (m.params.size() != args.size()) return false;
        for (size_t i = 0; i < args.size(); ++i)
            if (!canPromote(args[i], m.params[i].type))
                return false;
        return true;
    }

    inline std::string manifestSummary() {
        std::ostringstream out;
        out << "=== CIAM Plugin Registry (" << registry().size() << " loaded) ===\n";
        for (auto& m : registry()) {
            out << "→ " << m->pluginName << " v" << m->pluginVersion
                << " by " << m->pluginAuthor << "\n";
            for (auto& mac : m->macros) {
                out << "   • " << mac.name << "(";
                for (size_t i = 0; i < mac.params.size(); ++i) {
                    out << ciam::toString(mac.params[i].type) << " " << mac.params[i].name;
                    if (i + 1 < mac.params.size()) out << ", ";
                }
                out << ")\n";
            }
        }
        out << "===============================================\n";
        return out.str();
    }

} // namespace ciam

// ==========================================================
//  END OF FILE
// ==========================================================
