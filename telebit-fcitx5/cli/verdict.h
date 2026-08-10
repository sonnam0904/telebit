// Turning probed facts into a verdict.
//
// This layer is deliberately pure: it takes the plain structs probe.h fills in
// and returns rows of text. No filesystem, no subprocesses, no terminal. That
// is what makes the interesting cases testable — including every Wayland branch
// from an X11 machine, which is otherwise unreachable.
//
// Rendering lives in doctor.cpp, so a change to how the table looks can never
// change what the report concludes.

#pragma once

#include <string>
#include <vector>

#include "probe.h"

namespace telebit::doctor {

enum class Status {
    Ok,
    Warn,
    Fail,
    Info,
};

struct Row {
    Status status = Status::Info;
    std::string label;
    std::string value;
    std::string note;  // the "why", rendered under the value

    // Nesting level, for rows that belong under the row above them (an app
    // under its runtime). This is a display property, not part of the label:
    // padding the label with spaces instead meant word-wrapping silently ate
    // the indent, so the pretty table lost the hierarchy the report is built
    // around while the markdown table kept it.
    int depth = 0;
};

struct Section {
    std::string title;
    std::vector<Row> rows;
};

struct Output {
    std::vector<Section> sections;
    std::vector<std::string> suggestions;

    Section &section(const std::string &title);

    // True when at least one row failed. This is what the process exit status
    // is built from, so a Warn must never reach it: a latent risk is not a
    // broken machine.
    bool has_failure() const;
};

struct RuntimeVerdict {
    Row row;
    // A toolkit this runtime ships has no fcitx module, and nothing on the
    // Telebit side can change that.
    bool unfixable_gap = false;
};

void judge_session(const SessionInfo &session, Output &out);
void judge_fcitx5(const Fcitx5Info &fcitx5, const SessionInfo &session, const Report &report,
                  Output &out);
RuntimeVerdict judge_runtime(const SandboxRuntime &runtime, const SessionInfo &session);

// The host root, i.e. every application installed from a .deb / .rpm / pacman
// package. Unlike a sandbox this one is fixable — a missing module is a package
// away — so the advice names packages rather than explaining a dead end.
void judge_host(const HostInfo &host, const SessionInfo &session, Output &out);

// Which scanned applications link one toolkit, and how much of the scan could
// not answer.
//
// Separated from the row it feeds so the two things that make this list honest
// are testable on their own: it reports only positive matches, and it carries the
// count of applications whose toolkit could not be read — without which a partial
// list reads as a complete one.
struct AffectedApps {
    std::vector<std::string> names;
    std::size_t unknown = 0;  // toolkit could not be determined
    std::size_t scanned = 0;  // applications inspected in total
};

AffectedApps apps_using(const HostInfo &host, bool NativeApp::*toolkit);
void judge_sandboxes(const Report &report, const SessionInfo &session, Output &out);

// Display name for a runtime, with the architecture segment of a Flatpak ref
// dropped — noise in a report about the machine you are already on, and the
// reason the longest refs used to overflow the label column.
std::string runtime_label(const std::string &kind, const std::string &id);

// Name of a module kind as it appears in the report.
const char *module_kind_label(ModuleKind kind);

// One cell of the module matrix. "thiếu" and "n/a" look alike but mean opposite
// things: the runtime ships the toolkit with no module to drive it, versus it
// has no such toolkit and nothing to fix.
std::string module_cell(ModuleKind kind, bool toolkit_present);

}  // namespace telebit::doctor
