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
	
	// Keep task alive during execution
	Task->RegisterWithGameInstance(WorldContextObject);
	
	return Task;
}

void USpatialHashTableBuilderAsyncTask::Activate()
{
	// Use weak pointer to avoid use-after-free
	TWeakObjectPtr<USpatialHashTableBuilderAsyncTask> WeakThis(this);
	
	// Execute build on a background thread
	Async(EAsyncExecution::ThreadPool, [WeakThis]()
	{
		if (USpatialHashTableBuilderAsyncTask* Task = WeakThis.Get())
		{
			Task->ExecuteBuild();
		}
	});
}

void USpatialHashTableBuilderAsyncTask::ExecuteBuild()
{
	FSpatialHashTableBuilder Builder;
	
	// Build hash tables (this will use parallel processing internally)
	bool bSuccess = Builder.BuildHashTables(BuildConfig, SamplesData);
	
	// Use weak pointer for game thread callback
	TWeakObjectPtr<USpatialHashTableBuilderAsyncTask> WeakThis(this);
	
	// Return to game thread for callback
	AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess]()
	{
		if (USpatialHashTableBuilderAsyncTask* Task = WeakThis.Get())
		{
			if (bSuccess)
			{
				Task->OnBuildComplete(true, TEXT(""));
			}
			else
			{
				Task->OnBuildComplete(false, TEXT("Failed to build hash tables"));
			}
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
