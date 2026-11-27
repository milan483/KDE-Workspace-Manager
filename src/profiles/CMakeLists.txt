add_library(kwm_profiles STATIC
    profile.cpp
    profile.h
    profile_error.h
    profile_migrator.cpp
    profile_migrator.h
    profile_repository.cpp
    profile_repository.h
    profile_serializer.cpp
    profile_serializer.h
    profile_validator.cpp
    profile_validator.h
)

add_library(KDEWorkspaceManager::Profiles ALIAS kwm_profiles)

target_include_directories(kwm_profiles
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
)

target_link_libraries(kwm_profiles PUBLIC Qt6::Core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kwm_profiles PRIVATE -Wall -Wextra -Wpedantic)
endif()
