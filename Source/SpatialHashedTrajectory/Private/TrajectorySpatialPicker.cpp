#include "TrajectorySpatialPicker.h"
#include "TrajectoryQueryNiagaraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/ConstructorHelpers.h"
#include "TrajectoryHandPoseProvider.h"
#include "Features/IModularFeatures.h"

ATrajectorySpatialPicker::ATrajectorySpatialPicker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	TipSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FingertipSphere"));
	HandTrigger = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandTrigger"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	for (auto* Mesh : {TipSphere.Get(), HandTrigger.Get()})
	{
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetStaticMesh(Sphere.Object);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetHiddenInGame(true);
	}
	TriggerLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TriggerLabel"));
	TriggerLabel->SetupAttachment(GetRootComponent());
	TriggerLabel->SetWorldSize(1.2f);
	TriggerLabel->SetHorizontalAlignment(EHTA_Center);
	TriggerLabel->SetHiddenInGame(true);
}

ATrajectorySpatialPicker* ATrajectorySpatialPicker::FindForTarget(ATrajectoryQueryNiagaraActor* Actor)
{
	if (!IsValid(Actor) || !Actor->GetWorld()) return nullptr;
	for (TActorIterator<ATrajectorySpatialPicker> It(Actor->GetWorld()); It; ++It)
		if (It->Target.Get() == Actor) return *It;
	return nullptr;
}

ATrajectorySpatialPicker* ATrajectorySpatialPicker::EnableForTarget(ATrajectoryQueryNiagaraActor* Actor)
{
	if (!IsValid(Actor) || !Actor->GetWorld()) return nullptr;
	if (auto* Existing = FindForTarget(Actor)) return Existing;
	// One local hand tool owns the gesture; switching panels cannot leave another active selector behind.
	TArray<ATrajectorySpatialPicker*> Previous;
	for (TActorIterator<ATrajectorySpatialPicker> It(Actor->GetWorld()); It; ++It) Previous.Add(*It);
	for (auto* Tool : Previous) Tool->Disable();
	auto* Tool = Actor->GetWorld()->SpawnActor<ATrajectorySpatialPicker>();
	if (Tool) Tool->Target = Actor;
	Actor->OnTrajectoryInspectionChanged.Broadcast();
	return Tool;
}

void ATrajectorySpatialPicker::Disable()
{
	auto* Actor = Target.Get();
	if (Actor) Actor->CancelSpatialPicking();
	Target.Reset();
	Destroy();
	if (Actor) Actor->OnTrajectoryInspectionChanged.Broadcast();
}

void ATrajectorySpatialPicker::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Actor = Target.Get()) Actor->CancelSpatialPicking();
	Super::EndPlay(Reason);
}

void ATrajectorySpatialPicker::TapTrigger()
{
	auto* Actor = Target.Get();
	if (!Actor || !bHandsTracked) return;
	if (Actor->bSpatialPickingActive) Actor->ConfirmSpatialPicking();
	else Actor->BeginSpatialPicking(FingerPosition, SelectionRadius);
}

void ATrajectorySpatialPicker::UpdateHandPose(bool bTracked, FVector IndexTip, FVector TriggerPosition, FVector ThumbTip)
{
	auto* Actor = Target.Get();
	bHandsTracked = bTracked && !IndexTip.ContainsNaN() && !TriggerPosition.ContainsNaN() && !ThumbTip.ContainsNaN();
	if (!Actor) { Disable(); return; }
	HandTrigger->SetHiddenInGame(!bHandsTracked);
	TriggerLabel->SetHiddenInGame(!bHandsTracked);
	if (!bHandsTracked)
	{
		bTriggerLatched = true;
		Actor->CancelSpatialPicking();
		TipSphere->SetHiddenInGame(true);
		return;
	}
	FingerPosition = IndexTip;
	Actor->UpdateSpatialPickingPosition(IndexTip);
	HandTrigger->SetWorldLocation(TriggerPosition);
	HandTrigger->SetWorldScale3D(FVector(FMath::Max(0.1f, TriggerRadius) / 50.f));
	const float Distance = FVector::Dist(ThumbTip, TriggerPosition);
	if (Distance > TriggerRadius + 0.8f) bTriggerLatched = false;
	if (!bTriggerLatched && Distance <= TriggerRadius && FPlatformTime::Seconds() - LastTap > 0.25)
	{
		bTriggerLatched = true;
		LastTap = FPlatformTime::Seconds();
		TapTrigger();
	}
	TipSphere->SetHiddenInGame(!Actor->bSpatialPickingActive);
	TipSphere->SetWorldLocation(IndexTip);
	TipSphere->SetWorldScale3D(FVector(Actor->TrajSelectRadius / 50.f));
	FString Label = TEXT("Tap thumb to pick");
	if (Actor->bSpatialPickingActive)
		Label = Actor->CandidateTrajectoryIndex == INDEX_NONE ? TEXT("No hit - tap to finish") :
			FString::Printf(TEXT("Tap to confirm %d"), Actor->CandidateTrajectoryIndex);
	TriggerLabel->SetText(FText::FromString(Label));
	if (auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		TriggerLabel->SetWorldLocation(TriggerPosition + Camera->GetActorUpVector() * 2.5f);
		TriggerLabel->SetWorldRotation((Camera->GetCameraLocation() - TriggerLabel->GetComponentLocation()).Rotation());
	}
}

void ATrajectorySpatialPicker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Target.IsValid()) { Destroy(); return; }
	if (!bUseXRTracking) return;
	const auto Providers = IModularFeatures::Get().GetModularFeatureImplementations<ITrajectoryHandPoseProvider>(ITrajectoryHandPoseProvider::GetModularFeatureName());
	if (!Providers.IsEmpty())
	{
		FTransform Tip, Knuckle, Thumb;
		const bool Tracked = Providers[0]->GetPickingHandPose(this, Tip, Knuckle, Thumb);
		UpdateHandPose(Tracked, Tip.GetLocation(), Knuckle.TransformPosition(TriggerOffset), Thumb.GetLocation());
		return;
	}
	FXRHandTrackingState Left, Right;
	UHeadMountedDisplayFunctionLibrary::GetHandTrackingState(this, EXRSpaceType::UnrealWorldSpace, EControllerHand::Left, Left);
	UHeadMountedDisplayFunctionLibrary::GetHandTrackingState(this, EXRSpaceType::UnrealWorldSpace, EControllerHand::Right, Right);
	const int32 Tip = static_cast<int32>(EHandKeypoint::IndexTip);
	const int32 Knuckle = static_cast<int32>(EHandKeypoint::IndexProximal);
	const int32 Thumb = static_cast<int32>(EHandKeypoint::ThumbTip);
	const bool Tracked = Left.bValid && Right.bValid && Left.TrackingStatus == ETrackingStatus::Tracked &&
		Right.TrackingStatus == ETrackingStatus::Tracked && Right.HandKeyLocations.IsValidIndex(Tip) &&
		Left.HandKeyLocations.IsValidIndex(Knuckle) && Left.HandKeyLocations.IsValidIndex(Thumb) && Left.HandKeyRotations.IsValidIndex(Knuckle);
	if (!Tracked) { UpdateHandPose(false, {}, {}, {}); return; }
	UpdateHandPose(true, Right.HandKeyLocations[Tip],
		Left.HandKeyLocations[Knuckle] + Left.HandKeyRotations[Knuckle].RotateVector(TriggerOffset), Left.HandKeyLocations[Thumb]);
}
