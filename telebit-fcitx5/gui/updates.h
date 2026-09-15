// Is the installed Telebit the newest one, and what would updating it mean on
// this particular machine?
//
// Two questions, because the answer to the second is not the same everywhere.
// Telebit ships through four channels — an APT repo, a hand-downloaded .deb, an
// .rpm, and install.sh from source — and only the first two can be upgraded by
// a package manager that already knows where the package came from. Offering
// one "update" action to all four would be a button that silently does nothing
// for half the people who press it, so the check reports what it found and the
// caller gets an action that matches.
//
// Everything here blocks: one HTTPS request and up to two package-manager
// queries. It is meant to be called from a worker thread.

#pragma once

#include <string>

namespace telebit::setup::update {

// How the running copy was installed, as far as the package databases admit.
enum class Method {
    Unknown,  // install.sh, a hand-unpacked build, or no package manager
    Apt,
    Dnf,
};

struct Check {
    // The network half. `error` is non-empty exactly when `latest` is empty:
    // being offline is a normal state for this window and must read as "not
    // checked", never as "you are up to date".
    std::string latest;  // "2.15.0", without the tag's leading v
    std::string error;
    bool newer = false;  // latest is strictly newer than what is installed

    // What the package database says is installed, which is not the same thing
    // as the version compiled into this process. An upgrade replaces the binary
    // on disk while the window keeps running the old inode — so after pressing
    // the update button, the compiled-in number is stale by construction, and
    // reporting it would tell the user the upgrade did nothing.
    //
    // Empty when no package manager knows about Telebit, in which case the
    // running version is the only answer available.
    std::string installed;

    // The running process is older than what is installed: the upgrade landed,
    // but this window is still the previous binary and has to be reopened
    // before it can describe itself accurately.
    bool window_is_stale = false;

    // The upgrade half.
    Method method = Method::Unknown;

    // What the package manager would actually install, which is not always the
    // release GitHub just announced: an APT repo that has not been `apt
    // update`d yet, or a suite that has not been published to, offers an older
    // one. The button is labelled from this rather than from `latest`, because
    // a button that promises 2.15.0 and installs 2.14.0 is a lie the user only
    // discovers afterwards.
    std::string candidate;

    // True only when `candidate` is strictly newer than the running version.
    // An upgrade that would change nothing still exits 0, so without this the
    // window would report a successful update and the same version afterwards.
    bool upgradable = false;
    std::string command;  // human-readable, for the note under the button
};

// MAJOR.MINOR.PATCH with any packaging decoration removed: "2.14.0+noble",
// "2.14.0~noble" and "2.14.0-1" all come back as "2.14.0". What the window
// shows a user, since the suffix names the build suite and not the release.
std::string version_core(const std::string &version);

// The running version, as compiled in, through version_core(). This describes
// the process, not the machine — see Check::installed.
std::string current_version();

// The version the package database holds, or empty when neither dpkg nor rpm
// has heard of Telebit. Blocking: spawns the package tool.
std::string installed_version();

// What the window should call "the installed Telebit": the package database's
// answer when there is one, else the running process's own.
std::string effective_version(const Check &check);

// Blocking. Safe to call when offline: the result then carries `error`.
Check check();

// Compares dotted numeric versions. Exposed for the tests, because "2.9.0" is
// newer than "2.10.0" under every string comparison and that is exactly the
// kind of bug this would hide for a year.
bool is_newer(const std::string &candidate, const std::string &current);

// Parses the `tag_name` out of a GitHub releases API response. Exposed for the
// tests; a hand-rolled scan rather than a JSON parser, because one field out of
// one endpoint does not earn a dependency.
std::string tag_from_release_json(const std::string &json);

const char *releases_url();

}  // namespace telebit::setup::update
