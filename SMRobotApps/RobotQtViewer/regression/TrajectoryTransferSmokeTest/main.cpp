#include <PlanningTrajectoryTransfer.h>
#include <PlanningDataSource.h>
#include <MotionPlanningModuleController.h>
#include <MotionPlanningWorkbenchLifecycle.h>
#include <MotionPlanningEditorWidget.h>
#include <CoatingAnalysisModuleController.h>
#include <CoatingAnalysisPanel.h>
#include <CoatingAnalysisTreePanel.h>
#include <CoatingAnalysisInfoPanel.h>
#include <CoatingAnalysisVisibilityBar.h>
#include <FreeformTrajPlanWorkbenchController.h>
#include <RobotQtViewerDocumentContext.h>
#include <RobotQtViewerDocumentController.h>
#include <RobotQtViewerDocumentViewRegistry.h>
#include <RobotQtViewerEventHub.h>
#include <RobotQtViewerOperationStatus.h>
#include <RobotQtViewerSelectionModel.h>
#include <RobotQtViewerViewportPreviewState.h>
#include <RobotQtViewerViewportServicesAdapter.h>
#include <RobotViewport.h>
#include <ProjectMotionPlanning/TrajectoryControlPointEditing.h>
#include <ProjectMotionPlanning/TrajectoryInverseKinematics.h>
#include <ProjectMotionPlanning/CdfQpTrajectoryRepair.h>
#include <SimulationProject/ProjectDocumentService.h>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QPushButton>
#include <QTableWidget>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSettings>
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>

