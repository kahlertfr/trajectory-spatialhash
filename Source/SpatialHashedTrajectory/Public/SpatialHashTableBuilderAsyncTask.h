// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "SpatialHashTableBuilder.h"
#include "SpatialHashTableBuilderAsyncTask.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHashTableBuildProgress, int32, CompletedTimeSteps, int32, TotalTimeSteps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHashTableBuildComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHashTableBuildFailed, const FString&, ErrorMessage);

/**
 * Async task for building spatial hash tables without blocking the game thread
 * 
 * This task runs hash table creation in a background thread and provides progress updates.
 * Time steps are processed in parallel for maximum performance.
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API USpatialHashTableBuilderAsyncTask : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Build spatial hash tables asynchronously
	 * 
	 * @param Config Build configuration
	 * @param TimeStepSamples Trajectory samples for each time step
	 * @return Async task object
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static USpatialHashTableBuilderAsyncTask* BuildHashTablesAsync(
		UObject* WorldContextObject,
		const FSpatialHashTableBuilder::FBuildConfig& Config,
		const TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& TimeStepSamples);

	// Delegates
	UPROPERTY(BlueprintAssignable)
	FOnHashTableBuildProgress OnProgress;

	UPROPERTY(BlueprintAssignable)
	FOnHashTableBuildComplete OnComplete;

	UPROPERTY(BlueprintAssignable)
	FOnHashTableBuildFailed OnFailed;

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	void ExecuteBuild();
	void OnBuildComplete(bool bSuccess, const FString& ErrorMessage);

	FSpatialHashTableBuilder::FBuildConfig BuildConfig;
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> SamplesData;
	UObject* WorldContext;
};
