// `telebit doctor` — judge the input-method plumbing and say what to fix.
//
// The division of labour with fcitx5-diagnose is deliberate. That tool is
// mature at inspecting the host (locales, ldd, immodule caches) and there is no
// value in reimplementing it, but it has four blind spots this command exists
// to cover:
//
//   1. it cannot see inside a Flatpak or Snap sandbox, which is where the
//      module an app needs either exists or does not;
//   2. it reads the environment of the shell it runs in, not the environment
//      the graphical session actually handed to the apps;
//   3. it does not identify the compositor, which on Wayland decides the whole
//      correct configuration;
//   4. it treats "GTK_IM_MODULE != fcitx" as an error, which upstream no
//      longer recommends on Plasma/Wayland.
//
// So doctor reports on those and points at fcitx5-diagnose for the rest.

#pragma once

#include <cstddef>
#include <iosfwd>

#include "verdict.h"

namespace telebit::doctor {

struct Options {
    // Enter every sandbox and read its environment for real. Exact, but costs
    // a sandbox launch per app.
    bool deep = false;
    // Plain, colourless output shaped for pasting into a bug report.
    bool markdown = false;
};

// Returns a process exit status: 0 when nothing failed, 1 when at least one
// check did, so the command composes in scripts.
int run(const Options &options);

// Renderers, taking their stream and width as arguments rather than reaching
// for std::cout and ioctl(). That is what makes them testable, and rendering
// had no tests at all until a wrapped indent silently flattened the app/runtime
// hierarchy in the pretty table while the markdown one kept it.
void render_pretty(const Output &out, std::ostream &os, bool colour, std::size_t width);
void render_markdown(const Output &out, std::ostream &os);

}  // namespace telebit::doctor
