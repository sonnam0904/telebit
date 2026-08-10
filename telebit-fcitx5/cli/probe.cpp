#include "probe.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace telebit::doctor {
namespace {

std::string trim(const std::string &s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> split_lines(const std::string &text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) out.push_back(line);
    return out;
}

std::vector<std::string> split_ws(const std::string &line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

// Every external command is read-only; stderr is dropped because these tools
// warn liberally (snap about refresh state, flatpak about remotes) and none of
// it is a signal doctor can act on.
struct CommandResult {
    std::string output;
    // Exit status, or -1 when the command could not be started at all. An empty
    // output means nothing on its own: a variable that is genuinely unset and a
    // command that timed out both print nothing, and only this tells them apart.
    int status = -1;
};

// A probe that never returns is worse than one that fails. Doctor is pointed at
// broken machines, and a session bus that stopped answering, a systemd user
// manager that hangs, or snapd mid-refresh are exactly those states — without a
// deadline the default `telebit doctor` run blocks forever with no output.
// Bounding every command costs one empty row instead.
constexpr int kProbeTimeoutSeconds = 10;
// A sandbox launch is a cold start of a whole runtime, so it gets its own budget.
constexpr int kSandboxTimeoutSeconds = 25;
// One ldd call covers every installed application at once, so it is allowed more
// than a single-answer probe: it does the work that used to be hundreds of them.
constexpr int kLddTimeoutSeconds = 30;

// popen already hands the string to `sh -c`, but `timeout` has to wrap the whole
// command: prefixed bare it would bound only the first stage of a pipeline.
// Quoting into a nested `sh -c` keeps one mechanism for every call site.
std::string shell_quote(const std::string &s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

CommandResult run_capturing_status(const std::string &command,
                                   int timeout_seconds = kProbeTimeoutSeconds) {
    CommandResult result;
    // GNU timeout runs the command in its own process group and signals the
    // group, so pipeline members die with it rather than surviving to hold the
    // pipe open — which would leave the read loop below blocked anyway.
    const std::string full = "timeout " + std::to_string(timeout_seconds) + " sh -c " +
                             shell_quote(command) + " 2>/dev/null";
    std::FILE *pipe = ::popen(full.c_str(), "r");
    if (pipe == nullptr) return result;
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }
    const int closed = ::pclose(pipe);
    if (closed != -1 && WIFEXITED(closed)) result.status = WEXITSTATUS(closed);
    return result;
}

std::string run(const std::string &command) {
    return run_capturing_status(command).output;
}

bool have_command(const char *name) {
    const std::string probe = std::string("command -v ") + name + " >/dev/null 2>&1";
    return std::system(probe.c_str()) == 0;
}

// Package and app identifiers reach us from `flatpak list` / `snap list`, but
// they end up inside a shell command line. Anything outside this set is not a
// real id, so refusing it is cheaper than quoting correctly.
bool is_safe_id(const std::string &id) {
    if (id.empty()) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '.' || c == '-' || c == '_' || c == '/' || c == '+';
    });
}

std::string env_or_empty(const char *name) {
    const char *value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}

std::string read_file(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool dir_exists(const fs::path &path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

// /proc/<pid>/environ is NUL-separated; this pulls one variable out of it.
std::string environ_lookup(const std::string &blob, const std::string &key) {
    const std::string needle = key + "=";
    std::size_t pos = 0;
    while (pos < blob.size()) {
        const std::size_t end = blob.find('\0', pos);
        const std::string entry = blob.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (entry.rfind(needle, 0) == 0) return entry.substr(needle.size());
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return {};
}

struct CompositorProcess {
    std::string name;
    std::string pid;
};

// The session processes worth looking for, and what each one means.
//
// `comm` must be the name as the kernel reports it in /proc/<pid>/comm, which
// is truncated to 15 characters — anything longer added here would silently
// never match.
//
// `ime_native` is the distinction the report turns on: a compositor that
// implements zwp_input_method_v2 lets fcitx5 attach directly, so the
// GTK_IM_MODULE/QT_IM_MODULE variables are unnecessary there. Mutter does not
// implement it (it drives IMEs over the ibus D-Bus protocol instead) and Weston
// has no text-input-v3 at all, so both are false.
//
// The X11 window managers at the end can never satisfy that, but they are still
// listed: without them a session started from .xinitrc — no systemd user
// manager to ask — would leave the report unable to read the environment the
// applications actually got.
//
// There is deliberately no "is this Wayland" column. It would be a lie for the
// two entries that matter most: gnome-shell and mutter run the same binary on
// X11 and on Wayland. The display server is read from the compositor's own
// environment instead, which cannot be wrong about it.
struct CompositorEntry {
    const char *comm;
    const char *display;
    bool ime_native;
};

constexpr CompositorEntry kCompositors[] = {
    // Wayland
    {"gnome-shell", "mutter", false},
    {"mutter", "mutter", false},
    {"kwin_wayland", "kwin", true},
    {"cosmic-comp", "cosmic-comp", true},
    {"sway", "sway", true},
    {"Hyprland", "Hyprland", true},
    {"niri", "niri", true},
    {"wayfire", "wayfire", true},
    {"labwc", "labwc", true},
    {"river", "river", true},
    {"dwl", "dwl", true},
    {"cage", "cage", true},
    {"phoc", "phoc", true},
    {"weston", "weston", false},
    // X11 — listed only so their environment can be read.
    {"kwin_x11", "kwin", false},
    {"xfwm4", "xfwm4", false},
    {"marco", "marco", false},
    {"muffin", "muffin", false},
    {"openbox", "openbox", false},
    {"i3", "i3", false},
};

// Walks /proc looking for the process that owns the graphical session. We need
// its pid (to read the environment children inherit) and its name (to know
// which Wayland input-method protocol is on offer).
//
// More than one candidate can be running: a nested compositor for testing, a
// leftover from a crashed session, or a second desktop under Xephyr. Taking
// whichever /proc happened to list first could describe a Wayland toy window as
// the session and then advise unsetting the IM variables on a real X11 desktop,
// which would leave the user unable to type at all. The session's own
// compositor is started before anything nested inside it, so the oldest
// candidate wins.
CompositorProcess find_compositor() {
    std::vector<CompositorCandidate> candidates;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator("/proc", ec)) {
        const std::string pid = entry.path().filename().string();
        if (pid.empty() || std::isdigit(static_cast<unsigned char>(pid[0])) == 0) continue;
        const std::string comm = trim(read_file(entry.path() / "comm"));

        bool known = false;
        for (const auto &candidate : kCompositors) {
            if (comm == candidate.comm) {
                known = true;
                break;
            }
        }
        if (!known) continue;

        // Only our own processes: /proc/<pid>/environ of another user is
        // unreadable, which doubles as the ownership test we need.
        if (read_file(entry.path() / "environ").empty()) continue;

        candidates.push_back({comm, pid, parse_proc_starttime(read_file(entry.path() / "stat"))});
    }

    const CompositorCandidate chosen = pick_compositor(candidates);
    return {chosen.comm, chosen.pid};
}

// ---------------------------------------------------------------------------
// Sandbox module detection
// ---------------------------------------------------------------------------

ModuleKind classify(const std::string &filename, bool gtk3) {
    if (filename.find("fcitx5") != std::string::npos) return ModuleKind::Fcitx5;
    // GTK3 is the only place the legacy module still turns up, and it is named
    // exactly im-fcitx.so — a substring test would also match im-fcitx5.so.
    if (gtk3 && filename == "im-fcitx.so") return ModuleKind::Fcitx4;
    if (!gtk3 && filename.find("fcitx") != std::string::npos) return ModuleKind::Fcitx4;
    return ModuleKind::None;
}

// `filename`, when given, receives the basename of the module that won. The host
// GTK3 check needs it: GTK loads a module only through the name recorded in
// immodules.cache, so knowing *which* file is installed is what tells a
// registered module apart from a cache still pointing at the previous one.
ModuleKind scan_dir(const fs::path &dir, bool gtk3, std::string *filename = nullptr) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return ModuleKind::None;
    ModuleKind best = ModuleKind::None;
    std::string best_name;
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        const std::string name = entry.path().filename().string();
        const ModuleKind kind = classify(name, gtk3);
        if (kind == ModuleKind::Fcitx5) {  // nothing outranks it
            if (filename != nullptr) *filename = name;
            return ModuleKind::Fcitx5;
        }
        if (kind == ModuleKind::Fcitx4) {
            best = ModuleKind::Fcitx4;
            best_name = name;
        }
    }
    if (filename != nullptr && best != ModuleKind::None) *filename = best_name;
    return best;
}

