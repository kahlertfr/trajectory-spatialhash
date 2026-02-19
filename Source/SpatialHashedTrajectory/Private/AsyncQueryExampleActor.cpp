// Example usage of async query methods with TrajectoryDataCppApi
// This file demonstrates proper async query patterns without busy-waiting

#include "AsyncQueryExampleActor.h"
#include "DrawDebugHelpers.h"

AAsyncQueryExampleActor::AAsyncQueryExampleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// Create spatial hash manager
	Manager = NewObject<USpatialHashTableManager>();
	
	// Set example dataset path (modify for your setup)
	DatasetDirectory = TEXT("C:/Data/Trajectories");
}

// ============================================================================
// EXAMPLE 1: Basic Async Query
// ============================================================================

void AAsyncQueryExampleActor::Example1_BasicAsyncQuery()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 1: Basic Async Query ==="));
	
	// Define query parameters
	FVector QueryPosition = GetActorLocation();
	float Radius = 500.0f;
	float CellSize = 10.0f;
	int32 TimeStep = 100;
	
	UE_LOG(LogTemp, Log, TEXT("Starting async query at position (%.1f, %.1f, %.1f)..."),
		QueryPosition.X, QueryPosition.Y, QueryPosition.Z);
	
	// Start async query - NO BUSY WAITING!
	Manager->QueryRadiusWithDistanceCheckAsync(
		DatasetDirectory,
		QueryPosition,
		Radius,
		CellSize,
		TimeStep,
		FOnSpatialHashQueryComplete::CreateLambda([QueryPosition, Radius](const TArray<FSpatialHashQueryResult>& Results)
		{
			// This callback executes when data is ready
			UE_LOG(LogTemp, Log, TEXT("✓ Query complete! Found %d trajectories"), Results.Num());
			
			// Process results
			for (const FSpatialHashQueryResult& Result : Results)
			{
				UE_LOG(LogTemp, Log, TEXT("  Trajectory %d: %d samples"), 
					Result.TrajectoryId, Result.SamplePoints.Num());
				
				// Example: Log closest sample
				if (Result.SamplePoints.Num() > 0)
				{
					float MinDist = FLT_MAX;
					const FTrajectorySamplePoint* ClosestSample = nullptr;
					
					for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
					{
						float Dist = FVector::Dist(Sample.Position, QueryPosition);
						if (Dist < MinDist)
						{
							MinDist = Dist;
							ClosestSample = &Sample;
						}
					}
					
					if (ClosestSample)
					{
						UE_LOG(LogTemp, Log, TEXT("    Closest sample: t=%d, dist=%.1f"),
							ClosestSample->TimeStep, MinDist);
					}
				}
			}
		})
	);
	
	// Game thread continues immediately - no blocking!
	UE_LOG(LogTemp, Log, TEXT("Query started - game thread continues..."));
}

// ============================================================================
// EXAMPLE 2: Dual Radius Query
// ============================================================================

void AAsyncQueryExampleActor::Example2_DualRadiusQuery()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 2: Dual Radius Query ==="));
	
	FVector QueryPosition = GetActorLocation();
	float InnerRadius = 200.0f;
	float OuterRadius = 500.0f;
	
	Manager->QueryDualRadiusWithDistanceCheckAsync(
		DatasetDirectory,
		QueryPosition,
		InnerRadius,
		OuterRadius,
		10.0f,
		100,
		FOnSpatialHashDualQueryComplete::CreateLambda([InnerRadius, OuterRadius](
			const TArray<FSpatialHashQueryResult>& InnerResults,
			const TArray<FSpatialHashQueryResult>& OuterResults)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Dual query complete!"));
			UE_LOG(LogTemp, Log, TEXT("  Inner radius (%.1f): %d trajectories"), 
				InnerRadius, InnerResults.Num());
			UE_LOG(LogTemp, Log, TEXT("  Outer radius (%.1f): %d trajectories (includes inner samples)"), 
				OuterRadius, OuterResults.Num());
			
			// Process inner results (high priority)
			UE_LOG(LogTemp, Log, TEXT("  Processing inner results (high priority):"));
			for (const FSpatialHashQueryResult& Result : InnerResults)
			{
				UE_LOG(LogTemp, Log, TEXT("    Inner trajectory %d"), Result.TrajectoryId);
			}
			
			// Process outer results (includes inner samples for consistent trajectories)
			UE_LOG(LogTemp, Log, TEXT("  Processing outer results:"));
			for (const FSpatialHashQueryResult& Result : OuterResults)
			{
				UE_LOG(LogTemp, Log, TEXT("    Outer trajectory %d"), Result.TrajectoryId);
			}
		})
	);
	
	UE_LOG(LogTemp, Log, TEXT("Dual query started..."));
}

// ============================================================================
// EXAMPLE 3: Time Range Query
// ============================================================================

