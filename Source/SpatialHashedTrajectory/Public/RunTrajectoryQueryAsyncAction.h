// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "RunTrajectoryQueryAsyncAction.generated.h"

class ATrajectoryQueryNiagaraActor;

/** Delegate type for the RunQuery async node output pins */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrajectoryQueryFinished);

/**
 * Async Blueprint action that fires the nearest-neighbour queries on an
 * ATrajectoryQueryNiagaraActor and exposes an OnSuccess / OnFailure exec pin.
 *
 * Usage in Blueprint:
 *   1. Call "Run Query (Async)" and connect the Target actor.
 *   2. From the OnSuccess pin, call "Transfer Data To Niagara" on the actor to
 *      push the cached results to the Niagara system.
 *   3. Use the actor's ResultBoundsMin / ResultBoundsMax properties for scaling.
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API URunTrajectoryQueryAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Run the nearest-neighbour queries on the actor asynchronously.
	 * Results and bounding box are stored on the actor when OnSuccess fires.
	 * OnFailure fires if the actor could not start the queries (e.g. empty DatasetDirectory
	 * or QueryPositions array).
	 *
	 * @param TargetActor  The actor whose QueryPositions and query settings to use.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Visualization",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
		        DisplayName = "Run Query (Async)"))
	static URunTrajectoryQueryAsyncAction* RunQuery(
		UObject* WorldContextObject,
		ATrajectoryQueryNiagaraActor* TargetActor);

	/** Fires when all async queries have completed and results are cached on the actor */
	UPROPERTY(BlueprintAssignable)
	FOnTrajectoryQueryFinished OnSuccess;

	/** Fires if the actor could not start the queries */
	UPROPERTY(BlueprintAssignable)
	FOnTrajectoryQueryFinished OnFailure;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	UPROPERTY()
	ATrajectoryQueryNiagaraActor* Actor;

	void HandleQueryComplete();
	void HandleQueryFailed();
};