void merge(ModuleKind &slot, ModuleKind found) {
    if (found == ModuleKind::Fcitx5) slot = ModuleKind::Fcitx5;
    else if (found == ModuleKind::Fcitx4 && slot == ModuleKind::None) slot = ModuleKind::Fcitx4;
}

// Which toolkit a library on disk proves is installed.
enum class ToolkitId {
    Gtk3,
    Gtk4,
    Qt5,
    Qt6,
};

// The library whose presence proves each toolkit. One table for the sandbox and
// the host scanner: they used to carry a copy each, and copies of this knowledge
// drift — the Qt plugin layout list below had already diverged between them,
// which left a Flatpak runtime reported as missing a Qt plugin it shipped.
struct ToolkitSoname {
    const char *prefix;
    ToolkitId toolkit;
};

constexpr ToolkitSoname kToolkitSonames[] = {
    {"libgtk-3.so", ToolkitId::Gtk3},
    {"libgtk-4.so", ToolkitId::Gtk4},
    {"libQt5Gui.so", ToolkitId::Qt5},
    {"libQt6Gui.so", ToolkitId::Qt6},
};

// The Qt plugin layouts in the wild, and which Qt each one serves. Qt5 and Qt6
// install the *same* plugin filename, so the directory is the only thing that
// says which version a plugin belongs to: Debian and Fedora use qt5/ and qt6/,
// Arch puts Qt5 in qt/ and Qt6 in qt6/, and the bare plugins/ layout predates
// the split and is therefore Qt5.
struct QtPluginRoot {
    const char *dir;
    bool qt6;
};

constexpr QtPluginRoot kQtPluginRoots[] = {
    {"qt6/plugins", true},
    {"qt5/plugins", false},
    {"qt/plugins", false},
    {"plugins", false},
};

// Every library directory worth scanning under one root: the root itself plus one
// level down, which is where the architecture triplet (/usr/lib/x86_64-linux-gnu)
// and the per-Qt-version trees live.
std::vector<fs::path> lib_bases(const fs::path &lib_root) {
    std::vector<fs::path> bases{lib_root};
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(lib_root, ec)) {
        if (entry.is_directory(ec)) bases.push_back(entry.path());
    }
    return bases;
}

// Runs the Qt plugin table over one base, routing each hit to the Qt version the
// directory belongs to. `sink` takes (is_qt6, kind) so the caller decides where
// the answer lands — the sandbox keeps one Qt slot, the host keeps two.
template <typename Sink>
void scan_qt_plugins(const fs::path &base, const Sink &sink) {
    // A base that is itself a Qt tree already declares its version; asking the
    // table again would read <libdir>/qt6/plugins as a Qt5 directory.
    if (const auto qt6 = qt6_from_directory_name(base.filename().string())) {
        sink(*qt6, scan_dir(base / "plugins" / "platforminputcontexts", false));
        return;
    }
    for (const auto &qt : kQtPluginRoots) {
        sink(qt.qt6, scan_dir(base / qt.dir / "platforminputcontexts", false));
    }
}

// Looks for IM modules under one library root. Flatpak runtimes put them in
// files/lib/... while the Snap platform snaps use usr/lib/<triplet>/..., so the
// caller passes both candidates and we also descend one level to cover the
// architecture triplet directory.
void scan_lib_root(const fs::path &lib_root, ModuleSet &out) {
    if (!dir_exists(lib_root)) return;

    std::error_code ec;
    for (const auto &base : lib_bases(lib_root)) {
        // One listing of the library directory answers "is this toolkit even
        // here", which is what separates a missing module from a non-existent
        // problem.
        for (const auto &entry : fs::directory_iterator(base, ec)) {
            const std::string name = entry.path().filename().string();
            for (const auto &soname : kToolkitSonames) {
                if (name.rfind(soname.prefix, 0) != 0) continue;
                switch (soname.toolkit) {
                    case ToolkitId::Gtk3: out.gtk3_present = true; break;
                    case ToolkitId::Gtk4: out.gtk4_present = true; break;
                    // A runtime ships one Qt, so both versions land in one slot.
                    case ToolkitId::Qt5:
                    case ToolkitId::Qt6: out.qt_present = true; break;
                }
            }
        }

        merge(out.gtk3, scan_dir(base / "gtk-3.0" / "3.0.0" / "immodules", true));

        // GTK4 keys the directory on its own version, so the level in between
        // has to be enumerated rather than guessed.
        const fs::path gtk4 = base / "gtk-4.0";
        if (dir_exists(gtk4)) {
            for (const auto &entry : fs::directory_iterator(gtk4, ec)) {
                merge(out.gtk4, scan_dir(entry.path() / "immodules", false));
            }
        }

        scan_qt_plugins(base, [&out](bool, ModuleKind found) { merge(out.qt, found); });
    }
}

