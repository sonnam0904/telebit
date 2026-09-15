#include "macros_page.h"

#include <string>
#include <vector>

#include "fcitx5_bus.h"
#include "widgets.h"

namespace telebit::setup {

struct MacrosPage {
    GtkWidget *root = nullptr;
    GtkWidget *card = nullptr;
    GtkWidget *add_button = nullptr;
    GtkWidget *save_button = nullptr;
    GtkWidget *status = nullptr;

    // The working copy. Entries write straight into it as they are typed; only
    // Lưu turns it into a configuration.
    std::vector<bus::Macro> draft;
    bool dirty = false;
    bool loading = false;
};

namespace {

void set_status(MacrosPage *page, const std::string &message, bool is_error) {
    gtk_label_set_text(GTK_LABEL(page->status), message.c_str());
    gtk_widget_remove_css_class(page->status, "tb-error");
    gtk_widget_remove_css_class(page->status, "tb-note");
    gtk_widget_add_css_class(page->status, is_error ? "tb-error" : "tb-note");
}

void set_dirty(MacrosPage *page, bool dirty) {
    page->dirty = dirty;
    gtk_widget_set_sensitive(page->save_button, dirty);
}

void rebuild(MacrosPage *page);

std::size_t index_of(GtkWidget *widget) {
    return static_cast<std::size_t>(
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "tb-index")));
}

void on_abbrev_changed(GtkEditable *entry, gpointer data) {
    auto *page = static_cast<MacrosPage *>(data);
    if (page->loading) return;
    const std::size_t index = index_of(GTK_WIDGET(entry));
    if (index >= page->draft.size()) return;
    page->draft[index].abbrev = gtk_editable_get_text(entry);
    set_dirty(page, true);
}

void on_expansion_changed(GtkEditable *entry, gpointer data) {
    auto *page = static_cast<MacrosPage *>(data);
    if (page->loading) return;
    const std::size_t index = index_of(GTK_WIDGET(entry));
    if (index >= page->draft.size()) return;
    page->draft[index].expansion = gtk_editable_get_text(entry);
    set_dirty(page, true);
}

void on_delete_clicked(GtkButton *button, gpointer data) {
    auto *page = static_cast<MacrosPage *>(data);
    const std::size_t index = index_of(GTK_WIDGET(button));
    if (index >= page->draft.size()) return;
    page->draft.erase(page->draft.begin() + static_cast<std::ptrdiff_t>(index));
    set_dirty(page, true);
    // Every row below this one has just changed index, so the card is rebuilt
    // rather than patched: the index each widget carries would otherwise point
    // one row too far down for the rest of the session.
    rebuild(page);
}

void on_add_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<MacrosPage *>(data);
    page->draft.push_back(bus::Macro{});
    // Not dirty yet: an empty pair is dropped on save, so a stray click on
    // "Thêm" should not light up a button that would write nothing.
    rebuild(page);

    // Put the cursor in the row that was just added, so it can be typed into
    // without aiming at it.
    GtkWidget *last = gtk_widget_get_last_child(page->card);
    if (last != nullptr) {
        GtkWidget *first_entry = gtk_widget_get_first_child(last);
        if (first_entry != nullptr) gtk_widget_grab_focus(first_entry);
    }
}

void on_save_clicked(GtkButton *, gpointer data) {
    auto *page = static_cast<MacrosPage *>(data);

    // A macro needs both halves. The engine already ignores the incomplete ones
    // (rebuildMacroIndex skips an empty abbrev or expansion), so dropping them
    // here only makes the stored list match what is actually in effect.
    std::vector<bus::Macro> keep;
    int dropped = 0;
    for (const auto &macro : page->draft) {
        if (macro.abbrev.empty() || macro.expansion.empty()) {
            ++dropped;
            continue;
        }
        keep.push_back(macro);
    }

    if (!bus::write_macros(keep)) {
        set_status(page, "Không lưu được. fcitx5 không nhận lệnh — thử khởi động lại fcitx5.",
                   true);
        return;
    }

    set_dirty(page, false);
    std::string message = "Đã lưu " + std::to_string(keep.size()) + " gõ tắt.";
    if (dropped > 0) {
        message += " Bỏ qua " + std::to_string(dropped) +
                   " dòng thiếu từ viết tắt hoặc nội dung thay thế.";
    }
    set_status(page, message, false);

    // Read back rather than trust the draft: the addon lower-cases and
    // de-duplicates on the way in, and the list shown should be the list stored.
    page->draft = bus::read_macros();
    rebuild(page);
}

