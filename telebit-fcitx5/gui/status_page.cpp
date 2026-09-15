#include "status_page.h"

#include <sstream>
#include <string>
#include <utility>

#include "doctor.h"
#include "verdict.h"
#include "widgets.h"

namespace telebit::setup {
namespace {

using telebit::doctor::Options;
using telebit::doctor::Output;

}  // namespace

struct StatusPage {
    GtkApplication *app = nullptr;
    GtkWidget *root = nullptr;
    GtkWidget *hero = nullptr;
    GtkWidget *badge = nullptr;
    GtkWidget *hero_title = nullptr;
    GtkWidget *hero_detail = nullptr;
    GtkWidget *report_box = nullptr;  // rebuilt on every run
    GtkWidget *spinner = nullptr;
    GtkWidget *refresh_button = nullptr;
    GtkWidget *deep_switch = nullptr;
    GtkWidget *copy_button = nullptr;

    // The markdown rendering of the last run, which is what the copy button
    // puts on the clipboard: a bug report wants the table, not the widgets.
    std::string markdown;

    bool closed = false;
};

namespace {

void set_hero(StatusPage *page, const char *state, const char *icon_name, const std::string &title,
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

void show_output(StatusPage *page, const Output &out) {
    clear_children(page->report_box);

    for (const auto &section : out.sections) {
        gtk_box_append(GTK_BOX(page->report_box), make_section_title(section.title));
        GtkWidget *card = make_card();
        for (const auto &row : section.rows) {
            card_append(card, make_status_row(row.status, row.label, row.value, row.note,
                                              row.depth));
        }
        gtk_box_append(GTK_BOX(page->report_box), card);
    }

    if (!out.suggestions.empty()) {
        gtk_box_append(GTK_BOX(page->report_box), make_section_title("Nên làm"));
        GtkWidget *card = make_card();
        for (const auto &suggestion : out.suggestions) {
            card_append(card, make_status_row(telebit::doctor::Status::Info, suggestion, "", "", 0));
        }
        gtk_box_append(GTK_BOX(page->report_box), card);
    }

    const bool failed = out.has_failure();
    set_hero(page, failed ? "fail" : "ok",
             failed ? "dialog-error-symbolic" : "emblem-ok-symbolic",
             failed ? "Bộ gõ chưa tới được ứng dụng" : "Bộ gõ tới được ứng dụng",
             failed ? "Xem những dòng màu đỏ bên dưới và phần “Nên làm”."
                    : "Không tìm thấy lỗi nào trên đường đi của input method.");

    // A re-run replaces every row, so the old scroll offset means nothing —
    // and the verdict is at the top. The scroller belongs to the window (each
    // stack page gets its own), hence the ancestor lookup rather than a
    // pointer threaded through the constructor.
    GtkWidget *scroller = gtk_widget_get_ancestor(page->root, GTK_TYPE_SCROLLED_WINDOW);
    if (scroller != nullptr) {
        gtk_adjustment_set_value(
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)), 0.0);
    }

    std::ostringstream markdown;
    telebit::doctor::render_markdown(out, markdown);
    page->markdown = markdown.str();
    gtk_widget_set_sensitive(page->copy_button, TRUE);
}

// ---------------------------------------------------------------------------
// The worker thread and its two ways back to the main loop

struct ProgressMessage {
    StatusPage *page;
    std::string text;
};

gboolean deliver_progress(gpointer data) {
    auto *message = static_cast<ProgressMessage *>(data);
    if (!message->page->closed) {
        gtk_label_set_text(GTK_LABEL(message->page->hero_detail), message->text.c_str());
    }
    delete message;
    return G_SOURCE_REMOVE;
}

struct ResultMessage {
    StatusPage *page;
    Output out;
};

gboolean deliver_result(gpointer data) {
    auto *message = static_cast<ResultMessage *>(data);
    StatusPage *page = message->page;
    if (!page->closed) {
        show_output(page, message->out);
        gtk_spinner_stop(GTK_SPINNER(page->spinner));
        gtk_widget_set_visible(page->spinner, FALSE);
        gtk_widget_set_sensitive(page->refresh_button, TRUE);
        gtk_widget_set_sensitive(page->deep_switch, TRUE);
    }
    delete message;
    // Balances the hold in status_page_refresh: without it, closing the window
    // mid-probe returns from g_application_run and runs static destructors
    // while the worker is still inside popen().
    g_application_release(G_APPLICATION(page->app));
    return G_SOURCE_REMOVE;
}

struct Job {
    StatusPage *page;
    Options options;
};

gpointer run_probe(gpointer data) {
    auto *job = static_cast<Job *>(data);

    Output out = telebit::doctor::collect(job->options, [job](const std::string &text) {
        g_idle_add(deliver_progress, new ProgressMessage{job->page, text});
    });

    g_idle_add(deliver_result, new ResultMessage{job->page, std::move(out)});
    delete job;
    return nullptr;
}

void on_refresh(GtkButton *, gpointer data) {
    status_page_refresh(static_cast<StatusPage *>(data));
}

// Toggling depth re-runs immediately: a switch that changes nothing until a
// second button is pressed reads as broken.
//
// gboolean, not void: GtkSwitch::state-set is declared to return one, and a
// void handler leaves the marshaller reading whatever happens to be in the
// return register. There is no rollback here for a garbage TRUE to break, but
// it would still suppress the default handler at random and leave the switch
// drawn in the position the user did not choose. FALSE lets `state` follow
// `active`, which is all this switch needs.
gboolean on_deep_toggled(GtkSwitch *, gboolean, gpointer data) {
    status_page_refresh(static_cast<StatusPage *>(data));
    return FALSE;
}

void on_copy(GtkButton *button, gpointer data) {
    auto *page = static_cast<StatusPage *>(data);
    if (page->markdown.empty()) return;
    gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(button)), page->markdown.c_str());
    gtk_label_set_text(GTK_LABEL(page->hero_detail),
                       "Đã sao chép báo cáo dạng markdown vào clipboard.");
}

