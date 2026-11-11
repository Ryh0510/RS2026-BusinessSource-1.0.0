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
#include <SectionSweepWorkbench/SectionSweepAdapters.h>
#include <SectionSweepWorkbench/SectionSweepParameterPanel.h>
#include <SlicingTrajPlan/STLProcessor.h>
#include <QStackedWidget>
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
#include <ProjectMotionPlanning/XyzrKinematics.h>
#include <ProjectMotionPlanning/TrajectoryImport.h>
#include <ProjectMotionPlanning/CdfQpTrajectoryRepair.h>
#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectIO.h>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QListWidget>
#include <ProjectMotionPlanning/ProjectMotionPlanning.h>
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

    void chooseCollisionConditions(Workbenches& fixture, bool create, const QString& targetId,
        const QString& detectorId = {}, bool cancel = false)
    {
        QTimer::singleShot(0, &fixture.motion, [&]() {
            auto* dialog = fixture.motion.findChild<QDialog*>(QStringLiteral("motionPlanningCollisionDialog"));
            check(dialog != nullptr, "Collision prerequisites are presented in a modal task dialog");
            if(!dialog) return;
            if(cancel) { dialog->reject(); return; }
            auto* target = dialog->findChild<QComboBox*>(QStringLiteral("planningCollisionTarget"));
            auto* list = dialog->findChild<QListWidget*>(QStringLiteral("planningCollisionDetectors"));
            dialog->findChild<QCheckBox*>(QStringLiteral("createPlanningCollisionDetector"))->setChecked(create);
            dialog->findChild<QCheckBox*>(QStringLiteral("enablePlanningCollisionQueries"))->setChecked(true);
            target->setCurrentIndex(target->findData(targetId));
            if(!create) {
                list->clearSelection();
                for(int i = 0; i < list->count(); ++i)
                    list->item(i)->setSelected(list->item(i)->data(Qt::UserRole).toString() == detectorId);
            }
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            if(dialog->isVisible()) { check(false, "Test collision selection is accepted"); dialog->reject(); }
        });
        fixture.motion.collisionConditionsRequested();
    }

    void collisionConditionTests()
    {
        using namespace motion_planning;
        Workbenches fixture;
        check(fixture.context.document().collision.detectors.empty(), "Opening Motion Planning creates no implicit detector");
        const bool queries = fixture.context.document().collision.query.enabled;
        chooseCollisionConditions(fixture, false, {}, {}, true);
        check(fixture.context.document().collision.detectors.empty() &&
            fixture.context.document().collision.query.enabled == queries, "Cancelling prerequisites leaves the scene unchanged");
        check(fixture.documents.mutateProject(QStringLiteral("customWorkpiece"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string&) {
                auto& object = service.document().objects.front();
                object.id = "custom_panel"; object.name = "Custom panel"; object.transform.x = 0; object.transform.z = .5;
                changed = true; return true;
            }).success, "The scene uses an arbitrary workpiece ID without A or burnner");
        chooseCollisionConditions(fixture, true, QStringLiteral("custom_panel"));
        check(fixture.context.document().collision.detectors.size() == 1, "Explicit creation stores one detector through document mutation");
        if(fixture.context.document().collision.detectors.empty()) return;
        const auto created = fixture.context.document().collision.detectors.front();
        check(created.pairGenerators.size() == 1 && created.pairGenerators.front().objectId == "custom_panel" &&
            created.pairGenerators.front().robotId == "fixture_robot", "Created detector binds the actual robot and user-selected workpiece");
        chooseCollisionConditions(fixture, false, QStringLiteral("custom_panel"), QString::fromStdString(created.id));
        check(fixture.context.document().collision.detectors.size() == 1 &&
            fixture.context.document().collision.detectors.front().id == created.id, "Selecting an existing detector does not rewrite or duplicate it");
        check(fixture.documentViewport.loadProjectDocument(fixture.context.document(), fixture.folder).success,
            "Arbitrary scene reloads after detector creation");
        const auto measurement = fixture.services.sprayMeasurement(QStringLiteral("fixture_robot"), QStringLiteral("custom_panel"));
        check(measurement.valid && measurement.distanceMeters > 0, "Viewport adapter measures the explicit target without the legacy A target");
        check(!fixture.services.sprayMeasurement(QStringLiteral("fixture_robot"), QStringLiteral("A")).valid,
            "A missing target is not silently substituted");
        ProjectCdfQpRepairOptions options; options.detectorIds = {created.id};
        auto document = fixture.context.document();
        check(ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(document, "fixture_robot", options),
            "The generic domain precondition accepts selected robot detectors");
        document.collision.detectors.front().nearestPoints = false;
        check(!ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document, "fixture_robot", options),
            "Explicit selection rejects missing nearest-point capability without rewriting the detector");
        document = fixture.context.document(); document.collision.query.enabled = false;
        check(!ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(document, "fixture_robot", options),
            "Global collision disable invalidates the run prerequisite");
        check(!ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(fixture.context.document(), "another_robot", options),
            "A detector selected for a different robot cannot be reused");
        ProjectPlanningRequest query; query.robotId = "fixture_robot"; query.jointNames = {"slide"};
        query.collisionDetectorIds = {created.id};
        std::string error;
        auto scene = ProjectPlanningSceneBuilder::build(fixture.context.document(), fixture.folder, query, &error);
        check(scene != nullptr, "The selected detector builds an independent planning snapshot");
        if(scene) {
            const auto distance = scene->nearestDistance({0.});
            check(distance.valid && distance.distanceMeters > 0 && distance.detectorId == created.id,
                "Nearest distance is computed against exactly the selected detector");
        }
        document = fixture.context.document();
        document.collision.detectors.front().pairFilters.push_back({});
        document.collision.detectors.front().pairFilters.back().type = "ExcludeAll";
        document.collision.detectors.front().pairGenerators.clear();
        document.collision.detectors.front().targets.clear();
        check(!ProjectPlanningSceneBuilder::build(document, fixture.folder, query, &error),
            "A detector with no effective pairs cannot pass planning preflight");
        StoredMotionPlan plan; plan.id = "custom_path"; plan.robotId = "fixture_robot"; plan.jointNames = {"slide"};
        plan.trajectory.points = {{0., {0.}}, {1., {.01}}};
        check(fixture.documents.mutateProject(QStringLiteral("testPlan"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& message) {
                changed = MotionPlanningProjectStore::upsertPlan(service.document(), plan, &message); return changed;
            }).success, "A solved generic trajectory is available for the distance entry point");
        fixture.motion.trajectorySelectionChanged(QStringLiteral("custom_path"));
        fixture.motion.plotNearestDistancesRequested();
        auto* plot = fixture.motion.findChild<QDialog*>(QStringLiteral("nearestDistancePlot"));
        bool selectedOnly = false;
        if(plot) {
            for(auto* label : plot->findChildren<QLabel*>())
                selectedOnly |= label->text().contains(QString::fromStdString(created.id));
            plot->close();
        }
        check(plot && selectedOnly, "The real nearest-distance action plots the selected detector for the generic trajectory");
        fixture.motion.playbackRequested(.03);
        QElapsedTimer playback; playback.start();
        auto* plotSpray = fixture.motion.findChild<QPushButton*>(QStringLiteral("plotSprayMeasurements"));
        while(plotSpray && !plotSpray->isEnabled() && playback.elapsed() < 10000) {
            QApplication::processEvents(); QThread::msleep(5);
        }
        check(plotSpray && plotSpray->isEnabled(), "Playback records spray measurements using the configured generic target");
        fixture.motion.plotSprayMeasurementsRequested();
        QDialog* sprayPlot = nullptr;
        for(auto* dialog : fixture.motion.findChildren<QDialog*>())
            if(dialog->windowTitle().contains(QStringLiteral("custom_panel"))) sprayPlot = dialog;
        check(sprayPlot != nullptr,
            "The spray plot identifies the chosen workpiece instead of A.STL");
        if(sprayPlot) sprayPlot->close();
        check(fixture.documents.mutateProject(QStringLiteral("removeDetector"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string&) {
                service.document().collision.detectors.clear(); changed = true; return true;
            }).success, "The selected scene detector can be removed");
        bool asked = false;
        QTimer::singleShot(0, &fixture.motion, [&]() {
            auto* dialog = fixture.motion.findChild<QDialog*>(QStringLiteral("motionPlanningCollisionDialog"));
            asked = dialog != nullptr; if(dialog) dialog->reject();
        });
        fixture.motion.plotNearestDistancesRequested();
        check(asked && fixture.context.document().collision.detectors.empty(),
            "Distance query requests new prerequisites after deletion and cancellation creates nothing");
        std::ofstream(fixture.folder / "alternate.urdf") << R"(<robot name="alternate">
