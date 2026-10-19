#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "VisibleSampleBoxComponent.h"
#include "TrajectoryQueryNiagaraActor.h"

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
	const FTransform OtherTransform = Other->GetComponentTransform();
	const FBox OtherBounds = Other->CalcBounds(OtherTransform).GetBox();
	Box->SetWorldScale3D(FVector(4));
	Box->SetVisibleSampleBounds(FBox(FVector(-50), FVector(50)));
	TestTrue(TEXT("Scaling and refitting one view leaves the other view unchanged"),
		Other->GetComponentTransform().Equals(OtherTransform) &&
		Other->CalcBounds(OtherTransform).GetBox().Equals(OtherBounds));
	Box->SetVisibleSampleBounds(FBox(FVector(7), FVector(7)));
	TestTrue(TEXT("A point has a finite minimum interaction thickness"),
			 Box->GetUnscaledBoxExtent().Equals(FVector(0.1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibleBoundsRevisionTest, "TrajectoryBLab.Query.VisibleBounds.StaleReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVisibleBoundsRevisionTest::RunTest(const FString& Parameters)
{
	auto* Actor = NewObject<ATrajectoryQueryNiagaraActor>();
	const FBox Initial(FVector(-10), FVector(10));
	const FBox Stale(FVector(-100), FVector(100));
	const uint64 Revision = Actor->GetVisibleBoundsRevision();
	Actor->ReceiveVisibleBounds(Revision, Initial, Initial);
	TestTrue(TEXT("Current readbacks are accepted"), Actor->GetVisiblePathBounds().Equals(Initial));
	Actor->CancelQuery();
	Actor->ReceiveVisibleBounds(Revision, Stale, Stale);
	TestTrue(TEXT("Cancelled query readbacks cannot replace current bounds"),
		Actor->GetVisiblePathBounds().Equals(Initial) && Actor->GetVisibleObjectBounds().Equals(Initial));
	Actor->ReceiveVisibleBounds(Actor->GetVisibleBoundsRevision(), Stale, FBox(ForceInit));
	TestTrue(TEXT("New revisions accept independent empty object bounds"),
		Actor->GetVisiblePathBounds().Equals(Stale) && !Actor->GetVisibleObjectBounds().IsValid);
	return true;
}
#endif
