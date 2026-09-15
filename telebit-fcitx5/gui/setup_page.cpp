#include "setup_page.h"

// getpid(), for the helper that waits on this process before relaunching it.
// Included explicitly rather than relied on through glib's headers, which is
// not a promise glib makes.
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include "fcitx5_bus.h"
#include "updates.h"
#include "widgets.h"

namespace telebit::setup {

// One switch row bound to one boolean option of the addon configuration.
struct OptionRow {
    std::string key;  // the fcitx5 configuration key, e.g. "SpellCheckRestore"
    GtkWidget *toggle = nullptr;
};

// What the single button in the version row does right now. One button with a
// mode rather than four buttons that are hidden most of the time: the row has
// space for one control, and which one it is depends entirely on what the
// release check found.
enum class UpdateAction {
    None,
    Upgrade,        // run the package manager under pkexec
    OpenReleases,   // no package manager can help; hand it to the browser
    Relaunch,       // the binary on disk is newer than this process
    CancelRelaunch  // a countdown is running and this aborts it
};

struct SetupPage {
    // Held for the duration of the version check and of an upgrade, so closing
    // the window mid-call does not return from g_application_run and run static
    // destructors while a worker thread is still inside curl.
    GtkApplication *app = nullptr;
    GtkWidget *root = nullptr;
    GtkWidget *hero = nullptr;
    GtkWidget *badge = nullptr;
    GtkWidget *hero_title = nullptr;
    GtkWidget *hero_detail = nullptr;
    GtkWidget *enable_button = nullptr;

    GtkWidget *telex_radio = nullptr;
    GtkWidget *vni_radio = nullptr;
    std::vector<OptionRow> options;

    GtkWidget *restart_button = nullptr;

    // The version row: a title that states what is installed, a note that says
    // what was found upstream, and a button that only appears when there is
    // something to press.
    GtkWidget *version_title = nullptr;
    GtkWidget *version_note = nullptr;
    GtkWidget *update_button = nullptr;
    GtkWidget *update_spinner = nullptr;
    update::Check latest;

    UpdateAction update_action = UpdateAction::None;

    // The countdown that runs after a successful upgrade, before this window
    // replaces itself. Non-zero timer id means one is in flight, which is also
    // what makes the button mean "cancel".
    guint relaunch_timer = 0;
    int relaunch_left = 0;

    // Set while the widgets are being filled in from fcitx5, so the handlers
    // that write back do not fire for values they just read.
    bool loading = false;

