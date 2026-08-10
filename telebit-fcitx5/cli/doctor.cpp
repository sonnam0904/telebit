#include "doctor.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "probe.h"
#include "textfmt.h"
#include "verdict.h"

namespace telebit::doctor {
namespace {

const char *status_symbol(Status status) {
    switch (status) {
        case Status::Ok: return "✔";
        case Status::Warn: return "!";
        case Status::Fail: return "✘";
        case Status::Info: return "·";
    }
    return "·";
}

const char *status_word(Status status) {
    switch (status) {
        case Status::Ok: return "OK";
        case Status::Warn: return "CẢNH BÁO";
        case Status::Fail: return "LỖI";
        case Status::Info: return "TIN";
    }
    return "TIN";
}

const char *status_colour(Status status) {
    switch (status) {
        case Status::Ok: return "\033[32m";
        case Status::Warn: return "\033[33m";
        case Status::Fail: return "\033[31m";
        case Status::Info: return "\033[90m";
    }
    return "";
}

// Terminal width, clamped: below 80 the three columns stop being readable, and
// above 120 the eye loses the row it is on.
std::size_t terminal_width() {
    struct winsize ws {};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        if (ws.ws_col < 80) return 80;
        if (ws.ws_col > 120) return 120;
        return ws.ws_col;
    }
    return 100;
}

}  // namespace

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void render_pretty(const Output &out, std::ostream &os, bool colour, std::size_t width) {
    const char *reset = colour ? "\033[0m" : "";
    const char *dim = colour ? "\033[90m" : "";
    const char *bold = colour ? "\033[1m" : "";

    // Each section is drawn as its own table: three columns, sized so that
    // borders + padding + columns come to exactly the terminal width.
    //   width = 1 + 3 + 1 + (label+2) + 1 + (value+2) + 1
    // Size the label column to the longest label actually present, so short
    // reports do not carry a wide empty gutter and long runtime refs still fit
    // on one line. The cap keeps the value column usable on an 80-wide terminal.
    // Two cells per nesting level, added by the renderer rather than baked into
    // the label — see Row::depth.
    const auto indent_of = [](const Row &row) { return static_cast<std::size_t>(row.depth) * 2; };

    std::size_t label_column = 18;
    for (const auto &section : out.sections) {
        for (const auto &row : section.rows) {
            label_column = std::max(label_column, display_width(row.label) + indent_of(row));
        }
    }
    label_column = std::min(label_column, std::min<std::size_t>(40, width / 2));
    const std::size_t value_column = width - label_column - 11;
    const auto repeat = [](const char *unit, std::size_t times) {
        std::string out_string;
        for (std::size_t i = 0; i < times; ++i) out_string += unit;
        return out_string;
    };
    const std::string status_rule = repeat("─", 3);
    const std::string label_rule = repeat("─", label_column + 2);
    const std::string value_rule = repeat("─", value_column + 2);
    const std::string full_rule = repeat("─", width - 2);

    const auto bar = [&] { return std::string(dim) + "│" + reset; };
    const auto rule_line = [&](const char *left, const char *right) {
        os << dim << left << full_rule << right << reset << "\n";
    };
    const auto split_rule = [&](const char *left, const char *mid, const char *right) {
        os << dim << left << status_rule << mid << label_rule << mid << value_rule << right
                  << reset << "\n";
    };
    const auto title_row = [&](const std::string &text) {
        os << bar() << " " << bold << pad_to(text, width - 4) << reset << " " << bar()
                  << "\n";
    };
    const auto cell = [&](const std::string &text, std::size_t column, const char *prefix) {
        os << " " << prefix << pad_to(text, column) << reset << " " << bar();
    };

    for (const auto &section : out.sections) {
        os << "\n";
        rule_line("┌", "┐");
        title_row(section.title);
        split_rule("├", "┬", "┤");

        for (const auto &row : section.rows) {
            const char *colour_code = colour ? status_colour(row.status) : "";
            // Wrap inside the space the indent leaves, then put the indent back
            // on. Wrapping an already-indented string drops the leading spaces.
            const std::size_t indent = std::min(indent_of(row), label_column);
            const std::string pad(indent, ' ');
            auto label_lines = wrap(row.label, label_column - indent);
            for (auto &line : label_lines) line = pad + line;
            auto value_lines = wrap(row.value, value_column);
            // The note explains the row, so it stays in the same cell; anywhere
            // else it would lose its subject.
            const std::size_t value_line_count = value_lines.size();
            if (!row.note.empty()) {
                for (const auto &note_line : wrap("↳ " + row.note, value_column)) {
                    value_lines.push_back(note_line);
                }
            }

            const std::size_t lines = std::max(label_lines.size(), value_lines.size());
            for (std::size_t i = 0; i < lines; ++i) {
                os << bar() << " "
                          << (i == 0 ? std::string(colour_code) + status_symbol(row.status) + reset
                                     : std::string(" "))
                          << " " << bar();
                cell(i < label_lines.size() ? label_lines[i] : "", label_column, "");
                cell(i < value_lines.size() ? value_lines[i] : "", value_column,
                     i >= value_line_count ? dim : "");
                os << "\n";
            }
        }
        split_rule("└", "┴", "┘");
    }

    if (!out.suggestions.empty()) {
        os << "\n";
        rule_line("┌", "┐");
        title_row("Nên làm");
        rule_line("├", "┤");
        for (const auto &suggestion : out.suggestions) {
            bool first = true;
            for (const auto &text : wrap(suggestion, width - 6)) {
                os << bar() << " " << (first ? "•" : " ") << " " << pad_to(text, width - 6)
                          << " " << bar() << "\n";
                first = false;
            }
        }
        rule_line("└", "┘");
    }

    os << "\n" << dim
              << "Locale và cấu hình chi tiết của fcitx5 đã được fcitx5-diagnose lo — chạy nó nếu "
                 "cần phần đó."
              << reset << "\n";
}

