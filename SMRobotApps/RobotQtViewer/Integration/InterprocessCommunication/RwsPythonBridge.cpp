#include "RwsPythonBridge.h"

#include "RobotViewport.h"
#include "SceneEntitySemantics.h"
#include "UserCoordinateFrame.h"
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <SimulationProject/ProjectIo.h>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <limits>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <ProjectMotionPlanning/ExternalPlanningJson.h>
#include <ProjectMotionPlanning/ZigzagTrajectorySegmentation.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include <future>

namespace {
QString text(const std::string& value) { return QString::fromStdString(value); }
QJsonObject poseJson(const simulation_project::TransformDesc& p)
{
    return {{"x", p.x}, {"y", p.y}, {"z", p.z},
        {"roll", p.roll}, {"pitch", p.pitch}, {"yaw", p.yaw}};
}
QJsonObject objectJson(const simulation_project::SceneObjectDesc& o, const simulation_project::ProjectDocument& document)
{
    const auto role = robot_qt_viewer::sceneObjectRole(document, o);
    QJsonObject result{{"id", text(o.id)}, {"name", text(o.name)}, {"source_path", text(o.sourcePath)},
        {"type", text(role)}, {"source_type", text(o.objectType)}, {"pose", poseJson(o.transform)},
        {"role", text(role)}, {"is_workpiece", role == "workpiece"}, {"is_tool", role == "tool"},
        {"collision_enabled", o.collisionEnabled}, {"frame", "world"}, {"angular_unit", "rad"}};
    if(const auto* attachment = robot_qt_viewer::objectAttachment(document, o.id)) {
        result["attachment_id"] = text(attachment->id);
        result["asset_id"] = text(attachment->assetId);
        for(const auto& mount : document.robotMounts)
            if(mount.id == attachment->mountFrameId) result["owner_robot_id"] = text(mount.robotId);
    } else result["owner_robot_id"] = QJsonValue();
    return result;
}
bool requiredBool(const QJsonObject& r, const char* key)
{
    if(!r.value(key).isBool()) throw std::runtime_error(std::string(key) + " must be boolean");
    return r.value(key).toBool();
}
QJsonArray detectorsJson(RobotViewport* viewport)
{
    QJsonArray result;
    for(const auto& d : viewport->collisionDetectors()) {
        const auto& n = d.nearest;
        const auto point = [](const ProjectScene::CollisionDetectorInfo::Vec3Info& v) {
            return QJsonArray{v.x, v.y, v.z};
        };
        QJsonArray contacts;
        for(const auto& c : d.contacts)
            contacts.append(QJsonObject{{"body_a", text(c.bodyA)}, {"body_b", text(c.bodyB)},
                {"position", point(c.position)}, {"normal", point(c.normal)}, {"penetration_depth", c.penetrationDepth}});
        QJsonValue distance;
        if(d.hasResult && std::isfinite(d.minDistance) && d.minDistance < std::numeric_limits<double>::max())
            distance = d.minDistance;
        result.append(QJsonObject{{"id", text(d.id)}, {"name", text(d.name)},
            {"enabled", d.enabled}, {"valid", d.valid}, {"error", text(d.errorMessage)},
            {"has_result", d.hasResult}, {"in_collision", d.inCollision}, {"min_distance", distance},
            {"configured_pair_count", double(d.includePairCount)}, {"effective_pair_count", double(d.effectiveIncludePairCount)},
            {"contacts", contacts}, {"nearest", QJsonObject{{"valid", n.valid}, {"state", text(n.state)},
                {"reason", text(n.reason)}, {"body_a", text(n.bodyA)}, {"body_b", text(n.bodyB)},
                {"point_a", n.valid ? QJsonValue(point(n.pointA)) : QJsonValue()},
                {"point_b", n.valid ? QJsonValue(point(n.pointB)) : QJsonValue()},
                {"distance", n.valid ? QJsonValue(n.distance) : QJsonValue()}}}});
    }
    return result;
}
}


RwsPythonBridge::RwsPythonBridge(RobotViewport* viewport, const QString& directory, QObject* parent)
    : QObject(parent), viewport_(viewport), directory_(QDir(directory).absolutePath()),
      lock_(new QLockFile(directory_ + "/server.lock")),
      session_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    if(!QDir().mkpath(directory_) || !lock_->tryLock(0))
        throw std::runtime_error("RWS SDK directory unavailable or already in use");
    startTcp();
    write(directory_ + "/server.json", {{"protocol", 1}, {"session", session_},
        {"tcp_host", "127.0.0.1"}, {"tcp_port", int(server_->serverPort())}});
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { poll(); });
    timer->start(20);
}
struct RwsPythonBridge::PlanningJob
{
    std::atomic<bool> cancel{false}, done{false};
    std::mutex mutex;
    std::string progress="Snapshot captured", result;
    // Independent entries avoid detaching/copying a large shared JSON array
    // every time a streamed candidate is appended or an old entry is removed.
    struct Event { QJsonObject value; std::size_t bytes; };
    std::deque<Event> events;
    std::size_t eventBytes=0;
    int eventSequence=0;
    QString snapshotId=QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 submittedMs=QDateTime::currentMSecsSinceEpoch();
    std::promise<void> exited;
    std::future<void> completion=exited.get_future();
    std::thread worker;
    // Result publication precedes capture/TLS destruction. Only report completion
    // after the worker exits, so forgetting a job cannot block the GUI in join().
    bool finished() const {
        return done.load() && completion.wait_for(std::chrono::seconds(0))==std::future_status::ready;
    }
    ~PlanningJob() {cancel.store(true); if(worker.joinable()) worker.join();}
};
RwsPythonBridge::~RwsPythonBridge()
{
    for(auto& entry:planningJobs_) entry.second->cancel.store(true);
    planningJobs_.clear();
    QFile::remove(directory_ + "/server.json");
}

