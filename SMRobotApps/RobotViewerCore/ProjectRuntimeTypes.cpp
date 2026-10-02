#include "ProjectRuntimeTypes.h"

namespace
{
    std::shared_ptr<simulation_runtime::ProjectParallelRobotState> makeOwnedParallelState()
    {
        return std::make_shared<simulation_runtime::ProjectParallelRobotState>();
    }
}

RuntimeRobot::RuntimeRobot()
    : ownedParallelState(makeOwnedParallelState())
    , parallelState(ownedParallelState.get())
    , simulationRobot(parallelState->runtime)
    , runtimeId(parallelState->runtimeId)
    , documentId(parallelState->documentId)
    , model(parallelState->model)
    , instance(parallelState->instance)
    , name(parallelState->name)
    , sourceType(parallelState->sourceType)
    , sourcePath(parallelState->sourcePath)
    , sourceModelIndex(parallelState->sourceModelIndex)
    , baseTransform(parallelState->baseTransform)
    , collisionEnabled(parallelState->collisionEnabled)
    , autoMotionEnabled(parallelState->autoMotionEnabled)
    , autoMotionAmplitude(parallelState->autoMotionAmplitude)
    , autoMotionSpeed(parallelState->autoMotionSpeed)
    , parallelControlEnabled(parallelState->parallelControlEnabled)
    , parallelHomeBaseTransform(parallelState->parallelHomeBaseTransform)
    , parallelGeometry(parallelState->parallelGeometry)
    , parallelPose(parallelState->parallelPose)
    , parallelActuatorLengths(parallelState->parallelActuatorLengths)
    , parallelActuatorHomeLengths(parallelState->parallelActuatorHomeLengths)
    , parallelActuatorRates(parallelState->parallelActuatorRates)
    , parallelActuatorDofIndices(parallelState->parallelActuatorDofIndices)
    , parallelActuatorSigns(parallelState->parallelActuatorSigns)
    , parallelLegControls(parallelState->parallelLegControls)
    , parallelInternalPlatformVisualsEnabled(
        parallelState->parallelInternalPlatformVisualsEnabled)
    , parallelInternalPlatformDrivenLinks(parallelState->parallelInternalPlatformDrivenLinks)
    , parallelInternalPlatformHomeLocalTransforms(
        parallelState->parallelInternalPlatformHomeLocalTransforms)
    , parallelFollowerEnabled(parallelState->parallelFollowerEnabled)
    , parallelFollowerLegIndex(parallelState->parallelFollowerLegIndex)
    , parallelFollowerHomeTransform(parallelState->parallelFollowerHomeTransform)
    , parallelFollowerAnchorsValid(parallelState->parallelFollowerAnchorsValid)
    , parallelFollowerHomeBaseAnchor(parallelState->parallelFollowerHomeBaseAnchor)
    , parallelFollowerHomePlatformAnchor(parallelState->parallelFollowerHomePlatformAnchor)
    , parallelFollowerActuatorDofIndex(parallelState->parallelFollowerActuatorDofIndex)
    , parallelFollowerActuatorSign(parallelState->parallelFollowerActuatorSign)
    , parallelFollowerHomeLength(parallelState->parallelFollowerHomeLength)
    , parallelFollowerDrivenLinks(parallelState->parallelFollowerDrivenLinks)
    , parallelFollowerHomeLinkTransforms(parallelState->parallelFollowerHomeLinkTransforms)
{
}

RuntimeRobot::RuntimeRobot(simulation_runtime::ProjectParallelRobotState& state)
    : parallelState(&state)
    , simulationRobot(state.runtime)
    , runtimeId(state.runtimeId)
    , documentId(state.documentId)
    , model(state.model)
    , instance(state.instance)
    , name(state.name)
    , sourceType(state.sourceType)
    , sourcePath(state.sourcePath)
    , sourceModelIndex(state.sourceModelIndex)
    , baseTransform(state.baseTransform)
    , collisionEnabled(state.collisionEnabled)
    , autoMotionEnabled(state.autoMotionEnabled)
    , autoMotionAmplitude(state.autoMotionAmplitude)
    , autoMotionSpeed(state.autoMotionSpeed)
    , parallelControlEnabled(state.parallelControlEnabled)
    , parallelHomeBaseTransform(state.parallelHomeBaseTransform)
    , parallelGeometry(state.parallelGeometry)
    , parallelPose(state.parallelPose)
    , parallelActuatorLengths(state.parallelActuatorLengths)
    , parallelActuatorHomeLengths(state.parallelActuatorHomeLengths)
    , parallelActuatorRates(state.parallelActuatorRates)
    , parallelActuatorDofIndices(state.parallelActuatorDofIndices)
    , parallelActuatorSigns(state.parallelActuatorSigns)
    , parallelLegControls(state.parallelLegControls)
    , parallelInternalPlatformVisualsEnabled(state.parallelInternalPlatformVisualsEnabled)
    , parallelInternalPlatformDrivenLinks(state.parallelInternalPlatformDrivenLinks)
    , parallelInternalPlatformHomeLocalTransforms(
        state.parallelInternalPlatformHomeLocalTransforms)
    , parallelFollowerEnabled(state.parallelFollowerEnabled)
    , parallelFollowerLegIndex(state.parallelFollowerLegIndex)
    , parallelFollowerHomeTransform(state.parallelFollowerHomeTransform)
    , parallelFollowerAnchorsValid(state.parallelFollowerAnchorsValid)
    , parallelFollowerHomeBaseAnchor(state.parallelFollowerHomeBaseAnchor)
    , parallelFollowerHomePlatformAnchor(state.parallelFollowerHomePlatformAnchor)
    , parallelFollowerActuatorDofIndex(state.parallelFollowerActuatorDofIndex)
    , parallelFollowerActuatorSign(state.parallelFollowerActuatorSign)
    , parallelFollowerHomeLength(state.parallelFollowerHomeLength)
    , parallelFollowerDrivenLinks(state.parallelFollowerDrivenLinks)
    , parallelFollowerHomeLinkTransforms(state.parallelFollowerHomeLinkTransforms)
{
}

