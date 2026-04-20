// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UserSelectionManager.h"
#include "TrajectorySelectionProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable, Category = "User Selection")
class UTrajectorySelectionProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement this interface in any Blueprint (or C++ class) that can supply
 * trajectory selections to a UUserSelectionManager.
 *
 * After implementing the interface, call UUserSelectionManager::RegisterProvider
 * with your object.  Whenever UUserSelectionManager::RefreshFromProviders is
 * called, the manager will call GetProvidedSelections on every registered
 * provider and merge the results into its internal list.
 */
class SPATIALHASHEDTRAJECTORY_API ITrajectorySelectionProvider
{
	GENERATED_BODY()

public:

	/**
	 * Return the selections this object wants to contribute to the manager.
	 *
	 * Implement this event in your Blueprint.  The manager calls it via the
	 * Execute_ wrapper during RefreshFromProviders.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "User Selection")
	TArray<FUserTrajectorySelection> GetProvidedSelections() const;
};
