// Shared widget construction for the two pages, so a row on the setup page and
// a row on the status page cannot drift apart visually.
//
// Everything here is GTK 4.6 or older; gui/CMakeLists.txt fences the whole
// target to that API surface.

#pragma once

#include <gtk/gtk.h>

#include <string>

#include "verdict.h"

namespace telebit::setup {

// Falls back rather than rendering the broken-image icon: not every icon name
// exists in every adwaita-icon-theme version across jammy..Fedora.
const char *icon_or(const char *name, const char *fallback);

const char *status_icon_name(telebit::doctor::Status status);
const char *status_css_class(telebit::doctor::Status status);

// `wrap` also turns on WORD_CHAR wrapping and a character cap: the report is
// full of absolute paths and Flatpak refs with no spaces in them, which plain
// word wrapping would let push the window wider than the screen.
GtkWidget *make_label(const std::string &text, const char *css_class, bool wrap);

// The rounded surface every group of rows sits on.
GtkWidget *make_card();

// The small caps heading above a card. Uppercasing happens here because GTK
// CSS has no text-transform, and with g_utf8_strup because these are
// Vietnamese ("Phiên làm việc").
GtkWidget *make_section_title(const std::string &title);

// A row of: bold title, optional note underneath, and a control on the right
// (a switch, a button, a pair of entries). Every editable row in the window is
// built from this, which is what keeps the setup, macro and application pages
// looking like one window rather than three.
GtkWidget *make_setting_row(const std::string &title, const std::string &note, GtkWidget *control);

// The same row, handing back its two labels so a caller can rewrite them in
// place when an answer arrives later — rebuilding the card instead would move
// the controls out from under the pointer. Either out-parameter may be null;
// asking for `note_out` always creates the note label, even when it starts
// empty, because there has to be something to write into.
GtkWidget *make_setting_row_labels(const std::string &title, const std::string &note,
                                   GtkWidget *control, GtkWidget **title_out,
                                   GtkWidget **note_out);

// A row of: status icon, bold title, optional value, optional note.
// `depth` indents children under their parent (an app under its runtime).
GtkWidget *make_status_row(telebit::doctor::Status status, const std::string &label,
                           const std::string &value, const std::string &note, int depth);

// Appends `row` to `card`, drawing a separator first for every row but the
// first. Returns the row, for callers that need to keep it.
GtkWidget *card_append(GtkWidget *card, GtkWidget *row);

void clear_children(GtkWidget *container);

}  // namespace telebit::setup
