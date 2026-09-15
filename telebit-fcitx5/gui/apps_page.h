// "Ứng dụng" — which applications type through preedit instead of direct
// commit.
//
// The list is not something the user builds: the addon appends a row the first
// time it sees an application, so this page is also the only place to find out
// what Telebit has noticed. That is why it shows every row rather than only the
// switched-on ones, and why the program name stays visible under the label —
// the label is resolved from a desktop entry and can be wrong or missing, the
// program name is what actually decides the match.

#pragma once

#include <gtk/gtk.h>

namespace telebit::setup {

struct AppsPage;

AppsPage *apps_page_new();

GtkWidget *apps_page_widget(AppsPage *page);

// Re-reads the list from fcitx5 and redraws. Called on construction and every
// time the tab comes back into view, because the addon appends to this list on
// its own while the window is open.
void apps_page_reload(AppsPage *page);

}  // namespace telebit::setup
