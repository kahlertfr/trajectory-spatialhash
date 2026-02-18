// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashQueryAsyncTasks.h"
#include "Async/Async.h"

// ============================================================================
// USpatialHashQueryRadiusAsyncTask
// ============================================================================

USpatialHashQueryRadiusAsyncTask* USpatialHashQueryRadiusAsyncTask::QueryRadiusAsync(
	UObject* WorldContextObject,
	USpatialHashTableManager* Manager,
	const FString& DatasetDirectory,
	FVector QueryPosition,
	float Radius,
	float CellSize,
	int32 TimeStep)
{
	USpatialHashQueryRadiusAsyncTask* Task = NewObject<USpatialHashQueryRadiusAsyncTask>();
	Task->SpatialHashManager = Manager;
	Task->Dataset = DatasetDirectory;
	Task->Position = QueryPosition;
	Task->QueryRadius = Radius;
	Task->HashCellSize = CellSize;
	Task->QueryTimeStep = TimeStep;
	Task->RegisterWithGameInstance(WorldContextObject);
	return Task;
}

void USpatialHashQueryRadiusAsyncTask::Activate()
{
	if (!SpatialHashManager)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashQueryRadiusAsyncTask::Activate: Manager is null"));
		OnComplete.Broadcast(TArray<FSpatialHashQueryResult>());
		SetReadyToDestroy();
		return;
	}

	// Use weak pointer to prevent issues if task is destroyed before callback executes
	TWeakObjectPtr<USpatialHashQueryRadiusAsyncTask> WeakThis(this);

	// Call the C++ async method
	SpatialHashManager->QueryRadiusWithDistanceCheckAsync(
		Dataset,
		Position,
		QueryRadius,
		HashCellSize,
		QueryTimeStep,
		FOnSpatialHashQueryComplete::CreateLambda([WeakThis](const TArray<FSpatialHashQueryResult>& Results)
		{
			// Check if task is still valid before broadcasting
			if (USpatialHashQueryRadiusAsyncTask* Task = WeakThis.Get())
			{
				// Broadcast to Blueprint
				Task->OnComplete.Broadcast(Results);
				Task->SetReadyToDestroy();
			}
		})
	);
}

// ============================================================================
// USpatialHashQueryDualRadiusAsyncTask
// ============================================================================

USpatialHashQueryDualRadiusAsyncTask* USpatialHashQueryDualRadiusAsyncTask::QueryDualRadiusAsync(
	UObject* WorldContextObject,
	USpatialHashTableManager* Manager,
	const FString& DatasetDirectory,
	FVector QueryPosition,
	float InnerRadius,
	float OuterRadius,
	float CellSize,
	int32 TimeStep)
{
	USpatialHashQueryDualRadiusAsyncTask* Task = NewObject<USpatialHashQueryDualRadiusAsyncTask>();
	Task->SpatialHashManager = Manager;
	Task->Dataset = DatasetDirectory;
	Task->Position = QueryPosition;
	Task->Inner = InnerRadius;
	Task->Outer = OuterRadius;
	Task->HashCellSize = CellSize;
	Task->QueryTimeStep = TimeStep;
	Task->RegisterWithGameInstance(WorldContextObject);
	return Task;
}

void USpatialHashQueryDualRadiusAsyncTask::Activate()
{
	if (!SpatialHashManager)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashQueryDualRadiusAsyncTask::Activate: Manager is null"));
		OnComplete.Broadcast(TArray<FSpatialHashQueryResult>(), TArray<FSpatialHashQueryResult>());
		SetReadyToDestroy();
		return;
	}

	// Use weak pointer to prevent issues if task is destroyed before callback executes
	TWeakObjectPtr<USpatialHashQueryDualRadiusAsyncTask> WeakThis(this);

	// Call the C++ async method
	SpatialHashManager->QueryDualRadiusWithDistanceCheckAsync(
		Dataset,
		Position,
		Inner,
		Outer,
		HashCellSize,
		QueryTimeStep,
		FOnSpatialHashDualQueryComplete::CreateLambda([WeakThis](const TArray<FSpatialHashQueryResult>& InnerResults, const TArray<FSpatialHashQueryResult>& OuterResults)
		{
			// Check if task is still valid before broadcasting
			if (USpatialHashQueryDualRadiusAsyncTask* Task = WeakThis.Get())
			{
				// Broadcast to Blueprint
				Task->OnComplete.Broadcast(InnerResults, OuterResults);
				Task->SetReadyToDestroy();
			}
		})
	);
}

