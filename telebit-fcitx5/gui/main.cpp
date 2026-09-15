// telebit-setup — the window Telebit needs in order to be an application.
//
// It exists for two reasons, in this order:
//
//   1. someone who installed Telebit and cannot type Vietnamese needs both
//      halves of the answer without opening a terminal: is it switched on
//      (setup_page), and does it actually reach applications (status_page);
//   2. a software centre only lists AppStream `desktop-application`
//      components — COSMIC Store drops every other kind when it reads the
//      DEP-11 catalog — so Telebit needs a launchable window before it can be
//      installed from a store at all.
//
// The two pages are deliberately separate. "Cài đặt" is where things are
// changed, over fcitx5's own DBus interface; "Trạng thái" only reports, and
// reports exactly what `telebit doctor` reports, from the same collect().
//
// The look is carried by the stylesheet below rather than by libadwaita:
// libadwaita is 1.0 on jammy and 1.6 on trixie, and the widgets that would
// earn the dependency (AdwActionRow's wrapping subtitles, AdwToast) are 1.3+.
// GDK_VERSION_MAX_ALLOWED in gui/CMakeLists.txt pins this whole target to the
// GTK 4.6 API surface, which is jammy's, so a newer call is a build error here
// instead of a runtime break on an older suite.

#include <gtk/gtk.h>

#include <string>

#include "apps_page.h"
#include "macros_page.h"
#include "setup_page.h"
#include "status_page.h"
#include "widgets.h"

#ifndef TELEBIT_VERSION
#define TELEBIT_VERSION "unknown"
#endif

