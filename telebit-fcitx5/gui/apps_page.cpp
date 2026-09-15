#include "apps_page.h"

#include <algorithm>
#include <string>
#include <vector>

#include "fcitx5_bus.h"
#include "widgets.h"

namespace telebit::setup {

struct AppsPage {
    GtkWidget *root = nullptr;
    GtkWidget *search = nullptr;
    GtkWidget *summary = nullptr;
    GtkWidget *card = nullptr;
    GtkWidget *error = nullptr;

    // The list as fcitx5 last handed it over, already sorted for display. It is
    // kept whole because a write replaces the entire option: toggling one row
    // still sends every other row back untouched.
    std::vector<bus::AppRule> apps;

    // Set while switches are being filled in, so the handler that writes back
    // does not fire for the values it just read.
    bool loading = false;

    // What the last rebuild() drew, so the summary line can be recomputed after
    // a toggle without rebuilding the rows — which would destroy the very
    // GtkSwitch whose callback is still running.
    int shown = 0;
    bool filtered = false;
};

namespace {

// Case- and accent-insensitive enough for a filter box: g_utf8_casefold folds
// Vietnamese the way the locale does, which plain tolower() does not.
std::string fold(const std::string &text) {
    char *folded = g_utf8_casefold(text.c_str(), -1);
    std::string out = folded != nullptr ? folded : text;
    g_free(folded);
    return out;
}

bool matches(const bus::AppRule &app, const std::string &needle) {
    if (needle.empty()) return true;
    return fold(app.label).find(needle) != std::string::npos ||
           fold(app.program).find(needle) != std::string::npos;
}

void update_summary(AppsPage *page);

void set_error(AppsPage *page, const std::string &message) {
    gtk_label_set_text(GTK_LABEL(page->error), message.c_str());
    gtk_widget_set_visible(page->error, !message.empty());
}

// Returns gboolean because GtkSwitch::state-set does, and the value decides
// whether the default handler runs. A GtkSwitch carries two properties: `active`
// is what the user just flipped, `state` is what gets drawn. The default handler
// copies the first into the second; returning TRUE suppresses it, for a caller
// that sets `state` itself.
//
// Declaring this void — as every switch handler in this window used to — left
// the marshaller reading an undefined return value. When it happened to read
// FALSE on the failure path below, the default handler still ran and painted
// `state` with the value the write had just rejected, so the switch showed a
// setting fcitx5 had never been given.
gboolean on_app_toggled(GtkSwitch *toggle, gboolean state, gpointer data) {
    auto *page = static_cast<AppsPage *>(data);
    // FALSE, not TRUE: this is also the path taken by the nested emission the
    // rollback below triggers, and there the default handler is exactly what
    // has to run to pull `state` back with `active`.
    if (page->loading) return FALSE;

    // The index is carried on the widget rather than looked up by scanning,
    // because the rows are rebuilt on every filter change and a pointer into
    // the old list would outlive it.
    const auto index = static_cast<std::size_t>(
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(toggle), "tb-index")));
    if (index >= page->apps.size()) return FALSE;

    std::vector<bus::AppRule> next = page->apps;
    next[index].enabled = state != 0;

    if (!bus::write_apps(next)) {
        // Put it back rather than show a state fcitx5 does not have. The
        // set_active call re-enters this handler with `loading` set, and that
        // pass is what restores `state`.
        page->loading = true;
        gtk_switch_set_active(toggle, state == 0);
        page->loading = false;
        set_error(page, "Không lưu được. fcitx5 không nhận lệnh — thử khởi động lại fcitx5.");
        // TRUE so this emission's default handler cannot undo the rollback by
        // writing the rejected value into `state`.
        return TRUE;
    }
    page->apps = std::move(next);
    set_error(page, "");

    // The count in the summary line just changed. Recomputed rather than
    // rebuilt: rebuild() would free this very switch mid-callback.
    update_summary(page);

    // Deliberately not re-sorted here. Enabled rows sort first, so re-sorting
    // on a toggle would make the row jump out from under the pointer the
    // instant it was switched on.
    return FALSE;
}

GtkWidget *make_app_row(AppsPage *page, const bus::AppRule &app, std::size_t index) {
    GtkWidget *toggle = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(toggle), app.enabled);
    g_object_set_data(G_OBJECT(toggle), "tb-index", GINT_TO_POINTER(static_cast<int>(index)));
    g_signal_connect(toggle, "state-set", G_CALLBACK(on_app_toggled), page);

    // The program name is repeated under the label only when it adds something:
    // for most applications the two are the same word, and printing "Cursor"
    // over "cursor" is noise. For a PWA they differ completely, which is the
    // case this whole row exists to make readable.
    const std::string note = fold(app.label) == fold(app.program) ? std::string() : app.program;
    return make_setting_row(app.label, note, toggle);
}

