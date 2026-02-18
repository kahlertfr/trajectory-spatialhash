// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "SpatialHashTableManager.h"
#include "SpatialHashQueryAsyncTasks.generated.h"

// Forward declarations
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQueryComplete, const TArray<FSpatialHashQueryResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDualQueryComplete, const TArray<FSpatialHashQueryResult>&, InnerResults, const TArray<FSpatialHashQueryResult>&, OuterResults);

/**
 * Async task for querying trajectories within a radius at a single timestep
 * Returns results via Blueprint event when complete
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API USpatialHashQueryRadiusAsyncTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query trajectories within a radius at a single timestep (async, non-blocking)
	 * 
	 * @param Manager The spatial hash table manager
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @return Async task object - bind to OnComplete event
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash|Async", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static USpatialHashQueryRadiusAsyncTask* QueryRadiusAsync(
		UObject* WorldContextObject,
		USpatialHashTableManager* Manager,
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 TimeStep);

	// Output delegates
	UPROPERTY(BlueprintAssignable)
	FOnQueryComplete OnComplete;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	UPROPERTY()
	USpatialHashTableManager* SpatialHashManager;
	
	UPROPERTY()
	FString Dataset;
	
	UPROPERTY()
	FVector Position;
	
	UPROPERTY()
	float QueryRadius;
	
	UPROPERTY()
	float HashCellSize;
	
	UPROPERTY()
	int32 QueryTimeStep;
};

/**
 * Async task for querying trajectories with dual radius at a single timestep
 * Returns inner and outer results via Blueprint events when complete
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API USpatialHashQueryDualRadiusAsyncTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query trajectories with dual radius at a single timestep (async, non-blocking)
	 * 
	 * @param Manager The spatial hash table manager
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param InnerRadius Inner search radius in world units
	 * @param OuterRadius Outer search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @return Async task object - bind to OnComplete event
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash|Async", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static USpatialHashQueryDualRadiusAsyncTask* QueryDualRadiusAsync(
		UObject* WorldContextObject,
		USpatialHashTableManager* Manager,
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float InnerRadius,
		float OuterRadius,
		float CellSize,
		int32 TimeStep);

	// Output delegates
	UPROPERTY(BlueprintAssignable)
	FOnDualQueryComplete OnComplete;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	UPROPERTY()
	USpatialHashTableManager* SpatialHashManager;
	
	UPROPERTY()
	FString Dataset;
	
	UPROPERTY()
	FVector Position;
	
	UPROPERTY()
	float InnerRadius;
	
	UPROPERTY()
	float OuterRadius;
	
	UPROPERTY()
	float HashCellSize;
	
	UPROPERTY()
	int32 QueryTimeStep;
};

/**
 * Async task for querying trajectories over a time range
 * Returns results via Blueprint event when complete
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API USpatialHashQueryTimeRangeAsyncTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query trajectories over a time range (async, non-blocking)
	 * 
	 * @param Manager The spatial hash table manager
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param StartTimeStep First time step to query
	 * @param EndTimeStep Last time step to query
	 * @return Async task object - bind to OnComplete event
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash|Async", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static USpatialHashQueryTimeRangeAsyncTask* QueryTimeRangeAsync(
		UObject* WorldContextObject,
		USpatialHashTableManager* Manager,
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep);

	// Output delegates
	UPROPERTY(BlueprintAssignable)
	FOnQueryComplete OnComplete;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	UPROPERTY()
	USpatialHashTableManager* SpatialHashManager;
	
	UPROPERTY()
	FString Dataset;
	
	UPROPERTY()
	FVector Position;
	
	UPROPERTY()
	float QueryRadius;
	
	UPROPERTY()
	float HashCellSize;
	
	UPROPERTY()
	int32 StartTime;
	
	UPROPERTY()
	int32 EndTime;
};

/**
 * Async task for querying trajectories that interact with a moving trajectory
 * Returns results via Blueprint event when complete
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API USpatialHashQueryTrajectoryAsyncTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Query trajectories that interact with a moving query trajectory (async, non-blocking)
	 * 
	 * @param Manager The spatial hash table manager
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryTrajectoryId ID of the query trajectory
	 * @param Radius Search radius around query trajectory
	 * @param CellSize Cell size of hash table to use
	 * @param StartTimeStep First time step to query
	 * @param EndTimeStep Last time step to query
	 * @return Async task object - bind to OnComplete event
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash|Async", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static USpatialHashQueryTrajectoryAsyncTask* QueryTrajectoryAsync(
		UObject* WorldContextObject,
		USpatialHashTableManager* Manager,
		const FString& DatasetDirectory,
		int32 QueryTrajectoryId,
		float Radius,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep);

	// Output delegates
	UPROPERTY(BlueprintAssignable)
	FOnQueryComplete OnComplete;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	UPROPERTY()
	USpatialHashTableManager* SpatialHashManager;
	
	UPROPERTY()
	FString Dataset;
	
	UPROPERTY()
	int32 TrajId;
	
	UPROPERTY()
	float QueryRadius;
	
	UPROPERTY()
	float HashCellSize;
	
	UPROPERTY()
	int32 StartTime;
	
	UPROPERTY()
	int32 EndTime;
};
