#include "runtime/workspace_service_factory.h"

#include "kwin/kwin_capture_client.h"

#include <utility>

namespace kwm::runtime
{

std::unique_ptr<workspaces::WorkspaceService> createWorkspaceService(profiles::ProfileRepository repository)
{
    auto *captureBackend = new kwm::kwin::KWinCaptureClient;
    return std::make_unique<workspaces::WorkspaceService>(std::move(repository), captureBackend);
}

} // namespace kwm::runtime
