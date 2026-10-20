#include "VisibleSampleBoxComponent.h"
#include "PhysicsEngine/BodySetup.h"

void UVisibleSampleBoxComponent::SetVisibleSampleBoundsFromSimulation(
	const FBox& SimulationBounds, const FTransform& SimulationToWorld, const FTransform& ReferenceToWorld)
{
	// The reduction runs before grabbing and exports Niagara local-space positions.
	// Blueprint reference origins are world-space transforms. Compose the conversion
	// before transforming the AABB, avoiding an extra world-axis-aligned enclosure.
	const FMatrix SimulationToReference =
		SimulationToWorld.ToMatrixWithScale() * ReferenceToWorld.ToInverseMatrixWithScale();
	SetVisibleSampleBounds(SimulationBounds.IsValid
		? SimulationBounds.TransformBy(SimulationToReference) : FBox(ForceInit));
}

void UVisibleSampleBoxComponent::SetVisibleSampleBounds(const FBox& LocalBounds)
{
	bHasVisibleSamples = LocalBounds.IsValid != 0;
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
FBox UVisibleSampleBoxComponent::GetVisibleSampleLocalBounds() const
{
	return bHasVisibleSamples ? FBox(SampleCenter - SampleExtent, SampleCenter + SampleExtent) : FBox(ForceInit);
}

void UVisibleSampleBoxComponent::UpdateAxesCorner(USceneComponent* Axes)
{
	if (!Axes || Axes == this || IsAttachedTo(Axes)) return;
	if (CornerAxes.Get() != Axes)
	{
		if (bAxesHiddenForEmptyView && CornerAxes.IsValid())
			CornerAxes->SetHiddenInGame(bAxesOriginallyHidden, true);
		CornerAxes = Axes;
		bAxesHiddenForEmptyView = false;
	}
	const FBox LocalBounds = GetVisibleSampleLocalBounds();
	if (!LocalBounds.IsValid)
	{
		if (!bAxesHiddenForEmptyView)
		{
			bAxesOriginallyHidden = Axes->bHiddenInGame;
			bAxesHiddenForEmptyView = true;
			Axes->SetHiddenInGame(true, true);
		}
		return;
	}
	if (Axes->GetAttachParent() != this &&
		!Axes->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform)) return;
	// Use the offset geometry's minimum, not -BoxExtent or a world-axis-aligned box.
	Axes->SetRelativeLocation(LocalBounds.Min);
	if (bAxesHiddenForEmptyView)
	{
		Axes->SetHiddenInGame(bAxesOriginallyHidden, true);
		bAxesHiddenForEmptyView = false;
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
