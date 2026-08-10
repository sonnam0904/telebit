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
    // The host reaches the same state whenever the module installed for a toolkit
    // is the fcitx4-era one (the old fcitx-frontend-* packages), which still
    // answers GTK_IM_MODULE=fcitx. Checking snaps alone meant a natively
    // installed desktop in exactly that state was told nothing at all.
    //
    // Gated on the variable that would load it, because a module nothing routes
    // to is not load-bearing: on Wayland with the IM variables unset, GTK and Qt
    // reach the compositor directly and never open the fcitx4 module — judge_host
    // says exactly that for the same state, and an ungated Fail here made
    // `telebit doctor` exit 1 on a machine where nothing was wrong.
    std::vector<std::string> host_legacy;
    const bool gtk_routed = classify_im_var(session.gtk_im_module, false) == VarState::Correct;
    const bool qt_routed = classify_im_var(session.qt_im_module, false) == VarState::Correct;
    if (gtk_routed) {
        if (report.host.gtk3.module == ModuleKind::Fcitx4) host_legacy.emplace_back("GTK3");
        if (report.host.gtk4.module == ModuleKind::Fcitx4) host_legacy.emplace_back("GTK4");
    }
    if (qt_routed) {
        if (report.host.qt5.module == ModuleKind::Fcitx4) host_legacy.emplace_back("Qt5");
        if (report.host.qt6.module == ModuleKind::Fcitx4) host_legacy.emplace_back("Qt6");
    }
    if ((snap_needs_fcitx4 || !host_legacy.empty()) && !fcitx5.bus_fcitx4) {
        std::vector<std::string> who;
        if (snap_needs_fcitx4) who.emplace_back("platform snap chỉ có module GTK3 đời fcitx4");
        // Names the toolkits that are actually affected: hardcoding "GTK3" told a
        // user whose fcitx4-era module was the Qt5 plugin to look at GTK.
        if (!host_legacy.empty()) {
            who.emplace_back("module " + join(host_legacy, "/") +
                             " trên host là bản đời fcitx4");
        }
        s.rows.push_back({Status::Fail, "Frontend fcitx4", "tắt",
                          join(who, ", và ") +
                              ", nên ứng dụng dùng các toolkit đó cần addon Fcitx4 Frontend của "
                              "fcitx5 mới gõ được."});
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

AffectedApps apps_using(const HostInfo &host, bool NativeApp::*toolkit) {
    AffectedApps affected;
    affected.scanned = host.apps.size();
    for (const auto &app : host.apps) {
        if (app.toolkit_unknown) {
            ++affected.unknown;
            continue;
        }
        if (app.*toolkit) affected.names.push_back(app.name);
    }
    std::sort(affected.names.begin(), affected.names.end());
    return affected;
}

namespace {

// How many application names a row shows before summarising the rest. Eight fits
// the value column at any terminal width the report supports; the remainder is
// always counted out loud, because a list silently cut short reads as the whole
// answer.
constexpr std::size_t kMaxNamedApps = 8;

std::string app_list_value(const AffectedApps &affected) {
    std::vector<std::string> shown(
        affected.names.begin(),
        affected.names.begin() +
            static_cast<std::ptrdiff_t>(std::min(kMaxNamedApps, affected.names.size())));
    std::string value = join(shown, ", ");
    if (affected.names.size() > kMaxNamedApps) {
        value += " … và " + std::to_string(affected.names.size() - kMaxNamedApps) + " app nữa";
    }
    return value;
}

// What the scan could not see. Every row built from the application list carries
// this, so a machine whose apps are mostly wrapper scripts never gets a confident
// answer it did not earn.
std::string app_scan_coverage(const AffectedApps &affected) {
    std::string note = "Dò " + std::to_string(affected.scanned) +
                       " ứng dụng cài trực tiếp (từ desktop entry của /usr/share/applications, "
                       "không tính Flatpak/Snap)";
    if (affected.unknown == 0) return note + ".";
    return note + "; " + std::to_string(affected.unknown) +
           " app không đọc được toolkit (script bao ngoài, hoặc tự dlopen toolkit như Firefox, "
           "LibreOffice) nên danh sách này có thể còn thiếu.";
}

}  // namespace

void judge_host(const HostInfo &host, const SessionInfo &session, Output &out) {
    Section &s = out.section("Ứng dụng cài trực tiếp (.deb/.rpm)");

    // No root to scan is a failed measurement. Reporting the module matrix of a
    // scan that never happened would announce four missing modules on a machine
    // that has them all.
    if (host.lib_roots.empty()) {
        s.rows.push_back({Status::Warn, "Module trên host", "không đọc được",
                          "Không thấy thư mục library nào (/usr/lib, /usr/lib64, /usr/local/lib) "
                          "để dò module, nên phần này không kết luận gì."});
        return;
    }

    const bool wayland = session.display_server == "wayland";

    // One toolkit the host may have, the package that ships its module, and the
    // flag on NativeApp that says an application links it. The member pointer is
    // what lets the affected-application list be derived from this one table
    // instead of a second switch that could disagree with it.
    struct Toolkit {
        const char *name;
        const char *debian_package;
        HostToolkit state;
        bool NativeApp::*used_by;
    };

    // One row per toolkit family, each judged against the variable that family
    // actually reads. Which of GTK_IM_MODULE / QT_IM_MODULE points at fcitx is
    // what decides whether a missing module is fatal or irrelevant, so a single
    // row covering both would carry one status for two unrelated verdicts — a
    // broken GTK and a healthy Qt came out as one red line whose note had to
    // explain both, and the four cells no longer fit the value column.
    struct Family {
        const char *label;
        const char *variable_name;
        std::string variable;
        std::vector<Toolkit> toolkits;
    };
    const std::vector<Family> families = {
        {"Module GTK trên host",
         "GTK_IM_MODULE",
         session.gtk_im_module,
         {{"GTK3", "fcitx5-frontend-gtk3", host.gtk3, &NativeApp::gtk3},
          {"GTK4", "fcitx5-frontend-gtk4", host.gtk4, &NativeApp::gtk4}}},
        {"Module Qt trên host",
         "QT_IM_MODULE",
         session.qt_im_module,
         {{"Qt5", "fcitx5-frontend-qt5", host.qt5, &NativeApp::qt5},
          {"Qt6", "fcitx5-frontend-qt6", host.qt6, &NativeApp::qt6}}},
    };

    std::vector<std::string> missing;
    std::vector<std::string> packages;
    bool coverage_explained = false;

    for (const auto &family : families) {
        std::string value;
        std::vector<std::string> gaps;
        std::vector<std::string> legacy;
        std::vector<std::string> legacy_packages;
        for (const auto &toolkit : family.toolkits) {
            if (!value.empty()) value += "  ·  ";
            value += std::string(toolkit.name) + " " +
                     module_cell(toolkit.state.module, toolkit.state.present);
            // Only a toolkit the host actually has can be missing its module.
            if (toolkit.state.present && toolkit.state.module == ModuleKind::None) {
                gaps.emplace_back(toolkit.name);
                packages.emplace_back(toolkit.debian_package);
            }
            if (toolkit.state.module == ModuleKind::Fcitx4) {
                legacy.emplace_back(toolkit.name);
                legacy_packages.emplace_back(toolkit.debian_package);
            }
        }

        // The application list is read *before* the verdict, not after it. Pushing
        // the row first and listing applications afterwards meant the status was
        // already Fail by the time the scan reported that nothing on the machine
        // uses the missing toolkit — the row said "broken" while the row under it
        // said "affects no application", and the exit status backed the wrong one.
        struct GapApps {
            const char *toolkit;
            bool NativeApp::*used_by;
            AffectedApps affected;
        };
        std::vector<GapApps> gap_apps;
        if (host.apps_scanned) {
            for (const auto &toolkit : family.toolkits) {
                if (!(toolkit.state.present && toolkit.state.module == ModuleKind::None)) continue;
                gap_apps.push_back({toolkit.name, toolkit.used_by,
                                    apps_using(host, toolkit.used_by)});
            }
        }
        // Conclusive only when applications were actually inspected AND the scan
        // answered for every one it found: one unreadable wrapper script is enough
        // to make "nothing uses GTK4" a guess, an empty list is not evidence at
        // all, and neither may clear a failure.
        const bool gap_hits_nothing =
            !gap_apps.empty() &&
            std::all_of(gap_apps.begin(), gap_apps.end(), [](const GapApps &entry) {
                return entry.affected.scanned > 0 && entry.affected.names.empty() &&
                       entry.affected.unknown == 0;
            });

        Status status = Status::Ok;
        std::vector<std::string> notes;

        if (!gaps.empty()) {
            const std::string list = join(gaps, "/");
            missing.insert(missing.end(), gaps.begin(), gaps.end());

            if (classify_im_var(family.variable, false) == VarState::Correct) {
                // The variable names a module that is not installed. The toolkit
                // does not fall back to the compositor here — it falls back to its
                // own built-in context — so this is broken right now, Wayland
                // included. That is also why a host gap can reach Fail while the
                // identical sandbox gap never does: there the app's toolkit is
                // unknown, here the session has already declared what it wants.
                //
                // Unless the scan proved there is nobody to break: the downgrade
                // is applied only to this branch, because it is the only one that
                // would otherwise claim a failure. Applied ahead of the Wayland
                // case below it would *raise* a harmless Info to a warning.
                if (gap_hits_nothing) {
                    status = Status::Warn;
                    notes.push_back("Thiếu module " + list + ", và " + family.variable_name +
                                    " đang trỏ vào nó, nhưng không ứng dụng cài trực tiếp nào trên "
                                    "máy dùng " + list +
                                    " — nên đây là rủi ro chưa xảy ra, không phải lỗi đang hỏng. "
                                    "Cài một app " + list + " thì nó thành lỗi thật.");
                } else {
                    status = Status::Fail;
                    notes.push_back(std::string(family.variable_name) + "=" + family.variable +
                                    " trỏ tới module " + list + " không có trên máy, nên ứng dụng " +
                                    list +
                                    " cài trực tiếp rơi về bộ gõ mặc định của toolkit và không gõ "
                                    "được — kể cả trên Wayland, vì biến này chặn luôn đường "
                                    "text-input-v3.");
                }
            } else if (wayland) {
                status = Status::Info;
                notes.push_back("Thiếu module " + list + ", nhưng " + family.variable_name +
                                " đang để trống nên ứng dụng " + list +
                                " đi thẳng text-input-v3 qua compositor.");
            } else {
                // On X11 there is no other path, but judge_session already fails
                // for the empty variable — this row carries the other half of the
                // fix rather than reporting the same incident twice.
                status = Status::Warn;
                notes.push_back("Trên X11 ứng dụng " + list +
                                " cài trực tiếp không có module nào của fcitx5 để nạp.");
            }
        }

        // A module from the fcitx4 era still satisfies the variable, so nothing
        // above notices it. It only works while fcitx5's compatibility frontend
        // is loaded, which judge_fcitx5 checks — the row's job is to say why that
        // frontend is suddenly load-bearing on a machine with no snaps at all.
        if (!legacy.empty()) {
            // Raised from Info as well as Ok, and never from Fail. Guarding on Ok
            // alone left a row that carries an actionable suggestion wearing the
            // `·` glyph the legend defines as "not an analysis result" — the same
            // guard judge_session uses spells out both.
            if (status == Status::Ok || status == Status::Info) status = Status::Warn;
            notes.push_back("Module " + join(legacy, "/") +
                            " là bản đời fcitx4 (gói fcitx-frontend-* cũ). Nó vẫn đăng ký context "
                            "id \"fcitx\" nên biến trỏ vào đây, và chỉ chạy được khi addon Fcitx4 "
                            "Frontend của fcitx5 đang bật.");
            out.suggestions.push_back(
                "Module " + join(legacy, "/") +
                " trên máy là bản fcitx4. Cài bản fcitx5 (Debian/Ubuntu: `sudo apt install " +
                join(legacy_packages, " ") +
                "`) để ứng dụng đi đường native thay vì qua lớp tương thích fcitx4.");
        }

        s.rows.push_back({status, family.label, value, join(notes, " ")});

        // Naming the applications is what turns "GTK4 thiếu" from a risk into a
        // finding with a subject. Only positive matches are ever listed: an
        // application whose toolkit could not be read is counted in the note, not
        // described as unaffected — claiming Firefox does not use GTK because its
        // launcher dlopens libxul would be exactly the confident nonsense the
        // sandbox section refuses to print.
        for (const auto &entry : gap_apps) {
            const AffectedApps &affected = entry.affected;
            const std::string label = std::string("app dùng ") + entry.toolkit;

            // One scan, one description of its limits. Repeating it under every
            // affected toolkit filled the section with four identical lines and
            // pushed the findings off the screen.
            std::string note;
            if (!coverage_explained) {
                note = app_scan_coverage(affected);
                coverage_explained = true;
            }

            if (affected.names.empty()) {
                // "No application links this toolkit" is a claim, and it needs
                // applications to have been inspected. With nothing inspected the
                // row says that instead — asserting the claim from an empty list
                // is what let a real failure read as harmless.
                if (affected.scanned == 0) {
                    append_note(note,
                                "Không dò được ứng dụng nào để đối chiếu, nên chưa kết luận được "
                                "ô \"thiếu\" ở trên có ảnh hưởng ứng dụng nào không.");
                } else if (affected.unknown == 0) {
                    append_note(note,
                                "Không app nào liên kết toolkit này, nên ô \"thiếu\" ở trên hiện "
                                "chưa ảnh hưởng ứng dụng nào.");
                }
                s.rows.push_back({affected.scanned == 0 ? Status::Warn : Status::Info, label,
                                  affected.scanned == 0 ? "không dò được app nào"
                                                        : "không thấy app nào",
                                  note, 1});
                continue;
            }
            // Same finding as the row above, itemised — so it wears the same
            // status rather than a milder one of its own.
            s.rows.push_back({status, label, app_list_value(affected), note, 1});
        }
    }

    if (!packages.empty()) {
        // Both naming schemes are named because nothing on the filesystem says
        // which one this machine uses, and Fedora/Arch ship one package per
        // toolkit family rather than one per version.
        out.suggestions.push_back(
            "Thiếu module client cho " + join(missing, ", ") +
            ". Debian/Ubuntu: `sudo apt install " + join(packages, " ") +
            "`. Fedora/openSUSE/Arch: module nằm chung trong `fcitx5-gtk` (GTK) và `fcitx5-qt` "
            "(Qt). Cài xong thì đăng xuất rồi đăng nhập lại.");
    }

    // One verdict from one library root, so the file named as installed and the
    // file named as stale always come from the same place. NoModule needs no row:
    // the module matrix above already says everything, and a cache that registers
    // nothing is then simply correct.
    const Gtk3Cache &cache = host.gtk3_cache;
    const bool gtk_wants_fcitx = classify_im_var(session.gtk_im_module, false) == VarState::Correct;
    const std::string module_file = display_or(cache.module_file, "fcitx");

    switch (cache.state) {
        case Gtk3CacheState::NoModule:
            break;
        case Gtk3CacheState::Registered:
            s.rows.push_back({Status::Ok, "Cache immodule GTK3", "đăng ký " + module_file, ""});
            break;
        case Gtk3CacheState::NoCache:
            s.rows.push_back({Status::Warn, "Cache immodule GTK3", "không có file cache",
                              "GTK3 chỉ nạp immodule nào được liệt kê trong immodules.cache, nên "
                              "module " + module_file + " sẽ không bao giờ được nạp."});
            out.suggestions.push_back(
                "Dựng lại cache immodule của GTK3: `sudo gtk-query-immodules-3.0 --update-cache` "
                "(trên Debian/Ubuntu lệnh này nằm trong /usr/lib/<triplet>/libgtk-3-0*/), hoặc cài "
                "lại gói module: `sudo apt install --reinstall fcitx5-frontend-gtk3`.");
            break;
        case Gtk3CacheState::NotRegistered: {
            // Naming the file the cache still points at is the whole diagnosis: a
            // cache holding im-fcitx.so beside an installed im-fcitx5.so is not
            // "no fcitx registered", it is the previous package's cache, and
            // saying so is what tells the user which command to re-run.
            const bool stale_entry = !cache.other_fcitx.empty();
            std::string note = "Module " + module_file +
                               " có trên đĩa nhưng cache không đăng ký nó, và GTK3 chỉ nạp những gì "
                               "cache liệt kê.";
            if (stale_entry) {
                note += " Cache đang đăng ký " + cache.other_fcitx +
                        " — bản module của gói cũ, tức là cache chưa được dựng lại sau khi cài "
                        "module mới.";
            } else {
                note += " Cache thường bị cũ khi module được copy tay hoặc khi trigger của gói "
                        "không chạy.";
            }
            s.rows.push_back({gtk_wants_fcitx ? Status::Fail : Status::Warn, "Cache immodule GTK3",
                              stale_entry ? "đăng ký " + cache.other_fcitx
                                          : std::string("không đăng ký module"),
                              note});
            out.suggestions.push_back(
                "Dựng lại cache immodule của GTK3: `sudo gtk-query-immodules-3.0 --update-cache` "
                "(trên Debian/Ubuntu lệnh này nằm trong /usr/lib/<triplet>/libgtk-3-0*/), hoặc cài "
                "lại gói module: `sudo apt install --reinstall fcitx5-frontend-gtk3`.");
            break;
        }
    }
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
