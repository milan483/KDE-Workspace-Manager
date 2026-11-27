#pragma once

#include "profiles/profile.h"
#include "profiles/profile_error.h"

#include <QVector>

namespace kwm::profiles
{

class ProfileValidator final
{
    public:
    [[nodiscard]] static QVector<ProfileIssue> validate(const Profile &profile);
};

} // namespace kwm::profiles
