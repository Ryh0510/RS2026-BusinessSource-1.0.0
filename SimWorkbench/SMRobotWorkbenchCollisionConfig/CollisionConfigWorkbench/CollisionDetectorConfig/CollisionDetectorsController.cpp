#include "CollisionDetectorsController.h"

#include <algorithm>
#include <QStringList>
#include <unordered_map>
#include <utility>

namespace
{
    QString bindingTargetLabel(const CollisionDetectorModelBindingView& binding)
    {
        if(!binding.robotId.isEmpty()) {
            return QString("link: %1.%2").arg(binding.robotId, binding.linkName);
        }
        if(!binding.attachmentId.isEmpty()) {
            return QString("attachment: %1").arg(binding.attachmentId);
        }
        if(!binding.pointCloudId.isEmpty()) {
            return QString("point cloud: %1").arg(binding.pointCloudId);
        }
        return QString("object: %1").arg(binding.objectId);
    }

    QString bindingKey(const CollisionDetectorModelBindingView& binding)
    {
        return binding.context + "\n" + binding.robotId + "\n" + binding.linkName + "\n" +
            binding.objectId + "\n" + binding.attachmentId + "\n" + binding.pointCloudId;
    }

    CollisionDetectorModelBindingView bindingView(
        const simulation_project::CollisionDetectorModelBindingDesc& binding)
    {
        CollisionDetectorModelBindingView view;
        view.context = QString::fromStdString(binding.context.empty() ? std::string("AnyEndpoint") : binding.context);
        view.robotId = QString::fromStdString(binding.robotId);
        view.linkName = QString::fromStdString(binding.linkName);
        view.objectId = QString::fromStdString(binding.objectId);
        view.attachmentId = QString::fromStdString(binding.attachmentId);
        view.pointCloudId = QString::fromStdString(binding.pointCloudId);
        view.mode = QString::fromStdString(binding.mode.empty() ? std::string("Current") : binding.mode);
        view.modelId = QString::fromStdString(binding.modelId);
        view.targetLabel = bindingTargetLabel(view);
        return view;
    }

    void appendBinding(
        QVector<CollisionDetectorModelBindingView>& bindings,
        CollisionDetectorModelBindingView binding)
    {
        if((binding.robotId.isEmpty() || binding.linkName.isEmpty()) &&
            binding.objectId.isEmpty() && binding.attachmentId.isEmpty() &&
            binding.pointCloudId.isEmpty()) {
            return;
        }
        const QString key = bindingKey(binding);
        if(std::any_of(bindings.begin(), bindings.end(), [&](const CollisionDetectorModelBindingView& item) {
               return bindingKey(item) == key;
           })) {
            return;
        }
        binding.targetLabel = bindingTargetLabel(binding);
        bindings.push_back(std::move(binding));
    }

    CollisionDetectorModelBindingView memberBinding(
        const simulation_project::CollisionSelectionSetMemberDesc& member,
        const QString& context)
    {
        CollisionDetectorModelBindingView view;
        view.context = context;
        view.robotId = QString::fromStdString(member.robotId);
        view.linkName = QString::fromStdString(member.linkName);
        if(!member.attachmentId.empty()) {
            view.attachmentId = QString::fromStdString(member.attachmentId);
        } else {
            view.objectId = QString::fromStdString(member.objectId);
        }
        return view;
    }

    QString detectorDisplayName(const simulation_project::CollisionDetectorDesc& detector)
    {
        return QString::fromStdString(detector.name.empty() ? detector.id : detector.name);
    }

    QString selectionSetDisplayName(const simulation_project::CollisionSelectionSetDesc& selectionSet)
    {
        return QString::fromStdString(selectionSet.name.empty() ? selectionSet.id : selectionSet.name);
    }

    const simulation_project::CollisionDetectorDesc* findDetector(
        const simulation_project::ProjectDocument& document,
        const QString& detectorId)
    {
        const std::string id = detectorId.toStdString();
        auto it = std::find_if(
            document.collision.detectors.begin(),
            document.collision.detectors.end(),
            [&](const simulation_project::CollisionDetectorDesc& detector) {
                return detector.id == id;
            });
        return it == document.collision.detectors.end() ? nullptr : &(*it);
    }

