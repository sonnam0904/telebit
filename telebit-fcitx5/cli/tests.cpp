// Tests for everything in `telebit doctor` that can be asserted without a live
// system: text measurement, the compositor table, and the verdicts.
//
// That boundary is the point. The Wayland branches below are unreachable on the
// X11 machine this was written on, and a table built by hand reaches states no
// single machine has — a runtime with Qt and no Qt module, a compositor that is
// not the one running. Every case marked "regression" is a bug that shipped.

// Release is the default build type, and NDEBUG would compile every assert
// away — the suite would then pass without testing anything. Same guard as the
// engine suite in tests.cpp at the repository root.
#undef NDEBUG

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "doctor.h"
#include "probe.h"
#include "textfmt.h"
#include "verdict.h"

using namespace telebit::doctor;

namespace {

int checks_run = 0;

void check(bool condition, const char *what) {
    ++checks_run;
    if (!condition) {
        std::cerr << "FAILED: " << what << "\n";
    }
    assert(condition);
}

std::vector<std::string> split_rendered_lines(const std::string &text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

// Border lines and row lines all start with one of the box characters; blank
// separators between sections do not.
bool is_table_line(const std::string &line) {
    for (const char *edge : {"\u2502", "\u250c", "\u251c", "\u2514"}) {
        if (line.rfind(edge, 0) == 0) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// textfmt
// ---------------------------------------------------------------------------

void test_display_width() {
    check(display_width("abc") == 3, "ASCII counts one cell per character");
    check(display_width("") == 0, "empty string is zero cells");

    // Precomposed U+1EDF versus o + U+031B (horn) + U+0309 (hook above). Both
    // render as one cell; counting code points would call the second one three.
    const std::string precomposed = "ở";
    const std::string decomposed = "ở";
    check(precomposed != decomposed, "the two forms really are different bytes");
    check(display_width(precomposed) == 1, "precomposed Vietnamese letter is one cell");
    check(display_width(decomposed) == 1,
          "regression: decomposed letter is one cell, not three");
    check(display_width(precomposed) == display_width(decomposed),
          "regression: both normalisation forms measure the same");

    check(display_width("Tiến trình") == 10, "a real label measures by cells");
    check(display_width("│") == 1, "box drawing character is one cell");
}

void test_pad_to() {
    check(pad_to("ab", 5) == "ab   ", "short text is padded to the column");
    check(pad_to("abcde", 5) == "abcde",
          "regression: text that exactly fills the column gets no padding");
    check(pad_to("abcdefg", 5) == "abcdefg", "overlong text is returned untouched");
    check(display_width(pad_to("Tiến", 10)) == 10, "padding measures in cells, not bytes");
}

void test_hard_break() {
    const std::string path = "/usr/lib/x86_64-linux-gnu/fcitx5/telebit-fcitx5.so";
    const auto pieces = hard_break(path, 20);
    std::string rejoined;
    for (const auto &piece : pieces) {
        check(display_width(piece) <= 20, "no piece is wider than the column");
        rejoined += piece;
    }
    check(rejoined == path, "the pieces rejoin to the original, nothing dropped");

    // Breaking inside a multi-byte character would emit invalid UTF-8.
    const auto vietnamese = hard_break("đường dẫn rất dài", 4);
    for (const auto &piece : vietnamese) {
        check(display_width(piece) <= 4, "multi-byte text also respects the column");
    }
}

void test_wrap() {
    const auto lines = wrap("một hai ba bốn năm sáu bảy tám", 10);
    for (const auto &line : lines) {
        check(display_width(line) <= 10, "wrap never exceeds the column");
    }

    check(wrap("", 10).size() == 1, "empty input still yields one line to index");

    // A word with no spaces and wider than the column used to punch straight
    // through the table border.
    const auto long_word = wrap("flatpak:org.freedesktop.Platform/25.08", 12);
    for (const auto &line : long_word) {
        check(display_width(line) <= 12, "regression: unbreakable word is hard-broken");
    }
    check(long_word.size() > 1, "the long word really was split");
}

// ---------------------------------------------------------------------------
// verdict
// ---------------------------------------------------------------------------

// A correctly configured session. All three variables are set, because with
// only GTK_IM_MODULE filled in this is no longer a healthy configuration —
// which is the whole point of the Qt/XIM checks below.
SessionInfo session_on(const std::string &display_server, const std::string &compositor) {
    SessionInfo session;
    session.display_server = display_server;
    session.compositor = compositor;
    session.env_source = "test";
    session.gtk_im_module = "fcitx";
    session.qt_im_module = "fcitx";
    session.xmodifiers = "@im=fcitx";
    session.shell_gtk_im_module = "fcitx";
    return session;
}

const Row *find_row(const Output &out, const std::string &label) {
    for (const auto &section : out.sections) {
        for (const auto &row : section.rows) {
            if (row.label == label) return &row;
        }
    }
    return nullptr;
}

// Mirrors org.gnome.Platform: GTK3 and GTK4 modules present, no Qt at all.
SandboxRuntime gnome_flatpak_runtime() {
    SandboxRuntime runtime;
    runtime.kind = "flatpak";
    runtime.id = "org.gnome.Platform/x86_64/49";
    runtime.modules.gtk3 = ModuleKind::Fcitx5;
    runtime.modules.gtk3_present = true;
    runtime.modules.gtk4 = ModuleKind::Fcitx5;
    runtime.modules.gtk4_present = true;
    runtime.modules.qt_present = false;  // no Qt shipped
    return runtime;
}

// Mirrors gnome-46-2404: the legacy fcitx4 GTK3 module, GTK4 and Qt shipped
// with no module for either.
SandboxRuntime snap_platform_runtime() {
    SandboxRuntime runtime;
    runtime.kind = "snap";
    runtime.id = "gnome-46-2404";
    runtime.modules.gtk3 = ModuleKind::Fcitx4;
    runtime.modules.gtk3_present = true;
    runtime.modules.gtk4_present = true;
    runtime.modules.qt_present = true;
    return runtime;
}

void test_compositor_identification() {
    check(compositor_family("gnome-shell") == "mutter",
          "GNOME's compositor runs as gnome-shell but is called mutter");
    check(compositor_family("kwin_wayland") == "kwin", "both KWin binaries fold into one name");
    check(compositor_family("kwin_x11") == "kwin", "including the X11 one");
    check(compositor_family("cosmic-comp") == "cosmic-comp",
          "regression: COSMIC ships as the default desktop on Pop!_OS and was not recognised "
          "at all, leaving the compositor column blank on that distro");
    check(compositor_family("niri") == "niri", "niri is recognised");
    check(compositor_family("something-else") == "something-else",
          "an unknown compositor is reported verbatim rather than guessed at");

    check(!compositor_handles_ime_natively("mutter"),
          "mutter has no zwp_input_method_v2 — it drives IMEs over ibus, so the variables "
          "still matter");
    check(!compositor_handles_ime_natively("weston"), "weston has no text-input-v3 at all");
    check(compositor_handles_ime_natively("kwin"), "KWin lets fcitx5 attach directly");
    check(compositor_handles_ime_natively("cosmic-comp"), "so does COSMIC");
    check(compositor_handles_ime_natively("sway"), "and sway");
    check(compositor_handles_ime_natively("niri"), "and niri");
    check(!compositor_handles_ime_natively("something-else"),
          "an unknown compositor answers no: advising someone to unset the variables on a "
          "compositor that cannot drive the IME would leave them unable to type");
}

void test_native_ime_advice_is_not_kwin_only() {
    // The advice was hardcoded to KWin and said nothing on every other
    // compositor the same reasoning applies to.
    for (const char *compositor : {"kwin", "cosmic-comp", "sway", "niri"}) {
        Output out;
        judge_session(session_on("wayland", compositor), out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Info,
              "regression: every compositor with input-method-v2 gets the same advice");
    }
    {
        Output out;
        judge_session(session_on("wayland", "mutter"), out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Ok,
              "but mutter must not be told to unset them");
    }
}

void test_module_cell() {
    check(module_cell(ModuleKind::Fcitx5, true) == "fcitx5", "a present module names itself");
    check(module_cell(ModuleKind::None, true) == "thiếu",
          "toolkit shipped without a module is a gap");
    check(module_cell(ModuleKind::None, false) == "n/a",
          "regression: a toolkit that is not shipped is not a defect");
}

void test_runtime_label() {
    check(runtime_label("flatpak", "org.gnome.Platform/x86_64/49") ==
              "flatpak:org.gnome.Platform/49",
          "the architecture segment is dropped");
    check(runtime_label("snap", "gnome-46-2404") == "snap:gnome-46-2404",
          "a snap id has no architecture to drop");
}

void test_runtime_without_qt_is_clean() {
    const auto verdict = judge_runtime(gnome_flatpak_runtime(), session_on("x11", "mutter"));
    check(verdict.row.status == Status::Ok,
          "regression: a runtime that ships no Qt is not broken for lacking a Qt module");
    check(!verdict.unfixable_gap, "and there is no gap to report");
}

void test_missing_module_on_x11_is_a_risk_not_a_failure() {
    const auto verdict = judge_runtime(snap_platform_runtime(), session_on("x11", "mutter"));
    check(verdict.row.status != Status::Fail,
          "regression: a toolkit gap is a risk, not an incident — Fail would put a cross "
          "above apps that work and make the exit status call a healthy machine broken");
    check(verdict.row.status == Status::Warn, "it is still worth warning about");
    check(verdict.unfixable_gap, "and it is flagged as unfixable from the Telebit side");
}

void test_missing_module_on_wayland_is_harmless() {
    const auto verdict = judge_runtime(snap_platform_runtime(), session_on("wayland", "mutter"));
    check(!verdict.unfixable_gap,
          "on Wayland the missing GTK4/Qt modules are bypassed by text-input-v3");
    check(verdict.row.status != Status::Fail, "so nothing here is a failure");
}

void test_session_env_empty_depends_on_display_server() {
    {
        SessionInfo session = session_on("x11", "mutter");
        session.gtk_im_module.clear();
        session.shell_gtk_im_module.clear();
        Output out;
        judge_session(session, out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Fail,
              "on X11 an empty GTK_IM_MODULE means no fcitx5 module is loaded at all");
    }
    {
        SessionInfo session = session_on("wayland", "kwin");
        session.gtk_im_module.clear();
        session.shell_gtk_im_module.clear();
        Output out;
        judge_session(session, out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status != Status::Fail,
              "on Wayland leaving it unset is a supported configuration, not an error");
    }
}

void test_session_env_pointing_elsewhere_fails() {
    SessionInfo session = session_on("x11", "mutter");
    session.gtk_im_module = "ibus";
    session.shell_gtk_im_module = "ibus";
    Output out;
    judge_session(session, out);
    const Row *row = find_row(out, "IM env của session");
    check(row != nullptr && row->status == Status::Fail, "pointing at ibus is a real failure");
    check(!out.suggestions.empty(), "and the user is told where to look");
}

void test_qt_and_xmodifiers_are_judged_too() {
    // Judging GTK_IM_MODULE alone reported a healthy session while every Qt
    // application still talked to ibus.
    {
        SessionInfo session = session_on("x11", "mutter");
        session.gtk_im_module = "fcitx";
        session.qt_im_module = "ibus";
        session.xmodifiers = "@im=fcitx";
        Output out;
        judge_session(session, out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Fail,
              "regression: QT_IM_MODULE pointing at ibus is a failure even when GTK is right");
    }
    {
        SessionInfo session = session_on("x11", "mutter");
        session.gtk_im_module = "fcitx";
        session.qt_im_module.clear();
        session.xmodifiers = "@im=fcitx";
        Output out;
        judge_session(session, out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Fail,
              "regression: on X11 a missing QT_IM_MODULE leaves Qt apps unable to reach fcitx5");
    }
    {
        // XIM is the legacy path — worth flagging, not worth failing over.
        SessionInfo session = session_on("x11", "mutter");
        session.gtk_im_module = "fcitx";
        session.qt_im_module = "fcitx";
        session.xmodifiers.clear();
        Output out;
        judge_session(session, out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Warn,
              "regression: a missing XMODIFIERS warns but does not fail");
    }
    {
        SessionInfo session = session_on("x11", "mutter");
        session.gtk_im_module = "fcitx";
        session.qt_im_module = "fcitx";
        session.xmodifiers = "@im=fcitx";
        Output out;
        judge_session(session, out);
        const Row *row = find_row(out, "IM env của session");
        check(row != nullptr && row->status == Status::Ok, "all three correct is clean");
    }
}

// /proc/<pid>/environ is NUL-separated, so the fixtures below build one.
std::string environ_blob(const std::vector<std::string> &entries) {
    std::string blob;
    for (const auto &entry : entries) {
        blob += entry;
        blob.push_back('\0');
    }
    return blob;
}

void test_session_type_comes_from_the_compositor() {
    // A Wayland desktop, inspected from an SSH shell or a terminal left over
    // from an earlier X11 login. Reading our own environment called this X11 and
    // then applied every X11 rule to a Wayland session.
    {
        SessionInfo info;
        const bool filled = fill_session_from_environ(
            environ_blob({"XDG_SESSION_TYPE=wayland", "WAYLAND_DISPLAY=wayland-0",
                          "XDG_CURRENT_DESKTOP=KDE", "GTK_IM_MODULE=fcitx"}),
            info);
        check(filled, "a usable environment block is accepted");
        check(info.display_server == "wayland",
              "regression: the display server comes from the compositor's own environment, "
              "not from whatever shell launched doctor");
        check(info.desktop == "KDE", "and so does the desktop name");
        check(info.gtk_im_module == "fcitx", "along with the IM variables");
    }
    {
        // GNOME on X11: same gnome-shell binary as the Wayland session, so no
        // table of process names could tell these two apart.
        SessionInfo info;
        fill_session_from_environ(environ_blob({"XDG_SESSION_TYPE=x11", "DISPLAY=:0"}), info);
        check(info.display_server == "x11", "an X11 session is recognised as X11");
    }
    {
        // XDG_SESSION_TYPE missing. A Wayland session also sets DISPLAY for
        // XWayland, so DISPLAY alone must not win.
        SessionInfo info;
        fill_session_from_environ(environ_blob({"WAYLAND_DISPLAY=wayland-0", "DISPLAY=:0"}), info);
        check(info.display_server == "wayland",
              "WAYLAND_DISPLAY outranks DISPLAY, which XWayland sets too");
    }
    {
        SessionInfo info;
        check(!fill_session_from_environ("", info), "an unreadable environment is refused");
        check(info.display_server.empty(), "and nothing is invented from it");
    }
    {
        SessionInfo info;
        const bool filled = fill_session_from_systemd(
            "GTK_IM_MODULE=fcitx\nQT_IM_MODULE=fcitx\nXMODIFIERS=@im=fcitx\n", info);
        check(filled && info.qt_im_module == "fcitx",
              "the systemd fallback reads the same three variables");
    }
    {
        SessionInfo info;
        check(!fill_session_from_systemd("LANG=vi_VN.UTF-8\nPATH=/usr/bin\n", info),
              "a block with no IM variables is not treated as an answer");
    }
}

// The bug was never in parsing one source, it was in choosing between them.
// These fixtures put the sources in conflict on purpose.
void test_session_source_precedence() {
    SessionSources sources;
    sources.compositor_comm = "kwin_wayland";
    sources.compositor_pid = "500";
    sources.compositor_environ =
        environ_blob({"XDG_SESSION_TYPE=wayland", "GTK_IM_MODULE=fcitx", "QT_IM_MODULE=fcitx"});
    // The caller is an SSH shell, or a terminal left over from an older X11
    // login. Everything it says about the session is wrong.
    sources.own_session_type = "tty";
    sources.own_display = ":0";
    sources.own_gtk_im_module = "ibus";

    const SessionInfo info = resolve_session(sources);
    check(info.display_server == "wayland",
          "regression: the compositor's environment wins over the caller's — reading ours "
          "called a Wayland desktop X11 and then applied every X11 rule to it");
    check(info.compositor == "kwin", "and the compositor is named from its process");
    check(info.gtk_im_module == "fcitx", "the session's IM variables win too");
    check(info.shell_gtk_im_module == "ibus",
          "the caller's own value is kept separately, so the mismatch can be reported");

    {
        // No compositor recognised: systemd is the next authority, but it
        // carries no display server, so that one does fall through to ours.
        SessionSources fallback;
        fallback.systemd_environment = "GTK_IM_MODULE=fcitx\nXMODIFIERS=@im=fcitx\n";
        fallback.own_session_type = "x11";
        const SessionInfo second = resolve_session(fallback);
        check(second.env_source == "systemctl --user show-environment",
              "systemd is preferred over the caller for the IM variables");
        check(second.gtk_im_module == "fcitx", "and supplies them");
        check(second.display_server == "x11", "the display server falls back to the caller");
    }
    {
        // Nothing at all to read.
        SessionSources bare;
        bare.own_session_type = "wayland";
        bare.own_desktop = "GNOME";
        const SessionInfo third = resolve_session(bare);
        check(third.env_source.empty(), "with no session view, none is claimed");
        check(third.display_server == "wayland" && third.desktop == "GNOME",
              "but the caller's environment is still better than nothing");
    }
}

// What the --deep script actually writes: a blank line, then the marker carrying
// whatever the variable held. Built through the same constant the probe uses, so
// renaming the marker cannot leave these tests asserting against a dead format.
std::string marked(const std::string &value) {
    return std::string("\n") + kDeepProbeMarker + value + "\n";
}

void test_deep_probe_status_decides() {
    {
        SandboxApp app;
        record_deep_probe(0, marked("fcitx"), app);
        check(app.deep_probed && !app.deep_failed, "a clean exit is a real answer");
        check(app.deep_gtk_im_module == "fcitx", "and the value comes from the marked line");
    }
    {
        SandboxApp app;
        record_deep_probe(0, marked(""), app);
        check(app.deep_probed && !app.deep_failed,
              "the marker present with nothing after it is a genuinely unset variable");
        check(app.deep_gtk_im_module.empty(), "recorded as empty, which the verdict may fail on");
    }
    {
        SandboxApp app;
        record_deep_probe(124, "", app);  // `timeout` kills the command
        check(app.deep_failed && !app.deep_probed,
              "regression: a timeout is a broken probe, not an unset variable — treating the "
              "two alike diagnosed the user's input method for our own 25s limit");
    }
    {
        SandboxApp app;
        record_deep_probe(1, "error: app is not installed", app);
        check(app.deep_failed && !app.deep_probed, "so is a failed launch");
    }
    {
        SandboxApp app;
        record_deep_probe(-1, "", app);
        check(app.deep_failed, "so is a command that never started");
    }
    {
        SandboxApp app;
        record_deep_probe(0, "Gtk-Message: some warning" + marked("fcitx"), app);
        check(app.deep_gtk_im_module == "fcitx",
              "startup chatter before the value does not become the value");
    }
    {
        // The shape that broke it: an unset variable printed nothing, so the
        // last non-empty line was the sandbox's own warning. That was recorded
        // as the value, which made the verdict layer skip the empty-variable
        // failure and draw a healthy row over an app that could not type.
        SandboxApp app;
        record_deep_probe(0, "Gtk-Message: some warning\n", app);
        check(!app.deep_probed && app.deep_failed,
              "regression: a clean exit that never printed the marker is a failed measurement, "
              "not a value — chatter must never be mistaken for what the variable held");
        check(app.deep_gtk_im_module.empty(), "and nothing is invented for it");
    }
    {
        SandboxApp app;
        record_deep_probe(0, marked("fcitx") + "some trailing chatter\n", app);
        check(app.deep_probed && app.deep_gtk_im_module == "fcitx",
              "regression: chatter after the value does not become the value either");
    }
}

void test_pick_compositor_prefers_the_oldest() {
    // A GNOME X11 session with a nested Wayland compositor started later for
    // testing. Taking /proc order would describe the toy window as the session
    // and then advise unsetting the IM variables on the real X11 desktop.
    const std::vector<CompositorCandidate> candidates = {
        {"sway", "9001", 5000000},        // nested, listed first
        {"gnome-shell", "1234", 120000},  // the session, started long before
    };
    check(pick_compositor(candidates).comm == "gnome-shell",
          "regression: the oldest candidate is the session's compositor — anything nested is "
          "started from inside it, so /proc listing order must not decide");

    check(pick_compositor({}).pid.empty(), "no candidates yields nothing");
    check(pick_compositor({{"kwin_wayland", "5", 42}}).pid == "5", "a single candidate wins");

    // An unreadable /proc/<pid>/stat gives 0; an unknown age must never beat a
    // known one.
    const std::vector<CompositorCandidate> partial = {
        {"weston", "7", 0},
        {"gnome-shell", "8", 900},
    };
    check(pick_compositor(partial).comm == "gnome-shell",
          "a candidate with an unknown start time does not win over a known one");
}

void test_select_snap_runtimes() {
    SnapConnections connections;
    connections.providers = {"gnome-46-2404", "gtk-common-themes"};
    connections.providers_of["chromium"] = {"gnome-46-2404", "gtk-common-themes"};
    connections.providers_of["someapp"] = {"gtk-common-themes"};

    ModuleSet platform;
    platform.gtk3 = ModuleKind::Fcitx4;
    platform.gtk3_present = true;
    platform.gtk4_present = true;

    const std::map<std::string, ModuleSet> scanned = {
        {"gnome-46-2404", platform},
        {"gtk-common-themes", ModuleSet{}},  // icons only, no toolkit
    };

    const SnapRuntimeSelection selection = select_snap_runtimes(connections, scanned);
    check(selection.runtimes == std::vector<std::string>{"gnome-46-2404"},
          "regression: a content provider with no toolkit is not a runtime — gtk-common-themes "
          "is a provider too, and counting it filed every app under a snap that holds no "
          "input-method module at all");
    check(selection.runtime_of_app.at("chromium") == "gnome-46-2404",
          "an app connected to several providers gets the one carrying the toolkit");
    check(selection.runtime_of_app.count("someapp") == 0,
          "an app with no toolkit provider is left unassigned rather than mis-assigned");
}

void test_proc_starttime_parsing() {
    // The shape /proc/<pid>/stat has: pid, (comm), state, then 20 more fields
    // before starttime. Every value is invented — only the field *count* and
    // the position of 987654 matter, since that is what the parser has to find.
    const std::string stat =
        "4242 (fcitx5) S 1 4242 4242 0 -1 4194560 7220 0 5619 0 667 307 0 0 20 0 3 0 "
        "987654 123456789 4321 18446744073709551615";
    check(parse_proc_starttime(stat) == 987654, "field 22 is the start time");

    // A process may be named with spaces and parentheses; splitting from the
    // front would shift every field.
    const std::string awkward =
        "42 (my (weird) app) S 1 1 1 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 555 0 0 0";
    check(parse_proc_starttime(awkward) == 555,
          "parsing anchors on the last ')', not the first");

    check(parse_proc_starttime("") == 0, "unreadable stat yields 0");
    check(parse_proc_starttime("42 (short) S 1 2 3") == 0, "a truncated line yields 0");
}

void test_snap_connections_parsing() {
    const std::string output =
        "Interface               Plug                        Slot                          Notes\n"
        "content[gnome-46-2404]  chromium:gnome-46-2404      gnome-46-2404:gnome-46-2404   -\n"
        "content[gtk-3-themes]   chromium:gtk-3-themes       gtk-common-themes:gtk-3-themes -\n"
        "desktop                 chromium:desktop            :desktop                      -\n"
        "wayland                 chromium:wayland            -                             -\n"
        "alsa                    firefox:alsa                -                             -\n"
        "x11                     firefox:x11                 :x11                          -\n"
        "content[kf6-core24]     kdenlive:kf6-core24         kf6-core24:kf6-core24         -\n"
        "desktop                 kdenlive:desktop            :desktop                      -\n"
        "network                 kubectl:network             :network                      -\n";

    const SnapConnections parsed = parse_snap_connections(output);

    check(parsed.providers.size() == 3, "every content provider is found");
    check(std::find(parsed.providers.begin(), parsed.providers.end(), "gnome-46-2404") !=
              parsed.providers.end(),
          "the gnome platform snap is one of them");
    check(std::find(parsed.providers.begin(), parsed.providers.end(), "kf6-core24") !=
              parsed.providers.end(),
          "regression: a platform snap is recognised by sitting on the slot side of a content "
          "interface, not by its name prefix — the old gnome-/kde-/kf5-/kf6- list would have "
          "needed editing for every new one");

    check(parsed.providers_of.at("chromium") ==
              std::vector<std::string>{"gnome-46-2404", "gtk-common-themes"},
          "every content provider an app connects to is kept, in listing order");
    check(parsed.providers_of.at("kdenlive") == std::vector<std::string>{"kf6-core24"},
          "including the KDE one");
    check(std::find(parsed.providers.begin(), parsed.providers.end(), "gtk-common-themes") !=
              parsed.providers.end(),
          "regression: a themes snap is a content provider too, so the parser must not decide "
          "on its own which provider is the toolkit — probe_snap keeps only the providers whose "
          "filesystem actually holds GTK or Qt, and picking the last content row instead put "
          "every app under gtk-common-themes");

    check(parsed.graphical.count("chromium") == 1 && parsed.graphical.count("firefox") == 1,
          "snaps plugging a graphical interface are listed");
    check(parsed.graphical.count("kubectl") == 0,
          "a CLI snap has no IME story and falls out without a name-based skip list");

    check(parsed.unconnected.at("chromium") == std::vector<std::string>{"wayland"},
          "an interface with slot '-' is recorded as unconnected");
    check(parsed.unconnected.count("firefox") == 0,
          "and a connected one is not — alsa is unconnected but is not IM-relevant");
}

void test_empty_flatpak_override_is_a_setting() {
    Report report;
    report.flatpak_present = true;
    report.runtimes.push_back(gnome_flatpak_runtime());

    SandboxApp app;
    app.kind = "flatpak";
    app.id = "org.mozilla.firefox";
    app.runtime_id = "org.gnome.Platform/x86_64/49";
    app.override_gtk_im_module = std::string();  // `--env=GTK_IM_MODULE=`
    report.apps.push_back(app);

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);
    const Row *row = find_row(out, "org.mozilla.firefox");
    check(row != nullptr && row->status == Status::Fail,
          "regression: an override setting GTK_IM_MODULE to empty disables input inside that "
          "sandbox; collapsing it into \"no override\" reported it as healthy");

    // No override at all is still not a finding.
    Report clean = report;
    clean.apps[0].override_gtk_im_module.reset();
    Output clean_out;
    judge_sandboxes(clean, session_on("x11", "mutter"), clean_out);
    const Row *clean_row = find_row(clean_out, "org.mozilla.firefox");
    check(clean_row != nullptr && clean_row->status == Status::Info,
          "an absent override is not a problem");
}

// Counts suggestions mentioning `needle`, so a machine-wide fix can be asserted
// to appear exactly once no matter how many apps the override lands on.
int suggestions_mentioning(const Output &out, const std::string &needle) {
    int found = 0;
    for (const auto &suggestion : out.suggestions) {
        if (suggestion.find(needle) != std::string::npos) ++found;
    }
    return found;
}

void test_global_flatpak_override_is_not_a_per_app_override() {
    // `flatpak override --env=GTK_IM_MODULE=xim` with no app id is ONE setting
    // for the whole machine. It was copied into every app and then reported as
    // that app's own override, so N apps went red each blaming a per-app
    // override that does not exist, and nothing ever named the file to edit.
    Report report;
    report.flatpak_present = true;
    report.runtimes.push_back(gnome_flatpak_runtime());

    for (const char *id : {"org.mozilla.firefox", "org.gnome.meld"}) {
        SandboxApp app;
        app.kind = "flatpak";
        app.id = id;
        app.runtime_id = "org.gnome.Platform/x86_64/49";
        app.override_gtk_im_module = "xim";
        app.override_is_global = true;
        report.apps.push_back(app);
    }

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);

    const Row *row = find_row(out, "org.mozilla.firefox");
    check(row != nullptr && row->status == Status::Fail,
          "a global override pointing elsewhere still breaks every app it covers");
    check(row->note.find("chung") != std::string::npos,
          "regression: a machine-wide override must be named as such");
    check(row->note.find("của ứng dụng này") == std::string::npos,
          "regression: and must not be blamed on the app, which has no override of its own");
    check(suggestions_mentioning(out, "flatpak override --reset") == 1,
          "regression: the one file to edit is named, once, not once per affected app");

    // A genuine per-app override keeps the per-app wording and its own fix.
    Report per_app;
    per_app.flatpak_present = true;
    per_app.runtimes.push_back(gnome_flatpak_runtime());
    SandboxApp app;
    app.kind = "flatpak";
    app.id = "org.gnome.meld";
    app.runtime_id = "org.gnome.Platform/x86_64/49";
    app.override_gtk_im_module = "ibus";
    app.override_is_global = false;
    per_app.apps.push_back(app);

    Output per_app_out;
    judge_sandboxes(per_app, session_on("x11", "mutter"), per_app_out);
    const Row *per_app_row = find_row(per_app_out, "org.gnome.meld");
    check(per_app_row != nullptr && per_app_row->note.find("riêng của ứng dụng này") !=
                                        std::string::npos,
          "a real per-app override is still reported as one");
    check(suggestions_mentioning(per_app_out, "org.gnome.meld") == 1,
          "and its fix names the app rather than the machine-wide file");
}

void test_bus_name_ownership() {
    // The shape `busctl --user list --acquired` prints: NAME PID PROCESS USER ...
    // Values are invented — this parses a string, it never runs busctl, so the
    // result is identical on every machine.
    {
        const std::string output =
            "NAME                          PID PROCESS  USER  CONNECTION\n"
            "org.fcitx.Fcitx5          4242 fcitx5   user :1.645\n"
            "org.fcitx.Fcitx-0         4242 fcitx5   user :1.648\n"
            "org.freedesktop.IBus      4242 fcitx5   user :1.645\n"
            "org.freedesktop.portal.Fcitx 4242 fcitx5 user :1.646\n";
        Fcitx5Info info;
        parse_bus_names(output, info);
        check(info.running && info.bus_native, "fcitx5 owns its own name");
        check(info.bus_ibus, "and the ibus frontend, because the pid matches");
        check(info.bus_fcitx4 && info.bus_portal, "and the other two frontends");
        check(info.foreign_bus_owners.empty(), "nothing is held by anyone else");
    }
    {
        // fcitx5 is running, but a real ibus-daemon holds org.freedesktop.IBus.
        const std::string output =
            "org.fcitx.Fcitx5          4242 fcitx5      user :1.645\n"
            "org.freedesktop.IBus         9001 ibus-daemon user :1.900\n";
        Fcitx5Info info;
        parse_bus_names(output, info);
        check(info.bus_native, "fcitx5 still owns its own name");
        check(!info.bus_ibus,
              "regression: a name held by another pid is not an fcitx5 frontend — reading it "
              "as one hid a broken GNOME Wayland setup behind a green report");
        check(info.foreign_bus_owners.size() == 1, "the conflict is recorded");
        check(info.foreign_bus_owners.front() == "org.freedesktop.IBus (ibus-daemon)",
              "and it names the process that took it");
    }
    {
        // fcitx5 not running at all, ibus holding the name.
        const std::string output = "org.freedesktop.IBus 9001 ibus-daemon user :1.900\n";
        Fcitx5Info info;
        parse_bus_names(output, info);
        check(!info.running && !info.bus_ibus, "no fcitx5, no frontends");
        check(info.foreign_bus_owners.size() == 1, "but the conflict is still worth naming");
    }
    {
        // An activatable name has "-" in the owner columns and owns nothing.
        const std::string output =
            "org.fcitx.Fcitx5     4242 fcitx5 user :1.645\n"
            "org.freedesktop.IBus       - -      -     (activatable)\n";
        Fcitx5Info info;
        parse_bus_names(output, info);
        check(!info.bus_ibus, "an activatable name is not a running frontend");
        check(info.foreign_bus_owners.empty(),
              "nor is it a conflict — nobody holds it, so there is nothing to report");
    }
}

void test_foreign_bus_owner_is_reported() {
    // fcitx5 runs, but a real ibus-daemon holds org.freedesktop.IBus. Treating
    // the name's existence as proof of an fcitx5 frontend hid this entirely.
    Report report;
    report.session = session_on("wayland", "mutter");

    Fcitx5Info fcitx5;
    fcitx5.running = true;
    fcitx5.bus_native = true;
    fcitx5.addon_path = "/usr/lib/fcitx5/telebit-fcitx5.so";
    fcitx5.bus_ibus = false;
    fcitx5.foreign_bus_owners.push_back("org.freedesktop.IBus (ibus-daemon)");

    Output out;
    judge_fcitx5(fcitx5, report.session, report, out);
    const Row *row = find_row(out, "Tên D-Bus bị chiếm");
    check(row != nullptr && row->status == Status::Fail,
          "regression: another process holding a name fcitx5 needs is a failure, and the "
          "report names the culprit");
    check(out.has_failure(), "and it reaches the exit status");
}

void test_kwin_wayland_advice() {
    Output out;
    judge_session(session_on("wayland", "kwin"), out);
    const Row *row = find_row(out, "IM env của session");
    check(row != nullptr && row->status == Status::Info,
          "on Plasma/Wayland upstream recommends leaving the variables unset, so setting "
          "them is informational rather than correct");
}

void test_mutter_wayland_needs_ibus_frontend() {
    Report report;
    report.session = session_on("wayland", "mutter");
    Fcitx5Info fcitx5;
    fcitx5.running = true;
    fcitx5.bus_native = true;
    fcitx5.addon_path = "/usr/lib/fcitx5/telebit-fcitx5.so";
    fcitx5.bus_ibus = false;

    Output out;
    judge_fcitx5(fcitx5, report.session, report, out);
    check(find_row(out, "Frontend ibus") != nullptr,
          "GNOME Wayland drives the IME over ibus, so a missing ibus frontend is reported");
}

void test_snap_fcitx4_module_needs_fcitx4_frontend() {
    Report report;
    report.session = session_on("x11", "mutter");
    report.snap_present = true;
    report.runtimes.push_back(snap_platform_runtime());

    Fcitx5Info fcitx5;
    fcitx5.running = true;
    fcitx5.bus_native = true;
    fcitx5.bus_portal = true;
    fcitx5.addon_path = "/usr/lib/fcitx5/telebit-fcitx5.so";
    fcitx5.bus_fcitx4 = false;

    Output out;
    judge_fcitx5(fcitx5, report.session, report, out);
    const Row *row = find_row(out, "Frontend fcitx4");
    check(row != nullptr && row->status == Status::Fail,
          "the snap ships only a fcitx4 module, so without that frontend it is broken now");
}

void test_fcitx5_not_running_stops_early() {
    Report report;
    Fcitx5Info fcitx5;  // running == false
    Output out;
    judge_fcitx5(fcitx5, report.session, report, out);
    check(out.has_failure(), "a stopped fcitx5 is a failure");
    check(out.sections.size() == 1 && out.sections[0].rows.size() == 1,
          "and nothing downstream is worth reporting");
}

void test_misrouted_does_not_hide_a_missing_variable() {
    // GTK_IM_MODULE handed to another input method AND QT_IM_MODULE unset. As an
    // else-if chain only the first was ever printed: the user fixed GTK, every Qt
    // application stayed dead, and doctor said so for the first time one run later.
    SessionInfo session = session_on("x11", "mutter");
    session.gtk_im_module = "ibus";
    session.qt_im_module.clear();

    Output out;
    judge_session(session, out);
    const Row *row = find_row(out, "IM env của session");
    check(row != nullptr && row->status == Status::Fail, "the session is broken either way");
    check(row->note.find("GTK_IM_MODULE=ibus") != std::string::npos,
          "the misrouted variable is named");
    check(row->note.find("QT_IM_MODULE") != std::string::npos,
          "regression: and so is the missing one, which the else-if chain swallowed");
    check(suggestions_mentioning(out, "environment.d") >= 1,
          "regression: the fix for the missing variable is offered too");
}

void test_app_without_a_runtime_is_not_drawn_as_one() {
    // A snap that bundles its own toolkit resolves to no runtime. Emitted at
    // depth 0 it sat level with the real runtime rows, and the reference page
    // tells the reader every top-level row is a runtime or platform snap — so
    // the app was read as a runtime doctor had failed to analyse.
    Report report;
    report.snap_present = true;
    report.runtimes.push_back(snap_platform_runtime());

    SandboxApp app;
    app.kind = "snap";
    app.id = "firefox";
    // no runtime_id: nothing to sit under
    report.apps.push_back(app);

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);

    const Row *row = find_row(out, "snap:firefox");
    check(row != nullptr, "the app is still listed rather than silently dropped");
    check(row->depth == 1,
          "regression: an app must be nested, not drawn level with the runtimes");

    const Row *heading = find_row(out, "Không gắn được runtime");
    check(heading != nullptr && heading->depth == 0,
          "regression: and it sits under a heading that cannot be mistaken for a runtime");
}

void test_unused_runtime_is_not_reported() {
    Report report;
    report.flatpak_present = true;
    report.runtimes.push_back(gnome_flatpak_runtime());  // no app uses it

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);
    check(find_row(out, "flatpak:org.gnome.Platform/49") == nullptr,
          "a runtime no installed app sits on is noise");
}

