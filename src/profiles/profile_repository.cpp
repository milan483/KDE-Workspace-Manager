#include "profiles/profile_repository.h"

#include "profiles/profile_serializer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace kwm::profiles
{
namespace
{

ProfileError repositoryError(ProfileErrorCode code, const QString &message, const QString &path = {})
{
    return {code, message, path, {}};
}

} // namespace

ProfileRepository::ProfileRepository()
{
    const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataLocation.isEmpty())
    {
        m_storageDirectory = QDir(appDataLocation).filePath(QStringLiteral("profiles"));
    }
}

ProfileRepository::ProfileRepository(QString storageDirectory)
{
    if (!storageDirectory.trimmed().isEmpty())
    {
        m_storageDirectory = QDir::cleanPath(std::move(storageDirectory));
    }
}

QString ProfileRepository::storageDirectory() const { return m_storageDirectory; }

QString ProfileRepository::filePathForId(const QUuid &profileId) const
{
    return QDir(m_storageDirectory).filePath(profileId.toString(QUuid::WithoutBraces) + QStringLiteral(".json"));
}

ProfileResult<ProfileListResult> ProfileRepository::list() const
{
    if (m_storageDirectory.trimmed().isEmpty())
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage directory is not configured."));
    }

    const QFileInfo directoryInfo(m_storageDirectory);
    if (!directoryInfo.exists())
    {
        return ProfileListResult{};
    }
    if (!directoryInfo.isDir() || !directoryInfo.isReadable())
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage path is not a readable directory."), m_storageDirectory);
    }

    const QDir directory(m_storageDirectory);
    const QFileInfoList files =
        directory.entryInfoList({QStringLiteral("*.json")}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    ProfileListResult result;
    for (const QFileInfo &fileInfo : files)
    {
        const QString baseName = fileInfo.completeBaseName();
        const QUuid profileId = QUuid::fromString(baseName);
        if (profileId.isNull() || profileId.toString(QUuid::WithoutBraces) != baseName)
        {
            result.errors.push_back(
                repositoryError(ProfileErrorCode::InvalidProfileFileName,
                                QStringLiteral("Profile filename must be a canonical UUID followed by .json."),
                                fileInfo.absoluteFilePath()));
            continue;
        }

        if (fileInfo.isSymLink())
        {
            result.errors.push_back(repositoryError(ProfileErrorCode::IoError,
                                                    QStringLiteral("Symbolic links are not accepted as profile files."),
                                                    fileInfo.absoluteFilePath()));
            continue;
        }

        auto profileResult = loadFile(fileInfo.absoluteFilePath(), profileId);
        if (const auto *failure = std::get_if<ProfileError>(&profileResult))
        {
            result.errors.push_back(*failure);
            continue;
        }

        result.profiles.push_back(std::get<Profile>(std::move(profileResult)));
    }

    return result;
}

ProfileResult<Profile> ProfileRepository::load(const QUuid &profileId) const
{
    if (profileId.isNull())
    {
        return repositoryError(ProfileErrorCode::InvalidArgument,
                               QStringLiteral("Profile identifier must be a non-null UUID."));
    }

    if (m_storageDirectory.trimmed().isEmpty())
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage directory is not configured."));
    }

    const QFileInfo storageInfo(m_storageDirectory);
    if (storageInfo.exists() && (!storageInfo.isDir() || !storageInfo.isReadable()))
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage path is not a readable directory."), m_storageDirectory);
    }

    const QString path = filePathForId(profileId);
    if (!QFileInfo::exists(path))
    {
        return repositoryError(ProfileErrorCode::NotFound, QStringLiteral("Profile does not exist."), path);
    }

    return loadFile(path, profileId);
}

