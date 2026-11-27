#pragma once

#include "profiles/profile.h"
#include "profiles/profile_error.h"
#include "profiles/profile_migrator.h"

#include <QByteArray>

namespace kwm::profiles
{

class ProfileSerializer final
{
    public:
    static constexpr int CurrentSchemaVersion = ProfileMigrator::CurrentSchemaVersion;

    [[nodiscard]] static ProfileResult<QByteArray> serialize(const Profile &profile);
    [[nodiscard]] static ProfileResult<Profile> deserialize(const QByteArray &json);
};

} // namespace kwm::profiles
