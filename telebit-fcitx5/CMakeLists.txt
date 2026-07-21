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
set(CPACK_DEBIAN_PACKAGE_DEPENDS "fcitx5, libfcitx5core5 | libfcitx5core7 | libfcitx5core8 | libfcitx5core9 | libfcitx5core10")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# RPM settings
set(CPACK_RPM_PACKAGE_SUMMARY "Vietnamese Telex input method for fcitx5")
set(CPACK_RPM_PACKAGE_NAME "telebit-fcitx5")
set(CPACK_RPM_PACKAGE_VERSION "${_telebit_pkg_ver}")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3+")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES "fcitx5")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

include(CPack)

