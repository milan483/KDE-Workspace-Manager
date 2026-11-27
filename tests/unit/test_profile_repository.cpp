#include "profile_test_utils.h"

#include "profiles/profile_repository.h"
#include "profiles/profile_serializer.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <sys/stat.h>

using namespace kwm::profiles;

class ProfileRepositoryTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void listOnMissingDirectoryReturnsEmptyWithoutCreatingIt();
    void rejectsEmptyStorageDirectory();
    void savesLoadsListsUpdatesAndRemovesProfiles();
    void savesProfilesWithOwnerOnlyPermissions();
    void keepsExistingStorageAndUpdatedProfilesPrivate();
    void rejectsSymbolicLinkStorageDirectory();
    void rejectsInvalidProfilesWithoutCreatingStorage();
    void listsHealthyProfilesAlongsideCorruptFiles();
    void reportsFilesWithInvalidNames();
    void rejectsDocumentIdThatDoesNotMatchFilename();
    void reportsMissingProfiles();
    void failedAtomicWritePreservesExistingProfile();
    void rejectsStoragePathThatIsNotDirectory();
};

namespace
{

QString profilePath(const QString &directory, const QUuid &id)
{
    return directory + QLatin1Char('/') + id.toString(QUuid::WithoutBraces) + QStringLiteral(".json");
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

class UmaskGuard final
{
    public:
    explicit UmaskGuard(mode_t mask)
        : m_previous(umask(mask))
    {
    }

    ~UmaskGuard()
    {
        umask(m_previous);
    }

    UmaskGuard(const UmaskGuard &) = delete;
    UmaskGuard &operator=(const UmaskGuard &) = delete;

    private:
    mode_t m_previous;
};

QFileDevice::Permissions nonOwnerPermissions()
{
    return QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther
        | QFileDevice::WriteOther | QFileDevice::ExeOther;
}

} // namespace

void ProfileRepositoryTest::listOnMissingDirectoryReturnsEmptyWithoutCreatingIt()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storage = temporaryDirectory.filePath(QStringLiteral("profiles"));
    const ProfileRepository repository(storage);

    const auto result = repository.list();
    QVERIFY(std::holds_alternative<ProfileListResult>(result));
    QVERIFY(std::get<ProfileListResult>(result).profiles.isEmpty());
    QVERIFY(std::get<ProfileListResult>(result).errors.isEmpty());
    QVERIFY(!QFileInfo::exists(storage));
}

void ProfileRepositoryTest::rejectsEmptyStorageDirectory()
{
    ProfileRepository repository(QString{});
    QVERIFY(repository.storageDirectory().isEmpty());

    const auto listed = repository.list();
    QVERIFY(std::holds_alternative<ProfileError>(listed));
    QVERIFY(std::get<ProfileError>(listed).code == ProfileErrorCode::DirectoryUnavailable);

    const auto saved = repository.save(test::makeValidProfile());
    QVERIFY(saved.has_value());
    QVERIFY(saved->code == ProfileErrorCode::DirectoryUnavailable);

    const auto removed = repository.remove(QUuid::createUuid());
    QVERIFY(removed.has_value());
    QVERIFY(removed->code == ProfileErrorCode::DirectoryUnavailable);
}

void ProfileRepositoryTest::savesLoadsListsUpdatesAndRemovesProfiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    Profile profile = test::makeValidProfile();

    QVERIFY(!repository.save(profile).has_value());
    QVERIFY(QFileInfo::exists(profilePath(repository.storageDirectory(), profile.id)));

    const auto loaded = repository.load(profile.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    QVERIFY(std::get<Profile>(loaded) == profile);

    const auto listed = repository.list();
    QVERIFY(std::holds_alternative<ProfileListResult>(listed));
    QCOMPARE(std::get<ProfileListResult>(listed).profiles.size(), 1);
    QVERIFY(std::get<ProfileListResult>(listed).errors.isEmpty());

    profile.name = QStringLiteral("Updated development");
    QVERIFY(!repository.save(profile).has_value());

    const auto updated = repository.load(profile.id);
    QVERIFY(std::holds_alternative<Profile>(updated));
    QCOMPARE(std::get<Profile>(updated).name, QStringLiteral("Updated development"));

    QVERIFY(!repository.remove(profile.id).has_value());
    QVERIFY(!QFileInfo::exists(profilePath(repository.storageDirectory(), profile.id)));
}