namespace {

using telebit::setup::AppsPage;
using telebit::setup::MacrosPage;
using telebit::setup::SetupPage;
using telebit::setup::StatusPage;

// Tints and borders are derived from four semantic colours with alpha(), so
// the same sheet reads correctly on a light and a dark theme; surfaces come
// from the theme's own @theme_base_color / @theme_fg_color instead of being
// pinned to a shade.
const char *const kStyle = R"CSS(
.tb-hero {
  border-radius: 16px;
  padding: 18px 20px;
}
.tb-hero.ok    { background: alpha(#2ec27e, 0.10); border: 1px solid alpha(#2ec27e, 0.30); }
.tb-hero.fail  { background: alpha(#e01b24, 0.10); border: 1px solid alpha(#e01b24, 0.30); }
.tb-hero.busy  { background: alpha(@theme_fg_color, 0.05); border: 1px solid alpha(@theme_fg_color, 0.12); }

.tb-badge {
  min-width: 46px;
  min-height: 46px;
  border-radius: 23px;
}
.tb-badge.ok   { background: alpha(#2ec27e, 0.18); color: #1f8a5b; }
.tb-badge.fail { background: alpha(#e01b24, 0.18); color: #c01c28; }
.tb-badge.busy { background: alpha(@theme_fg_color, 0.08); }

.tb-hero-title  { font-size: 1.35em; font-weight: 800; }
.tb-hero-detail { opacity: 0.68; }

.tb-section {
  font-size: 0.80em;
  font-weight: 800;
  letter-spacing: 0.09em;
  opacity: 0.55;
  margin-top: 20px;
  margin-bottom: 8px;
}

.tb-card {
  background: @theme_base_color;
  border: 1px solid alpha(@theme_fg_color, 0.13);
  border-radius: 12px;
}
.tb-card separator { background: alpha(@theme_fg_color, 0.09); }

.tb-label { font-weight: 700; }
.tb-value { font-family: monospace; font-size: 0.92em; opacity: 0.92; }
.tb-note  { font-size: 0.90em; opacity: 0.62; }

.tb-dot.ok   { color: #2ec27e; }
.tb-dot.warn { color: #e5a50a; }
.tb-dot.fail { color: #e01b24; }
.tb-dot.info { color: #3584e4; }

.tb-footer { font-size: 0.85em; opacity: 0.45; }
.tb-pill { border-radius: 99px; padding-left: 16px; padding-right: 16px; }
.tb-error { font-size: 0.90em; color: #c01c28; }
)CSS";

struct Ui {
    GtkApplication *app = nullptr;
    GtkWidget *window = nullptr;
    GtkWidget *stack = nullptr;
    GtkWidget *setup_scroller = nullptr;
    GtkWidget *macros_scroller = nullptr;
    GtkWidget *apps_scroller = nullptr;
    GtkWidget *status_scroller = nullptr;
    SetupPage *setup = nullptr;
    MacrosPage *macros = nullptr;
    AppsPage *apps = nullptr;
    StatusPage *status = nullptr;
};

// Both pages are taller than the window, and a scrolled window that is asked
// for its adjustment before its child has been measured comes back already
// scrolled — which opened the window halfway down the page. Runs once, after
// the first layout.
gboolean scroll_pages_to_top(gpointer data) {
    auto *ui = static_cast<Ui *>(data);
    for (GtkWidget *scroller :
         {ui->setup_scroller, ui->macros_scroller, ui->apps_scroller, ui->status_scroller}) {
        if (scroller == nullptr) continue;
        gtk_adjustment_set_value(
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)), 0.0);
    }
    return G_SOURCE_REMOVE;
}

void load_style() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, kStyle, -1);
    GdkDisplay *display = gdk_display_get_default();
    if (display != nullptr) {
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(provider);
}

// Each page scrolls on its own, so a long report does not push the tab
// switcher off the top, and the column is width-limited because GTK 4.6 has no
// AdwClamp and text spanning a maximised window is unreadable.
GtkWidget *wrap_in_scroller(GtkWidget *content) {
    GtkWidget *column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(column, 620, -1);
    gtk_widget_set_halign(column, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(column, 24);
    gtk_widget_set_margin_end(column, 24);
    gtk_widget_set_margin_top(column, 20);
    gtk_widget_set_margin_bottom(column, 24);
    gtk_box_append(GTK_BOX(column), content);

    GtkWidget *footer =
        telebit::setup::make_label(std::string("Telebit ") + TELEBIT_VERSION, "tb-footer", false);
    gtk_widget_set_margin_top(footer, 20);
    gtk_box_append(GTK_BOX(column), footer);

    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), column);
    return scroller;
}

// Two things change these settings behind this window's back: fcitx5-configtool,
// and the addon itself, which appends to the application list every time it sees
// a program it has not seen before. So each editable page re-reads when it comes
// back into view rather than trusting what it drew on startup.
void on_visible_page_changed(GObject *, GParamSpec *, gpointer data) {
    auto *ui = static_cast<Ui *>(data);
    const char *name = gtk_stack_get_visible_child_name(GTK_STACK(ui->stack));
    if (g_strcmp0(name, "setup") == 0) telebit::setup::setup_page_reload(ui->setup);
    else if (g_strcmp0(name, "macros") == 0) telebit::setup::macros_page_reload(ui->macros);
    else if (g_strcmp0(name, "apps") == 0) telebit::setup::apps_page_reload(ui->apps);
}

void on_window_destroy(GtkWidget *, gpointer data) {
    auto *ui = static_cast<Ui *>(data);
    telebit::setup::status_page_closed(ui->status);
    telebit::setup::setup_page_closed(ui->setup);
}

void on_activate(GtkApplication *app, gpointer data) {
    auto *ui = static_cast<Ui *>(data);
    load_style();

    ui->setup = telebit::setup::setup_page_new(app);
    ui->macros = telebit::setup::macros_page_new();
    ui->apps = telebit::setup::apps_page_new();
    ui->status = telebit::setup::status_page_new(app);

    ui->setup_scroller = wrap_in_scroller(telebit::setup::setup_page_widget(ui->setup));
    ui->macros_scroller = wrap_in_scroller(telebit::setup::macros_page_widget(ui->macros));
    ui->apps_scroller = wrap_in_scroller(telebit::setup::apps_page_widget(ui->apps));
    ui->status_scroller = wrap_in_scroller(telebit::setup::status_page_widget(ui->status));

    ui->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(ui->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_add_titled(GTK_STACK(ui->stack), ui->setup_scroller, "setup", "Cài đặt");
    gtk_stack_add_titled(GTK_STACK(ui->stack), ui->macros_scroller, "macros", "Gõ tắt");
    gtk_stack_add_titled(GTK_STACK(ui->stack), ui->apps_scroller, "apps", "Ứng dụng");
    gtk_stack_add_titled(GTK_STACK(ui->stack), ui->status_scroller, "status", "Trạng thái");
    // Set explicitly rather than relying on "the first child added wins": the
    // status page fills itself in from a worker thread after the window is up,
    // and whichever page last touched its widgets should not decide which tab
    // the window opens on. Setup is first because switching Telebit on is what
    // someone opening this window most often came to do.
    gtk_stack_set_visible_child_name(GTK_STACK(ui->stack), "setup");
    g_signal_connect(ui->stack, "notify::visible-child-name",
                     G_CALLBACK(on_visible_page_changed), ui);

    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(ui->stack));

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header), switcher);

    ui->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(ui->window), "Telebit");
    gtk_window_set_default_size(GTK_WINDOW(ui->window), 820, 760);
    gtk_window_set_titlebar(GTK_WINDOW(ui->window), header);
    gtk_window_set_child(GTK_WINDOW(ui->window), ui->stack);
    g_signal_connect(ui->window, "destroy", G_CALLBACK(on_window_destroy), ui);
    gtk_window_present(GTK_WINDOW(ui->window));
    g_idle_add(scroll_pages_to_top, ui);

    // The status page starts probing straight away even though the setup page
    // is in front: by the time someone switches tabs the answer is there.
    telebit::setup::status_page_refresh(ui->status);
}

}  // namespace

int main(int argc, char **argv) {
    // Leaked on purpose: it must outlive any probe thread still blocked in
    // popen() when the process is on its way out.
    Ui *ui = new Ui();

    // A literal 0 rather than a named flag: G_APPLICATION_DEFAULT_FLAGS needs
    // GLib 2.74 (noble) and G_APPLICATION_FLAGS_NONE is deprecated from 2.80,
    // so either name breaks the build at one end of jammy..trixie.
    ui->app = gtk_application_new("io.github.sonnam0904.Telebit",
                                  static_cast<GApplicationFlags>(0));
    g_signal_connect(ui->app, "activate", G_CALLBACK(on_activate), ui);
    const int status = g_application_run(G_APPLICATION(ui->app), argc, argv);
    g_object_unref(ui->app);
    return status;
}