ModuleSet scan_runtime(const fs::path &root) {
    ModuleSet set;
    scan_lib_root(root / "lib", set);
    scan_lib_root(root / "usr" / "lib", set);
    return set;
}

// ---------------------------------------------------------------------------
// Host module detection
// ---------------------------------------------------------------------------

// Where a natively packaged toolkit keeps its libraries. lib64 is a *sibling* of
// lib rather than a child, so /usr/lib alone never reaches the layout every .rpm
// uses — the same trap probe_fcitx5() had to work around for the addon.
//
// Unlike the sandbox scan there is deliberately no $HOME entry: GTK and Qt look
// only in the libdir they were built against, so a module under ~/.local would
// be found by us and never loaded by them. Reporting it as present would be
// worse than not looking at all.
std::vector<fs::path> host_lib_roots() {
    return {"/usr/lib", "/usr/lib64", "/usr/local/lib", "/usr/local/lib64"};
}

// The GTK3 module/cache verdict for exactly one library directory. Deciding it
// per base is what keeps `module_file` and `other_fcitx` describing the same
// place: aggregated field-by-field they could come from two different bases and
// name the same file as both installed and stale.
Gtk3Cache gtk3_cache_for_base(const fs::path &base) {
    Gtk3Cache result;
    const fs::path gtk3 = base / "gtk-3.0" / "3.0.0";

    std::string module_file;
    const ModuleKind kind = scan_dir(gtk3 / "immodules", true, &module_file);
    if (kind == ModuleKind::None) return result;  // nothing here to register

    result.module_file = module_file;
    const std::string cache = read_file(gtk3 / "immodules.cache");
    if (cache.empty()) {
        result.state = Gtk3CacheState::NoCache;
        return result;
    }

    const CacheRegistration registration = immodules_cache_registration(cache, module_file);
    result.state =
        registration.lists_module ? Gtk3CacheState::Registered : Gtk3CacheState::NotRegistered;
    result.other_fcitx = registration.other_fcitx;
    return result;
}

// Same shape as scan_lib_root: the root itself plus one level down, which is
// where the architecture triplet directory (/usr/lib/x86_64-linux-gnu) lives.
void scan_host_root(const fs::path &lib_root, HostInfo &out) {
    std::error_code ec;
    for (const auto &base : lib_bases(lib_root)) {
        for (const auto &entry : fs::directory_iterator(base, ec)) {
            const std::string name = entry.path().filename().string();
            for (const auto &soname : kToolkitSonames) {
                if (name.rfind(soname.prefix, 0) != 0) continue;
                switch (soname.toolkit) {
                    case ToolkitId::Gtk3: out.gtk3.present = true; break;
                    case ToolkitId::Gtk4: out.gtk4.present = true; break;
                    case ToolkitId::Qt5: out.qt5.present = true; break;
                    case ToolkitId::Qt6: out.qt6.present = true; break;
                }
            }
        }

        merge(out.gtk3.module, scan_dir(base / "gtk-3.0" / "3.0.0" / "immodules", true));

        // Keep the worst pairing seen, whole. Merging its fields separately is
        // what produced a row saying the cache both does and does not register
        // one filename.
        out.gtk3_cache = worse_gtk3_cache(out.gtk3_cache, gtk3_cache_for_base(base));

        // GTK4 keys the directory on its own version, so the level in between has
        // to be enumerated rather than guessed.
        const fs::path gtk4 = base / "gtk-4.0";
        if (dir_exists(gtk4)) {
            for (const auto &entry : fs::directory_iterator(gtk4, ec)) {
                merge(out.gtk4.module, scan_dir(entry.path() / "immodules", false));
            }
        }

        scan_qt_plugins(base, [&out](bool qt6, ModuleKind found) {
            merge(qt6 ? out.qt6.module : out.qt5.module, found);
        });
    }
}

// ---------------------------------------------------------------------------
// Native applications
// ---------------------------------------------------------------------------

// Where desktop entries for natively installed applications live.
//
// XDG_DATA_DIRS is deliberately not consulted: on Ubuntu it also contains
// /var/lib/snapd/desktop and the Flatpak exports tree, and those applications
// belong to the sandbox section — their toolkit lives inside the sandbox, so
// judging them against the host's modules would be wrong twice over.
std::vector<fs::path> desktop_entry_dirs() {
    std::vector<fs::path> dirs{"/usr/share/applications", "/usr/local/share/applications"};
    const std::string home = env_or_empty("HOME");
    if (!home.empty()) dirs.push_back(fs::path(home) / ".local" / "share" / "applications");
    return dirs;
}

// The libraries that identify a toolkit, and which flag each one sets. Qt is
// matched on Widgets and Quick as well as Gui because a Qt application links
// whichever of them it uses and ldd's output is transitive — Gui alone would
// still catch most, but not all, and a missed application is a missed incident.
struct ToolkitLibrary {
    const char *soname;
    bool NativeApp::*flag;
};

constexpr ToolkitLibrary kToolkitLibraries[] = {
    {"libgtk-3.so", &NativeApp::gtk3},
    {"libgtk-4.so", &NativeApp::gtk4},
    {"libQt5Gui.so", &NativeApp::qt5},
    {"libQt5Widgets.so", &NativeApp::qt5},
    {"libQt5Quick.so", &NativeApp::qt5},
    {"libQt6Gui.so", &NativeApp::qt6},
    {"libQt6Widgets.so", &NativeApp::qt6},
    {"libQt6Quick.so", &NativeApp::qt6},
};

// Splits an Exec value the way the desktop entry spec does for our purpose:
// far enough to find the binary. Quotes are honoured because a path with a space
// is quoted there, and the field codes are dropped because "%U" is not an
// argument the binary ever sees.
std::vector<std::string> split_exec(const std::string &exec) {
    std::vector<std::string> tokens;
    std::string current;
    // Which character opened the quoted region, not merely "are we in one": a
    // single flag toggled by either quote let the apostrophe in
    // "/home/user/Bob's App/run" close the region the double quote opened, so the
    // next space ended the token and the binary came out as /home/user/Bobs.
    char quote = '\0';
    bool have = false;
    for (const char c : exec) {
        if (quote == '\0' && (c == '"' || c == '\'')) {
            quote = c;
            have = true;
            continue;
        }
        if (c == quote) {
            quote = '\0';
            continue;
        }
        if (quote == '\0' && (c == ' ' || c == '\t')) {
            if (have) tokens.push_back(current);
            current.clear();
            have = false;
            continue;
        }
        current += c;
        have = true;
    }
    if (have) tokens.push_back(current);

    std::vector<std::string> out;
    for (const auto &token : tokens) {
        if (!token.empty() && token[0] == '%') continue;
        out.push_back(token);
    }
    return out;
}

