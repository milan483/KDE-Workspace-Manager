#pragma once

#include "profiles/profile_error.h"
#include "profiles/profile.h"

#include <QByteArray>

namespace kwm::kwin
{

struct KWinCaptureSnapshot
{
    profiles::Profile profile;
    int excludedWindowCount = 0;
    int geometryOmittedCount = 0;
    int stateGeometryUnavailableCount = 0;
};

class KWinSnapshotParser final
{
    public:
    static constexpr int ProtocolVersion = 1;

    [[nodiscard]] static profiles::ProfileResult<KWinCaptureSnapshot> parse(const QByteArray &responseJson,
                                                                              const QString &expectedRequestId,
                                                                              const QString &profileName);
};

} // namespace kwm::kwin
