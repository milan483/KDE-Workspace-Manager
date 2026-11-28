#include "workspaces/profile_list_model.h"

#include <algorithm>
#include <utility>

namespace kwm::workspaces
{
namespace
{

bool profileLess(const kwm::profiles::Profile &left, const kwm::profiles::Profile &right)
{
    const int nameOrder = QString::compare(left.name, right.name, Qt::CaseInsensitive);
    if (nameOrder != 0)
    {
        return nameOrder < 0;
    }
    return left.id.toString(QUuid::WithoutBraces) < right.id.toString(QUuid::WithoutBraces);
}

} // namespace

ProfileListModel::ProfileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProfileListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_profiles.size();
}

QVariant ProfileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_profiles.size())
    {
        return {};
    }

    const kwm::profiles::Profile &profile = m_profiles.at(index.row());
    switch (role)
    {
    case Qt::DisplayRole:
    case ProfileNameRole:
        return profile.name;
    case ProfileIdRole:
        return profile.id.toString(QUuid::WithoutBraces);
    case DesktopCountRole:
        return profile.virtualDesktops.size();
    case ApplicationCountRole:
        return profile.applications.size();
    case WindowCountRole:
        return windowCount(profile);
    default:
        return {};
    }
}

QHash<int, QByteArray> ProfileListModel::roleNames() const
{
    return {
        {ProfileIdRole, "profileId"},
        {ProfileNameRole, "profileName"},
        {DesktopCountRole, "desktopCount"},
        {ApplicationCountRole, "applicationCount"},
        {WindowCountRole, "windowCount"},
    };
}

int ProfileListModel::count() const
{
    return m_profiles.size();
}

void ProfileListModel::setProfiles(QVector<kwm::profiles::Profile> profiles)
{
    std::sort(profiles.begin(), profiles.end(), profileLess);
    const bool countChangedValue = m_profiles.size() != profiles.size();

    beginResetModel();
    m_profiles = std::move(profiles);
    endResetModel();

    if (countChangedValue)
    {
        Q_EMIT countChanged();
    }
}

void ProfileListModel::upsertProfile(const kwm::profiles::Profile &profile)
{
    for (qsizetype index = 0; index < m_profiles.size(); ++index)
    {
        if (m_profiles.at(index).id != profile.id)
        {
            continue;
        }

        const bool nameChanged = m_profiles.at(index).name != profile.name;
        if (!nameChanged)
        {
            m_profiles[index] = profile;
            const QModelIndex modelIndex = this->index(static_cast<int>(index), 0);
            Q_EMIT dataChanged(modelIndex, modelIndex);
            return;
        }

        QVector<kwm::profiles::Profile> updated = m_profiles;
        updated[static_cast<int>(index)] = profile;
        std::sort(updated.begin(), updated.end(), profileLess);
        beginResetModel();
        m_profiles = std::move(updated);
        endResetModel();
        return;
    }

    const qsizetype position = insertionIndex(profile);
    beginInsertRows({}, static_cast<int>(position), static_cast<int>(position));
    m_profiles.insert(position, profile);
    endInsertRows();
    Q_EMIT countChanged();
}

bool ProfileListModel::removeProfile(const QUuid &profileId)
{
    for (qsizetype index = 0; index < m_profiles.size(); ++index)
    {
        if (m_profiles.at(index).id != profileId)
        {
            continue;
        }

        beginRemoveRows({}, static_cast<int>(index), static_cast<int>(index));
        m_profiles.removeAt(index);
        endRemoveRows();
        Q_EMIT countChanged();
        return true;
    }

    return false;
}

std::optional<kwm::profiles::Profile> ProfileListModel::findProfile(const QUuid &profileId) const
{
    for (const kwm::profiles::Profile &profile : m_profiles)
    {
        if (profile.id == profileId)
        {
            return profile;
        }
    }
    return std::nullopt;
}

QVector<kwm::profiles::Profile> ProfileListModel::profiles() const
{
    return m_profiles;
}

int ProfileListModel::windowCount(const kwm::profiles::Profile &profile)
{
    int count = 0;
    for (const kwm::profiles::Application &application : profile.applications)
    {
        count += application.windows.size();
    }
    return count;
}

qsizetype ProfileListModel::insertionIndex(const kwm::profiles::Profile &profile) const
{
    for (qsizetype index = 0; index < m_profiles.size(); ++index)
    {
        if (profileLess(profile, m_profiles.at(index)))
        {
            return index;
        }
    }
    return m_profiles.size();
}

} // namespace kwm::workspaces
