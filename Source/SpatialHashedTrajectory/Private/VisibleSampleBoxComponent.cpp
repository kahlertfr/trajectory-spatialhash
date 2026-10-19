#include "VisibleSampleBoxComponent.h"
#include "PhysicsEngine/BodySetup.h"

void UVisibleSampleBoxComponent::SetVisibleSampleBounds(const FBox& LocalBounds)
{
	if (!bSavedCollision)
	{
		OriginalCollision = GetCollisionEnabled();
		bSavedCollision = true;
	}
	if (!LocalBounds.IsValid)
	{
		SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	const FVector Center = LocalBounds.GetCenter();
	// Keep degenerate axes usable by physics without making a large fallback box.
	const FVector Extent = (LocalBounds.GetExtent() + FVector(0.1)).ComponentMax(FVector(0.1));
	const bool Changed =
		!bHasSampleBounds || !Center.Equals(SampleCenter, 0.001) || !Extent.Equals(SampleExtent, 0.001);
	bHasSampleBounds = true;
	SampleCenter = Center;
	SampleExtent = Extent;
	BoxExtent = Extent;
	SetCollisionEnabled(OriginalCollision);
	if (Changed)
	{
		UpdateBodySetup();
		RecreatePhysicsState();
		UpdateBounds();
		MarkRenderStateDirty();
		if (IsCollisionEnabled() && GetOwner())
			UpdateOverlaps();
	}
}
void UVisibleSampleBoxComponent::UpdateBodySetup()
{
	if (bHasSampleBounds)
		BoxExtent = SampleExtent;
	Super::UpdateBodySetup();
	if (bHasSampleBounds && ShapeBodySetup)
	{
		// Archetype collision data must never be mutated by an individual visualization.
		if (bUseArchetypeBodySetup)
		{
			ShapeBodySetup = DuplicateObject<UBodySetup>(ShapeBodySetup, this);
			bUseArchetypeBodySetup = false;
		}
		if (ShapeBodySetup->AggGeom.BoxElems.Num() == 1)
			ShapeBodySetup->AggGeom.BoxElems[0].Center = SampleCenter;
	}
}
FBoxSphereBounds UVisibleSampleBoxComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return bHasSampleBounds ? FBoxSphereBounds(FBox(SampleCenter - SampleExtent, SampleCenter + SampleExtent))
								  .TransformBy(LocalToWorld)
							: Super::CalcBounds(LocalToWorld);
}
FMatrix UVisibleSampleBoxComponent::GetRenderMatrix() const
{
	return bHasSampleBounds ? FTranslationMatrix(SampleCenter) * Super::GetRenderMatrix() : Super::GetRenderMatrix();
}
