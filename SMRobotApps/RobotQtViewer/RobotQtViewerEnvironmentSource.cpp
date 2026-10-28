#include "RobotQtViewerEnvironmentController.h"
#include <SimulationProject/RuntimePaths.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace robot_qt_viewer
{
bool readEnvironmentSelection(const QString& source,
    ProjectSceneEnvironmentSelection& selection, QString& error)
{
    selection = ProjectSceneEnvironmentSelection{};
    error.clear();
    if(parseProjectSceneEnvironmentPreset(source.toStdString(), selection.preset)) {
        selection.source = source.toStdString();
        return true;
    }
    QString path = source;
    if(source == "factory-detailed") path = QString::fromStdWString(
        (simulation_project::RuntimePaths::configRoot() / "environments/factory-detailed.environment.json").wstring());
    const QFileInfo info(path);
    selection.source = source.toUtf8().toStdString();
    selection.id = source == "factory-detailed" ? "factory-detailed" : "custom";
    if(info.suffix().compare("gltf", Qt::CaseInsensitive) == 0) {
        selection.parts.push_back({"model", info.absoluteFilePath().toUtf8().toStdString()});
        selection.cutaway = false;
    } else {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) { error = "Cannot open environment descriptor: " + path; return false; }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        const auto object = document.object();
        if(parseError.error != QJsonParseError::NoError || !document.isObject() ||
            object.value("schema").toString() != "smrobot.presentation-environment" ||
            object.value("version").toDouble(-1) != 1.0 || object.value("units").toString() != "meters" ||
            !object.value("parts").isArray() || object.value("parts").toArray().isEmpty()) {
            error = "Invalid environment descriptor schema/version/units/parts: " + path; return false;
        }
        for(const QString& key : {QString("scale"), QString("groundHeight")}) {
            if(object.contains(key) && !object.value(key).isDouble()) { error = "Expected number: " + key; return false; }
        }
        for(const QString& key : {QString("hasGround"), QString("showGrid")}) {
            if(object.contains(key) && !object.value(key).isBool()) { error = "Expected boolean: " + key; return false; }
        }
        selection.scale = static_cast<float>(object.value("scale").toDouble(1));
        selection.groundHeight = static_cast<float>(object.value("groundHeight").toDouble(0));
        selection.hasGround = object.value("hasGround").toBool(false);
        selection.showGrid = object.value("showGrid").toBool(false);
        for(const auto& value : object.value("parts").toArray()) {
            const auto partObject = value.toObject();
            ProjectSceneEnvironmentPart part;
            part.id = partObject.value("id").toString().toStdString();
            const QString reference = partObject.value("source").toString();
            const QString role = partObject.value("role").toString();
            if(role == "floor") part.role = ProjectSceneEnvironmentPartRole::Floor;
            else if(role == "wall") part.role = ProjectSceneEnvironmentPartRole::Wall;
            else if(role == "roof") part.role = ProjectSceneEnvironmentPartRole::Roof;
            else if(role != "solid") { error = "Unknown environment part role."; return false; }
            if(reference.isEmpty()) { error = "Missing environment model source."; return false; }
            if(reference.startsWith("data://")) part.source = reference.toStdString();
            else part.source = QDir(info.absolutePath()).absoluteFilePath(reference).toUtf8().toStdString();
            if(part.role == ProjectSceneEnvironmentPartRole::Wall) {
                const auto normal = partObject.value("normal").toArray();
                if(normal.size() != 3 || !normal[0].isDouble() || !normal[1].isDouble() ||
                    !normal[2].isDouble() || !partObject.value("offset").isDouble()) {
                    error = "Wall requires a numeric plane."; return false;
                }
                part.wallNormal = {static_cast<float>(normal[0].toDouble()), static_cast<float>(normal[1].toDouble()), static_cast<float>(normal[2].toDouble())};
                part.wallOffset = static_cast<float>(partObject.value("offset").toDouble());
            }
            selection.parts.push_back(std::move(part));
        }
    }
    std::string detail;
    if(!validateProjectSceneEnvironment(selection, detail)) { error = QString::fromStdString(detail); return false; }
    return true;
}

}