    // The window is gone but a worker thread is still on its way back. Same
    // role as StatusPage::closed.
    bool closed = false;
};

namespace {

constexpr const char *kVniKey = "VNIMode";

void set_hero(SetupPage *page, const char *state, const char *icon_name, const std::string &title,
              const std::string &detail) {
    for (const char *css_class : {"ok", "fail", "busy"}) {
        gtk_widget_remove_css_class(page->hero, css_class);
        gtk_widget_remove_css_class(page->badge, css_class);
    }
    gtk_widget_add_css_class(page->hero, state);
    gtk_widget_add_css_class(page->badge, state);
    gtk_image_set_from_icon_name(GTK_IMAGE(page->badge), icon_name);
    gtk_label_set_text(GTK_LABEL(page->hero_title), title.c_str());
    gtk_label_set_text(GTK_LABEL(page->hero_detail), detail.c_str());
}

// Returns gboolean because GtkSwitch::state-set does. A GtkSwitch carries two
// properties: `active` is what the user just flipped, `state` is what gets
// drawn, and the default handler copies the first into the second unless the
// handler returns TRUE.
//
// This was declared void, so the marshaller read an undefined return value.
// Whenever that read as FALSE on the failure path, the default handler ran
// after the rollback and painted `state` with the value fcitx5 had just
// refused — the switch then showed a setting that was never saved.
gboolean on_option_toggled(GtkSwitch *toggle, gboolean state, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    // FALSE, not TRUE: the rollback below re-enters here with `loading` set, and
    // that pass needs the default handler to pull `state` back with `active`.
    if (page->loading) return FALSE;

    // Find which option this switch belongs to.
    for (const auto &row : page->options) {
        if (row.toggle != GTK_WIDGET(toggle)) continue;
        if (!bus::write_bool_option(row.key, state != 0)) {
            // Put it back rather than show a state fcitx5 does not have.
            page->loading = true;
            gtk_switch_set_active(toggle, state == 0);
            page->loading = false;
            set_hero(page, "fail", "dialog-error-symbolic", "Không lưu được cấu hình",
                     "fcitx5 không nhận lệnh. Thử khởi động lại fcitx5 rồi mở lại cửa sổ này.");
            // TRUE so this emission's default handler cannot undo the rollback.
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

void on_layout_toggled(GtkCheckButton *button, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (page->loading) return;
    // Only react to the button that became active, or every change would write
    // twice — once for the button turning off and once for the one turning on.
    if (gtk_check_button_get_active(button) == 0) return;

    const bool vni = GTK_WIDGET(button) == page->vni_radio;
    if (!bus::write_bool_option(kVniKey, vni)) {
        set_hero(page, "fail", "dialog-error-symbolic", "Không lưu được kiểu gõ",
                 "fcitx5 không nhận lệnh. Thử khởi động lại fcitx5 rồi mở lại cửa sổ này.");
    }
}

void on_enable_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    std::string error;
    if (bus::enable_input_method(&error)) {
        setup_page_reload(page);
    } else {
        set_hero(page, "fail", "dialog-error-symbolic", "Chưa bật được Telebit", error);
    }
}

void on_configure_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (bus::configure_addon()) return;

    // fcitx5 could not open it (usually because fcitx5-configtool is not
    // installed); try the binary directly before giving up, so a working
    // configtool with a wedged fcitx5 still opens.
    const char *argv[] = {"fcitx5-configtool", nullptr};
    if (g_spawn_async(nullptr, const_cast<char **>(argv), nullptr, G_SPAWN_SEARCH_PATH, nullptr,
                      nullptr, nullptr, nullptr) == 0) {
        set_hero(page, "fail", "dialog-error-symbolic", "Không mở được cấu hình fcitx5",
                 "Cài gói fcitx5-configtool để mở được trang cấu hình đầy đủ.");
    }
}

// fcitx5 drops its bus name while it restarts, so everything asked in the next
// second or so fails. This used to be a second button ("Đọc lại") that the user
// pressed once they judged it was back — which is a question the window can
// answer for itself.
constexpr int kRestartPollMs = 400;
constexpr int kRestartPollAttempts = 25;  // ~10 seconds

// Restart() replies before fcitx5 has dropped its bus name, so the first sample
// can catch the process that is on its way out. Reloading from it would read
// the state of something about to die and then never look again, which is worse
// than the stale page this whole flow exists to fix. So "back up" is only
// believed once the name has been seen gone — or once enough time has passed
// that a restart too quick to sample is the likelier explanation.
constexpr int kRestartSettleMs = 2000;

struct RestartPoll {
    SetupPage *page;
    int attempts = 0;
    bool seen_down = false;
};

gboolean poll_restarted(gpointer data) {
    auto *poll = static_cast<RestartPoll *>(data);
    SetupPage *page = poll->page;
    if (page->closed) {
        delete poll;
        return G_SOURCE_REMOVE;
    }

    const bool up = bus::running();
    if (!up) {
        poll->seen_down = true;
    }
    const bool settled = poll->seen_down || poll->attempts * kRestartPollMs >= kRestartSettleMs;

    if (up && settled) {
        gtk_widget_set_sensitive(page->restart_button, TRUE);
        setup_page_reload(page);
        delete poll;
        return G_SOURCE_REMOVE;
    }

    if (++poll->attempts >= kRestartPollAttempts) {
        gtk_widget_set_sensitive(page->restart_button, TRUE);
        set_hero(page, "fail", "dialog-error-symbolic", "fcitx5 chưa quay lại",
                 "Đã đợi 10 giây mà fcitx5 vẫn chưa trả lời. Thử chạy `fcitx5 -r` trong "
                 "terminal.");
        delete poll;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

void on_restart_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (!bus::restart()) {
        set_hero(page, "fail", "dialog-error-symbolic", "Không khởi động lại được fcitx5",
                 "Thử chạy `fcitx5 -r` trong terminal.");
        return;
    }
    gtk_widget_set_sensitive(page->restart_button, FALSE);
    set_hero(page, "busy", icon_or("content-loading-symbolic", "system-search-symbolic"),
             "Đang khởi động lại fcitx5…", "Cửa sổ sẽ tự đọc lại trạng thái khi fcitx5 quay lại.");
    g_timeout_add(kRestartPollMs, poll_restarted, new RestartPoll{page, 0});
}

// ---------------------------------------------------------------------------
// Version check

void show_version(SetupPage *page);

struct VersionMessage {
    SetupPage *page;
    update::Check result;
};

gboolean deliver_version(gpointer data) {
    auto *message = static_cast<VersionMessage *>(data);
    SetupPage *page = message->page;
    if (!page->closed) {
        page->latest = message->result;
        gtk_spinner_stop(GTK_SPINNER(page->update_spinner));
        gtk_widget_set_visible(page->update_spinner, FALSE);
        show_version(page);
    }
    delete message;
    // Balances the hold in start_version_check.
    g_application_release(G_APPLICATION(page->app));
    return G_SOURCE_REMOVE;
}

gpointer run_version_check(gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    g_idle_add(deliver_version, new VersionMessage{page, update::check()});
    return nullptr;
}

void start_version_check(SetupPage *page) {
    gtk_widget_set_visible(page->update_spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(page->update_spinner));
    gtk_widget_set_visible(page->update_button, FALSE);
    gtk_label_set_text(GTK_LABEL(page->version_note), "Đang hỏi GitHub xem có bản mới…");

    g_application_hold(G_APPLICATION(page->app));
    // Detached, like the doctor probe: a curl call with a 6-second timeout has
    // nothing to cancel, and `closed` keeps a late answer from drawing into a
    // window that is gone.
    GThread *thread = g_thread_new("telebit-version", run_version_check, page);
    g_thread_unref(thread);
}

// ---------------------------------------------------------------------------
// Replacing this window with the version that was just installed

// Seconds of grace before the window replaces itself, and the button that
// aborts it. An upgrade the user asked for still should not make a window
// vanish without warning.
constexpr int kRelaunchDelaySeconds = 3;

// How long the helper waits for this process to go away before giving up, in
// tenths of a second. Bounded so a window that somehow never exits leaves a
// shell spinning for 10 seconds rather than forever.
constexpr const char *kRelaunchWaitTenths = "100";

// Where the replacement comes from. Deliberately NOT /proc/self/exe: after an
// in-place upgrade that resolves to the unlinked inode this process is still
// running ("/usr/bin/telebit-setup (deleted)"), which would relaunch the very
// binary being replaced. PATH gives the file that is on disk now, which is
// exactly what the package manager just wrote.
std::string replacement_binary() {
    char *found = g_find_program_in_path("telebit-setup");
    std::string path = found != nullptr ? found : "";
    g_free(found);
    return path;
}

// Spawns a helper that waits for THIS process to exit and only then starts the
// new one, and returns false without spawning anything if it cannot.
//
// The order is the whole point. GtkApplication is single-instance: a second
// telebit-setup started while this one still owns the bus name registers as a
// remote instance, forwards an "activate" to us, and exits immediately. Spawn
// first and quit second, and the result is no window at all — which is exactly
// what happens when you launch a second copy by hand.
bool spawn_replacement(SetupPage *page) {
    const std::string binary = replacement_binary();
    if (binary.empty()) return false;

    // Quoted for the shell: the path comes from PATH rather than from user
    // input, but a prefix with a space in it is not a reason to misbehave.
    char *quoted = g_shell_quote(binary.c_str());
    std::string script = "i=0; while kill -0 " + std::to_string(getpid()) +
                         " 2>/dev/null && [ $i -lt " + kRelaunchWaitTenths +
                         " ]; do i=$((i+1)); sleep 0.1; done; exec " + quoted;
    g_free(quoted);

    const char *argv[] = {"sh", "-c", script.c_str(), nullptr};
    GError *error = nullptr;
    const gboolean ok =
        g_spawn_async(nullptr, const_cast<char **>(argv), nullptr,
                      static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH), nullptr, nullptr, nullptr,
                      &error);
    if (ok == 0) {
        gtk_label_set_text(GTK_LABEL(page->version_note),
                           error != nullptr && error->message != nullptr
                               ? error->message
                               : "Không mở lại được cửa sổ. Đóng và mở lại telebit-setup.");
        if (error != nullptr) g_error_free(error);
        return false;
    }
    return true;
}

void cancel_relaunch(SetupPage *page) {
    if (page->relaunch_timer != 0) {
        g_source_remove(page->relaunch_timer);
        page->relaunch_timer = 0;
    }
    page->update_action = UpdateAction::Relaunch;
    gtk_button_set_label(GTK_BUTTON(page->update_button), "Mở lại");
    gtk_label_set_text(GTK_LABEL(page->version_note),
                       "Đã cập nhật. Cửa sổ này vẫn đang chạy bản cũ — bấm “Mở lại” khi bạn "
                       "sẵn sàng.");
}

gboolean tick_relaunch(gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    if (page->closed) {
        page->relaunch_timer = 0;
        return G_SOURCE_REMOVE;
    }

    if (--page->relaunch_left > 0) {
        gtk_label_set_text(GTK_LABEL(page->version_note),
                           ("Đã cập nhật. Mở lại cửa sổ sau " +
                            std::to_string(page->relaunch_left) + "…")
                               .c_str());
        return G_SOURCE_CONTINUE;
    }

    page->relaunch_timer = 0;
    // Only quit once the helper is confirmed running, so a failed spawn leaves
    // the user with this window and an explanation instead of nothing at all.
    if (!spawn_replacement(page)) {
        cancel_relaunch(page);
        return G_SOURCE_REMOVE;
    }
    g_application_quit(G_APPLICATION(page->app));
    return G_SOURCE_REMOVE;
}

void start_relaunch_countdown(SetupPage *page) {
    // Nothing to relaunch into: say what is true and stop there.
    if (replacement_binary().empty()) {
        gtk_label_set_text(
            GTK_LABEL(page->version_note),
            "Đã cập nhật. Bấm “Khởi động lại” bên trên để fcitx5 nạp addon mới, rồi đóng và mở "
            "lại cửa sổ này");
        gtk_widget_set_visible(page->update_button, FALSE);
        page->update_action = UpdateAction::None;
        return;
    }

    page->relaunch_left = kRelaunchDelaySeconds;
    page->update_action = UpdateAction::CancelRelaunch;
    gtk_button_set_label(GTK_BUTTON(page->update_button), "Huỷ");
    gtk_widget_set_visible(page->update_button, TRUE);
    gtk_widget_set_sensitive(page->update_button, TRUE);
    gtk_label_set_text(
        GTK_LABEL(page->version_note),
        ("Đã cập nhật. Mở lại cửa sổ sau " + std::to_string(page->relaunch_left) + "…").c_str());
    page->relaunch_timer = g_timeout_add_seconds(1, tick_relaunch, page);
}

// ---------------------------------------------------------------------------
// The upgrade itself

void on_upgrade_finished(GObject *source, GAsyncResult *result, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);

    GError *error = nullptr;
    char *output = nullptr;
    const gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result, &output,
                                                             nullptr, &error);
    const gboolean succeeded =
        ok != 0 && g_subprocess_get_successful(G_SUBPROCESS(source)) != 0;
    const int status = g_subprocess_get_exit_status(G_SUBPROCESS(source));

