#pragma once

#include "profiles/profile.h"
#include "profiles/profile_error.h"

#include <QVector>

#include <optional>

namespace kwm::profiles
{

struct ProfileListResult
{
    QVector<Profile> profiles;
    QVector<ProfileError> errors;
};

class ProfileRepository final
{
    public:
    ProfileRepository();
    explicit ProfileRepository(QString storageDirectory);

    [[nodiscard]] QString storageDirectory() const;

    [[nodiscard]] ProfileResult<ProfileListResult> list() const;
    [[nodiscard]] ProfileResult<Profile> load(const QUuid &profileId) const;
    [[nodiscard]] std::optional<ProfileError> save(const Profile &profile) const;
    [[nodiscard]] std::optional<ProfileError> remove(const QUuid &profileId) const;

    private:
    [[nodiscard]] QString filePathForId(const QUuid &profileId) const;
    [[nodiscard]] static ProfileResult<Profile> loadFile(const QString &path, const QUuid &expectedId);

    QString m_storageDirectory;
};

} // namespace kwm::profiles