void ProfileRepositoryTest::savesProfilesWithOwnerOnlyPermissions()
{
    const UmaskGuard permissiveUmask(0022);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storage = temporaryDirectory.filePath(QStringLiteral("profiles"));
    ProfileRepository repository(storage);
    const Profile profile = test::makeValidProfile();

    QVERIFY(!repository.save(profile).has_value());

    const QFileInfo directoryInfo(storage);
    const QFileInfo fileInfo(profilePath(storage, profile.id));
    QVERIFY(directoryInfo.isDir());
    QVERIFY(fileInfo.isFile());
    QVERIFY((directoryInfo.permissions() & nonOwnerPermissions()) == QFileDevice::Permissions{});
    QVERIFY((fileInfo.permissions() & nonOwnerPermissions()) == QFileDevice::Permissions{});
}

void ProfileRepositoryTest::keepsExistingStorageAndUpdatedProfilesPrivate()
{
    const UmaskGuard permissiveUmask(0022);
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storage = temporaryDirectory.filePath(QStringLiteral("profiles"));
    ProfileRepository repository(storage);
    Profile profile = test::makeValidProfile();
    QVERIFY(!repository.save(profile).has_value());

    const QString path = profilePath(storage, profile.id);
    const QFileDevice::Permissions publicDirectoryPermissions =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
        | QFileDevice::WriteGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::WriteOther
        | QFileDevice::ExeOther;
    const QFileDevice::Permissions publicFilePermissions =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::WriteGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther;
    QVERIFY(QFile::setPermissions(storage, publicDirectoryPermissions));
    QVERIFY(QFile::setPermissions(path, publicFilePermissions));

    profile.name = QStringLiteral("Updated private profile");
    QVERIFY(!repository.save(profile).has_value());

    const QFileInfo directoryInfo(storage);
    const QFileInfo fileInfo(path);
    QVERIFY((directoryInfo.permissions() & nonOwnerPermissions()) == QFileDevice::Permissions{});
    QVERIFY((fileInfo.permissions() & nonOwnerPermissions()) == QFileDevice::Permissions{});
}

void ProfileRepositoryTest::rejectsSymbolicLinkStorageDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString targetDirectory = temporaryDirectory.filePath(QStringLiteral("target"));
    const QString storageLink = temporaryDirectory.filePath(QStringLiteral("profiles-link"));
    QVERIFY(QDir().mkpath(targetDirectory));
    QVERIFY(QFile::link(targetDirectory, storageLink));
    QVERIFY(QFileInfo(storageLink).isSymLink());

    ProfileRepository repository(storageLink);
    const auto error = repository.save(test::makeValidProfile());
    QVERIFY(error.has_value());
    QCOMPARE(error->code, ProfileErrorCode::DirectoryUnavailable);
}
void ProfileRepositoryTest::rejectsInvalidProfilesWithoutCreatingStorage()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storage = temporaryDirectory.filePath(QStringLiteral("profiles"));
    ProfileRepository repository(storage);
    Profile profile = test::makeValidProfile();
    profile.name.clear();

    const auto error = repository.save(profile);
    QVERIFY(error.has_value());
    QCOMPARE(error->code, ProfileErrorCode::ValidationFailed);
    QVERIFY(!QFileInfo::exists(storage));
}

void ProfileRepositoryTest::listsHealthyProfilesAlongsideCorruptFiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile healthyProfile = test::makeValidProfile();
    QVERIFY(!repository.save(healthyProfile).has_value());

    const QUuid corruptId = QUuid::createUuid();
    QVERIFY(writeBytes(profilePath(repository.storageDirectory(), corruptId), QByteArrayLiteral("{ not json")));

    const auto result = repository.list();
    QVERIFY(std::holds_alternative<ProfileListResult>(result));

    const auto &listing = std::get<ProfileListResult>(result);
    QCOMPARE(listing.profiles.size(), 1);
    QCOMPARE(listing.profiles.constFirst().id, healthyProfile.id);
    QCOMPARE(listing.errors.size(), 1);
    QCOMPARE(listing.errors.constFirst().code, ProfileErrorCode::InvalidJson);
}