    const simulation_project::CollisionSelectionSetDesc* findSelectionSet(
        const simulation_project::ProjectDocument& document,
        const std::string& id)
    {
        const auto it = std::find_if(
            document.collision.selectionSets.begin(),
            document.collision.selectionSets.end(),
            [&](const simulation_project::CollisionSelectionSetDesc& set) { return set.id == id; });
        return it != document.collision.selectionSets.end() ? &(*it) : nullptr;
    }

    void appendSelectionSetBindings(
        const simulation_project::ProjectDocument& document,
        const std::string& setId,
        const QString& context,
        QVector<CollisionDetectorModelBindingView>& bindings)
    {
        const simulation_project::CollisionSelectionSetDesc* set = findSelectionSet(document, setId);
        if(set == nullptr) {
            return;
        }
        for(const simulation_project::CollisionSelectionSetMemberDesc& member : set->members) {
            appendBinding(bindings, memberBinding(member, context));
        }
    }

    void setObjectBindingTarget(
        const simulation_project::ProjectDocument& document,
        const std::string& id,
        CollisionDetectorModelBindingView& binding)
    {
        const bool attachment = std::any_of(
            document.mountedAttachments.begin(), document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& item) { return item.id == id; });
        if(attachment) {
            binding.attachmentId = QString::fromStdString(id);
            return;
        }
        const bool pointCloud = std::any_of(
            document.pointClouds.begin(), document.pointClouds.end(),
            [&](const simulation_project::PointCloudDesc& item) { return item.id == id; });
        if(pointCloud) {
            binding.pointCloudId = QString::fromStdString(id);
            return;
        }
        binding.objectId = QString::fromStdString(id);
    }
}

QVector<CollisionDetectorListItemView> CollisionDetectorsController::buildDetectorListItems(
    const simulation_project::ProjectDocument& document,
    const std::vector<robot_qt_viewer::CollisionRuntimeDetectorInfo>& runtimeDetectors)
{
    std::unordered_map<std::string, robot_qt_viewer::CollisionRuntimeDetectorInfo> runtimeById;
    for(const robot_qt_viewer::CollisionRuntimeDetectorInfo& detector : runtimeDetectors) {
        runtimeById[detector.id] = detector;
    }

    QVector<CollisionDetectorListItemView> items;
    for(const simulation_project::CollisionDetectorDesc& detector : document.collision.detectors) {
        const auto runtimeIt = runtimeById.find(detector.id);
        const bool visible = detector.visualization.visible;
        const std::size_t includePairCount = runtimeIt != runtimeById.end() ? runtimeIt->second.includePairCount : 0;
        const std::size_t contactCount = runtimeIt != runtimeById.end() ? runtimeIt->second.contactCount : 0;
        const bool hasResult = runtimeIt != runtimeById.end() && runtimeIt->second.hasResult;
        const bool inCollision = runtimeIt != runtimeById.end() && runtimeIt->second.inCollision;

        const QString displayName = detectorDisplayName(detector);
        QString text = displayName;

        QStringList tooltipLines;
        tooltipLines << displayName;
        tooltipLines << QString("id: %1").arg(QString::fromStdString(detector.id));
        tooltipLines << QString("type: %1").arg(QString::fromStdString(detector.type));
        if(detector.type == "SceneAll") {
            tooltipLines << "scope: all";
        } else if(includePairCount == 0) {
            tooltipLines << "resolved pairs: 0";
        } else {
            tooltipLines << QString("resolved pairs: %1").arg(static_cast<qulonglong>(includePairCount));
        }
        if(hasResult) {
            tooltipLines << QString("contacts: %1").arg(static_cast<qulonglong>(contactCount));
            tooltipLines << QString("state: %1").arg(inCollision ? "collision" : "clear");
        } else {
            tooltipLines << "result: not available";
        }

        CollisionDetectorListItemView item;
        item.id = QString::fromStdString(detector.id);
        item.text = text;
        item.tooltip = tooltipLines.join('\n');
        item.checked = detector.enabled;
        item.selected = visible;
        items.push_back(std::move(item));
    }

    if(items.isEmpty()) {
        CollisionDetectorListItemView item;
        item.text = "No collision detectors";
        item.enabled = false;
        items.push_back(std::move(item));
    }

    return items;
}

