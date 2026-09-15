# Deciding which application gets preedit mode, and what to call it in the
# configtool, are pure functions of a program name and a set of directories.
# Kept in their own library — and free of Fcitx5::Core — so the tests below can
# ask them directly: the addon itself cannot be linked into a test binary
# without a live fcitx instance to attach to.
add_library(telebit_app_identity STATIC app_identity.cpp)
target_compile_features(telebit_app_identity PUBLIC cxx_std_17)
target_include_directories(telebit_app_identity PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# It is linked into a shared object, so its objects have to be relocatable.
set_target_properties(telebit_app_identity PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(telebit_app_identity_tests app_identity_tests.cpp)
target_link_libraries(telebit_app_identity_tests PRIVATE telebit_app_identity)

# No install() rule on purpose — same reason as the doctor suite: this is for
# ctest and install.sh, not for the user's /usr/bin.
add_test(NAME app_identity COMMAND telebit_app_identity_tests)

add_library(telebit_fcitx5 SHARED
    telebit_fcitx5.cpp
    ../../ai_client.cpp
    ../../engine.cpp
    ../../vietnamese.cpp
    ../../rime_table.cpp
    ../../canonicalize.cpp
    ../../render_utf8.cpp
)

target_include_directories(telebit_fcitx5 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(telebit_fcitx5 PRIVATE Fcitx5::Core CURL::libcurl telebit_app_identity)

# Produce telebit-fcitx5.so instead of libtelebit_fcitx5.so
set_target_properties(telebit_fcitx5 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "telebit-fcitx5"
)

install(TARGETS telebit_fcitx5 DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

# Addon config file
configure_file(telebit-fcitx5-addon.conf.in telebit-fcitx5-addon.conf)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/telebit-fcitx5-addon.conf"
    RENAME telebit-fcitx5.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
)

# Input Method registration file
install(
    FILES "telebit-fcitx5.conf.in"
    RENAME "telebit-fcitx5.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)

# Freedesktop icon (Icon=telebit-fcitx5 in inputmethod .conf — required for tray / menus)
install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../icons/telebit-fcitx5.svg"
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME "telebit-fcitx5.svg"
)

