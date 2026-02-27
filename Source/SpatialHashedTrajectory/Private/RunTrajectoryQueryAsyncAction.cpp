// Copyright Epic Games, Inc. All Rights Reserved.

#include "RunTrajectoryQueryAsyncAction.h"
#include "TrajectoryQueryNiagaraActor.h"

URunTrajectoryQueryAsyncAction* URunTrajectoryQueryAsyncAction::RunQuery(
	UObject* WorldContextObject,
	ATrajectoryQueryNiagaraActor* TargetActor)
{
	URunTrajectoryQueryAsyncAction* Action = NewObject<URunTrajectoryQueryAsyncAction>();
	Action->Actor = TargetActor;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void URunTrajectoryQueryAsyncAction::Activate()
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("URunTrajectoryQueryAsyncAction: TargetActor is null."));
		HandleQueryFailed();
		return;
	}

	TWeakObjectPtr<URunTrajectoryQueryAsyncAction> WeakThis(this);

	Actor->FireAsyncQueriesWithCallback(
		FSimpleDelegate::CreateLambda([WeakThis]()
		{
			if (URunTrajectoryQueryAsyncAction* This = WeakThis.Get())
			{
				This->HandleQueryComplete();
			}
		}),
		FSimpleDelegate::CreateLambda([WeakThis]()
		{
			if (URunTrajectoryQueryAsyncAction* This = WeakThis.Get())
			{
				This->HandleQueryFailed();
			}
		})
	);
}

void URunTrajectoryQueryAsyncAction::HandleQueryComplete()
{
	OnSuccess.Broadcast();
	SetReadyToDestroy();
}

void URunTrajectoryQueryAsyncAction::HandleQueryFailed()
{
	OnFailure.Broadcast();
	SetReadyToDestroy();
}
