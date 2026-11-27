#pragma once

#include "profiles/profile_error.h"

#include <QJsonObject>

namespace kwm::profiles
{

class ProfileMigrator final
{
    public:
    static constexpr int CurrentSchemaVersion = 1;

    [[nodiscard]] static ProfileResult<QJsonObject> migrateToCurrent(const QJsonObject &document);
};

} // namespace kwm::profiles
