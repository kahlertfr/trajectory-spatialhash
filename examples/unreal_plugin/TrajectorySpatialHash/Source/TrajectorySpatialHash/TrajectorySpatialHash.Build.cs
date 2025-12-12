// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TrajectorySpatialHash : ModuleRules
{
	public TrajectorySpatialHash(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
		
		// ==================================================================
		// APPROACH 1: Link Prebuilt Static Libraries (Recommended)
		// ==================================================================
		
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "../../ThirdParty/trajectory_spatialhash");
		
		// Add include path
		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "include"));
		
		// Link appropriate library based on target platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = Path.Combine(ThirdPartyPath, "lib/win64");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "trajectory_spatialhash.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = Path.Combine(ThirdPartyPath, "lib/mac");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libtrajectory_spatialhash.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string LibPath = Path.Combine(ThirdPartyPath, "lib/linux");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libtrajectory_spatialhash.a"));
		}
		
		// ==================================================================
		// APPROACH 2: Compile Sources Directly (Alternative for Development)
		// ==================================================================
		// Uncomment the following to compile library sources in-plugin:
		
		/*
		// Add library source files
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
		
		// Note: You would need to copy the library .cpp files to Private/ folder
		// and the .h files to Public/ folder
		
		// Enable exceptions and RTTI if needed
		bEnableExceptions = true;
		bUseRTTI = true;
		
		// Add C++17 support
		CppStandard = CppStandardVersion.Cpp17;
		*/
		
		// ==================================================================
		// Common Settings
		// ==================================================================
		
		// The library uses exceptions for error handling in the C++ API
		// The C wrapper (ue_wrapper.h) catches all exceptions, so this is safe
		// However, if you prefer, you can leave exceptions disabled and only use the C API
		bEnableExceptions = true;
		
		// C++17 is required
		CppStandard = CppStandardVersion.Cpp17;
	}
}
