#pragma once

#include "profiles/profile_error.h"
#include "workspaces/workspace_capture_snapshot.h"

#include <QByteArray>

namespace kwm::kwin
{

class KWinSnapshotParser final
{
    public:
    static constexpr int ProtocolVersion = 1;

    [[nodiscard]] static profiles::ProfileResult<kwm::workspaces::WorkspaceCaptureSnapshot> parse(
        const QByteArray &responseJson, const QString &expectedRequestId, const QString &profileName);
};

} // namespace kwm::kwin
