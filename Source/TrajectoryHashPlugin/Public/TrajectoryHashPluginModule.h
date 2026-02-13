// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Trajectory Hash Plugin Module
 * 
 * This module provides spatial hash table functionality for efficient
 * fixed radius nearest neighbor searches on trajectory data.
 */
class FTrajectoryHashPluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
