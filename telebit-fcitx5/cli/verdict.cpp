#include "verdict.h"

#include <algorithm>

namespace telebit::doctor {
namespace {

std::string display_or(const std::string &value, const char *fallback) {
    return value.empty() ? std::string(fallback) : value;
}

// Adds one more sentence to a row's note. Assigning instead is how a Fail ended
// up wearing an explanation belonging to a later, milder finding.
void append_note(std::string &note, const std::string &sentence) {
    if (!note.empty()) note += " ";
    note += sentence;
}

std::string join(const std::vector<std::string> &items, const char *separator) {
    std::string out;
    for (const auto &item : items) {
        if (!out.empty()) out += separator;
        out += item;
    }
    return out;
}

// What one of the three IM variables is doing. "Elsewhere" is deliberately a
// separate state from "Missing": an empty variable is a gap that some sessions
// can live with, while one set to ibus actively routes typing away from fcitx5
// and is wrong everywhere.
enum class VarState {
    Correct,
    Missing,
    Elsewhere,
};

VarState classify_im_var(const std::string &value, bool is_xmodifiers) {
    if (value.empty()) return VarState::Missing;
    if (is_xmodifiers) {
        return value == "@im=fcitx" || value == "@im=fcitx5" ? VarState::Correct
                                                             : VarState::Elsewhere;
    }
    return value == "fcitx" || value == "fcitx5" ? VarState::Correct : VarState::Elsewhere;
}

}  // namespace

Section &Output::section(const std::string &title) {
    for (auto &existing : sections) {
        if (existing.title == title) return existing;
    }
    sections.push_back(Section{title, {}});
    return sections.back();
}

bool Output::has_failure() const {
    for (const auto &section : sections) {
        for (const auto &row : section.rows) {
            if (row.status == Status::Fail) return true;
        }
    }
    return false;
}

void judge_session(const SessionInfo &session, Output &out) {
    Section &s = out.section("Phiên làm việc");
    const bool wayland = session.display_server == "wayland";

    std::string server = display_or(session.display_server, "không xác định");
    if (!session.desktop.empty()) server += " · " + session.desktop;
    if (!session.compositor.empty()) server += " · " + session.compositor;
    s.rows.push_back({Status::Info, "Display server", server, ""});

    if (session.env_source.empty()) {
        s.rows.push_back({Status::Warn, "IM env của session", "không đọc được",
                          "Không tìm thấy tiến trình compositor và systemd user manager cũng "
                          "không khai báo biến nào."});
    } else {
        const std::string value = "GTK=" + display_or(session.gtk_im_module, "(trống)") +
                                  "  QT=" + display_or(session.qt_im_module, "(trống)") +
                                  "  XMODIFIERS=" + display_or(session.xmodifiers, "(trống)");
        // All three variables are judged, not just GTK_IM_MODULE. Checking only
        // that one reported a healthy session while every Qt application still
        // talked to ibus.
        const VarState gtk = classify_im_var(session.gtk_im_module, false);
        const VarState qt = classify_im_var(session.qt_im_module, false);
        const VarState xmod = classify_im_var(session.xmodifiers, true);

        Status status = Status::Ok;
        std::string note = "đọc từ " + session.env_source;

        std::vector<std::string> misrouted;
        if (gtk == VarState::Elsewhere) misrouted.push_back("GTK_IM_MODULE=" + session.gtk_im_module);
        if (qt == VarState::Elsewhere) misrouted.push_back("QT_IM_MODULE=" + session.qt_im_module);

        std::vector<std::string> missing;
        if (gtk == VarState::Missing) missing.emplace_back("GTK_IM_MODULE");
        if (qt == VarState::Missing) missing.emplace_back("QT_IM_MODULE");

        // The two are reported independently. Chained as else-if, a session with
        // GTK_IM_MODULE=ibus and QT_IM_MODULE unset printed only the first: the
        // user fixed GTK, every Qt application stayed dead, and doctor mentioned
        // it for the first time on the next run.
        if (!misrouted.empty()) {
            // Wrong everywhere: this does not merely fail to help, it hands the
            // keystrokes to another input method.
            status = Status::Fail;
            note += ". Đang trỏ sang bộ gõ khác: " + join(misrouted, ", ") + ".";
            out.suggestions.push_back(
                "Có biến đang trỏ sang bộ gõ khác (" + join(misrouted, ", ") +
                "). Kiểm tra /etc/environment và các file environment.d sắp sau 60- "
                "(ví dụ 99-environment.conf) xem cái nào ghi đè.");
        }
        if (!missing.empty()) {
            if (wayland) {
                // Never downgrade: a misrouted variable above is broken now,
                // while a missing one here may be survivable on Wayland.
                if (status == Status::Ok) status = Status::Info;
                note += ". Thiếu " + join(missing, ", ") +
                        ": trên Wayland ứng dụng GTK4/Qt6 vẫn gõ được qua text-input-v3, nhưng "
                        "ứng dụng GTK3/Qt5 và ứng dụng chạy qua XWayland thì không.";
            } else {
                status = Status::Fail;
                note += ". Trên X11 thiếu " + join(missing, ", ") +
                        " thì ứng dụng dùng toolkit tương ứng không nạp được module nào của "
                        "fcitx5.";
                out.suggestions.push_back(
                    "Cài lại gói telebit-fcitx5 (nó đặt drop-in environment.d) rồi đăng xuất và "
                    "đăng nhập lại.");
            }
        }
        // Only when nothing above fired: this explains why the two variables are
        // fine to leave empty, which is advice, not a finding.
        if (misrouted.empty() && missing.empty() && wayland &&
            compositor_handles_ime_natively(session.compositor)) {
            // Any compositor implementing zwp_input_method_v2 drives the IME on
            // its own, so this used to be hardcoded to KWin and quietly said
            // nothing on sway, COSMIC or niri, where the same advice applies.
            status = Status::Info;
            note += ". Trên " + session.compositor +
                    " fcitx5 khuyến nghị để trống hai biến này — compositor tự điều khiển IME "
                    "qua text-input-v3.";
        }

        // XMODIFIERS only carries XIM, which nothing but legacy clients still
        // use — Java Swing, xterm, a few older toolkits. Wrong here costs less
        // than wrong above, so it never escalates past a warning, and on a pure
        // Wayland session it is not expected at all.
        if (xmod == VarState::Elsewhere || (xmod == VarState::Missing && !wayland)) {
            if (status == Status::Ok || status == Status::Info) status = Status::Warn;
            note += xmod == VarState::Elsewhere
                        ? " XMODIFIERS đang trỏ sang '" + session.xmodifiers + "'."
                        : std::string(" Thiếu XMODIFIERS.");
            note += " Chỉ ảnh hưởng client XIM (Java Swing, xterm, vài toolkit cũ).";
        }

        s.rows.push_back({status, "IM env của session", value, note});
    }

    // A terminal opened before the last login keeps the old environment, which
    // is the usual explanation for "fcitx5-diagnose says it is wrong but
    // everything works".
    if (!session.env_source.empty() && session.shell_gtk_im_module != session.gtk_im_module) {
        s.rows.push_back({Status::Info, "IM env của shell này",
                          display_or(session.shell_gtk_im_module, "(trống)"),
                          "Khác với session. Terminal này được mở trước lần đăng nhập gần nhất; "
                          "giá trị của session mới là cái các ứng dụng nhận."});
    }

    if (wayland && session.compositor == "mutter") {
        s.rows.push_back({Status::Info, "Giao thức IME của compositor", "ibus (mutter)",
                          "Mutter không hiện thực zwp_input_method_v2, nên fcitx5 phải đóng vai "
                          "ibus daemon thì IME mới hoạt động trên app Wayland."});
    }
}

void judge_fcitx5(const Fcitx5Info &fcitx5, const SessionInfo &session, const Report &report,
                  Output &out) {
    Section &s = out.section("fcitx5");

    if (!fcitx5.running) {
        s.rows.push_back({Status::Fail, "Tiến trình", "không chạy",
                          fcitx5.bus_unavailable
                              ? "Không có tiến trình fcitx5 nào đang chạy."
                              : "Không thấy tên org.fcitx.Fcitx5 trên session bus."});
        out.suggestions.push_back("Khởi động fcitx5: fcitx5 -d");
        return;
    }
    s.rows.push_back({Status::Ok, "Tiến trình", "đang chạy " + display_or(fcitx5.version, ""), ""});

    if (fcitx5.addon_path.empty()) {
        s.rows.push_back({Status::Fail, "Addon Telebit", "không tìm thấy",
                          "Không có telebit-fcitx5.so trong thư mục addon nào của fcitx5."});
        out.suggestions.push_back(
            "Cài addon: xem docs/getting-started/installation.md (bản cài user-local vào "
            "~/.local sẽ không được fcitx5 hệ thống nạp).");
    } else {
        s.rows.push_back({Status::Ok, "Addon Telebit", fcitx5.addon_path, ""});
    }

    // Without busctl none of the frontend checks below can run, and reporting
    // every one of them as "off" would be inventing failures. Say so and stop.
    if (fcitx5.bus_unavailable) {
        s.rows.push_back({Status::Warn, "Frontend", "không kiểm tra được",
                          "Không có lệnh `busctl` nên không đọc được các tên D-Bus của fcitx5. "
                          "Cài gói systemd, hoặc bỏ qua phần này."});
        return;
    }

    // Each frontend is a different door into fcitx5, and which doors are open
    // decides which of the sandbox paths below can possibly work.
    std::string frontends;
    const auto add = [&frontends](bool on, const char *name) {
        if (!on) return;
        if (!frontends.empty()) frontends += ", ";
        frontends += name;
    };
    add(fcitx5.bus_native, "native");
    add(fcitx5.bus_portal, "portal");
    add(fcitx5.bus_fcitx4, "fcitx4");
    add(fcitx5.bus_ibus, "ibus");
    s.rows.push_back({Status::Info, "Frontend đang mở", display_or(frontends, "(không có)"), ""});

    // A name fcitx5 needs but does not hold is the most useful line in the
    // whole report when it appears: it names the process that took it.
    if (!fcitx5.foreign_bus_owners.empty()) {
        s.rows.push_back({Status::Fail, "Tên D-Bus bị chiếm",
                          join(fcitx5.foreign_bus_owners, ", "),
                          "Tiến trình khác đang giữ tên D-Bus mà fcitx5 cần. Ứng dụng sẽ nói "
                          "chuyện với tiến trình đó chứ không phải fcitx5, nên Telebit không bao "
                          "giờ nhận được phím."});
        out.suggestions.push_back(
            "Tắt bộ gõ đang tranh chấp rồi khởi động lại fcitx5. Nếu là ibus: "
            "`systemctl --user mask ibus.service` hoặc gỡ ibus, sau đó `fcitx5 -r`.");
    }

    if (!fcitx5.bus_portal && (report.flatpak_present || report.snap_present)) {
        s.rows.push_back({Status::Warn, "Frontend portal", "tắt",
                          "org.freedesktop.portal.Fcitx là tên D-Bus duy nhất mà ứng dụng trong "
                          "sandbox được phép gọi mặc định. Thiếu nó thì Flatpak/Snap không tới "
                          "được fcitx5."});
        out.suggestions.push_back(
            "Bật addon DBus Frontend trong fcitx5-configtool → Addons để có "
            "org.freedesktop.portal.Fcitx.");
    }

    // The snap platform ships the fcitx4 GTK3 module; without fcitx5's fcitx4
    // compatibility frontend that module has nothing to talk to.
    const bool snap_needs_fcitx4 =
        std::any_of(report.runtimes.begin(), report.runtimes.end(), [](const SandboxRuntime &r) {
            return r.kind == "snap" && r.modules.gtk3 == ModuleKind::Fcitx4;
        });
    if (snap_needs_fcitx4 && !fcitx5.bus_fcitx4) {
        s.rows.push_back({Status::Fail, "Frontend fcitx4", "tắt",
                          "Platform snap chỉ có module GTK3 đời fcitx4, nên app Snap cần addon "
                          "Fcitx4 Frontend của fcitx5 mới gõ được."});
        out.suggestions.push_back(
            "Bật addon Fcitx4 Frontend trong fcitx5-configtool → Addons, rồi fcitx5 -r.");
    }

    if (session.display_server == "wayland" && session.compositor == "mutter" && !fcitx5.bus_ibus) {
        s.rows.push_back({Status::Warn, "Frontend ibus", "tắt",
                          "GNOME Wayland điều khiển IME qua giao thức ibus; thiếu frontend này "
                          "thì app Wayland gốc sẽ không nhận được fcitx5."});
    }
}

// Display name for a runtime. The architecture segment of a Flatpak ref is
// noise in a report about the machine you are already on, and dropping it is
// what keeps the longest of these inside the label column.
std::string runtime_label(const std::string &kind, const std::string &id) {
    std::string trimmed = id;
    for (const char *arch : {"/x86_64", "/aarch64", "/i386", "/arm"}) {
        const auto at = trimmed.find(arch);
        if (at != std::string::npos) {
            trimmed.erase(at, std::string(arch).size());
            break;
        }
    }
    return kind + ":" + trimmed;
}

// An app keeps its id verbatim. runtime_label strips an arch segment, which is
// right for a runtime ref like org.gnome.Platform/x86_64/49 and wrong for an app
// id that merely happens to contain the same substring.
std::string app_label(const std::string &kind, const std::string &id) {
    return kind + ":" + id;
}

const char *module_kind_label(ModuleKind kind) {
    switch (kind) {
        case ModuleKind::Fcitx5: return "fcitx5";
        case ModuleKind::Fcitx4: return "fcitx4";
        case ModuleKind::None: return "—";
    }
    return "—";
}

std::string module_cell(ModuleKind kind, bool toolkit_present) {
    if (kind != ModuleKind::None) return module_kind_label(kind);
    return toolkit_present ? "thiếu" : "n/a";
}

// Turns one runtime's module matrix into a verdict. The subject is the runtime,
// not the app: we cannot tell from the outside whether an app is GTK3, GTK4 or
// Qt, and guessing would produce confident nonsense.
//
// That same limit is why a missing module never reaches Fail here. Fail means
// "something is broken right now", and without knowing an app's toolkit we only
// know the runtime *could* break one — a risk, not an incident. Marking it Fail
// put a ✘ directly above apps that work fine and made the exit status claim a
// healthy machine was broken.
RuntimeVerdict judge_runtime(const SandboxRuntime &runtime, const SessionInfo &session) {
    const ModuleSet &modules = runtime.modules;
    const bool wayland = session.display_server == "wayland";
    const std::string value =
        "GTK3 " + module_cell(modules.gtk3, modules.gtk3_present) + "  ·  GTK4 " +
        module_cell(modules.gtk4, modules.gtk4_present) + "  ·  Qt " +
        module_cell(modules.qt, modules.qt_present);

    Status status = Status::Ok;
    std::vector<std::string> notes;

    if (modules.gtk3 == ModuleKind::Fcitx4) {
        status = Status::Warn;
        notes.push_back(
            "Module GTK3 là bản fcitx4 (im-fcitx.so). Nó vẫn đăng ký context id \"fcitx\" nên "
            "GTK_IM_MODULE=fcitx trỏ vào đây, chạy được nhờ addon tương thích của fcitx5, " +
            std::string(wayland ? "nhưng trên Wayland đây là bước lùi so với text-input-v3."
                                : "và trên X11 đây là đường duy nhất hoạt động."));
    } else if (modules.gtk3 == ModuleKind::None && modules.gtk3_present) {
        status = Status::Warn;
        notes.push_back("Có GTK3 nhưng không có module fcitx nào cho nó.");
    }

    // Only a toolkit that is actually shipped can be missing its module.
    std::vector<std::string> gaps;
    if (modules.gtk4_present && modules.gtk4 == ModuleKind::None) gaps.emplace_back("GTK4");
    if (modules.qt_present && modules.qt == ModuleKind::None) gaps.emplace_back("Qt");
    bool unfixable_gap = false;
    if (!gaps.empty()) {
        std::string list;
        for (const auto &name : gaps) {
            if (!list.empty()) list += "/";
            list += name;
        }
        if (wayland) {
            notes.push_back("Thiếu module " + list +
                            ", nhưng trên Wayland không sao: các toolkit đó đi thẳng "
                            "text-input-v3 qua compositor.");
            if (status == Status::Ok) status = Status::Info;
        } else {
            notes.push_back(
                "Rủi ro: ứng dụng " + list +
                " đóng gói ở đây sẽ không gõ được trên X11, vì module không tồn tại trong "
                "sandbox. Doctor không xác định được ứng dụng nào bên dưới dùng " + list +
                ", nên đây chưa chắc là lỗi đang xảy ra — ứng dụng GTK3 (Chromium, Firefox, "
                "Electron) vẫn đi đường GTK3 ở trên và gõ bình thường.");
            unfixable_gap = true;
            if (status == Status::Ok) status = Status::Warn;
        }
    }

    std::string note;
    for (const auto &fragment : notes) {
        if (!note.empty()) note += " ";
        note += fragment;
    }
    return {{status, runtime_label(runtime.kind, runtime.id), value, note}, unfixable_gap};
}

void judge_sandboxes(const Report &report, const SessionInfo &session, Output &out) {
    if (!report.flatpak_present && !report.snap_present) return;

    Section &s = out.section("Ứng dụng đóng hộp (sandbox)");
    bool suggested_deb = false;
    // The machine-wide override is one file. Suggesting it once per affected app
    // would repeat the same fix as many times as there are Flatpaks installed.
    bool suggested_global_override = false;

    for (const auto &runtime : report.runtimes) {
        // A runtime nothing uses is noise; only report the ones an app is
        // actually sitting on.
        const bool used = std::any_of(report.apps.begin(), report.apps.end(),
                                      [&](const SandboxApp &app) {
                                          return app.runtime_id == runtime.id;
                                      });
        if (!used) continue;

        RuntimeVerdict verdict = judge_runtime(runtime, session);
        if (verdict.unfixable_gap && !suggested_deb) {
            out.suggestions.push_back(
                "Không có cách nào ép module vào một sandbox thiếu nó. Nếu sau này bạn cài một "
                "ứng dụng GTK4 hoặc Qt đóng gói Snap và nó không gõ được: dùng bản .deb/rpm của "
                "ứng dụng đó, hoặc chuyển sang phiên Wayland để nó đi qua text-input-v3.");
            suggested_deb = true;
        }
        s.rows.push_back(std::move(verdict.row));

        for (const auto &app : report.apps) {
            if (app.runtime_id != runtime.id) continue;

            Status status = Status::Info;
            std::string note;
            std::string value;

            if (app.override_gtk_im_module) {
                const std::string &value_set = *app.override_gtk_im_module;
                const bool global = app.override_is_global;
                value = std::string(global ? "override chung GTK_IM_MODULE="
                                           : "override GTK_IM_MODULE=") +
                        display_or(value_set, "(trống)");
                if (value_set != "fcitx" && value_set != "fcitx5") {
                    status = Status::Fail;
                    // A machine-wide override is one setting shown once per app,
                    // not N per-app settings. Saying "của ứng dụng này" sent the
                    // user looking for a per-app override that does not exist.
                    if (global) {
                        append_note(note,
                                    value_set.empty()
                                        ? "Override chung của Flatpak đặt GTK_IM_MODULE thành "
                                          "rỗng cho mọi ứng dụng, tức là tắt hẳn bộ gõ trong "
                                          "sandbox."
                                        : "Override chung của Flatpak ghi đè biến của session "
                                          "cho mọi ứng dụng.");
                        if (!suggested_global_override) {
                            out.suggestions.push_back(
                                "Một override chung của Flatpak đang đặt GTK_IM_MODULE=" +
                                display_or(value_set, "(rỗng)") +
                                " cho mọi ứng dụng. Xem `flatpak override --show`, sửa bằng "
                                "`sudo flatpak override --env=GTK_IM_MODULE=fcitx` hoặc gỡ hẳn "
                                "bằng `sudo flatpak override --reset`.");
                            suggested_global_override = true;
                        }
                    } else {
                        append_note(note,
                                    value_set.empty()
                                        ? "Override của ứng dụng này đặt GTK_IM_MODULE thành "
                                          "rỗng, tức là tắt hẳn bộ gõ bên trong sandbox của nó."
                                        : "Override riêng của ứng dụng này ghi đè biến của "
                                          "session.");
                        out.suggestions.push_back(
                            "Override riêng của " + app.id + " đang đặt GTK_IM_MODULE=" +
                            display_or(value_set, "(rỗng)") + ". Sửa bằng `flatpak override --user "
                            "--env=GTK_IM_MODULE=fcitx " + app.id + "`.");
                    }
                }
            }
            if (!app.unconnected_interfaces.empty()) {
                std::string list;
                for (const auto &name : app.unconnected_interfaces) {
                    if (!list.empty()) list += ", ";
                    list += name;
                }
                if (!value.empty()) value += "  ·  ";
                value += "interface chưa nối: " + list;
                // Never downgrade: a Fail already set above describes something
                // that is broken now, and this is only a warning.
                if (status == Status::Info) status = Status::Warn;
                append_note(note, "Chưa nối interface thì snap không tới được fcitx5.");
                out.suggestions.push_back("snap connect " + app.id + ":" +
                                          app.unconnected_interfaces.front());
            }
            if (app.deep_failed) {
                // The probe broke, not the input method. Reporting this as an
                // empty variable would blame the user's setup for our timeout.
                if (!value.empty()) value += "  ·  ";
                value += "không dò sâu được";
                if (status == Status::Info) status = Status::Warn;
                // Appended, not assigned: replacing the note would delete the
                // explanation of a Fail found above and leave a red row whose
                // only text says nothing was concluded.
                append_note(note,
                            "Không khởi động được sandbox để đọc môi trường (ứng dụng lỗi, hoặc "
                            "quá 25 giây). Riêng phép đo này không kết luận gì về bộ gõ.");
            } else if (app.deep_probed) {
                if (!value.empty()) value += "  ·  ";
                value += "trong sandbox: GTK_IM_MODULE=" +
                         display_or(app.deep_gtk_im_module, "(trống)");
                if (app.deep_gtk_im_module.empty() && session.display_server != "wayland") {
                    status = Status::Fail;
                    // Appended for the same reason as the branch above, and only
                    // when nothing already accounts for the emptiness: an app
                    // whose own override sets GTK_IM_MODULE empty *did* receive
                    // the variable and cleared it itself, so claiming it never
                    // arrived would print a second, wrong cause beside the real
                    // one — and point the user at environment.d instead of at
                    // the one thing they can fix, `flatpak override --reset`.
                    const bool emptied_by_own_override =
                        app.override_gtk_im_module.has_value() &&
                        app.override_gtk_im_module->empty();
                    if (!emptied_by_own_override) {
                        append_note(note, "Biến không tới được bên trong sandbox.");
                    }
                }
            }

            s.rows.push_back({status, app.id, display_or(value, "—"), note, 1});
        }
    }

    // Apps whose runtime could not be resolved still deserve a line, otherwise
    // the report silently under-reports what is installed.
    std::vector<const SandboxApp *> unresolved;
    for (const auto &app : report.apps) {
        const bool matched = std::any_of(report.runtimes.begin(), report.runtimes.end(),
                                         [&](const SandboxRuntime &runtime) {
                                             return runtime.id == app.runtime_id;
                                         });
        if (!matched) unresolved.push_back(&app);
    }

    // Grouped under one heading instead of emitted straight at depth 0. Every
    // other top-level row is a runtime or platform snap — which is exactly what
    // the reference page tells the reader — so an app sitting up there was read
    // as a runtime doctor had failed to analyse, and the reader went looking for
    // a platform snap by that name.
    if (!unresolved.empty()) {
        s.rows.push_back({Status::Info, "Không gắn được runtime",
                          std::to_string(unresolved.size()) + " ứng dụng",
                          "Những ứng dụng dưới đây không nằm trên runtime nào doctor nhận ra, nên "
                          "phán quyết về module ở trên không áp dụng cho chúng."});
        for (const SandboxApp *app : unresolved) {
            if (!app->runtime_id.empty()) {
                // The app names a runtime that is not on disk, so it cannot
                // start at all — worth saying plainly rather than filing under
                // "unknown".
                s.rows.push_back({Status::Warn, app_label(app->kind, app->id),
                                  "runtime " + app->runtime_id + " chưa cài",
                                  "Ứng dụng khai báo runtime này nhưng nó không có trên máy.", 1});
            } else {
                s.rows.push_back({Status::Info, app_label(app->kind, app->id),
                                  "runtime không xác định",
                                  "Không tìm ra content snap / runtime nào cho ứng dụng này nên "
                                  "doctor không kết luận được.", 1});
            }
        }
    }
}

}  // namespace telebit::doctor
