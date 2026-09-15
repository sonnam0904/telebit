// Tests for how an input context's program name is interpreted: which names get
// preedit mode without the user asking, and what the configtool calls them.
//
// Both questions shipped wrong. Preedit was decided by a four-entry list of
// literal browser names, so a PWA — a browser window under a different name —
// was handed direct commit and corrupted words. And the list displayed the
// program name, so the row for Zalo read "ffpwa-01arz3ndektsv4rrffq69g5fav"
// and the user concluded PWAs were missing from it entirely.
//
// The desktop-entry cases build their own directory rather than reading the
// machine's: a suite that passes only on a computer with Firefox installed is
// testing the computer.

// Release is the default build type, and NDEBUG would compile every assert
// away — the suite would then pass without testing anything. Same guard as the
// engine suite in tests.cpp at the repository root.
#undef NDEBUG

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "app_identity.h"

namespace fs = std::filesystem;
using namespace telebit;

namespace {

int checks_run = 0;

void check(bool condition, const char *what) {
    ++checks_run;
    if (!condition) {
        std::cerr << "FAILED: " << what << "\n";
    }
    assert(condition);
}

// ---------------------------------------------------------------------------
// Which programs get preedit mode by default
// ---------------------------------------------------------------------------

void test_browsers_are_recognised() {
    check(isDefaultPreeditProgram("firefox"), "the original four still match");
    check(isDefaultPreeditProgram("chrome"), "chrome, as the GTK module reports it");
    check(isDefaultPreeditProgram("google-chrome"), "and as WM_CLASS reports it under X11");
    check(isDefaultPreeditProgram("chromium"), "chromium");
}

void test_browser_name_variants() {
    // Every one of these was a browser typing through direct commit because its
    // name was not the canonical one. firefox-bin is what fcitx sees where the
    // launcher is a shell script — including PWAsForFirefox's bundled runtime.
    check(isDefaultPreeditProgram("firefox-bin"), "firefox behind a launcher script");
    check(isDefaultPreeditProgram("firefox-esr"), "the ESR package");
    check(isDefaultPreeditProgram("msedge"), "Edge is Blink and has the same defect");
    check(isDefaultPreeditProgram("chromium-browser"), "Debian's chromium binary name");
    check(isDefaultPreeditProgram("brave-browser"), "Brave");
    check(isDefaultPreeditProgram("vivaldi-bin"), "Vivaldi");
}

void test_pwa_windows_are_recognised() {
    // The bug this whole file exists for: a PWA window carries neither the
    // browser's name nor the site's, so no list of names could ever match it.
    check(isPwaProgram("ffpwa-01arz3ndektsv4rrffq69g5fav"),
          "PWAsForFirefox names the window after the install id");
    check(isDefaultPreeditProgram("ffpwa-01arz3ndektsv4rrffq69g5fav"),
          "and it is Gecko, so it needs preedit exactly as Firefox does");
    check(isDefaultPreeditProgram("chrome-abcdefghijklmnopabcdefghijklmnop-default"),
          "Chromium's app windows are named chrome-<appid>-Default");
    check(isDefaultPreeditProgram("msedge-abcdefghijklmnopabcdefghijklmnop-default"),
          "Edge uses its own binary name for the same scheme");
    check(isDefaultPreeditProgram("crx_abcdefghijklmnopabcdefghijklmnop"),
          "and older Chrome app windows use crx_<appid>");
}

void test_non_browsers_are_left_alone() {
    // The default must stay off for everything else: direct commit is the fast
    // path, and turning it off for an application that handles surrounding text
    // correctly costs that application its rollback behaviour for nothing.
    check(!isDefaultPreeditProgram("cursor"), "an Electron editor is not a browser window");
    check(!isDefaultPreeditProgram("telegram"), "nor is Telegram");
    check(!isDefaultPreeditProgram("gnome-terminal-server"), "nor a terminal");
    check(!isDefaultPreeditProgram(""), "and an unnamed client decides nothing here");

    // firefoxpwa is the *launcher* binary: it execs the runtime and exits, so it
    // never owns an input context. Users who found the PWA missing from the list
    // added this name by hand, and it could never have matched anything.
    check(!isPwaProgram("firefoxpwa"), "the launcher binary is not a PWA window");
}

// ---------------------------------------------------------------------------
// Reading a name out of a desktop entry
// ---------------------------------------------------------------------------

void test_name_is_read_from_the_desktop_entry_group() {
    const std::string entry =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Zalo\n"
        "Exec=/usr/bin/firefoxpwa site launch 01ARZ3NDEKTSV4RRFFQ69G5FAV\n";
    check(desktopEntryNameFromContents(entry) == "Zalo", "the plain Name= is taken");
}

void test_localised_names_are_skipped() {
    // Name[vi] before Name is the normal layout in translated entries. Matching
    // on a prefix would have labelled rows in whichever language sorted first.
    const std::string entry =
        "[Desktop Entry]\n"
        "Name[vi]=Tệp\n"
        "Name[de]=Dateien\n"
        "Name=Files\n";
    check(desktopEntryNameFromContents(entry) == "Files",
          "only the unlocalised key is a name this addon can use");
}

void test_action_groups_do_not_supply_the_name() {
    // [Desktop Action *] groups carry a Name of their own naming a menu item.
    // Reading on past the first group would label Firefox "New Private Window".
    const std::string entry =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "[Desktop Action new-private-window]\n"
        "Name=New Private Window\n";
    check(desktopEntryNameFromContents(entry).empty(),
          "an entry with no Name of its own yields nothing, not the action's");
}

void test_comments_and_blank_lines() {
    const std::string entry =
        "# generated by firefoxpwa\n"
        "\n"
        "[Desktop Entry]\n"
        "  Name = draw.io  \n";
    check(desktopEntryNameFromContents(entry) == "draw.io",
          "whitespace around key and value is not part of either");
}

// ---------------------------------------------------------------------------
// Finding that entry from a program name
// ---------------------------------------------------------------------------

fs::path make_fixture_dir() {
    const fs::path dir = fs::temp_directory_path() / "telebit-app-identity-tests";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    check(!ec, "the fixture directory can be created");

    // Spelled exactly as PWAsForFirefox writes it: upper-case ULID, while the
    // program name fcitx reports is lower-cased before it is stored. The ULID
    // itself is the one from the ULID spec's own examples — nothing here depends
    // on its value, only on its shape, and a real install id copied off someone's
    // machine would suggest otherwise.
    std::ofstream(dir / "FFPWA-01ARZ3NDEKTSV4RRFFQ69G5FAV.desktop")
        << "[Desktop Entry]\nType=Application\nName=Zalo\n";
    std::ofstream(dir / "org.gnome.Nautilus.desktop")
        << "[Desktop Entry]\nType=Application\nName=Files\n";
    std::ofstream(dir / "nameless.desktop") << "[Desktop Entry]\nType=Application\n";
    return dir;
}

void test_entry_is_found_case_insensitively() {
    const fs::path dir = make_fixture_dir();
    const std::vector<std::string> dirs{dir.string()};

    check(desktopEntryName("ffpwa-01arz3ndektsv4rrffq69g5fav", dirs) == "Zalo",
          "the lower-cased rule finds the upper-case file it came from");
    check(desktopEntryName("org.gnome.nautilus", dirs) == "Files",
          "and reverse-DNS entry names resolve the same way");

    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_missing_or_nameless_entries_resolve_to_nothing() {
    const fs::path dir = make_fixture_dir();
    const std::vector<std::string> dirs{dir.string()};

    check(desktopEntryName("cursor", dirs).empty(), "no entry, no name");
    check(desktopEntryName("nameless", dirs).empty(),
          "an entry without Name= is not a name either");
    check(desktopEntryName("", dirs).empty(), "an empty program matches nothing");
    check(desktopEntryName("cursor", {"/nonexistent/applications"}).empty(),
          "a directory that is not there is a miss, not a crash");

    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_label_falls_back_to_the_program_name() {
    const fs::path dir = make_fixture_dir();
    const std::vector<std::string> dirs{dir.string()};

    check(displayLabelFor("ffpwa-01arz3ndektsv4rrffq69g5fav", dirs) == "Zalo",
          "this is the whole point: the row says Zalo, not the install id");
    // Never blank. The list annotation displays this field, and an empty one
    // would leave the user with an unlabelled row — strictly worse than the raw
    // program name it replaced.
    check(displayLabelFor("cursor", dirs) == "cursor",
          "an application with no desktop entry keeps the name it had");

    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_xdg_dirs_are_listed_user_first() {
    // A PWA is only ever installed into the user's own data dir, so a system
    // entry of the same name must not be able to shadow it.
    setenv("XDG_DATA_HOME", "/home/tester/.local/share", 1);
    setenv("XDG_DATA_DIRS", "/usr/local/share:/usr/share", 1);
    const std::vector<std::string> dirs = desktopEntryDirs();
    check(dirs.size() == 3, "one applications/ per data dir");
    check(dirs[0] == "/home/tester/.local/share/applications", "the user's dir comes first");
    check(dirs[2] == "/usr/share/applications", "followed by the system ones, in order");

    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_DATA_DIRS");
    setenv("HOME", "/home/tester", 1);
    const std::vector<std::string> fallback = desktopEntryDirs();
    check(fallback[0] == "/home/tester/.local/share/applications",
          "XDG_DATA_HOME unset falls back to the spec's default, not to nothing");
    check(fallback.size() == 3, "as does XDG_DATA_DIRS");
}

}  // namespace

int main() {
    test_browsers_are_recognised();
    test_browser_name_variants();
    test_pwa_windows_are_recognised();
    test_non_browsers_are_left_alone();

    test_name_is_read_from_the_desktop_entry_group();
    test_localised_names_are_skipped();
    test_action_groups_do_not_supply_the_name();
    test_comments_and_blank_lines();

    test_entry_is_found_case_insensitively();
    test_missing_or_nameless_entries_resolve_to_nothing();
    test_label_falls_back_to_the_program_name();
    test_xdg_dirs_are_listed_user_first();

    std::cout << "app identity: " << checks_run << " checks passed\n";
    return 0;
}
