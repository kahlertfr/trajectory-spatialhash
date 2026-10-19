#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UserSelectionManager.h"
#include "TrajectoryVisibilityBlueprintLibrary.generated.h"

/** Checkbox helpers for the zone bitmask. Rendering classification remains in Niagara. */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API UTrajectoryVisibilityBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Trajectory|Visibility")
	static int32 SetZoneVisible(int32 Mask, ETrajectoryVisibilityZone Zone, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Trajectory|Visibility")
	static bool IsZoneVisible(int32 Mask, ETrajectoryVisibilityZone Zone);
};