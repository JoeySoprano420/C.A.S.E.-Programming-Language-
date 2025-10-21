//=============================================================================
//  ?? Violet Aura Creations — CIAM Preprocessor Implementation
//=============================================================================

#include "intelligence.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ciam {
    namespace {

        // Lightweight tokenizer for the DSL, preserving whitespace to keep formatting stable
        enum class K { Word, String, Symbol, Whitespace };

        struct Tok {
            K kind;
            std::string text; // For String, this stores unescaped content (without quotes)
        };

        static bool isWordStart(char c) {
            unsigned char uc = static_cast<unsigned char>(c);
            return std::isalpha(uc) || c == '_';
        }
        static bool isWordChar(char c) {
            unsigned char uc = static_cast<unsigned char>(c);
            return std::isalnum(uc) || c == '_';
        }
        static bool isSymbolChar(char c) {
            switch (c) {
            case '(':
            case ')':
            case '{':
            case '}':
            case '[':
            case ']':
            case '=':
            case ';':
            case ',':
            case '+':
            case '-':
            case '*':
            case '/':
            case '<':
            case '>':
            case '!':
            case '&':
            case '|':
            case '%':
            case ':':
            case '.':
                return true;
            default:
                return false;
            }
        }

        static std::vector<Tok> lex(const std::string& src) {
            std::vector<Tok> out;
            size_t i = 0, n = src.size();

            auto push = [&](K k, const std::string& t) { out.push_back(Tok{ k, t }); };

            while (i < n) {
                char c = src[i];

                // Whitespace
                if (std::isspace(static_cast<unsigned char>(c))) {
                    size_t s = i++;
                    while (i < n && std::isspace(static_cast<unsigned char>(src[i]))) ++i;
                    push(K::Whitespace, src.substr(s, i - s));
                    continue;
                }

                // Line comments //...
                if (c == '/' && i + 1 < n && src[i + 1] == '/') {
                    size_t s = i;
                    i += 2;
                    while (i < n && src[i] != '\n') ++i;
                    push(K::Whitespace, src.substr(s, i - s));
                    continue;
                }

                // String literal "..."
                if (c == '"') {
                    ++i; // consume opening "
                    std::string acc;
                    bool closed = false;
                    while (i < n) {
                        char ch = src[i++];
                        if (ch == '"') { closed = true; break; }
                        if (ch == '\\' && i < n) {
                            char esc = src[i++];
                            switch (esc) {
                            case 'n': acc.push_back('\n'); break;
                            case 't': acc.push_back('\t'); break;
                            case 'r': acc.push_back('\r'); break;
                            case '\\': acc.push_back('\\'); break;
                            case '"': acc.push_back('"'); break;
                            default: acc.push_back(esc); break;
                            }
                        }
                        else {
                            acc.push_back(ch);
                        }
                    }
                    // Accept even if unterminated (best-effort preservation)
                    push(K::String, acc);
                    continue;
                }

                // Word
                if (isWordStart(c)) {
                    size_t s = i++;
                    while (i < n && isWordChar(src[i])) ++i;
                    push(K::Word, src.substr(s, i - s));
                    continue;
                }

                // Symbol
                if (isSymbolChar(c)) {
                    push(K::Symbol, std::string(1, c));
                    ++i;
                    continue;
                }

                // Fallback: keep unknown single char as whitespace to remain lossless
                push(K::Whitespace, std::string(1, c));
                ++i;
            }

            return out;
        }

        static std::string escapeString(const std::string& s) {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s) {
                switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                default:   out.push_back(c); break;
                }
            }
            return out;
        }

        static std::string toText(const std::vector<Tok>& toks) {
            std::string s;
            s.reserve(toks.size() * 4);
            for (const auto& t : toks) {
                if (t.kind == K::String) {
                    s.push_back('"');
                    s += escapeString(t.text);
                    s.push_back('"');
                }
                else {
                    s += t.text;
                }
            }
            return s;
        }

        static size_t nextNonWs(const std::vector<Tok>& ts, size_t i) {
            size_t j = i;
            while (j < ts.size() && ts[j].kind == K::Whitespace) ++j;
            return j;
        }

        static bool eqWord(const Tok& t, const char* w) {
            return t.kind == K::Word && t.text == w;
        }

        static bool eqSym(const Tok& t, char c) {
            return t.kind == K::Symbol && t.text.size() == 1 && t.text[0] == c;
        }

        static int editDistance(const std::string& a, const std::string& b) {
            const size_t n = a.size(), m = b.size();
            std::vector<int> prev(m + 1), cur(m + 1);
            for (size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
            for (size_t i = 1; i <= n; ++i) {
                cur[0] = static_cast<int>(i);
                for (size_t j = 1; j <= m; ++j) {
                    int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
                    cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
                }
                std::swap(prev, cur);
            }
            return prev[m];
        }

        static std::string toLower(std::string s) {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        static size_t findMatchingBrace(const std::vector<Tok>& ts, size_t openIdx) {
            if (openIdx >= ts.size() || !eqSym(ts[openIdx], '{')) return ts.size();
            int depth = 0;
            for (size_t i = openIdx; i < ts.size(); ++i) {
                if (eqSym(ts[i], '{')) ++depth;
                else if (eqSym(ts[i], '}')) {
                    --depth;
                    if (depth == 0) return i;
                }
            }
            return ts.size();
        }

        static bool isBase12Word(const std::string& w) {
            if (w.empty()) return false;
            if (!std::isdigit(static_cast<unsigned char>(w[0]))) return false; // must start with digit
            bool ok = true, hasAB = false;
            for (char c : w) {
                if (c >= '0' && c <= '9') continue;
                char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (lc == 'a' || lc == 'b') { hasAB = true; continue; }
                ok = false; break;
            }
            return ok && hasAB; // only convert if it actually uses a/b
        }

        static bool base12ToDecimal(const std::string& w, std::string& out) {
            unsigned long long val = 0;
            for (char c : w) {
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else {
                    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (lc == 'a') d = 10;
                    else if (lc == 'b') d = 11;
                    else return false;
                }
                if (val > (std::numeric_limits<unsigned long long>::max)() / 12ULL) return false;
                val *= 12ULL;
                if (val > (std::numeric_limits<unsigned long long>::max)() - static_cast<unsigned long long>(d)) return false;
                val += static_cast<unsigned long long>(d);
            }
            out = std::to_string(val);
            return true;
        }

        struct PrintOccur {
            size_t idxPrint;   // index of 'Print'
            size_t idxString;  // index of string token if present, else (size_t)-1
            std::string literal;
        };

        struct CallOccur {
            size_t idxCall;  // index of 'call'
            size_t idxName;  // index of call target word
        };

        static void eraseRange(std::vector<Tok>& ts, size_t i, size_t j) {
            if (i >= j || i >= ts.size()) return;
            j = std::min(j, ts.size());
            ts.erase(ts.begin() + static_cast<std::ptrdiff_t>(i),
                ts.begin() + static_cast<std::ptrdiff_t>(j));
        }

    } // namespace

    std::string Preprocessor::Process(const std::string& src) {
        // 0) Tokenize input
        auto toks = lex(src);

        // 1) Detect CIAM directives: call CIAM[on|off]
        bool enabled = false;
        std::vector<std::pair<size_t, size_t>> directiveRanges; // [start, end)
        for (size_t i = 0; i < toks.size(); ++i) {
            size_t a = nextNonWs(toks, i);
            if (a >= toks.size() || !eqWord(toks[a], "call")) continue;
            size_t b = nextNonWs(toks, a + 1);
            if (b >= toks.size() || !eqWord(toks[b], "CIAM")) continue;
            size_t lbr = nextNonWs(toks, b + 1);
            if (lbr >= toks.size() || !eqSym(toks[lbr], '[')) continue;
            size_t arg = nextNonWs(toks, lbr + 1);
            if (arg >= toks.size() || toks[arg].kind != K::Word) continue;
            size_t rbr = nextNonWs(toks, arg + 1);
            if (rbr >= toks.size() || !eqSym(toks[rbr], ']')) continue;

            // Found directive
            if (toks[arg].text == "on") enabled = true;
            if (toks[arg].text == "off") enabled = false;

            // Mark range to remove (trim leading inline whitespace)
            size_t start = a;
            while (start > 0 && toks[start - 1].kind == K::Whitespace &&
                toks[start - 1].text.find('\n') == std::string::npos) {
                --start;
            }
            size_t end = rbr + 1;
            directiveRanges.emplace_back(start, end);

            i = rbr;
        }

        // If never enabled, return the input unchanged (including directives)
        if (!enabled) return src;

        // 2) Remove directives from token stream
        std::sort(directiveRanges.begin(), directiveRanges.end(),
            [](const std::pair<size_t, size_t>& x, const std::pair<size_t, size_t>& y) {
                return x.first > y.first;
            });
        for (auto& rg : directiveRanges) eraseRange(toks, rg.first, rg.second);

        // 3) Learn function names declared as: Fn <name> { ... }
        std::unordered_set<std::string> fnNames;
        for (size_t i = 0; i < toks.size(); ++i) {
            size_t a = nextNonWs(toks, i);
            if (a >= toks.size() || !eqWord(toks[a], "Fn")) continue;
            size_t nameIdx = nextNonWs(toks, a + 1);
            if (nameIdx >= toks.size() || toks[nameIdx].kind != K::Word) continue;
            fnNames.insert(toks[nameIdx].text);
            i = nameIdx;
        }

        // --- New features ---
        // Overlay registry (names declared via CIAM overlay[Name])
        std::unordered_set<std::string> overlaysActive;
        int sandboxCounter = 1;

        // 4) Pre-transform CIAM commands:
        // - CIAM write_stdout { ... }   => Print "..." (content is textual concat of block)
        // - CIAM overlay[Name]          => line comment annotation (and record overlay)
        // - CIAM inspect[Fns|symbols|overlays] => Print "info..."
        // - CIAM sandbox { ... }        => Fn _CIAM_sandbox_N { ... } \n call _CIAM_sandbox_N[]
        // - CIAM audit[]                => Print "capabilities..."
        for (size_t i = 0; i < toks.size(); ++i) {
            size_t a = nextNonWs(toks, i);
            if (a >= toks.size() || !eqWord(toks[a], "CIAM")) continue;
            size_t cmd = nextNonWs(toks, a + 1);
            if (cmd >= toks.size() || toks[cmd].kind != K::Word) continue;
            const std::string cmdName = toLower(toks[cmd].text);

            if (cmdName == "write_stdout") {
                size_t open = nextNonWs(toks, cmd + 1);
                if (open >= toks.size() || !eqSym(toks[open], '{')) continue;
                size_t close = findMatchingBrace(toks, open);
                if (close == toks.size()) continue;

                // Build body text (use unquoted content for strings, keep whitespace/symbols as-is)
                std::string body;
                for (size_t k = open + 1; k < close; ++k) {
                    const auto& tk = toks[k];
                    if (tk.kind == K::String) body += tk.text;
                    else body += tk.text;
                }

                // Replace [a..close] with: Print "body"
                eraseRange(toks, a, close + 1);
                size_t ins = a;
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Word, "Print" });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Whitespace, " " });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::String, body });
                i = ins;
                continue;
            }

            if (cmdName == "overlay") {
                size_t lbr = nextNonWs(toks, cmd + 1);
                if (lbr >= toks.size() || !eqSym(toks[lbr], '[')) continue;
                size_t nameIdx = nextNonWs(toks, lbr + 1);
                size_t rbr = nextNonWs(toks, nameIdx + 1);
                if (nameIdx >= toks.size() || toks[nameIdx].kind != K::Word) continue;
                if (rbr >= toks.size() || !eqSym(toks[rbr], ']')) continue;

                overlaysActive.insert(toks[nameIdx].text);
                // Replace with a comment line (no-op for the DSL)
                eraseRange(toks, a, rbr + 1);
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(a), Tok{ K::Whitespace, "// CIAM overlay: " + toks[nameIdx].text + "\n" });
                i = a;
                continue;
            }

            if (cmdName == "inspect") {
                size_t lbr = nextNonWs(toks, cmd + 1);
                if (lbr >= toks.size() || !eqSym(toks[lbr], '[')) continue;
                size_t arg = nextNonWs(toks, lbr + 1);
                size_t rbr = nextNonWs(toks, arg + 1);
                if (arg >= toks.size() || toks[arg].kind != K::Word) continue;
                if (rbr >= toks.size() || !eqSym(toks[rbr], ']')) continue;

                std::string what = toLower(toks[arg].text);
                std::string info;
                if (what == "fns" || what == "funcs" || what == "functions") {
                    info = "Fns: ";
                    bool first = true;
                    for (const auto& fn : fnNames) {
                        if (!first) info += ", ";
                        first = false;
                        info += fn;
                    }
                    if (first) info += "(none)";
                }
                else if (what == "symbols") {
                    info = "Symbols: fns=" + std::to_string(fnNames.size());
                }
                else if (what == "overlays") {
                    info = "Overlays: ";
                    bool first = true;
                    for (const auto& ov : overlaysActive) {
                        if (!first) info += ", ";
                        first = false;
                        info += ov;
                    }
                    if (first) info += "(none)";
                }
                else {
                    info = "Unknown inspect target: " + toks[arg].text;
                }

                eraseRange(toks, a, rbr + 1);
                size_t ins = a;
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Word, "Print" });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Whitespace, " " });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::String, info });
                i = ins;
                continue;
            }

            if (cmdName == "sandbox") {
                size_t open = nextNonWs(toks, cmd + 1);
                if (open >= toks.size() || !eqSym(toks[open], '{')) continue;
                size_t close = findMatchingBrace(toks, open);
                if (close == toks.size()) continue;

                std::string name = "_CIAM_sandbox_" + std::to_string(sandboxCounter++);

                // Replace with: Fn <name> { <body> } \n call <name> []
                std::vector<Tok> rep;
                rep.push_back(Tok{ K::Word, "Fn" });
                rep.push_back(Tok{ K::Whitespace, " " });
                rep.push_back(Tok{ K::Word, name });
                rep.push_back(Tok{ K::Whitespace, " " });
                rep.push_back(Tok{ K::Symbol, "{" });
                // body tokens
                for (size_t k = open + 1; k < close; ++k) rep.push_back(toks[k]);
                rep.push_back(Tok{ K::Symbol, "}" });
                rep.push_back(Tok{ K::Whitespace, "\n" });
                rep.push_back(Tok{ K::Word, "call" });
                rep.push_back(Tok{ K::Whitespace, " " });
                rep.push_back(Tok{ K::Word, name });
                rep.push_back(Tok{ K::Whitespace, " " });
                rep.push_back(Tok{ K::Symbol, "[" });
                rep.push_back(Tok{ K::Symbol, "]" });

                eraseRange(toks, a, close + 1);
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(a), rep.begin(), rep.end());
                i = a + rep.size();
                continue;
            }

            if (cmdName == "audit") {
                // Build capability report
                std::string info = "CIAM capabilities: typo_correction, print_inference, macro_abstraction, write_stdout, inspect, overlay, sandbox, base12";
                if (!fnNames.empty()) info += " | fns=" + std::to_string(fnNames.size());
                if (!overlaysActive.empty()) {
                    info += " | overlays=";
                    bool first = true;
                    for (const auto& ov : overlaysActive) {
                        if (!first) info += ",";
                        first = false;
                        info += ov;
                    }
                }
                eraseRange(toks, a, a + 2); // at least "CIAM audit"; allow missing [] to be optional
                // If next tokens are [ ], remove them too
                size_t lbr = nextNonWs(toks, a);
                if (lbr < toks.size() && eqSym(toks[lbr], '[')) {
                    size_t rbr = nextNonWs(toks, lbr + 1);
                    if (rbr < toks.size() && eqSym(toks[rbr], ']')) eraseRange(toks, lbr, rbr + 1);
                }
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(a), Tok{ K::Word, "Print" });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(a + 1), Tok{ K::Whitespace, " " });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(a + 2), Tok{ K::String, info });
                i = a + 3;
                continue;
            }
        }

        // 5) Base-12 numerics: convert tokens like [0-9aAbB]+ (starting with digit, containing a/b) to decimal
        for (auto& tk : toks) {
            if (tk.kind == K::Word && isBase12Word(tk.text)) {
                std::string dec;
                if (base12ToDecimal(tk.text, dec)) {
                    tk.text = dec; // Will be lexed as NUMBER by the downstream tokenizer
                }
            }
        }

        // 6) Scan occurrences of Print and call; collect stats and last seen string
        std::unordered_map<std::string, int> printFreq;
        std::vector<PrintOccur> printOccs;
        std::vector<CallOccur> callOccs;
        std::string lastString;

        for (size_t i = 0; i < toks.size(); ++i) {
            if (toks[i].kind == K::String) lastString = toks[i].text;

            if (eqWord(toks[i], "Print")) {
                size_t s = nextNonWs(toks, i + 1);
                if (s < toks.size() && toks[s].kind == K::String) {
                    printFreq[toks[s].text] += 1;
                    printOccs.push_back(PrintOccur{ i, s, toks[s].text });
                }
                else {
                    printOccs.push_back(PrintOccur{ i, static_cast<size_t>(-1), "" });
                }
            }
            else if (eqWord(toks[i], "call")) {
                size_t nidx = nextNonWs(toks, i + 1);
                if (nidx < toks.size() && toks[nidx].kind == K::Word) {
                    callOccs.push_back(CallOccur{ i, nidx });
                }
            }
        }

        // 7) Fix ambiguous Print with no argument by inferring content
        for (auto& po : printOccs) {
            if (po.idxString != static_cast<size_t>(-1)) continue;
            const std::string inferred = !lastString.empty()
                ? lastString
                : std::string("[CIAM] Inferred print content (no argument provided)");

            size_t insertAt = nextNonWs(toks, po.idxPrint + 1);
            if (insertAt < toks.size() && toks[insertAt].kind != K::Whitespace) {
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(insertAt), Tok{ K::Whitespace, " " });
                ++insertAt;
            }
            toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(insertAt), Tok{ K::String, inferred });
            printFreq[inferred] += 1;
        }

        // 8) Build macros for repeated Print literals (frequency >= 2)
        struct MacroInfo { std::string name; std::string literal; };
        std::vector<MacroInfo> macros;
        int macroCounter = 1;
        for (const auto& kv : printFreq) {
            if (kv.second >= 2) {
                MacroInfo mi;
                mi.literal = kv.first;
                mi.name = "_CIAM_Print_" + std::to_string(macroCounter++);
                macros.push_back(mi);
            }
        }

        // 9) Replace repeated Prints with call MacroName[] and inject macro Fns
        if (!macros.empty()) {
            std::unordered_map<std::string, std::string> lit2macro;
            for (const auto& m : macros) lit2macro[m.literal] = m.name;

            for (size_t i = 0; i < toks.size(); ++i) {
                if (!eqWord(toks[i], "Print")) continue;
                size_t s = nextNonWs(toks, i + 1);
                if (s >= toks.size() || toks[s].kind != K::String) continue;
                auto it = lit2macro.find(toks[s].text);
                if (it == lit2macro.end()) continue;

                // Preserve whitespace between Print and its argument
                std::vector<Tok> between;
                for (size_t k = i + 1; k < s; ++k) between.push_back(toks[k]);

                // Remove "Print <...> "literal""
                eraseRange(toks, i, s + 1);

                // Insert "call [between] MacroName []"
                size_t ins = i;
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Word, "call" });
                for (const auto& b : between) {
                    toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), b);
                }
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Word, it->second });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Whitespace, " " });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Symbol, "[" });
                toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(ins++), Tok{ K::Symbol, "]" });

                i = ins;
            }

            // Prepend Fn definitions for macros after leading whitespace
            std::vector<Tok> defs;
            for (const auto& m : macros) {
                // Fn <name> { Print "<literal>" }
                defs.push_back(Tok{ K::Word, "Fn" });
                defs.push_back(Tok{ K::Whitespace, " " });
                defs.push_back(Tok{ K::Word, m.name });
                defs.push_back(Tok{ K::Whitespace, " " });
                defs.push_back(Tok{ K::Symbol, "{" });
                defs.push_back(Tok{ K::Whitespace, "\n  " });
                defs.push_back(Tok{ K::Word, "Print" });
                defs.push_back(Tok{ K::Whitespace, " " });
                defs.push_back(Tok{ K::String, m.literal });
                defs.push_back(Tok{ K::Whitespace, "\n" });
                defs.push_back(Tok{ K::Symbol, "}" });
                defs.push_back(Tok{ K::Whitespace, "\n\n" });
            }

            size_t head = 0;
            while (head < toks.size() && toks[head].kind == K::Whitespace) ++head;
            toks.insert(toks.begin() + static_cast<std::ptrdiff_t>(head), defs.begin(), defs.end());
        }

        // 10) Correct near-miss call targets using learned Fn names (edit distance ? 2)
        if (!fnNames.empty()) {
            for (const auto& co : callOccs) {
                if (co.idxName >= toks.size() || toks[co.idxName].kind != K::Word) continue;
                const std::string name = toks[co.idxName].text;
                if (fnNames.find(name) != fnNames.end()) continue;

                int bestDist = std::numeric_limits<int>::max();
                std::string bestName;
                for (const auto& fn : fnNames) {
                    int d = editDistance(name, fn);
                    if (d < bestDist) {
                        bestDist = d;
                        bestName = fn;
                        if (bestDist == 0) break;
                    }
                }
                if (bestDist <= 2 && !bestName.empty()) {
                    toks[co.idxName].text = bestName;
                }
            }
        }

        return toText(toks);
    }

} // namespace ciam
