#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerEventHub.h"

#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectSession.h>

#include <QCoreApplication>
#include <QObject>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace robot_qt_viewer;
    using namespace simulation_project;

    void require(bool condition, const char* message)
    {
        if(!condition) {
            throw std::runtime_error(message);
        }
        std::cout << "[OK] " << message << '\n';
    }

    bool containsDomain(
        const ProjectChangeSet& changes,
        ProjectChangeDomain domain)
    {
        return std::find(changes.domains.begin(), changes.domains.end(), domain) !=
            changes.domains.end();
    }

    bool containsId(const ProjectChangeSet& changes, const std::string& id)
    {
        return std::find(changes.affectedIds.begin(), changes.affectedIds.end(), id) !=
            changes.affectedIds.end();
    }

    ProjectDocument makeBindingDocument()
    {
        ProjectDocument document;
        document.version = 3;

        RobotDesc robot;
        robot.id = "robot";
        robot.name = "Robot";
        robot.sourceType = "urdf";
        robot.sourcePath = "data/robot.urdf";
        document.robots.push_back(robot);

        RobotMountDesc firstMount;
        firstMount.id = "mount_a";
        firstMount.name = "Mount A";
        firstMount.robotId = robot.id;
        firstMount.linkName = "tool0";
        document.robotMounts.push_back(firstMount);

        RobotMountDesc secondMount = firstMount;
        secondMount.id = "mount_b";
        secondMount.name = "Mount B";
        document.robotMounts.push_back(secondMount);

        SceneObjectDesc object;
        object.id = "object";
        object.name = "Object";
        object.sourcePath = "data/tool.stl";
        ObjectFrameDesc frame;
        frame.id = "object_frame";
        frame.name = "Object Frame";
        frame.objectToFrame.x = 0.1;
        frame.objectToFrame.y = -0.2;
        frame.objectToFrame.z = 0.3;
        object.objectFrames.push_back(frame);
        document.objects.push_back(object);
        return document;
    }

    void testCompatibilityTransactions()
    {
        ProjectSession session;
        RobotQtViewerEventHub eventHub;
        RobotQtViewerDocumentController controller(session, eventHub);
        QObject receiver;
        int documentEventCount = 0;
        int dirtyEventCount = 0;
        RobotQtViewerEvent lastDocumentEvent;
        eventHub.subscribe(
            RobotQtViewerEventKind::ProjectDocumentChanged,
            &receiver,
            [&](const RobotQtViewerEvent& event) {
                ++documentEventCount;
                lastDocumentEvent = event;
            });
        eventHub.subscribe(
            RobotQtViewerEventKind::ProjectDirtyChanged,
            &receiver,
            [&](const RobotQtViewerEvent&) {
                ++dirtyEventCount;
            });

        const ProjectMutationResult committed = controller.mutateProject(
            QStringLiteral("compatibilityCommit"),
            ProjectDirtyPolicy::UserEdit,
            [](ProjectDocumentService& service, bool& changed, std::string&) {
                service.document().view.showGrid = false;
                changed = true;
                return true;
            });
        require(committed.success && committed.changed && committed.hasProjectChange,
            "compatibility mutation commits a candidate");
        require(!session.document().view.showGrid && session.isDirty(),
            "committed candidate becomes live and dirty");
        require(controller.revision() == 1 && committed.projectChange.afterRevision == 1,
            "compatibility commit advances revision once");
        require(documentEventCount == 1 && dirtyEventCount == 1,
            "compatibility commit publishes one document and one dirty event");
        require(lastDocumentEvent.hasProjectChange &&
                lastDocumentEvent.projectChange.afterRevision == 1 &&
                containsDomain(lastDocumentEvent.projectChange, ProjectChangeDomain::Document),
            "document event carries the committed change set");

        const ProjectDocument beforeNoOp = session.document();
        const ProjectMutationResult noOp = controller.mutateProject(
            QStringLiteral("compatibilityNoOp"),
            ProjectDirtyPolicy::UserEdit,
            [](ProjectDocumentService&, bool& changed, std::string&) {
                changed = false;
                return true;
            });
        require(noOp.success && !noOp.changed && controller.revision() == 1,
            "compatibility no-op preserves revision");
        require(session.document().view.showGrid == beforeNoOp.view.showGrid &&
                documentEventCount == 1 && dirtyEventCount == 1,
            "compatibility no-op preserves state and events");

        const ProjectMutationResult failed = controller.mutateProject(
            QStringLiteral("compatibilityFailure"),
            ProjectDirtyPolicy::UserEdit,
            [](ProjectDocumentService& service, bool& changed, std::string& error) {
                service.document().view.showGrid = true;
                changed = true;
                error = "expected failure";
                return false;
            });
        require(!failed.success && !session.document().view.showGrid &&
                controller.revision() == 1 && documentEventCount == 1,
            "failed compatibility mutation is atomic and silent");

        const ProjectMutationResult threw = controller.mutateProject(
            QStringLiteral("compatibilityException"),
            ProjectDirtyPolicy::UserEdit,
            [](ProjectDocumentService& service, bool&, std::string&) -> bool {
                service.document().view.showGrid = true;
                throw std::runtime_error("expected exception");
            });
        require(!threw.success && threw.message.contains(QStringLiteral("expected exception")) &&
                !session.document().view.showGrid && controller.revision() == 1 &&
                documentEventCount == 1,
            "throwing compatibility mutation is atomic and reports the error");

        const ProjectMutationResult preview = controller.mutateProject(
            QStringLiteral("compatibilityPreview"),
            ProjectDirtyPolicy::PreviewOnly,
            [](ProjectDocumentService& service, bool& changed, std::string&) {
                service.document().view.showGrid = true;
                changed = true;
                return true;
            });
        require(preview.success && !preview.changed && !session.document().view.showGrid &&
                controller.revision() == 1 && documentEventCount == 1 && dirtyEventCount == 1,
            "preview validates without committing or publishing events");
    }

    void testTypedBindingTransactions()
    {
        ProjectSession session;
        session.setDocument(makeBindingDocument(), {}, false, false);
        RobotQtViewerEventHub eventHub;
        RobotQtViewerDocumentController controller(session, eventHub);
        QObject receiver;
        std::vector<RobotQtViewerEvent> documentEvents;
        eventHub.subscribe(
            RobotQtViewerEventKind::ProjectDocumentChanged,
            &receiver,
            [&](const RobotQtViewerEvent& event) {
                documentEvents.push_back(event);
            });

        BindFramesRequest bind;
        bind.hostFrame.kind = ProjectFrameKind::RobotMount;
        bind.hostFrame.owner.kind = ProjectEntityKind::RobotMount;
        bind.hostFrame.owner.id = "mount_a";
        bind.hostFrame.frameId = "mount_a";
        bind.boundEntity.kind = ProjectEntityKind::SceneObject;
        bind.boundEntity.id = "object";
        bind.boundFrame.kind = ProjectFrameKind::ObjectFrame;
        bind.boundFrame.owner = bind.boundEntity;
        bind.boundFrame.frameId = "object_frame";
        bind.assetId = "bound_asset";
        bind.attachmentId = "bound_attachment";

        const ProjectBindFramesMutationResult bound =
            controller.executeBindFrames(QStringLiteral("typedBind"), bind);
        require(bound.command.success && bound.transaction.success &&
                bound.transaction.changed && controller.revision() == 1,
            "typed binding commits through the transaction service");
        require(documentEvents.size() == 1 && documentEvents.back().hasProjectChange &&
                containsDomain(documentEvents.back().projectChange, ProjectChangeDomain::Assembly),
            "typed binding publishes one Assembly change set");
        for(const std::string& id : {
                std::string("bound_asset"),
                std::string("bound_attachment"),
                std::string("mount_a"),
                std::string("object")}) {
            require(containsId(documentEvents.back().projectChange, id),
                "typed binding change set contains an affected entity ID");
        }

        RebindFramesRequest rebind;
        rebind.attachmentId = "bound_attachment";
        rebind.hostFrame.kind = ProjectFrameKind::RobotMount;
        rebind.hostFrame.owner.kind = ProjectEntityKind::RobotMount;
        rebind.hostFrame.owner.id = "mount_b";
        rebind.hostFrame.frameId = "mount_b";
        rebind.hostToBoundOffset.x = 0.05;
        const ProjectMutationResult rebound =
            controller.executeRebindFrames(QStringLiteral("typedRebind"), rebind);
        require(rebound.success && rebound.changed && controller.revision() == 2 &&
                rebound.projectChange.beforeRevision == 1 &&
                rebound.projectChange.afterRevision == 2,
            "typed rebind advances the revision monotonically");
        require(documentEvents.size() == 2 &&
                containsDomain(documentEvents.back().projectChange, ProjectChangeDomain::Assembly) &&
                containsId(documentEvents.back().projectChange, "bound_attachment") &&
                containsId(documentEvents.back().projectChange, "mount_b"),
            "typed rebind publishes its Assembly affected IDs");
    }
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    testCompatibilityTransactions();
    testTypedBindingTransactions();
    std::cout << "Document transaction tests passed.\n";
    return 0;
}
