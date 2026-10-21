#include "ConfirmTrajectorySelectionAsyncAction.h"
#include "TrajectoryQueryNiagaraActor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

UConfirmTrajectorySelectionAsyncAction* UConfirmTrajectorySelectionAsyncAction::ConfirmTrajectorySelection(
	UObject* WorldContextObject, ATrajectoryQueryNiagaraActor* TargetActor)
{
	auto* Action = NewObject<UConfirmTrajectorySelectionAsyncAction>();
	Action->Actor = TargetActor;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UConfirmTrajectorySelectionAsyncAction::Activate()
{
	if (bActivated || bFinished) return;
	bActivated = true;
	auto* Target = Actor.Get();
	if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !Target->GetWorld())
	{
		Finish(INDEX_NONE, -1, TEXT("Target actor is unavailable."));
		return;
	}
	if (Target->AsyncRibbonConfirmation.IsValid() || Target->PendingRibbonConfirmationFrame != 0)
	{
		Finish(INDEX_NONE, -1, TEXT("A confirmation is already pending."));
		return;
	}
	FVector Position;
	float Radius;
	int32 Samples;
	if (!Target->GetRibbonPickingState(Position, Radius, Samples, Session))
	{
		Finish(INDEX_NONE, -1, TEXT("Selector is inactive, expired, or has no trajectory data."));
		return;
	}
	Revision = Target->VisibleBoundsRevision;
	Target->AsyncRibbonConfirmation = this;
	// Wait for a new simulation frame: an already submitted hover readback must not confirm.
	Target->PendingRibbonConfirmationFrame = GFrameCounter + 1;
	Target->PendingRibbonConfirmationRevision = Revision;
	Target->ConfirmedProbePosition = Position;
	Target->ConfirmedProbeRadius = Radius;
	Target->RibbonConfirmationTime = Target->GetWorld()->GetTimeSeconds();
	Deadline = FPlatformTime::Seconds() + 0.5;
	Watchdog = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(
		this, &UConfirmTrajectorySelectionAsyncAction::CheckRequest));
}

bool UConfirmTrajectorySelectionAsyncAction::CheckRequest(float DeltaTime)
{
	auto* Target = Actor.Get();
	if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !Target->GetWorld())
		Finish(INDEX_NONE, -1, TEXT("Target actor was destroyed."));
	else if (Target->VisibleBoundsRevision != Revision || Target->TrajectorySelectorSession != Session ||
		Target->PendingRibbonConfirmationFrame == 0 || !Target->bTrajectorySelectorEnabled ||
		Target->GetWorld()->GetTimeSeconds() - Target->TrajectorySelectorUpdateTime > Target->TrajectorySelectorTimeout)
		Finish(INDEX_NONE, -1, TEXT("Confirmation cancelled: display or selector changed."));
	else if (FPlatformTime::Seconds() >= Deadline)
		Finish(INDEX_NONE, -1, TEXT("GPU confirmation timed out."));
	return !bFinished;
}

void UConfirmTrajectorySelectionAsyncAction::Finish(int32 Index, int64 Id, const FString& Error)
{
	if (bFinished) return;
	bFinished = true;
	FTSTicker::GetCoreTicker().RemoveTicker(Watchdog);
	Watchdog.Reset();
	if (auto* Target = Actor.Get(); Target && Target->AsyncRibbonConfirmation.Get() == this)
	{
		Target->AsyncRibbonConfirmation.Reset();
		Target->PendingRibbonConfirmationFrame = 0;
	}
	if (!Error.IsEmpty()) Failed.Broadcast(Error);
	else if (Index == INDEX_NONE) NoHit.Broadcast();
	else Completed.Broadcast(Index, Id);
	SetReadyToDestroy();
}

void UConfirmTrajectorySelectionAsyncAction::BeginDestroy()
{
	FTSTicker::GetCoreTicker().RemoveTicker(Watchdog);
	if (auto* Target = Actor.Get(); Target && Target->AsyncRibbonConfirmation.Get() == this)
	{
		Target->AsyncRibbonConfirmation.Reset();
		Target->PendingRibbonConfirmationFrame = 0;
	}
	Super::BeginDestroy();
}
