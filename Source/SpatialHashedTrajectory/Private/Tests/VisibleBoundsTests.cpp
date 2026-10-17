#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "VisibleSampleBoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibleSampleCollisionTest, "TrajectoryBLab.Query.VisibleBounds.CollisionPivot",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVisibleSampleCollisionTest::RunTest(const FString& Parameters)
{
	auto* Box = NewObject<UVisibleSampleBoxComponent>();
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	const FTransform Transform(FRotator(20, 45, 0), FVector(100, 200, 300), FVector(2));
	Box->SetWorldTransform(Transform);
	const FBox Samples(FVector(-10, 20, 30), FVector(30, 40, 50));
	Box->SetVisibleSampleBounds(Samples);
	TestTrue(TEXT("Fitting preserves the interaction pivot"), Box->GetComponentTransform().Equals(Transform));
	TestTrue(TEXT("Bounds follow visible geometry"),
			 Box->CalcBounds(Transform).GetBox().Equals(Samples.ExpandBy(0.1).TransformBy(Transform)));
	auto* Body = Box->GetBodySetup();
	TestTrue(TEXT("Physics shape uses the visible center"),
			 Body && Body->AggGeom.BoxElems.Num() == 1 && Body->AggGeom.BoxElems[0].Center.Equals(Samples.GetCenter()));
	Box->SetBoxExtent(FVector(1000));
	TestTrue(TEXT("Legacy full-result extent updates cannot override GPU fit"),
			 Box->GetUnscaledBoxExtent().Equals((Samples.GetExtent() + FVector(0.1))));
	Box->SetVisibleSampleBounds(FBox(ForceInit));
	TestEqual(TEXT("An empty view has no collision"), Box->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	Box->SetVisibleSampleBounds(Samples);
	TestEqual(TEXT("Returning samples restore the collision mode"), Box->GetCollisionEnabled(),
			  ECollisionEnabled::QueryOnly);
	auto* Other = NewObject<UVisibleSampleBoxComponent>();
	Other->SetVisibleSampleBounds(FBox(FVector(-1), FVector(1)));
	TestTrue(TEXT("Separate visualizations have independent collision geometry"),
			 Box->GetBodySetup() != Other->GetBodySetup() &&
				 Body->AggGeom.BoxElems[0].Center.Equals(Samples.GetCenter()));
	Box->SetVisibleSampleBounds(FBox(FVector(7), FVector(7)));
	TestTrue(TEXT("A point has a finite minimum interaction thickness"),
			 Box->GetUnscaledBoxExtent().Equals(FVector(0.1)));
	return true;
}
#endif
