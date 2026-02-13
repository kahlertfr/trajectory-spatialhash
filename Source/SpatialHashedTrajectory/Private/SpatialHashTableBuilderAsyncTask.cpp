// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashTableBuilderAsyncTask.h"
#include "Async/Async.h"

USpatialHashTableBuilderAsyncTask* USpatialHashTableBuilderAsyncTask::BuildHashTablesAsync(
	UObject* WorldContextObject,
	const FSpatialHashTableBuilder::FBuildConfig& Config,
	const TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& TimeStepSamples)
{
	USpatialHashTableBuilderAsyncTask* Task = NewObject<USpatialHashTableBuilderAsyncTask>();
	Task->BuildConfig = Config;
	Task->SamplesData = TimeStepSamples;
	Task->WorldContext = WorldContextObject;
	return Task;
}

void USpatialHashTableBuilderAsyncTask::Activate()
{
	// Execute build on a background thread
	Async(EAsyncExecution::ThreadPool, [this]()
	{
		ExecuteBuild();
	});
}

void USpatialHashTableBuilderAsyncTask::ExecuteBuild()
{
	FSpatialHashTableBuilder Builder;
	
	// Build hash tables (this will use parallel processing internally)
	bool bSuccess = Builder.BuildHashTables(BuildConfig, SamplesData);
	
	// Return to game thread for callback
	AsyncTask(ENamedThreads::GameThread, [this, bSuccess]()
	{
		if (bSuccess)
		{
			OnBuildComplete(true, TEXT(""));
		}
		else
		{
			OnBuildComplete(false, TEXT("Failed to build hash tables"));
		}
	});
}

void USpatialHashTableBuilderAsyncTask::OnBuildComplete(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		OnComplete.Broadcast();
	}
	else
	{
		OnFailed.Broadcast(ErrorMessage);
	}
	
	SetReadyToDestroy();
}
