// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Example usage of the Spatial Hash Table Manager with Fixed Radius Nearest Neighbor Queries
 * 
 * This example demonstrates all the query types supported by the plugin:
 * - Case A: Single point, single timestep query
 * - Case B: Single point, time range query
 * - Case C: Query trajectory over time range
 * - Dual radius queries
 * 
 * To use this example:
 * 1. Include this header in your Unreal Engine project
 * 2. Ensure TrajectoryData plugin is installed
 * 3. Have a dataset with trajectory data in shard files
 * 4. Load hash tables before querying
 */

#pragma once

#include "CoreMinimal.h"
#include "SpatialHashTableManager.h"
#include "GameFramework/Actor.h"
#include "FixedRadiusQueryExample.generated.h"

/**
 * Example actor demonstrating fixed radius nearest neighbor queries
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API AFixedRadiusQueryExample : public AActor
{
	GENERATED_BODY()

public:
	AFixedRadiusQueryExample();

	/** Path to dataset directory containing shard files */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	FString DatasetDirectory;

	/** Cell size for hash tables (must match loaded tables) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	float CellSize = 10.0f;

	/** Query radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	float QueryRadius = 50.0f;

	/** Inner radius for dual radius queries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	float InnerRadius = 25.0f;

	/** Start time step for time range queries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	int32 StartTimeStep = 0;

	/** End time step for time range queries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	int32 EndTimeStep = 100;

	/** Query trajectory ID for trajectory-based queries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	int32 QueryTrajectoryId = 0;

	/** Whether to visualize query results */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bVisualizeResults = true;

	/** Color for inner radius visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor InnerRadiusColor = FLinearColor::Green;

	/** Color for outer radius visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	FLinearColor OuterRadiusColor = FLinearColor::Yellow;

protected:
	virtual void BeginPlay() override;

public:
	/**
	 * Example: Case A - Query single point at single timestep
	 * Returns one sample per trajectory within radius at the specified timestep
	 */
	UFUNCTION(BlueprintCallable, Category = "Query Examples")
	void ExampleCaseA_SinglePointSingleTimestep();

	/**
	 * Example: Case B - Query single point over time range
	 * Returns trajectories with samples within radius over the time range
	 */
	UFUNCTION(BlueprintCallable, Category = "Query Examples")
	void ExampleCaseB_SinglePointTimeRange();

	/**
	 * Example: Case C - Query trajectory over time range
	 * Returns trajectories intersecting with the moving query trajectory
	 */
	UFUNCTION(BlueprintCallable, Category = "Query Examples")
	void ExampleCaseC_TrajectoryTimeRange();

	/**
	 * Example: Dual radius query
	 * Queries inner and outer radius simultaneously
	 */
	UFUNCTION(BlueprintCallable, Category = "Query Examples")
	void ExampleDualRadius();

	/**
	 * Initialize the manager and load hash tables
	 */
	UFUNCTION(BlueprintCallable, Category = "Setup")
	bool InitializeManager();

	/**
	 * Visualize query results in the world
	 */
	UFUNCTION(BlueprintCallable, Category = "Visualization")
	void VisualizeResults(const TArray<FTrajectoryQueryResult>& Results, FLinearColor Color);

	/**
	 * Draw a sphere to represent the query radius
	 */
	UFUNCTION(BlueprintCallable, Category = "Visualization")
	void DrawQueryRadius(FVector Center, float Radius, FLinearColor Color);

private:
	/** Manager instance */
	UPROPERTY()
	USpatialHashTableManager* Manager;

	/** Helper to log query results */
	void LogQueryResults(const TArray<FTrajectoryQueryResult>& Results, const FString& QueryName);
};

// Implementation

AFixedRadiusQueryExample::AFixedRadiusQueryExample()
{
	PrimaryActorTick.bCanEverTick = false;
	Manager = nullptr;
}

void AFixedRadiusQueryExample::BeginPlay()
{
	Super::BeginPlay();

	// Auto-initialize on begin play
	if (!DatasetDirectory.IsEmpty())
	{
		InitializeManager();
	}
}