void AAsyncQueryExampleActor::Example3_TimeRangeQuery()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 3: Time Range Query ==="));
	
	FVector QueryPosition = GetActorLocation();
	int32 StartTime = 0;
	int32 EndTime = 100;
	
	Manager->QueryRadiusOverTimeRangeAsync(
		DatasetDirectory,
		QueryPosition,
		500.0f,
		10.0f,
		StartTime,
		EndTime,
		FOnSpatialHashQueryComplete::CreateLambda([StartTime, EndTime](const TArray<FSpatialHashQueryResult>& Results)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Time range query complete!"));
			UE_LOG(LogTemp, Log, TEXT("  Time range: %d to %d"), StartTime, EndTime);
			UE_LOG(LogTemp, Log, TEXT("  Found %d trajectories"), Results.Num());
			
			// Analyze temporal patterns
			for (const FSpatialHashQueryResult& Result : Results)
			{
				if (Result.SamplePoints.Num() > 0)
				{
					int32 FirstTime = Result.SamplePoints[0].TimeStep;
					int32 LastTime = Result.SamplePoints.Last().TimeStep;
					int32 Duration = LastTime - FirstTime;
					
					UE_LOG(LogTemp, Log, TEXT("  Trajectory %d: %d samples from t=%d to t=%d (duration=%d)"),
						Result.TrajectoryId, Result.SamplePoints.Num(), 
						FirstTime, LastTime, Duration);
				}
			}
		})
	);
	
	UE_LOG(LogTemp, Log, TEXT("Time range query started..."));
}

// ============================================================================
// EXAMPLE 4: Query Trajectory Interaction
// ============================================================================

void AAsyncQueryExampleActor::Example4_QueryTrajectoryInteraction()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 4: Query Trajectory Interaction ==="));
	
	uint32 QueryTrajectoryId = 12345;
	float InteractionRadius = 200.0f;
	
	Manager->QueryTrajectoryRadiusOverTimeRangeAsync(
		DatasetDirectory,
		QueryTrajectoryId,
		InteractionRadius,
		10.0f,
		0, 100,
		FOnSpatialHashQueryComplete::CreateLambda([QueryTrajectoryId, InteractionRadius](const TArray<FSpatialHashQueryResult>& Results)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ Trajectory interaction query complete!"));
			UE_LOG(LogTemp, Log, TEXT("  Query trajectory: %d"), QueryTrajectoryId);
			UE_LOG(LogTemp, Log, TEXT("  Interaction radius: %.1f"), InteractionRadius);
			UE_LOG(LogTemp, Log, TEXT("  Found %d interacting trajectories"), Results.Num());
			
			// Find closest interactions
			for (const FSpatialHashQueryResult& Result : Results)
			{
				float MinDistance = FLT_MAX;
				int32 MinDistanceTime = -1;
				
				for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
				{
					if (Sample.Distance < MinDistance)
					{
						MinDistance = Sample.Distance;
						MinDistanceTime = Sample.TimeStep;
					}
				}
				
				UE_LOG(LogTemp, Log, TEXT("  Trajectory %d: closest at t=%d, dist=%.1f"),
					Result.TrajectoryId, MinDistanceTime, MinDistance);
			}
		})
	);
	
	UE_LOG(LogTemp, Log, TEXT("Trajectory interaction query started..."));
}

// ============================================================================
// EXAMPLE 5: Sequential Async Queries
// ============================================================================

void AAsyncQueryExampleActor::Example5_SequentialQueries()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 5: Sequential Queries ==="));
	
	FVector QueryPos = GetActorLocation();
	
	// First query
	Manager->QueryRadiusWithDistanceCheckAsync(
		DatasetDirectory, QueryPos, 500.0f, 10.0f, 50,
		FOnSpatialHashQueryComplete::CreateLambda([this, QueryPos](const TArray<FSpatialHashQueryResult>& FirstResults)
		{
			UE_LOG(LogTemp, Log, TEXT("✓ First query complete: %d results"), FirstResults.Num());
			
			// Based on first results, start second query
			if (FirstResults.Num() > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("Starting second query..."));
				
				Manager->QueryRadiusWithDistanceCheckAsync(
					DatasetDirectory, QueryPos, 300.0f, 10.0f, 100,
					FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& SecondResults)
					{
						UE_LOG(LogTemp, Log, TEXT("✓ Second query complete: %d results"), SecondResults.Num());
						UE_LOG(LogTemp, Log, TEXT("All sequential queries finished!"));
					})
				);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("No results from first query - skipping second query"));
			}
		})
	);
	
	UE_LOG(LogTemp, Log, TEXT("Sequential queries started..."));
}

// ============================================================================
// EXAMPLE 6: Visualization
// ============================================================================

