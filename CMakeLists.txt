# =============================================================================
#  Enstein Stock Manager
# =============================================================================
#  The project is split into two halves that are built separately and meet only
#  at the executable:
#
#      src/   EnsteinCore   C++ backend  - database, domain rules, PDFs
#      qml/   EnsteinUi     QML frontend - screens, dialogs, controls
#
#  Neither links against the other. They meet at runtime through one object,
#  `excelHandler`, described in docs/QML_API_REFERENCE.md. See
#  docs/ARCHITECTURE.md for why it is arranged this way.
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# CI overrides this from the git tag (-DAPP_VERSION=x.y.z) so the exe metadata,
# the installer and the reported application version can never drift apart.
set(APP_VERSION "2.1.4" CACHE STRING "Application version (x.y.z)")

project(EnsteinStockManager VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The Windows resource compiler is needed for the embedded icon + version info
# below. project(... LANGUAGES CXX) alone does not enable it.
if (WIN32)
    enable_language(RC)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    Network
    Widgets
    Sql
    QuickControls2
)

qt_standard_project_setup()

# ---- Third-party dependencies ----------------------------------------------
# Vendored rather than fetched so an offline or air-gapped machine can still
# build a release.
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx/CMakeLists.txt")
    message(FATAL_ERROR
        "QXlsx not found at third_party/QXlsx. It is vendored into this "
        "repository; if the folder is empty the checkout is incomplete.")
endif()
add_subdirectory(third_party/QXlsx/QXlsx)

# ---- The two halves of the application -------------------------------------
add_subdirectory(src)   # EnsteinCore  (backend)
add_subdirectory(qml)   # EnsteinUi    (frontend)

# ---- The executable that ties them together --------------------------------
# main.cpp is the only file that knows about both halves: it registers the
# backend type with the QML engine and loads the root QML file.
qt_add_executable(EnsteinStockManager
    src/app/main.cpp
)

target_link_libraries(EnsteinStockManager
    PRIVATE
        EnsteinCore
        # The QML module's plugin, not the backing EnsteinUi library:
        # linking the plugin is what registers the module and pulls its
        # compiled QML into the executable.
        EnsteinUiplugin
        # main.cpp constructs these itself: QApplication (rather than
        # QGuiApplication, because the backend opens native file dialogs) and
        # QQmlApplicationEngine.
        Qt6::Widgets
        Qt6::Qml
)

# Compile the window icon into the binary so it is present no matter how the
# app is packaged or where the exe is copied to.
qt_add_resources(EnsteinStockManager "app_assets"
    PREFIX "/"
    BASE "assets"
    FILES
        assets/app-icon.png
)

# ---- Windows: embedded exe icon and file/version metadata ----
if (WIN32)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/app-icon.ico" APP_ICON_ICO)
    set(VER_MAJOR ${PROJECT_VERSION_MAJOR})
    set(VER_MINOR ${PROJECT_VERSION_MINOR})
    set(VER_PATCH ${PROJECT_VERSION_PATCH})
    set(VER_STR   "${PROJECT_VERSION}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
        @ONLY
    )
    target_sources(EnsteinStockManager PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

set_target_properties(EnsteinStockManager PROPERTIES
    # Windows GUI subsystem: without this the app opens a console window
    # alongside the UI on every launch.
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
        ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

target_compile_definitions(EnsteinStockManager
    PRIVATE APP_VERSION_STR="${PROJECT_VERSION}"
)

include(GNUInstallDirs)

install(TARGETS EnsteinStockManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