using namespace robot_qt_viewer;
namespace
{
    int failures = 0;
    void check(bool pass, const char* message)
    {
        std::cout << (pass ? "[OK] " : "[FAIL] ") << message << std::endl;
        if(!pass) ++failures;
    }
    PlanningDataSnapshot sampleSource()
    {
        PlanningDataSnapshot snapshot;
        snapshot.modelObjectId = QStringLiteral("surface");
        snapshot.modelPath = QStringLiteral("surface.stl");
        snapshot.modelName = QStringLiteral("Surface");
        snapshot.sceneScale = 0.001;
        snapshot.trajectory.name = "Surface scan";
        for(int i = 0; i < 4; ++i) {
            spraytrajectory::SpraySegment segment;
            segment.passIndex = i;
            segment.sprayEnabled = i != 1;
            segment.processId = "freeform";
            spraytrajectory::SprayPathPoint p;
            p.time = i; p.tcpPose.translation() = Eigen::Vector3d(i*0.01,0,0.1);
            p.sprayEnabled = true;
            p.pointType = i == 1 ? spraytrajectory::SprayPathPointType::AirMove : spraytrajectory::SprayPathPointType::Regular;
            p.velocity = 0.01; p.acceleration = 0.02; p.targetDistance = 0.1;
            p.targetNormal = Eigen::Vector3d::UnitZ(); p.workpieceRegionId = i;
            segment.points.push_back(p);
            snapshot.trajectory.segments.push_back(segment);
        }
        return snapshot;
    }
    simulation_project::ProjectDocument sampleDocument()
    {
        simulation_project::ProjectDocument document;
        simulation_project::SceneObjectDesc object;
        object.id = "surface"; object.sourcePath = "surface.stl";
        object.transform.x = 1.0; object.transform.y = 2.0;
        object.transform.yaw = 1.5707963267948966;
        object.visualScale = object.collisionScale = 0.001;
        document.objects.push_back(object);
        return document;
    }
    void dataTests()
    {
        auto input = sampleSource(); auto document = sampleDocument();
        PlanningDataSource source;
        int originals = 0, analyses = 0;
        QObject::connect(&source,&PlanningDataSource::snapshotChanged,&source,[&](const auto&) { ++originals; });
        QObject::connect(&source,&PlanningDataSource::analysisSnapshotChanged,&source,[&](const auto&) { ++analyses; });
        source.publish(input); input = source.snapshot();
        PlanningDataSnapshot world; QString error;
        check(planningDataToWorld(input,document,world,&error),"Model-local trajectory converts to world");
        auto points = world.trajectory.flattenedPoints();
        check((points[1].tcpPose.translation()-Eigen::Vector3d(1,2.01,0.1)).norm()<1e-10,
            "Scene rotation/translation applied once; meter data is not scaled twice");
        check(!points[1].sprayEnabled && points[1].pointType == spraytrajectory::SprayPathPointType::AirMove,
            "Segment spray mask and air moves survive transfer");
        motion_planning::StoredMotionPlan plan;
        check(PlanningTrajectoryTransfer::makeMotionPlan(world,"robot",{"joint"},plan,error),"Freeform trajectory becomes a normal stored Cartesian plan");
        std::string storeError;
        check(motion_planning::MotionPlanningProjectStore::upsertPlan(document,plan,&storeError),"Process-linked plan stores in project document");
        auto stored = motion_planning::MotionPlanningProjectStore::plans(document);
        PlanningDataSnapshot decoded;
        check(stored.size()==1 && PlanningTrajectoryTransfer::decode(stored[0].processMetadata,decoded) &&
            decoded.trajectory.flattenedPoints()[1].sprayEnabled==false,"Project round-trip retains process and workpiece metadata");
        std::vector<double> referenceTimes{0,0.5,1,1.5,2,2.5,3};
        for(std::size_t i = 0; i < referenceTimes.size(); ++i) {
            robottrajectory::TimedJointPoint p;
            p.time = i*0.25; p.q = {referenceTimes[i]}; plan.trajectory.points.push_back(p);
        }
        const auto fk = [](const std::vector<double>& q) {
            Eigen::Isometry3d p = Eigen::Isometry3d::Identity();
            p.translation() = Eigen::Vector3d(q[0]*0.02,0,0.2);
            p.linear() = Eigen::AngleAxisd(q[0]*0.1,Eigen::Vector3d::UnitY()).toRotationMatrix();
            return p;
        };
        PlanningDataSnapshot optimized;
        check(PlanningTrajectoryTransfer::makeAnalysisSnapshot(plan,fk,referenceTimes,optimized,error),
            "Dense retimed CDF output converts through actual TCP FK");
        points = optimized.trajectory.flattenedPoints();
        check(points.size()==7 && std::abs(points.back().time-1.5)<1e-10 &&
            (points.back().tcpPose.translation()-Eigen::Vector3d(0.06,0,0.2)).norm()<1e-10,
            "Optimized positions, orientations, timing and joint data replace original geometry");
        check(points[2].sprayEnabled==false && points[3].sprayEnabled==false && points[4].sprayEnabled,
            "Spray boundaries follow original dense knots rather than retimed wall-clock values");
        check(std::abs(points[3].velocity-0.04)<1e-10,"Deposition feed rates reflect final execution timing");
        optimized.producer = QStringLiteral("CDF");
        check(source.publishForAnalysis(optimized) && originals==1 && analyses==2,
            "Optimized analysis channel does not loop back into freeform/Motion Planning");
        source.publish(input);
        check(originals==1 && analyses==2 && source.analysisSnapshot().producer==QStringLiteral("CDF"),
            "Repeated freeform UI refresh preserves edited and optimized data");
        auto stale = optimized;
        input.trajectory.segments[0].points[0].tcpPose.translation().x()+=0.01;
        source.publish(input);
        check(!source.publishForAnalysis(stale),"A stale CDF result cannot replace a newer freeform revision");
        auto edited = stored[0];
        robottrajectory::TimedCartesianPoint inserted = edited.cartesianControlPoints.points[1];
        inserted.time=0.5;
        check(motion_planning::ProjectTrajectoryControlPointEditor::insertCartesianControlPoint(edited,1,
            motion_planning::CartesianControlPointInsertLocation::Before,inserted,&storeError) &&
            PlanningTrajectoryTransfer::updateCartesianProcessData(edited,1),"Inserted control point receives matching process metadata");
        check(motion_planning::ProjectTrajectoryControlPointEditor::removeCartesianControlPoint(edited,1,&storeError) &&
            PlanningTrajectoryTransfer::updateCartesianProcessData(edited,-1,1),"Deleting a point preserves the remaining process boundaries");
        edited.cartesianControlPoints.points[0].tcpPose.translation().z()+=0.1;
        check(PlanningTrajectoryTransfer::updateCartesianProcessData(edited) &&
            PlanningTrajectoryTransfer::decode(edited.processMetadata,decoded) &&
            std::abs(decoded.trajectory.flattenedPoints()[0].tcpPose.translation().z()-0.2)<1e-10,
            "Cartesian edits update the persistent process copy");
        world.trajectory.segments[0].points[0].tcpPose.matrix()(0,0)=std::numeric_limits<double>::quiet_NaN();
        check(!PlanningTrajectoryTransfer::makeMotionPlan(world,"robot",{"joint"},plan,error),"Invalid trajectory is rejected before document mutation");
        check(!PlanningTrajectoryTransfer::makeAnalysisSnapshot(stored[0],{}, {0},optimized,error),"Mismatched process correspondence is rejected");
        source.clear();
        check(!source.snapshot().hasTrajectory() && !source.analysisSnapshot().hasTrajectory(),"Reset clears both transfer channels");
    }