void test_failed_deep_probe_is_not_an_empty_variable() {
    // --deep could not start the sandbox. That says nothing about the input
    // method, and reporting it as an empty GTK_IM_MODULE blamed the user's
    // setup for our own timeout.
    Report report;
    report.snap_present = true;
    report.runtimes.push_back(snap_platform_runtime());

    SandboxApp failed;
    failed.kind = "snap";
    failed.id = "chromium";
    failed.runtime_id = "gnome-46-2404";
    failed.deep_failed = true;
    report.apps.push_back(failed);

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);
    const Row *row = find_row(out, "chromium");
    check(row != nullptr, "the app is still listed");
    check(row->status != Status::Fail,
          "regression: a probe that could not run is not a broken input method");
    check(!out.has_failure(), "and it must not reach the exit status");

    // The same app answering with a genuinely empty variable on X11 *is* broken.
    Report answered = report;
    answered.apps[0].deep_failed = false;
    answered.apps[0].deep_probed = true;
    answered.apps[0].deep_gtk_im_module.clear();

    Output out2;
    judge_sandboxes(answered, session_on("x11", "mutter"), out2);
    const Row *row2 = find_row(out2, "chromium");
    check(row2 != nullptr && row2->status == Status::Fail,
          "an empty variable from a sandbox that did answer is a real failure");
}