// A path or an app id may contain a pipe; unescaped it would silently split the
// row into extra columns and the pasted report would read as nonsense.
static std::string escape_cell(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '|') out += "\\|";
        else out += c;
    }
    return out;
}

void render_markdown(const Output &out, std::ostream &os) {
    os << "# telebit doctor\n";
    for (const auto &section : out.sections) {
        os << "\n## " << section.title << "\n\n";
        os << "| | Mục | Giá trị |\n|---|---|---|\n";
        for (const auto &row : section.rows) {
            // Leading spaces collapse in a rendered markdown table, so nesting
            // needs a visible mark rather than indentation.
            const std::string prefix = row.depth > 0 ? "└ " : "";
            os << "| " << status_word(row.status) << " | " << prefix
                      << escape_cell(row.label) << " | " << escape_cell(row.value);
            if (!row.note.empty()) os << "<br>_" << escape_cell(row.note) << "_";
            os << " |\n";
        }
    }
    if (!out.suggestions.empty()) {
        os << "\n## Nên làm\n\n";
        for (const auto &suggestion : out.suggestions) {
            os << "- " << suggestion << "\n";
        }
    }
    os << "\n_Locale và cấu hình chi tiết của fcitx5 do `fcitx5-diagnose` phụ trách._\n";
}

int run(const Options &options) {
    Report report;
    report.session = probe_session();
    report.host = probe_host();
    // The application scan costs one ldd over every installed desktop entry, and
    // the only thing it changes the reading of is a toolkit gap — with a module
    // for everything installed, knowing which apps are GTK4 answers no question
    // anyone asked. So it is paid for only when there is a gap to explain.
    if (host_has_toolkit_gap(report.host)) probe_native_apps(report.host);
    probe_flatpak(report);
    probe_snap(report);
    if (options.deep) {
        std::cerr << "Đang vào từng sandbox để đọc môi trường thật, việc này mất một lúc...\n";
        probe_sandbox_env(report);
    }
    report.fcitx5 = probe_fcitx5();

    Output out;
    judge_session(report.session, out);
    judge_fcitx5(report.fcitx5, report.session, report, out);
    // Host before sandbox: the natively installed applications are the ones a
    // missing module actually breaks, and the fix for them is a package rather
    // than the dead end a sandbox gap usually is.
    judge_host(report.host, report.session, out);
    judge_sandboxes(report, report.session, out);

    if (!options.deep && (report.flatpak_present || report.snap_present)) {
        out.suggestions.push_back(
            "Chạy `telebit doctor --deep` để đọc môi trường thật bên trong từng sandbox "
            "(chậm hơn, nhưng thấy được cả override riêng của từng ứng dụng).");
    }

    if (options.markdown) render_markdown(out, std::cout);
    else render_pretty(out, std::cout, ::isatty(STDOUT_FILENO) != 0, terminal_width());

    return out.has_failure() ? 1 : 0;
}

}  // namespace telebit::doctor