bool AFixedRadiusQueryExample::InitializeManager()
{
	if (DatasetDirectory.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Dataset directory not set"));
		return false;
	}

	// Create manager if needed
	if (!Manager)
	{
		Manager = NewObject<USpatialHashTableManager>(this);
	}

	// Load hash tables for the time range
	int32 LoadedCount = Manager->LoadHashTables(
		DatasetDirectory,
		CellSize,
		StartTimeStep,
		EndTimeStep,
		true  // Auto-create if needed
	);

	if (LoadedCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load hash tables"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Successfully loaded %d hash tables"), LoadedCount);
	return true;
}

void AFixedRadiusQueryExample::ExampleCaseA_SinglePointSingleTimestep()
{
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("Manager not initialized. Call InitializeManager first."));
		return;
	}

	// Use actor location as query point
	FVector QueryPosition = GetActorLocation();
	int32 TimeStep = StartTimeStep;

	UE_LOG(LogTemp, Log, TEXT("=== Case A: Single Point, Single Timestep ==="));
	UE_LOG(LogTemp, Log, TEXT("Query Position: %s, Radius: %.2f, TimeStep: %d"),
		*QueryPosition.ToString(), QueryRadius, TimeStep);

	// Execute query
	TArray<FTrajectoryQueryResult> Results;
	int32 NumFound = Manager->QueryRadiusWithDistanceCheck(
		DatasetDirectory,
		QueryPosition,
		QueryRadius,
		CellSize,
		TimeStep,
		Results
	);

	UE_LOG(LogTemp, Log, TEXT("Found %d trajectories"), NumFound);
	
	// Log and visualize results
	LogQueryResults(Results, TEXT("Case A"));
	
	if (bVisualizeResults)
	{
		DrawQueryRadius(QueryPosition, QueryRadius, InnerRadiusColor);
		VisualizeResults(Results, InnerRadiusColor);
	}
}

void AFixedRadiusQueryExample::ExampleCaseB_SinglePointTimeRange()
{
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("Manager not initialized. Call InitializeManager first."));
		return;
	}

	FVector QueryPosition = GetActorLocation();

	UE_LOG(LogTemp, Log, TEXT("=== Case B: Single Point, Time Range ==="));
	UE_LOG(LogTemp, Log, TEXT("Query Position: %s, Radius: %.2f, TimeSteps: %d-%d"),
		*QueryPosition.ToString(), QueryRadius, StartTimeStep, EndTimeStep);

	// Execute query
	TArray<FTrajectoryQueryResult> Results;
	int32 NumFound = Manager->QueryRadiusOverTimeRange(
		DatasetDirectory,
		QueryPosition,
		QueryRadius,
		CellSize,
		StartTimeStep,
		EndTimeStep,
		Results
	);

	UE_LOG(LogTemp, Log, TEXT("Found %d trajectories with samples in time range"), NumFound);
	
	// Log and visualize results
	LogQueryResults(Results, TEXT("Case B"));
	
	if (bVisualizeResults)
	{
		DrawQueryRadius(QueryPosition, QueryRadius, InnerRadiusColor);
		VisualizeResults(Results, InnerRadiusColor);
	}
}

void AFixedRadiusQueryExample::ExampleCaseC_TrajectoryTimeRange()
{
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("Manager not initialized. Call InitializeManager first."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("=== Case C: Query Trajectory Over Time Range ==="));
	UE_LOG(LogTemp, Log, TEXT("Query Trajectory ID: %d, Radius: %.2f, TimeSteps: %d-%d"),
		QueryTrajectoryId, QueryRadius, StartTimeStep, EndTimeStep);

	// Execute query
	TArray<FTrajectoryQueryResult> Results;
	int32 NumFound = Manager->QueryTrajectoryRadiusOverTimeRange(
		DatasetDirectory,
		QueryTrajectoryId,
		QueryRadius,
		CellSize,
		StartTimeStep,
		EndTimeStep,
		Results
	);

	UE_LOG(LogTemp, Log, TEXT("Found %d trajectories intersecting with query trajectory"), NumFound);
	
	// Log and visualize results
	LogQueryResults(Results, TEXT("Case C"));
	
	if (bVisualizeResults)
	{
		VisualizeResults(Results, InnerRadiusColor);
	}
}