// ============================================================================
// USpatialHashQueryTimeRangeAsyncTask
// ============================================================================

USpatialHashQueryTimeRangeAsyncTask* USpatialHashQueryTimeRangeAsyncTask::QueryTimeRangeAsync(
	UObject* WorldContextObject,
	USpatialHashTableManager* Manager,
	const FString& DatasetDirectory,
	FVector QueryPosition,
	float Radius,
	float CellSize,
	int32 StartTimeStep,
	int32 EndTimeStep)
{
	USpatialHashQueryTimeRangeAsyncTask* Task = NewObject<USpatialHashQueryTimeRangeAsyncTask>();
	Task->SpatialHashManager = Manager;
	Task->Dataset = DatasetDirectory;
	Task->Position = QueryPosition;
	Task->QueryRadius = Radius;
	Task->HashCellSize = CellSize;
	Task->StartTime = StartTimeStep;
	Task->EndTime = EndTimeStep;
	Task->RegisterWithGameInstance(WorldContextObject);
	return Task;
}

void USpatialHashQueryTimeRangeAsyncTask::Activate()
{
	if (!SpatialHashManager)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashQueryTimeRangeAsyncTask::Activate: Manager is null"));
		OnComplete.Broadcast(TArray<FSpatialHashQueryResult>());
		SetReadyToDestroy();
		return;
	}

	// Use weak pointer to prevent issues if task is destroyed before callback executes
	TWeakObjectPtr<USpatialHashQueryTimeRangeAsyncTask> WeakThis(this);

	// Call the C++ async method
	SpatialHashManager->QueryRadiusOverTimeRangeAsync(
		Dataset,
		Position,
		QueryRadius,
		HashCellSize,
		StartTime,
		EndTime,
		FOnSpatialHashQueryComplete::CreateLambda([WeakThis](const TArray<FSpatialHashQueryResult>& Results)
		{
			// Check if task is still valid before broadcasting
			if (USpatialHashQueryTimeRangeAsyncTask* Task = WeakThis.Get())
			{
				// Broadcast to Blueprint
				Task->OnComplete.Broadcast(Results);
				Task->SetReadyToDestroy();
			}
		})
	);
}

// ============================================================================
// USpatialHashQueryTrajectoryAsyncTask
// ============================================================================

USpatialHashQueryTrajectoryAsyncTask* USpatialHashQueryTrajectoryAsyncTask::QueryTrajectoryAsync(
	UObject* WorldContextObject,
	USpatialHashTableManager* Manager,
	const FString& DatasetDirectory,
	int32 QueryTrajectoryId,
	float Radius,
	float CellSize,
	int32 StartTimeStep,
	int32 EndTimeStep)
{
	USpatialHashQueryTrajectoryAsyncTask* Task = NewObject<USpatialHashQueryTrajectoryAsyncTask>();
	Task->SpatialHashManager = Manager;
	Task->Dataset = DatasetDirectory;
	Task->TrajId = QueryTrajectoryId;
	Task->QueryRadius = Radius;
	Task->HashCellSize = CellSize;
	Task->StartTime = StartTimeStep;
	Task->EndTime = EndTimeStep;
	Task->RegisterWithGameInstance(WorldContextObject);
	return Task;
}

void USpatialHashQueryTrajectoryAsyncTask::Activate()
{
	if (!SpatialHashManager)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashQueryTrajectoryAsyncTask::Activate: Manager is null"));
		OnComplete.Broadcast(TArray<FSpatialHashQueryResult>());
		SetReadyToDestroy();
		return;
	}

	// Use weak pointer to prevent issues if task is destroyed before callback executes
	TWeakObjectPtr<USpatialHashQueryTrajectoryAsyncTask> WeakThis(this);

	// Call the C++ async method
	SpatialHashManager->QueryTrajectoryRadiusOverTimeRangeAsync(
		Dataset,
		static_cast<uint32>(TrajId),
		QueryRadius,
		HashCellSize,
		StartTime,
		EndTime,
		FOnSpatialHashQueryComplete::CreateLambda([WeakThis](const TArray<FSpatialHashQueryResult>& Results)
		{
			// Check if task is still valid before broadcasting
			if (USpatialHashQueryTrajectoryAsyncTask* Task = WeakThis.Get())
			{
				// Broadcast to Blueprint
				Task->OnComplete.Broadcast(Results);
				Task->SetReadyToDestroy();
			}
		})
	);
}
