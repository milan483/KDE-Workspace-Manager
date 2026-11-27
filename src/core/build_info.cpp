#include "core/build_info.h"

namespace kwm::core
{

QString applicationName()
{
    return QStringLiteral("KDE Workspace Manager");
}

QString applicationVersion()
{
    return QStringLiteral(KWM_VERSION);
}

} // namespace kwm::core
