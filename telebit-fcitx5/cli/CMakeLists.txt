# The doctor CLI deliberately links nothing from fcitx5. It inspects the system
# from the outside — busctl, the filesystem, /proc — so staying free of
# Fcitx5::Core means it keeps building and keeps running in exactly the broken
# states it exists to explain.
add_executable(telebit_cli
    main.cpp
    doctor.cpp
    probe.cpp
    textfmt.cpp
    verdict.cpp
)

set_target_properties(telebit_cli PROPERTIES OUTPUT_NAME "telebit")
target_compile_features(telebit_cli PRIVATE cxx_std_17)

# Same version string the addon reports, so `telebit --version` and the addon's
# Version field can never disagree.
#
# TELEBIT_ADDON_DIR is the directory src/CMakeLists.txt actually installs the
# addon into. Handing it over makes the search authoritative instead of guessing
# from a prefix list: on Fedora/openSUSE that path is /usr/lib64/fcitx5, and
# /usr/lib64 is a *sibling* of /usr/lib, so no amount of scanning /usr/lib finds it.
target_compile_definitions(telebit_cli PRIVATE
    TELEBIT_VERSION="${_telebit_pkg_ver}"
    TELEBIT_ADDON_DIR="${FCITX_INSTALL_LIBDIR}/fcitx5")

include(GNUInstallDirs)
install(TARGETS telebit_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Tests cover what can be asserted deterministically: textfmt, verdict, and the
# compositor table in probe.cpp. probe.cpp is linked for that table alone — its
# probe_* entry points read the live system and are never called from here.
add_executable(telebit_doctor_tests
    tests.cpp
    doctor.cpp
    probe.cpp
    textfmt.cpp
    verdict.cpp
)
target_compile_features(telebit_doctor_tests PRIVATE cxx_std_17)
target_compile_definitions(telebit_doctor_tests PRIVATE
    TELEBIT_ADDON_DIR="${FCITX_INSTALL_LIBDIR}/fcitx5")

# No install() rule for this target on purpose — it is for ctest and install.sh,
# and shipping it would put a test binary in the user's /usr/bin.
add_test(NAME doctor COMMAND telebit_doctor_tests)
