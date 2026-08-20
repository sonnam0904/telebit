// English passthrough check for the Telex core.
//
// The other half of the dictionary story. Round-tripping Vietnamese proves the
// engine converts what it should; this proves it leaves alone what it should
// not. Every word in an English list is fed through telex_to_unicode() and must
// come back byte-identical — spellCheckRestore exists precisely so that "person"
// does not become "persơn".
//
// stdout carries the mangled words, one "<word>\t<got>" per line. stderr
// carries a single machine-readable "total=N mangled=N" line, parsed by run.sh;
// report.py owns everything a human reads.

#include "vietnamese.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

// Only plain lowercase words: capitals, apostrophes and digits are separate
// code paths (applyWordCase, the non-alpha literal branch) with their own tests,
// and including them would just add noise to the count this gate watches.
bool isPlainLowerWord(const std::string& w) {
    if (w.size() < 2) return false;
    for (char c : w) {
        auto uc = static_cast<unsigned char>(c);
        if (std::isalpha(uc) == 0 || std::islower(uc) == 0) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: passthrough <wordlist> [max-mangled]\n");
        return 2;
    }
    const long budget = (argc > 2) ? std::strtol(argv[2], nullptr, 10) : -1;

    std::ifstream f(argv[1]);
    if (!f) {
        std::fprintf(stderr, "passthrough: cannot open %s\n", argv[1]);
        return 2;
    }

    std::string word;
    long total = 0, mangled = 0;
    while (std::getline(f, word)) {
        if (!word.empty() && word.back() == '\r') word.pop_back();
        if (!isPlainLowerWord(word)) continue;
        ++total;

        const std::string got = telex_to_unicode(word);
        if (got == word) continue;
        ++mangled;
        std::cout << word << '\t' << got << '\n';
    }

    std::fprintf(stderr, "total=%ld mangled=%ld\n", total, mangled);

    if (budget >= 0 && mangled > budget) {
        std::fprintf(stderr, "passthrough: %ld mangled exceeds the budget of %ld\n", mangled, budget);
        return 1;
    }
    return 0;
}
