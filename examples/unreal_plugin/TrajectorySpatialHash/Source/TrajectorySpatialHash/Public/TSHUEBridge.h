// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TSHUEBridge.generated.h"

// Forward declare the opaque C handle type
typedef void* TSH_Handle;

/**
 * Unreal-friendly wrapper for trajectory spatial hash point
 */
USTRUCT(BlueprintType)
struct FTSHPoint
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Trajectory Spatial Hash")
	float X = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category = "Trajectory Spatial Hash")
	float Y = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category = "Trajectory Spatial Hash")
	float Z = 0.0f;
	
	UPROPERTY(BlueprintReadWrite, Category = "Trajectory Spatial Hash")
	int32 TrajectoryID = 0;
	
	UPROPERTY(BlueprintReadWrite, Category = "Trajectory Spatial Hash")
	int32 PointIndex = 0;
};

/**
 * Opaque handle to a spatial hash grid
 */
USTRUCT(BlueprintType)
struct FTSHGrid
{
	GENERATED_BODY()
	
	TSH_Handle Handle = nullptr;
};

/**
 * Blueprint function library providing access to trajectory spatial hash functionality
 * 
 * This wraps the C API (ue_wrapper.h) for safe use from Unreal Engine C++ and Blueprints.
 */
UCLASS()
class TRAJECTORYSPATIALH_API UTSHUEBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/**
	 * Create a new spatial hash grid
	 * @param CellSize Size of each cell in the spatial hash
	 * @return Grid handle
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static FTSHGrid CreateGrid(float CellSize = 10.0f);
	
	/**
	 * Free a spatial hash grid
	 * @param Grid Grid to free
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static void FreeGrid(UPARAM(ref) FTSHGrid& Grid);
	
	/**
	 * Build spatial hash grid from trajectory shard CSV files
	 * @param Grid Grid to build into
	 * @param ShardPaths Array of file paths to shard CSV files
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static bool BuildFromShards(UPARAM(ref) FTSHGrid& Grid, const TArray<FString>& ShardPaths);
	
	/**
	 * Serialize grid to binary file
	 * @param Grid Grid to serialize
	 * @param OutputPath Path to output file
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static bool SerializeGrid(const FTSHGrid& Grid, const FString& OutputPath);
	
	/**
	 * Load grid from binary file
	 * @param Grid Grid to load into
	 * @param InputPath Path to input file
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static bool LoadGrid(UPARAM(ref) FTSHGrid& Grid, const FString& InputPath);
	
	/**
	 * Query for the nearest point
	 * @param Grid Grid to query
	 * @param Location Query location
	 * @param OutPoint Output point
	 * @return True if a point was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static bool QueryNearest(const FTSHGrid& Grid, FVector Location, FTSHPoint& OutPoint);
	
	/**
	 * Query for all points within a radius
	 * @param Grid Grid to query
	 * @param Location Query location
	 * @param Radius Search radius
	 * @param OutPoints Output points array
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static bool QueryRadius(const FTSHGrid& Grid, FVector Location, float Radius, TArray<FTSHPoint>& OutPoints);
	
	/**
	 * Get the number of points in the grid
	 * @param Grid Grid to query
	 * @return Number of points
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static int32 GetPointCount(const FTSHGrid& Grid);
	
	/**
	 * Get the last error message
	 * @param Grid Grid to query
	 * @return Error message or empty string
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Spatial Hash")
	static FString GetLastError(const FTSHGrid& Grid);
};
