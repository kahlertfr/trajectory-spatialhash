// Example usage of async query methods with TrajectoryDataCppApi
// This file demonstrates proper async query patterns without busy-waiting

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialHashTableManager.h"
#include "AsyncQueryExampleActor.generated.h"

/**
 * Example actor demonstrating async trajectory queries
 * Shows proper callback handling without busy-waiting
 */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API AAsyncQueryExampleActor : public AActor
{
	GENERATED_BODY()

public:
	AAsyncQueryExampleActor();

	// ============================================================================
	// EXAMPLE 1: Basic Async Query
	// ============================================================================
	
	/**
	 * Simple async query example
	 * Queries trajectories and logs results when ready
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example1_BasicAsyncQuery();

	// ============================================================================
	// EXAMPLE 2: Dual Radius Query
	// ============================================================================
	
	/**
	 * Dual radius query example
	 * Shows handling of two result arrays (inner/outer)
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example2_DualRadiusQuery();

	// ============================================================================
	// EXAMPLE 3: Time Range Query
	// ============================================================================
	
	/**
	 * Time range query example
	 * Queries trajectories over multiple timesteps
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example3_TimeRangeQuery();

	// ============================================================================
	// EXAMPLE 4: Query Trajectory Interaction
	// ============================================================================
	
	/**
	 * Query trajectory interaction example
	 * Finds trajectories near another trajectory
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example4_QueryTrajectoryInteraction();

	// ============================================================================
	// EXAMPLE 5: Sequential Async Queries
	// ============================================================================
	
	/**
	 * Sequential queries example
	 * Shows how to chain async queries properly
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example5_SequentialQueries();

	// ============================================================================
	// EXAMPLE 6: Visualization
	// ============================================================================
	
	/**
	 * Visualization example
	 * Queries and visualizes results with debug drawing
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example6_VisualizeResults();

	// ============================================================================
	// EXAMPLE 7: Error Handling
	// ============================================================================
	
	/**
	 * Error handling example
	 * Shows proper error checking and recovery
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example7_ErrorHandling();

	// ============================================================================
	// EXAMPLE 8: Member Function Callbacks
	// ============================================================================
	
	/**
	 * Member function callback example
	 * Uses UObject method instead of lambda
	 */
	UFUNCTION(BlueprintCallable, Category = "Examples|Async Queries")
	void Example8_MemberCallbacks();

private:
	/** Spatial hash table manager */
	UPROPERTY()
	USpatialHashTableManager* Manager;

	/** Example dataset directory */
	FString DatasetDirectory;

	/** Cached query results for visualization */
	TArray<FSpatialHashQueryResult> CachedResults;

	/** Callback for member function example */
	void OnQueryComplete_MemberFunction(const TArray<FSpatialHashQueryResult>& Results);

	/** Helper: Print query statistics */
	void PrintQueryStats(const TArray<FSpatialHashQueryResult>& Results, const FString& QueryName);
};
