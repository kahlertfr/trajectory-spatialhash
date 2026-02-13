// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashedTrajectoryModule.h"

#define LOCTEXT_NAMESPACE "FSpatialHashedTrajectoryModule"

void FSpatialHashedTrajectoryModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTemp, Log, TEXT("SpatialHashedTrajectory: Module started"));
}

void FSpatialHashedTrajectoryModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogTemp, Log, TEXT("SpatialHashedTrajectory: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSpatialHashedTrajectoryModule, SpatialHashedTrajectory)
