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
	virtual void UpdateBodySetup() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FMatrix GetRenderMatrix() const override;

  private:
	FVector SampleCenter = FVector::ZeroVector;
	FVector SampleExtent = FVector::ZeroVector;
	bool bHasSampleBounds = false;
	bool bSavedCollision = false;
	ECollisionEnabled::Type OriginalCollision = ECollisionEnabled::QueryOnly;
};