RuntimeRobot::RuntimeRobot(const RuntimeRobot& other)
    : RuntimeRobot()
{
    *this = other;
}

RuntimeRobot::RuntimeRobot(RuntimeRobot&& other) noexcept
    : ownedParallelState(std::move(other.ownedParallelState))
    , parallelState(other.parallelState)
    , simulationRobot(other.simulationRobot)
    , runtimeId(parallelState->runtimeId)
    , documentId(parallelState->documentId)
    , model(parallelState->model)
    , instance(parallelState->instance)
    , visualBridge(std::move(other.visualBridge))
    , collisionModel(std::move(other.collisionModel))
    , collisionInstance(std::move(other.collisionInstance))
    , meshOverlays(std::move(other.meshOverlays))
    , linkOriginalMaterials(std::move(other.linkOriginalMaterials))
    , highlightedLinks(std::move(other.highlightedLinks))
    , name(parallelState->name)
    , sourceType(parallelState->sourceType)
    , sourcePath(parallelState->sourcePath)
    , sourceModelIndex(parallelState->sourceModelIndex)
    , baseTransform(parallelState->baseTransform)
    , collisionEnabled(parallelState->collisionEnabled)
    , autoMotionEnabled(parallelState->autoMotionEnabled)
    , autoMotionAmplitude(parallelState->autoMotionAmplitude)
    , autoMotionSpeed(parallelState->autoMotionSpeed)
    , parallelControlEnabled(parallelState->parallelControlEnabled)
    , parallelHomeBaseTransform(parallelState->parallelHomeBaseTransform)
    , parallelGeometry(parallelState->parallelGeometry)
    , parallelPose(parallelState->parallelPose)
    , parallelActuatorLengths(parallelState->parallelActuatorLengths)
    , parallelActuatorHomeLengths(parallelState->parallelActuatorHomeLengths)
    , parallelActuatorRates(parallelState->parallelActuatorRates)
    , parallelActuatorDofIndices(parallelState->parallelActuatorDofIndices)
    , parallelActuatorSigns(parallelState->parallelActuatorSigns)
    , parallelLegControls(parallelState->parallelLegControls)
    , parallelInternalPlatformVisualsEnabled(
        parallelState->parallelInternalPlatformVisualsEnabled)
    , parallelInternalPlatformDrivenLinks(parallelState->parallelInternalPlatformDrivenLinks)
    , parallelInternalPlatformHomeLocalTransforms(
        parallelState->parallelInternalPlatformHomeLocalTransforms)
    , parallelFollowerEnabled(parallelState->parallelFollowerEnabled)
    , parallelFollowerLegIndex(parallelState->parallelFollowerLegIndex)
    , parallelFollowerHomeTransform(parallelState->parallelFollowerHomeTransform)
    , parallelFollowerAnchorsValid(parallelState->parallelFollowerAnchorsValid)
    , parallelFollowerHomeBaseAnchor(parallelState->parallelFollowerHomeBaseAnchor)
    , parallelFollowerHomePlatformAnchor(parallelState->parallelFollowerHomePlatformAnchor)
    , parallelFollowerActuatorDofIndex(parallelState->parallelFollowerActuatorDofIndex)
    , parallelFollowerActuatorSign(parallelState->parallelFollowerActuatorSign)
    , parallelFollowerHomeLength(parallelState->parallelFollowerHomeLength)
    , parallelFollowerDrivenLinks(parallelState->parallelFollowerDrivenLinks)
    , parallelFollowerHomeLinkTransforms(parallelState->parallelFollowerHomeLinkTransforms)
    , sprayNozzleLinkName(std::move(other.sprayNozzleLinkName))
    , sprayNozzleLocalTransform(other.sprayNozzleLocalTransform)
{
}

RuntimeRobot& RuntimeRobot::operator=(const RuntimeRobot& other)
{
    if(this == &other) {
        return *this;
    }

    *parallelState = *other.parallelState;
    runtimeId = other.runtimeId;
    documentId = other.documentId;
    visualBridge = other.visualBridge;
    collisionModel = other.collisionModel;
    collisionInstance = other.collisionInstance;
    meshOverlays = other.meshOverlays;
    linkOriginalMaterials = other.linkOriginalMaterials;
    highlightedLinks = other.highlightedLinks;
    name = other.name;
    sourceType = other.sourceType;
    sourcePath = other.sourcePath;
    sourceModelIndex = other.sourceModelIndex;
    sprayNozzleLinkName = other.sprayNozzleLinkName;
    sprayNozzleLocalTransform = other.sprayNozzleLocalTransform;
    return *this;
}

RuntimeRobot& RuntimeRobot::operator=(RuntimeRobot&& other) noexcept
{
    return operator=(static_cast<const RuntimeRobot&>(other));
}

RuntimeRobot::~RuntimeRobot() = default;
