// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialHashTableManager.h"
#include "UserSelectionManager.h"
#include "SampleTrajectoryNiagaraActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UENUM(BlueprintType)
enum class ESampleTrajectoryScenario : uint8
{
	A UMETA(DisplayName = "A - Straight Up"),
	B UMETA(DisplayName = "B - Straight Sideways"),
	C UMETA(DisplayName = "C - Straight Orthogonal"),
	D UMETA(DisplayName = "D - Diagonal"),
	E UMETA(DisplayName = "E - Curvy"),
	F UMETA(DisplayName = "F - Periodic Edge")
};

/**
 * Sample visualization actor that generates synthetic query/result trajectories
 * and transfers them to the same Niagara system used by ATrajectoryQueryNiagaraActor.
 */
UCLASS(BlueprintType, Blueprintable)
class SPATIALHASHEDTRAJECTORY_API ASampleTrajectoryNiagaraActor : public AActor
{
	GENERATED_BODY()

public:
	ASampleTrajectoryNiagaraActor();

	// ─── Sample Scenario Settings ────────────────────────────────────────────

	/** Which sample query trajectory (A–F) is currently visualized */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Data")
	ESampleTrajectoryScenario SelectedScenario = ESampleTrajectoryScenario::A;

	/** Number of timesteps for the generated sample data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Data", meta = (ClampMin = "2", UIMin = "2"))
	int32 NumTimeSteps = 360;

	/** Periodic simulation box size (world units per axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sample Data")
	FVector PeriodicVolumeExtent = FVector(5.0f, 5.0f, 5.0f);

	// ─── Niagara Settings ────────────────────────────────────────────────────

	/** Niagara System asset to spawn / configure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraSystem = nullptr;

	/** Niagara component that hosts the effect (read-only from Blueprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* NiagaraComponent = nullptr;

	// ─── Visualization Settings ──────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 VisualizationTimeStart = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 VisualizationTimeEnd = 359;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	ETrajectoryColorEncoding ColorEncoding = ETrajectoryColorEncoding::Time;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	ETrajectoryDistanceFilter DistanceFilter = ETrajectoryDistanceFilter::NoFilter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool ParticleVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool TimeRangeSensitivity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float ParticleRadius = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float InnerRadius = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float QueryRadius = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	float VisibilityRadius = 1.0f;

	// ─── User Selection Settings ─────────────────────────────────────────────

	/** Optional selection manager that can update visualization parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	UUserSelectionManager* SelectionManager = nullptr;

	// ─── Blueprint Callable API ──────────────────────────────────────────────

	/** Switch the active sample scenario and refresh the Niagara system */
	UFUNCTION(BlueprintCallable, Category = "Sample Visualization")
	void SetSelectedScenario(ESampleTrajectoryScenario NewScenario);

	/** Rebuild all sample trajectories (A-F) and refresh the current view */
	UFUNCTION(BlueprintCallable, Category = "Sample Visualization")
	void RebuildSampleData();

	/** Transfer the currently selected scenario to Niagara */
	UFUNCTION(BlueprintCallable, Category = "Sample Visualization")
	void RefreshVisualization();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FScenarioData
	{
		TArray<FVector> QueryPointsUnwrapped;
		TArray<FVector> QueryPointsRaw;
		TArray<FSpatialHashQueryResult> Results;
		FVector BoundsMin = FVector::ZeroVector;
		FVector BoundsMax = FVector::ZeroVector;
	};

	UPROPERTY()
	TArray<FScenarioData> ScenarioCache;

	bool bScenarioDataBuilt = false;

	UFUNCTION()
	void HandleSelectionsChanged(const TArray<FUserTrajectorySelection>& Selections);

	void ApplyUserSelection(const FUserTrajectorySelection& Selection);
	void UpdateUserParametersOnNiagara();

	void BuildScenarioData();
	void BuildScenarioData(ESampleTrajectoryScenario Scenario);
	void TransferScenarioToNiagara(ESampleTrajectoryScenario Scenario);
};
