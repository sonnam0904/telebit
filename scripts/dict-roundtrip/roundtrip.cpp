// Round-trip check for the Telex core against a Vietnamese dictionary.
//
// Reads "<expected syllable>\t<telex keys>" lines produced by to_telex.py and
// reports how many syllables survive Unicode -> keystrokes -> Unicode.
//
// stdout carries the mismatches, one "<expected>\t<keys>\t<got>" per line.
// stderr carries a single machine-readable "total=N failed=N" line: run.sh
// parses it and report.py owns everything a human reads, so the numbers in the
// tables cannot drift from the numbers here.

#include "vietnamese.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: roundtrip <pairs.tsv> [modern] [max-failures]\n");
        return 2;
    }

    TelexOptions opts;
    opts.modernTone = (argc > 2 && std::string(argv[2]) == "modern");
    const long budget = (argc > 3) ? std::strtol(argv[3], nullptr, 10) : -1;

    std::ifstream f(argv[1]);
    if (!f) {
        std::fprintf(stderr, "roundtrip: cannot open %s\n", argv[1]);
        return 2;
    }

    std::string line;
    long total = 0, failed = 0;
    while (std::getline(f, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string expected = line.substr(0, tab);
        const std::string keys = line.substr(tab + 1);
        ++total;

        const std::string got = telex_to_unicode(keys, opts);
        if (got == expected) continue;
        ++failed;
        std::cout << expected << '\t' << keys << '\t' << got << '\n';
    }

    std::fprintf(stderr, "total=%ld failed=%ld\n", total, failed);

    if (budget >= 0 && failed > budget) {
        std::fprintf(stderr, "roundtrip: %ld failures exceed the budget of %ld\n", failed, budget);
        return 1;
    }
    return 0;
}
