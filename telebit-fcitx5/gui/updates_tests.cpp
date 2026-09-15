// Tests for the two decisions in the release check that can be made without a
// network: is this tag newer than what is installed, and what is the tag.
//
// check() itself is not tested here — it reaches GitHub and interrogates the
// package databases, so what it returns is a property of the machine, not of
// the code. What is testable is exactly where the bugs would be: a version
// comparison that is really a string comparison, and a JSON field read by hand.

// Release is the default build type, and NDEBUG would compile every assert
// away — the suite would then pass without testing anything.
#undef NDEBUG

#include <cassert>
#include <iostream>
#include <string>

#include "updates.h"

using namespace telebit::setup::update;

namespace {

int checks_run = 0;

void check(bool condition, const char *what) {
    ++checks_run;
    if (!condition) {
        std::cerr << "FAILED: " << what << "\n";
    }
    assert(condition);
}

void test_ordering_is_numeric_not_lexical() {
    // The bug this test exists for: every string comparison puts "2.9.0" after
    // "2.10.0", so a machine on 2.9.0 would be told it is up to date for as
    // long as the minor number stayed in double digits.
    check(is_newer("2.10.0", "2.9.0"), "10 is a later minor than 9");
    check(!is_newer("2.9.0", "2.10.0"), "and not the other way round");
    check(is_newer("2.14.1", "2.14.0"), "patch releases count");
    check(is_newer("3.0.0", "2.99.99"), "so does the major");
}

void test_same_version_is_not_newer() {
    check(!is_newer("2.14.0", "2.14.0"), "an identical version is not an update");
    check(!is_newer("2.14", "2.14.0"), "a missing component reads as zero, not as older");
    check(!is_newer("2.14.0", "2.14"), "in both directions");
}

void test_packaging_suffixes_are_not_part_of_the_number() {
    // The installed version carries a suite suffix on Debian builds
    // (2.14.0+noble) and a rewritten one on RPM (2.14.0~noble). Neither is a
    // version component, and comparing them as text made a machine running
    // 2.14.0+noble look older than the 2.14.0 tag it was built from.
    check(!is_newer("2.14.0", "2.14.0+noble"), "a suffix does not make the tag newer");
    check(!is_newer("2.14.0", "2.14.0~noble"), "nor does the RPM spelling");
    check(is_newer("2.15.0", "2.14.0+noble"), "but a real release still is");
}

void test_garbage_is_never_an_update() {
    // Offering an upgrade on the strength of an unparseable answer is the one
    // failure mode that costs the user something.
    check(!is_newer("", "2.14.0"), "an empty candidate is not an update");
    check(!is_newer("not-a-version", "2.14.0"), "nor is a word");
}

void test_tag_is_read_from_the_release_payload() {
    const std::string json =
        R"({"url":"https://api.github.com/repos/sonnam0904/telebit/releases/1",)"
        R"("tag_name":"v2.15.0","name":"2.15.0","draft":false})";
    check(tag_from_release_json(json) == "2.15.0",
          "the tag is taken and its leading v dropped");
}

void test_tag_without_a_v_prefix() {
    check(tag_from_release_json(R"({"tag_name":"2.15.0"})") == "2.15.0",
          "a tag published without the v still reads");
}

void test_missing_or_malformed_payload() {
    // A rate-limit reply is valid JSON with no tag_name in it, and an empty
    // result is what makes the row say "chưa kiểm tra được" instead of
    // claiming the installed version is current.
    check(tag_from_release_json(R"({"message":"API rate limit exceeded"})").empty(),
          "an error payload yields no tag");
    check(tag_from_release_json("").empty(), "neither does an empty body");
    check(tag_from_release_json(R"({"tag_name":)").empty(), "nor a truncated one");
}

void test_current_version_is_a_bare_number() {
    // Whatever the packaging appended, what is compared and displayed is the
    // MAJOR.MINOR.PATCH core.
    const std::string current = current_version();
    check(!current.empty(), "the compiled-in version is not empty");
    for (char ch : current) {
        check((ch >= '0' && ch <= '9') || ch == '.',
              "and carries no packaging suffix into the comparison");
    }
}

}  // namespace

int main() {
    test_ordering_is_numeric_not_lexical();
    test_same_version_is_not_newer();
    test_packaging_suffixes_are_not_part_of_the_number();
    test_garbage_is_never_an_update();
    test_tag_is_read_from_the_release_payload();
    test_tag_without_a_v_prefix();
    test_missing_or_malformed_payload();
    test_current_version_is_a_bare_number();

    std::cout << "updates: " << checks_run << " checks passed\n";
    return 0;
}