    void writeCube(const std::filesystem::path& path)
    {
        const double v[8][3]={{-50,-50,0},{50,-50,0},{50,50,0},{-50,50,0},
            {-50,-50,100},{50,-50,100},{50,50,100},{-50,50,100}};
        const int triangles[12][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
            {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
        std::ofstream file(path,std::ios::binary);
        const char header[80]={};
        const std::uint32_t count=12;
        const std::uint16_t attributes=0;
        file.write(header,sizeof(header));
        file.write(reinterpret_cast<const char*>(&count),sizeof(count));
        for(const auto& tri:triangles) {
            const Eigen::Vector3d a(v[tri[0]]),b(v[tri[1]]),c(v[tri[2]]);
            const auto normal=(b-a).cross(c-a).normalized();
            float facet[12]={static_cast<float>(normal.x()),static_cast<float>(normal.y()),static_cast<float>(normal.z())};
            for(int j=0;j<3;++j) for(int k=0;k<3;++k) facet[3+j*3+k]=static_cast<float>(v[tri[j]][k]);
            file.write(reinterpret_cast<const char*>(facet),sizeof(facet));
            file.write(reinterpret_cast<const char*>(&attributes),sizeof(attributes));
        }
    }
    struct Workbenches
    {
        QTemporaryDir temporary;
        std::filesystem::path folder;
        RobotQtViewerEventHub hub;
        simulation_project::ProjectSession session;
        RobotQtViewerDocumentController documents{session,hub};
        RobotQtViewerSelectionModel selection{hub};
        RobotQtViewerViewportPreviewState preview{hub};
        RobotQtViewerOperationStatusStore status{hub};
        RobotQtViewerDocumentContext context{session,documents,selection,preview,hub,status};
        RobotViewport viewport;
        RobotQtViewerViewportProjectState projectState;
        RobotQtViewerMotionPlanningViewportAdapter services{viewport};
        RobotQtViewerDocumentViewportAdapter documentViewport{viewport,projectState};
        RobotQtViewerSelectionViewportAdapter selectionViewport{viewport};
        RobotQtViewerAssemblyViewportAdapter assemblyViewport{viewport,projectState};
        RobotQtViewerCollisionViewportAdapter collisionViewport{viewport,projectState};
        RobotQtViewerVisualizationViewportAdapter visualizationViewport{viewport};
        PlanningDataSource source;
        MotionPlanningEditorWidget motion;
        CoatingAnalysisPanel panel;
        CoatingAnalysisTreePanel tree;
        CoatingAnalysisInfoPanel info;
        CoatingAnalysisVisibilityBar visibility;
        std::unique_ptr<MotionPlanningModuleController> planning;
        std::unique_ptr<CoatingAnalysisModuleController> coating;
        bool hasCoatingResult=false;
        QString coatingStatus;
        Workbenches(bool savedJoints = true, bool createPlanning = true, bool sixAxis = false)
        {
            folder=std::filesystem::path(temporary.path().toStdWString());
            writeCube(folder/"cube.stl");
            std::ofstream(folder/"robot.urdf") << R"(<robot name="fixture">
<link name="base"/>
<link name="Link6"><visual><geometry><sphere radius="0.01"/></geometry></visual>
<collision><geometry><sphere radius="0.01"/></geometry></collision></link>
<joint name="slide" type="prismatic"><parent link="base"/><child link="Link6"/>
<axis xyz="1 0 0"/><limit lower="-5" upper="5" effort="100" velocity="1"/></joint></robot>)";
            simulation_project::ProjectDocument document;
            simulation_project::RobotDesc robot;
            robot.id="fixture_robot"; robot.name="Fixture"; robot.sourceType="urdf";
            robot.sourcePath=(folder/"robot.urdf").generic_u8string();
            if(savedJoints) robot.initialJoints.push_back({"slide",0.0});
            if(sixAxis) {
                std::ofstream out(folder/"robot.urdf");
                out<<"<robot name='six-axis'><link name='base'/>";
                const char* axes[]={"0 0 1","0 1 0","0 1 0","1 0 0","0 1 0","1 0 0"};
                const char* offsets[]={"0 0 0.4","0.1 0 0.1","0.4 0 0","0.3 0 0","0.1 0 0","0.1 0 0"};
                robot.initialJoints.clear();
                const double seed[]={.2,-.4,.5,.2,.6,-.2};
                for(int j=0;j<6;++j) {
                    const auto child=j==5?std::string("Link6"):"l"+std::to_string(j);
                    out<<"<link name='"<<child<<"'><visual><geometry><sphere radius='0.01'/></geometry></visual>"
                        <<"<collision><geometry><sphere radius='0.01'/></geometry></collision></link>"
                        <<"<joint name='j"<<j<<"' type='revolute'><parent link='"<<(j==0?"base":"l"+std::to_string(j-1))
                        <<"'/><child link='"<<child<<"'/><origin xyz='"<<offsets[j]<<"'/><axis xyz='"<<axes[j]
                        <<"'/><limit lower='-3.14' upper='3.14' effort='100' velocity='2'/></joint>";
                    robot.initialJoints.push_back({"j"+std::to_string(j),seed[j]});
                }
                out<<"</robot>";out.close();
                simulation_project::RobotMountDesc mount;mount.id="test_mount";mount.robotId=robot.id;mount.linkName="Link6";
                document.robotMounts.push_back(mount);
                simulation_project::AttachmentAssetDesc asset;asset.id="test_tool";asset.visualPath=(folder/"cube.stl").generic_u8string();asset.visualScale=.001;asset.visible=false;
                simulation_project::AttachmentFunctionalFrameDesc tcp;tcp.id="tcp";tcp.frameType="tcp";tcp.primary=true;
                asset.functionalFrames.push_back(tcp);document.attachmentAssets.push_back(asset);
                simulation_project::MountedAttachmentDesc attachment;attachment.id="test_attachment";attachment.mountFrameId=mount.id;attachment.assetId=asset.id;attachment.visible=false;
                document.mountedAttachments.push_back(attachment);
            }
            document.robots.push_back(robot);
            simulation_project::SceneObjectDesc object;
            object.id="burnner"; object.name="Demo workpiece"; object.sourcePath=(folder/"cube.stl").generic_u8string();
            object.visualScale=object.collisionScale=0.001; object.transform.x=2.0;
            document.objects.push_back(object);
            session.setDocument(document,folder/"test.scene.json",false,false);
            context.setMotionPlanningViewport(&services);
            context.setDocumentViewport(&documentViewport); context.setSelectionViewport(&selectionViewport);
            context.setAssemblyViewport(&assemblyViewport); context.setVisualizationViewport(&visualizationViewport);
            context.setCollisionViewport(&collisionViewport);
            if(createPlanning) {
                selection.selectRobotLink(QStringLiteral("fixture_robot"),QStringLiteral("Link6"));
                planning=std::make_unique<MotionPlanningModuleController>(motion,context);
                planning->setPlanningDataSource(&source);
            }
            coating=std::make_unique<CoatingAnalysisModuleController>(panel,tree,info,visibility,context);
            QObject::connect(coating.get(),&CoatingAnalysisModuleController::statusMessageRequested,&panel,
                [this](const QString& message,int){coatingStatus=message;});
            hub.subscribe(RobotQtViewerEventKind::CoatingAnalysisChanged,&panel,[this](const auto& e) {
                hasCoatingResult=e.coatingAnalysis.hasResult &&
                    std::isfinite(e.coatingAnalysis.minimumThicknessMeters) &&
                    std::isfinite(e.coatingAnalysis.maximumThicknessMeters);
            });
            QObject::connect(&source,&PlanningDataSource::analysisSnapshotChanged,coating.get(),
                [this](const auto& snapshot){coating->synchronizePlanningData(snapshot);});
            for(const auto kind:{RobotQtViewerEventKind::SelectionChanged,RobotQtViewerEventKind::ProjectDocumentChanged,
                RobotQtViewerEventKind::ProjectOpened}) {
                if(planning) hub.subscribe(kind,planning.get(),[this](const auto& e){planning->handleEvent(e);});
                hub.subscribe(kind,coating.get(),[this](const auto& e){coating->handleEvent(e);});
            }
            check(documentViewport.loadProjectDocument(context.document(),folder).success,"Actual robot/workpiece viewport initializes");
            viewport.resize(640,480); viewport.show(); QApplication::processEvents();
        }
        bool predictReceivedTrajectory()
        {
            coating->activate();
            hasCoatingResult=false;
            panel.predictionRequested();
            QElapsedTimer timer; timer.start();
            while(!hasCoatingResult && timer.elapsed()<30000) {
                QApplication::processEvents(); QThread::msleep(5);
            }
            std::cout << "Coating prediction: " << coatingStatus.toStdString() << std::endl;
            return hasCoatingResult;
        }
    };
    void receivingTests()
    {
        // Use the application's real factory and event subscriptions, with the
        // optional initialJoints omitted as in 420-red4600.sys.json.
        for(int scenario = 0; scenario < 3; ++scenario) {
            Workbenches fixture(false, false);
            const auto robot = fixture.context.document().robots.front();
            if(scenario == 2) {
                check(fixture.documents.mutateProject(QStringLiteral("testRemoveRobot"),ProjectDirtyPolicy::UserEdit,
                    [](simulation_project::ProjectDocumentService& service,bool& changed,std::string&) {
                        service.document().robots.clear(); changed=true; return true;
                    }).success,"Robot-free scene is ready for deferred transfer");
            }
            const auto config = defaultRobotQtViewerWindowConfig();
            RobotQtViewerDocumentViewRegistry registry(fixture.hub,config);
            QWidget parent;
            MotionPlanningWorkbenchComposition composition;
            composition.documentContext=&fixture.context;
            composition.documentViewRegistry=&registry;
            composition.dataSource=&fixture.source;
            const auto factory=makeOwnedMotionPlanningWorkbenchRuntimeContributionFactory(composition);
            std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution> runtime;
            if(scenario != 1) runtime=factory.factory(&parent);
            auto input=sampleSource(); input.modelObjectId=QStringLiteral("burnner");
            input.modelPath=QString::fromStdWString((fixture.folder/"cube.stl").wstring());
            fixture.source.publish(input);
            if(scenario == 1) runtime=factory.factory(&parent);
            if(scenario == 2) {
                check(motion_planning::MotionPlanningProjectStore::plans(fixture.context.document()).empty(),
                    "Trajectory waits while no robot exists");
                check(fixture.documents.mutateProject(QStringLiteral("testAddRobot"),ProjectDirtyPolicy::UserEdit,
                    [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string&) {
                        service.document().robots.push_back(robot); changed=true; return true;
                    }).success,"Robot can be added after freeform generation");
                QApplication::processEvents();
            }
            auto plans=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document());
            check(plans.size()==1 && plans.front().robotId==robot.id &&
                plans.front().cartesianControlPoints.points.size()==4,
                scenario==0 ? "Robot with no saved joint values receives shared trajectory" :
                scenario==1 ? "Opening Motion Planning after generation receives the existing snapshot" :
                              "Adding a robot automatically consumes the waiting trajectory");
            auto* widget=runtime ? runtime->resolvePanel()->findChild<MotionPlanningEditorWidget*>() : nullptr;
            auto* points=widget ? widget->findChild<QTableWidget*>(QStringLiteral("cartesianControlPoints")) : nullptr;
            check(points && points->rowCount()==4,"Received control points are visible in the Motion Planning panel");
            // Repeating publication and robot selection must preserve user edits.
            if(!plans.empty()) {
                const auto id=plans.front().id;
                check(fixture.documents.mutateProject(QStringLiteral("testEditReceivedPlan"),ProjectDirtyPolicy::UserEdit,
                    [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string& error) {
                        auto edited=plans.front(); edited.name="User edited path";
                        changed=motion_planning::MotionPlanningProjectStore::upsertPlan(service.document(),edited,&error);
                        return changed;
                    }).success,"Received trajectory remains editable");
                fixture.source.publish(input);
                fixture.selection.selectRobotLink(QString::fromStdString(robot.id),QStringLiteral("Link6"));
                QApplication::processEvents();
                plans=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document());
                check(plans.size()==1 && plans.front().id==id && plans.front().name=="User edited path",
                    "Repeated selection/publication preserves the received plan and user edits");
            }
        }
    }
    void workbenchTests()
    {
        Workbenches fixture;
        using namespace smrobot::workbench::freeform;
        FreeformTrajPlanWorkflowPanel workflow;
        FreeformTrajPlanParameterPanel parameters;
        FreeformTrajPlanWorkbenchController freeform(workflow,parameters,fixture.context,
            &fixture.source,{},{});
        QTimer unitDialogTimer;
        QObject::connect(&unitDialogTimer,&QTimer::timeout,&workflow,[&] {
            auto* units=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(units && units->defaultButton()) {
                std::cout << "Accepting STL millimeter units" << std::endl;
                unitDialogTimer.stop(); units->defaultButton()->click();
            }
        });
        unitDialogTimer.start(50);
        workflow.importModelRequested(QString::fromStdWString((fixture.folder/"cube.stl").wstring()));
        check(fixture.source.snapshot().hasModel(),"Real freeform controller imports a model into the shared document");
        parameters.parametersChanged(20,1,5,0,0,30,50,2,12);
        workflow.slicingMethodRequested(false);
        workflow.workflowActionRequested(spray_freeform_traj_plan::WorkflowAction::StartSlicing);
        workflow.workflowActionRequested(spray_freeform_traj_plan::WorkflowAction::GenerateTrajectory);
        const auto snapshot=fixture.source.snapshot();
        const auto sourceCount=snapshot.trajectory.flattenedPoints().size();
        check(sourceCount>2,"Real freeform slicing generates a shared TCP trajectory");
        if(sourceCount<2) return;
        const auto plans=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document());
        const auto found=std::find_if(plans.begin(),plans.end(),[&](const auto& p) {
            return p.cartesianControlPoints.points.size()==sourceCount && !p.processMetadata.empty();
        });
        check(found!=plans.end(),"Generated freeform data automatically enters Motion Planning without file dialogs");
        check(fixture.coating->waypoints().size()==sourceCount,"Generated freeform data automatically enters coating waypoint operations");
        if(found==plans.end()) return;
        fixture.motion.sendTrajectoryToCoatingRequested();
        check(fixture.source.analysisSnapshot().producer==QStringLiteral("Motion Planning"),
            "Existing Motion Planning Cartesian path can be sent directly to coating");
        const auto before=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document()).size();
        workflow.planningMethodRequested(false);
        check(motion_planning::MotionPlanningProjectStore::plans(fixture.context.document()).size()==before &&
            fixture.source.analysisSnapshot().producer==QStringLiteral("Motion Planning"),
            "Re-entering the same freeform method does not overwrite downstream work");
        fixture.coating->activate(); fixture.coating->deactivate(); fixture.coating->activate();
        check(fixture.coating->waypoints().size()==sourceCount,"Switching analysis workbenches retains the received path");
        check(fixture.predictReceivedTrajectory(),"Transferred freeform trajectory completes actual coating prediction");
        fixture.viewport.grab().save(QStringLiteral("trajectory-transfer-preview.png"));
        fixture.source.clear();
        check(fixture.coating->waypoints().empty(),"Clearing shared source invalidates dependent analysis input");
    }

    void freeformTransitionTests()
    {
        QTemporaryDir settingsDirectory;
        QCoreApplication::setOrganizationName(QStringLiteral("RS2026Regression"));
        QCoreApplication::setApplicationName(QStringLiteral("FreeformTransitionControls"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());
        Workbenches fixture(false,false);
        using namespace smrobot::workbench::freeform;
        const auto configure=spray_freeform_traj_plan::WorkflowAction::ConfigureTrajectoryFrame;
        {
            FreeformTrajPlanWorkflowPanel workflow;
            FreeformTrajPlanParameterPanel parameters;
            FreeformTrajPlanWorkbenchController controller(workflow,parameters,fixture.context,&fixture.source,{},{});
            parameters.parametersChanged(20,1,5,5,5,100,50,2,12);
            const auto open=[&]() {
                workflow.workflowActionRequested(configure);
                QApplication::processEvents();
                return workflow.findChild<QDialog*>(QStringLiteral("trajectoryFrameDialog"));
            };
            auto* dialog=open();
            check(dialog!=nullptr,"Actual freeform frame configuration opens");
            if(!dialog)return;
            const auto combo=[&](const char* id){return dialog->findChild<QComboBox*>(QString::fromLatin1(id));};
            const auto flag=[&](const char* id){return dialog->findChild<QCheckBox*>(QString::fromLatin1(id));};
            const auto number=[&](const char* id){return dialog->findChild<QDoubleSpinBox*>(QString::fromLatin1(id));};
            auto* upperPose=combo("upperTransitionPose");auto* lowerPose=combo("lowerTransitionPose");
            auto* upperOffset=combo("upperTransitionOffset");auto* lowerOffset=combo("lowerTransitionOffset");
            auto* upperNormal=flag("upperTransitionNormalOffset");auto* lowerNormal=flag("lowerTransitionNormalOffset");
            auto* upperFollow=flag("upperTransitionFollowSprayDistance");auto* lowerFollow=flag("lowerTransitionFollowSprayDistance");
            auto* upperDistance=number("upperTransitionSprayDistance");auto* lowerDistance=number("lowerTransitionSprayDistance");
            const bool controls=upperPose&&lowerPose&&upperOffset&&lowerOffset&&upperNormal&&lowerNormal&&upperFollow&&lowerFollow&&upperDistance&&lowerDistance;
            check(controls,"Independent upper/lower transition controls are present");
            if(!controls)return;
            check(upperNormal->isChecked()&&lowerNormal->isChecked()&&upperFollow->isChecked()&&lowerFollow->isChecked()
                && upperDistance->value()==100&&lowerDistance->value()==100,"Default transition distances follow the 100 mm global input");
            upperPose->setCurrentIndex(1);lowerPose->setCurrentIndex(0);
            upperOffset->setCurrentIndex(0);lowerOffset->setCurrentIndex(1);
            upperDistance->setValue(80.5);lowerDistance->setValue(135);
            check(!upperFollow->isChecked()&&!lowerFollow->isChecked(),"Manual distance edits independently disable global following");
            lowerNormal->setChecked(false);
            check(!lowerDistance->isEnabled()&&!lowerFollow->isEnabled()&&lowerDistance->value()==135,
                "Disabling normal offset preserves its custom distance");
            dialog->accept();QApplication::processEvents();
            QSettings saved;
            check(saved.value("freeform/upperTransitionSmoothPose").toBool()&&!saved.value("freeform/upperTransitionOppositeOffset").toBool()
                && !saved.value("freeform/lowerTransitionSmoothPose").toBool()&&saved.value("freeform/lowerTransitionOppositeOffset").toBool(),
                "Opposite travel direction and smooth pose persist independently");
            check(saved.value("freeform/upperTransitionSprayDistance").toDouble()==80.5
                &&saved.value("freeform/lowerTransitionSprayDistance").toDouble()==135
                &&!saved.value("freeform/lowerTransitionNormalOffset").toBool(),"Custom upper/lower distances and disabled state persist");
            dialog=open();
            auto* retained=dialog->findChild<QDoubleSpinBox*>(QStringLiteral("upperTransitionSprayDistance"));
            check(retained&&retained->value()==80.5,"Reopening restores the custom distance");
            if(retained)retained->setValue(90);
            dialog->reject();QApplication::processEvents();
            check(QSettings().value("freeform/upperTransitionSprayDistance").toDouble()==80.5,"Cancel leaves saved transition settings unchanged");
            dialog=open();
            dialog->findChild<QCheckBox*>(QStringLiteral("lowerTransitionNormalOffset"))->setChecked(true);
            dialog->findChild<QCheckBox*>(QStringLiteral("lowerTransitionFollowSprayDistance"))->setChecked(true);
            check(dialog->findChild<QDoubleSpinBox*>(QStringLiteral("lowerTransitionSprayDistance"))->value()==100,
                "Re-enabling follow resets to the current global distance");
            dialog->accept();QApplication::processEvents();
        }
        {
            FreeformTrajPlanWorkflowPanel workflow;
            FreeformTrajPlanParameterPanel parameters;
            FreeformTrajPlanWorkbenchController restored(workflow,parameters,fixture.context,&fixture.source,{},{});
            parameters.parametersChanged(20,1,5,5,5,120,50,2,12);
            workflow.workflowActionRequested(configure);QApplication::processEvents();
            auto* dialog=workflow.findChild<QDialog*>(QStringLiteral("trajectoryFrameDialog"));
            check(dialog!=nullptr,"Recreated controller restores transition preferences");
            if(!dialog)return;
            check(dialog->findChild<QDoubleSpinBox*>(QStringLiteral("upperTransitionSprayDistance"))->value()==80.5
                &&dialog->findChild<QDoubleSpinBox*>(QStringLiteral("lowerTransitionSprayDistance"))->value()==120,
                "Custom mode survives restart while follow mode uses the new global distance");
            dialog->reject();QApplication::processEvents();
        }
    }

    void cdfTests()
    {
        Workbenches fixture(true,true,true);
        auto* sprayDistance=fixture.motion.findChild<QDoubleSpinBox*>(QStringLiteral("gaSprayDistance"));
        check(sprayDistance && std::abs(sprayDistance->value()-100.)<1e-12,"GA spray distance starts at 100 millimetres");
        if(sprayDistance) sprayDistance->setValue(125.);
        check(std::abs(fixture.motion.cdfQpRepairSettings().gaSprayDistance-.125)<1e-12,
            "User-entered spray distance converts millimetres to domain metres");
        if(sprayDistance) sprayDistance->setValue(100.);
        check(std::abs(fixture.motion.cdfQpRepairSettings().gaSprayDistance-.1)<1e-12,
            "Full UI GA/APF/CDF workflow uses the requested 100 millimetre distance");
        // An explicit source workpiece must win over the legacy burnner target.
        const auto mutation=fixture.documents.mutateProject(QStringLiteral("testSource"),ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string&) {
                auto object=service.document().objects.front(); object.id="surface"; object.name="Actual source";
                object.transform.x=3.0; service.document().objects.push_back(object); changed=true; return true;
            });
        check(mutation.success,"A separate freeform source workpiece is available alongside burnner");
        check(fixture.documentViewport.loadProjectDocument(fixture.context.document(),fixture.folder).success,"Source workpiece runtime loads");
        const auto fk=fixture.services.robotForwardKinematics(QStringLiteral("fixture_robot"),{"j0","j1","j2","j3","j4","j5"},true);
        check(static_cast<bool>(fk),"Actual TCP FK is available for optimized transfer");
        if(!fk) return;
        auto input=sampleSource(); input.worldCoordinates=true;
        input.modelPath=QString::fromStdWString((fixture.folder/"cube.stl").wstring());
        for(std::size_t i=0;i<input.trajectory.segments.size();++i) {
            auto& p=input.trajectory.segments[i].points[0]; p.tcpPose=fk({.2+i*.01,-.4,.5,.2,.6,-.2});
        }
        fixture.source.publish(input);
        auto plans=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document());
        auto plan=plans.back();
        for(int i=0;i<4;++i) {robottrajectory::TimedJointPoint p;p.time=i;p.q={.2+i*.01,-.4,.5,.2,.6,-.2};plan.trajectory.points.push_back(p);}
        check(fixture.documents.mutateProject(QStringLiteral("testSolvedIk"),ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string& error) {
                changed=motion_planning::MotionPlanningProjectStore::upsertPlan(service.document(),plan,&error);return changed;
            }).success,"Solved source-linked joint path is stored using existing planning APIs");
        fixture.motion.useCurrentTrajectoryForCdfRequested();
        auto* initial=fixture.motion.findChild<QTableWidget*>(QStringLiteral("cdfInitialJointAngles"));
        check(initial && initial->rowCount()==4,"Current joint trajectory can seed CDF without exporting/re-importing a file");
        auto* optional=fixture.motion.findChild<QCheckBox*>(QStringLiteral("cdfOptionalOptimization"));
        check(optional && !optional->isChecked() && !fixture.motion.cdfQpRepairSettings().useCdf,"CDF/QP starts disabled in both the UI and settings");
        if(optional)optional->setChecked(true);
        check(fixture.motion.cdfQpRepairSettings().useCdf,"Optional CDF/QP can be explicitly enabled");
        fixture.motion.repairImportedCdfTrajectoryRequested();
        QApplication::processEvents();
        auto* stages=fixture.motion.findChild<QComboBox*>(QStringLiteral("cdfStageSelection"));
        check(stages && stages->findText(QStringLiteral("CDF accepted"))>=0,
            "CDF output and Cartesian reference times survive exact replay after retiming");
        const auto output=fixture.source.analysisSnapshot();
        check(output.producer==QStringLiteral("CDF"),"Successful actual CDF execution automatically publishes to coating");
        check(output.modelObjectId==QStringLiteral("surface"),"CDF output remains bound to the actual source workpiece");
        check(output.trajectory.flattenedPoints().size()==fixture.coating->waypoints().size() && output.hasTrajectory(),
            "Coating receives every optimized point");
        const auto detector=std::find_if(fixture.context.document().collision.detectors.begin(),
            fixture.context.document().collision.detectors.end(),[](const auto& d){return d.id=="cdf_shared_fixture_robot_surface";});
        check(detector!=fixture.context.document().collision.detectors.end() &&
            std::any_of(detector->pairGenerators.begin(),detector->pairGenerators.end(),[](const auto& p) {
                return p.type=="RobotObject" && p.objectId=="surface";
            }),"CDF collision setup targets the source object rather than the demo obstacle");
        auto outputs=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document());
        const auto optimized=std::find_if(outputs.begin(),outputs.end(),[](const auto& p){return p.id.find("_cdf_qp_")!=std::string::npos;});
        PlanningDataSnapshot restored;
        check(optimized!=outputs.end() && PlanningTrajectoryTransfer::decode(optimized->processMetadata,restored) &&
            restored.producer==QStringLiteral("CDF"),"Optimized workpiece/process association survives project storage");
        bool poses=true; for(const auto& p:output.trajectory.flattenedPoints())
            poses=poses && p.jointValues.size()==6 && (p.tcpPose.matrix()-fk(p.jointValues).matrix()).norm()<1e-9;
        check(poses,"Coating receives actual optimized robot TCP poses rather than the original freeform poses");
        check(fixture.predictReceivedTrajectory(),"CDF output completes actual coating prediction without file import");
        const auto saved=output.trajectory.flattenedPoints();
        fixture.source.publish(input);
        check(fixture.source.analysisSnapshot().producer==QStringLiteral("CDF") &&
            fixture.coating->waypoints().size()==saved.size(),"A freeform no-op cannot revert the coating input after CDF");
        input.trajectory.segments.front().points.front().tcpPose.translation().z()+=0.001;
        fixture.source.publish(input);
        check(initial && initial->rowCount()==1 && initial->columnSpan(0,0)>1,
            "A new freeform revision invalidates its previous CDF seed");
    }
}
int main(int argc,char** argv)
{
    QSurfaceFormat format; format.setVersion(4,3); format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication application(argc,argv);
    const std::string mode=argc>1?argv[1]:"--data";
    if(mode=="--data") dataTests();
    else if(mode=="--workbenches") workbenchTests();
    else if(mode=="--freeform-transitions") freeformTransitionTests();
    else if(mode=="--cdf") cdfTests();
    else if(mode=="--receiving") receivingTests();
    else return 2;
    std::cout << "Failures: " << failures << std::endl;
    return failures?1:0;
}