// Resolves a bare command name against PATH. Entries name their binary either
// absolutely or by name, and only the caller's PATH can tell the second apart
// from a typo.
std::string resolve_on_path(const std::string &command) {
    if (command.find('/') != std::string::npos) return command;
    std::error_code ec;
    std::istringstream path(env_or_empty("PATH"));
    std::string dir;
    while (std::getline(path, dir, ':')) {
        if (dir.empty()) continue;
        const fs::path candidate = fs::path(dir) / command;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
    return {};
}

// ---------------------------------------------------------------------------
// Flatpak overrides
// ---------------------------------------------------------------------------

// Minimal reader for the key=value files in <installation>/overrides/. Only the
// [Environment] section matters here; a full INI parser would buy nothing.
//
// Returns nullopt when the key is absent, and an empty string when it is
// present but set to nothing — which is what `--env=GTK_IM_MODULE=` writes.
std::optional<std::string> override_gtk_im_module(const fs::path &file) {
    const std::string text = read_file(file);
    if (text.empty()) return std::nullopt;
    bool in_environment = false;
    for (const auto &raw : split_lines(text)) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[') {
            in_environment = (line == "[Environment]");
            continue;
        }
        if (!in_environment) continue;
        if (line.rfind("GTK_IM_MODULE=", 0) == 0) return line.substr(std::string("GTK_IM_MODULE=").size());
    }
    return std::nullopt;
}

std::vector<fs::path> flatpak_roots() {
    std::vector<fs::path> roots{"/var/lib/flatpak"};
    const std::string home = env_or_empty("HOME");
    if (!home.empty()) roots.emplace_back(fs::path(home) / ".local" / "share" / "flatpak");
    return roots;
}

}  // namespace

void parse_bus_names(const std::string &busctl_output, Fcitx5Info &info) {
    struct BusName {
        std::string name;
        std::string pid;
        std::string process;
    };

    std::vector<BusName> acquired;
    for (const auto &line : split_lines(busctl_output)) {
        const auto columns = split_ws(line);
        if (columns.size() < 3) continue;
        if (columns[0] == "NAME" || columns[0].front() == ':') continue;  // header, unique names
        // An owner column of "-" means the name is merely activatable. Reading
        // one as a live frontend is exactly the false positive to avoid.
        if (columns[1] == "-" || columns[2] == "-") continue;
        acquired.push_back({columns[0], columns[1], columns[2]});
    }

    // Identify fcitx5 by the one name only it can own, then require every other
    // frontend to come from that same process. Matching on the process name
    // would break for a build installed under a different name; matching on the
    // pid cannot.
    std::string fcitx5_pid;
    for (const auto &entry : acquired) {
        if (entry.name == "org.fcitx.Fcitx5") {
            fcitx5_pid = entry.pid;
            info.bus_native = true;
            break;
        }
    }

    for (const auto &entry : acquired) {
        const bool wanted = entry.name == "org.freedesktop.portal.Fcitx" ||
                            entry.name.rfind("org.fcitx.Fcitx-", 0) == 0 ||
                            entry.name == "org.freedesktop.IBus";
        if (!wanted) continue;

        if (!fcitx5_pid.empty() && entry.pid == fcitx5_pid) {
            if (entry.name == "org.freedesktop.portal.Fcitx") info.bus_portal = true;
            else if (entry.name == "org.freedesktop.IBus") info.bus_ibus = true;
            else info.bus_fcitx4 = true;
        } else {
            info.foreign_bus_owners.push_back(entry.name + " (" + entry.process + ")");
        }
    }
    info.running = info.bus_native;
}

CompositorCandidate pick_compositor(const std::vector<CompositorCandidate> &candidates) {
    const CompositorCandidate *best = nullptr;
    for (const auto &candidate : candidates) {
        if (best == nullptr) {
            best = &candidate;
            continue;
        }
        // A starttime of 0 means /proc/<pid>/stat was unreadable; never let an
        // unknown age beat a known one.
        if (candidate.starttime == 0) continue;
        if (best->starttime == 0 || candidate.starttime < best->starttime) best = &candidate;
    }
    return best != nullptr ? *best : CompositorCandidate{};
}

SnapRuntimeSelection select_snap_runtimes(const SnapConnections &connections,
                                          const std::map<std::string, ModuleSet> &scanned) {
    SnapRuntimeSelection selection;

    for (const auto &name : connections.providers) {
        const auto entry = scanned.find(name);
        if (entry == scanned.end()) continue;
        const ModuleSet &modules = entry->second;
        // A content provider that carries no toolkit is not a runtime.
        // gtk-common-themes provides icons and nothing else, and counting it
        // filed every app under a snap holding no input-method module at all.
        if (!modules.gtk3_present && !modules.gtk4_present && !modules.qt_present) continue;
        selection.runtimes.push_back(name);
    }

    for (const auto &app : connections.providers_of) {
        for (const auto &provider : app.second) {
            if (std::find(selection.runtimes.begin(), selection.runtimes.end(), provider) !=
                selection.runtimes.end()) {
                selection.runtime_of_app[app.first] = provider;
                break;
            }
        }
    }

    return selection;
}

unsigned long long parse_proc_starttime(const std::string &stat_contents) {
    // Field 2 is "(comm)" and a process can be named "(weird) thing)", so the
    // only safe anchor is the final ')'.
    const auto close = stat_contents.rfind(')');
    if (close == std::string::npos) return 0;

    const auto fields = split_ws(stat_contents.substr(close + 1));
    // The first token after ')' is field 3 (state), so field 22 is index 19.
    constexpr std::size_t kStarttimeIndex = 19;
    if (fields.size() <= kStarttimeIndex) return 0;

    try {
        return std::stoull(fields[kStarttimeIndex]);
    } catch (const std::exception &) {
        return 0;
    }
}

std::string compositor_family(const std::string &comm) {
    for (const auto &entry : kCompositors) {
        if (comm == entry.comm) return entry.display;
    }
    // Something we do not know: report it verbatim rather than guessing, so the
    // user at least sees the real name in the report.
    return comm;
}

