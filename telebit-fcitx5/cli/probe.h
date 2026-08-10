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

#include <filesystem>
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

// One toolkit as it exists on the host filesystem — the root that applications
// installed from a .deb / .rpm actually see. `present` is what separates "this
// machine has the toolkit but no module to drive it" from "nothing here uses
// that toolkit", exactly as in ModuleSet.
struct HostToolkit {
    bool present = false;
    ModuleKind module = ModuleKind::None;
};

// One application installed natively (.deb / .rpm / pacman), as named by its
// desktop entry, and the toolkits its executable really links.
//
// This exists to turn "GTK4 thiếu" from a risk into a named incident: without it
// the report can say a toolkit has no module but not who that costs, which is
// the same dead end the sandbox section is stuck with — except here the answer is
// readable off the binary.
struct NativeApp {
    std::string name;    // Name= from the desktop entry, else the binary name
    std::string binary;  // resolved executable path, deduplicated

    bool gtk3 = false;
    bool gtk4 = false;
    bool qt5 = false;
    bool qt6 = false;

    // ldd could not answer: a shell wrapper (soffice, most Python apps), or a
    // small launcher that dlopens its toolkit later (Firefox loads GTK through
    // libxul). Such an app is never reported as unaffected — only the positive
    // matches above are ever claimed, and this count is what stops the report
    // from presenting a partial list as a complete one.
    bool toolkit_unknown = false;
};

// How one library root pairs its GTK3 immodule with its immodules.cache.
//
// GTK3 loads an immodule only when the cache lists it; the .so being on disk is
// not enough. A cache written before the module arrived — a hand-copied build, a
// package whose dpkg trigger never ran, an install into /usr/local — silently
// kills input in every native GTK3 app, and no other row in the report shows it.
// GTK4 needs no equivalent check: it scans its immodules directory directly.
enum class Gtk3CacheState {
    NoModule,       // no fcitx immodule here, so there is nothing to register
    Registered,     // the cache lists the module that is installed
    NoCache,        // a module with no cache file at all beside it
    NotRegistered,  // a cache that does not list the installed module
};

// The verdict for one library root, kept whole rather than as separate fields.
//
// The comparison is against the file that is actually installed, not against the
// word "fcitx": a cache still registering the fcitx4-era im-fcitx.so while
// im-fcitx5.so sits unregistered beside it is exactly the stale cache this
// exists to catch, and a substring test called it healthy.
//
// Whole, because the two strings only mean anything together. Aggregating them
// field by field across library roots let `module_file` come from one root and
// `other_fcitx` from another, producing a row that named the same file as both
// the installed module and the stale entry the user should replace.
struct Gtk3Cache {
    Gtk3CacheState state = Gtk3CacheState::NoModule;
    std::string module_file;   // the installed module in that root
    std::string other_fcitx;   // a different fcitx module its cache registers
};

// The host root. Kept as its own struct rather than reusing ModuleSet because
// the two questions differ in one place that matters: a Flatpak runtime or a
// platform snap ships exactly one Qt, while a host routinely carries Qt5 and
// Qt6 side by side from two separately installable packages
// (fcitx5-frontend-qt5 / -qt6 on Debian). Collapsing them into one cell would
// report a missing plugin the user has no way to locate — and call a machine
// broken for lacking a Qt5 plugin when every Qt app on it is Qt6.
struct HostInfo {
    HostToolkit gtk3;
    HostToolkit gtk4;
    HostToolkit qt5;
    HostToolkit qt6;

    // The GTK3 module/cache pairing, worst library root wins. See Gtk3Cache.
    Gtk3Cache gtk3_cache;

    // Library roots that exist on this machine, in scan order. Empty means
    // there was nowhere to look, which is a failed measurement rather than a
    // host without modules — reporting the second would invent a defect.
    std::vector<std::string> lib_roots;