void test_a_failure_keeps_its_explanation() {
    // Later, milder findings about the same app must add to the note, never
    // replace it. Assigning produced a red row whose only text said nothing had
    // been concluded, with the actual cause never printed.
    Report report;
    report.flatpak_present = true;
    report.runtimes.push_back(gnome_flatpak_runtime());

    SandboxApp app;
    app.kind = "flatpak";
    app.id = "com.example.App";
    app.runtime_id = "org.gnome.Platform/x86_64/49";
    app.override_gtk_im_module = "ibus";  // a real failure
    app.deep_failed = true;               // and the probe also could not run
    report.apps.push_back(app);

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);
    const Row *row = find_row(out, "com.example.App");
    check(row != nullptr, "the app is listed");
    check(row->status == Status::Fail,
          "regression: a failed probe must not downgrade a real failure");
    check(row->note.find("Override") != std::string::npos,
          "regression: the explanation of the failure survives alongside the probe note");
    check(row->note.find("Không khởi động được sandbox") != std::string::npos,
          "and the probe's own note is appended, not dropped");

    // Same shape for the interface branch, which had the identical bug.
    SandboxApp snap_app;
    snap_app.kind = "snap";
    snap_app.id = "example-snap";
    snap_app.runtime_id = "org.gnome.Platform/x86_64/49";
    snap_app.override_gtk_im_module = "ibus";
    snap_app.unconnected_interfaces.push_back("wayland");

    Report snap_report;
    snap_report.snap_present = true;
    snap_report.runtimes.push_back(gnome_flatpak_runtime());
    snap_report.apps.push_back(snap_app);

    Output snap_out;
    judge_sandboxes(snap_report, session_on("x11", "mutter"), snap_out);
    const Row *snap_row = find_row(snap_out, "example-snap");
    check(snap_row != nullptr && snap_row->status == Status::Fail,
          "regression: an unconnected interface is a warning and must not downgrade a Fail");
}

