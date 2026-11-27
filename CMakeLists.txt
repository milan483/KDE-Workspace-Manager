cmake_minimum_required(VERSION 3.24)

project(KDEWorkspaceManager
    VERSION 0.1.0
    DESCRIPTION "Workspace profile manager for KDE Plasma"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(CTest)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMFindQmlModule)
include(ECMQmlModule)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
)

find_package(KF6 6.8 REQUIRED COMPONENTS
    CoreAddons
    I18n
    IconThemes
    Kirigami
    QQC2DesktopStyle
)

ecm_find_qmlmodule(org.kde.desktop REQUIRED)
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
ecm_find_qmlmodule(org.kde.ki18n REQUIRED)

add_library(kwm_core STATIC
    src/core/build_info.cpp
    src/core/build_info.h
)
add_library(KDEWorkspaceManager::Core ALIAS kwm_core)

target_include_directories(kwm_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(kwm_core PUBLIC Qt6::Core)
target_compile_definitions(kwm_core
    PRIVATE
        "KWM_VERSION=\"${PROJECT_VERSION}\""
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kwm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/app)

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

    add_executable(kwm_core_tests
        tests/unit/test_build_info.cpp
    )
    target_link_libraries(kwm_core_tests
        PRIVATE
            KDEWorkspaceManager::Core
            Qt6::Test
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(kwm_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME kwm_core_tests COMMAND kwm_core_tests)
    set_tests_properties(kwm_core_tests PROPERTIES TIMEOUT 30)

    add_test(NAME kwm_application_smoke COMMAND kde-workspace-manager --smoke-test)
    set_tests_properties(kwm_application_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )

    find_program(QMLLINT_EXECUTABLE
        NAMES qmllint
        HINTS /usr/lib/qt6/bin
        REQUIRED
    )
    set(KWM_QML_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Main.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/WorkspacesPage.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/SettingsPage.qml"
    )
    add_test(
        NAME kwm_qml_lint
        COMMAND "${QMLLINT_EXECUTABLE}"
            -I "${CMAKE_BINARY_DIR}/bin"
            -I /usr/lib/qt6/qml
            ${KWM_QML_FILES}
    )

    find_program(DESKTOP_FILE_VALIDATE_EXECUTABLE
        NAMES desktop-file-validate
        REQUIRED
    )
    add_test(
        NAME kwm_desktop_file_validate
        COMMAND "${DESKTOP_FILE_VALIDATE_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/org.kde.kdeworkspacemanager.desktop"
    )
endif()

include(FeatureSummary)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
