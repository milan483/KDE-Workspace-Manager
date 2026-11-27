#pragma once

#include <QVector>
#include <QString>

#include <variant>

namespace kwm::profiles
{

enum class ProfileErrorCode
{
    InvalidArgument,
    InvalidJson,
    InvalidStructure,
    MissingSchemaVersion,
    InvalidSchemaVersion,
    UnsupportedSchemaVersion,
    ValidationFailed,
    NotFound,
    DirectoryUnavailable,
    IoError,
    InvalidProfileFileName,
    ProfileIdMismatch
};

struct ProfileIssue
{
    QString path;
    QString message;

    bool operator==(const ProfileIssue &) const = default;
};

struct ProfileError
{
    ProfileErrorCode code = ProfileErrorCode::InvalidStructure;
    QString message;
    QString path;
    QVector<ProfileIssue> issues;
};

template <typename T> using ProfileResult = std::variant<T, ProfileError>;

} // namespace kwm::profiles