    if (!page->closed) {
        gtk_spinner_stop(GTK_SPINNER(page->update_spinner));
        gtk_widget_set_visible(page->update_spinner, FALSE);
        gtk_widget_set_sensitive(page->update_button, TRUE);

        if (succeeded != 0) {
            // The upgrade replaced this very binary on disk, so the window is
            // now the old version and no button on this page can turn it into
            // the new one. It replaces itself instead — after a countdown,
            // because a window that disappears on its own is alarming even when
            // it was asked for.
            start_relaunch_countdown(page);
        } else if (status == 126 || status == 127) {
            // pkexec's own exit codes: the authorisation dialog was dismissed,
            // or the user is not allowed to run it at all. Neither is a failed
            // upgrade, and calling it one would send the user hunting for a
            // problem that does not exist.
            gtk_label_set_text(GTK_LABEL(page->version_note),
                               "Chưa cập nhật — cửa sổ xác thực bị huỷ hoặc bị từ chối.");
        } else {
            std::string note = "Cập nhật thất bại. Chạy trong terminal để xem lỗi: sudo " +
                               page->latest.command;
            if (error != nullptr && error->message != nullptr) {
                note += " (" + std::string(error->message) + ")";
            }
            gtk_label_set_text(GTK_LABEL(page->version_note), note.c_str());
        }
    }

