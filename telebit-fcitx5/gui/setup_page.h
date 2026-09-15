// "Cài đặt" — turn Telebit on and change how it types, here rather than in
// fcitx5-configtool.
//
// The page exposes the handful of options a person changes when they start
// using Telebit — which layout (Telex or VNI) and the typing behaviours — plus
// the two things that are about the installation rather than about typing:
// restarting fcitx5, and whether this is the newest Telebit. Macros and the
// per-application list have pages of their own; only the key bindings are still
// fcitx5-configtool's, which the page can open through fcitx5 itself.

#pragma once

#include <gtk/gtk.h>

namespace telebit::setup {

struct SetupPage;

// `app` is held (g_application_hold) while the version check or an upgrade is
// in flight, for the same reason the status page holds it during a probe.
SetupPage *setup_page_new(GtkApplication *app);

GtkWidget *setup_page_widget(SetupPage *page);

// Called from the window's destroy handler: a version check cannot be
// cancelled once it is inside curl, so the delivery callback needs to be told
// there is nothing left to draw into.
void setup_page_closed(SetupPage *page);

// Re-reads everything from fcitx5: whether it is running, whether Telebit is
// in the current input-method group, and the current option values. Called on
// construction and whenever the page is shown again, because fcitx5-configtool
// may have changed things in the meantime.
void setup_page_reload(SetupPage *page);

}  // namespace telebit::setup