std::optional<ProfileError> ProfileRepository::save(const Profile &profile) const
{
    auto serializedResult = ProfileSerializer::serialize(profile);
    if (const auto *failure = std::get_if<ProfileError>(&serializedResult))
    {
        return *failure;
    }

    if (m_storageDirectory.trimmed().isEmpty())
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage directory is not configured."));
    }

    if (!QDir().mkpath(m_storageDirectory))
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Could not create the profile storage directory."), m_storageDirectory);
    }

    const QString path = filePathForId(profile.id);
    const QFileInfo existingFile(path);
    if (existingFile.isSymLink())
    {
        return repositoryError(ProfileErrorCode::IoError,
                               QStringLiteral("Refusing to replace a symbolic link in profile storage."), path);
    }

    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
    {
        return repositoryError(
            ProfileErrorCode::IoError,
            QStringLiteral("Could not open the atomic profile file for writing: %1").arg(file.errorString()), path);
    }

    const QByteArray serialized = std::get<QByteArray>(std::move(serializedResult));
    const qint64 bytesWritten = file.write(serialized);
    if (bytesWritten != serialized.size())
    {
        const QString message = QStringLiteral("Could not write the complete profile: %1").arg(file.errorString());
        file.cancelWriting();
        return repositoryError(ProfileErrorCode::IoError, message, path);
    }

    if (!file.commit())
    {
        return repositoryError(ProfileErrorCode::IoError,
                               QStringLiteral("Could not atomically commit the profile: %1").arg(file.errorString()),
                               path);
    }

    return std::nullopt;
}

std::optional<ProfileError> ProfileRepository::remove(const QUuid &profileId) const
{
    if (profileId.isNull())
    {
        return repositoryError(ProfileErrorCode::InvalidArgument,
                               QStringLiteral("Profile identifier must be a non-null UUID."));
    }

    if (m_storageDirectory.trimmed().isEmpty())
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage directory is not configured."));
    }

    const QFileInfo storageInfo(m_storageDirectory);
    if (storageInfo.exists() && (!storageInfo.isDir() || !storageInfo.isWritable()))
    {
        return repositoryError(ProfileErrorCode::DirectoryUnavailable,
                               QStringLiteral("Profile storage path is not a writable directory."), m_storageDirectory);
    }

    const QString path = filePathForId(profileId);
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() && !fileInfo.isSymLink())
    {
        return repositoryError(ProfileErrorCode::NotFound, QStringLiteral("Profile does not exist."), path);
    }
    if (fileInfo.isSymLink() || !fileInfo.isFile())
    {
        return repositoryError(ProfileErrorCode::IoError, QStringLiteral("Profile path is not a regular file."), path);
    }
    if (!QFile::remove(path))
    {
        return repositoryError(ProfileErrorCode::IoError, QStringLiteral("Could not remove profile file."), path);
    }

    return std::nullopt;
}

ProfileResult<Profile> ProfileRepository::loadFile(const QString &path, const QUuid &expectedId)
{
    const QFileInfo fileInfo(path);
    if (fileInfo.isSymLink() || !fileInfo.isFile())
    {
        return repositoryError(ProfileErrorCode::IoError, QStringLiteral("Profile path is not a regular file."), path);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return repositoryError(ProfileErrorCode::IoError,
                               QStringLiteral("Could not read profile file: %1").arg(file.errorString()), path);
    }

    const QByteArray contents = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        return repositoryError(ProfileErrorCode::IoError,
                               QStringLiteral("Could not read the complete profile file: %1").arg(file.errorString()),
                               path);
    }

    auto profileResult = ProfileSerializer::deserialize(contents);
    if (const auto *failure = std::get_if<ProfileError>(&profileResult))
    {
        ProfileError error = *failure;
        error.path = path;
        return error;
    }

    Profile profile = std::get<Profile>(std::move(profileResult));
    if (profile.id != expectedId)
    {
        return repositoryError(ProfileErrorCode::ProfileIdMismatch,
                               QStringLiteral("Profile identifier in the document does not match its filename."), path);
    }

    return profile;
}

} // namespace kwm::profiles
