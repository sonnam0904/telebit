#include "app_identity.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

namespace telebit {

namespace {

std::string toLower(std::string s) {
    for (auto &ch : s) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return s;
}

bool startsWith(const std::string &s, const char *prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string trim(const std::string &s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace

// A PWA window is named after the *installation* — not after the browser that
// draws it, and not after the site it shows. PWAsForFirefox launches its runtime
// with `--name FFPWA-<ULID>`, and the Chromium family uses
// `chrome-<appid>-Default` (msedge-…, brave-… for the rebrands). The window is
// still Gecko or Blink underneath, so it carries exactly the unreliable
// deleteSurroundingText that isDefaultPreeditProgram() exists to route around —
// but under a name no fixed list could ever contain, which is why this is a
// prefix test rather than more entries in kPrograms below.
//
// The same prefixes double as the variant test for the browsers themselves
// (chromium-browser, brave-browser, vivaldi-bin). Matching those here is right
// for the same reason, so they are not repeated in the exact list.
bool isPwaProgram(const std::string &programLower) {
    for (const char *prefix : {"ffpwa-", "chrome-", "chromium-", "msedge-",
                               "microsoft-edge-", "brave-", "vivaldi-", "crx_"}) {
        if (startsWith(programLower, prefix)) {
            return true;
        }
    }
    return false;
}

// Programs that need preedit mode instead of direct commit: their text fields
// report SurroundingText support but handle deleteSurroundingText unreliably, so
// rewriting a word in place corrupts it. Browsers are the offenders, both Gecko
// and Blink — the exact name fcitx reports varies by frontend and by build
// ("chrome" via the GTK/Wayland module, "google-chrome" from WM_CLASS under X11,
// "firefox-bin" where the launcher is a shell script), so the variants are
// listed rather than guessed at.
//
// Used for BOTH paths: the fresh-install default in the header, and
// recordSeenProgram(), which is what an existing config file goes through —
// there the entry is created (enabled) the first time the program is focused.
// Once the entry exists the user's own choice wins forever after.
bool isDefaultPreeditProgram(const std::string &programLower) {
    static const std::unordered_set<std::string> kPrograms{
        // Gecko
        "firefox", "firefox-bin", "firefox-esr", "firefox-developer-edition",
        "librewolf", "waterfox",
        // Blink
        "chrome", "google-chrome", "google-chrome-stable", "chromium",
        "msedge", "microsoft-edge", "brave", "vivaldi", "opera"};
    return kPrograms.count(programLower) != 0 || isPwaProgram(programLower);
}

std::vector<std::string> desktopEntryDirs() {
    std::vector<std::string> dirs;

    // XDG order puts the user's own entries first, which is also what a PWA
    // needs: nothing installs one system-wide.
    if (const char *dataHome = std::getenv("XDG_DATA_HOME");
        dataHome && dataHome[0] != '\0') {
        dirs.push_back(std::string(dataHome) + "/applications");
    } else if (const char *home = std::getenv("HOME"); home && home[0] != '\0') {
        dirs.push_back(std::string(home) + "/.local/share/applications");
    }

    std::string dataDirs = "/usr/local/share:/usr/share";
    if (const char *v = std::getenv("XDG_DATA_DIRS"); v && v[0] != '\0') {
        dataDirs = v;
    }
    std::istringstream in(dataDirs);
    std::string dir;
    while (std::getline(in, dir, ':')) {
        if (!dir.empty()) {
            dirs.push_back(dir + "/applications");
        }
    }
    return dirs;
}

std::string desktopEntryNameFromContents(const std::string &contents) {
    std::istringstream in(contents);
    std::string raw;
    bool inEntry = false;

    while (std::getline(in, raw)) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.front() == '[') {
            // Leaving [Desktop Entry] ends the group we care about: the
            // [Desktop Action *] groups that follow carry a Name of their own,
            // and it names a menu item ("Open a New Window"), not the program.
            if (inEntry) {
                break;
            }
            inEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inEntry) {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        // Exact "Name" only: the localised forms (Name[vi]) are skipped because
        // the addon has no locale of its own to pick between them, and falling
        // into whichever came first in the file would label the row in a
        // language chosen by chance.
        if (trim(line.substr(0, equals)) != "Name") {
            continue;
        }
        return trim(line.substr(equals + 1));
    }
    return std::string();
}

// The file name is the only reliable way back from a program name to a desktop
// entry. Exec= cannot do it: it is a command line with wrapper prefixes and
// %-codes, and for a PWA it has no relationship to the window at all — the entry
// reads `Exec=firefoxpwa site launch <ULID>` while the window that appears is
// named FFPWA-<ULID> and the firefoxpwa process is already gone.
//
// Matched case-insensitively because the two spellings genuinely differ: the
// stored rule is lower-cased (so that matching an input context is
// case-insensitive), while the file on disk keeps the upper-case ULID.
std::string desktopEntryName(const std::string &programLower,
                             const std::vector<std::string> &dirs) {
    if (programLower.empty()) {
        return std::string();
    }
    const std::string wanted = programLower + ".desktop";

    for (const auto &dir : dirs) {
        // Non-throwing throughout: this runs inside the input method, and a
        // directory that is absent, unreadable, or removed mid-scan is a normal
        // state of an XDG data dir — not something to propagate out of a
        // cosmetic lookup.
        std::error_code ec;
        fs::directory_iterator it(dir, ec);
        const fs::directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            if (toLower(it->path().filename().string()) != wanted) {
                continue;
            }
            std::ifstream file(it->path());
            if (!file) {
                break;
            }
            std::ostringstream buf;
            buf << file.rdbuf();
            const std::string name = desktopEntryNameFromContents(buf.str());
            if (!name.empty()) {
                return name;
            }
            // The entry exists but carries no Name=. Nothing better will turn up
            // further down the same directory, so move on to the next one, where
            // a system-wide copy may still be complete.
            break;
        }
    }
    return std::string();
}

std::string displayLabelFor(const std::string &programLower,
                            const std::vector<std::string> &dirs) {
    const std::string name = desktopEntryName(programLower, dirs);
    return name.empty() ? programLower : name;
}

}  // namespace telebit
