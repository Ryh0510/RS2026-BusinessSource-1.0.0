#include "ToolSetupTaskSupport.h"

#include "RobotQtWidgetUtils.h"

#include <Eigen/Geometry>

#include <QByteArray>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cctype>

namespace robot_qt_viewer::tool_setup_detail
{
    namespace
    {
        bool nearlyEqual(double lhs, double rhs)
        {
            return std::abs(lhs - rhs) <= 1.0e-9;
        }

        bool sameTransform(
            const simulation_project::TransformDesc& lhs,
            const simulation_project::TransformDesc& rhs)
        {
            return nearlyEqual(lhs.x, rhs.x) &&
                nearlyEqual(lhs.y, rhs.y) &&
                nearlyEqual(lhs.z, rhs.z) &&
                nearlyEqual(lhs.roll, rhs.roll) &&
                nearlyEqual(lhs.pitch, rhs.pitch) &&
                nearlyEqual(lhs.yaw, rhs.yaw);
        }

        Eigen::Isometry3d makeTransform(const simulation_project::TransformDesc& desc)
        {
            Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
            transform.translation() = Eigen::Vector3d(desc.x, desc.y, desc.z);
            transform.linear() =
                Eigen::AngleAxisd(desc.yaw, Eigen::Vector3d::UnitZ()).toRotationMatrix() *
                Eigen::AngleAxisd(desc.pitch, Eigen::Vector3d::UnitY()).toRotationMatrix() *
                Eigen::AngleAxisd(desc.roll, Eigen::Vector3d::UnitX()).toRotationMatrix();
            return transform;
        }

        simulation_project::TransformDesc makeTransformDesc(const Eigen::Isometry3d& transform)
        {
            simulation_project::TransformDesc desc;
            desc.x = transform.translation().x();
            desc.y = transform.translation().y();
            desc.z = transform.translation().z();
            const Eigen::Vector3d euler = transform.linear().eulerAngles(2, 1, 0);
            desc.yaw = euler[0];
            desc.pitch = euler[1];
            desc.roll = euler[2];
            return desc;
        }

        std::string trimUnderscores(std::string value)
        {
            while(!value.empty() && value.front() == '_') {
                value.erase(value.begin());
            }
            while(!value.empty() && value.back() == '_') {
                value.pop_back();
            }
            return value;
        }
    }

    EntitySelectionDialog::EntitySelectionDialog(
        const QString& title,
        const QString& fieldLabel,
        const QVector<QPair<QString, QString>>& items,
        const QString& selectedId,
        QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(title);
        setMinimumWidth(520);
        auto* root = new QVBoxLayout(this);
        auto* form = new QFormLayout();
        m_combo = new QComboBox(this);
        configureInspectorEntityCombo(m_combo, 24);
        for(const auto& item : items) {
            m_combo->addItem(item.first, item.second);
            m_combo->setItemData(m_combo->count() - 1, item.first, Qt::ToolTipRole);
        }
        const int selectedIndex = m_combo->findData(selectedId);
        if(selectedIndex >= 0) {
            m_combo->setCurrentIndex(selectedIndex);
        }
        form->addRow(fieldLabel, m_combo);
        root->addLayout(form);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        configureDialogButtonBox(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
    }

    QString EntitySelectionDialog::selectedId() const
    {
        return m_combo != nullptr ? m_combo->currentData().toString() : QString();
    }

    const simulation_project::RobotMountDesc* findRobotMountDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& mountId)
    {
        const auto it = std::find_if(
            document.robotMounts.begin(),
            document.robotMounts.end(),
            [&](const simulation_project::RobotMountDesc& mount) {
                return mount.id == mountId;
            });
        return it == document.robotMounts.end() ? nullptr : &(*it);
    }