QJsonObject RwsPythonBridge::executePlanning(const QJsonObject& request)
{
    const auto op=request.value("op").toString();
    if(op=="planning_submit") {
        for(const auto& entry:planningJobs_) if(!entry.second->finished())
            throw std::runtime_error("A planning job is already running; cancel it or wait.");
        if(planningJobs_.size()>=8) throw std::runtime_error("Forget a completed planning job before submitting another.");
        auto* scene=viewport_->m_scene.get();
        if(!scene || !scene->isInitialized()) throw std::runtime_error("No initialized scene to snapshot.");
        const bool fromFile=request.value("request_file").isString();
        if(fromFile==request.value("planning_request").isObject())
            throw std::runtime_error("Supply exactly one planning_request object or absolute request_file.");
        const QString requestFile=request.value("request_file").toString();
        if(fromFile && !QDir::isAbsolutePath(requestFile)) throw std::runtime_error("request_file must be absolute.");
        auto snapshot=scene->runtimeDocumentSnapshot();
        const auto basePath=scene->projectBasePath();
        const auto inlineJson=QJsonDocument(request.value("planning_request").toObject()).toJson(QJsonDocument::Compact).toStdString();
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        auto job=std::make_shared<PlanningJob>();
        // Seed defaults from the captured GUI state, not from the saved project.
        QJsonArray snapshotRobots;
        for(const auto& robot:snapshot.robots) {
            QJsonObject joints;
            for(const auto& joint:robot.initialJoints) joints.insert(text(joint.jointName),joint.value);
            snapshotRobots.append(QJsonObject{{"robot_id",text(robot.id)},{"joints",joints},{"base_pose",poseJson(robot.baseTransform)}});
        }
        QJsonArray snapshotObjects;
        for(const auto& object:snapshot.objects) snapshotObjects.append(objectJson(object, snapshot));
        planningJobs_.emplace(id,job);
        try {
            // Worker captures plain data only. It never references the bridge, viewport or GUI scene.
            job->worker=std::thread([job=job.get(),snapshot=std::move(snapshot),basePath,fromFile,requestFile,inlineJson]() {
                const bool trace=qEnvironmentVariableIsSet("RWS_SDK_TRACE");
                if(trace) qInfo("RWS planning worker started");
                std::string result;
                try {
                    auto json=inlineJson;
                    auto requestBase=basePath;
                    if(fromFile) {
                        QFile file(requestFile);
                        if(!file.open(QIODevice::ReadOnly) || file.size()>16*1024*1024)
                            throw std::runtime_error("Cannot read planning request or file exceeds 16 MiB.");
                        json=file.readAll().toStdString();
                        requestBase=std::filesystem::u8path(QFileInfo(requestFile).absolutePath().toUtf8().constData());
                    }
                    result=motion_planning::runExternalPlanningJson(snapshot,basePath,json,requestBase,
                        [job](){return job->cancel.load();},[job](const std::string& stage){
                            std::lock_guard<std::mutex> lock(job->mutex);
                            const bool oversized=stage.size()>1024*1024;
                            const auto parsed=oversized?QJsonDocument():QJsonDocument::fromJson(QByteArray::fromStdString(stage));
                            QJsonObject event=parsed.isObject()?parsed.object():QJsonObject{{"type","progress"},
                                {"message",oversized?QStringLiteral("Oversized progress event omitted; full data retained in report"):text(stage)}};
                            event["sequence"]=++job->eventSequence;
                            const auto eventBytes=(oversized?std::size_t(128):stage.size())+128;
                            // Progress is bounded; complete candidate data stays in the report.
                            if(eventBytes<=1024*1024) {
                                job->events.push_back({std::move(event),eventBytes});
                                job->eventBytes+=eventBytes;
                            }
                            while(job->events.size()>2048 || job->eventBytes>8*1024*1024) {
                                job->eventBytes-=job->events.front().bytes;
                                job->events.pop_front();
                            }
                            if(!parsed.isObject()) job->progress=oversized?"Oversized progress event omitted":stage;
                        });
                } catch(const std::exception& e) {
                    result=QJsonDocument(QJsonObject{{"success",false},{"error",QString::fromUtf8(e.what())}}).toJson(QJsonDocument::Compact).toStdString();
                }
                {
                    std::lock_guard<std::mutex> lock(job->mutex);
                    if(job->cancel.load()) result=QJsonDocument(QJsonObject{{"success",false},{"cancelled",true}}).toJson(QJsonDocument::Compact).toStdString();
                    job->result=std::move(result); job->progress="Finished";
                }
                job->done.store(true);
                if(trace) qInfo("RWS planning worker finished");
                job->exited.set_value_at_thread_exit();
            });
        } catch(...) {planningJobs_.erase(id); throw;}
        return {{"job_id",text(id)},{"snapshot_id",job->snapshotId},{"submitted_ms",double(job->submittedMs)},
            {"snapshot_robots",snapshotRobots},{"snapshot_objects",snapshotObjects},{"state","running"}};
    }
    const auto id=request.value("job_id").toString().toStdString();
    const auto found=planningJobs_.find(id);
    if(found==planningJobs_.end()) throw std::runtime_error("Unknown planning job_id.");
    const auto job=found->second;
    if(op=="planning_cancel") {if(!job->done.load()) job->cancel.store(true);}
    else if(op=="planning_forget") {
        if(!job->finished()) throw std::runtime_error("Cancel and wait for completion before forgetting a job.");
        planningJobs_.erase(found); return {{"forgotten",true}};
    } else if(op!="planning_status" && op!="planning_result") throw std::runtime_error("Unknown planning operation.");
    const bool done=job->finished();
    std::lock_guard<std::mutex> lock(job->mutex);
    QJsonObject status{{"job_id",text(id)},{"snapshot_id",job->snapshotId},{"done",done},
        {"cancel_requested",job->cancel.load()},{"progress",text(job->progress)},
        {"state",done?"completed":(job->cancel.load()?"cancelling":"running")}};
    const int after=request.value("after_sequence").toInt(0);
    QJsonArray events;
    std::size_t responseBytes=0;
    for(const auto& e:job->events) {
        if(e.value.value("sequence").toInt()<=after) continue;
        if(events.size()>=16 || (!events.isEmpty() && responseBytes+e.bytes>1024*1024)) break;
        events.append(e.value); responseBytes+=e.bytes;
    }
    status["events"]=events;
    status["latest_sequence"]=job->eventSequence;
    status["first_available_sequence"]=job->events.empty()?job->eventSequence+1:job->events.front().value.value("sequence").toInt();
    if(op=="planning_result" && done && request.contains("result_file")) {
        const auto path=request.value("result_file").toString();
        if(!QDir::isAbsolutePath(path)) throw std::runtime_error("result_file must be absolute");
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(QByteArray::fromStdString(job->result))<0 || !file.commit())
            throw std::runtime_error("Cannot export planning report");
        status["report_file"]=path;
        return status;
    }
    if(op=="planning_result" && done) {
        const auto result=QJsonDocument::fromJson(QByteArray::fromStdString(job->result));
        if(!result.isObject()) throw std::runtime_error("Invalid planning result.");
        status.insert("report",result.object());
    }
    return status;
}

