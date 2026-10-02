#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerEditSession.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "RobotQtViewerPlatformProfile.h"
#include "RobotQtViewerWorkbenchTransitionCoordinator.h"

#include <QCoreApplication>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
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

    class FakeLifecycle : public IRobotQtViewerWorkbenchLifecycle
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

    class FakeEditSession final : public IWorkbenchEditSession
    {
    public:
        QString ownerWorkbenchId() const override
        {
            return QStringLiteral("smrobot.mode.test");
        }

        QString taskId() const override
        {
            return QStringLiteral("test.edit");
        }

        RobotQtViewerEditSessionState editSessionState() const override
        {
            return state;
        }

        RobotQtViewerWorkbenchTransitionResult prepareTransition(
            const RobotQtViewerEditTransitionRequest& request) override
        {
            ++prepareCount;
            lastCause = request.cause;
            if(onPrepare) {
                onPrepare();
            }
            if(throwOnPrepare) {
                throw std::runtime_error("edit-session prepare failed");
            }
            if(reject) {
                return workbenchTransitionRejected(QStringLiteral("cancelled"));
            }
            state = RobotQtViewerEditSessionState::Clean;
            return workbenchTransitionSucceeded();
        }

        void releaseEditSession(std::uint64_t generation) noexcept override
        {
            ++releaseCount;
            releasedGeneration = generation;
            state = RobotQtViewerEditSessionState::Idle;
        }

        RobotQtViewerEditSessionState state = RobotQtViewerEditSessionState::Clean;
        RobotQtViewerWorkbenchTransitionCause lastCause =
            RobotQtViewerWorkbenchTransitionCause::UserWorkbenchSwitch;
        bool reject = false;
        bool throwOnPrepare = false;
        int prepareCount = 0;
        int releaseCount = 0;
        std::uint64_t releasedGeneration = 0;
        std::function<void()> onPrepare;
    };

    class FakeLifecycleEditSession final
        : public FakeLifecycle
        , public IWorkbenchEditSession
    {
    public:
        FakeLifecycleEditSession(std::string id, std::vector<std::string>& calls)
            : FakeLifecycle(std::move(id), calls)
        {
        }

        QString ownerWorkbenchId() const override
        {
            return robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        }

        QString taskId() const override
        {
            return QStringLiteral("test.runtime-contribution");
        }

        RobotQtViewerEditSessionState editSessionState() const override
        {
            return RobotQtViewerEditSessionState::Dirty;
        }

        RobotQtViewerWorkbenchTransitionResult prepareTransition(
            const RobotQtViewerEditTransitionRequest&) override
        {
            return workbenchTransitionSucceeded();
        }

        void releaseEditSession(std::uint64_t) noexcept override
        {
        }
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
            registry.bindWorkbenchLifecycle(kind, lifecycle, noExecutionPolicy()),
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

    void testEditSessionSelectionTransition()
    {
        RobotQtViewerEventHub events;
        RobotQtViewerSelectionModel selection(events);
        RobotQtViewerEditSessionCoordinator coordinator;
        FakeEditSession session;
        require(coordinator.registerSession(session), "edit session registration failed");
        selection.setEditSessionCoordinator(&coordinator);

        selection.selectRobotLink(
            QStringLiteral("robot_a"),
            QStringLiteral("link_a"),
            QStringLiteral("initial"));
        session.state = RobotQtViewerEditSessionState::Dirty;
        session.reject = true;
        selection.selectRobotLink(
            QStringLiteral("robot_b"),
            QStringLiteral("link_b"),
            QStringLiteral("rejectedSelection"));
        require(selection.state().robotId == QStringLiteral("robot_a") &&
                selection.state().linkName == QStringLiteral("link_a"),
            "rejected selection did not preserve the committed target");
        require(session.prepareCount == 1 &&
                session.lastCause == RobotQtViewerWorkbenchTransitionCause::SelectionChange,
            "selection did not invoke the shared edit-session virtual protocol");

        session.reject = false;
        selection.selectRobotLink(
            QStringLiteral("robot_b"),
            QStringLiteral("link_b"),
            QStringLiteral("acceptedSelection"));
        require(selection.state().robotId == QStringLiteral("robot_b") &&
                selection.state().linkName == QStringLiteral("link_b"),
            "accepted selection did not commit the target");
        require(session.prepareCount == 2,
            "accepted selection did not reuse the edit-session protocol");

        session.state = RobotQtViewerEditSessionState::Dirty;
        session.reject = true;
        require(!selection.prepareSelectionChange(QStringLiteral("rejectedPreflight")),
            "selection preflight ignored edit-session rejection");
        require(selection.state().robotId == QStringLiteral("robot_b") &&
                selection.state().linkName == QStringLiteral("link_b"),
            "selection preflight changed the committed target");

        session.reject = false;
        require(selection.prepareSelectionChange(QStringLiteral("acceptedPreflight")),
            "selection preflight did not accept a successful edit-session transition");
        require(session.prepareCount == 4,
            "selection preflight did not use the shared edit-session protocol");

        coordinator.releaseProject(7);
        require(session.releaseCount == 1 && session.releasedGeneration == 7 &&
                session.state == RobotQtViewerEditSessionState::Idle,
            "project release did not clear registered edit sessions");
    }

    void testEditSessionCoordinatorFailureAndReentrancy()
    {
        RobotQtViewerEditSessionCoordinator coordinator;
        FakeEditSession session;
        require(coordinator.registerSession(session), "edit session registration failed");

        RobotQtViewerWorkbenchTransitionResult nestedResult;
        session.state = RobotQtViewerEditSessionState::Dirty;
        session.onPrepare = [&]() {
            nestedResult = coordinator.prepareTransition({});
        };
        const RobotQtViewerWorkbenchTransitionResult outerResult =
            coordinator.prepareTransition({});
        require(outerResult.succeeded(), "outer edit-session transition failed");
        require(!nestedResult.succeeded() &&
                nestedResult.diagnosticCode ==
                    QStringLiteral("edit_session.transition.reentrant"),
            "reentrant edit-session transition was not rejected");
        require(!coordinator.transitionInProgress(),
            "edit-session coordinator remained busy after a successful transition");

        session.onPrepare = {};
        session.state = RobotQtViewerEditSessionState::Dirty;
        session.throwOnPrepare = true;
        const RobotQtViewerWorkbenchTransitionResult exceptionResult =
            coordinator.prepareTransition({});
        require(!exceptionResult.succeeded() &&
                exceptionResult.diagnosticCode ==
                    QStringLiteral("edit_session.transition.exception"),
            "edit-session exception did not produce the expected failure");
        require(!coordinator.transitionInProgress(),
            "edit-session coordinator remained busy after an exception");

        session.throwOnPrepare = false;
        session.state = RobotQtViewerEditSessionState::Dirty;
        require(coordinator.prepareTransition({}).succeeded(),
            "edit-session coordinator did not recover after an exception");

        FakeEditSession secondSession;
        session.state = RobotQtViewerEditSessionState::Dirty;
        secondSession.state = RobotQtViewerEditSessionState::Dirty;
        require(coordinator.registerSession(secondSession),
            "second edit session registration failed");
        const RobotQtViewerWorkbenchTransitionResult multipleOwnerResult =
            coordinator.prepareTransition({});
        require(!multipleOwnerResult.succeeded() &&
                multipleOwnerResult.diagnosticCode ==
                    QStringLiteral("edit_session.multiple_mutation_owners"),
            "multiple dirty edit-session owners were not rejected");
        require(session.prepareCount == 3 && secondSession.prepareCount == 0,
            "multiple-owner failure invoked an edit session");
    }

    void testReturnToWorkbenchEntrySource()
    {
        std::vector<std::string> calls;
        FakeLifecycle browse("browse", calls);
        FakeLifecycle motion("motion", calls);
        FakeLifecycle toolSetup("toolSetup", calls);
        auto registry = defaultRobotQtViewerWorkbenchPackageRegistry();
        bind(registry, RobotQtViewerWorkbenchKind::Browse, browse);
        bind(registry, RobotQtViewerWorkbenchKind::Motion, motion);
        bind(registry, RobotQtViewerWorkbenchKind::ToolSetup, toolSetup);
        RobotQtViewerWorkbenchManager manager;
        RobotQtViewerEventHub events;
        RobotQtViewerWorkbenchTransitionCoordinator coordinator(registry, manager, events);

        require(coordinator.initializeActiveWorkbench(QStringLiteral("test-return")).succeeded(),
            "initial activation for Workbench return failed");
        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::Motion,
                    QStringLiteral("enter-motion"))
                    .succeeded(),
            "entering Motion before Tool Setup failed");
        require(coordinator.requestTransition(
                    RobotQtViewerWorkbenchKind::ToolSetup,
                    QStringLiteral("enter-tool-setup"))
                    .succeeded(),
            "entering Tool Setup failed");
        require(coordinator.requestReturn(
                    robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
                    QStringLiteral("tool-setup-done"))
                    .succeeded(),
            "returning from Tool Setup failed");
        require(manager.activeWorkbench() == RobotQtViewerWorkbenchKind::Motion,
            "Workbench Done did not return to the entry source");
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
                    QStringLiteral("Test Workbench"))),
            "profile test package registration failed");
        require(catalog.registerWorkbench(makeRobotQtViewerWorkbench(
                    packageId,
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("browseWorkbench"),
                    10,
                    { QStringLiteral("smrobot.feature.project-assembly") })),
            "profile test Browse registration failed");
        require(catalog.registerWorkbench(makeRobotQtViewerWorkbench(
                    packageId,
                    RobotQtViewerWorkbenchKind::Motion,
                    QStringLiteral("motionWorkbench"),
                    20,
                    { QStringLiteral("smrobot.feature.robot-run") },
                    { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) })),
            "profile test Motion registration failed");
        require(catalog.registerWorkbench(makeRobotQtViewerWorkbench(
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
        profile.workbenchIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion),
        };
        profile.defaultWorkbenchId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        return profile;
    }

    void testPlatformProfileResolutionAndOverlay()
    {
        RobotQtViewerWorkbenchPackageRegistry catalog = makeProfileCatalog();
        std::vector<std::string> calls;
        FakeLifecycle collisionLifecycle("collision", calls);
        RobotQtViewerNoOpLanguageParticipant collisionLanguage;
        bind(catalog, RobotQtViewerWorkbenchKind::Collision, collisionLifecycle);
        require(catalog.bindWorkbenchLanguageParticipant(
                    RobotQtViewerWorkbenchKind::Collision, collisionLanguage),
            "Collision language participant binding failed");
        require(catalog.isWorkbenchReady(RobotQtViewerWorkbenchKind::Collision) &&
                catalog.isWorkbenchLanguageReady(RobotQtViewerWorkbenchKind::Collision),
            "Collision Workbench was not ready before Profile filtering");
        const RobotQtViewerPlatformProfile profile = makeProfile();
        RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(resolved.succeeded(), "valid platform profile did not resolve");
        require(resolved.enabledWorkbenchIds == profile.workbenchIds,
            "resolved Workbench composition differs from the Profile order");
        require(catalog.setEnabledWorkbenchIds(resolved.enabledWorkbenchIds),
            "resolved Workbench set was rejected by the catalog");
        require(catalog.hasWorkbench(RobotQtViewerWorkbenchKind::Motion) &&
                !catalog.hasWorkbench(RobotQtViewerWorkbenchKind::Collision),
            "catalog filtering did not follow resolved composition");
        require(catalog.lifecycle(RobotQtViewerWorkbenchKind::Collision) == nullptr &&
                catalog.languageParticipant(RobotQtViewerWorkbenchKind::Collision) == nullptr,
            "disabled Workbench retained lifecycle or language runtime bindings");
    }

    void testRuntimeContributionHost()
    {
        auto registry = makeProfileCatalog();
        const QString browseId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        const QString motionId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion);
        require(registry.setEnabledWorkbenchIds(QStringList{ browseId }),
            "runtime contribution test catalog filtering failed");

        RobotQtViewerEditSessionCoordinator editSessions;
        RobotQtViewerWorkbenchContributionHost host(registry, editSessions);
        QWidget* const browsePanel = reinterpret_cast<QWidget*>(std::uintptr_t{ 1 });
        int browseFactoryCalls = 0;
        int disabledFactoryCalls = 0;
        std::vector<std::string> lifecycleCalls;

        require(host.registerFactory({
                    browseId,
                    [&](QWidget*) {
                        ++browseFactoryCalls;
                        return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                            browseId,
                            [browsePanel]() { return browsePanel; },
                            std::make_unique<FakeLifecycleEditSession>(
                                "browse", lifecycleCalls),
                            noExecutionPolicy(),
                            std::make_unique<RobotQtViewerNoOpLanguageParticipant>());
                    } }),
            "enabled runtime contribution factory registration failed");
        require(!host.registerFactory({ browseId, {} }),
            "duplicate runtime contribution factory was accepted");
        require(host.registerFactory({
                    motionId,
                    [&](QWidget*) {
                        ++disabledFactoryCalls;
                        QWidget* const panel =
                            reinterpret_cast<QWidget*>(std::uintptr_t{ 2 });
                        return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                            motionId,
                            [panel]() { return panel; },
                            std::make_unique<FakeLifecycle>("motion", lifecycleCalls),
                            noExecutionPolicy(),
                            std::make_unique<RobotQtViewerNoOpLanguageParticipant>());
                    } }),
            "disabled runtime contribution factory registration failed");

        QString error;
        require(host.instantiateEnabled(QStringList{ browseId }, nullptr, &error),
            "enabled runtime contribution instantiation failed");
        require(error.isEmpty() && browseFactoryCalls == 1 && disabledFactoryCalls == 0,
            "runtime contribution construction did not follow enabled Workbenches");
        require(host.panelForWorkbench(browseId) == browsePanel &&
                host.hasContribution(browseId) && !host.hasContribution(motionId),
            "runtime contribution panel resolution failed");
        require(registry.lifecycle(browseId) != nullptr &&
                registry.languageParticipant(RobotQtViewerWorkbenchKind::Browse) != nullptr &&
                editSessions.pendingSessionCount() == 1,
            "runtime contribution bindings were not registered");
        require(!host.instantiateEnabled(QStringList{ browseId }, nullptr, &error),
            "runtime contributions were instantiated twice");

        host.clear();
        require(registry.lifecycle(browseId) == nullptr &&
                registry.languageParticipant(RobotQtViewerWorkbenchKind::Browse) == nullptr &&
                editSessions.pendingSessionCount() == 0,
            "runtime contribution teardown left registry or edit-session bindings");

        require(!host.instantiateEnabled(QStringList{ browseId, browseId }, nullptr, &error),
            "duplicate enabled runtime contribution was accepted");
        require(browseFactoryCalls == 2,
            "duplicate enabled runtime contribution did not stop transactionally");

        auto missingRegistry = makeProfileCatalog();
        require(missingRegistry.setEnabledWorkbenchIds(QStringList{ browseId }),
            "missing-factory test catalog filtering failed");
        RobotQtViewerEditSessionCoordinator missingSessions;
        RobotQtViewerWorkbenchContributionHost missingHost(missingRegistry, missingSessions);
        require(!missingHost.instantiateEnabled(QStringList{ browseId }, nullptr, &error),
            "missing runtime contribution factory was accepted");
        require(missingRegistry.lifecycle(browseId) == nullptr,
            "missing runtime contribution left a lifecycle binding");

        require(missingHost.registerFactory({
                    browseId,
                    [](QWidget*) {
                        return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>();
                    } }),
            "incomplete contribution factory registration failed");
        require(!missingHost.instantiateEnabled(QStringList{ browseId }, nullptr, &error),
            "incomplete runtime contribution was accepted");
    }

    void testPlatformProfileDiagnosticsAndOverlayIo()
    {
        RobotQtViewerWorkbenchPackageRegistry catalog = makeProfileCatalog();
        RobotQtViewerPlatformProfile profile = makeProfile();
        profile.workbenchIds.push_back(QStringLiteral("smrobot.mode.not-built"));
        RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(!resolved.succeeded(), "missing Workbench was accepted");
        require(hasPlatformDiagnostic(resolved, QStringLiteral("platform.workbench.missing")),
            "missing Workbench did not produce a diagnostic");

        profile = makeProfile();
        profile.workbenchIds.removeAll(
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse));
        profile.defaultWorkbenchId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion);
        resolved = RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(!resolved.succeeded(), "undeclared Workbench dependency was accepted");
        require(hasPlatformDiagnostic(
                    resolved, QStringLiteral("platform.workbench.dependency_not_declared")),
            "undeclared Workbench dependency did not produce a diagnostic");

        profile = makeProfile();
        profile.workbenchIds.push_back(profile.workbenchIds.front());
        resolved = RobotQtViewerPlatformProfileResolver::resolve(catalog, profile);
        require(!resolved.succeeded(), "duplicate Workbench ID was accepted");
        require(hasPlatformDiagnostic(resolved, QStringLiteral("platform.workbench.duplicate")),
            "duplicate Workbench ID did not produce a diagnostic");

        RobotQtViewerWorkbenchPackageRegistry missingDependencyCatalog;
        const QString dependencyPackageId = QStringLiteral("smrobot.workbench.dependency-test");
        require(missingDependencyCatalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
                    dependencyPackageId,
                    QStringLiteral("Dependency Test"))),
            "dependency test package registration failed");
        require(missingDependencyCatalog.registerWorkbench(makeRobotQtViewerWorkbench(
                    dependencyPackageId,
                    RobotQtViewerWorkbenchKind::Browse,
                    QStringLiteral("browseWorkbench"),
                    10)),
            "dependency test Browse registration failed");
        require(missingDependencyCatalog.registerWorkbench(makeRobotQtViewerWorkbench(
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
        missingDependencyProfile.workbenchIds = QStringList{
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion)
        };
        missingDependencyProfile.defaultWorkbenchId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        resolved = RobotQtViewerPlatformProfileResolver::resolve(
            missingDependencyCatalog, missingDependencyProfile);
        require(!resolved.succeeded(), "missing explicit dependency was accepted");
        require(hasPlatformDiagnostic(
                    resolved, QStringLiteral("platform.workbench.dependency_not_declared")),
            "missing explicit dependency did not produce a diagnostic");

        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "temporary profile directory creation failed");
        QString ioError;
        const std::filesystem::path selectionPath = std::filesystem::u8path(
            temporaryDirectory.path().toUtf8().constData()) / "simulation-platform.json";
        RobotQtViewerPlatformSelection savedSelection;
        savedSelection.profileId = QStringLiteral("test-platform");
        require(RobotQtViewerPlatformProfileIo::saveSelection(
                    selectionPath, savedSelection, &ioError),
            "platform selection save failed");
        RobotQtViewerPlatformSelection loadedSelection;
        require(RobotQtViewerPlatformProfileIo::loadSelection(
                    selectionPath, &loadedSelection, &ioError),
            "platform selection load failed");
        require(loadedSelection.profileId == savedSelection.profileId,
            "platform selection round trip changed the profile id");

        const std::filesystem::path profilePath = std::filesystem::u8path(
            temporaryDirectory.path().toUtf8().constData()) / "test.platform.json";
        const RobotQtViewerPlatformProfile savedProfile = makeProfile();
        require(RobotQtViewerPlatformProfileIo::saveProfile(
                    profilePath, savedProfile, &ioError),
            "platform profile save failed");
        RobotQtViewerPlatformProfile loadedProfile;
        require(RobotQtViewerPlatformProfileIo::loadProfile(
                    profilePath, &loadedProfile, &ioError),
            "platform profile load failed");
        require(loadedProfile.id == savedProfile.id &&
                loadedProfile.displayName == savedProfile.displayName &&
                loadedProfile.workbenchIds == savedProfile.workbenchIds &&
                loadedProfile.defaultWorkbenchId == savedProfile.defaultWorkbenchId,
            "platform profile round trip changed data");

        const std::filesystem::path legacyProfilePath = std::filesystem::u8path(
            temporaryDirectory.path().toUtf8().constData()) / "legacy.platform.json";
        QFile legacyProfileFile(QString::fromStdWString(legacyProfilePath.wstring()));
        require(legacyProfileFile.open(QIODevice::WriteOnly),
            "legacy profile fixture could not be written");
        require(legacyProfileFile.write(
                    "{\"schema\":\"smrobot.platform-profile\",\"version\":1,"
                    "\"id\":\"legacy\",\"displayName\":\"Legacy\"}") > 0,
            "legacy profile fixture write failed");
        legacyProfileFile.close();
        require(!RobotQtViewerPlatformProfileIo::loadProfile(
                    legacyProfilePath, &loadedProfile, &ioError) &&
                ioError.contains(QStringLiteral("migrated to v2")),
            "legacy Product Profile did not receive an explicit v2 migration error");

        const std::filesystem::path invalidSelectionPath = std::filesystem::u8path(
            temporaryDirectory.path().toUtf8().constData()) / "invalid-selection.json";
        QFile invalidSelectionFile(QString::fromStdWString(invalidSelectionPath.wstring()));
        require(invalidSelectionFile.open(QIODevice::WriteOnly),
            "invalid selection fixture could not be written");
        require(invalidSelectionFile.write("{\"schema\":\"wrong\",\"profileId\":\"base-robot\"}") > 0,
            "invalid selection fixture write failed");
        invalidSelectionFile.close();
        require(!RobotQtViewerPlatformProfileIo::loadSelection(
                    invalidSelectionPath, &loadedSelection, &ioError),
            "invalid platform selection was accepted");

        const RobotQtViewerPlatformProfile fallback =
            makeRobotQtViewerBuiltInBaseProfile(catalog);
        resolved = RobotQtViewerPlatformProfileResolver::resolve(catalog, fallback);
        require(resolved.succeeded(), "built-in base profile did not resolve");
        require(resolved.defaultWorkbenchId ==
                robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse),
            "built-in base profile has the wrong default mode");
        require(resolved.enabledWorkbenchIds.size() == catalog.workbenches().size(),
            "built-in base profile did not enable every built mode");
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    testSuccessfulTransitionAndResume();
    testEditSessionSelectionTransition();
    testEditSessionCoordinatorFailureAndReentrancy();
    testReturnToWorkbenchEntrySource();
    testRejectionAndActivationRollback();
    testDeactivateAndRollbackFailures();
    testTargetCleanupFailureAndReentrancy();
    testPreparedTransitionContextAndSharedLifecycle();
    testProjectReleaseShutdownAndMissingLifecycle();
    testPlatformProfileResolutionAndOverlay();
    testRuntimeContributionHost();
    testPlatformProfileDiagnosticsAndOverlayIo();
    std::cout << "Workbench lifecycle tests passed.\n";
    return 0;
}