    g_free(output);
    if (error != nullptr) g_error_free(error);
    g_object_unref(source);
    g_application_release(G_APPLICATION(page->app));
}

void on_update_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<SetupPage *>(data);
    const update::Check &check = page->latest;

    switch (page->update_action) {
        case UpdateAction::CancelRelaunch:
            cancel_relaunch(page);
            return;
        case UpdateAction::Relaunch:
            if (spawn_replacement(page)) g_application_quit(G_APPLICATION(page->app));
            return;
        case UpdateAction::OpenReleases:
            // Nothing a package manager on this machine can do: hand it to the
            // browser rather than run a command that would report success and
            // change nothing.
            gtk_show_uri(GTK_WINDOW(gtk_widget_get_root(page->update_button)),
                         update::releases_url(), GDK_CURRENT_TIME);
            return;
        case UpdateAction::None:
            return;
        case UpdateAction::Upgrade:
            break;
    }

    const char *manager = check.method == update::Method::Dnf ? "dnf" : "apt-get";
    const char *verb = check.method == update::Method::Dnf ? "upgrade" : "install";
    const char *only_upgrade =
        check.method == update::Method::Dnf ? "telebit-fcitx5" : "--only-upgrade";

    // pkexec rather than a setuid helper or a polkit action of Telebit's own:
    // the privileged step is "run the distribution's package manager", which
    // the distribution already has a policy for.
    const char *argv[] = {"pkexec", manager, verb, "-y", only_upgrade, "telebit-fcitx5", nullptr};
    // dnf's form has one argument fewer — the package name is already in place.
    if (check.method == update::Method::Dnf) argv[5] = nullptr;