QJsonObject RwsPythonBridge::handleMessage(const QByteArray& bytes)
{
    struct Trace {
        bool enabled=qEnvironmentVariableIsSet("RWS_SDK_TRACE");
        QElapsedTimer timer;
        QString op;
        Trace() {timer.start();}
        ~Trace() {if(enabled) qInfo("RWS SDK end %s: %lld ms",qPrintable(op),timer.elapsed());}
    } trace;
    QJsonObject response{{"ok", false}};
    try {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(bytes, &error);
        if(bytes.size() > 65536 || error.error != QJsonParseError::NoError || !document.isObject())
            throw std::runtime_error("Invalid request JSON");
        const auto request = document.object();
        trace.op=request.value("op").toString();
        if(trace.enabled) qInfo("RWS SDK begin %s",qPrintable(trace.op));
        const auto id = request.value("id").toString();
        response.insert("id", id);
        if(QUuid(id).isNull() || request.value("session").toString() != session_)
            throw std::runtime_error("Request/session mismatch; reconnect client");
        const auto deadline = request.value("deadline_ms");
        if(!deadline.isDouble() || !std::isfinite(deadline.toDouble()) || deadline.toDouble() < QDateTime::currentMSecsSinceEpoch())
            throw std::runtime_error("Request expired without execution");
          response.insert("result", execute(request));
        response.insert("ok", true);
    } catch(const std::exception& error) {
        response.insert("error", QString::fromUtf8(error.what()));
    }
    return response;
}

void RwsPythonBridge::startTcp()
{
    server_ = new QTcpServer(this);
    if(!server_->listen(QHostAddress::LocalHost, 0))
        throw std::runtime_error("RWS SDK TCP listen failed");
    connect(server_, &QTcpServer::newConnection, this, [this] {
        while(server_->hasPendingConnections()) {
            auto* socket = server_->nextPendingConnection();
            socket->setParent(server_);
            socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
            socket->setReadBufferSize(65537);
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                // One outstanding request per client; pipelined commands are rejected,
                // so stale motion commands cannot accumulate in a GUI-thread queue.
                if(!socket->canReadLine()) {
                    if(socket->bytesAvailable() >= 65537) socket->abort();
                    return;
                }
                const auto bytes = socket->readLine(65538);
                if(bytes.size() > 65536 || socket->bytesAvailable() != 0 || socket->bytesToWrite() > 0) {
                    socket->abort();
                    return;
                }
                try {
                    const auto response = handleMessage(bytes);
                    const auto payload = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
                    socket->write(payload);
                    socket->flush();
                } catch(const std::exception& error) {
                    // An allocation/serialization failure must not unwind the GUI event loop.
                    qWarning("RWS TCP response failed: %s",error.what());
                    socket->abort();
                }
            });
        }
    });
}

