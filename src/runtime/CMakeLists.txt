add_library(kwm_runtime STATIC
    workspace_service_factory.cpp
    workspace_service_factory.h
)

add_library(KDEWorkspaceManager::Runtime ALIAS kwm_runtime)

target_include_directories(kwm_runtime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(kwm_runtime
    PUBLIC
        KDEWorkspaceManager::Workspaces
    PRIVATE
        KDEWorkspaceManager::KWin
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kwm_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()