bool compositor_handles_ime_natively(const std::string &family) {
    for (const auto &entry : kCompositors) {
        if (family == entry.display) return entry.ime_native;
    }
    // Unknown compositors are assumed not to, which keeps the advice on the
    // conservative side: telling someone to unset the variables when the
    // compositor cannot in fact drive the IME would break their input.
    return false;
}

bool fill_session_from_environ(const std::string &environ_blob, SessionInfo &info) {
    if (environ_blob.empty()) return false;

    info.display_server = environ_lookup(environ_blob, "XDG_SESSION_TYPE");
    if (info.display_server.empty()) {
        // WAYLAND_DISPLAY is checked first because a Wayland session also sets
        // DISPLAY for XWayland; the reverse never happens.
        if (!environ_lookup(environ_blob, "WAYLAND_DISPLAY").empty()) {
            info.display_server = "wayland";
        } else if (!environ_lookup(environ_blob, "DISPLAY").empty()) {
            info.display_server = "x11";
        }
    }

    const std::string desktop = environ_lookup(environ_blob, "XDG_CURRENT_DESKTOP");
    if (!desktop.empty()) info.desktop = desktop;

    info.gtk_im_module = environ_lookup(environ_blob, "GTK_IM_MODULE");
    info.qt_im_module = environ_lookup(environ_blob, "QT_IM_MODULE");
    info.xmodifiers = environ_lookup(environ_blob, "XMODIFIERS");
    return true;
}

void record_deep_probe(int status, const std::string &output, SandboxApp &app) {
    // An empty output proves nothing by itself — the variable may be unset, or
    // the sandbox may have refused to start, or `timeout` may have killed it at
    // 25s (status 124). Only a clean exit means the answer is real.
    if (status != 0) {
        app.deep_failed = true;
        return;
    }

    // The answer is the marked line, never "the last non-empty line". Sandbox
    // startup prints chatter of its own, and `printf %s` on an unset variable
    // prints nothing — so positional parsing recorded that chatter as the value,
    // which suppressed the empty-variable failure and rendered a broken app as a
    // healthy row.
    const std::string marker = kDeepProbeMarker;
    bool answered = false;
    for (const auto &line : split_lines(output)) {
        const std::string candidate = trim(line);
        if (candidate.rfind(marker, 0) != 0) continue;
        app.deep_gtk_im_module = trim(candidate.substr(marker.size()));
        answered = true;
    }

    // A clean exit that never printed the marker means our script did not run —
    // a wrapper swallowed it, or the sandbox shell is not the one we assumed.
    // That is a failed measurement, not a variable we saw to be empty.
    if (!answered) {
        app.deep_failed = true;
        return;
    }
    app.deep_probed = true;
}

bool fill_session_from_systemd(const std::string &show_environment_output, SessionInfo &info) {
    for (const auto &line : split_lines(show_environment_output)) {
        const std::string entry = trim(line);
        if (entry.rfind("GTK_IM_MODULE=", 0) == 0) info.gtk_im_module = entry.substr(14);
        else if (entry.rfind("QT_IM_MODULE=", 0) == 0) info.qt_im_module = entry.substr(13);
        else if (entry.rfind("XMODIFIERS=", 0) == 0) info.xmodifiers = entry.substr(11);
    }
    return !info.gtk_im_module.empty() || !info.qt_im_module.empty();
}

SessionInfo resolve_session(const SessionSources &sources) {
    SessionInfo info;
    info.shell_gtk_im_module = sources.own_gtk_im_module;

    if (!sources.compositor_comm.empty()) {
        info.compositor = compositor_family(sources.compositor_comm);
        if (fill_session_from_environ(sources.compositor_environ, info)) {
            info.env_source =
                sources.compositor_comm + " (pid " + sources.compositor_pid + ")";
        }
    }

    // No compositor found (a bare X session, a nested one, or a name we do not
    // know): the systemd user manager's environment block is the next best
    // authority, since that is what environment.d(5) actually feeds.
    if (info.env_source.empty()) {
        if (fill_session_from_systemd(sources.systemd_environment, info)) {
            info.env_source = "systemctl --user show-environment";
        }
    }

    // Only now, having failed to find the session's own view, fall back to the
    // caller's environment.
    if (info.display_server.empty()) {
        info.display_server = sources.own_session_type;
        if (info.display_server.empty()) {
            if (!sources.own_wayland_display.empty()) info.display_server = "wayland";
            else if (!sources.own_display.empty()) info.display_server = "x11";
        }
    }
    if (info.desktop.empty()) info.desktop = sources.own_desktop;

    return info;
}

SessionInfo probe_session() {
    SessionSources sources;
    const CompositorProcess compositor = find_compositor();
    if (!compositor.pid.empty()) {
        sources.compositor_comm = compositor.name;
        sources.compositor_pid = compositor.pid;
        sources.compositor_environ = read_file(fs::path("/proc") / compositor.pid / "environ");
    }
    sources.systemd_environment = run("systemctl --user show-environment");
    sources.own_session_type = env_or_empty("XDG_SESSION_TYPE");
    sources.own_wayland_display = env_or_empty("WAYLAND_DISPLAY");
    sources.own_display = env_or_empty("DISPLAY");
    sources.own_desktop = env_or_empty("XDG_CURRENT_DESKTOP");
    sources.own_gtk_im_module = env_or_empty("GTK_IM_MODULE");
    return resolve_session(sources);
}

