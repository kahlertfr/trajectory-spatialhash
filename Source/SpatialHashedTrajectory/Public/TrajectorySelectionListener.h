// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UserSelectionManager.h"
#include "TrajectorySelectionListener.generated.h"

UINTERFACE(MinimalAPI, Blueprintable, Category = "User Selection")
class UTrajectorySelectionListener : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement this interface in any Blueprint (or C++ class) that should be
 * notified when the user's trajectory selections change.
 *
 * After implementing the interface, call UUserSelectionManager::RegisterListener
 * with your object so the manager knows to notify you.
 */
class SPATIALHASHEDTRAJECTORY_API ITrajectorySelectionListener
{
	GENERATED_BODY()

public:

	/**
	 * Called by UUserSelectionManager on every registered listener whenever the
	 * selections are added to, updated, removed from, or refreshed from
	 * providers.
	 *
	 * @param Selections  The complete, up-to-date list of user selections.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "User Selection")
	void OnUserSelectionsChanged(const TArray<FUserTrajectorySelection>& Selections);
};