void AAsyncQueryExampleActor::Example6_VisualizeResults()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 6: Visualization ==="));
	
	FVector QueryPosition = GetActorLocation();
	float Radius = 500.0f;
	
	// Capture 'this' safely with weak pointer
	TWeakObjectPtr<AAsyncQueryExampleActor> WeakThis(this);
	
	Manager->QueryRadiusWithDistanceCheckAsync(
		DatasetDirectory, QueryPosition, Radius, 10.0f, 100,
		FOnSpatialHashQueryComplete::CreateLambda([WeakThis, QueryPosition, Radius](const TArray<FSpatialHashQueryResult>& Results)
		{
			if (!WeakThis.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Actor destroyed before callback"));
				return;
			}
			
			AAsyncQueryExampleActor* This = WeakThis.Get();
			UWorld* World = This->GetWorld();
			
			if (!World)
			{
				return;
			}
			
			UE_LOG(LogTemp, Log, TEXT("✓ Query complete - visualizing %d trajectories"), Results.Num());
			
			// Draw query sphere
			DrawDebugSphere(World, QueryPosition, Radius, 32, FColor::Yellow, false, 5.0f);
			
			// Draw each trajectory
			int32 ColorIndex = 0;
			TArray<FColor> Colors = {FColor::Red, FColor::Green, FColor::Blue, FColor::Cyan, FColor::Magenta};
			
			for (const FSpatialHashQueryResult& Result : Results)
			{
				FColor TrajectoryColor = Colors[ColorIndex % Colors.Num()];
				ColorIndex++;
				
				// Draw trajectory samples as connected line
				for (int32 i = 0; i < Result.SamplePoints.Num() - 1; ++i)
				{
					const FVector& Start = Result.SamplePoints[i].Position;
					const FVector& End = Result.SamplePoints[i + 1].Position;
					
					DrawDebugLine(World, Start, End, TrajectoryColor, false, 5.0f, 0, 2.0f);
					DrawDebugPoint(World, Start, 5.0f, TrajectoryColor, false, 5.0f);
				}
				
				// Draw last point
				if (Result.SamplePoints.Num() > 0)
				{
					DrawDebugPoint(World, Result.SamplePoints.Last().Position, 
						5.0f, TrajectoryColor, false, 5.0f);
				}
			}
		})
	);
	
	UE_LOG(LogTemp, Log, TEXT("Visualization query started..."));
}

// ============================================================================
// EXAMPLE 7: Error Handling
// ============================================================================

void AAsyncQueryExampleActor::Example7_ErrorHandling()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 7: Error Handling ==="));
	
	// Intentionally use invalid parameters to demonstrate error handling
	FString InvalidDirectory = TEXT("C:/Invalid/Path");
	
	Manager->QueryRadiusWithDistanceCheckAsync(
		InvalidDirectory,
		GetActorLocation(),
		500.0f,
		10.0f,
		100,
		FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& Results)
		{
			// Callback is ALWAYS invoked, even on error
			if (Results.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Query returned no results (may indicate error)"));
				// Handle error case gracefully
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("✓ Query succeeded with %d results"), Results.Num());
			}
		})
	);
	
	UE_LOG(LogTemp, Log, TEXT("Error handling example started (check for warnings)..."));
}

// ============================================================================
// EXAMPLE 8: Member Function Callbacks
// ============================================================================

void AAsyncQueryExampleActor::Example8_MemberCallbacks()
{
	UE_LOG(LogTemp, Log, TEXT("=== Example 8: Member Function Callbacks ==="));
	
	// Use UObject method instead of lambda
	// Safer for accessing member variables
	Manager->QueryRadiusWithDistanceCheckAsync(
		DatasetDirectory,
		GetActorLocation(),
		500.0f,
		10.0f,
		100,
		FOnSpatialHashQueryComplete::CreateUObject(this, &AAsyncQueryExampleActor::OnQueryComplete_MemberFunction)
	);
	
	UE_LOG(LogTemp, Log, TEXT("Member callback query started..."));
}

void AAsyncQueryExampleActor::OnQueryComplete_MemberFunction(const TArray<FSpatialHashQueryResult>& Results)
{
	UE_LOG(LogTemp, Log, TEXT("✓ Member function callback invoked"));
	
	// Can safely access member variables
	CachedResults = Results;
	
	PrintQueryStats(Results, TEXT("Member Callback Query"));
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void AAsyncQueryExampleActor::PrintQueryStats(const TArray<FSpatialHashQueryResult>& Results, const FString& QueryName)
{
	UE_LOG(LogTemp, Log, TEXT("Statistics for '%s':"), *QueryName);
	UE_LOG(LogTemp, Log, TEXT("  Total trajectories: %d"), Results.Num());
	
	int32 TotalSamples = 0;
	float TotalDistance = 0.0f;
	
	for (const FSpatialHashQueryResult& Result : Results)
	{
		TotalSamples += Result.SamplePoints.Num();
		
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			TotalDistance += Sample.Distance;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("  Total samples: %d"), TotalSamples);
	
	if (TotalSamples > 0)
	{
		float AvgDistance = TotalDistance / TotalSamples;
		UE_LOG(LogTemp, Log, TEXT("  Average distance: %.1f"), AvgDistance);
	}
}
