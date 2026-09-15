// "Gõ tắt" — abbreviations the engine expands when a word ends.
//
// Unlike every other page in this window, this one does not write on each
// change: a half-typed abbreviation is not a value worth sending to fcitx5, and
// an entry has no moment that means "done" the way a switch does. Edits
// accumulate in a draft and go out when the user presses Lưu.

#pragma once

#include <gtk/gtk.h>

namespace telebit::setup {

struct MacrosPage;

MacrosPage *macros_page_new();

GtkWidget *macros_page_widget(MacrosPage *page);

// Re-reads from fcitx5 and redraws — but only when there is nothing unsaved,
// because this is also called when the tab comes back into view, and dropping
// a half-written macro because the user glanced at another tab would be worse
// than showing a slightly stale list.
void macros_page_reload(MacrosPage *page);

}  // namespace telebit::setup
