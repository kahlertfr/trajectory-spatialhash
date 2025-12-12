// Copyright Epic Games, Inc. All Rights Reserved.

#include "TSHUEBridge.h"

// Include the C wrapper API
#include "trajectory_spatialhash/ue_wrapper.h"

FTSHGrid UTSHUEBridge::CreateGrid(float CellSize)
{
	FTSHGrid Grid;
	Grid.Handle = TSH_Create(static_cast<double>(CellSize));
	return Grid;
}

void UTSHUEBridge::FreeGrid(FTSHGrid& Grid)
{
	if (Grid.Handle)
	{
		TSH_Free(Grid.Handle);
		Grid.Handle = nullptr;
	}
}

bool UTSHUEBridge::BuildFromShards(FTSHGrid& Grid, const TArray<FString>& ShardPaths)
{
	if (!Grid.Handle || ShardPaths.Num() == 0)
	{
		return false;
	}
	
	// Convert FString paths to C strings
	TArray<const char*> CPaths;
	TArray<std::string> PathStrings; // Keep strings alive
	
	for (const FString& Path : ShardPaths)
	{
		PathStrings.Add(TCHAR_TO_UTF8(*Path));
		CPaths.Add(PathStrings.Last().c_str());
	}
	
	return TSH_BuildFromShards(Grid.Handle, CPaths.GetData(), static_cast<unsigned int>(CPaths.Num())) == 1;
}

bool UTSHUEBridge::SerializeGrid(const FTSHGrid& Grid, const FString& OutputPath)
{
	if (!Grid.Handle)
	{
		return false;
	}
	
	return TSH_Serialize(Grid.Handle, TCHAR_TO_UTF8(*OutputPath)) == 1;
}

bool UTSHUEBridge::LoadGrid(FTSHGrid& Grid, const FString& InputPath)
{
	if (!Grid.Handle)
	{
		return false;
	}
	
	return TSH_LoadGrid(Grid.Handle, TCHAR_TO_UTF8(*InputPath)) == 1;
}

bool UTSHUEBridge::QueryNearest(const FTSHGrid& Grid, FVector Location, FTSHPoint& OutPoint)
{
	if (!Grid.Handle)
	{
		return false;
	}
	
	TSH_Point CPoint;
	if (TSH_QueryNearest(Grid.Handle, Location.X, Location.Y, Location.Z, &CPoint) == 1)
	{
		OutPoint.X = static_cast<float>(CPoint.x);
		OutPoint.Y = static_cast<float>(CPoint.y);
		OutPoint.Z = static_cast<float>(CPoint.z);
		OutPoint.TrajectoryID = CPoint.trajectory_id;
		OutPoint.PointIndex = CPoint.point_index;
		return true;
	}
	
	return false;
}

bool UTSHUEBridge::QueryRadius(const FTSHGrid& Grid, FVector Location, float Radius, TArray<FTSHPoint>& OutPoints)
{
	if (!Grid.Handle)
	{
		return false;
	}
	
	TSH_QueryResult CResult;
	if (TSH_RadiusQuery(Grid.Handle, Location.X, Location.Y, Location.Z, static_cast<double>(Radius), &CResult) == 1)
	{
		OutPoints.Empty();
		OutPoints.Reserve(CResult.count);
		
		for (unsigned int i = 0; i < CResult.count; ++i)
		{
			FTSHPoint Point;
			Point.X = static_cast<float>(CResult.points[i].x);
			Point.Y = static_cast<float>(CResult.points[i].y);
			Point.Z = static_cast<float>(CResult.points[i].z);
			Point.TrajectoryID = CResult.points[i].trajectory_id;
			Point.PointIndex = CResult.points[i].point_index;
			OutPoints.Add(Point);
		}
		
		TSH_FreeQueryResult(&CResult);
		return true;
	}
	
	return false;
}

int32 UTSHUEBridge::GetPointCount(const FTSHGrid& Grid)
{
	if (!Grid.Handle)
	{
		return 0;
	}
	
	return static_cast<int32>(TSH_GetPointCount(Grid.Handle));
}

FString UTSHUEBridge::GetLastError(const FTSHGrid& Grid)
{
	if (!Grid.Handle)
	{
		return TEXT("Invalid grid handle");
	}
	
	const char* Error = TSH_GetLastError(Grid.Handle);
	if (Error)
	{
		return FString(UTF8_TO_TCHAR(Error));
	}
	
	return FString();
}
