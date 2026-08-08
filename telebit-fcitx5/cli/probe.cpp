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

ModuleKind scan_dir(const fs::path &dir, bool gtk3) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return ModuleKind::None;
    ModuleKind best = ModuleKind::None;
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        const ModuleKind kind = classify(entry.path().filename().string(), gtk3);
        if (kind == ModuleKind::Fcitx5) return ModuleKind::Fcitx5;  // nothing outranks it
        if (kind == ModuleKind::Fcitx4) best = ModuleKind::Fcitx4;
    }
    return best;
}

void merge(ModuleKind &slot, ModuleKind found) {
    if (found == ModuleKind::Fcitx5) slot = ModuleKind::Fcitx5;
    else if (found == ModuleKind::Fcitx4 && slot == ModuleKind::None) slot = ModuleKind::Fcitx4;
}

// Looks for IM modules under one library root. Flatpak runtimes put them in
// files/lib/... while the Snap platform snaps use usr/lib/<triplet>/..., so the
// caller passes both candidates and we also descend one level to cover the
// architecture triplet directory.
void scan_lib_root(const fs::path &lib_root, ModuleSet &out) {
    if (!dir_exists(lib_root)) return;

    std::vector<fs::path> bases{lib_root};
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(lib_root, ec)) {
        if (entry.is_directory(ec)) bases.push_back(entry.path());
    }

    for (const auto &base : bases) {
        // One listing of the library directory answers "is this toolkit even
        // here", which is what separates a missing module from a non-existent
        // problem.
        for (const auto &entry : fs::directory_iterator(base, ec)) {
            const std::string name = entry.path().filename().string();
            if (name.rfind("libgtk-3.so", 0) == 0) out.gtk3_present = true;
            else if (name.rfind("libgtk-4.so", 0) == 0) out.gtk4_present = true;
            else if (name.rfind("libQt5Gui.so", 0) == 0 || name.rfind("libQt6Gui.so", 0) == 0) {
                out.qt_present = true;
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

        for (const char *qt : {"plugins", "qt5/plugins", "qt6/plugins"}) {
            merge(out.qt, scan_dir(base / qt / "platforminputcontexts", false));
        }
    }
}

ModuleSet scan_runtime(const fs::path &root) {
    ModuleSet set;
    scan_lib_root(root / "lib", set);
    scan_lib_root(root / "usr" / "lib", set);
    return set;
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
