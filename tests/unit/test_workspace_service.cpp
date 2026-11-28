#include "profile_test_utils.h"

#include "profiles/profile_repository.h"
#include "profiles/profile_validator.h"
#include "workspaces/workspace_capture_backend.h"
#include "workspaces/profile_list_model.h"
#include "workspaces/workspace_service.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace kwm::profiles;
using namespace kwm::workspaces;
using kwm::workspaces::WorkspaceCaptureSnapshot;

namespace
{

class FakeCaptureBackend final : public WorkspaceCaptureBackend
{
    Q_OBJECT

    public:
    explicit FakeCaptureBackend(QObject *parent = nullptr)
        : WorkspaceCaptureBackend(parent)
    {
    }

    [[nodiscard]] bool isRunning() const override
    {
        return m_running;
    }

    void start() override
    {
        ++startCount;
        if (failOnStart)
        {
            Q_EMIT failed(startFailureMessage);
            return;
        }
        m_running = true;
        Q_EMIT started();
    }

    void stop() override
    {
        m_running = false;
        Q_EMIT stopped();
    }

    QString capture(const QString &profileName) override
    {
        ++captureCount;
        lastProfileName = profileName;
        lastRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        return lastRequestId;
    }

    void complete(const WorkspaceCaptureSnapshot &snapshot)
    {
        Q_EMIT captureCompleted(lastRequestId, snapshot);
    }

    void fail(const QString &path, const QString &message)
    {
        Q_EMIT captureFailed(lastRequestId, path, message);
    }

    int startCount = 0;
    int captureCount = 0;
    QString lastProfileName;
    QString lastRequestId;
    bool failOnStart = false;
    QString startFailureMessage = QStringLiteral("KWin is unavailable.");

    private:
    bool m_running = false;
};

WorkspaceCaptureSnapshot makeSnapshot(const QString &name)
{
    Profile profile = test::makeValidProfile();
    profile.name = name;
    for (Application &application : profile.applications)
    {
        application.launch.reset();
    }
    return {profile, 2, 1, 1};
}

QSet<QUuid> allIds(const Profile &profile)
{
    QSet<QUuid> ids;
    ids.insert(profile.id);
    for (const VirtualDesktop &desktop : profile.virtualDesktops)
    {
        ids.insert(desktop.id);
    }
    for (const Application &application : profile.applications)
    {
        ids.insert(application.id);
        for (const Window &window : application.windows)
        {
            ids.insert(window.id);
        }
    }
    return ids;
}

void addThirdDesktop(Profile &profile, const QString &name)
{
    const QUuid id = QUuid::createUuid();
    profile.virtualDesktops.push_back({id, name});
    if (!profile.applications.isEmpty() && !profile.applications.first().windows.isEmpty())
    {
        profile.applications.first().windows.first().desktopMembership.desktopIds.push_back(id);
    }
}

} // namespace

class WorkspaceServiceTest final : public QObject
{
    Q_OBJECT

    private Q_SLOTS:
    void profileListModelSupportsInsertUpdateAndRemove();
    void loadsExistingProfilesAtStartup();
    void saveRequiresPreviewConfirmationBeforePersistence();
    void rejectsConflictingOperationsWhileCaptureIsPending();
    void savedProfilesLoadAfterServiceRestart();
    void captureTimeoutDoesNotPersistAProfile();
    void captureStartFailureDoesNotPersistAProfile();
    void updatePreservesProfileAndMatchingDesktopIds();
    void updatePreservesLaunchSpecificationsForMatchingApplications();
    void updateRejectsAmbiguousLaunchSpecificationMatches();
    void updateRemovesObsoleteDesktopIds();
    void duplicateRegeneratesAllIdsAndRemapsReferences();
    void renamePreservesProfileIdentity();
    void deleteRemovesProfile();
    void profilePreviewIncludesWindowAndDesktopDetails();
    void capturePreviewIncludesLimitationsAndMissingLaunchMetadata();
    void persistenceFailureKeepsPreviewAndDoesNotReplaceExistingData();
};