Fcitx5Info probe_fcitx5() {
    Fcitx5Info info;

    if (have_command("busctl")) {
        // --acquired lists only names a process actually holds. The default
        // listing also includes activatable names, whose owner column is "-",
        // and treating one of those as a live frontend would be the same false
        // positive parse_bus_names() exists to prevent.
        parse_bus_names(run("busctl --user list --no-pager --acquired"), info);
    } else {
        // No busctl (a container, a systemd-less distro). pgrep still answers
        // the one question that matters most — is fcitx5 up — and leaves the
        // frontend flags unknown rather than falsely false.
        info.bus_unavailable = true;
        info.running = run_capturing_status("pgrep -u \"$(id -u)\" -x fcitx5").status == 0;
    }

    const auto version_lines = split_lines(run("fcitx5 --version"));
    if (!version_lines.empty()) {
        const auto columns = split_ws(version_lines.front());
        if (!columns.empty()) info.version = columns.back();
    }

    // fcitx5 loads addons only from the libdir of the build it belongs to. The
    // authoritative answer is where this build installs the addon, handed down
    // from CMake; the scan afterwards is the fallback for a doctor binary that
    // came from a different build than the addon it is looking for.
    std::vector<fs::path> candidates;
    std::error_code ec;
#ifdef TELEBIT_ADDON_DIR
    candidates.emplace_back(TELEBIT_ADDON_DIR);
#endif

    // lib64 is a *sibling* of lib, not a child, so iterating /usr/lib alone
    // never reaches /usr/lib64/fcitx5 — the layout every .rpm this project ships
    // installs into, which used to make doctor call a working Fedora box broken.
    std::vector<fs::path> lib_roots = {"/usr/lib", "/usr/lib64", "/usr/local/lib",
                                       "/usr/local/lib64"};
    const std::string home = env_or_empty("HOME");
    if (!home.empty()) lib_roots.push_back(fs::path(home) / ".local" / "lib");

    for (const auto &root : lib_roots) {
        candidates.push_back(root / "fcitx5");
        for (const auto &entry : fs::directory_iterator(root, ec)) {
            if (entry.is_directory(ec)) candidates.push_back(entry.path() / "fcitx5");
        }
    }

    for (const auto &dir : candidates) {
        const fs::path so = dir / "telebit-fcitx5.so";
        if (fs::exists(so, ec)) {
            info.addon_path = so.string();
            break;
        }
    }

    return info;
}

std::optional<bool> qt6_from_directory_name(const std::string &name) {
    if (name.find("qt6") != std::string::npos) return true;
    if (name == "qt" || name.find("qt5") != std::string::npos) return false;
    return std::nullopt;
}

Gtk3Cache worse_gtk3_cache(const Gtk3Cache &a, const Gtk3Cache &b) {
    // Ordered worst-last: a problem outranks a healthy pairing, and a cache that
    // registers the wrong module outranks having no cache at all, because the
    // first is a machine that looks configured and is not.
    const auto severity = [](Gtk3CacheState state) {
        switch (state) {
            case Gtk3CacheState::NoModule: return 0;
            case Gtk3CacheState::Registered: return 1;
            case Gtk3CacheState::NoCache: return 2;
            case Gtk3CacheState::NotRegistered: return 3;
        }
        return 0;
    };
    return severity(b.state) > severity(a.state) ? b : a;
}

CacheRegistration immodules_cache_registration(const std::string &cache_contents,
                                               const std::string &module_file) {
    CacheRegistration registration;
    for (const auto &raw : split_lines(cache_contents)) {
        const std::string line = trim(raw);
        // The header names gtk-query-immodules itself and lists every directory
        // it searched. Matching in there would report a cache that registers no
        // module at all as healthy — a comment is documentation, and GTK does
        // not read it.
        if (line.empty() || line[0] == '#') continue;

        // Registration entries are the quoted module *path*; the lines after one
        // describe the contexts it provides. Comparing basenames is exact here
        // because the two names cannot contain each other: "im-fcitx.so" is not a
        // substring of "im-fcitx5.so", nor the reverse.
        if (!module_file.empty() && line.find(module_file) != std::string::npos) {
            registration.lists_module = true;
            continue;
        }
        if (line.find("fcitx") == std::string::npos) continue;
        // Some other fcitx module is registered. Only a path line can name a
        // file, and only the first one found is worth reporting.
        if (registration.other_fcitx.empty() && line.find(".so") != std::string::npos) {
            const auto slash = line.rfind('/');
            const std::string tail =
                slash == std::string::npos ? line : line.substr(slash + 1);
            const auto quote = tail.find('"');
            registration.other_fcitx = quote == std::string::npos ? tail : tail.substr(0, quote);
        }
    }
    return registration;
}

HostInfo probe_host() {
    HostInfo info;
    for (const auto &root : host_lib_roots()) {
        // Only roots that exist are recorded, so an empty list means the scan
        // found nowhere to look rather than a host that ships no modules.
        if (!dir_exists(root)) continue;
        info.lib_roots.push_back(root.string());
        scan_host_root(root, info);
    }
    return info;
}

// Program launchers that are not the program. `ldd /usr/bin/python3` answers for
// the interpreter, which links no toolkit — and recording that as "this
// application uses no GTK or Qt" is the one answer that must never be invented,
// because it is what lets a real toolkit gap be dismissed as harmless.
//
// Matched on the basename so /usr/bin/env and env are the same entry: the strip
// loop in parse_desktop_entry only removed the bare word, so an absolute
// /usr/bin/env became the recorded "application".
bool is_interpreter(const std::string &binary) {
    const std::string name = fs::path(binary).filename().string();
    for (const char *interpreter : {"env", "sh", "bash", "dash", "zsh", "fish", "python",
                                    "python2", "python3", "perl", "ruby", "node", "gjs",
                                    "java", "mono", "wine", "flatpak", "snap"}) {
        if (name == interpreter) return true;
    }
    // Versioned interpreters (python3.12, ruby3.1) are the same story.
    return name.rfind("python3.", 0) == 0 || name.rfind("python2.", 0) == 0;
}

