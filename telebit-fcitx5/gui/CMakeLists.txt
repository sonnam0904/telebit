# telebit-setup: the doctor report as a window. It links telebit_doctor_core
# for the probing and judging, so the two front ends can never disagree about
# what is wrong with a machine.
add_executable(telebit_setup
    main.cpp
    setup_page.cpp
    macros_page.cpp
    apps_page.cpp
    status_page.cpp
    widgets.cpp
    fcitx5_bus.cpp
    updates.cpp
)
set_target_properties(telebit_setup PROPERTIES OUTPUT_NAME "telebit-setup")
# libcurl for the release check on GitHub. No packaging work: the addon in the
# same binary package already pulls libcurl in, and dpkg-shlibdeps derives the
# dependency from the ELF rather than from this list.
target_link_libraries(telebit_setup PRIVATE telebit_doctor_core PkgConfig::GTK4 CURL::libcurl)

# Shown in the corner of the window; same string as `telebit --version` and the
# addon's Version field.
#
# The version fence is the load-bearing part: MAX_ALLOWED turns any API newer
# than jammy's GTK 4.6 / GLib 2.72 into a deprecated-declarations diagnostic,
# and -Werror= makes it fatal. Without it a 4.10-only call compiles fine on
# this machine and on noble, and only breaks when the jammy container builds.
# MIN_REQUIRED silences deprecations introduced *after* those versions, which
# is what keeps gtk_css_provider_load_from_data (deprecated 4.12) quiet.
target_compile_definitions(telebit_setup PRIVATE
    TELEBIT_VERSION="${_telebit_pkg_ver}"
    GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_6
    GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_6
    GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_72
    GLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_72)
target_compile_options(telebit_setup PRIVATE -Werror=deprecated-declarations)

# The release check's two decidable halves: version ordering and reading the tag
# out of GitHub's answer. check() itself is not covered — it reaches the network
# and the package databases, so its result describes the machine, not the code.
add_executable(telebit_updates_tests updates_tests.cpp updates.cpp)
target_link_libraries(telebit_updates_tests PRIVATE PkgConfig::GTK4 CURL::libcurl)
target_compile_definitions(telebit_updates_tests PRIVATE TELEBIT_VERSION="${_telebit_pkg_ver}")
add_test(NAME updates COMMAND telebit_updates_tests)

include(GNUInstallDirs)
install(TARGETS telebit_setup RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