CollisionDetectorPropertiesView CollisionDetectorsController::buildProperties(
    const simulation_project::ProjectDocument& document,
    const QString& detectorId)
{
    CollisionDetectorPropertiesView view;
    for(const simulation_project::CollisionSelectionSetDesc& selectionSet : document.collision.selectionSets) {
        CollisionDetectorSelectionSetOptionView option;
        option.id = QString::fromStdString(selectionSet.id);
        option.text = selectionSetDisplayName(selectionSet);
        view.selectionSets.push_back(std::move(option));
    }

    const simulation_project::CollisionDetectorDesc* detector = findDetector(document, detectorId);
    if(detector == nullptr) {
        return view;
    }

    view.hasDetector = true;
    view.id = QString::fromStdString(detector->id);
    view.name = QString::fromStdString(detector->name);
    view.enabled = detector->enabled;
    view.visible = detector->visualization.visible;
    view.geometry = detector->visualization.showCollisionGeometry;
    view.contacts = detector->contacts;
    view.normals = detector->visualization.showNormals;
    view.nearest = detector->nearestPoints || detector->distance;
    view.maxContacts = static_cast<double>(detector->maxContacts);
    view.distanceThreshold = detector->distanceThreshold;
    view.policy = detector->queryPolicy.empty()
        ? QString("Legacy")
        : QString::fromStdString(detector->queryPolicy);
    view.setA = QString::fromStdString(detector->selectionSetAId);
    view.setB = QString::fromStdString(detector->selectionSetBId);

    for(const simulation_project::CollisionDetectorModelBindingDesc& binding : detector->modelBindings) {
        appendBinding(view.modelBindings, bindingView(binding));
    }
    if(detector->queryPolicy == "WithinSet" || detector->queryPolicy == "BetweenSets") {
        appendSelectionSetBindings(document, detector->selectionSetAId, "SetA", view.modelBindings);
    }
    if(detector->queryPolicy == "BetweenSets") {
        appendSelectionSetBindings(document, detector->selectionSetBId, "SetB", view.modelBindings);
    }
    for(const simulation_project::CollisionDetectorTargetDesc& target : detector->targets) {
        if(!target.robotId.empty()) {
            for(const std::string& linkName : target.includeLinks) {
                CollisionDetectorModelBindingView binding;
                binding.robotId = QString::fromStdString(target.robotId);
                binding.linkName = QString::fromStdString(linkName);
                appendBinding(view.modelBindings, std::move(binding));
            }
        } else if(!target.objectId.empty()) {
            CollisionDetectorModelBindingView binding;
            setObjectBindingTarget(document, target.objectId, binding);
            appendBinding(view.modelBindings, std::move(binding));
        }
    }
    for(const simulation_project::CollisionPairGeneratorDesc& generator : detector->pairGenerators) {
        CollisionDetectorModelBindingView a;
        a.context = "PairGeneratorA";
        a.robotId = QString::fromStdString(!generator.robotA.empty() ? generator.robotA : generator.robotId);
        a.linkName = QString::fromStdString(!generator.linkA.empty() ? generator.linkA : generator.linkName);
        if(!generator.objectA.empty()) {
            setObjectBindingTarget(document, generator.objectA, a);
        }
        appendBinding(view.modelBindings, std::move(a));

        CollisionDetectorModelBindingView b;
        b.context = "PairGeneratorB";
        b.robotId = QString::fromStdString(generator.robotB);
        b.linkName = QString::fromStdString(generator.linkB);
        const std::string objectB = !generator.objectB.empty() ? generator.objectB : generator.objectId;
        if(!objectB.empty()) {
            setObjectBindingTarget(document, objectB, b);
        }
        appendBinding(view.modelBindings, std::move(b));
    }
    return view;
}