    GError *error = nullptr;
    GSubprocess *process = g_subprocess_newv(
        argv, static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                            G_SUBPROCESS_FLAGS_STDERR_MERGE),
        &error);
    if (process == nullptr) {
        gtk_label_set_text(GTK_LABEL(page->version_note),
                           error != nullptr && error->message != nullptr
                               ? error->message
                               : "Không chạy được pkexec. Cài gói polkit rồi thử lại.");
        if (error != nullptr) g_error_free(error);
        return;
    }

    gtk_widget_set_sensitive(page->update_button, FALSE);
    gtk_widget_set_visible(page->update_spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(page->update_spinner));
    gtk_label_set_text(GTK_LABEL(page->version_note),
                       "Đang cập nhật… xác nhận ở cửa sổ xin quyền rồi đợi một lát.");

    g_application_hold(G_APPLICATION(page->app));
    g_subprocess_communicate_utf8_async(process, nullptr, nullptr, on_upgrade_finished, page);
}

// Appended to whatever else the row has to say. An upgrade replaces the binary
// under a running window, so this process can be the *old* Telebit while the
// machine already has the new one — after an upgrade done here, but also after
// one done in a terminal while this window sat open.
std::string stale_window_note(const update::Check &check) {
    if (!check.window_is_stale) return {};
    return " Cửa sổ này vẫn đang chạy bản " + update::current_version() + ".";
}

