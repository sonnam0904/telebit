cmake_minimum_required(VERSION 3.21)
project(telebit_fcitx5 VERSION 0.1.0)

# Same rationale as the root CMakeLists.txt: don't ship an unoptimized addon
# just because the caller didn't pass -DCMAKE_BUILD_TYPE=Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

find_package(Fcitx5Core REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

# One version for every artifact, resolved BEFORE add_subdirectory(src) because
# the addon .conf is configured in there. The release tag is the source of truth
# and arrives through the environment (scripts/build-deb.sh, build-rpm.sh); a
# plain from-source build has no such variable, so fall back to the newest tag
# before falling back to PROJECT_VERSION. That fallback is not cosmetic: an
# install.sh user has no package database to query, which makes the addon's own
# Version field the only way they can tell what they are running.
if(DEFINED ENV{TELEBIT_PACKAGE_VERSION} AND NOT "$ENV{TELEBIT_PACKAGE_VERSION}" STREQUAL "")
  set(_telebit_pkg_ver "$ENV{TELEBIT_PACKAGE_VERSION}")
elseif(DEFINED ENV{TELEBIT_DEB_PACKAGE_VERSION} AND NOT "$ENV{TELEBIT_DEB_PACKAGE_VERSION}" STREQUAL "")
  set(_telebit_pkg_ver "$ENV{TELEBIT_DEB_PACKAGE_VERSION}")
else()
  set(_telebit_pkg_ver "${PROJECT_VERSION}")
  find_package(Git QUIET)
  if(Git_FOUND)
    # Released tarballs carry no .git; ERROR_QUIET + the return code keep that
    # case silent instead of noisy.
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _telebit_git_tag
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE _telebit_git_rc)
    if(_telebit_git_rc EQUAL 0 AND
       _telebit_git_tag MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
      set(_telebit_pkg_ver "${CMAKE_MATCH_1}")
    endif()
  endif()
endif()

# fcitx5 reads the addon's Version field through SemanticVersion::parse and
# silently substitutes its default — 0.1.0 — when parsing fails. The packaging
# suffixes are not uniformly semver: "+noble" is valid build metadata, but
# build-rpm.sh rewrites '+' and '-' to '~', which is not. Hand it only the
# MAJOR.MINOR.PATCH core so the field can never quietly become a lie.
if(_telebit_pkg_ver MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
  set(TELEBIT_ADDON_VERSION "${CMAKE_MATCH_1}")
else()
  set(TELEBIT_ADDON_VERSION "${PROJECT_VERSION}")
endif()
message(STATUS "Telebit version: package=${_telebit_pkg_ver} addon=${TELEBIT_ADDON_VERSION}")

# Must be called from the top-level list file: add_test() in a subdirectory is
# silently ignored otherwise, and `ctest` reports "No tests were found".
enable_testing()

add_subdirectory(src)
add_subdirectory(cli)

# systemd user-manager environment drop-in: sets GTK_IM_MODULE/QT_IM_MODULE/
# XMODIFIERS for the graphical session, which is also what makes Flatpak apps
# talk to fcitx5 (they inherit the launcher's environment). See the header of
# env/60-telebit-fcitx5.conf for why this beats shipping `flatpak override`.
#
# environment.d(5) only searches ~/.config/environment.d and the system dirs —
# a --user install into ~/.local must therefore land in ~/.config, not in
# ${CMAKE_INSTALL_PREFIX}/lib where nothing would ever read it.
option(TELEBIT_INSTALL_ENVIRONMENTD "Install the systemd environment.d drop-in that selects fcitx5" ON)

# Deliberately defaults to empty rather than to the computed path: install.sh
# reuses one build dir across --user and --system, and a cached destination
# would keep sending the drop-in to the prefix of whichever ran first.
set(TELEBIT_ENVIRONMENTD_DIR "" CACHE PATH
    "Destination for the environment.d drop-in; empty derives it from CMAKE_INSTALL_PREFIX")

set(_telebit_envd "${TELEBIT_ENVIRONMENTD_DIR}")
if(_telebit_envd STREQUAL "")
  if(CMAKE_INSTALL_PREFIX MATCHES "^/usr(/local)?/?$")
    set(_telebit_envd "lib/environment.d")
  elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(_telebit_envd "$ENV{HOME}/.config/environment.d")
  endif()
endif()

if(TELEBIT_INSTALL_ENVIRONMENTD AND NOT _telebit_envd STREQUAL "")
  message(STATUS "Telebit environment.d drop-in -> ${_telebit_envd}")
  install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/env/60-telebit-fcitx5.conf"
    DESTINATION "${_telebit_envd}"
  )
elseif(TELEBIT_INSTALL_ENVIRONMENTD)
  message(WARNING "No environment.d destination for prefix '${CMAKE_INSTALL_PREFIX}' and no HOME; "
                  "skipping the drop-in. fcitx5 must then be selected by hand (im-config -n fcitx5).")
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "telebit-fcitx5")
set(CPACK_PACKAGE_VENDOR "telebit")
set(CPACK_PACKAGE_VERSION "${_telebit_pkg_ver}")
set(CPACK_PACKAGE_CONTACT "https://github.com/sonnam0904/telebit")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vietnamese Telex input method for fcitx5")

# DEB settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "telebit-fcitx5 <https://github.com/sonnam0904/telebit>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Vietnamese Telex (UTF-8) engine and fcitx5 addon (Telebit). After install, add Telebit in fcitx5-configtool and restart fcitx5.")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "fcitx5, libfcitx5core5 | libfcitx5core7 | libfcitx5core8 | libfcitx5core9 | libfcitx5core10, libcurl4 | libcurl4t64")
# The AI Ctrl+V context feature shells out to wl-paste (Wayland) or xclip (X11).
# Recommend both — a user only runs one display server, so neither is a hard
# dependency, but at least one should be present for the feature to work.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "wl-clipboard, xclip")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Prints the "log out and back in" note for the environment.d drop-in — dpkg
# installs the file but only the next login exports it.
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/packaging/deb/postinst")

# RPM settings
set(CPACK_RPM_PACKAGE_SUMMARY "Vietnamese Telex input method for fcitx5")
set(CPACK_RPM_PACKAGE_NAME "telebit-fcitx5")
set(CPACK_RPM_PACKAGE_VERSION "${_telebit_pkg_ver}")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3+")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES "fcitx5, libcurl")
# See the DEB recommends above: wl-paste / xclip back the AI Ctrl+V context path.
set(CPACK_RPM_PACKAGE_SUGGESTS "wl-clipboard, xclip")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
# See the DEB postinst above — same note, rpm scriptlet form.
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/rpm/post")
# /usr/lib/environment.d is owned by systemd on rpm distros; claiming it would
# conflict on install.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/lib/environment.d")

include(CPack)

