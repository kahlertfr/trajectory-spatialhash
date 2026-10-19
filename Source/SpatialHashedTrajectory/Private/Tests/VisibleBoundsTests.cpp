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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibleBoundsPlacementTest, "TrajectoryBLab.Query.VisibleBounds.WorldPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVisibleBoundsPlacementTest::RunTest(const FString& Parameters)
{
	const FBox PathSamples(FVector(-6, 2, 0), FVector(14, 14, 40));
	const FBox ObjectSamples(FVector(55, -4, 10), FVector(75, 8, 30));
	// Include a large-world placement to catch accidental cancellation of the
	// simulation origin, along with rotation and uniform actor scale.
	for (const FTransform SimulationToWorld : {
		FTransform(FVector(1200, -800, 350)),
		FTransform(FRotator(15, 35, -10), FVector(1200, -800, 350), FVector(1.5)),
		FTransform(FVector(3000000, -2000000, 350))})
	{
		auto* Path = NewObject<UVisibleSampleBoxComponent>();
		auto* Object = NewObject<UVisibleSampleBoxComponent>();
		const FTransform PathReference = FTransform(FVector(0, 0, 20)) * SimulationToWorld;
		const FTransform ObjectReference = FTransform(FVector(60, 0, 20)) * SimulationToWorld;
		Path->SetWorldTransform(PathReference);
		Object->SetWorldTransform(ObjectReference);
		Path->SetVisibleSampleBoundsFromSimulation(PathSamples, SimulationToWorld, PathReference);
		Object->SetVisibleSampleBoundsFromSimulation(ObjectSamples, SimulationToWorld, ObjectReference);
		TestTrue(TEXT("Path collision follows displayed samples away from world origin"),
			Path->CalcBounds(PathReference).Origin.Equals(SimulationToWorld.TransformPosition(PathSamples.GetCenter()), 0.001));
		TestTrue(TEXT("Object collision follows its independent displayed samples"),
			Object->CalcBounds(ObjectReference).Origin.Equals(SimulationToWorld.TransformPosition(ObjectSamples.GetCenter()), 0.001));
		TestTrue(TEXT("Fitting keeps both reference transforms unchanged"),
			Path->GetComponentTransform().Equals(PathReference) && Object->GetComponentTransform().Equals(ObjectReference));

		const FBox PathBefore = Path->CalcBounds(PathReference).GetBox();
		const FTransform Grab(FRotator(10, -25, 5), FVector(80, 40, 20), FVector(2));
		const FTransform GrabbedObject = ObjectReference * Grab;
		Object->SetWorldTransform(GrabbedObject);
		Object->SetVisibleSampleBoundsFromSimulation(ObjectSamples, SimulationToWorld, ObjectReference);
		// ObjectSamples' center is (65,2,20), with reference pivot (60,0,20).
		const FVector ExpectedCenter = GrabbedObject.TransformPosition(FVector(5, 2, 0));
		TestTrue(TEXT("Refitting a grabbed/scaled object uses the reference rather than current component transform"),
			Object->CalcBounds(GrabbedObject).Origin.Equals(ExpectedCenter, 0.001));
		TestTrue(TEXT("Object refitting leaves path geometry and pivot unchanged"),
			Path->CalcBounds(PathReference).GetBox().Equals(PathBefore) && Path->GetComponentTransform().Equals(PathReference));
		Object->SetVisibleSampleBoundsFromSimulation(FBox(ForceInit), SimulationToWorld, ObjectReference);
		TestEqual(TEXT("Empty simulation bounds still disable collision"), Object->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	}
	return true;
}
#endif
