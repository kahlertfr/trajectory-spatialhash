// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryHashPluginModule.h"

#define LOCTEXT_NAMESPACE "FTrajectoryHashPluginModule"

void FTrajectoryHashPluginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTemp, Log, TEXT("TrajectoryHashPlugin: Module started"));
}

void FTrajectoryHashPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogTemp, Log, TEXT("TrajectoryHashPlugin: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTrajectoryHashPluginModule, TrajectoryHashPlugin)