// Offers the relaunch on a row that has no upgrade to offer. Returns true when
// it took the button, so the caller leaves it alone.
bool offer_relaunch(SetupPage *page) {
    if (!page->latest.window_is_stale || replacement_binary().empty()) {
        gtk_widget_set_visible(page->update_button, FALSE);
        page->update_action = UpdateAction::None;
        return false;
    }
    // No countdown here: nothing just happened in this window, so closing it
    // unprompted would be pure surprise. The upgrade path starts a countdown
    // because the user asked for the upgrade a moment earlier.
    page->update_action = UpdateAction::Relaunch;
    gtk_button_set_label(GTK_BUTTON(page->update_button), "Mở lại");
    gtk_widget_set_visible(page->update_button, TRUE);
    return true;
}

void show_version(SetupPage *page) {
    const update::Check &check = page->latest;
    // A countdown owns the row until it finishes or is cancelled; a background
    // check landing mid-count must not relabel the button out from under it.
    if (page->relaunch_timer != 0) return;
    // The package database's answer, not this process's compiled-in constant:
    // the row describes the Telebit installed on the machine.
    gtk_label_set_text(GTK_LABEL(page->version_title),
                       ("Telebit " + update::effective_version(check)).c_str());

    if (check.latest.empty()) {
        // Offline, rate-limited, or an unreadable answer. None of those is
        // evidence that this is the newest version, so the row must not say so.
        const std::string note =
            (check.error.empty() ? std::string("Chưa kiểm tra được bản mới.") : check.error) +
            stale_window_note(check);
        gtk_label_set_text(GTK_LABEL(page->version_note), note.c_str());
        offer_relaunch(page);
        return;
    }

    if (!check.newer) {
        const std::string note = "Đang dùng bản mới nhất." + stale_window_note(check);
        gtk_label_set_text(GTK_LABEL(page->version_note), note.c_str());
        offer_relaunch(page);
        return;
    }

    if (check.upgradable) {
        page->update_action = UpdateAction::Upgrade;
        // Labelled with what the package manager will install, not with what
        // GitHub announced — the two differ whenever the repo index is behind.
        const std::string installing = update::version_core(check.candidate);
        gtk_button_set_label(GTK_BUTTON(page->update_button),
                             ("Cập nhật lên " + installing).c_str());
        std::string note = "Sẽ chạy: " + check.command;
        if (installing != check.latest) {
            note += ". GitHub đã có " + check.latest +
                    ", nhưng repo trên máy mới tới " + installing + ".";
        }
        gtk_label_set_text(GTK_LABEL(page->version_note), note.c_str());
    } else {
        // GitHub has it, this machine's package manager does not — either the
        // repo index is stale, or Telebit was not installed from a repo at all.
        // Sending the user to the releases page is the only honest action left.
        page->update_action = UpdateAction::OpenReleases;
        gtk_button_set_label(GTK_BUTTON(page->update_button), ("Xem bản " + check.latest).c_str());
        gtk_label_set_text(
            GTK_LABEL(page->version_note),
            check.method == update::Method::Unknown
                ? "Bản đang chạy không do apt/dnf quản lý, nên cập nhật theo đúng cách bạn đã cài."
                : "Repo trên máy chưa thấy bản này. Chạy `sudo apt update` rồi mở lại cửa sổ.");
    }
    gtk_widget_set_visible(page->update_button, TRUE);
}

