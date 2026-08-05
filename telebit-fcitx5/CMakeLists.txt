cmake_minimum_required(VERSION 3.21)
project(telebit_fcitx5 VERSION 0.1.0)

# Same rationale as the root CMakeLists.txt: don't ship an unoptimized addon
# just because the caller didn't pass -DCMAKE_BUILD_TYPE=Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

find_package(Fcitx5Core REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_subdirectory(src)

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

if(DEFINED ENV{TELEBIT_PACKAGE_VERSION} AND NOT "$ENV{TELEBIT_PACKAGE_VERSION}" STREQUAL "")
  set(_telebit_pkg_ver "$ENV{TELEBIT_PACKAGE_VERSION}")
elseif(DEFINED ENV{TELEBIT_DEB_PACKAGE_VERSION} AND NOT "$ENV{TELEBIT_DEB_PACKAGE_VERSION}" STREQUAL "")
  set(_telebit_pkg_ver "$ENV{TELEBIT_DEB_PACKAGE_VERSION}")
else()
  set(_telebit_pkg_ver "${PROJECT_VERSION}")
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