void update_summary(AppsPage *page) {
    const int enabled = static_cast<int>(std::count_if(
        page->apps.begin(), page->apps.end(), [](const bus::AppRule &a) { return a.enabled; }));
    std::string summary = std::to_string(page->apps.size()) + " ứng dụng · " +
                          std::to_string(enabled) + " đang dùng preedit";
    if (page->filtered) summary += " · đang lọc: " + std::to_string(page->shown);
    gtk_label_set_text(GTK_LABEL(page->summary), summary.c_str());
}

void rebuild(AppsPage *page) {
    clear_children(page->card);

    const std::string needle = fold(gtk_editable_get_text(GTK_EDITABLE(page->search)));
    page->shown = 0;
    page->filtered = !needle.empty();
    page->loading = true;
    for (std::size_t i = 0; i < page->apps.size(); ++i) {
        if (!matches(page->apps[i], needle)) continue;
        card_append(page->card, make_app_row(page, page->apps[i], i));
        ++page->shown;
    }
    page->loading = false;

    if (page->shown == 0) {
        const char *empty = page->apps.empty()
                                ? "Chưa có ứng dụng nào. Danh sách tự dài ra khi bạn gõ vào một "
                                  "ứng dụng mới."
                                : "Không có ứng dụng nào khớp với từ khoá.";
        card_append(page->card, make_setting_row(empty, "", nullptr));
    }

    update_summary(page);
}

void on_search_changed(GtkSearchEntry *, gpointer data) {
    rebuild(static_cast<AppsPage *>(data));
}

GtkWidget *build_header(AppsPage *page) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    GtkWidget *intro = make_label(
        "Ứng dụng bật ở đây sẽ gõ có gạch chân (preedit) thay vì gõ trực tiếp. Cần bật cho những "
        "ứng dụng làm hỏng chữ khi sửa dấu — trình duyệt và cửa sổ PWA là hay gặp nhất. "
        "Telebit tự thêm một dòng cho mỗi ứng dụng nó gặp lần đầu.",
        "tb-note", true);
    gtk_box_append(GTK_BOX(box), intro);

    page->search = gtk_search_entry_new();
    gtk_widget_set_hexpand(page->search, TRUE);
    // GTK 4.6 has no gtk_search_entry_set_placeholder_text; the entry's own
    // placeholder is set through GtkEditable's property instead.
    g_object_set(page->search, "placeholder-text", "Tìm theo tên hoặc tên tiến trình…", nullptr);
    g_signal_connect(page->search, "search-changed", G_CALLBACK(on_search_changed), page);
    gtk_box_append(GTK_BOX(box), page->search);

    page->summary = make_label("", "tb-note", false);
    gtk_box_append(GTK_BOX(box), page->summary);

    page->error = make_label("", "tb-error", true);
    gtk_widget_set_visible(page->error, FALSE);
    gtk_box_append(GTK_BOX(box), page->error);

    return box;
}

}  // namespace

AppsPage *apps_page_new() {
    auto *page = new AppsPage();

    page->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(page->root), make_section_title("Ứng dụng dùng preedit"));
    gtk_box_append(GTK_BOX(page->root), build_header(page));

    page->card = make_card();
    gtk_widget_set_margin_top(page->card, 12);
    gtk_box_append(GTK_BOX(page->root), page->card);

    apps_page_reload(page);
    return page;
}

GtkWidget *apps_page_widget(AppsPage *page) { return page->root; }

void apps_page_reload(AppsPage *page) {
    const bool running = bus::running();
    page->apps = running ? bus::read_apps() : std::vector<bus::AppRule>{};

    // Switched-on first, then by name. The addon's own order is chronological —
    // the order applications happened to be focused in — which puts the rows
    // that matter at whatever depth the user last opened something.
    std::stable_sort(page->apps.begin(), page->apps.end(),
                     [](const bus::AppRule &a, const bus::AppRule &b) {
                         if (a.enabled != b.enabled) return a.enabled;
                         return fold(a.label) < fold(b.label);
                     });

    gtk_widget_set_sensitive(page->search, running);
    set_error(page, running ? "" : "fcitx5 chưa chạy, nên chưa đọc được danh sách.");
    rebuild(page);
}

}  // namespace telebit::setup