GtkWidget *build_hero(SetupPage *page) {
    page->hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_add_css_class(page->hero, "tb-hero");

    page->badge = gtk_image_new_from_icon_name("emblem-ok-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(page->badge), 24);
    gtk_widget_add_css_class(page->badge, "tb-badge");
    gtk_widget_set_valign(page->badge, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(page->hero), page->badge);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    page->hero_title = make_label("", "tb-hero-title", false);
    page->hero_detail = make_label("", "tb-hero-detail", true);
    gtk_box_append(GTK_BOX(text), page->hero_title);
    gtk_box_append(GTK_BOX(text), page->hero_detail);
    gtk_box_append(GTK_BOX(page->hero), text);

    page->enable_button = gtk_button_new_with_label("Bật Telebit");
    gtk_widget_add_css_class(page->enable_button, "tb-pill");
    gtk_widget_add_css_class(page->enable_button, "suggested-action");
    gtk_widget_set_valign(page->enable_button, GTK_ALIGN_CENTER);
    g_signal_connect(page->enable_button, "clicked", G_CALLBACK(on_enable_clicked), page);
    gtk_box_append(GTK_BOX(page->hero), page->enable_button);

    return page->hero;
}

GtkWidget *build_layout_card(SetupPage *page) {
    GtkWidget *card = make_card();

    page->telex_radio = gtk_check_button_new();
    page->vni_radio = gtk_check_button_new();
    // A GtkCheckButton group is GTK4's radio group; the old GtkRadioButton is
    // gone.
    gtk_check_button_set_group(GTK_CHECK_BUTTON(page->vni_radio),
                               GTK_CHECK_BUTTON(page->telex_radio));
    g_signal_connect(page->telex_radio, "toggled", G_CALLBACK(on_layout_toggled), page);
    g_signal_connect(page->vni_radio, "toggled", G_CALLBACK(on_layout_toggled), page);

    card_append(card, make_setting_row("Telex", "aa = â · dd = đ · as = á · af = à",
                                       page->telex_radio));
    card_append(card, make_setting_row("VNI", "a6 = â · d9 = đ · a1 = á · a2 = à",
                                       page->vni_radio));
    return card;
}

// Title, note and configuration key for each switch, in the order they appear.
struct OptionSpec {
    const char *key;
    const char *title;
    const char *note;
};

const OptionSpec kOptionSpecs[] = {
    {"SpellCheckRestore", "Kiểm tra chính tả",
     "Từ không phải tiếng Việt được trả lại đúng phím đã gõ"},
    {"ModernToneStyle", "Đặt dấu kiểu mới", "hoà, khoẻ, thuý — thay vì hòa, khỏe, thúy"},
    {"AutoCapitalizeSentence", "Tự viết hoa đầu câu", "Sau dấu . ? ! rồi dấu cách hoặc Enter"},
    {"DirectCommitRollback", "Gõ trực tiếp, không gạch chân",
     "Nhanh và tự nhiên hơn, nhưng cần ứng dụng hỗ trợ surrounding text"},
    {"AIEnabled", "Trợ lý AI",
     "Ctrl+Shift+Space mở ô nhập yêu cầu. Cần cấu hình API key qua biến môi trường"},
};

GtkWidget *build_options_card(SetupPage *page) {
    GtkWidget *card = make_card();
    for (const auto &spec : kOptionSpecs) {
        GtkWidget *toggle = gtk_switch_new();
        g_signal_connect(toggle, "state-set", G_CALLBACK(on_option_toggled), page);
        page->options.push_back(OptionRow{spec.key, toggle});
        card_append(card, make_setting_row(spec.title, spec.note, toggle));
    }
    return card;
}

GtkWidget *build_version_row(SetupPage *page) {
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    page->update_spinner = gtk_spinner_new();
    gtk_widget_set_valign(page->update_spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(page->update_spinner, FALSE);
    gtk_box_append(GTK_BOX(controls), page->update_spinner);

    // Label left empty: it names a version number nobody knows yet, and the
    // button stays hidden until there is one to name.
    page->update_button = gtk_button_new_with_label("");
    gtk_widget_add_css_class(page->update_button, "tb-pill");
    gtk_widget_add_css_class(page->update_button, "suggested-action");
    gtk_widget_set_visible(page->update_button, FALSE);
    g_signal_connect(page->update_button, "clicked", G_CALLBACK(on_update_clicked), page);
    gtk_box_append(GTK_BOX(controls), page->update_button);

    return make_setting_row_labels("Telebit " + update::current_version(), "", controls,
                                   &page->version_title, &page->version_note);
}

GtkWidget *build_advanced_card(SetupPage *page) {
    GtkWidget *card = make_card();

    GtkWidget *configure = gtk_button_new_with_label("Mở fcitx5-configtool");
    gtk_widget_add_css_class(configure, "tb-pill");
    g_signal_connect(configure, "clicked", G_CALLBACK(on_configure_clicked), page);
    card_append(card, make_setting_row("Phím tắt bật/tắt tiếng Việt và mở ô AI",
                                       "Mục duy nhất còn lại trong trang cấu hình của fcitx5",
                                       configure));

    // One row, not two. Restarting and re-reading were always the same
    // intention: the old "Đọc lại" button existed only because the restart left
    // the window showing a state it could no longer verify.
    page->restart_button = gtk_button_new_with_label("Khởi động lại");
    gtk_widget_add_css_class(page->restart_button, "tb-pill");
    g_signal_connect(page->restart_button, "clicked", G_CALLBACK(on_restart_clicked), page);
    card_append(card,
                make_setting_row("Khởi động lại fcitx5",
                                 "Cần sau khi đổi frontend hoặc cài thêm addon. Xong sẽ tự đọc "
                                 "lại trạng thái.",
                                 page->restart_button));

    card_append(card, build_version_row(page));
    return card;
}

}  // namespace

SetupPage *setup_page_new(GtkApplication *app) {
    auto *page = new SetupPage();
    page->app = app;

    page->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(page->root), build_hero(page));

    gtk_box_append(GTK_BOX(page->root), make_section_title("Kiểu gõ"));
    gtk_box_append(GTK_BOX(page->root), build_layout_card(page));

    gtk_box_append(GTK_BOX(page->root), make_section_title("Cách gõ"));
    gtk_box_append(GTK_BOX(page->root), build_options_card(page));

    gtk_box_append(GTK_BOX(page->root), make_section_title("Khác"));
    gtk_box_append(GTK_BOX(page->root), build_advanced_card(page));

    setup_page_reload(page);
    // Started once per window, not per tab switch: the user asked for a check
    // "each time this window opens", and GitHub rate-limits unauthenticated
    // callers to 60 requests an hour per address.
    start_version_check(page);
    return page;
}

