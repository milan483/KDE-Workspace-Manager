#pragma once

#include "profiles/profile_repository.h"
#include "workspaces/workspace_service.h"

#include <memory>

namespace kwm::runtime
{

[[nodiscard]] std::unique_ptr<workspaces::WorkspaceService> createWorkspaceService(
    profiles::ProfileRepository repository);

} // namespace kwm::runtime
