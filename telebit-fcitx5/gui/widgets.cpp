#include "widgets.h"

namespace telebit::setup {

using telebit::doctor::Status;

const char *icon_or(const char *name, const char *fallback) {
    GdkDisplay *display = gdk_display_get_default();
    if (display == nullptr) return fallback;
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
    return gtk_icon_theme_has_icon(theme, name) != 0 ? name : fallback;
}

const char *status_icon_name(Status status) {
    switch (status) {
        case Status::Ok: return "emblem-ok-symbolic";
        case Status::Warn: return "dialog-warning-symbolic";
        case Status::Fail: return "dialog-error-symbolic";
        case Status::Info: return "dialog-information-symbolic";
    }
    return "dialog-information-symbolic";
}

const char *status_css_class(Status status) {
    switch (status) {
        case Status::Ok: return "ok";
        case Status::Warn: return "warn";
        case Status::Fail: return "fail";
        case Status::Info: return "info";
    }
    return "info";
}

GtkWidget *make_label(const std::string &text, const char *css_class, bool wrap) {
    GtkWidget *label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    if (wrap) {
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        // The GUI counterpart of textfmt.cpp's hard_break(): absolute paths and
        // Flatpak refs contain no spaces, and word wrapping alone would let one
        // of them set the window's minimum width.
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 56);
        // Deliberately not selectable: a selectable GtkLabel takes focus, and a
        // scrolled window scrolls to its focused child, which opened the window
        // already scrolled past the verdict.
    }
    if (css_class != nullptr) gtk_widget_add_css_class(label, css_class);
    return label;
}

GtkWidget *make_card() {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "tb-card");
    return card;
}

GtkWidget *make_section_title(const std::string &title) {
    char *upper = g_utf8_strup(title.c_str(), -1);
    GtkWidget *label = make_label(upper != nullptr ? upper : title, "tb-section", false);
    g_free(upper);
    return label;
}

GtkWidget *make_setting_row_labels(const std::string &title, const std::string &note,
                                   GtkWidget *control, GtkWidget **title_out,
                                   GtkWidget **note_out) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(text, TRUE);

    GtkWidget *title_label = make_label(title, "tb-label", true);
    gtk_box_append(GTK_BOX(text), title_label);
    if (title_out != nullptr) *title_out = title_label;

    if (!note.empty() || note_out != nullptr) {
        GtkWidget *note_label = make_label(note, "tb-note", true);
        gtk_box_append(GTK_BOX(text), note_label);
        if (note_out != nullptr) *note_out = note_label;
    }
    gtk_box_append(GTK_BOX(box), text);

    if (control != nullptr) {
        gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(box), control);
    }
    return box;
}

GtkWidget *make_setting_row(const std::string &title, const std::string &note, GtkWidget *control) {
    return make_setting_row_labels(title, note, control, nullptr, nullptr);
}

GtkWidget *make_status_row(Status status, const std::string &label, const std::string &value,
                           const std::string &note, int depth) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_start(box, 16 + (24 * (depth > 0 ? depth : 0)));

    GtkWidget *icon = gtk_image_new_from_icon_name(status_icon_name(status));
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);
    gtk_widget_set_valign(icon, GTK_ALIGN_START);
    gtk_widget_set_margin_top(icon, 2);
    gtk_widget_add_css_class(icon, "tb-dot");
    gtk_widget_add_css_class(icon, status_css_class(status));
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(text), make_label(label, "tb-label", true));
    if (!value.empty()) gtk_box_append(GTK_BOX(text), make_label(value, "tb-value", true));
    if (!note.empty()) gtk_box_append(GTK_BOX(text), make_label(note, "tb-note", true));
    gtk_box_append(GTK_BOX(box), text);

    return box;
}

GtkWidget *card_append(GtkWidget *card, GtkWidget *row) {
    if (gtk_widget_get_first_child(card) != nullptr) {
        gtk_box_append(GTK_BOX(card), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    }
    gtk_box_append(GTK_BOX(card), row);
    return row;
}

void clear_children(GtkWidget *container) {
    GtkWidget *child = gtk_widget_get_first_child(container);
    while (child != nullptr) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(container), child);
        child = next;
    }
}

}  // namespace telebit::setup