// Prefixes a binary must live under before doctor will hand it to `ldd`.
//
// ldd resolves dependencies by running the target's ELF interpreter, so it is not
// a read. The desktop entries it takes names from include the user-writable
// ~/.local/share/applications, and those names are resolved through the caller's
// PATH — without this restriction a diagnostic that exists to be run on a broken
// machine would execute whatever those pointed at. Anything outside these
// prefixes is still reported, as an application whose toolkit was not measured.
bool under_system_prefix(const fs::path &binary) {
    const std::string path = binary.lexically_normal().string();
    for (const char *prefix : {"/usr/", "/opt/", "/bin/", "/sbin/"}) {
        if (path.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

DesktopEntry parse_desktop_entry(const std::string &contents) {
    DesktopEntry entry;
    bool in_entry = false;
    std::string exec;
    std::string type = "Application";  // the spec's default

    for (const auto &raw : split_lines(contents)) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[') {
            // Leaving [Desktop Entry] ends the group we care about: the
            // [Desktop Action *] groups that follow carry an Exec of their own.
            if (in_entry) break;
            in_entry = (line == "[Desktop Entry]");
            continue;
        }
        if (!in_entry) continue;

        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        // First occurrence wins, and the localised forms (Name[vi]) are skipped:
        // the report is about identifying an application, not displaying it in
        // the user's locale.
        if (key == "Name" && entry.name.empty()) entry.name = value;
        else if (key == "Exec" && exec.empty()) exec = value;
        else if (key == "Type") type = value;
        else if ((key == "NoDisplay" || key == "Hidden") && value == "true") entry.hidden = true;
    }

    entry.is_application = (type == "Application");

    auto tokens = split_exec(exec);
    // A wrapper prefix names the environment, not the program: `env GDK_BACKEND=x
    // gedit`, `/usr/bin/env GDK_BACKEND=x gedit` and `GDK_BACKEND=x gedit` all
    // start gedit, and taking the first token blindly filed the application under
    // "env" — which then measured as an application using no toolkit at all.
    while (!tokens.empty()) {
        const std::string &token = tokens.front();
        const auto equals = token.find('=');
        const auto slash = token.find('/');
        const bool assignment = equals != std::string::npos &&
                                (slash == std::string::npos || equals < slash);
        if (fs::path(token).filename() == "env" || assignment) {
            tokens.erase(tokens.begin());
            continue;
        }
        break;
    }
    if (!tokens.empty()) entry.exec_binary = tokens.front();
    return entry;
}

void record_ldd_output(const std::string &output, const std::vector<std::string> &measured,
                       std::vector<NativeApp> &apps) {
    std::map<std::string, NativeApp *> by_path;
    for (auto &app : apps) by_path[app.binary] = &app;

    // With one argument ldd prints no header, so there is nothing to match on and
    // every line belongs to the only binary we asked about. Keyed on the argument
    // list, not on apps.size(): applications deliberately left unmeasured are in
    // `apps` too, so counting those made a genuine one-argument run attribute its
    // output to nothing and report every application as unmeasured.
    NativeApp *current = nullptr;
    if (measured.size() == 1) {
        const auto only = by_path.find(measured.front());
        if (only != by_path.end()) current = only->second;
    }
    std::set<const NativeApp *> answered;

    for (const auto &raw : split_lines(output)) {
        // A header is a section start: unindented and ending in ':'. Dependency
        // lines are indented with a tab, which is what keeps a library path
        // ending in ':' from being read as one.
        if (!raw.empty() && raw.front() != ' ' && raw.front() != '\t' && raw.back() == ':') {
            const auto entry = by_path.find(raw.substr(0, raw.size() - 1));
            current = entry != by_path.end() ? entry->second : nullptr;
            continue;
        }
        if (current == nullptr) continue;

        const std::string line = trim(raw);
        if (line.empty()) continue;
        // The section had readable content, so whatever it does or does not link
        // is now known rather than unmeasured.
        answered.insert(current);
        for (const auto &library : kToolkitLibraries) {
            if (line.find(library.soname) != std::string::npos) current->*library.flag = true;
        }
    }

    for (auto &app : apps) {
        if (answered.count(&app) == 0) app.toolkit_unknown = true;
    }
}

bool host_has_toolkit_gap(const HostInfo &host) {
    for (const HostToolkit *toolkit : {&host.gtk3, &host.gtk4, &host.qt5, &host.qt6}) {
        if (toolkit->present && toolkit->module == ModuleKind::None) return true;
    }
    return false;
}

void probe_native_apps(HostInfo &host) {
    std::error_code ec;
    std::map<std::string, NativeApp> by_path;  // resolved path -> application
    for (const auto &dir : desktop_entry_dirs()) {
        for (const auto &file : fs::directory_iterator(dir, ec)) {
            if (file.path().extension() != ".desktop") continue;

            const DesktopEntry entry = parse_desktop_entry(read_file(file.path()));
            if (!entry.is_application || entry.hidden || entry.exec_binary.empty()) continue;

            const std::string resolved = resolve_on_path(entry.exec_binary);
            // A binary the entry names but that is not installed cannot be the
            // application that fails to type, and passing it to ldd would only
            // produce a section we then have to discard.
            if (resolved.empty() || !fs::exists(resolved, ec)) continue;
            // Several entries routinely point at one binary (a browser and its
            // private-window entry). Keying on the real path also collapses the
            // /usr/bin symlink farms, so ldd is asked once per program.
            const fs::path canonical = fs::canonical(resolved, ec);
            const std::string key = ec ? resolved : canonical.string();
            ec.clear();

            NativeApp app;
            app.binary = key;
            app.name = entry.name.empty() ? entry.exec_binary : entry.name;
            // Two kinds of application we refuse to measure rather than measure
            // wrongly: a launcher whose ldd output describes the interpreter
            // instead of the program, and a binary outside the system prefixes,
            // which ldd would have to execute (see under_system_prefix).
            app.toolkit_unknown = is_interpreter(key) || !under_system_prefix(key);
            by_path.emplace(key, std::move(app));
        }
        ec.clear();
    }

    // Nothing inspected, so nothing measured: leaving apps_scanned false is what
    // stops judge_host from reading an empty list as proof that no application
    // uses the missing toolkit. Setting it up front turned "we could not look"
    // into "we looked and found nobody", which cleared a real failure.
    if (by_path.empty()) return;

    std::vector<std::string> measured;
    std::string arguments;
    for (auto &pair : by_path) {
        if (!pair.second.toolkit_unknown) {
            measured.push_back(pair.first);
            arguments += " " + shell_quote(pair.first);
        }
        host.apps.push_back(std::move(pair.second));
    }
    host.apps_scanned = true;

    // No ldd, or nothing we are willing to run it on: the applications are known,
    // their toolkits are not. Every one is an unmeasured application rather than
    // an application that uses no toolkit.
    if (measured.empty() || !have_command("ldd")) {
        for (auto &app : host.apps) app.toolkit_unknown = true;
        return;
    }

    // One ldd for the whole list. Per application it was one subprocess each,
    // which on a normal desktop is a few hundred spawns and seconds of wall
    // clock; ldd prints a "<path>:" header per file precisely so this works.
    // The budget is raised because it is now one call doing all the work.
    const CommandResult result = run_capturing_status("ldd" + arguments, kLddTimeoutSeconds);
    record_ldd_output(result.output, measured, host.apps);
}

void probe_flatpak(Report &report) {
    report.flatpak_present = have_command("flatpak");
    if (!report.flatpak_present) return;

    const std::vector<fs::path> roots = flatpak_roots();

    // The global override is one value for the whole machine; reading it inside
    // the app loop re-parsed the same two files once per installed app.
    std::optional<std::string> global_override;
    for (const auto &root : roots) {
        if (const auto value = override_gtk_im_module(root / "overrides" / "global")) {
            global_override = value;
        }
    }

    // Apps first, so only the runtimes something actually sits on get scanned.
    // Walking <root>/runtime instead meant scanning every ref on disk — .Locale
    // and .Debug extensions, GL drivers, SDKs, old branches — and judge_sandboxes
    // then discarded all but the handful an app referenced. On this machine that
    // was 39 directory trees walked to report on 4.
    for (const auto &line : split_lines(run("flatpak list --app --columns=application,runtime"))) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        SandboxApp app;
        app.kind = "flatpak";
        app.id = trim(line.substr(0, tab));
        app.runtime_id = trim(line.substr(tab + 1));
        if (app.id.empty()) continue;
        // `flatpak list` repeats an app once per installed branch; the extra
        // rows would only duplicate every finding.
        const bool seen = std::any_of(report.apps.begin(), report.apps.end(),
                                      [&](const SandboxApp &other) {
                                          return other.kind == "flatpak" && other.id == app.id;
                                      });
        if (seen) continue;

        app.override_gtk_im_module = global_override;
        app.override_is_global = global_override.has_value();
        for (const auto &root : roots) {
            if (const auto per_app = override_gtk_im_module(root / "overrides" / app.id)) {
                app.override_gtk_im_module = per_app;  // beats global
                app.override_is_global = false;
            }
        }

        report.apps.push_back(std::move(app));
    }

    // A runtime ref is "<name>/<arch>/<branch>", which is exactly its path under
    // <root>/runtime, so each one is reached directly instead of by searching.
    for (const auto &app : report.apps) {
        if (app.kind != "flatpak" || app.runtime_id.empty()) continue;
        const bool known = std::any_of(
            report.runtimes.begin(), report.runtimes.end(),
            [&](const SandboxRuntime &other) { return other.id == app.runtime_id; });
        if (known) continue;

        for (const auto &root : roots) {
            const fs::path files = root / "runtime" / app.runtime_id / "active" / "files";
            if (!dir_exists(files)) continue;
            SandboxRuntime runtime;
            runtime.kind = "flatpak";
            runtime.id = app.runtime_id;
            runtime.modules = scan_runtime(files);
            report.runtimes.push_back(std::move(runtime));
            break;  // the same runtime is routinely installed in both roots
        }
    }
}

SnapConnections parse_snap_connections(const std::string &output) {
    SnapConnections parsed;

    for (const auto &line : split_lines(output)) {
        const auto columns = split_ws(line);
        if (columns.size() < 3) continue;
        const std::string &interface = columns[0];
        const std::string &plug = columns[1];
        const std::string &slot = columns[2];
        if (interface == "Interface") continue;  // header

        // The plug column reads "<snap>:<plug>". Rows describing a slot alone
        // have "-" here and name no consumer.
        const auto plug_colon = plug.find(':');
        if (plug_colon == std::string::npos || plug_colon == 0) continue;
        const std::string owner = plug.substr(0, plug_colon);

        const auto slot_colon = slot.find(':');
        const std::string provider =
            slot == "-" || slot_colon == 0 ? std::string() : slot.substr(0, slot_colon);

        if (interface.rfind("content[", 0) == 0) {
            // Whatever snap sits on the slot side of a content interface IS the
            // runtime, by definition. Deriving the set this way replaces two
            // copies of a hardcoded gnome-/kde-/kf5-/kf6- prefix list that would
            // silently misclassify the next platform snap to appear.
            if (provider.empty()) continue;
            if (std::find(parsed.providers.begin(), parsed.providers.end(), provider) ==
                parsed.providers.end()) {
                parsed.providers.push_back(provider);
            }
            auto &owned = parsed.providers_of[owner];
            if (std::find(owned.begin(), owned.end(), provider) == owned.end()) {
                owned.push_back(provider);
            }
            continue;
        }

        const bool im_relevant = interface == "desktop" || interface == "desktop-legacy" ||
                                 interface == "wayland" || interface == "x11";
        if (!im_relevant) continue;

        parsed.graphical.insert(owner);
        if (slot == "-") parsed.unconnected[owner].push_back(interface);
    }

    return parsed;
}

void probe_snap(Report &report) {
    report.snap_present = have_command("snap") && dir_exists("/snap");
    if (!report.snap_present) return;

    // One listing for the whole machine. This used to be one `snap connections
    // <name>` per installed snap, i.e. a subprocess and a snapd round-trip each,
    // which put seconds on every default run.
    const SnapConnections connections = parse_snap_connections(run("snap connections --all"));

    std::map<std::string, ModuleSet> scanned;
    for (const auto &name : connections.providers) {
        if (!is_safe_id(name)) continue;
        const fs::path root = fs::path("/snap") / name / "current";
        if (!dir_exists(root)) continue;
        scanned[name] = scan_runtime(root);
    }

    const SnapRuntimeSelection selection = select_snap_runtimes(connections, scanned);
    for (const auto &name : selection.runtimes) {
        SandboxRuntime runtime;
        runtime.kind = "snap";
        runtime.id = name;
        runtime.modules = scanned.at(name);
        report.runtimes.push_back(std::move(runtime));
    }

    // Only snaps that plug a graphical interface can have an IME story at all,
    // so bases, snapd and CLI snaps fall out without a name-based skip list.
    for (const auto &name : connections.graphical) {
        if (!is_safe_id(name)) continue;
        SandboxApp app;
        app.kind = "snap";
        app.id = name;

        const auto runtime = selection.runtime_of_app.find(name);
        if (runtime != selection.runtime_of_app.end()) app.runtime_id = runtime->second;

        const auto unconnected = connections.unconnected.find(name);
        if (unconnected != connections.unconnected.end()) {
            app.unconnected_interfaces = unconnected->second;
        }

        report.apps.push_back(std::move(app));
    }
}

void probe_sandbox_env(Report &report) {
    for (auto &app : report.apps) {
        if (!is_safe_id(app.id)) continue;

        // The bare `echo` first guarantees the marker starts a line of its own
        // even when the sandbox left the cursor mid-line, and echo's newline is
        // what makes the value a *line* rather than a tail of the chatter.
        const std::string script =
            std::string("echo; echo \"") + kDeepProbeMarker + "$GTK_IM_MODULE\"";

        // A sandbox launch can hang on a broken app; run_capturing_status bounds
        // it the same way it bounds every other probe, so the deadline lives in
        // one place instead of being spelled out per command.
        std::string command;
        if (app.kind == "flatpak") {
            command = "flatpak run --command=sh " + app.id + " -c '" + script + "'";
        } else {
            command = "echo '" + script + "' | snap run --shell " + app.id;
        }

        const CommandResult result = run_capturing_status(command, kSandboxTimeoutSeconds);
        record_deep_probe(result.status, result.output, app);
    }
}

}  // namespace telebit::doctor
