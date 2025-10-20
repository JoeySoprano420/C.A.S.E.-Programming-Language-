#pragma once
#include <string>

namespace ciam {

    // CIAM source-to-source preprocessor. Safe to call unconditionally.
    // If the DSL contains `call CIAM[on]`, it will:
    // - repair ambiguous constructs (e.g., Print with no arg),
    // - auto-alias near-miss function calls,
    // - abstract repeated Print literals into generated Fn macros,
    // - CIAM write_stdout macros: `CIAM write_stdout { ... }` -> Print "...",
    // - Plugin overlays and symbolic introspection: `CIAM overlay[Name]`, `CIAM inspect[Fns|symbols|overlays]`,
    // - Self-mutation sandbox and capability audit: `CIAM sandbox { ... }`, `CIAM audit[]`,
    // - Base-12 numerics (0–9, a, b) for integer literals starting with a digit.
    //
    // If CIAM is not enabled inline, Process returns the input unchanged.
    class Preprocessor {
    public:
        // Transform DSL source according to CIAM rules (only when enabled inline).
        // - Keeps original formatting as much as reasonably possible.
        // - Removes CIAM directives (`call CIAM[on]`/`off`) from output.
        std::string Process(const std::string& src);
    };

} // namespace ciam
