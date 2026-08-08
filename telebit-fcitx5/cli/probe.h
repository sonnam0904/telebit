// Fact gathering for `telebit doctor`.
//
// Everything here only *reads* — it inspects the session, the fcitx5 D-Bus
// names and the on-disk contents of Flatpak runtimes / Snap platform snaps.
// Nothing in this header formats or judges; doctor.cpp turns these structs
// into findings, which keeps the "what is true" and "what to tell the user"
// halves independently testable.
//
// The expensive-but-exact probe (actually entering each sandbox) lives behind
// probe_sandbox_env(): a sandbox launch costs seconds per app, so `doctor`
// only does it under --deep.

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace telebit::doctor {

// Which client-side input-method module a sandbox root ships. The distinction
// that matters is Fcitx5 vs Fcitx4: both register the GTK context id "fcitx",
// so GTK_IM_MODULE=fcitx resolves in either case — but the fcitx4 module
// speaks the legacy protocol and only works while fcitx5's fcitx4frontend
// addon is loaded.
enum class ModuleKind {
    None,
    Fcitx5,
    Fcitx4,
};

// The three toolkit paths an app can take out of a sandbox.
//
// The *_present flags carry the difference between "this runtime ships the
// toolkit but no fcitx module for it" (a real defect) and "this runtime has no
// such toolkit at all" (nothing to fix). Without them a GNOME runtime would be
// reported as broken for missing a Qt plugin it has no use for.
struct ModuleSet {
    ModuleKind gtk3 = ModuleKind::None;
    ModuleKind gtk4 = ModuleKind::None;
    ModuleKind qt = ModuleKind::None;

    bool gtk3_present = false;
    bool gtk4_present = false;
    bool qt_present = false;
};

struct SessionInfo {
    // "wayland", "x11" or "" when it cannot be determined.
    std::string display_server;
    std::string desktop;  // XDG_CURRENT_DESKTOP, verbatim
    // Best-effort compositor identification, used to explain which Wayland
    // input-method protocol is actually available. Inferred from the running
    // process list, not probed from the compositor itself.
    std::string compositor;

    // The IM variables as the *graphical session* sees them. This is the point
    // of reading them here at all: fcitx5-diagnose documents that it can only
    // report the environment of the shell it runs in, which is exactly the
    // environment that is not in question.
    std::string gtk_im_module;
    std::string qt_im_module;
    std::string xmodifiers;
    std::string env_source;  // human-readable origin of the three above

    // Same variables as inherited by this process, kept so doctor can report a
    // disagreement between the two (a terminal started before the last login
    // is the usual cause of "it works everywhere except my shell").
    std::string shell_gtk_im_module;
};

struct Fcitx5Info {
    bool running = false;
    std::string version;

    // busctl was not on PATH, so the frontend flags below could not be read at
    // all. Without this the report cannot tell "fcitx5 is stopped" from "we had
    // no way to look", and it announced the first while the second was true —
    // on a container or a systemd-less host that is a healthy machine being
    // told its input method is dead.
    bool bus_unavailable = false;

    // Well-known bus names, each of which unlocks a different class of client.
    // These are true only when *fcitx5 itself* owns the name: the name existing
    // is not proof, since org.freedesktop.IBus is normally owned by
    // ibus-daemon and org.fcitx.Fcitx-0 by a real fcitx4.
    bool bus_native = false;   // org.fcitx.Fcitx5        — host apps
    bool bus_portal = false;   // org.freedesktop.portal.Fcitx — sandboxed apps
    bool bus_fcitx4 = false;   // org.fcitx.Fcitx-0       — snap's GTK3 module
    bool bus_ibus = false;     // org.freedesktop.IBus    — GNOME Wayland, ibus clients

    // Names fcitx5 would need but that another process holds, formatted as
    // "<name> (<process>)". This is the far more useful diagnosis than a silent
    // false above: a running ibus-daemon owning org.freedesktop.IBus explains
    // exactly why nothing types on GNOME Wayland.
    std::vector<std::string> foreign_bus_owners;

    // Path of the installed Telebit addon, empty when it was not found.
    std::string addon_path;
};

// A Flatpak runtime or a Snap platform snap: the filesystem an app actually
// sees, and therefore the thing that decides whether an IM module exists.
struct SandboxRuntime {
    std::string kind;  // "flatpak" | "snap"
    std::string id;
    ModuleSet modules;
};

struct SandboxApp {
    std::string kind;  // "flatpak" | "snap"
    std::string id;
    std::string runtime_id;  // key into the runtime list; empty when unknown

    // Flatpak only: GTK_IM_MODULE as set by a per-app or global override.
    // Worth surfacing because an override silently beats the session
    // environment, which is how an app can work while its neighbours do not.
    //
    // Optional rather than a plain string because `--env=GTK_IM_MODULE=` is a
    // real and destructive setting: it unsets the variable inside that sandbox.
    // Collapsing it into "" made the one override that breaks an app
    // indistinguishable from having no override at all.
    std::optional<std::string> override_gtk_im_module;

    // Where that override came from. The machine-wide overrides/global file
    // applies to every app, so without this the report called one setting a
    // per-app override N times over and sent the user hunting for per-app
    // overrides that do not exist — while never naming the single file to edit.
    bool override_is_global = false;

    // Snap only: plugs that are declared but not connected, restricted to the
    // ones that carry input-method traffic.
    std::vector<std::string> unconnected_interfaces;

    // --deep only: what `echo $GTK_IM_MODULE` printed from inside the sandbox.
    //
    // deep_probed means the sandbox started and answered; deep_failed means it
    // could not be launched or timed out. Collapsing the two would report a
    // launch error as an empty variable, i.e. diagnose a broken input method
    // where the only broken thing was the probe.
    std::string deep_gtk_im_module;
    bool deep_probed = false;
    bool deep_failed = false;
};

struct Report {
    SessionInfo session;
    Fcitx5Info fcitx5;
    std::vector<SandboxRuntime> runtimes;
    std::vector<SandboxApp> apps;
    bool flatpak_present = false;
    bool snap_present = false;
};

// Everything probe_session() reads off the machine, before any of it is
// interpreted. Splitting the gathering from the deciding is what lets the
// precedence below be tested: the bug worth guarding against was never in
// parsing one of these, it was in choosing the wrong one.
struct SessionSources {
    std::string compositor_comm;      // empty when no compositor was found
    std::string compositor_pid;
    std::string compositor_environ;   // that process's /proc/<pid>/environ blob
    std::string systemd_environment;  // `systemctl --user show-environment`

    // This process's own environment — the caller's, which over SSH or from a
    // terminal left over from an earlier login describes a different session
    // entirely. Last resort only.
    std::string own_session_type;
    std::string own_wayland_display;
    std::string own_display;
    std::string own_desktop;
    std::string own_gtk_im_module;
};

// Applies the precedence: the compositor's own environment, then the systemd
// user manager's, then ours. The display server in particular must come from
// the session being diagnosed, since it decides nearly every rule that follows.
SessionInfo resolve_session(const SessionSources &sources);

// Fills the session fields from one process's /proc/<pid>/environ blob. Returns
// false when the blob carries no usable environment.
bool fill_session_from_environ(const std::string &environ_blob, SessionInfo &info);

// Fallback for sessions with no compositor we recognise: the systemd user
// manager's environment block, which is what environment.d(5) feeds. Returns
// false when it carried no IM variables.
bool fill_session_from_systemd(const std::string &show_environment_output, SessionInfo &info);

// Prefix the --deep script puts in front of the value it read, on a line of its
// own. Sandbox startup writes chatter of its own to stdout and an unset variable
// prints nothing at all, so there is no position in the output that reliably
// holds the answer — only this marker distinguishes "the variable is empty" from
// "the last thing printed happened to be a warning".
inline constexpr const char *kDeepProbeMarker = "__telebit_gtk_im_module__=";

// Records the outcome of one --deep sandbox probe. `status` is the command's
// exit code, or -1 if it never started; 124 is what `timeout` reports.
//
// Separated from the launching so the discriminator can be tested: an empty
// output means nothing on its own, and reading it as an unset variable turned
// our own timeout into a diagnosis of the user's input method.
void record_deep_probe(int status, const std::string &output, SandboxApp &app);

// What one `snap connections --all` listing says about every installed snap.
struct SnapConnections {
    // Snaps that appear on the slot side of a content interface. Not all of
    // them are toolkit platforms — gtk-common-themes provides icons and nothing
    // else — so probe_snap keeps only the ones whose filesystem actually
    // contains GTK or Qt. That test is structural too, and unlike a name-prefix
    // list it cannot go stale when a new platform snap ships.
    std::vector<std::string> providers;
    // Snap -> every content provider it connects to, in listing order.
    std::map<std::string, std::vector<std::string>> providers_of;
    // Snaps plugging desktop/wayland/x11 — the only ones with an IME story.
    std::set<std::string> graphical;
    // Snap -> IM-relevant interfaces it declares but has not connected.
    std::map<std::string, std::vector<std::string>> unconnected;
};

SnapConnections parse_snap_connections(const std::string &output);

// One compositor process found in /proc.
struct CompositorCandidate {
    std::string comm;
    std::string pid;
    unsigned long long starttime = 0;  // clock ticks since boot
};

// Picks the session's compositor out of everything running. Anything nested —
// a test compositor, a second desktop under Xephyr, a leftover from a crash —
// is started from inside the session, so the oldest candidate is the real one.
// Taking whichever /proc listed first could describe a Wayland toy window as
// the session and then advise unsetting the IM variables on a real X11 desktop.
CompositorCandidate pick_compositor(const std::vector<CompositorCandidate> &candidates);

// Which content providers are real toolkit runtimes, and which one each app
// belongs to.
struct SnapRuntimeSelection {
    std::vector<std::string> runtimes;
    std::map<std::string, std::string> runtime_of_app;
};

// A snap can connect to several content providers at once — a platform snap and
// a themes snap, say. Only the one that actually carries GTK or Qt decides
// whether an input-method module exists, so the choice is made from the scanned
// module sets rather than from whichever content row happened to come last.
SnapRuntimeSelection select_snap_runtimes(const SnapConnections &connections,
                                          const std::map<std::string, ModuleSet> &scanned);

// Extracts field 22 (starttime, in clock ticks since boot) from the contents of
// /proc/<pid>/stat, or 0 when it cannot be read.
//
// Field 2 is the executable name in parentheses and may itself contain spaces
// and parentheses, so parsing has to start after the LAST ')' rather than split
// the line from the front.
//
// Used to rank compositor candidates: a nested compositor is started from
// inside the session, so the real one is always the older process.
unsigned long long parse_proc_starttime(const std::string &stat_contents);

// Turns `busctl --user list --acquired` output into the frontend flags, setting
// one only when fcitx5 itself owns the name and recording any name held by
// another process instead.
//
// Exposed for testing rather than kept private: this is where "the name exists,
// so fcitx5 must provide the frontend" lived, and that assumption is wrong the
// moment a real ibus-daemon or fcitx4 is running.
void parse_bus_names(const std::string &busctl_output, Fcitx5Info &info);

// Maps a /proc/<pid>/comm value to the compositor name worth showing — mostly
// identity, except that GNOME's compositor runs as "gnome-shell" and is called
// mutter. An unrecognised name is returned unchanged.
std::string compositor_family(const std::string &comm);

// Whether this compositor implements zwp_input_method_v2, i.e. whether fcitx5
// can attach to it directly. When it can, GTK_IM_MODULE/QT_IM_MODULE are
// unnecessary; when it cannot — mutter drives IMEs over ibus instead, weston
// has no text-input-v3 at all — they still matter. Unknown names answer false,
// because advising someone to unset the variables on a compositor that cannot
// drive the IME would leave them unable to type.
bool compositor_handles_ime_natively(const std::string &family);

SessionInfo probe_session();
Fcitx5Info probe_fcitx5();

// Fills report.runtimes / report.apps for whichever of the two packaging
// systems is installed.
void probe_flatpak(Report &report);
void probe_snap(Report &report);

// --deep: launch a throwaway shell inside every listed app and record the IM
// environment it really receives. Mutates report.apps in place.
void probe_sandbox_env(Report &report);

}  // namespace telebit::doctor
