#include "UserSelectionManager.h"
#include "TrajectoryVisibilityBlueprintLibrary.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibilityZoneSelectionTest, "SpatialHash.VisibilityZones.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVisibilityZoneSelectionTest::RunTest(const FString& Parameters)
{
	const ETrajectoryVisibilityZone Zones[] = {
		ETrajectoryVisibilityZone::Collision, ETrajectoryVisibilityZone::Inner,
		ETrajectoryVisibilityZone::Middle, ETrajectoryVisibilityZone::Outer
	};
	for (int32 Mask = 0; Mask < 16; ++Mask)
	{
		for (const auto Zone : Zones)
		{
			const int32 Bit = static_cast<int32>(Zone);
			TestEqual(TEXT("Every combination supports independent checkbox state"),
				UTrajectoryVisibilityBlueprintLibrary::IsZoneVisible(Mask, Zone), (Mask & Bit) != 0);
			TestEqual(TEXT("Enabling retains other zones"),
				UTrajectoryVisibilityBlueprintLibrary::SetZoneVisible(Mask, Zone, true), Mask | Bit);
			TestEqual(TEXT("Disabling retains other zones"),
				UTrajectoryVisibilityBlueprintLibrary::SetZoneVisible(Mask, Zone, false), Mask & ~Bit);
		}
	}
	FUserTrajectorySelection Selection;
	TestTrue(TEXT("Orientation widgets visible by default"), Selection.TransformationIndicatorVisibility);
	Selection.TransformationIndicatorVisibility = false;
	TestFalse(TEXT("Zone radius guides hidden by default"), Selection.NeighborhoodVisibility);
	Selection.NeighborhoodVisibility = true;
	TestFalse(TEXT("Behavior guide hidden by default"), Selection.BehaviorVisibility);
	TestEqual(TEXT("All zones visible by default"), Selection.VisibleZonesMask, 15);
	Selection.TrajectoryId = 42;
	Selection.BehaviorRadius = 99.f;
	Selection.CollisionRadius = -1.f;
	Selection.ZoneInnerRadius = 3.f;
	Selection.MiddleRadius = 2.f;
	Selection.VisibleZonesMask = 31;
	auto* Manager = NewObject<UUserSelectionManager>();
	Manager->AddSelection(Selection);
	FUserTrajectorySelection Stored;
	TestTrue(TEXT("Selection stored"), Manager->GetSelectionByTrajectoryId(42, Stored));
	TestEqual(TEXT("Negative collision clamped"), Stored.CollisionRadius, 0.f);
	TestEqual(TEXT("Inner radius retained"), Stored.ZoneInnerRadius, 3.f);
	TestEqual(TEXT("Middle follows inner"), Stored.MiddleRadius, 3.f);
	TestEqual(TEXT("Unknown mask bits discarded"), Stored.VisibleZonesMask, 15);
	TestEqual(TEXT("Behavior threshold independent"), Stored.BehaviorRadius, 99.f);
	TestTrue(TEXT("Adding a selection preserves guide visibility"), Stored.NeighborhoodVisibility);
	TestFalse(TEXT("Adding a selection preserves hidden orientation widgets"), Stored.TransformationIndicatorVisibility);
	Selection.TransformationIndicatorVisibility = true;
	Selection.NeighborhoodVisibility = false;
	Selection.BehaviorVisibility = true;
	Selection.VisibleZonesMask = 0;
	Selection.CollisionRadius = std::numeric_limits<float>::quiet_NaN();
	Selection.ZoneInnerRadius = std::numeric_limits<float>::infinity();
	Selection.MiddleRadius = -1.f;
	TestTrue(TEXT("Update by source ID"), Manager->UpdateSelectionByTrajectoryId(42, Selection));
	Manager->GetSelectionByTrajectoryId(42, Stored);
	TestEqual(TEXT("Empty mask preserved"), Stored.VisibleZonesMask, 0);
	TestFalse(TEXT("Updating a selection can hide guides"), Stored.NeighborhoodVisibility);
	TestTrue(TEXT("Behavior guide stored independently"), Stored.BehaviorVisibility);
	TestTrue(TEXT("Updating a selection can show orientation widgets"), Stored.TransformationIndicatorVisibility);
	TestEqual(TEXT("Non-finite collision sanitized"), Stored.CollisionRadius, 0.f);
	TestEqual(TEXT("Non-finite inner sanitized"), Stored.ZoneInnerRadius, 0.f);
	TestEqual(TEXT("Negative middle sanitized"), Stored.MiddleRadius, 0.f);
	return true;
}
#endif
