#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "RobotQtViewerPlatformProfile.h"
#include "RobotQtViewerWorkbenchTransitionCoordinator.h"

#include <QCoreApplication>
#include <QObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    using namespace robot_qt_viewer;

    void require(bool condition, const char* message)
    {
        if(condition) {
            return;
        }
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }

    bool hasPlatformDiagnostic(
        const RobotQtViewerResolvedPlatformComposition& composition,
        const QString& code)
    {
        for(const RobotQtViewerPlatformDiagnostic& diagnostic : composition.diagnostics) {
            if(diagnostic.code == code) {
                return true;
            }
        }
        return false;
    }

    class FakeLifecycle final : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        FakeLifecycle(std::string id, std::vector<std::string>& calls)
            : m_id(std::move(id))
            , m_calls(calls)
        {
        }

        RobotQtViewerWorkbenchTransitionResult prepareDeactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override
        {
            ++prepareCount;
            prepareTransitionIds.push_back(context.transitionId);
            m_calls.push_back(m_id + ".prepare");
            if(onPrepare) {
                onPrepare();
            }
            return rejectPrepare
                ? workbenchTransitionRejected(QStringLiteral("rejected"))
                : workbenchTransitionSucceeded();
        }

        RobotQtViewerWorkbenchTransitionResult deactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override
        {
            ++deactivateCount;
            deactivateTransitionIds.push_back(context.transitionId);
            m_calls.push_back(m_id + ".deactivate");
            return failDeactivate
                ? workbenchTransitionFailed(QStringLiteral("deactivate failed"))
                : workbenchTransitionSucceeded();
        }

        RobotQtViewerWorkbenchTransitionResult activate(
            const RobotQtViewerWorkbenchActivationContext& context) override
        {
            ++activateCount;
            activationKinds.push_back(context.activationKind);
            m_calls.push_back(m_id + ".activate");
            return failActivate
                ? workbenchTransitionFailed(QStringLiteral("activate failed"))
                : workbenchTransitionSucceeded();
        }

        void releaseProject(
            const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept override
        {
            ++releaseCount;
            m_calls.push_back(m_id + ".release");
        }

        void shutdown(
            const RobotQtViewerWorkbenchShutdownContext&) noexcept override
        {
            ++shutdownCount;
            m_calls.push_back(m_id + ".shutdown");
        }

        bool rejectPrepare = false;
        bool failDeactivate = false;
        bool failActivate = false;
        int prepareCount = 0;
        int deactivateCount = 0;
        int activateCount = 0;
        int releaseCount = 0;
        int shutdownCount = 0;
        std::function<void()> onPrepare;
        std::vector<std::uint64_t> prepareTransitionIds;
        std::vector<std::uint64_t> deactivateTransitionIds;
        std::vector<RobotQtViewerWorkbenchActivationKind> activationKinds;

    private:
        std::string m_id;
        std::vector<std::string>& m_calls;
    };

    RobotQtViewerWorkbenchLifecyclePolicy noExecutionPolicy()
    {
        return {
            RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
            RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly
        };
    }

    void bind(
        RobotQtViewerWorkbenchPackageRegistry& registry,
        RobotQtViewerWorkbenchKind kind,
        FakeLifecycle& lifecycle)
    {
        require(
            registry.bindModeLifecycle(kind, lifecycle, noExecutionPolicy()),
            "lifecycle binding failed");
    }

    void testSuccessfulTransitionAndResume()
    {
        std::vector<std::string> calls;
        FakeLifecycle browse("browse", calls);
        FakeLifecycle motion("motion", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, browse);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, motion);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);

        require(coordinator.initializeActiveWorkbench(QStringLiteral("test")).succeeded(),
            "initial activation failed");
        calls.clear();
        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::Motion,
                    QStringLiteral("motion"))
                    .succeeded(),
            "Browse to Motion failed");
        require(calls == std::vector<std::string>{
                    "browse.prepare", "browse.deactivate", "motion.activate" },
            "successful transition order is incorrect");
        require(manager.activeWorkbench() == RobotQtViewerWorkbenchKind::Motion,
            "target mode was not committed");

        calls.clear();
        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("browse"))
                    .succeeded(),
            "Motion to Browse failed");
        require(browse.activationKinds.back() == RobotQtViewerWorkbenchActivationKind::Resume,
            "suspended mode did not resume");

        calls.clear();
        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("same"))
                    .succeeded(),
            "same-mode transition should succeed");
        require(calls.empty(), "same-mode transition invoked lifecycle hooks");
    }

    void testRejectionAndActivationRollback()
    {
        std::vector<std::string> calls;
        FakeLifecycle browse("browse", calls);
        FakeLifecycle motion("motion", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, browse);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, motion);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);
        require(coordinator.initializeActiveWorkbench().succeeded(), "initial activation failed");

        browse.rejectPrepare = true;
        calls.clear();
        require(!coordinator.requestTransition(
                     RobotQtViewerWorkbenchKind::Motion,
                     QStringLiteral("rejected"))
                     .succeeded(),
            "prepare rejection was ignored");
        require(manager.activeWorkbench() == RobotQtViewerWorkbenchKind::Browse,
            "rejected transition changed active mode");
        require(calls == std::vector<std::string>{ "browse.prepare" },
            "rejected transition executed later phases");

        browse.rejectPrepare = false;
        motion.failActivate = true;
        calls.clear();
        require(!coordinator.requestTransition(
                     RobotQtViewerWorkbenchKind::Motion,
                     QStringLiteral("activationFailure"))
                     .succeeded(),
            "target activation failure was ignored");
        require(manager.activeWorkbench() == RobotQtViewerWorkbenchKind::Browse,
            "activation failure committed target mode");
        require(calls == std::vector<std::string>{
                    "browse.prepare",
                    "browse.deactivate",
                    "motion.activate",
                    "motion.deactivate",
                    "browse.activate" },
            "activation rollback order is incorrect");
        require(coordinator.lifecycleState(RobotQtViewerWorkbenchKind::Browse) ==
                RobotQtViewerWorkbenchLifecycleState::Active,
            "previous mode was not restored after activation failure");
    }

    void testDeactivateAndRollbackFailures()
    {
        std::vector<std::string> calls;
        FakeLifecycle browse("browse", calls);
        FakeLifecycle motion("motion", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, browse);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, motion);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);
        require(coordinator.initializeActiveWorkbench().succeeded(), "initial activation failed");

        browse.failDeactivate = true;
        calls.clear();
        const RobotQtViewerWorkbenchTransitionResult recovered = coordinator.requestTransition(
            RobotQtViewerWorkbenchKind::Motion,
            QStringLiteral("deactivateFailure"));
        require(!recovered.succeeded(), "deactivate failure was ignored");
        require(calls == std::vector<std::string>{
                    "browse.prepare", "browse.deactivate", "browse.activate" },
            "deactivate rollback order is incorrect");
        require(manager.activeWorkbench() == RobotQtViewerWorkbenchKind::Browse,
            "deactivate failure changed committed mode");
        require(coordinator.lifecycleState(RobotQtViewerWorkbenchKind::Browse) ==
                RobotQtViewerWorkbenchLifecycleState::Active,
            "deactivate rollback did not restore active state");

        browse.failActivate = true;
        calls.clear();
        const RobotQtViewerWorkbenchTransitionResult rollbackFailed =
            coordinator.requestTransition(
                RobotQtViewerWorkbenchKind::Motion,
                QStringLiteral("rollbackFailure"));
        require(!rollbackFailed.succeeded(), "rollback failure was ignored");
        require(rollbackFailed.diagnosticCode == QStringLiteral("workbench.rollback.failed"),
            "rollback failure diagnostic is incorrect");
        require(coordinator.lifecycleState(RobotQtViewerWorkbenchKind::Browse) ==
                RobotQtViewerWorkbenchLifecycleState::Failed,
            "failed rollback did not enter failed state");

        browse.failDeactivate = false;
        browse.failActivate = false;
        calls.clear();
        const RobotQtViewerWorkbenchTransitionResult blocked = coordinator.requestTransition(
            RobotQtViewerWorkbenchKind::Motion,
            QStringLiteral("blockedAfterRollbackFailure"));
        require(!blocked.succeeded() && calls.empty(),
            "transition continued from a failed active state");
    }

    void testTargetCleanupFailureAndReentrancy()
    {
        std::vector<std::string> calls;
        FakeLifecycle browse("browse", calls);
        FakeLifecycle motion("motion", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, browse);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, motion);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);
        require(coordinator.initializeActiveWorkbench().succeeded(), "initial activation failed");

        RobotQtViewerWorkbenchTransitionResult nestedResult;
        browse.onPrepare = [&]() {
            nestedResult = coordinator.requestTransition(
                RobotQtViewerWorkbenchKind::Motion,
                QStringLiteral("nested"));
        };
        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::Motion,
                    QStringLiteral("outer"))
                    .succeeded(),
            "outer transition failed");
        require(!nestedResult.succeeded() &&
                nestedResult.diagnosticCode == QStringLiteral("workbench.transition.reentrant"),
            "reentrant transition was not rejected");

        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("return"))
                    .succeeded(),
            "return to Browse failed");
        browse.onPrepare = {};
        motion.failActivate = true;
        motion.failDeactivate = true;
        const RobotQtViewerWorkbenchTransitionResult cleanupFailed =
            coordinator.requestTransition(
                RobotQtViewerWorkbenchKind::Motion,
                QStringLiteral("cleanupFailure"));
        require(!cleanupFailed.succeeded() &&
                cleanupFailed.diagnosticCode ==
                    QStringLiteral("workbench.target_cleanup.failed"),
            "target cleanup failure diagnostic is incorrect");
        require(coordinator.lifecycleState(RobotQtViewerWorkbenchKind::Motion) ==
                RobotQtViewerWorkbenchLifecycleState::Failed,
            "target cleanup failure did not quarantine the target mode");

        calls.clear();
        const RobotQtViewerWorkbenchTransitionResult blocked = coordinator.requestTransition(
            RobotQtViewerWorkbenchKind::Motion,
            QStringLiteral("retryFailedTarget"));
        require(!blocked.succeeded() && calls.empty(),
            "failed target mode was immediately retried");
    }

    void testPreparedTransitionContextAndSharedLifecycle()
    {
        std::vector<std::string> calls;
        FakeLifecycle shared("shared", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, shared);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, shared);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);
        require(coordinator.initializeActiveWorkbench().succeeded(), "initial activation failed");

        require(coordinator.prepareActiveDeactivation(
                    RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
                    QStringLiteral("replace"))
                    .succeeded(),
            "project replacement prepare failed");
        require(coordinator.deactivateActive(
                    RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
                    QStringLiteral("replace"),
                    nullptr,
                    true)
                    .succeeded(),
            "prepared project deactivation failed");
        require(shared.prepareTransitionIds.back() == shared.deactivateTransitionIds.back(),
            "prepare and deactivate used different transition ids");

        coordinator.releaseProject(QStringLiteral("replace"));
        require(shared.releaseCount == 1,
            "shared lifecycle received duplicate project release");
        require(coordinator.lifecycleState(RobotQtViewerWorkbenchKind::Browse) ==
                RobotQtViewerWorkbenchLifecycleState::Inactive &&
                coordinator.lifecycleState(RobotQtViewerWorkbenchKind::Motion) ==
                    RobotQtViewerWorkbenchLifecycleState::Inactive,
            "shared lifecycle modes did not both reset after project release");

        coordinator.shutdownAll(QStringLiteral("test"));
        require(shared.shutdownCount == 1,
            "shared lifecycle received duplicate shutdown");
    }

    void testProjectReleaseShutdownAndMissingLifecycle()
    {
        std::vector<std::string> calls;
        FakeLifecycle browse("browse", calls);
        FakeLifecycle motion("motion", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, browse);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, motion);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);
        require(coordinator.initializeActiveWorkbench().succeeded(), "initial activation failed");

        require(!coordinator.requestTransition(
                     RobotQtViewerWorkbenchKind::Collision,
                     QStringLiteral("missing"))
                     .succeeded(),
            "mode without lifecycle was accepted");

        coordinator.releaseProject(QStringLiteral("newProject"));
        require(coordinator.projectGeneration() == 2, "project generation did not advance");
        require(browse.releaseCount == 1 && motion.releaseCount == 1,
            "project release was not delivered exactly once");
        require(coordinator.activateCommittedWorkbench(
                    RobotQtViewerWorkbenchActivationKind::FreshProject,
                    RobotQtViewerWorkbenchTransitionCause::ProjectLoaded,
                    QStringLiteral("newProject"))
                    .succeeded(),
            "fresh-project activation failed");

        coordinator.shutdownAll(QStringLiteral("test"));
        coordinator.shutdownAll(QStringLiteral("testAgain"));
        require(browse.shutdownCount == 1 && motion.shutdownCount == 1,
            "shutdown was not delivered exactly once");
    }

    RobotQtViewerWorkbenchPackageRegistry makeProfileCatalog()
    {
        RobotQtViewerWorkbenchPackageRegistry catalog;
        const QString packageId = QStringLiteral("smrobot.workbench.test");
        require(catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
                    packageId,
                    QStringLiteral("Test Workbench"),
                    RobotQtViewerWorkbenchPackageSource::BuiltInSource)),
            "profile test package registration failed");
        require(catalog.registerMode(makeRobotQtViewerWorkbenchMode(
                    packageId,
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("browseWorkbench"),
                    10,
                    { QStringLiteral("smrobot.feature.project-assembly") })),
            "profile test Browse registration failed");
        require(catalog.registerMode(makeRobotQtViewerWorkbenchMode(
                    packageId,
                    RobotQtViewerWorkbenchKind::Motion,
                    QStringLiteral("motionWorkbench"),
                    20,
                    { QStringLiteral("smrobot.feature.robot-run") },
                    { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) })),
            "profile test Motion registration failed");
        require(catalog.registerMode(makeRobotQtViewerWorkbenchMode(
                    packageId,
                    RobotQtViewerWorkbenchKind::Collision,
                    QStringLiteral("collisionWorkbench"),
                    30,
                    {},
                    { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) })),
            "profile test Collision registration failed");
        require(catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
                    QStringLiteral("smrobot.feature.project-assembly"),
                    QStringLiteral("Project Assembly"),
                    packageId,
                    { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) })),
            "profile test feature registration failed");
        return catalog;
    }

    RobotQtViewerPlatformProfile makeProfile()
    {
        RobotQtViewerPlatformProfile profile;
        profile.id = QStringLiteral("test-platform");
        profile.displayName = QStringLiteral("Test Platform");
        profile.requiredFeatureIds = QStringList{
            QStringLiteral("smrobot.feature.project-assembly") };
        profile.optionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion),
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision)
        };
        profile.defaultEnabledOptionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion)
        };
        profile.defaultModeId = robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        profile.modeOrder = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision),
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion)
        };
        return profile;
    }

    void testPlatformProfileResolutionAndOverlay()
    {
        RobotQtViewerWorkbenchPackageRegistry catalog = makeProfileCatalog();
        const RobotQtViewerPlatformProfile profile = makeProfile();
        RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(resolved.succeeded(), "valid platform profile did not resolve");
        require(resolved.enabledModeIds == QStringList({
                    robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
                    robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion) }),
            "default-enabled optional modes were not resolved deterministically");

        RobotQtViewerPlatformUserOverlay overlay;
        overlay.profileId = profile.id;
        overlay.enabledOptionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision)
        };
        overlay.disabledOptionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion)
        };
        resolved = RobotQtViewerPlatformProfileResolver::resolve(catalog, profile, overlay);
        require(resolved.succeeded(), "valid optional overlay did not resolve");
        require(resolved.enabledModeIds == QStringList({
                    robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
                    robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision) }),
            "optional overlay did not replace default choices");
        require(catalog.setEnabledModeIds(resolved.enabledModeIds),
            "resolved mode set was rejected by the catalog");
        require(catalog.hasMode(RobotQtViewerWorkbenchKind::Collision) &&
                !catalog.hasMode(RobotQtViewerWorkbenchKind::Motion),
            "catalog filtering did not follow resolved composition");
    }

    void testPlatformProfileDiagnosticsAndOverlayIo()
    {
        RobotQtViewerWorkbenchPackageRegistry catalog = makeProfileCatalog();
        RobotQtViewerPlatformProfile profile = makeProfile();
        RobotQtViewerPlatformUserOverlay invalidOverlay;
        invalidOverlay.profileId = profile.id;
        invalidOverlay.disabledOptionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse)
        };
        RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(catalog, profile, invalidOverlay);
        require(!resolved.succeeded(), "overlay was allowed to disable a non-optional mode");

        profile.requiredFeatureIds = QStringList{
            QStringLiteral("smrobot.feature.missing") };
        resolved = RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(!resolved.succeeded(), "missing required feature was accepted");

        profile = makeProfile();
        profile.optionalModeIds.push_back(QStringLiteral("smrobot.mode.not-built"));
        resolved = RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(resolved.succeeded(), "missing unused optional mode made the profile fail");
        require(hasPlatformDiagnostic(resolved, QStringLiteral("platform.mode.optional_missing")),
            "missing unused optional mode did not produce a warning");

        RobotQtViewerPlatformUserOverlay invalidSchemaOverlay;
        invalidSchemaOverlay.version = 2;
        invalidSchemaOverlay.profileId = profile.id;
        resolved = RobotQtViewerPlatformProfileResolver::resolve(
            catalog, profile, invalidSchemaOverlay);
        require(!resolved.succeeded(), "unsupported overlay schema version was accepted");

        RobotQtViewerPlatformUserOverlay contradictoryOverlay;
        contradictoryOverlay.profileId = profile.id;
        contradictoryOverlay.enabledOptionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion) };
        contradictoryOverlay.disabledOptionalModeIds =
            contradictoryOverlay.enabledOptionalModeIds;
        resolved = RobotQtViewerPlatformProfileResolver::resolve(
            catalog, profile, contradictoryOverlay);
        require(!resolved.succeeded(), "contradictory overlay was accepted");

        RobotQtViewerWorkbenchPackageRegistry missingDependencyCatalog;
        const QString dependencyPackageId = QStringLiteral("smrobot.workbench.dependency-test");
        require(missingDependencyCatalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
                    dependencyPackageId,
                    QStringLiteral("Dependency Test"),
                    RobotQtViewerWorkbenchPackageSource::BuiltInSource)),
            "dependency test package registration failed");
        require(missingDependencyCatalog.registerMode(makeRobotQtViewerWorkbenchMode(
                    dependencyPackageId,
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("browseWorkbench"),
                    10)),
            "dependency test Browse registration failed");
        require(missingDependencyCatalog.registerMode(makeRobotQtViewerWorkbenchMode(
                    dependencyPackageId,
                    RobotQtViewerWorkbenchKind::Motion,
                    QStringLiteral("motionWorkbench"),
                    20,
                    {},
                    { QStringLiteral("smrobot.mode.not-built") })),
            "dependency test Motion registration failed");
        RobotQtViewerPlatformProfile missingDependencyProfile;
        missingDependencyProfile.id = QStringLiteral("dependency-test");
        missingDependencyProfile.displayName = QStringLiteral("Dependency Test");
        missingDependencyProfile.requiredModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) };
        missingDependencyProfile.optionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion) };
        missingDependencyProfile.defaultEnabledOptionalModeIds =
            missingDependencyProfile.optionalModeIds;
        missingDependencyProfile.defaultModeId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        resolved = RobotQtViewerPlatformProfileResolver::resolve(
            missingDependencyCatalog, missingDependencyProfile);
        require(resolved.succeeded(), "unavailable optional dependency made the profile fail");
        require(!resolved.containsMode(RobotQtViewerWorkbenchKind::Motion),
            "mode with a missing dependency remained enabled");
        require(hasPlatformDiagnostic(resolved, QStringLiteral("platform.mode.dependency_missing")),
            "missing optional dependency did not produce a warning");

        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "temporary overlay directory creation failed");
        const std::filesystem::path overlayPath = std::filesystem::u8path(
            temporaryDirectory.path().toUtf8().constData()) / "test.overlay.json";
        RobotQtViewerPlatformUserOverlay savedOverlay;
        savedOverlay.profileId = QStringLiteral("test-platform");
        savedOverlay.enabledOptionalModeIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision)
        };
        QString ioError;
        require(RobotQtViewerPlatformProfileIo::saveOverlay(
                    overlayPath, savedOverlay, &ioError),
            "platform overlay save failed");
        RobotQtViewerPlatformUserOverlay loadedOverlay;
        require(RobotQtViewerPlatformProfileIo::loadOverlay(
                    overlayPath, &loadedOverlay, &ioError),
            "platform overlay load failed");
        require(loadedOverlay.profileId == savedOverlay.profileId &&
                loadedOverlay.enabledOptionalModeIds == savedOverlay.enabledOptionalModeIds,
            "platform overlay round trip changed data");
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testSuccessfulTransitionAndResume();
    testRejectionAndActivationRollback();
    testDeactivateAndRollbackFailures();
    testTargetCleanupFailureAndReentrancy();
    testPreparedTransitionContextAndSharedLifecycle();
    testProjectReleaseShutdownAndMissingLifecycle();
    testPlatformProfileResolutionAndOverlay();
    testPlatformProfileDiagnosticsAndOverlayIo();
    std::cout << "Workbench lifecycle tests passed.\n";
    return 0;
}