void WorkspaceServiceTest::profileListModelSupportsInsertUpdateAndRemove()
{
    ProfileListModel model;
    QSignalSpy countSpy(&model, &ProfileListModel::countChanged);
    Profile profile = test::makeValidProfile();

    model.upsertProfile(profile);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(model.data(model.index(0), ProfileListModel::ProfileIdRole).toString(),
             profile.id.toString(QUuid::WithoutBraces));
    QCOMPARE(model.data(model.index(0), ProfileListModel::ProfileNameRole).toString(), profile.name);
    QCOMPARE(model.data(model.index(0), ProfileListModel::DesktopCountRole).toInt(),
             static_cast<int>(profile.virtualDesktops.size()));
    QCOMPARE(model.data(model.index(0), ProfileListModel::ApplicationCountRole).toInt(),
             static_cast<int>(profile.applications.size()));
    QCOMPARE(model.data(model.index(0), ProfileListModel::WindowCountRole).toInt(), 2);

    profile.name = QStringLiteral("Renamed development");
    profile.applications.first().windows.removeLast();
    model.upsertProfile(profile);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(model.data(model.index(0), ProfileListModel::ProfileNameRole).toString(), profile.name);
    QCOMPARE(model.data(model.index(0), ProfileListModel::WindowCountRole).toInt(), 1);

    QVERIFY(model.removeProfile(profile.id));
    QCOMPARE(model.count(), 0);
    QCOMPARE(countSpy.count(), 2);
    QVERIFY(!model.removeProfile(profile.id));
}

void WorkspaceServiceTest::loadsExistingProfilesAtStartup()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storage = temporaryDirectory.filePath(QStringLiteral("profiles"));
    ProfileRepository repository(storage);
    const Profile profile = test::makeValidProfile();
    QVERIFY(!repository.save(profile).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);

    QCOMPARE(service.profileModel()->count(), 1);
    QCOMPARE(service.profileModel()->data(service.profileModel()->index(0),
                                          ProfileListModel::ProfileNameRole).toString(),
             profile.name);
    QVERIFY(service.warningMessage().isEmpty());
}

void WorkspaceServiceTest::saveRequiresPreviewConfirmationBeforePersistence()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storage = temporaryDirectory.filePath(QStringLiteral("profiles"));
    ProfileRepository repository(storage);
    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);

    service.saveWorkspace(QStringLiteral("New workspace"));
    QCOMPARE(capture->startCount, 1);
    QCOMPARE(capture->captureCount, 1);
    QCOMPARE(capture->lastProfileName, QStringLiteral("New workspace"));
    QVERIFY(service.isBusy());
    QCOMPARE(service.profileModel()->count(), 0);

    const WorkspaceCaptureSnapshot snapshot = makeSnapshot(QStringLiteral("New workspace"));
    capture->complete(snapshot);

    QVERIFY(!service.isBusy());
    QVERIFY(service.hasPreview());
    QVERIFY(!service.previewIsUpdate());
    QCOMPARE(service.profileModel()->count(), 0);
    QVERIFY(!QFileInfo::exists(storage));

    QVERIFY(service.confirmSavePreview());
    QVERIFY(!service.hasPreview());
    QCOMPARE(service.profileModel()->count(), 1);
    QCOMPARE(service.statusMessage(), QStringLiteral("Workspace saved."));

    const auto loaded = repository.load(snapshot.profile.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    QCOMPARE(std::get<Profile>(loaded).name, QStringLiteral("New workspace"));
}

void WorkspaceServiceTest::rejectsConflictingOperationsWhileCaptureIsPending()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);

    service.saveWorkspace(QStringLiteral("First workspace"));
    service.saveWorkspace(QStringLiteral("Second workspace"));
    QCOMPARE(capture->captureCount, 1);
    QVERIFY(service.isBusy());
    QVERIFY(service.errorMessage().contains(QStringLiteral("capture operation is in progress")));

    capture->complete(makeSnapshot(QStringLiteral("First workspace")));
    QVERIFY(service.hasPreview());
    QVERIFY(service.errorMessage().isEmpty());
    service.saveWorkspace(QStringLiteral("Third workspace"));
    QCOMPARE(capture->captureCount, 1);
    QVERIFY(service.errorMessage().contains(QStringLiteral("capture preview")));

    service.cancelPreview();
    QVERIFY(!service.hasPreview());
}