void test_successful_deep_probe_keeps_its_explanation() {
    // The third branch of the same shape, and the one that kept assigning while
    // its two siblings appended: a successful --deep read of an empty variable
    // overwrote whatever had already explained the row.
    Report report;
    report.snap_present = true;
    report.runtimes.push_back(gnome_flatpak_runtime());

    SandboxApp snap_app;
    snap_app.kind = "snap";
    snap_app.id = "example-snap";
    snap_app.runtime_id = "org.gnome.Platform/x86_64/49";
    snap_app.unconnected_interfaces.push_back("wayland");
    snap_app.deep_probed = true;
    snap_app.deep_gtk_im_module.clear();  // the probe ran and found nothing
    report.apps.push_back(snap_app);

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);
    const Row *row = find_row(out, "example-snap");
    check(row != nullptr && row->status == Status::Fail,
          "an empty variable inside a sandbox on X11 is a failure");
    check(row->note.find("Chưa nối interface") != std::string::npos,
          "regression: a successful deep probe must not erase the interface explanation");
    check(row->note.find("Biến không tới được") != std::string::npos,
          "and with nothing else accounting for it, the probe still says what it saw");

    // An app whose own override empties GTK_IM_MODULE did receive the variable.
    // Saying it "never reached the sandbox" would be a second, wrong cause, and
    // would send the user to environment.d instead of to `flatpak override`.
    Report overridden;
    overridden.flatpak_present = true;
    overridden.runtimes.push_back(gnome_flatpak_runtime());

    SandboxApp app;
    app.kind = "flatpak";
    app.id = "com.example.App";
    app.runtime_id = "org.gnome.Platform/x86_64/49";
    app.override_gtk_im_module = "";  // the override itself empties it
    app.deep_probed = true;
    app.deep_gtk_im_module.clear();   // so the probe consistently reads empty
    overridden.apps.push_back(app);

    Output out2;
    judge_sandboxes(overridden, session_on("x11", "mutter"), out2);
    const Row *row2 = find_row(out2, "com.example.App");
    check(row2 != nullptr && row2->status == Status::Fail,
          "the app is still broken");
    check(row2->note.find("thành rỗng") != std::string::npos,
          "regression: the override explanation survives the deep probe");
    check(row2->note.find("Biến không tới được") == std::string::npos,
          "regression: and the probe does not add a cause that contradicts it");
}

