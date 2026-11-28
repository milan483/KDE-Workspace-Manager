#pragma once

#include "profiles/profile.h"

#include <QMetaType>

namespace kwm::workspaces
{

struct WorkspaceCaptureSnapshot
{
    profiles::Profile profile;
    int excludedWindowCount = 0;
    int geometryOmittedCount = 0;
    int stateGeometryUnavailableCount = 0;
};

} // namespace kwm::workspaces

Q_DECLARE_METATYPE(kwm::workspaces::WorkspaceCaptureSnapshot)
