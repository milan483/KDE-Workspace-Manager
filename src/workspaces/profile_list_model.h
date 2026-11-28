#pragma once

#include "profiles/profile.h"

#include <QAbstractListModel>
#include <QVector>

#include <optional>

namespace kwm::workspaces
{

class ProfileListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

    public:
    enum Role
    {
        ProfileIdRole = Qt::UserRole + 1,
        ProfileNameRole,
        DesktopCountRole,
        ApplicationCountRole,
        WindowCountRole
    };
    Q_ENUM(Role)

    explicit ProfileListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const;

    void setProfiles(QVector<kwm::profiles::Profile> profiles);
    void upsertProfile(const kwm::profiles::Profile &profile);
    [[nodiscard]] bool removeProfile(const QUuid &profileId);

    [[nodiscard]] std::optional<kwm::profiles::Profile> findProfile(const QUuid &profileId) const;
    [[nodiscard]] QVector<kwm::profiles::Profile> profiles() const;

    Q_SIGNALS:
    void countChanged();

    private:
    [[nodiscard]] static int windowCount(const kwm::profiles::Profile &profile);
    [[nodiscard]] qsizetype insertionIndex(const kwm::profiles::Profile &profile) const;

    QVector<kwm::profiles::Profile> m_profiles;
};

} // namespace kwm::workspaces