void test_missing_busctl_is_not_a_stopped_fcitx5() {
    // No busctl on PATH: the frontend flags could not be read at all. Reporting
    // them as off would invent failures on a healthy machine.
    Report report;
    report.session = session_on("x11", "mutter");
    report.snap_present = true;

    Fcitx5Info fcitx5;
    fcitx5.running = true;  // pgrep found it
    fcitx5.bus_unavailable = true;
    fcitx5.addon_path = "/usr/lib/fcitx5/telebit-fcitx5.so";

    Output out;
    judge_fcitx5(fcitx5, report.session, report, out);
    check(find_row(out, "Frontend") != nullptr,
          "regression: say the frontends could not be checked");
    check(find_row(out, "Frontend portal") == nullptr && find_row(out, "Frontend fcitx4") == nullptr,
          "and do not report individual frontends as off when nothing could be read");
    check(!out.has_failure(), "a missing busctl is not a broken input method");
}

void test_exit_status_ignores_warnings() {
    Report report;
    report.snap_present = true;
    report.runtimes.push_back(snap_platform_runtime());
    SandboxApp app;
    app.kind = "snap";
    app.id = "chromium";
    app.runtime_id = "gnome-46-2404";
    report.apps.push_back(app);

    Output out;
    judge_sandboxes(report, session_on("x11", "mutter"), out);
    check(!out.has_failure(),
          "regression: a toolkit gap must not make the exit status claim the machine is "
          "broken when every installed app still types fine");
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------

Output nested_output() {
    Output out;
    Section &s = out.section("Sandbox");
    s.rows.push_back({Status::Ok, "snap:gnome-46-2404", "GTK3 fcitx5", "", 0});
    s.rows.push_back({Status::Info, "chromium", "—", "", 1});
    return out;
}

std::string render_to_string(const Output &out, std::size_t width) {
    std::ostringstream os;
    render_pretty(out, os, false, width);
    return os.str();
}

void test_pretty_table_is_rectangular() {
    // Every line of a box-drawn table must be exactly `width` cells, or the
    // right border goes ragged. Checked at several widths because the column
    // arithmetic derives from the terminal size.
    for (const std::size_t width : {80u, 100u, 120u}) {
        Output out;
        Section &s = out.section("Phiên làm việc");
        s.rows.push_back({Status::Ok, "IM env của session",
                          "GTK=fcitx QT=fcitx XMODIFIERS=@im=fcitx", "đọc từ gnome-shell", 0});
        s.rows.push_back({Status::Warn, "flatpak:org.freedesktop.Platform/25.08",
                          "GTK3 fcitx5 · GTK4 thiếu", std::string("một ghi chú rất dài ").append(60, 'x'), 1});
        out.suggestions.push_back(std::string("một gợi ý dài để ép xuống dòng ").append(50, 'y'));

        for (const auto &line : split_rendered_lines(render_to_string(out, width))) {
            if (line.empty() || !is_table_line(line)) continue;
            check(display_width(line) == width, "every table line is exactly the table width");
        }
    }
}

void test_nested_rows_are_indented() {
    const std::string rendered = render_to_string(nested_output(), 100);
    std::string runtime_line, app_line;
    for (const auto &line : split_rendered_lines(rendered)) {
        if (line.find("snap:gnome-46-2404") != std::string::npos) runtime_line = line;
        if (line.find("chromium") != std::string::npos) app_line = line;
    }
    check(!runtime_line.empty() && !app_line.empty(), "both rows were rendered");

    const auto runtime_col = runtime_line.find("snap:");
    const auto app_col = app_line.find("chromium");
    check(app_col > runtime_col,
          "regression: an app row is indented under its runtime — the indent used to live in "
          "the label, where word wrapping ate it, so the pretty table lost the hierarchy the "
          "whole report is built around while markdown kept it");
}

void test_markdown_marks_nesting() {
    std::ostringstream os;
    render_markdown(nested_output(), os);
    const std::string rendered = os.str();
    check(rendered.find("| └ chromium |") != std::string::npos,
          "markdown marks nesting with a visible character, since leading spaces collapse in a "
          "rendered table");
}

}  // namespace

