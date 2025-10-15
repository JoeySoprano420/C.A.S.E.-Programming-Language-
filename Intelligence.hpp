#pragma once
#include <string>

namespace ciam {

// CIAM source-to-source preprocessor. Safe to call unconditionally.
// If the DSL contains `call CIAM[on]`, it will:
// - repair ambiguous constructs (e.g., Print with no arg),
// - auto-alias near-miss function calls,
// - abstract repeated Print literals into generated Fn macros.
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

        // 4) Scan occurrences of Print and call; collect stats and last seen string
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

        // 5) Fix ambiguous Print with no argument by inferring content
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

        // 6) Build macros for repeated Print literals (frequency >= 2)
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

        // 7) Replace repeated Prints with call MacroName[]
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

        // 8) Correct near-miss call targets using learned Fn names (edit distance ≤ 2)
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
