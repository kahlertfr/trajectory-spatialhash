#include "TrajectoryVisibilityBlueprintLibrary.h"

int32 UTrajectoryVisibilityBlueprintLibrary::SetZoneVisible(int32 Mask, ETrajectoryVisibilityZone Zone, bool bVisible)
{
	const int32 Bit = static_cast<int32>(Zone) & TrajectoryVisibility::AllZones;
	return (bVisible ? Mask | Bit : Mask & ~Bit) & TrajectoryVisibility::AllZones;
}

bool UTrajectoryVisibilityBlueprintLibrary::IsZoneVisible(int32 Mask, ETrajectoryVisibilityZone Zone)
{
	const int32 Bit = static_cast<int32>(Zone) & TrajectoryVisibility::AllZones;
	return Bit != 0 && (Mask & Bit) == Bit;
}