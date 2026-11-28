add_library(kwm_workspaces STATIC
    profile_list_model.cpp
    profile_list_model.h
    workspace_capture_backend.h
    workspace_capture_snapshot.h
    workspace_service.cpp
    workspace_service.h
)

add_library(KDEWorkspaceManager::Workspaces ALIAS kwm_workspaces)

target_include_directories(kwm_workspaces
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_link_libraries(kwm_workspaces
    PUBLIC
        KDEWorkspaceManager::Profiles
        Qt6::Core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kwm_workspaces PRIVATE -Wall -Wextra -Wpedantic)
endif()