GtkWidget *setup_page_widget(SetupPage *page) { return page->root; }

void setup_page_closed(SetupPage *page) { page->closed = true; }

void setup_page_reload(SetupPage *page) {
    const bool running = bus::running();
    const bool enabled = running && bus::input_method_enabled();

    if (!running) {
        set_hero(page, "fail", "dialog-error-symbolic", "fcitx5 chưa chạy",
                 "Khởi động fcitx5 (hoặc đăng xuất rồi đăng nhập lại) rồi bấm “Đọc lại”.");
    } else if (enabled) {
        set_hero(page, "ok", "emblem-ok-symbolic", "Telebit đang bật",
                 "Telebit đã nằm trong nhóm input method của fcitx5. Chuyển bộ gõ bằng "
                 "Ctrl+Space.");
    } else {
        set_hero(page, "fail", "dialog-warning-symbolic", "Telebit chưa được bật",
                 "Addon đã cài nhưng chưa nằm trong nhóm input method của fcitx5, nên chưa "
                 "gõ được tiếng Việt.");
    }
    gtk_widget_set_visible(page->enable_button, running && !enabled);

    // Nothing below the hero can be honoured without fcitx5, so it is disabled
    // rather than showing values that are not in effect anywhere.
    const std::map<std::string, bool> values = running ? bus::read_bool_options()
                                                       : std::map<std::string, bool>{};

    page->loading = true;
    const auto vni = values.find(kVniKey);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(page->vni_radio),
                                vni != values.end() && vni->second);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(page->telex_radio),
                                vni == values.end() || !vni->second);
    gtk_widget_set_sensitive(page->telex_radio, running);
    gtk_widget_set_sensitive(page->vni_radio, running);

    for (const auto &row : page->options) {
        const auto found = values.find(row.key);
        gtk_switch_set_active(GTK_SWITCH(row.toggle), found != values.end() && found->second);
        gtk_widget_set_sensitive(row.toggle, running);
    }
    page->loading = false;
}

}  // namespace telebit::setup
