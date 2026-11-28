add_library(kwm_kwin STATIC
    kwin_bridge_service.cpp
    kwin_bridge_service.h
    kwin_capture_client.cpp
    kwin_capture_client.h
    kwin_request_queue.cpp
    kwin_request_queue.h
    kwin_script_controller.cpp
    kwin_script_controller.h
    kwin_snapshot_parser.cpp
    kwin_snapshot_parser.h
)

add_library(KDEWorkspaceManager::KWin ALIAS kwm_kwin)

target_include_directories(kwm_kwin
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_link_libraries(kwm_kwin
    PUBLIC
        KDEWorkspaceManager::Profiles
        Qt6::Core
        Qt6::DBus
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kwm_kwin PRIVATE -Wall -Wextra -Wpedantic)
endif()
