/*
 * telebit-fcitx5 — identifying the application behind an input context.
 *
 * Two questions live here: whether a program is one whose text fields need
 * preedit mode, and what to call it in the configtool's application list.
 *
 * Deliberately free of fcitx5: the answers are pure functions of a program name
 * and a set of directories, so the test suite can ask them without a live fcitx
 * session and without depending on which applications the build machine happens
 * to have installed.
 */

#ifndef TELEBIT_APP_IDENTITY_H
#define TELEBIT_APP_IDENTITY_H

#include <string>
#include <vector>

namespace telebit {

/// True for a window drawn by a browser engine — the browsers themselves and
/// the PWA/site-specific windows they spawn. Argument must already be
/// lower-cased.
bool isDefaultPreeditProgram(const std::string &programLower);

/// True when the name belongs to a PWA / app-mode browser window rather than to
/// a browser's own main window. Argument must already be lower-cased.
bool isPwaProgram(const std::string &programLower);

/// Directories holding desktop entries, in XDG lookup order (user first).
std::vector<std::string> desktopEntryDirs();

/// `Name=` of the desktop entry whose *file name* matches `programLower`
/// (case-insensitively), or empty when there is no such entry.
std::string desktopEntryName(const std::string &programLower,
                             const std::vector<std::string> &dirs);

/// `Name=` from the contents of one desktop entry. Exposed for the tests.
std::string desktopEntryNameFromContents(const std::string &contents);

/// What the configtool shows as the row title: the desktop entry's name when
/// one can be found, else the program name itself.
std::string displayLabelFor(const std::string &programLower,
                            const std::vector<std::string> &dirs);

}  // namespace telebit

#endif  // TELEBIT_APP_IDENTITY_H