void ProfileRepositoryTest::reportsFilesWithInvalidNames()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    QVERIFY(QDir().mkpath(repository.storageDirectory()));
    QVERIFY(writeBytes(repository.storageDirectory() + QStringLiteral("/unexpected.json"), QByteArrayLiteral("{}")));

    const auto result = repository.list();
    QVERIFY(std::holds_alternative<ProfileListResult>(result));
    QCOMPARE(std::get<ProfileListResult>(result).errors.size(), 1);
    QVERIFY(std::get<ProfileListResult>(result).errors.constFirst().code == ProfileErrorCode::InvalidProfileFileName);
}

void ProfileRepositoryTest::rejectsDocumentIdThatDoesNotMatchFilename()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    QVERIFY(QDir().mkpath(repository.storageDirectory()));

    const QUuid filenameId = QUuid::createUuid();
    const Profile storedProfile = test::makeValidProfile();
    const auto serialized = ProfileSerializer::serialize(storedProfile);
    QVERIFY(std::holds_alternative<QByteArray>(serialized));
    QVERIFY(writeBytes(profilePath(repository.storageDirectory(), filenameId), std::get<QByteArray>(serialized)));

    const auto loaded = repository.load(filenameId);
    QVERIFY(std::holds_alternative<ProfileError>(loaded));
    QVERIFY(std::get<ProfileError>(loaded).code == ProfileErrorCode::ProfileIdMismatch);
}

void ProfileRepositoryTest::reportsMissingProfiles()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const QUuid missingId = QUuid::createUuid();

    const auto loaded = repository.load(missingId);
    QVERIFY(std::holds_alternative<ProfileError>(loaded));
    QVERIFY(std::get<ProfileError>(loaded).code == ProfileErrorCode::NotFound);

    const auto removed = repository.remove(missingId);
    QVERIFY(removed.has_value());
    QVERIFY(removed->code == ProfileErrorCode::NotFound);
}

void ProfileRepositoryTest::failedAtomicWritePreservesExistingProfile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    Profile profile = test::makeValidProfile();
    QVERIFY(!repository.save(profile).has_value());

    const QString path = profilePath(repository.storageDirectory(), profile.id);
    QFile originalFile(path);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray originalContents = originalFile.readAll();
    originalFile.close();

    profile.name = QStringLiteral("Replacement that must not be written");
    const QFileDevice::Permissions readOnlyPermissions = QFileDevice::ReadOwner | QFileDevice::ExeOwner;
    if (!QFile::setPermissions(repository.storageDirectory(), readOnlyPermissions))
    {
        QSKIP("The test filesystem does not allow changing directory permissions.");
    }

    const auto error = repository.save(profile);
    const bool writeWasBlocked = error.has_value();
    QVERIFY(QFile::setPermissions(repository.storageDirectory(),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    if (!writeWasBlocked)
    {
        QSKIP("The current execution identity can write despite owner write permission being removed.");
    }

    QFile preservedFile(path);
    QVERIFY(preservedFile.open(QIODevice::ReadOnly));
    QCOMPARE(preservedFile.readAll(), originalContents);
}

void ProfileRepositoryTest::rejectsStoragePathThatIsNotDirectory()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storageFile = temporaryDirectory.filePath(QStringLiteral("not-a-directory"));
    QVERIFY(writeBytes(storageFile, QByteArrayLiteral("occupied")));

    ProfileRepository repository(storageFile);
    const auto result = repository.list();
    QVERIFY(std::holds_alternative<ProfileError>(result));
    QVERIFY(std::get<ProfileError>(result).code == ProfileErrorCode::DirectoryUnavailable);

    const QUuid profileId = QUuid::createUuid();
    const auto loaded = repository.load(profileId);
    QVERIFY(std::holds_alternative<ProfileError>(loaded));
    QVERIFY(std::get<ProfileError>(loaded).code == ProfileErrorCode::DirectoryUnavailable);

    const auto removed = repository.remove(profileId);
    QVERIFY(removed.has_value());
    QVERIFY(removed->code == ProfileErrorCode::DirectoryUnavailable);
}

QTEST_APPLESS_MAIN(ProfileRepositoryTest)

#include "test_profile_repository.moc"
