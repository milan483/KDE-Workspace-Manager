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

option(BUILD_KWIN_INTEGRATION_TESTS "Build live KWin session integration tests" OFF)

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
    DBus
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

add_subdirectory(src/profiles)

add_subdirectory(src/kwin)

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

    function(kwm_add_profile_unit_test target source)
        add_executable(${target} ${source})
        target_link_libraries(${target}
            PRIVATE
                KDEWorkspaceManager::Profiles
                Qt6::Test
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES TIMEOUT 30)
    endfunction()

    kwm_add_profile_unit_test(kwm_profile_tests tests/unit/test_profile.cpp)
    kwm_add_profile_unit_test(kwm_profile_serializer_tests tests/unit/test_profile_serializer.cpp)
    kwm_add_profile_unit_test(kwm_profile_validator_tests tests/unit/test_profile_validator.cpp)
    kwm_add_profile_unit_test(kwm_profile_migrator_tests tests/unit/test_profile_migrator.cpp)
    kwm_add_profile_unit_test(kwm_profile_repository_tests tests/unit/test_profile_repository.cpp)

    function(kwm_add_kwin_unit_test target source)
        add_executable(${target} ${source})
        target_link_libraries(${target}
            PRIVATE
                KDEWorkspaceManager::KWin
                Qt6::Test
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES TIMEOUT 30)
    endfunction()

    kwm_add_kwin_unit_test(kwm_kwin_request_queue_tests tests/unit/test_kwin_request_queue.cpp)
    kwm_add_kwin_unit_test(kwm_kwin_snapshot_parser_tests tests/unit/test_kwin_snapshot_parser.cpp)

    if(BUILD_KWIN_INTEGRATION_TESTS)
        add_executable(kwm_kwin_live_capture_test tests/integration/test_kwin_live_capture.cpp)
        target_link_libraries(kwm_kwin_live_capture_test
            PRIVATE
                KDEWorkspaceManager::KWin
                Qt6::Core
        )
        add_test(NAME kwm_kwin_live_capture
            COMMAND kwm_kwin_live_capture_test "${PROJECT_SOURCE_DIR}/kwin-script/contents/code/main.js")
        set_tests_properties(kwm_kwin_live_capture PROPERTIES
            TIMEOUT 30
            LABELS "integration;kwin"
            RUN_SERIAL TRUE
        )
    endif()

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

install(DIRECTORY "${PROJECT_SOURCE_DIR}/kwin-script/"
    DESTINATION "${KDE_INSTALL_DATADIR}/kwin/scripts/kde-workspace-manager"
)