void WorkspaceServiceTest::savedProfilesLoadAfterServiceRestart()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    QUuid savedProfileId;

    {
        auto *capture = new FakeCaptureBackend;
        WorkspaceService firstInstance(repository, capture);
        firstInstance.saveWorkspace(QStringLiteral("Persistent workspace"));
        WorkspaceCaptureSnapshot snapshot = makeSnapshot(QStringLiteral("Persistent workspace"));
        savedProfileId = snapshot.profile.id;
        capture->complete(snapshot);
        QVERIFY(firstInstance.confirmSavePreview());
    }

    auto *captureAfterRestart = new FakeCaptureBackend;
    WorkspaceService restartedInstance(repository, captureAfterRestart);
    QCOMPARE(restartedInstance.profileModel()->count(), 1);
    const QVariantMap details = restartedInstance.profileDetails(savedProfileId.toString(QUuid::WithoutBraces));
    QCOMPARE(details.value(QStringLiteral("name")).toString(), QStringLiteral("Persistent workspace"));
}

void WorkspaceServiceTest::captureTimeoutDoesNotPersistAProfile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);

    service.saveWorkspace(QStringLiteral("Timed out workspace"));
    capture->fail(QStringLiteral("timeout"),
                  QStringLiteral("Timed out waiting for KWin to return the capture snapshot."));

    QVERIFY(!service.isBusy());
    QVERIFY(!service.hasPreview());
    QVERIFY(service.errorMessage().contains(QStringLiteral("Timed out")));
    QCOMPARE(service.profileModel()->count(), 0);

    const auto listed = repository.list();
    QVERIFY(std::holds_alternative<ProfileListResult>(listed));
    QVERIFY(std::get<ProfileListResult>(listed).profiles.isEmpty());
}

void WorkspaceServiceTest::captureStartFailureDoesNotPersistAProfile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    auto *capture = new FakeCaptureBackend;
    capture->failOnStart = true;
    WorkspaceService service(repository, capture);

    service.saveWorkspace(QStringLiteral("Unavailable workspace"));

    QVERIFY(!service.isBusy());
    QVERIFY(!service.hasPreview());
    QCOMPARE(service.errorMessage(), QStringLiteral("KWin is unavailable."));
    QCOMPARE(capture->captureCount, 0);
    QCOMPARE(service.profileModel()->count(), 0);
}