int main() {
    test_display_width();
    test_pad_to();
    test_hard_break();
    test_wrap();

    test_compositor_identification();
    test_native_ime_advice_is_not_kwin_only();
    test_module_cell();
    test_runtime_label();
    test_runtime_without_qt_is_clean();
    test_missing_module_on_x11_is_a_risk_not_a_failure();
    test_missing_module_on_wayland_is_harmless();
    test_session_env_empty_depends_on_display_server();
    test_session_env_pointing_elsewhere_fails();
    test_qt_and_xmodifiers_are_judged_too();
    test_session_type_comes_from_the_compositor();
    test_session_source_precedence();
    test_deep_probe_status_decides();
    test_pick_compositor_prefers_the_oldest();
    test_select_snap_runtimes();
    test_proc_starttime_parsing();
    test_snap_connections_parsing();
    test_empty_flatpak_override_is_a_setting();
    test_global_flatpak_override_is_not_a_per_app_override();
    test_bus_name_ownership();
    test_foreign_bus_owner_is_reported();
    test_kwin_wayland_advice();
    test_mutter_wayland_needs_ibus_frontend();
    test_snap_fcitx4_module_needs_fcitx4_frontend();
    test_fcitx5_not_running_stops_early();
    test_misrouted_does_not_hide_a_missing_variable();
    test_app_without_a_runtime_is_not_drawn_as_one();
    test_unused_runtime_is_not_reported();
    test_failed_deep_probe_is_not_an_empty_variable();
    test_a_failure_keeps_its_explanation();
    test_successful_deep_probe_keeps_its_explanation();
    test_missing_busctl_is_not_a_stopped_fcitx5();
    test_exit_status_ignores_warnings();

    test_pretty_table_is_rectangular();
    test_nested_rows_are_indented();
    test_markdown_marks_nesting();

    std::cout << "All " << checks_run << " doctor checks passed.\n";
    return 0;
}
