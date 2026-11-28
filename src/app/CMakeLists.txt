add_executable(kde-workspace-manager
    main.cpp
)

ecm_add_qml_module(kde-workspace-manager
    URI org.kde.kdeworkspacemanager
    VERSION 1.0
)

ecm_target_qml_sources(kde-workspace-manager
    SOURCES
        ../ui/Main.qml
        ../ui/WorkspacesPage.qml
        ../ui/SettingsPage.qml
)

target_link_libraries(kde-workspace-manager
    PRIVATE
        KDEWorkspaceManager::Core
        KDEWorkspaceManager::Runtime
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        KF6::CoreAddons
        KF6::I18n
        KF6::I18nQml
        KF6::IconThemes
        KF6::Kirigami
)
target_compile_definitions(kde-workspace-manager
    PRIVATE
        "KWM_VERSION=\"${PROJECT_VERSION}\""
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kde-workspace-manager PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS kde-workspace-manager ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(
    PROGRAMS "${CMAKE_SOURCE_DIR}/org.kde.kdeworkspacemanager.desktop"
    DESTINATION "${KDE_INSTALL_APPDIR}"
)