void RwsPythonBridge::write(const QString& path, const QJsonObject& object)
{
    QSaveFile file(path);
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if(!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("RWS SDK response write failed");
}
QJsonObject RwsPythonBridge::execute(const QJsonObject& request)
{
    if(request.value("op").toString().startsWith("planning_")) return executePlanning(request);
    auto* scene = viewport_->m_scene.get();
    if(!scene) throw std::runtime_error("No scene loaded");
    const auto op = request.value("op").toString();
    if(op == "virtual_baffles") return QJsonDocument::fromJson(QByteArray::fromStdString(scene->virtualBaffleStatus())).object();
    if(op == "segment_trajectory") {
        const auto input=request.value("trajectory_file").toString(), output=request.value("output_directory").toString();
        if(!QDir::isAbsolutePath(input) || !QDir::isAbsolutePath(output)) throw std::runtime_error("Absolute segmentation paths required");
        QFile file(input);
        if(!file.open(QIODevice::ReadOnly) || file.size()>64*1024*1024) throw std::runtime_error("Cannot read segmentation source");
        motion_planning::ZigzagSegmentationOptions options;
        options.directionAngleDegrees=request.value("direction_angle_deg").toDouble(60);
        options.minimumScanLength=request.value("minimum_scan_length_m").toDouble(.005);
        const auto manifest=motion_planning::segmentZigzagTrajectoryJson(file.readAll().toStdString(),
            std::filesystem::u8path(output.toUtf8().constData()),options);
        const auto summary=QJsonDocument::fromJson(QByteArray::fromStdString(manifest)).object();
        return {{"complete",true},{"manifest_file",QDir(output).filePath("manifest.json")},{"segments",summary.value("segments")}};
    }
    if(op == "highlight_trajectory_segment") {
        viewport_->setTrajectoryHighlightRange(request.value("first").toInt(-1),request.value("last").toInt(-1));
        return {{"shown",true}};
    }
    if(op == "ping") return {{"protocol", 1}, {"session", session_}};
    if(op == "preview_joints") {
        const auto id=request.value("robot_id").toString().toStdString();
        if(id==viewport_->m_manualJointControlRobot && RobotViewport::Clock::now()<viewport_->m_manualJointControlExpiry)
            return {{"preview_only",true},{"enabled",false},{"reason","manual_joint_control_active"}};
        std::vector<std::string> names; std::vector<double> values;
        const auto n=request.value("joint_names").toArray(), q=request.value("q_rad").toArray();
        if(!id.empty()) {
            if(n.isEmpty() || n.size()!=q.size()) throw std::runtime_error("Invalid preview joints");
            for(int i=0;i<n.size();++i) {
                double original=0;
                if(!n[i].isString() || !q[i].isDouble() || !std::isfinite(q[i].toDouble()) ||
                   !scene->robotJointValue(id,n[i].toString().toStdString(),original)) throw std::runtime_error("Unknown preview joint");
                names.push_back(n[i].toString().toStdString());values.push_back(q[i].toDouble());
            }
        }
        viewport_->m_sdkPreviewRobot=id;viewport_->m_sdkPreviewNames=names;viewport_->m_sdkPreviewValues=values;
        viewport_->m_sdkPreviewExpiry=RobotViewport::Clock::now()+std::chrono::seconds(2);
        viewport_->update();
        return {{"preview_only",true},{"enabled",!id.empty()}};
    }
    if(op == "export_scene") {
        const auto path=request.value("path").toString();
        if(!QDir::isAbsolutePath(path)) throw std::runtime_error("Absolute output path required");
        auto document=scene->runtimeDocumentSnapshot();
        const auto absolute=[&](std::string& p) {
            if(!p.empty() && std::filesystem::u8path(p).is_relative()) p=(scene->projectBasePath()/std::filesystem::u8path(p)).generic_u8string();
        };
        for(auto& r:document.robots) absolute(r.sourcePath);
        for(auto& o:document.objects) absolute(o.sourcePath);
        for(auto& a:document.attachmentAssets) absolute(a.visualPath);
        std::string error;
        if(!simulation_project::saveProjectDocumentV3(std::filesystem::u8path(path.toUtf8().constData()),document,&error)) throw std::runtime_error(error);
        return {{"path",path},{"saved",true}};
    }
    if(op == "preview_spray_direction") {
        QString error;
        if(!viewport_->previewSprayDirection(request.value("trajectory").toObject(),error))throw std::runtime_error(error.toStdString());
        return {{"shown",true}};
    }
    if(op == "load_trajectory_preview") {
        const auto path=request.value("path").toString();
        if(!QDir::isAbsolutePath(path)) throw std::runtime_error("Absolute trajectory path required");
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly) || file.size()>64*1024*1024) throw std::runtime_error("Cannot read trajectory preview");
        const auto bytes=file.readAll(); const auto json=QJsonDocument::fromJson(bytes);
        if(!json.isObject()) throw std::runtime_error("Invalid trajectory JSON");
        const auto trajectory=json.object();
        QString error;
        if(!viewport_->loadTrajectoryRecord(trajectory,error)) throw std::runtime_error(error.toStdString());
        if(commitSceneChanges && !commitSceneChanges(error)) throw std::runtime_error(error.toStdString());
        return {{"loaded",true},{"point_count",trajectory.value("points").toArray().size()},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())},{"preview_only",true}};
    }
    if(op=="trajectories" || op=="get_trajectory" || op=="show_trajectory" || op=="delete_trajectory") {
        const auto id=request.value("trajectory_id").toString();
        if(op=="show_trajectory") {
            QString error;
            if(!viewport_->showStoredTrajectory(id,error) || (commitSceneChanges && !commitSceneChanges(error)))
                throw std::runtime_error(error.toStdString());
            return {{"shown",true}};
        }
        if(op=="delete_trajectory") {
            QString error;
            if(!viewport_->deleteStoredTrajectory(id,error) || (commitSceneChanges && !commitSceneChanges(error)))
                throw std::runtime_error(error.toStdString());
            return {{"deleted",true},{"trajectory_id",id}};
        }
        const auto document=scene->runtimeDocumentSnapshot();
        QString active;
        for(const auto& ext:document.extensions) if(ext.key=="rws.active_trajectory")
            active=QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object().value("id").toString();
        QJsonArray records;
        for(const auto& ext:document.extensions) if(ext.key.rfind("rws.trajectory.",0)==0) {
            auto record=QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object();
            if(op=="get_trajectory" && record.value("library_id").toString()==id) return record;
            const auto count=record.value("points").toArray().size();
            record.remove("points");record.remove("exports");record["point_count"]=count;record["active"]=record.value("library_id").toString()==active;records.push_back(record);
        }
        if(op=="get_trajectory") throw std::runtime_error("Trajectory not found");
        return {{"trajectories",records}};
    }

    if(op != "robots" && op != "get_joints" && op != "set_joints") return executeScene(request);
    const auto robotId = request.value("robot_id").toString().toStdString();
    const auto& catalog = scene->robotLinks();
    if(op == "robots") {
        QJsonArray robots;
        for(const auto& robot : catalog) {
            QJsonArray links, joints;
            for(const auto& link : robot.links) links.append(QString::fromStdString(link));
            for(const auto& joint : robot.movableJoints)
                joints.append(QJsonObject{{"name", QString::fromStdString(joint.jointName)},
                    {"type", QString::fromStdString(joint.jointType)}});
            robots.append(QJsonObject{{"id", QString::fromStdString(robot.robotId)},
                {"name", QString::fromStdString(robot.robotName)}, {"links", links}, {"joints", joints}});
        }
        return {{"robots", robots}};
    }
    const auto robot = std::find_if(catalog.begin(), catalog.end(),
        [&](const ProjectScene::RobotLinkGroup& r) { return r.robotId == robotId; });
    if(robot == catalog.end()) throw std::runtime_error("Unknown robot_id");
    if(op == "set_joints") {
        const auto values = request.value("values");
        if(!values.isObject() || values.toObject().isEmpty())
            throw std::runtime_error("values must be a nonempty joint-name to number object");
        std::vector<std::string> names;
        std::vector<double> positions;
        const auto object = values.toObject();
        for(auto it = object.begin(); it != object.end(); ++it) {
            const auto name = it.key().toStdString();
            const auto joint = std::find_if(robot->movableJoints.begin(), robot->movableJoints.end(),
                [&](const ProjectScene::RobotJointInfo& j) { return j.jointName == name; });
            if(joint == robot->movableJoints.end() || !it.value().isDouble() || !std::isfinite(it.value().toDouble()))
                throw std::runtime_error("Unknown movable joint or invalid numeric value");
            names.push_back(name);
            positions.push_back(it.value().toDouble());
        }
        if(!viewport_->setRobotJointValues(QString::fromStdString(robotId), names, positions))
            throw std::runtime_error("Joint update rejected; read state before retrying");
    } else if(op != "get_joints") throw std::runtime_error("Unknown operation");
    QJsonArray joints;
    for(const auto& joint : robot->movableJoints) {
        bool ok = false;
        const double value = viewport_->robotJointValue(QString::fromStdString(robotId),
            QString::fromStdString(joint.jointName), &ok);
        if(!ok || !std::isfinite(value)) throw std::runtime_error("Joint readback failed");
        joints.append(QJsonObject{{"name", QString::fromStdString(joint.jointName)},
            {"type", QString::fromStdString(joint.jointType)}, {"value", value}});
    }
    return {{"robot_id", QString::fromStdString(robotId)}, {"angular_unit", "rad"}, {"joints", joints}};
}