GtkWidget *build_hero(StatusPage *page) {
    page->hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_add_css_class(page->hero, "tb-hero");

    page->badge = gtk_image_new_from_icon_name(
        icon_or("content-loading-symbolic", "system-search-symbolic"));
    gtk_image_set_pixel_size(GTK_IMAGE(page->badge), 24);
    gtk_widget_add_css_class(page->badge, "tb-badge");
    gtk_widget_set_valign(page->badge, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(page->hero), page->badge);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    page->hero_title = make_label("Đang kiểm tra…", "tb-hero-title", false);
    page->hero_detail = make_label("", "tb-hero-detail", true);
    gtk_box_append(GTK_BOX(text), page->hero_title);
    gtk_box_append(GTK_BOX(text), page->hero_detail);
    gtk_box_append(GTK_BOX(page->hero), text);

    set_hero(page, "busy", icon_or("content-loading-symbolic", "system-search-symbolic"),
             "Đang kiểm tra…", "Đang đọc phiên đồ hoạ, compositor, host và các sandbox.");
    return page->hero;
}

GtkWidget *build_actions(StatusPage *page) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(box, 16);

    page->refresh_button = gtk_button_new_with_label("Kiểm tra lại");
    gtk_widget_add_css_class(page->refresh_button, "tb-pill");
    gtk_widget_set_valign(page->refresh_button, GTK_ALIGN_CENTER);
    g_signal_connect(page->refresh_button, "clicked", G_CALLBACK(on_refresh), page);
    gtk_box_append(GTK_BOX(box), page->refresh_button);

    page->spinner = gtk_spinner_new();
    gtk_widget_set_valign(page->spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(page->spinner, FALSE);
    gtk_box_append(GTK_BOX(box), page->spinner);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(box), spacer);

    page->deep_switch = gtk_switch_new();
    gtk_widget_set_valign(page->deep_switch, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(page->deep_switch,
                                "Vào thật bên trong từng sandbox để đọc biến môi trường. "
                                "Chính xác nhất, nhưng tốn vài giây cho mỗi ứng dụng.");
    g_signal_connect(page->deep_switch, "state-set", G_CALLBACK(on_deep_toggled), page);
    gtk_box_append(GTK_BOX(box), page->deep_switch);
    gtk_box_append(GTK_BOX(box), make_label("Kiểm tra sâu", "tb-note", false));

    page->copy_button = gtk_button_new_from_icon_name("edit-copy-symbolic");
    gtk_widget_add_css_class(page->copy_button, "tb-pill");
    gtk_widget_set_valign(page->copy_button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(page->copy_button,
                                "Sao chép báo cáo (markdown) để dán vào báo lỗi");
    gtk_widget_set_sensitive(page->copy_button, FALSE);
    g_signal_connect(page->copy_button, "clicked", G_CALLBACK(on_copy), page);
    gtk_box_append(GTK_BOX(box), page->copy_button);

    return box;
}

}  // namespace

StatusPage *status_page_new(GtkApplication *app) {
    auto *page = new StatusPage();
    page->app = app;

    page->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(page->root), build_hero(page));
    gtk_box_append(GTK_BOX(page->root), build_actions(page));

    page->report_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(page->root), page->report_box);

    return page;
}

GtkWidget *status_page_widget(StatusPage *page) { return page->root; }

void status_page_closed(StatusPage *page) { page->closed = true; }

void status_page_refresh(StatusPage *page) {
    gtk_widget_set_sensitive(page->refresh_button, FALSE);
    gtk_widget_set_sensitive(page->deep_switch, FALSE);
    gtk_widget_set_sensitive(page->copy_button, FALSE);
    gtk_widget_set_visible(page->spinner, TRUE);
    gtk_spinner_start(GTK_SPINNER(page->spinner));

    const bool deep = gtk_switch_get_active(GTK_SWITCH(page->deep_switch)) != 0;
    set_hero(page, "busy", icon_or("content-loading-symbolic", "system-search-symbolic"),
             "Đang kiểm tra…",
             deep ? "Đang vào từng sandbox để đọc môi trường thật, việc này mất một lúc."
                  : "Đang đọc phiên đồ hoạ, compositor, host và các sandbox.");

    Options options;
    options.deep = deep;

    g_application_hold(G_APPLICATION(page->app));
    // Detached: the probe cannot be cancelled once it is inside popen(), so
    // there is nothing to join. StatusPage::closed keeps a late result from
    // drawing into a window that is gone; the hold keeps the process alive
    // until the thread has handed its result over.
    GThread *thread = g_thread_new("telebit-probe", run_probe, new Job{page, options});
    g_thread_unref(thread);
}

}  // namespace telebit::setup
