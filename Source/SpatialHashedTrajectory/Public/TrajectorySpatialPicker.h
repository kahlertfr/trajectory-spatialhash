#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrajectorySpatialPicker.generated.h"

class ATrajectoryQueryNiagaraActor;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Local hand tool: right index fingertip picks; left thumb taps a trigger near the left index knuckle. */
UCLASS(BlueprintType, Blueprintable)
class SPATIALHASHEDTRAJECTORY_API ATrajectorySpatialPicker : public AActor
{
	GENERATED_BODY()
public:
	ATrajectorySpatialPicker();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION(BlueprintCallable, Category = "Trajectory Picking", meta = (DefaultToSelf = "Target"))
	static ATrajectorySpatialPicker* EnableForTarget(ATrajectoryQueryNiagaraActor* Target);
	static ATrajectorySpatialPicker* FindForTarget(ATrajectoryQueryNiagaraActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Trajectory Picking")
	void TapTrigger();

	UFUNCTION(BlueprintCallable, Category = "Trajectory Picking")
	void Disable();

	/** Alternative tracking integration and deterministic input for tests. Positions are world centimeters. */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Picking")
	void UpdateHandPose(bool bTracked, FVector IndexTip, FVector TriggerPosition, FVector ThumbTip);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory Picking")
	bool bUseXRTracking = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory Picking", meta = (ClampMin = "0.1", ClampMax = "20"))
	float SelectionRadius = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory Picking", meta = (ClampMin = "0.1"))
	float TriggerRadius = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory Picking")
	FVector TriggerOffset = FVector(0, 0, 2);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trajectory Picking")
	bool bHandsTracked = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> TipSphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> HandTrigger;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> TriggerLabel;
private:
	UPROPERTY() TWeakObjectPtr<ATrajectoryQueryNiagaraActor> Target;
	FVector FingerPosition = FVector::ZeroVector;
	bool bTriggerLatched = true;
	double LastTap = 0;
};