    // Installed applications, filled by probe_native_apps() only when a toolkit
    // gap made the scan worth its cost. Empty therefore means "not looked at",
    // never "no applications installed" — the report has to say which.
    std::vector<NativeApp> apps;
    bool apps_scanned = false;
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
    HostInfo host;
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

// Which Qt a directory that is itself a Qt tree serves, read off its name, or
// nullopt when the name says nothing.
//
// Exposed because it is the fix for a silent misattribution: the scan enumerates
// <libdir>/qt6 as a library base in its own right, so the bare "plugins" layout
// entry turned its Qt6 plugin into the Qt5 answer, and a machine with no Qt5
// plugin at all was reported healthy.
std::optional<bool> qt6_from_directory_name(const std::string &name);

// Keeps the worse of two library roots' GTK3 pairings.
//
// The aggregation has to prefer the *problem*: a healthy pairing in one root says
// nothing about a broken one in another, and OR-ing "healthy" across roots let a
// good i386 cache declare a stale x86_64 cache fine — masking the failure the
// check exists for. Whole records, never field by field, so the installed module
// and the stale entry always describe the same root.
Gtk3Cache worse_gtk3_cache(const Gtk3Cache &a, const Gtk3Cache &b);

// What a GTK3 immodules.cache says about the module that is installed.
struct CacheRegistration {
    bool lists_module = false;   // the cache registers `module_file` itself
    std::string other_fcitx;     // a *different* fcitx module it registers instead
};

// Reads one immodules.cache against the module file found on disk. Only the
// quoted entries count: the header gtk-query-immodules writes names the generator
// and every directory it searched, and matching there would call a cache that
// registers nothing at all healthy.
//
// `other_fcitx` is the useful half of the diagnosis. A cache holding
// im-fcitx.so while im-fcitx5.so is what is installed is not "no fcitx in the
// cache" — it is a cache from the previous package, and naming the file it still
// points at is what turns the row into something the user can act on.
//
// Exposed for testing because the failure this guards against is invisible from
// the outside: the module file is present, the cache is present, and GTK3 still
// loads nothing of ours.
CacheRegistration immodules_cache_registration(const std::string &cache_contents,
                                               const std::string &module_file);

SessionInfo probe_session();
Fcitx5Info probe_fcitx5();

// The host library roots, for applications installed natively (.deb / .rpm /
// pacman) rather than into a sandbox.
HostInfo probe_host();

// The [Desktop Entry] group of one .desktop file, reduced to what decides
// whether the entry names a real application and which binary it starts.
struct DesktopEntry {
    std::string name;
    // First Exec token with the field codes (%U, %f) and any `env VAR=x` prefix
    // removed. Still unresolved: it may be a bare command name.
    std::string exec_binary;
    bool is_application = false;
    // NoDisplay=true / Hidden=true. These are MIME handlers and settings panels,
    // not applications the user launches, and counting them would inflate every
    // number in the report.
    bool hidden = false;
};

// Only the [Desktop Entry] group is read. A .desktop file also carries
// [Desktop Action *] groups, each with an Exec of its own, and taking the last
// Exec in the file would start the wrong binary.
DesktopEntry parse_desktop_entry(const std::string &contents);

// Whether a binary is a launcher rather than the program: `ldd` on an
// interpreter answers for the interpreter, which links no toolkit, and recording
// that as "this application uses no GTK or Qt" is the one answer that must never
// be invented — it is what lets a real toolkit gap be dismissed as harmless.
bool is_interpreter(const std::string &binary);

// Whether doctor is willing to hand this binary to `ldd`. ldd resolves
// dependencies by running the target's ELF interpreter, so it is not a read, and
// the names come from desktop entries in user-writable directories resolved
// through the caller's PATH. Only binaries under the system prefixes are
// measured; the rest are reported as applications whose toolkit is unknown.
bool under_system_prefix(const std::filesystem::path &binary);

// Records the output of one `ldd <binary>...` run onto apps, matched by path.
// `measured` is the argument list that run was given, in order.
//
// With several arguments ldd prefixes each file with "<path>:", and a file it
// cannot read gets that header and nothing else — the explanation goes to stderr,
// which every probe here drops. An empty section is therefore how a wrapper
// script looks, and it must land in toolkit_unknown rather than being read as an
// application that uses no toolkit at all.
//
// With exactly one argument ldd prints no header, which is why `measured` is
// passed at all: the lone answer has to be attributed by argument, since `apps`
// also holds applications that were deliberately never measured.
void record_ldd_output(const std::string &output, const std::vector<std::string> &measured,
                       std::vector<NativeApp> &apps);

// Whether some toolkit on the host has no fcitx module for it. This is the only
// condition under which the application scan changes any reading, so it is what
// gates paying for it.
bool host_has_toolkit_gap(const HostInfo &host);

// Fills host.apps: every desktop entry's binary, and the toolkits it links.
// One `ldd` call covers the whole list, so the cost is a single subprocess
// rather than one per application.
void probe_native_apps(HostInfo &host);

// Fills report.runtimes / report.apps for whichever of the two packaging
// systems is installed.
void probe_flatpak(Report &report);
void probe_snap(Report &report);

// --deep: launch a throwaway shell inside every listed app and record the IM
// environment it really receives. Mutates report.apps in place.
void probe_sandbox_env(Report &report);

}  // namespace telebit::doctor