void AFixedRadiusQueryExample::ExampleDualRadius()
{
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("Manager not initialized. Call InitializeManager first."));
		return;
	}

	FVector QueryPosition = GetActorLocation();
	int32 TimeStep = StartTimeStep;

	UE_LOG(LogTemp, Log, TEXT("=== Dual Radius Query ==="));
	UE_LOG(LogTemp, Log, TEXT("Query Position: %s, Inner: %.2f, Outer: %.2f, TimeStep: %d"),
		*QueryPosition.ToString(), InnerRadius, QueryRadius, TimeStep);

	// Execute query
	TArray<FTrajectoryQueryResult> InnerResults;
	TArray<FTrajectoryQueryResult> OuterOnlyResults;
	
	int32 TotalFound = Manager->QueryDualRadiusWithDistanceCheck(
		DatasetDirectory,
		QueryPosition,
		InnerRadius,
		QueryRadius,
		CellSize,
		TimeStep,
		InnerResults,
		OuterOnlyResults
	);

	UE_LOG(LogTemp, Log, TEXT("Found %d trajectories total: %d in inner radius, %d in outer ring"),
		TotalFound, InnerResults.Num(), OuterOnlyResults.Num());
	
	// Log results separately
	LogQueryResults(InnerResults, TEXT("Inner Radius"));
	LogQueryResults(OuterOnlyResults, TEXT("Outer Ring"));
	
	// Visualize with different colors
	if (bVisualizeResults)
	{
		DrawQueryRadius(QueryPosition, InnerRadius, InnerRadiusColor);
		DrawQueryRadius(QueryPosition, QueryRadius, OuterRadiusColor);
		VisualizeResults(InnerResults, InnerRadiusColor);
		VisualizeResults(OuterOnlyResults, OuterRadiusColor);
	}
}

void AFixedRadiusQueryExample::LogQueryResults(
	const TArray<FTrajectoryQueryResult>& Results,
	const FString& QueryName)
{
	UE_LOG(LogTemp, Log, TEXT("--- %s Results ---"), *QueryName);
	
	for (const FTrajectoryQueryResult& Result : Results)
	{
		UE_LOG(LogTemp, Log, TEXT("Trajectory %d: %d sample points"),
			Result.TrajectoryId, Result.SamplePoints.Num());
		
		// Log first few samples
		int32 SamplesToLog = FMath::Min(3, Result.SamplePoints.Num());
		for (int32 i = 0; i < SamplesToLog; ++i)
		{
			const FTrajectorySamplePoint& Sample = Result.SamplePoints[i];
			UE_LOG(LogTemp, Log, TEXT("  Sample %d: Pos(%s), Distance: %.2f, TimeStep: %d"),
				i, *Sample.Position.ToString(), Sample.Distance, Sample.TimeStep);
		}
		
		if (Result.SamplePoints.Num() > 3)
		{
			UE_LOG(LogTemp, Log, TEXT("  ... and %d more samples"),
				Result.SamplePoints.Num() - 3);
		}
	}
}

void AFixedRadiusQueryExample::VisualizeResults(
	const TArray<FTrajectoryQueryResult>& Results,
	FLinearColor Color)
{
	if (!GetWorld())
		return;

	const float SphereRadius = 5.0f;
	const float LineDuration = 10.0f;

	for (const FTrajectoryQueryResult& Result : Results)
	{
		// Draw trajectory path
		for (int32 i = 0; i < Result.SamplePoints.Num(); ++i)
		{
			const FTrajectorySamplePoint& Sample = Result.SamplePoints[i];
			
			// Draw sphere at each sample point
			DrawDebugSphere(
				GetWorld(),
				Sample.Position,
				SphereRadius,
				12,
				Color.ToFColor(true),
				false,
				LineDuration
			);
			
			// Draw line to next sample
			if (i < Result.SamplePoints.Num() - 1)
			{
				DrawDebugLine(
					GetWorld(),
					Sample.Position,
					Result.SamplePoints[i + 1].Position,
					Color.ToFColor(true),
					false,
					LineDuration,
					0,
					1.0f
				);
			}
		}
	}
}

void AFixedRadiusQueryExample::DrawQueryRadius(
	FVector Center,
	float Radius,
	FLinearColor Color)
{
	if (!GetWorld())
		return;

	const int32 Segments = 32;
	const float LineDuration = 10.0f;

	DrawDebugSphere(
		GetWorld(),
		Center,
		Radius,
		Segments,
		Color.ToFColor(true),
		false,
		LineDuration,
		0,
		2.0f
	);
}
