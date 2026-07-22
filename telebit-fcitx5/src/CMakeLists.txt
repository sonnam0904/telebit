find_package(CURL REQUIRED)

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

target_link_libraries(telebit_fcitx5 PRIVATE Fcitx5::Core CURL::libcurl)

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