QJsonObject RwsPythonBridge::executeScene(const QJsonObject& request)
{
    auto* scene = viewport_->m_scene.get();
    auto document = scene->runtimeDocumentSnapshot();
    const auto op = request.value("op").toString();
    const auto actualObjectJson = [&](const simulation_project::SceneObjectDesc& object) {
        auto result = objectJson(object, document);
        if(robot_qt_viewer::belongsToUserFrame(document,object)) {
            result["parent_frame"]="user";
            result["user_pose"]=poseJson(robot_qt_viewer::coordinatePose(
                robot_qt_viewer::coordinateTransform(robot_qt_viewer::userCoordinateFrame(document)).inverse()*
                robot_qt_viewer::coordinateTransform(object.transform)));
        }
        if(const auto* attachment = robot_qt_viewer::objectAttachment(document, object.id)) {
            simulation_project::TransformDesc visual, tcp;
            if(scene->mountedAttachmentPoses(attachment->id, visual, tcp)) result["pose"] = poseJson(visual);
        }
        return result;
    };
    if(op == "scene_signature") {
        QTemporaryDir temporary;
        if(!temporary.isValid()) throw std::runtime_error("Cannot snapshot scene signature");
        const auto path=temporary.path()+"/scene.sys.json";
        std::string error;
        auto snapshot=scene->runtimeDocumentSnapshot();
        snapshot.extensions.erase(std::remove_if(snapshot.extensions.begin(),snapshot.extensions.end(),[](const auto& e) {
            return e.key.rfind("rws.trajectory.",0)==0 || e.key=="rws.active_trajectory" || e.key=="rws.accepted_trajectory";
        }),snapshot.extensions.end());
        if(!simulation_project::saveProjectDocumentV3(std::filesystem::u8path(path.toUtf8().constData()),snapshot,&error))
            throw std::runtime_error(error);
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read scene signature");
        QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(file.readAll());
        const auto assetStamp=[&](const std::string& source) {
            if(source.empty()) return;
            auto assetPath=std::filesystem::u8path(source);
            if(assetPath.is_relative()) assetPath=scene->projectBasePath()/assetPath;
            const QFileInfo info(QString::fromUtf8(assetPath.generic_u8string().c_str()));
            hash.addData(info.absoluteFilePath().toUtf8());
            hash.addData(QByteArray::number(info.size())); hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
        };
        for(const auto& r:snapshot.robots) assetStamp(r.sourcePath);
        for(const auto& o:snapshot.objects) assetStamp(o.sourcePath);
        for(const auto& a:snapshot.attachmentAssets) assetStamp(a.visualPath);
        return {{"sha256",QString::fromLatin1(hash.result().toHex())},{"session",session_}};
    }
    if(op == "get_user_frame" || op == "set_user_frame") {
        auto pose=robot_qt_viewer::userCoordinateFrame(document);
        if(op == "set_user_frame") {
            if(!request.value("pose").isObject() || request.value("pose").toObject().isEmpty())
                throw std::runtime_error("pose must be a nonempty object");
            const auto input=request.value("pose").toObject();
            for(auto it=input.begin();it!=input.end();++it) {
                if(!it.value().isDouble() || !std::isfinite(it.value().toDouble()))
                    throw std::runtime_error("Pose values must be finite numbers");
                const auto key=it.key();
                double* field=key=="x"?&pose.x:key=="y"?&pose.y:key=="z"?&pose.z:
                    key=="roll"?&pose.roll:key=="pitch"?&pose.pitch:key=="yaw"?&pose.yaw:nullptr;
                if(!field) throw std::runtime_error("Unknown pose component");
                *field=it.value().toDouble();
            }
            QString error;
            if(!setUserFrame || !setUserFrame(pose,error))
                throw std::runtime_error(error.isEmpty()?"User frame update unavailable":error.toStdString());
        }
        return {{"id","user"},{"frame","world"},{"pose",poseJson(pose)},
            {"linear_unit","m"},{"angular_unit","rad"}};
    }
    if(op == "objects" || op == "workpieces") {
        if(request.contains("include_tools") && !request.value("include_tools").isBool())
            throw std::runtime_error("include_tools must be boolean");
        const bool includeTools = request.value("include_tools").toBool(false);
        QJsonArray objects;
        for(const auto& object : document.objects) {
            const auto role = robot_qt_viewer::sceneObjectRole(document, object);
            if(op == "workpieces" ? role != "workpiece" : (!includeTools && role == "tool")) continue;
            objects.append(actualObjectJson(object));
        }
        return {{op == "workpieces" ? "workpieces" : "objects", objects}};
    }
    if(op == "tools") {
        QJsonArray tools;
        for(const auto& attachment : document.mountedAttachments) {
            const auto asset = std::find_if(document.attachmentAssets.begin(), document.attachmentAssets.end(),
                [&](const auto& value) { return value.id == attachment.assetId; });
            if(asset == document.attachmentAssets.end() || robot_qt_viewer::attachmentRole(*asset) != "tool") continue;
            QJsonObject tool{{"id", text(attachment.id)}, {"name", text(attachment.name)}, {"role", "tool"},
                {"is_workpiece", false}, {"asset_id", text(asset->id)}, {"source_object_id", text(attachment.sourceObjectId)},
                {"mount_id", text(attachment.mountFrameId)}, {"enabled", attachment.enabled}, {"visible", attachment.visible},
                {"has_tcp", false},
                {"linear_unit", "m"}, {"angular_unit", "rad"}, {"frame", "world"}};
            for(const auto& mount : document.robotMounts) if(mount.id == attachment.mountFrameId) {
                tool["owner_robot_id"] = text(mount.robotId); tool["parent_link"] = text(mount.linkName);
            }
            const simulation_project::AttachmentFunctionalFrameDesc* selectedTcp=nullptr;
            int tcpCount=0, primaryCount=0;
            for(const auto& frame:asset->functionalFrames) if(frame.frameType=="tcp") {
                ++tcpCount; if(!selectedTcp || frame.primary) selectedTcp=&frame;
                if(frame.primary) ++primaryCount;
            }
            if(selectedTcp && primaryCount<=1 && (tcpCount==1 || primaryCount==1)) {
                tool["has_tcp"]=true; tool["tcp_asset_local"]=poseJson(selectedTcp->assetMountToFrame);
            }
            simulation_project::TransformDesc visual, tcp;
            if(scene->mountedAttachmentPoses(attachment.id, visual, tcp)) {
                tool["pose"] = poseJson(visual); tool["tcp_world"] = poseJson(tcp);
            }
            tools.append(tool);
        }
        return {{"tools", tools}};
    }
    if(op == "get_object_pose" || op == "set_object_pose") {
        const auto id = request.value("object_id").toString().toStdString();
        auto object = std::find_if(document.objects.begin(), document.objects.end(),
            [&](const simulation_project::SceneObjectDesc& o) { return o.id == id; });
        if(object == document.objects.end()) throw std::runtime_error("Unknown object_id");
        const auto reference=request.value("frame").toString("world");
        if(reference!="world" && reference!="user") throw std::runtime_error("frame must be world or user");
        if(reference=="user" && !robot_qt_viewer::belongsToUserFrame(document,*object))
            throw std::runtime_error("Object does not belong to user frame");
        const auto worldFromUser=robot_qt_viewer::coordinateTransform(robot_qt_viewer::userCoordinateFrame(document));
        if(op == "set_object_pose") {
            if(robot_qt_viewer::objectAttachment(document, id))
                throw std::runtime_error("Object is robot-mounted; change the robot joints or attachment offset instead");
            if(!request.value("pose").isObject()) throw std::runtime_error("pose must be an object");
            const auto input = request.value("pose").toObject();
            if(input.isEmpty()) throw std::runtime_error("pose must not be empty");
            auto pose = object->transform;
            if(reference=="user") pose=robot_qt_viewer::coordinatePose(worldFromUser.inverse()*robot_qt_viewer::coordinateTransform(pose));
            for(auto it = input.begin(); it != input.end(); ++it) {
                if(!it.value().isDouble() || !std::isfinite(it.value().toDouble()))
                    throw std::runtime_error("Pose values must be finite numbers");
                const auto key = it.key();
                double* target = key == "x" ? &pose.x : key == "y" ? &pose.y : key == "z" ? &pose.z :
                    key == "roll" ? &pose.roll : key == "pitch" ? &pose.pitch : key == "yaw" ? &pose.yaw : nullptr;
                if(!target) throw std::runtime_error("Unknown pose component");
                *target = it.value().toDouble();
            }
            if(reference=="user") pose=robot_qt_viewer::coordinatePose(worldFromUser*robot_qt_viewer::coordinateTransform(pose));
            if(!viewport_->setSceneObjectTransform(text(id), pose)) throw std::runtime_error("Object pose update rejected");
            const auto actual = scene->runtimeDocumentSnapshot();
            for(const auto& o : actual.objects) if(o.id == id) {
                auto result=objectJson(o,actual);
                if(reference=="user") result["pose"]=poseJson(robot_qt_viewer::coordinatePose(worldFromUser.inverse()*robot_qt_viewer::coordinateTransform(o.transform)));
                result["frame"]=reference;
                return result;
            }
            throw std::runtime_error("Object disappeared after update");
        }
        auto result=actualObjectJson(*object);
        if(reference=="user") { result["pose"]=result["user_pose"]; result["frame"]="user"; }
        return result;
    }
    if(op == "set_collision_enabled") {
        const bool enabled = requiredBool(request, "enabled");
        viewport_->setCollisionQueriesEnabled(enabled);
        if(viewport_->collisionQueriesEnabled() != enabled) throw std::runtime_error("Collision runtime unavailable");
        return {{"enabled", enabled}};
    }
    if(op == "configure_detector") {
        const auto id = request.value("detector_id").toString().toStdString();
        if(id.empty()) throw std::runtime_error("detector_id is required");
        simulation_project::CollisionSelectionSetDesc a, b;
        a.id = id + "_A"; b.id = id + "_B";
        a.name = a.id; b.name = b.id;
        const auto parse = [&](const char* key, simulation_project::CollisionSelectionSetDesc& set) {
            if(!request.value(key).isArray() || request.value(key).toArray().isEmpty())
                throw std::runtime_error(std::string(key) + " must be a nonempty endpoint array");
            for(const auto& value : request.value(key).toArray()) {
                if(!value.isObject()) throw std::runtime_error("Endpoint must be an object");
                const auto e = value.toObject();
                simulation_project::CollisionSelectionSetMemberDesc member;
                member.robotId = e.value("robot_id").toString().toStdString();
                member.linkName = e.value("link_name").toString().toStdString();
                member.objectId = e.value("object_id").toString().toStdString();
                member.attachmentId = e.value("attachment_id").toString().toStdString();
                if(!member.attachmentId.empty()) {
                    if(e.size() != 1) throw std::runtime_error("Use attachment_id alone");
                    auto attachment = std::find_if(document.mountedAttachments.begin(), document.mountedAttachments.end(),
                        [&](const auto& value) { return value.id == member.attachmentId; });
                    if(attachment == document.mountedAttachments.end()) throw std::runtime_error("Unknown attachment endpoint");
                    if(!attachment->enabled) throw std::runtime_error("Attachment endpoint is disabled");
                    const auto asset = std::find_if(document.attachmentAssets.begin(), document.attachmentAssets.end(),
                        [&](const auto& value) { return value.id == attachment->assetId; });
                    if(asset == document.attachmentAssets.end()) throw std::runtime_error("Attachment asset not found");
                    set.members.push_back(member);
                    continue;
                }
                if(e.size() != (member.robotId.empty() ? 1 : 2) ||
                    (member.robotId.empty() == member.objectId.empty())) throw std::runtime_error("Use robot_id + link_name OR object_id");
                if(!member.robotId.empty()) {
                    const auto& catalog = scene->robotLinks();
                    const auto r = std::find_if(catalog.begin(), catalog.end(), [&](const ProjectScene::RobotLinkGroup& r) { return r.robotId == member.robotId; });
                    if(r == catalog.end() || std::find(r->links.begin(), r->links.end(), member.linkName) == r->links.end())
                        throw std::runtime_error("Unknown robot/link endpoint");
                    for(auto& desc : document.robots) if(desc.id == member.robotId) desc.collisionEnabled = true;
                } else {
                    auto o = std::find_if(document.objects.begin(), document.objects.end(), [&](const simulation_project::SceneObjectDesc& o) { return o.id == member.objectId; });
                    if(o == document.objects.end()) throw std::runtime_error("Unknown object endpoint");
                    o->collisionEnabled = true;
                }
                set.members.push_back(member);
            }
        };
        parse("endpoints_a", a); parse("endpoints_b", b);
        // Do not overwrite selection sets owned by an unrelated detector.
        for(const auto& detector : document.collision.detectors)
            if(detector.id != id && (detector.selectionSetAId == a.id || detector.selectionSetBId == a.id ||
                detector.selectionSetAId == b.id || detector.selectionSetBId == b.id))
                throw std::runtime_error("Selection set ID belongs to another detector");
        for(const auto& set : {a, b}) {
            auto it = std::find_if(document.collision.selectionSets.begin(), document.collision.selectionSets.end(),
                [&](const simulation_project::CollisionSelectionSetDesc& s) { return s.id == set.id; });
            if(it == document.collision.selectionSets.end()) document.collision.selectionSets.push_back(set);
            else *it = set;
        }
        simulation_project::CollisionDetectorDesc detector;
        detector.id = id; detector.name = id; detector.type = "SelectedObjects";
        detector.queryPolicy = "BetweenSets"; detector.selectionSetAId = a.id; detector.selectionSetBId = b.id;
        detector.enabled = requiredBool(request, "enabled");
        detector.distance = requiredBool(request, "distance"); detector.nearestPoints = detector.distance;
        detector.visualization.showNearestPoints = detector.distance;
        auto existing = std::find_if(document.collision.detectors.begin(), document.collision.detectors.end(),
            [&](const simulation_project::CollisionDetectorDesc& d) { return d.id == id; });
        if(existing == document.collision.detectors.end()) document.collision.detectors.push_back(detector);
        else *existing = detector;
        document.collision.query.enabled = true;
        if(!viewport_->rebuildCollisionDetectorsFromDocument(document)) throw std::runtime_error("Detector rebuild failed; inspect runtime before retrying");
        return {{"detectors", detectorsJson(viewport_)}, {"detector_id", text(id)}};
    }
    if(op == "set_detector_options") {
        const auto id = request.value("detector_id").toString().toStdString();
        auto detector = std::find_if(document.collision.detectors.begin(), document.collision.detectors.end(),
            [&](const simulation_project::CollisionDetectorDesc& d) { return d.id == id; });
        if(detector == document.collision.detectors.end()) throw std::runtime_error("Unknown detector_id");
        if(request.contains("enabled")) detector->enabled = requiredBool(request, "enabled");
        if(request.contains("distance")) {
            detector->distance = requiredBool(request, "distance");
            detector->nearestPoints = detector->distance;
        }
        if(!viewport_->rebuildCollisionDetectorsFromDocument(document)) throw std::runtime_error("Detector rebuild failed");
    } else if(op == "query_collision") {
        const auto id = request.value("detector_id").toString();
        if(!viewport_->collisionQueriesEnabled()) throw std::runtime_error("Global collision queries are disabled");
        if(!viewport_->refreshCollisionDetectorNearest(id)) throw std::runtime_error("Collision query failed; inspect detector state");
    } else if(op != "detectors") throw std::runtime_error("Unknown operation");
    auto baffles=QJsonDocument::fromJson(QByteArray::fromStdString(scene->virtualBaffleStatus())).object();
    auto detectors=detectorsJson(viewport_);
    for(int i=0;i<detectors.size();++i){auto d=detectors[i].toObject();d["selected_pairs_in_collision"]=d["in_collision"];d["virtual_baffle_collision"]=baffles.value("in_collision");
        if(baffles.value("in_collision").toBool()){d["in_collision"]=true;d["min_distance"]=QJsonValue();d["constraint_diagnostic"]=baffles.value("state_diagnostic");}
        if(!baffles.value("error").toString().isEmpty()){d["valid"]=false;d["error"]=baffles.value("error");}detectors[i]=d;}
    return {{"enabled", viewport_->collisionQueriesEnabled()}, {"detectors", detectors},{"virtual_baffles",baffles}};
}
void RwsPythonBridge::poll() noexcept
{
    const auto files = QDir(directory_).entryList({"*.request.json"}, QDir::Files, QDir::Name);
    int count = 0;
    for(const auto& name : files) {
        if(++count > 8) break;
        const QString id = name.left(name.size() - QString(".request.json").size());
        if(QUuid(id).isNull()) continue;
        const QString path = directory_ + '/' + name;
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) continue;
        const auto bytes = file.read(65537);
        file.close();
        // Claim once before execution: no repeated mutation after a write failure.
        if(!QFile::remove(path)) continue;
        QJsonObject response{{"id", id}, {"ok", false}};
        try {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(bytes, &error);
            if(bytes.size() > 65536 || error.error != QJsonParseError::NoError || !document.isObject())
                throw std::runtime_error("Invalid request JSON");
            const auto request = document.object();
            if(request.value("id").toString() != id || request.value("session").toString() != session_)
                throw std::runtime_error("Request/session mismatch; reconnect client");
            const double deadline = request.value("deadline_ms").toDouble();
            if(!std::isfinite(deadline) || deadline < QDateTime::currentMSecsSinceEpoch())
                throw std::runtime_error("Request expired without execution");
            response.insert("result", execute(request));
            response.insert("ok", true);
        } catch(const std::exception& error) {
            response.insert("error", QString::fromUtf8(error.what()));
        }
        try { write(directory_ + '/' + id + ".response.json", response); }
        catch(...) { /* client times out; never replay the command */ }
    }
}