void WorkspaceServiceTest::updatePreservesProfileAndMatchingDesktopIds()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile existing = test::makeValidProfile();
    QVERIFY(!repository.save(existing).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    service.updateWorkspace(existing.id.toString(QUuid::WithoutBraces));

    QCOMPARE(capture->lastProfileName, existing.name);
    QCOMPARE(capture->captureCount, 1);

    WorkspaceCaptureSnapshot snapshot = makeSnapshot(QStringLiteral("Captured name must not replace the profile name"));
    snapshot.profile.activeDesktopId = snapshot.profile.virtualDesktops.at(1).id;
    addThirdDesktop(snapshot.profile, QStringLiteral("Build"));
    capture->complete(snapshot);

    QVERIFY(service.hasPreview());
    QVERIFY(service.previewIsUpdate());
    QVERIFY(service.confirmSavePreview());

    const auto loaded = repository.load(existing.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    const Profile updated = std::get<Profile>(loaded);

    QCOMPARE(updated.id, existing.id);
    QCOMPARE(updated.name, existing.name);
    QCOMPARE(updated.virtualDesktops.size(), 3);
    QCOMPARE(updated.virtualDesktops.at(0).id, existing.virtualDesktops.at(0).id);
    QCOMPARE(updated.virtualDesktops.at(1).id, existing.virtualDesktops.at(1).id);
    QVERIFY(updated.activeDesktopId.has_value());
    QCOMPARE(*updated.activeDesktopId, updated.virtualDesktops.at(1).id);
    QVERIFY(updated.applications.first().id != existing.applications.first().id);
    QVERIFY(updated.applications.first().windows.first().id != existing.applications.first().windows.first().id);
    QVERIFY(updated.virtualDesktops.at(2).id != existing.virtualDesktops.at(0).id);
    QVERIFY(updated.virtualDesktops.at(2).id != existing.virtualDesktops.at(1).id);

    const Window &updatedWindow = updated.applications.first().windows.first();
    QCOMPARE(updatedWindow.desktopMembership.desktopIds.size(), 3);
    QCOMPARE(updatedWindow.desktopMembership.desktopIds.at(0), updated.virtualDesktops.at(0).id);
    QCOMPARE(updatedWindow.desktopMembership.desktopIds.at(1), updated.virtualDesktops.at(1).id);
    QCOMPARE(updatedWindow.desktopMembership.desktopIds.at(2), updated.virtualDesktops.at(2).id);
    QVERIFY(ProfileValidator::validate(updated).isEmpty());
}

void WorkspaceServiceTest::updatePreservesLaunchSpecificationsForMatchingApplications()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile existing = test::makeValidProfile();
    QVERIFY(!repository.save(existing).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    service.updateWorkspace(existing.id.toString(QUuid::WithoutBraces));

    WorkspaceCaptureSnapshot snapshot = makeSnapshot(existing.name);
    Application secondaryIdentityApplication = snapshot.profile.applications.first();
    secondaryIdentityApplication.id = QUuid::createUuid();
    secondaryIdentityApplication.windows = {secondaryIdentityApplication.windows.last()};
    snapshot.profile.applications.first().windows = {snapshot.profile.applications.first().windows.first()};
    snapshot.profile.applications.push_back(secondaryIdentityApplication);

    Application newlyObservedApplication;
    newlyObservedApplication.id = QUuid::createUuid();
    Window newlyObservedWindow;
    newlyObservedWindow.id = QUuid::createUuid();
    newlyObservedWindow.match.applicationIdentity = {
        ApplicationIdentityKind::DesktopFileName, QStringLiteral("org.example.new-application")};
    newlyObservedWindow.desktopMembership = {
        DesktopMembershipMode::Specific, {snapshot.profile.virtualDesktops.first().id}};
    newlyObservedApplication.windows.push_back(newlyObservedWindow);
    snapshot.profile.applications.push_back(newlyObservedApplication);

    capture->complete(snapshot);
    QVERIFY(service.hasPreview());
    QVERIFY(service.confirmSavePreview());

    const auto loaded = repository.load(existing.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    const Profile updated = std::get<Profile>(loaded);
    QCOMPARE(updated.applications.size(), 2);
    QCOMPARE(updated.applications.first().windows.size(), 2);

    const auto &preservedLaunch = updated.applications.first().launch;
    QVERIFY(preservedLaunch.has_value());
    QCOMPARE(preservedLaunch->desktopEntryId, existing.applications.first().launch->desktopEntryId);
    QCOMPARE(preservedLaunch->arguments, existing.applications.first().launch->arguments);
    QCOMPARE(preservedLaunch->workingDirectory, existing.applications.first().launch->workingDirectory);
    QVERIFY(!updated.applications.last().launch.has_value());
}

void WorkspaceServiceTest::updateRejectsAmbiguousLaunchSpecificationMatches()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    Profile existing = test::makeValidProfile();
    Application duplicateApplication = existing.applications.first();
    duplicateApplication.id = QUuid::createUuid();
    duplicateApplication.windows = {duplicateApplication.windows.first()};
    duplicateApplication.windows.first().id = QUuid::createUuid();
    existing.applications.push_back(duplicateApplication);
    QVERIFY(!repository.save(existing).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    service.updateWorkspace(existing.id.toString(QUuid::WithoutBraces));
    capture->complete(makeSnapshot(existing.name));

    QVERIFY(service.hasPreview());
    QVERIFY(!service.confirmSavePreview());
    QVERIFY(service.hasPreview());
    QVERIFY(service.errorMessage().contains(QStringLiteral("multiple existing applications")));

    const auto loaded = repository.load(existing.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    QVERIFY(std::get<Profile>(loaded) == existing);
}

void WorkspaceServiceTest::updateRemovesObsoleteDesktopIds()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    Profile existing = test::makeValidProfile();
    addThirdDesktop(existing, QStringLiteral("Obsolete"));
    const QUuid obsoleteId = existing.virtualDesktops.last().id;
    QVERIFY(!repository.save(existing).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    service.updateWorkspace(existing.id.toString(QUuid::WithoutBraces));

    const WorkspaceCaptureSnapshot snapshot = makeSnapshot(existing.name);
    capture->complete(snapshot);
    QVERIFY(service.confirmSavePreview());

    const auto loaded = repository.load(existing.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    const Profile updated = std::get<Profile>(loaded);

    QCOMPARE(updated.virtualDesktops.size(), 2);
    QVERIFY(updated.virtualDesktops.at(0).id == existing.virtualDesktops.at(0).id);
    QVERIFY(updated.virtualDesktops.at(1).id == existing.virtualDesktops.at(1).id);
    QVERIFY(!allIds(updated).contains(obsoleteId));
    QVERIFY(ProfileValidator::validate(updated).isEmpty());
}

void WorkspaceServiceTest::duplicateRegeneratesAllIdsAndRemapsReferences()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile source = test::makeValidProfile();
    QVERIFY(!repository.save(source).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    QVERIFY(service.duplicateProfile(source.id.toString(QUuid::WithoutBraces)));

    QCOMPARE(service.profileModel()->count(), 2);
    const auto listed = repository.list();
    QVERIFY(std::holds_alternative<ProfileListResult>(listed));
    QCOMPARE(std::get<ProfileListResult>(listed).profiles.size(), 2);

    Profile duplicate;
    bool foundDuplicate = false;
    for (const Profile &profile : std::get<ProfileListResult>(listed).profiles)
    {
        if (profile.id != source.id)
        {
            duplicate = profile;
            foundDuplicate = true;
            break;
        }
    }
    QVERIFY(foundDuplicate);
    QCOMPARE(duplicate.name, source.name + QStringLiteral(" (Copy)"));
    QVERIFY(ProfileValidator::validate(duplicate).isEmpty());

    const QSet<QUuid> sourceIds = allIds(source);
    const QSet<QUuid> duplicateIds = allIds(duplicate);
    for (const QUuid &id : duplicateIds)
    {
        QVERIFY(!sourceIds.contains(id));
    }

    QVERIFY(duplicate.activeDesktopId.has_value());
    QCOMPARE(*duplicate.activeDesktopId, duplicate.virtualDesktops.first().id);
    QVERIFY(duplicate.applications.first().launch.has_value());
    QVERIFY(source.applications.first().launch.has_value());
    QCOMPARE(duplicate.applications.first().launch->desktopEntryId,
             source.applications.first().launch->desktopEntryId);
    QCOMPARE(duplicate.applications.first().launch->arguments,
             source.applications.first().launch->arguments);
    for (const Window &window : duplicate.applications.first().windows)
    {
        if (window.desktopMembership.mode == DesktopMembershipMode::Specific)
        {
            for (const QUuid &desktopId : window.desktopMembership.desktopIds)
            {
                QVERIFY(std::any_of(duplicate.virtualDesktops.cbegin(), duplicate.virtualDesktops.cend(),
                                    [&desktopId](const VirtualDesktop &desktop) {
                                        return desktop.id == desktopId;
                                    }));
            }
        }
    }
}

void WorkspaceServiceTest::renamePreservesProfileIdentity()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile original = test::makeValidProfile();
    QVERIFY(!repository.save(original).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    QVERIFY(service.renameProfile(original.id.toString(QUuid::WithoutBraces), QStringLiteral("Research")));

    const auto loaded = repository.load(original.id);
    QVERIFY(std::holds_alternative<Profile>(loaded));
    QCOMPARE(std::get<Profile>(loaded).id, original.id);
    QCOMPARE(std::get<Profile>(loaded).name, QStringLiteral("Research"));
}

void WorkspaceServiceTest::deleteRemovesProfile()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile profile = test::makeValidProfile();
    QVERIFY(!repository.save(profile).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    QVERIFY(service.deleteProfile(profile.id.toString(QUuid::WithoutBraces)));

    QCOMPARE(service.profileModel()->count(), 0);
    const auto loaded = repository.load(profile.id);
    QVERIFY(std::holds_alternative<ProfileError>(loaded));
    QCOMPARE(std::get<ProfileError>(loaded).code, ProfileErrorCode::NotFound);
}

void WorkspaceServiceTest::profilePreviewIncludesWindowAndDesktopDetails()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    const Profile profile = test::makeValidProfile();
    QVERIFY(!repository.save(profile).has_value());

    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);
    const QVariantMap details = service.profileDetails(profile.id.toString(QUuid::WithoutBraces));

    QCOMPARE(details.value(QStringLiteral("name")).toString(), profile.name);
    QCOMPARE(details.value(QStringLiteral("desktopCount")).toInt(),
             static_cast<int>(profile.virtualDesktops.size()));
    QCOMPARE(details.value(QStringLiteral("applicationCount")).toInt(),
             static_cast<int>(profile.applications.size()));
    QCOMPARE(details.value(QStringLiteral("windowCount")).toInt(), 2);
    QCOMPARE(details.value(QStringLiteral("applications")).toList().size(), 1);
    QCOMPARE(details.value(QStringLiteral("windows")).toList().size(), 2);
}

void WorkspaceServiceTest::capturePreviewIncludesLimitationsAndMissingLaunchMetadata()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    ProfileRepository repository(temporaryDirectory.filePath(QStringLiteral("profiles")));
    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);

    service.saveWorkspace(QStringLiteral("Capture preview"));
    WorkspaceCaptureSnapshot snapshot = makeSnapshot(QStringLiteral("Capture preview"));
    snapshot.excludedWindowCount = 3;
    snapshot.geometryOmittedCount = 1;
    snapshot.stateGeometryUnavailableCount = 2;
    capture->complete(snapshot);

    const QVariantMap preview = service.preview();
    QCOMPARE(preview.value(QStringLiteral("excludedWindowCount")).toInt(), 3);
    QCOMPARE(preview.value(QStringLiteral("geometryOmittedCount")).toInt(), 1);
    QCOMPARE(preview.value(QStringLiteral("stateGeometryUnavailableCount")).toInt(), 2);
    QCOMPARE(preview.value(QStringLiteral("applicationsWithoutLaunchCount")).toInt(),
             static_cast<int>(snapshot.profile.applications.size()));
    QCOMPARE(preview.value(QStringLiteral("windows")).toList().size(), 2);
    QCOMPARE(preview.value(QStringLiteral("activeDesktopName")).toString(),
             snapshot.profile.virtualDesktops.first().name);
    service.cancelPreview();
    QVERIFY(!service.hasPreview());
}

