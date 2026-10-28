#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <map>
#include <string>
#include <functional>
#include <SimulationProject/ProjectDocument.h>

class RobotViewport;
class QLockFile;
class QJsonObject;
class QTcpServer;

// GUI commands and snapshot capture run on the GUI thread; planning uses private workers.
class RwsPythonBridge : public QObject
{
public:
    RwsPythonBridge(RobotViewport* viewport, const QString& directory, QObject* parent);
    ~RwsPythonBridge() override;
    std::function<bool(const simulation_project::TransformDesc&,QString&)> setUserFrame;
    std::function<bool(QString&)> commitSceneChanges;

private:
    static void write(const QString& path, const QJsonObject& object);
    QJsonObject execute(const QJsonObject& request);
    QJsonObject executeScene(const QJsonObject& request);
    QJsonObject executePlanning(const QJsonObject& request);
    struct PlanningJob;
    std::map<std::string,std::shared_ptr<PlanningJob>> planningJobs_;
    void poll() noexcept;
    QJsonObject handleMessage(const QByteArray& bytes);
    void startTcp();
    RobotViewport* viewport_;
    QString directory_;
    std::unique_ptr<QLockFile> lock_;
    QString session_;
    QTcpServer* server_ = nullptr;
};
