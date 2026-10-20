#pragma once
#include "Components/BoxComponent.h"
#include "VisibleSampleBoxComponent.generated.h"

/** Offset collision geometry without moving the component's interaction pivot or children. */
UCLASS(ClassGroup = "Collision", meta = (BlueprintSpawnableComponent))
class SPATIALHASHEDTRAJECTORY_API UVisibleSampleBoxComponent : public UBoxComponent
{
	GENERATED_BODY()
  public:
	void SetVisibleSampleBounds(const FBox& LocalBounds);
	/** Fitted collision geometry in component space, including the interaction margin. */
	FBox GetVisibleSampleLocalBounds() const;
	/** Place the axes at local -X/-Y/-Z, retaining their orientation and scale. */
	void UpdateAxesCorner(USceneComponent* Axes);
	/** Fit pre-grab Niagara local-space bounds without moving the interaction reference. */
	void SetVisibleSampleBoundsFromSimulation(const FBox& SimulationBounds,
		const FTransform& SimulationToWorld, const FTransform& ReferenceToWorld);
	virtual void UpdateBodySetup() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FMatrix GetRenderMatrix() const override;

  private:
	FVector SampleCenter = FVector::ZeroVector;
	FVector SampleExtent = FVector::ZeroVector;
	bool bHasSampleBounds = false;
	bool bHasVisibleSamples = false;
	TWeakObjectPtr<USceneComponent> CornerAxes;
	bool bAxesHiddenForEmptyView = false;
	bool bAxesOriginallyHidden = false;
	bool bSavedCollision = false;
	ECollisionEnabled::Type OriginalCollision = ECollisionEnabled::QueryOnly;
};