void WorkspaceServiceTest::persistenceFailureKeepsPreviewAndDoesNotReplaceExistingData()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    const QString storageFilePath = temporaryDirectory.filePath(QStringLiteral("profiles"));
    QFile storageFile(storageFilePath);
    QVERIFY(storageFile.open(QIODevice::WriteOnly));
    const QByteArray originalContents = QByteArrayLiteral("keep this file intact");
    QCOMPARE(storageFile.write(originalContents), originalContents.size());
    storageFile.close();

    ProfileRepository repository(storageFilePath);
    auto *capture = new FakeCaptureBackend;
    WorkspaceService service(repository, capture);

    service.saveWorkspace(QStringLiteral("Cannot be saved"));
    capture->complete(makeSnapshot(QStringLiteral("Cannot be saved")));

    QVERIFY(service.hasPreview());
    QVERIFY(!service.confirmSavePreview());
    QVERIFY(service.hasPreview());
    QVERIFY(!service.errorMessage().isEmpty());
    QCOMPARE(service.profileModel()->count(), 0);

    QFile preservedFile(storageFilePath);
    QVERIFY(preservedFile.open(QIODevice::ReadOnly));
    QCOMPARE(preservedFile.readAll(), originalContents);
}

QTEST_APPLESS_MAIN(WorkspaceServiceTest)

#include "test_workspace_service.moc"
