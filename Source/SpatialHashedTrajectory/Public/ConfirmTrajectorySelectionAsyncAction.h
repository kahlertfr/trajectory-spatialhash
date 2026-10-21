#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ConfirmTrajectorySelectionAsyncAction.generated.h"

class ATrajectoryQueryNiagaraActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrajectoryConfirmationCompleted, int32, TrajectoryIndex, int64, TrajectoryId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrajectoryConfirmationNoHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrajectoryConfirmationFailed, FString, Reason);

/** Confirms one GPU ribbon pick. Keep refreshing the selector until an output fires. */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API UConfirmTrajectorySelectionAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Trajectory Inspection",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
		DisplayName = "Confirm Trajectory Selection (Async)"))
	static UConfirmTrajectorySelectionAsyncAction* ConfirmTrajectorySelection(
		UObject* WorldContextObject, ATrajectoryQueryNiagaraActor* TargetActor);

	/** GPU readback accepted and selection committed; safe to disable the selector here. */
	UPROPERTY(BlueprintAssignable)
	FOnTrajectoryConfirmationCompleted Completed;
	UPROPERTY(BlueprintAssignable)
	FOnTrajectoryConfirmationNoHit NoHit;
	UPROPERTY(BlueprintAssignable)
	FOnTrajectoryConfirmationFailed Failed;

	virtual void Activate() override;
	virtual void BeginDestroy() override;

private:
	friend class ATrajectoryQueryNiagaraActor;
	friend class FTrajectoryRibbonPickingTest;
	friend class FTrajectoryRibbonPickingIntegrationTest;
	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> Actor;
	FTSTicker::FDelegateHandle Watchdog;
	double Deadline = 0;
	uint64 Revision = 0;
	uint64 Session = 0;
	bool bFinished = false;
	bool bActivated = false;
	bool CheckRequest(float DeltaTime);
	void Finish(int32 Index, int64 Id, const FString& Error = FString());
};
