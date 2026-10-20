#pragma once
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

/** Optional project adapter for the actual rendered hand skeleton. Transforms are world-space. */
class SPATIALHASHEDTRAJECTORY_API ITrajectoryHandPoseProvider : public IModularFeature
{
public:
	static FName GetModularFeatureName() { return TEXT("TrajectoryHandPoseProvider"); }
	virtual bool GetPickingHandPose(UObject* WorldContext, FTransform& RightIndexTip,
		FTransform& LeftIndexKnuckle, FTransform& LeftThumbTip) const = 0;
};
