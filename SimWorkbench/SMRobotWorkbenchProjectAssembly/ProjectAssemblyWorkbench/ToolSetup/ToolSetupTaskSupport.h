#pragma once

#include "ToolSetupViewModel.h"

#include <SimulationProject/ProjectDocument.h>

#include <QDialog>
#include <QPair>
#include <QString>
#include <QVector>

#include <cstddef>
#include <string>

class QComboBox;
class QWidget;

namespace robot_qt_viewer::tool_setup_detail
{
    class EntitySelectionDialog final : public QDialog
    {
    public:
        EntitySelectionDialog(
            const QString& title,
            const QString& fieldLabel,
            const QVector<QPair<QString, QString>>& items,
            const QString& selectedId,
            QWidget* parent);

        QString selectedId() const;

    private:
        QComboBox* m_combo = nullptr;
    };

    const simulation_project::RobotMountDesc* findRobotMountDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& mountId);
    const simulation_project::MountedAttachmentDesc* findMountedAttachmentDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& attachmentId);
    const simulation_project::RobotDesc* findRobotDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& robotId);
    const simulation_project::SceneObjectDesc* findSceneObjectDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& objectId);
    const simulation_project::AttachmentAssetDesc* findAttachmentAssetDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& assetId);
    const simulation_project::MountedAttachmentDesc* findMountedAttachmentByAssetId(
        const simulation_project::ProjectDocument& document,
        const std::string& assetId);
    const simulation_project::MountedAttachmentDesc* findMountedAttachmentByMountId(
        const simulation_project::ProjectDocument& document,
        const std::string& mountId);
    const simulation_project::ObjectFrameDesc* findObjectFrameDesc(
        const simulation_project::SceneObjectDesc& object,
        const std::string& frameId);

    bool sameRobotMount(
        const simulation_project::RobotMountDesc& lhs,
        const simulation_project::RobotMountDesc& rhs);
    std::string findHiddenSceneObjectIdBySourcePath(
        const simulation_project::ProjectDocument& document,
        const std::string& sourcePath);
    simulation_project::TransformDesc inverseTransform(
        const simulation_project::TransformDesc& transform);
    QString formatTransformMatrix(const simulation_project::TransformDesc& transform);
    ToolSetupBindingView makeBindingView(
        const simulation_project::ProjectDocument& document,
        const QString& mountId,
        const QString& objectId,
        const QString& frameId);

    std::string qStringToUtf8(const QString& text);
    std::string makeAsciiSlug(
        const std::string& displayName,
        const std::string& fallbackPrefix,
        std::size_t maxLength);
    std::string makeToolAssetDisplayName(
        const std::string& sourceName,
        const std::string& fallbackName);
    std::string makeAttachmentDisplayName(
        const std::string& assetName,
        const std::string& mountName);
    QString conciseUiText(const std::string& value, int maxCharacters);
    simulation_project::MountedAttachmentDesc makeMountedAttachmentMirror(
        const simulation_project::MountedAttachmentDesc& attachment);
    ToolSetupComboItem makeComboItem(const QString& text, const QString& id);
}