GtkWidget *make_macro_row(MacrosPage *page, const bus::Macro &macro, std::size_t index) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);

    GtkWidget *abbrev = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(abbrev), macro.abbrev.c_str());
    gtk_editable_set_width_chars(GTK_EDITABLE(abbrev), 10);
    gtk_editable_set_max_width_chars(GTK_EDITABLE(abbrev), 10);
    g_object_set(abbrev, "placeholder-text", "vn", nullptr);
    g_object_set_data(G_OBJECT(abbrev), "tb-index", GINT_TO_POINTER(static_cast<int>(index)));
    g_signal_connect(abbrev, "changed", G_CALLBACK(on_abbrev_changed), page);
    gtk_box_append(GTK_BOX(box), abbrev);

    GtkWidget *arrow = make_label("→", "tb-note", false);
    gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), arrow);

    GtkWidget *expansion = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(expansion), macro.expansion.c_str());
    gtk_widget_set_hexpand(expansion, TRUE);
    g_object_set(expansion, "placeholder-text", "Việt Nam", nullptr);
    g_object_set_data(G_OBJECT(expansion), "tb-index", GINT_TO_POINTER(static_cast<int>(index)));
    g_signal_connect(expansion, "changed", G_CALLBACK(on_expansion_changed), page);
    gtk_box_append(GTK_BOX(box), expansion);

    GtkWidget *remove = gtk_button_new_from_icon_name(
        icon_or("user-trash-symbolic", "edit-delete-symbolic"));
    gtk_widget_add_css_class(remove, "flat");
    gtk_widget_set_valign(remove, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(remove, "Xoá gõ tắt này");
    g_object_set_data(G_OBJECT(remove), "tb-index", GINT_TO_POINTER(static_cast<int>(index)));
    g_signal_connect(remove, "clicked", G_CALLBACK(on_delete_clicked), page);
    gtk_box_append(GTK_BOX(box), remove);

    return box;
}

void rebuild(MacrosPage *page) {
    clear_children(page->card);

    page->loading = true;
    for (std::size_t i = 0; i < page->draft.size(); ++i) {
        card_append(page->card, make_macro_row(page, page->draft[i], i));
    }
    page->loading = false;

    if (page->draft.empty()) {
        card_append(page->card,
                    make_setting_row("Chưa có gõ tắt nào",
                                     "Bấm “Thêm gõ tắt” để tạo dòng đầu tiên.", nullptr));
    }
}

GtkWidget *build_actions(MacrosPage *page) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(box, 12);

    page->add_button = gtk_button_new_with_label("Thêm gõ tắt");
    gtk_widget_add_css_class(page->add_button, "tb-pill");
    g_signal_connect(page->add_button, "clicked", G_CALLBACK(on_add_clicked), page);
    gtk_box_append(GTK_BOX(box), page->add_button);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(box), spacer);

    page->save_button = gtk_button_new_with_label("Lưu");
    gtk_widget_add_css_class(page->save_button, "tb-pill");
    gtk_widget_add_css_class(page->save_button, "suggested-action");
    gtk_widget_set_sensitive(page->save_button, FALSE);
    g_signal_connect(page->save_button, "clicked", G_CALLBACK(on_save_clicked), page);
    gtk_box_append(GTK_BOX(box), page->save_button);

    return box;
}

}  // namespace

MacrosPage *macros_page_new() {
    auto *page = new MacrosPage();

    page->root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(page->root), make_section_title("Gõ tắt"));
    gtk_box_append(
        GTK_BOX(page->root),
        make_label("Gõ từ viết tắt rồi dấu cách, Telebit thay bằng nội dung bên phải. "
                   "Từ viết tắt không phân biệt hoa thường.",
                   "tb-note", true));

    page->card = make_card();
    gtk_widget_set_margin_top(page->card, 12);
    gtk_box_append(GTK_BOX(page->root), page->card);

    gtk_box_append(GTK_BOX(page->root), build_actions(page));

    page->status = make_label("", "tb-note", true);
    gtk_widget_set_margin_top(page->status, 8);
    gtk_box_append(GTK_BOX(page->root), page->status);

    macros_page_reload(page);
    return page;
}

GtkWidget *macros_page_widget(MacrosPage *page) { return page->root; }

void macros_page_reload(MacrosPage *page) {
    // Unsaved edits outrank a refresh. This is called on every tab switch, and
    // re-reading here would silently discard a macro the user was in the middle
    // of typing when they looked at another tab.
    if (page->dirty) return;

    const bool running = bus::running();
    page->draft = running ? bus::read_macros() : std::vector<bus::Macro>{};
    gtk_widget_set_sensitive(page->add_button, running);
    if (!running) {
        set_status(page, "fcitx5 chưa chạy, nên chưa đọc được danh sách gõ tắt.", true);
    } else {
        set_status(page, "", false);
    }
    set_dirty(page, false);
    rebuild(page);
}

}  // namespace telebit::setup