    const simulation_project::MountedAttachmentDesc* findMountedAttachmentDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& attachmentId)
    {
        const auto it = std::find_if(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& attachment) {
                return attachment.id == attachmentId;
            });
        return it == document.mountedAttachments.end() ? nullptr : &(*it);
    }

    const simulation_project::RobotDesc* findRobotDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& robotId)
    {
        const auto it = std::find_if(
            document.robots.begin(),
            document.robots.end(),
            [&](const simulation_project::RobotDesc& robot) {
                return robot.id == robotId;
            });
        return it == document.robots.end() ? nullptr : &(*it);
    }

    const simulation_project::SceneObjectDesc* findSceneObjectDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& objectId)
    {
        const auto it = std::find_if(
            document.objects.begin(),
            document.objects.end(),
            [&](const simulation_project::SceneObjectDesc& object) {
                return object.id == objectId;
            });
        return it == document.objects.end() ? nullptr : &(*it);
    }

    const simulation_project::AttachmentAssetDesc* findAttachmentAssetDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& assetId)
    {
        const auto it = std::find_if(
            document.attachmentAssets.begin(),
            document.attachmentAssets.end(),
            [&](const simulation_project::AttachmentAssetDesc& asset) {
                return asset.id == assetId;
            });
        return it == document.attachmentAssets.end() ? nullptr : &(*it);
    }

    const simulation_project::MountedAttachmentDesc* findMountedAttachmentByAssetId(
        const simulation_project::ProjectDocument& document,
        const std::string& assetId)
    {
        const auto it = std::find_if(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& attachment) {
                return attachment.assetId == assetId;
            });
        return it == document.mountedAttachments.end() ? nullptr : &(*it);
    }

    const simulation_project::MountedAttachmentDesc* findMountedAttachmentByMountId(
        const simulation_project::ProjectDocument& document,
        const std::string& mountId)
    {
        const auto it = std::find_if(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& attachment) {
                return attachment.mountFrameId == mountId;
            });
        return it == document.mountedAttachments.end() ? nullptr : &(*it);
    }

    const simulation_project::ObjectFrameDesc* findObjectFrameDesc(
        const simulation_project::SceneObjectDesc& object,
        const std::string& frameId)
    {
        const auto it = std::find_if(
            object.objectFrames.begin(),
            object.objectFrames.end(),
            [&](const simulation_project::ObjectFrameDesc& frame) {
                return frame.id == frameId;
            });
        return it == object.objectFrames.end() ? nullptr : &(*it);
    }

    bool sameRobotMount(
        const simulation_project::RobotMountDesc& lhs,
        const simulation_project::RobotMountDesc& rhs)
    {
        return lhs.id == rhs.id &&
            lhs.name == rhs.name &&
            lhs.robotId == rhs.robotId &&
            lhs.linkName == rhs.linkName &&
            sameTransform(lhs.linkToMount, rhs.linkToMount);
    }

    std::string findHiddenSceneObjectIdBySourcePath(
        const simulation_project::ProjectDocument& document,
        const std::string& sourcePath)
    {
        if(sourcePath.empty()) {
            return std::string();
        }
        const auto it = std::find_if(
            document.objects.begin(),
            document.objects.end(),
            [&](const simulation_project::SceneObjectDesc& object) {
                return !object.visible && object.sourcePath == sourcePath;
            });
        return it == document.objects.end() ? std::string() : it->id;
    }

    simulation_project::TransformDesc inverseTransform(
        const simulation_project::TransformDesc& transform)
    {
        return makeTransformDesc(makeTransform(transform).inverse());
    }

    QString formatTransformMatrix(const simulation_project::TransformDesc& transform)
    {
        const Eigen::Matrix4d matrix = makeTransform(transform).matrix();
        QStringList rows;
        for(int row = 0; row < 4; ++row) {
            QStringList values;
            for(int column = 0; column < 4; ++column) {
                values.push_back(QString::number(matrix(row, column), 'f', 3));
            }
            rows.push_back(QStringLiteral("[ %1 ]").arg(values.join(QStringLiteral("  "))));
        }
        return rows.join(QLatin1Char('\n'));
    }

    ToolSetupBindingView makeBindingView(
        const simulation_project::ProjectDocument& document,
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        ToolSetupBindingView view;
        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(document, mountId.toStdString());
        const simulation_project::SceneObjectDesc* object =
            findSceneObjectDesc(document, objectId.toStdString());
        view.visible = mount != nullptr;
        if(mount != nullptr) {
            view.mountLinkName = QString::fromStdString(mount->linkName);
            view.mountFrameName = QString::fromStdString(mount->name.empty() ? mount->id : mount->name);
            view.mountTransformText = formatTransformMatrix(mount->linkToMount);
        }
        if(object != nullptr) {
            view.hasObjectBinding = true;
            const std::string objectName = object->name.empty() ? object->id : object->name;
            const std::string mountName =
                mount != nullptr ? (mount->name.empty() ? mount->id : mount->name) : std::string();
            view.objectName = QString::fromStdString(objectName);
            view.bindingName = QString::fromStdString(
                objectName.empty()
                    ? (mountName.empty() ? std::string("Attachment") : mountName + " Attachment")
                    : objectName);
            if(frameId.isEmpty()) {
                view.objectFrameName = QStringLiteral("Object origin");
                view.objectFrameInverseTransformText =
                    formatTransformMatrix(simulation_project::TransformDesc());
            } else {
                const simulation_project::ObjectFrameDesc* frame =
                    findObjectFrameDesc(*object, frameId.toStdString());
                if(frame != nullptr) {
                    view.objectFrameName = QString::fromStdString(frame->name.empty() ? frame->id : frame->name);
                    view.objectFrameInverseTransformText = formatTransformMatrix(inverseTransform(frame->objectToFrame));
                }
            }
        } else {
            view.bindingName = QStringLiteral("Pending binding");
            view.hasObjectBinding = false;
            view.objectFrameName = QStringLiteral("Object Frame");
            view.objectName = QStringLiteral("Object");
            view.objectFrameInverseTransformText =
                formatTransformMatrix(simulation_project::TransformDesc());
        }
        return view;
    }

    std::string qStringToUtf8(const QString& text)
    {
        const QByteArray bytes = text.toUtf8();
        return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
    }

    std::string makeAsciiSlug(
        const std::string& displayName,
        const std::string& fallbackPrefix,
        std::size_t maxLength)
    {
        std::string slug;
        slug.reserve(displayName.size());
        bool lastWasSeparator = false;
        for(const unsigned char ch : displayName) {
            if(ch < 128 && std::isalnum(ch)) {
                slug.push_back(static_cast<char>(std::tolower(ch)));
                lastWasSeparator = false;
            } else if(ch < 128 && (ch == '_' || ch == '-' || ch == '.' || std::isspace(ch))) {
                if(!slug.empty() && !lastWasSeparator) {
                    slug.push_back('_');
                    lastWasSeparator = true;
                }
            }
        }

        slug = trimUnderscores(slug);
        if(slug.empty()) {
            slug = fallbackPrefix;
        }
        if(slug.size() > maxLength) {
            slug.resize(maxLength);
            slug = trimUnderscores(slug);
        }
        return slug.empty() ? fallbackPrefix : slug;
    }

    std::string makeToolAssetDisplayName(
        const std::string& sourceName,
        const std::string& fallbackName)
    {
        return sourceName.empty() ? fallbackName : sourceName;
    }

    std::string makeAttachmentDisplayName(
        const std::string& assetName,
        const std::string& mountName)
    {
        if(!assetName.empty()) {
            return assetName;
        }
        return mountName.empty() ? std::string("Attachment") : mountName + " Attachment";
    }

    QString conciseUiText(const std::string& value, int maxCharacters)
    {
        QString text = QString::fromStdString(value);
        if(maxCharacters <= 8 || text.size() <= maxCharacters) {
            return text;
        }

        const int headCount = (maxCharacters - 3) / 2;
        const int tailCount = maxCharacters - 3 - headCount;
        return text.left(headCount) + "..." + text.right(tailCount);
    }

    simulation_project::MountedAttachmentDesc makeMountedAttachmentMirror(
        const simulation_project::MountedAttachmentDesc& attachment)
    {
        simulation_project::MountedAttachmentDesc value;
        value.id = attachment.id;
        value.name = attachment.name;
        value.mountFrameId = attachment.mountFrameId;
        value.assetId = attachment.assetId;
        value.sourceObjectId = attachment.sourceObjectId;
        value.sourceObjectFrameId = attachment.sourceObjectFrameId;
        value.mountToAssetMount = attachment.mountToAssetMount;
        value.visible = attachment.visible;
        value.enabled = attachment.enabled;
        return value;
    }

    ToolSetupComboItem makeComboItem(const QString& text, const QString& id)
    {
        ToolSetupComboItem item;
        item.text = text;
        item.id = id;
        return item;
    }
}