<link name="root"/><link name="tip"><collision><geometry><sphere radius="0.02"/></geometry></collision></link>
<joint name="travel" type="prismatic"><parent link="root"/><child link="tip"/>
<axis xyz="0 1 0"/><limit lower="-2" upper="2" effort="100" velocity="1"/></joint></robot>)";
        check(fixture.documents.mutateProject(QStringLiteral("alternateRobot"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string&) {
                auto robot = service.document().robots.front(); robot.id = "alternate_robot";
                robot.sourcePath = (fixture.folder / "alternate.urdf").generic_u8string();
                robot.initialJoints = {{"travel", 0.}}; service.document().robots.push_back(robot);
                changed = true; return true;
            }).success, "A second robot with different model and joint names is present");
        fixture.selection.selectRobotLink(QStringLiteral("alternate_robot"), QStringLiteral("tip"));
        chooseCollisionConditions(fixture, true, QStringLiteral("custom_panel"));
        const auto& alternate = fixture.context.document().collision.detectors.back();
        check(alternate.pairGenerators.front().robotId == "alternate_robot", "After switching robots creation binds the new robot");
        query.robotId = "alternate_robot"; query.jointNames = {"travel"}; query.collisionDetectorIds = {alternate.id};
        scene = ProjectPlanningSceneBuilder::build(fixture.context.document(), fixture.folder, query, &error);
        check(scene && scene->nearestDistance({0.}).valid, "The replacement robot uses its actual model in the distance snapshot");
        fixture.selection.selectRobotLink(QStringLiteral("fixture_robot"), QStringLiteral("Link6"));
        bool filtered = false;
        QTimer::singleShot(0, &fixture.motion, [&]() {
            auto* dialog = fixture.motion.findChild<QDialog*>(QStringLiteral("motionPlanningCollisionDialog"));
            if(dialog) {
                filtered = dialog->findChild<QListWidget*>(QStringLiteral("planningCollisionDetectors"))->count() == 0;
                dialog->reject();
            }
        });
        fixture.motion.collisionConditionsRequested();
        check(filtered, "After switching back the other robot's detector is excluded from the choices");
    }

    void collisionMountTests()
    {
        using namespace motion_planning;
        Workbenches fixture;
        ProjectCdfQpRepairOptions bodyOptions;
        bodyOptions.obstacleId = "burnner"; bodyOptions.detectorId = "body_only";
        check(fixture.documents.mutateProject(QStringLiteral("mountFixture"), ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string&) {
                auto& document = service.document();
                if(!ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document, "fixture_robot", bodyOptions)) return false;
                document.objects.front().transform.x = 0; document.objects.front().transform.z = .55;
                simulation_project::RobotMountDesc mount;
                mount.id = "tool_mount"; mount.robotId = "fixture_robot"; mount.linkName = "Link6";
                mount.linkToMount.z = .5; document.robotMounts.push_back(mount);
                simulation_project::AttachmentAssetDesc asset;
                asset.id = "tool_asset"; asset.name = "Tool mesh";
                asset.visualPath = (fixture.folder / "cube.stl").generic_u8string(); asset.visualScale = .001;
                document.attachmentAssets.push_back(asset);
                simulation_project::MountedAttachmentDesc tool;
                tool.id = "hidden_tool"; tool.name = "Hidden active tool"; tool.mountFrameId = mount.id;
                tool.assetId = asset.id; tool.visible = false; document.mountedAttachments.push_back(tool);
                tool.id = "second_tool"; tool.name = "Second active tool"; tool.mountToAssetMount.x = .2;
                document.mountedAttachments.push_back(tool);
                tool.id = "disabled_tool"; tool.name = "Disabled tool"; tool.enabled = false;
                document.mountedAttachments.push_back(tool);
                auto other = document.robots.front(); other.id = "other_robot"; other.name = "Other robot";
                other.baseTransform.x = 2; document.robots.push_back(other);
                mount.id = "other_mount"; mount.robotId = other.id; document.robotMounts.push_back(mount);
                tool.id = "other_tool"; tool.name = "Other robot tool"; tool.mountFrameId = mount.id; tool.enabled = true;
                document.mountedAttachments.push_back(tool);
                changed = true; return true;
            }).success, "Mount fixture includes hidden active, multiple, disabled and other-robot tools");
        ProjectCdfQpRepairOptions bodySelected; bodySelected.detectorIds = {"body_only"};
        check(!ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(fixture.context.document(), "fixture_robot", bodySelected),
            "A body-only detector does not cover enabled mounted tools");
        bool rejected = false;
        QTimer::singleShot(0, &fixture.motion, [&]() {
            auto* dialog = fixture.motion.findChild<QDialog*>(QStringLiteral("motionPlanningCollisionDialog"));
            if(!dialog) return;
            auto* list = dialog->findChild<QListWidget*>(QStringLiteral("planningCollisionDetectors"));
            for(int i = 0; i < list->count(); ++i)
                list->item(i)->setSelected(list->item(i)->data(Qt::UserRole).toString() == QStringLiteral("body_only"));
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            rejected = dialog->isVisible(); dialog->reject();
        });
        fixture.motion.collisionConditionsRequested();
        check(rejected && fixture.context.document().collision.detectors.size() == 1,
            "The real dialog retains an incomplete mount selection without mutating the project");
        chooseCollisionConditions(fixture, true, QStringLiteral("burnner"));
        auto document = fixture.context.document();
        check(document.collision.detectors.size() == 2, "The dialog creates one complete robot and tool detector");
        if(document.collision.detectors.size() != 2) return;
        const auto full = document.collision.detectors.back();
        ProjectCdfQpRepairOptions selected; selected.detectorIds = {full.id};
        check(full.queryPolicy == "BetweenSets" && full.targets.empty() && full.pairGenerators.empty() &&
            ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(document, "fixture_robot", selected),
            "Mounted-tool creation uses active selection sets and passes the shared prerequisite");
        const auto setA = std::find_if(document.collision.selectionSets.begin(), document.collision.selectionSets.end(),
            [&](const auto& set) { return set.id == full.selectionSetAId; });
        check(setA != document.collision.selectionSets.end() && setA->members.size() == 3 &&
            setA->members.front().robotId == "fixture_robot" && setA->members[1].attachmentId == "hidden_tool" &&
            setA->members[2].attachmentId == "second_tool",
            "Only the planning body and its enabled tools are included, irrespective of visibility");
        ProjectPlanningRequest query; query.robotId = "fixture_robot"; query.jointNames = {"slide"};
        query.collisionDetectorIds = {"body_only"}; std::string error;
        auto bodyScene = ProjectPlanningSceneBuilder::build(document, fixture.folder, query, &error);
        check(bodyScene && bodyScene->validateState({0.}).valid && bodyScene->nearestDistance({0.}).distanceMeters > .4,
            "The reference robot body is clear of the workpiece");
        query.collisionDetectorIds = {full.id};
        auto scene = ProjectPlanningSceneBuilder::build(document, fixture.folder, query, &error);
        check(bool(scene), "A selection-set detector builds actual mounted-tool geometry");
        if(scene) {
            const auto distance = scene->nearestDistance({0.});
            std::cout << "Mount contact distance=" << distance.distanceMeters << " valid=" << distance.valid
                << " collision=" << distance.inCollision << " message=" << distance.message << std::endl;
            check(distance.valid && distance.inCollision && distance.distanceMeters == 0 && !scene->validateState({0.}).valid,
                "Tool-only contact changes both the nearest distance and state collision verdict");
            scene->setState({0.}); const auto before = scene->attachmentWorldTransform("hidden_tool");
            scene->setState({.3}); const auto after = scene->attachmentWorldTransform("hidden_tool");
            check(std::abs(after.translation().x() - before.translation().x() - .3) < 1e-9,
                "Hidden mounted collision geometry follows the robot joint");
            MotionValidationOptions validation; validation.maxJointStep = .01;
            check(scene->validateState({-.4}).valid && scene->validateState({.4}).valid &&
                !scene->validateMotion({-.4}, {.4}, validation).valid,
                "Clear endpoints cannot hide an intermediate mounted-tool collision");
        }
        const auto saved = fixture.folder / "mounted.scene.json";
        simulation_project::ProjectDocument reloaded;
        check(simulation_project::saveProjectDocument(saved, document, &error) &&
            simulation_project::loadProjectDocument(saved, reloaded, &error) &&
            ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(reloaded, "fixture_robot", selected),
            "Mounted collision selection and prerequisite survive project save and load");
        auto reloadedScene = ProjectPlanningSceneBuilder::build(reloaded, fixture.folder, query, &error);
        check(reloadedScene && !reloadedScene->validateState({0.}).valid,
            "Reloaded mounted-tool geometry retains the actual collision");

        auto split = document;
        auto toolDetector = full; toolDetector.id = "tools_only";
        split.collision.detectors = {split.collision.detectors.front(), toolDetector};
        auto& toolSet = *std::find_if(split.collision.selectionSets.begin(), split.collision.selectionSets.end(),
            [&](const auto& set) { return set.id == full.selectionSetAId; });
        toolSet.members.erase(toolSet.members.begin());
        selected.detectorIds = {"body_only", "tools_only"};
        const auto originalFilterCount = split.collision.detectors.front().pairFilters.size();
        check(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(split, "fixture_robot", selected) &&
            split.collision.detectors.front().pairFilters.size() == originalFilterCount && toolSet.members.size() == 2,
            "Existing body and tool detectors can jointly cover all mounts without rewriting their rules");
        toolSet.enabled = false;
        check(!ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(split, "fixture_robot", selected),
            "A disabled tool selection set invalidates coverage");
        auto disabled = document;
        for(auto& tool : disabled.mountedAttachments) if(tool.mountFrameId == "tool_mount") tool.enabled = false;
        check(ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(disabled, "fixture_robot", bodySelected),
            "Disabled mounts and another robot's tools do not impose planning-tool coverage");

        ProjectCdfQpRepairOptions recreate; recreate.detectorId = full.id; recreate.obstacleId = "burnner";
        const auto count = document.collision.selectionSets.size();
        check(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document, "fixture_robot", recreate) &&
            ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document, "fixture_robot", recreate) &&
            document.collision.selectionSets.size() == count &&
            ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(document, "fixture_robot", recreate),
            "Repeated domain setup reuses private selection sets and recognizes mounted configuration");
        auto sharedDetector = full; sharedDetector.id = "shared_user_detector";
        document.collision.detectors.push_back(sharedDetector);
        auto extraTool = document.mountedAttachments.front(); extraTool.id = "later_tool";
        document.mountedAttachments.push_back(extraTool);
        check(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document, "fixture_robot", recreate),
            "Rebuilding a detector after adding a mount succeeds");
        const auto originalSet = std::find_if(document.collision.selectionSets.begin(), document.collision.selectionSets.end(),
            [&](const auto& set) { return set.id == full.selectionSetAId; });
        check(originalSet != document.collision.selectionSets.end() && originalSet->members.size() == 3,
            "Rebuilding does not overwrite a selection set shared by another detector");

        auto robotPairDocument = fixture.context.document();
        ProjectCdfQpRepairOptions robotPair; robotPair.detectorId = "robot_pair"; robotPair.obstacleId = "other_robot";
        check(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(robotPairDocument, "fixture_robot", robotPair) &&
            ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(robotPairDocument, "fixture_robot", robotPair),
            "Robot-to-robot setup includes mounts on both robots");
        const auto pairPlanDocument = robotPairDocument;
        robotPairDocument.collision.detectors.erase(robotPairDocument.collision.detectors.begin(), robotPairDocument.collision.detectors.end() - 1);
        const auto pairPlan = simulation_runtime::ProjectCollisionDetectorBuilder::collectBuildPlan(robotPairDocument);
        check(pairPlan.requiresAttachment("hidden_tool", "", "tool") && pairPlan.requiresAttachment("other_tool", "", "tool"),
            "Both moving and target robot tools enter the collision build plan");
        query.collisionDetectorIds = {"robot_pair"};
        auto pairScene = ProjectPlanningSceneBuilder::build(pairPlanDocument, fixture.folder, query, &error);
        check(pairScene && pairScene->nearestDistance({0.}).valid, "Robot-to-robot mount sets build a working distance scene");
        auto all = fixture.context.document(); all.collision.detectors = {full};
        all.collision.detectors.front().type = "SceneAll"; all.collision.detectors.front().queryPolicy = "AllEnabled";
        all.collision.detectors.front().selectionSetAId.clear(); all.collision.detectors.front().selectionSetBId.clear();
        selected.detectorIds = {full.id}; query.collisionDetectorIds = {full.id};
        auto allScene = ProjectPlanningSceneBuilder::build(all, fixture.folder, query, &error);
        check(ProjectCdfQpTrajectoryRepairService::collisionSetupConfigured(all, "fixture_robot", selected) &&
            allScene && allScene->nearestDistance({0.}).inCollision,
            "An existing AllEnabled scene detector covers mounted tools using the platform policy");
    }

    void xyzrTests()
    {
        using namespace motion_planning;
        Workbenches fixture(false,false);
        const auto root=std::filesystem::u8path(XYZR_SOURCE_ROOT);
        auto document=fixture.context.document();
        document.robots[0].sourcePath=(root/"data/robots/xyzr_spray_robot_demo/urdf/xyzr_spray_robot_demo.urdf").generic_u8string();
        document.robots[0].initialJoints.clear();document.robotMounts.clear();
        document.attachmentAssets.clear();document.mountedAttachments.clear();document.objects.clear();
        fixture.session.setDocument(document,fixture.folder/"xyzr.scene.json",false,false);
        check(fixture.documentViewport.loadProjectDocument(document,fixture.folder).success,"Actual XYZR meshes and four-axis viewport initialize");
        fixture.selection.selectRobotLink(QStringLiteral("fixture_robot"),QStringLiteral("spray_gun"));
        fixture.planning=std::make_unique<MotionPlanningModuleController>(fixture.motion,fixture.context);
        for(const auto kind:{RobotQtViewerEventKind::SelectionChanged,RobotQtViewerEventKind::ProjectDocumentChanged,RobotQtViewerEventKind::ProjectOpened})
            fixture.hub.subscribe(kind,fixture.planning.get(),[&](const auto& e){fixture.planning->handleEvent(e);});
        QString status;
        QObject::connect(fixture.planning.get(),&MotionPlanningModuleController::statusMessageRequested,&fixture.motion,
            [&](const QString& text,int){status=text;std::cout<<"XYZR UI: "<<text.toStdString()<<std::endl;});
        TrajectoryImportOptions options;options.robotId="fixture_robot";options.planId="xyzr_real";
        const auto imported=ProjectTrajectoryImporter::importFile(root/"data/trajectories/xyzr_tongpian.txt",options);
        check(imported.success,"Full user matrix trajectory imports through production importer");
        if(!imported.success)return;
        check(fixture.documents.mutateProject(QStringLiteral("xyzrTestImport"),ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string& error){
                changed=MotionPlanningProjectStore::upsertPlan(service.document(),imported.plan,&error);return changed;
            }).success,"Imported 8735-point plan stored through document mutation");
        fixture.motion.trajectorySelectionChanged(QString::fromStdString(imported.plan.id));
        fixture.motion.inverseKinematicsRequested(true);QApplication::processEvents();
        auto plans=MotionPlanningProjectStore::plans(fixture.context.document());
        const auto solved=std::find_if(plans.begin(),plans.end(),[](const auto& p){return p.id=="xyzr_real";});
        check(solved!=plans.end() && solved->trajectory.points.size()==8735,"Basic Planning IK button solves all 8735 XYZR targets");
        if(solved==plans.end() || solved->trajectory.points.size()!=8735)return;
        check(solved->jointNames==std::vector<std::string>{"x_joint","y_joint","z_joint","yaw_joint"},"Empty saved initial joints discover actual URDF joint names");
        // Playback uses the controller's actual viewport and waits for presented frames.
        fixture.motion.sprayMeasurementEnabledChanged(false);
        fixture.motion.trajectoryPointsVisibilityChanged(false);
        fixture.motion.trajectoryOrientationsVisibilityChanged(false);
        fixture.viewport.resize(960,720);
        fixture.viewport.resetCamera();fixture.viewport.show();QApplication::processEvents();fixture.motion.playbackRequested(1.5);
        QElapsedTimer timer;timer.start();int changed=0;double last=0;bool finished=false;
        while(timer.elapsed()<150000) {
            QApplication::processEvents();QThread::msleep(5);
            bool ok=false;const double q=fixture.services.robotJointValue(QStringLiteral("fixture_robot"),QStringLiteral("z_joint"),&ok);
            if(ok && std::abs(q-last)>1e-5){++changed;last=q;}
            bool stopped=false;
            for(auto* button:fixture.motion.findChildren<QPushButton*>())if(button->text()==QStringLiteral("Play IK result"))stopped=true;
            if(timer.elapsed()>100 && stopped){finished=true;break;}
        }
        check(finished && changed>4,"Actual Qt viewport presents dynamic four-axis frames and finishes all points");
        bool final=true;
        for(std::size_t j=0;j<4;++j) {
            bool ok=false;double q=fixture.services.robotJointValue(QStringLiteral("fixture_robot"),QString::fromStdString(solved->jointNames[j]),&ok);
            final &= ok && std::abs(q-solved->trajectory.points.back().q[j])<1e-9;
        }
        check(final,"Displayed robot reaches the exact last inverse-kinematics solution");
        const auto output=QDir::currentPath()+QStringLiteral("/xyzr_qt_playback.png");fixture.viewport.grab().save(output);
        // The six-axis fixture still enters the untouched actual-model numerical path.
        Workbenches legacy(true,true,true);legacy.selection.selectRobotLink(QStringLiteral("fixture_robot"),QStringLiteral("Link6"));
        QObject::connect(legacy.planning.get(),&MotionPlanningModuleController::statusMessageRequested,&legacy.motion,
            [](const QString& text,int){std::cout<<"Legacy IK UI: "<<text.toStdString()<<std::endl;});
        auto fk=legacy.services.robotForwardKinematics(QStringLiteral("fixture_robot"),{"j0","j1","j2","j3","j4","j5"},true);
        check(bool(fk),"Legacy six-axis TCP model remains available");
        if(fk) {
            StoredMotionPlan plan;plan.id="legacy_ik";plan.robotId="fixture_robot";plan.name="legacy";
            for(int i=0;i<3;++i){robottrajectory::TimedCartesianPoint p;p.time=i;p.tcpPose=fk({.2+i*.01,-.4,.5,.2,.6,-.2});plan.cartesianControlPoints.points.push_back(p);}
            check(legacy.documents.mutateProject(QStringLiteral("legacyTestImport"),ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string& error){changed=MotionPlanningProjectStore::upsertPlan(service.document(),plan,&error);return changed;}).success,
                "Legacy Cartesian targets stored through document API");
            legacy.selection.selectRobotLink(QStringLiteral("fixture_robot"),QStringLiteral("Link6"));
            RobotQtViewerEvent selected;selected.kind=RobotQtViewerEventKind::SelectionChanged;
            selected.selection.robotId=QStringLiteral("fixture_robot");
            legacy.planning->handleEvent(selected);
            legacy.motion.trajectorySelectionChanged(QStringLiteral("legacy_ik"));legacy.motion.inverseKinematicsRequested(true);
            const auto stored=MotionPlanningProjectStore::plans(legacy.context.document());
            const auto old=std::find_if(stored.begin(),stored.end(),[](const auto& p){return p.id=="legacy_ik";});
            check(old!=stored.end() && old->trajectory.points.size()==3 && old->jointNames.size()==6,"Same Basic Planning IK action still solves legacy six-axis targets");
            if(old==stored.end() || old->trajectory.points.size()!=3)for(auto* label:legacy.motion.findChildren<QLabel*>())
                if(label->text().contains(QStringLiteral("IK")) || label->text().contains(QStringLiteral("kinematics")))std::cout<<label->text().toStdString()<<'\n';
        }
    }

    void fanucTests()
    {
        using namespace motion_planning;
        Workbenches fixture(false,false);
        const auto root=std::filesystem::u8path(XYZR_SOURCE_ROOT);
        simulation_project::ProjectDocument document,original;std::string error;
        check(simulation_project::loadProjectDocument(root/"config/projects/ABB4600-Aburnner.sys.json",original,&error),"Original Fanuc engineering document loads");
        document.robots.push_back(original.robots.front());
        document.robotMounts=original.robotMounts;document.attachmentAssets=original.attachmentAssets;
        document.mountedAttachments=original.mountedAttachments;
        for(auto& attachment:document.mountedAttachments)attachment.sourceObjectId.clear();
        const auto robotId=document.robots.front().id;
        document.robots.front().sourcePath=(root/"data/fanuc_m710ic50/m710ic_50_drake.urdf").generic_u8string();
        // Keep the real saved TCP and robot; avoid unrelated workpiece collision queries.
        document.robots.resize(1);document.objects.clear();document.collision.detectors.clear();
        document.attachmentAssets.front().visualPath=(root/"data/Spray420/A2(1).STL").generic_u8string();
        fixture.session.setDocument(document,fixture.folder/"fanuc.scene.json",false,false);
        check(fixture.documentViewport.loadProjectDocument(document,root/"data").success,"Actual M-710iC/50 viewport loads with original saved tool");
        fixture.planning=std::make_unique<MotionPlanningModuleController>(fixture.motion,fixture.context);
        for(const auto kind:{RobotQtViewerEventKind::SelectionChanged,RobotQtViewerEventKind::ProjectDocumentChanged,RobotQtViewerEventKind::ProjectOpened})
            fixture.hub.subscribe(kind,fixture.planning.get(),[&](const auto& e){fixture.planning->handleEvent(e);});
        QObject::connect(fixture.planning.get(),&MotionPlanningModuleController::statusMessageRequested,&fixture.motion,
            [](const QString& text,int){std::cout<<"Fanuc UI: "<<text.toStdString()<<std::endl;});
        TrajectoryImportOptions import;import.robotId=robotId;import.planId="fanuc_real";
        for(const auto& joint:document.robots.front().initialJoints)import.jointNames.push_back(joint.jointName);
        const auto input=ProjectTrajectoryImporter::importFile(root/
            "SMRobotMotionPlanning/ProjectMotionPlanning/regression/ProjectMotionPlanningHeadless/fanuc_limit_branches.txt",import);
        check(input.success,"Original failing Fanuc poses import");if(!input.success)return;
        const auto solve=[&](StoredMotionPlan plan,bool expected) {
            const auto mutation=fixture.documents.mutateProject(QStringLiteral("fanucTestImport"),ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string& e){
                    changed=MotionPlanningProjectStore::upsertPlan(service.document(),plan,&e);return changed;
                });
            if(!mutation.success)std::cout<<"Fanuc mutation: "<<mutation.message.toStdString()<<std::endl;
            check(mutation.success,"Fanuc Cartesian targets stored by document mutation");
            fixture.selection.selectRobotLink(QString::fromStdString(robotId),QStringLiteral("ee_link"));
            RobotQtViewerEvent selected;selected.kind=RobotQtViewerEventKind::SelectionChanged;
            selected.selection.robotId=QString::fromStdString(robotId);fixture.planning->handleEvent(selected);
            fixture.motion.trajectorySelectionChanged(QString::fromStdString(plan.id));
            fixture.motion.inverseKinematicsRequested(true);
            const auto plans=MotionPlanningProjectStore::plans(fixture.context.document());
            const auto found=std::find_if(plans.begin(),plans.end(),[&](const auto& p){return p.id==plan.id;});
            const auto count=found==plans.end()?0:found->trajectory.points.size();
            check(expected?count==plan.cartesianControlPoints.points.size():count==0,
                expected?"Real Fanuc Basic Planning action solves reachable target(s)":"Real Fanuc action rejects original-limit violation without storing partial path");
            if(expected && count>0) {
                std::vector<double> displayed;
                for(const auto& name:found->jointNames)displayed.push_back(fixture.services.robotJointValue(
                    QString::fromStdString(robotId),QString::fromStdString(name)));
                const auto fk=fixture.services.robotForwardKinematics(QString::fromStdString(robotId),found->jointNames,true);
                const auto actual=fk(displayed);const auto& target=plan.cartesianControlPoints.points.front().tcpPose;
                check((actual.translation()-target.translation()).norm()<1e-6 &&
                    Eigen::AngleAxisd(actual.linear()*target.linear().transpose()).angle()<1e-6,
                    "Displayed Fanuc joints reach original target, including positions beyond URDF limits");
            }
            return count;
        };
        auto legal=input.plan;legal.cartesianControlPoints.points.clear();
        for(std::size_t i=0;i<input.plan.cartesianControlPoints.points.size();++i) {
            auto one=input.plan;one.cartesianControlPoints.points={input.plan.cartesianControlPoints.points[i]};
            solve(one,true);
            legal.cartesianControlPoints.points.push_back(input.plan.cartesianControlPoints.points[i]);
        }
        solve(input.plan,true);
        fixture.motion.multiIkRequested(true,QVector<double>(6,-360),QVector<double>(6,360),64);
        QElapsedTimer multiTimer;multiTimer.start();QComboBox* pointList=nullptr;
        while(multiTimer.elapsed()<120000) {
            QApplication::processEvents();QThread::msleep(5);
            pointList=fixture.motion.findChild<QComboBox*>(QStringLiteral("multiIkPoint"));
            if(pointList && pointList->count()==8)break;
        }
        check(pointList && pointList->count()==8,"Real Fanuc All IK action finishes all original failing fixtures");
        if(pointList && pointList->count()==8)for(int i=0;i<8;++i) {
            const bool hasRoot=!pointList->itemText(i).contains(QStringLiteral("| 0 "));
            check(hasRoot==(i<4 || i==7),"All IK agrees with single IK and original physical limits");
        }
        solve(legal,true);
        fixture.motion.sprayMeasurementEnabledChanged(false);
        fixture.motion.trajectoryPointsVisibilityChanged(false);
        fixture.motion.trajectoryOrientationsVisibilityChanged(false);
        fixture.viewport.resize(960,720);fixture.viewport.resetCamera();fixture.viewport.show();QApplication::processEvents();
        fixture.motion.playbackRequested(1.0);
        QElapsedTimer timer;timer.start();bool finished=false;int changes=0;double last=0;
        while(timer.elapsed()<60000) {
            QApplication::processEvents();QThread::msleep(5);
            const auto q=fixture.services.robotJointValue(QString::fromStdString(robotId),QStringLiteral("J3"));
            if(std::abs(q-last)>1e-5){last=q;++changes;}
            for(auto* button:fixture.motion.findChildren<QPushButton*>())
                if(timer.elapsed()>100 && button->text()==QStringLiteral("Play IK result"))finished=true;
            if(finished)break;
        }
        check(finished && changes>2,"Fanuc unbounded basic IK animates through all regression targets");
    }

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


    void sectionSweepTests()
    {
        QElapsedTimer stages;stages.start();
        const auto stage=[&](const char* name) {std::cout<<"[section-sweep ms="<<stages.elapsed()<<"] "<<name<<std::endl;};
        Workbenches fixture(true,true);
        using namespace smrobot::workbench::freeform;
        using spray_freeform_traj_plan::WorkflowAction;
        FreeformTrajPlanWorkflowPanel workflow;
        FreeformTrajPlanParameterPanel parameters;
        FreeformTrajPlanWorkbenchController controller(workflow,parameters,fixture.context,&fixture.source,{},{});
        auto* method=workflow.findChild<QComboBox*>(QStringLiteral("freeformPlanningMethod"));
        auto* scanPanel=parameters.findChild<SectionSweepParameterPanel*>();
        check(method && method->count()==3 && scanPanel,"Third planning mode and separate parameter panel exist");
        if(!method||!scanPanel)return;
        check(scanPanel->parameters().maximumTiltDegrees==88. && scanPanel->parameters().transferSpeed==.200,
            "UI selects the new 88-degree coverage method and 200 mm/s transfer limit");
        method->setCurrentIndex(2);
        check(scanPanel->isVisibleTo(&parameters),"Selecting third mode switches parameter UI");
        QTimer unitTimer;
        bool meters=true;
        QObject::connect(&unitTimer,&QTimer::timeout,&workflow,[&] {
            auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if(!box)return;
            for(auto* button:box->buttons()) {
                if(button->text()==(meters?QStringLiteral("米 (m)"):QStringLiteral("毫米 (mm)"))) {
                    unitTimer.stop();button->click();return;
                }
            }
        });
        const QString input=qEnvironmentVariable("RS2026_SECTION_SWEEP_MODEL",QString::fromUtf8(SECTION_SWEEP_FIXTURE));
        unitTimer.start(30);workflow.importModelRequested(input);
        check(fixture.source.snapshot().hasModel(),"Shared import loads the metre-valued plate/tube STL");
        auto* generate=workflow.findChild<QPushButton*>(QStringLiteral("sectionSweepWorkflowGenerate"));
        if(!generate){check(false,"Third workflow generate button missing");return;}
        generate->click();
        stage("first UI generation and downstream publication");
        auto snapshot=fixture.source.snapshot();
        auto points=snapshot.trajectory.flattenedPoints();
        check(!points.empty(),"Actual Qt third-mode action produces shared trajectory");
        if(points.empty())return;
        std::cout<<"Section shared points="<<points.size()<<" segments="<<snapshot.trajectory.segments.size()<<std::endl;
        bool spray=false,air=false,samplerCorrect=true;
        for(const auto& segment:snapshot.trajectory.segments) {
            spray|=segment.sprayEnabled;air|=!segment.sprayEnabled;
            if(segment.points.size()<2)continue;
            const double t=.5*(segment.points.front().time+segment.points[1].time);
            if(t>segment.points.front().time)
                samplerCorrect &= spraytrajectory::SprayTrajectorySampler::evaluate(snapshot.trajectory,t).sprayEnabled==segment.sprayEnabled;
        }
        check(spray&&air&&samplerCorrect,"Shared sampler sprays only on scan strokes, including entry/exit transitions");
        check(snapshot.sprayAxis==4 && snapshot.feedAxis==2,"Tool +Z spraying and +Y footprint major axis match the webpage coverage model");
        check(fixture.panel.sprayDirectionLocal().isApprox(Eigen::Vector3d::UnitZ()) &&
            fixture.panel.powderFeedDirectionLocal().isApprox(Eigen::Vector3d::UnitY()),
            "Actual coating panel receives the same spray/footprint frame as native coverage fitting");
        auto world=snapshot;QString error;
        check(planningDataToWorld(snapshot,fixture.context.document(),world,&error),"Third-mode trajectory converts through common world transfer");
        const auto plans=motion_planning::MotionPlanningProjectStore::plans(fixture.context.document());
        const auto plan=std::find_if(plans.begin(),plans.end(),[&](const auto& item) {
            return item.cartesianControlPoints.points.size()==points.size();
        });
        check(plan!=plans.end(),"Third-mode result automatically reaches the Motion Planning document");
        check(fixture.coating->waypoints().size()==points.size(),"Third-mode result reaches coating analysis with air flags");
        ModelTrajPlan::ModelData model;ModelTrajPlan::STLProcessor reader;
        check(reader.preprocessSTL(input.toStdString(),model),"Independent planner source loads");
        const auto mesh=sectionMeshInMeters(model.mainModelData_,1.);
        const auto domain=section_sweep::SingleSideScanPlanner::generate(mesh,scanPanel->parameters());
        stage("independent domain generation");
        check(domain.success && domain.passCount==36 && domain.exteriorSamples>5000 &&
            domain.coveredSamples==domain.exteriorSamples && domain.maximumUsedTiltDegrees>87.9,
            "Actual third-mode settings fit the existing 36 passes for pipe/root coverage without extra passes");
        const auto exported=sectionExportPoints(domain);
        check(domain.success&&!exported.empty(),"Independent domain generation succeeds");
        if(exported.empty())return;
        check((exported.front().pose.block<3,1>(0,3)*.001-points.front().tcpPose.translation()).norm()<1.e-10,
            "Export millimetres and shared metres agree without double scaling");
        auto mmModel=model.mainModelData_;for(auto& value:mmModel.m_vertices)value*=1000.f;
        const auto mmDomain=section_sweep::SingleSideScanPlanner::generate(sectionMeshInMeters(mmModel,.001),scanPanel->parameters());
        stage("millimetre input generation");
        check(mmDomain.success && mmDomain.passCount==domain.passCount &&
            (mmDomain.points.front().pose.translation()-domain.points.front().pose.translation()).norm()<1.e-7,
            "Millimetre and metre input adapters generate the same geometry");
        const auto interpolated=sectionExportPoints(domain,.04);
        bool metadata=true;
        for(const auto& point:interpolated) {
            if(point.point_type==2)metadata &= point.spray_distance==0.&&
                (std::abs(point.velocity-200.)<1.e-9 || std::abs(point.velocity-20.)<1.e-9);
            else metadata &= std::abs(point.spray_distance-100.)<1.e-9&&std::abs(point.velocity-20.)<1.e-9;
        }
        check(metadata,"Optional export sampling preserves each move's speed, spray mask and standoff");
        const QString output=qEnvironmentVariable("RS2026_SECTION_SWEEP_OUTPUT",fixture.temporary.path());
        std::filesystem::create_directories(std::filesystem::path(output.toStdWString()));
        std::ofstream coverage(std::filesystem::path((output+QStringLiteral("/coverage_summary.json")).toStdWString()));
        coverage<<"{\"passes\":"<<domain.passCount<<",\"points\":"<<domain.points.size()
            <<",\"exterior_samples\":"<<domain.exteriorSamples<<",\"covered_samples\":"<<domain.coveredSamples
            <<",\"maximum_tilt_degrees\":"<<domain.maximumUsedTiltDegrees
            <<",\"footprint_peak_threshold\":0.05,\"minimum_incidence_cosine\":0.02}";
        const auto paq=output+QStringLiteral("/tongban_single_side_paq.txt");
        const auto matrix=output+QStringLiteral("/tongban_single_side_matrix.txt");
        workflow.exportTrajectoryRequested(paq,FreeformTrajPlanExportFormat::Paq,0.);
        workflow.exportTrajectoryRequested(matrix,FreeformTrajPlanExportFormat::Matrix,0.);
        std::ifstream file(std::filesystem::path(paq.toStdWString()));
        bool roundTrip=true;
        for(const auto& point:exported) {
            double row[12];for(auto& x:row)if(!(file>>x)){roundTrip=false;break;}
            if(!roundTrip)break;
            Eigen::Quaterniond q(row[3],row[4],row[5],row[6]);q.normalize();
            roundTrip &= (Eigen::Vector3d(row[0],row[1],row[2])-point.pose.block<3,1>(0,3)).norm()<1.e-5;
            roundTrip &= (q.toRotationMatrix()-point.pose.block<3,3>(0,0)).norm()<1.e-5;
            roundTrip &= std::abs(row[9]-point.timestamp)<1.e-6 && row[10]==point.point_type;
        }
        check(roundTrip&&std::filesystem::file_size(std::filesystem::path(matrix.toStdWString()))>0,
            "Common TXT and matrix exports retain poses, times and air labels");
        stage("exports and round-trip verification");
        fixture.viewport.resize(1000,800);
        fixture.viewport.focusCoatingObject(snapshot.modelObjectId,0.);
        QApplication::processEvents();
        fixture.viewport.grab().save(output+QStringLiteral("/tongban_single_side_preview.png"));
        workflow.resize(320,1000);parameters.resize(360,1000);
        workflow.grab().save(output+QStringLiteral("/planning_method_ui.png"));
        parameters.grab().save(output+QStringLiteral("/scan_parameters_ui.png"));
        workflow.workflowActionRequested(WorkflowAction::TransformTrajectory);
        auto* transform=workflow.findChild<QDialog*>(QStringLiteral("trajectoryTransformDialog"));
        check(transform!=nullptr,"Common pose transform dialog supports the third mode");
        if(transform) {
            transform->findChild<QDoubleSpinBox*>(QStringLiteral("transformPose0"))->setValue(10.);
            transform->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            const auto transformed=fixture.source.snapshot();PlanningDataSnapshot transformedWorld;
            check(planningDataToWorld(transformed,fixture.context.document(),transformedWorld,&error) &&
                (transformed.trajectory.flattenedPoints().front().tcpPose.translation()-points.front().tcpPose.translation()).norm()<1.e-10 &&
                (transformedWorld.trajectory.flattenedPoints().front().tcpPose.translation()-
                    (points.front().tcpPose.translation()+Eigen::Vector3d(.01,0,0))).norm()<1.e-10,
                "Moving model and scan by 10 mm applies translation once in world coordinates");
            const auto transformedPath=output+QStringLiteral("/transformed_paq.txt");
            workflow.exportTrajectoryRequested(transformedPath,FreeformTrajPlanExportFormat::Paq,0.);
            std::ifstream moved(std::filesystem::path(transformedPath.toStdWString()));double x=0.;
            check(bool(moved>>x)&&std::abs(x-exported.front().pose(0,3)-10.)<1.e-5,
                "Third-mode TXT exports the displayed transformed position in millimetres");
        }
        method->setCurrentIndex(1);check(fixture.source.snapshot().trajectory.empty(),"UV mode does not publish the section scan as UV data");
        method->setCurrentIndex(0);check(fixture.source.snapshot().trajectory.empty(),"Slicing mode owns independent output");
        method->setCurrentIndex(2);check(fixture.source.snapshot().trajectory.flattenedPoints().size()==points.size(),"Returning to scan mode restores its generated trajectory");
        scanPanel->findChild<QDoubleSpinBox*>(QStringLiteral("sectionPassSpacing"))->setValue(6.);
        check(fixture.source.snapshot().trajectory.empty(),"Parameter edit invalidates stale shared trajectory");
        workflow.exportTrajectoryRequested(output+QStringLiteral("/stale.txt"),FreeformTrajPlanExportFormat::Paq,0.);
        check(!std::filesystem::exists(std::filesystem::path((output+QStringLiteral("/stale.txt")).toStdWString())),"Stale or empty trajectory cannot be exported");
        stage("transforms, mode switching and invalidation");
        generate->click();stage("regeneration after parameter edit");
        workflow.workflowActionRequested(WorkflowAction::ClearTrajectoryData);
        stage("clear trajectory");
        check(fixture.source.snapshot().hasModel()&&fixture.source.snapshot().trajectory.empty(),"Common clear-trajectory retains model and clears third-mode output");
        generate->click();stage("regeneration after clear");
        workflow.workflowActionRequested(WorkflowAction::ClearAllData);
        stage("clear all");
        check(!fixture.source.snapshot().hasModel()&&fixture.source.snapshot().trajectory.empty(),"Common clear-all clears third-mode model, output and downstream source");
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
        check(fixture.documents.mutateProject(QStringLiteral("unselectedDecoy"),ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service,bool& changed,std::string&) {
                service.document().objects.front().transform.x = 0;
                service.document().objects.front().transform.z = .4;
                motion_planning::ProjectCdfQpRepairOptions decoy;
                decoy.obstacleId = "burnner"; decoy.detectorId = "unselected_colliding_detector";
                changed = motion_planning::ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(
                    service.document(), "fixture_robot", decoy); return changed;
            }).success,"An enabled unselected detector covers an overlapping decoy workpiece");
        chooseCollisionConditions(fixture, true, QStringLiteral("surface"));
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
        const auto& detector = fixture.context.document().collision.detectors.back();
        const auto& sets = fixture.context.document().collision.selectionSets;
        check(detector.queryPolicy == "BetweenSets" && std::any_of(sets.begin(), sets.end(), [&](const auto& set) {
            return set.id == detector.selectionSetBId && set.members.size() == 1 && set.members.front().objectId == "surface";
        }) && std::any_of(sets.begin(), sets.end(), [&](const auto& set) {
            return set.id == detector.selectionSetAId && std::any_of(set.members.begin(), set.members.end(),
                [](const auto& member) { return member.attachmentId == "test_attachment"; });
        }),"CDF collision setup includes the mounted tool against the actual source object");
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
    else if(mode=="--collision-conditions") collisionConditionTests();
    else if(mode=="--collision-mounts") collisionMountTests();
    else if(mode=="--xyzr") xyzrTests();
    else if(mode=="--fanuc") fanucTests();
    else if(mode=="--section-sweep") sectionSweepTests();
    else if(mode=="--freeform-transitions") freeformTransitionTests();
    else if(mode=="--cdf") cdfTests();
    else if(mode=="--receiving") receivingTests();
    else return 2;
    std::cout << "Failures: " << failures << std::endl;
    return failures?1:0;
}
