// Copyright Epic Games, Inc. All Rights Reserved.

#include "SampleTrajectoryNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

namespace
{
	constexpr float CircleRadius = 0.55f;
	constexpr float StaticRadius = 0.7f;
	constexpr float ApproachRadiusMin = 0.55f;
	constexpr float ApproachRadiusMax = 0.9f;

	FVector WrapToExtent(const FVector& Position, const FVector& Extent)
	{
		auto WrapAxis = [](float Value, float AxisExtent)
		{
			if (AxisExtent <= 0.0f)
			{
				return Value;
			}
			float Wrapped = FMath::Fmod(Value, AxisExtent);
			if (Wrapped < 0.0f)
			{
				Wrapped += AxisExtent;
			}
			return Wrapped;
		};
		return FVector(
			WrapAxis(Position.X, Extent.X),
			WrapAxis(Position.Y, Extent.Y),
			WrapAxis(Position.Z, Extent.Z));
	}

	int32 EncodeVolumeIndex(const FVector& Raw, const FVector& Unwrapped, const FVector& Extent)
	{
		if (Extent.IsNearlyZero())
		{
			return 0;
		}

		auto EncodeAxis = [](float RawValue, float UnwrappedValue, float AxisExtent)
		{
			return (AxisExtent > 0.0f) ? FMath::RoundToInt((UnwrappedValue - RawValue) / AxisExtent) : 0;
		};

		const int32 ix = EncodeAxis(Raw.X, Unwrapped.X, Extent.X);
		const int32 iy = EncodeAxis(Raw.Y, Unwrapped.Y, Extent.Y);
		const int32 iz = EncodeAxis(Raw.Z, Unwrapped.Z, Extent.Z);

		return (ix & 0xFF) | ((iy & 0xFF) << 8) | ((iz & 0xFF) << 16);
	}

	FVector DecodeVolumeIndexToOffset(int32 VolumeIndex, const FVector& Extent)
	{
		if (VolumeIndex == 0 || Extent.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}

		int32 ix = VolumeIndex & 0xFF;
		int32 iy = (VolumeIndex >> 8) & 0xFF;
		int32 iz = (VolumeIndex >> 16) & 0xFF;
		if (ix >= 128) ix -= 256;
		if (iy >= 128) iy -= 256;
		if (iz >= 128) iz -= 256;
		return FVector(ix * Extent.X, iy * Extent.Y, iz * Extent.Z);
	}

	FVector GetForwardAt(const TArray<FVector>& Points, int32 Index)
	{
		if (Points.Num() <= 1)
		{
			return FVector::ForwardVector;
		}

		const int32 From = FMath::Min(Index, Points.Num() - 2);
		const int32 To = FMath::Min(Index + 1, Points.Num() - 1);
		const FVector Dir = (Points[To] - Points[From]).GetSafeNormal();
		return Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
	}

	FRotationMatrix MakeLocalFrame(const FVector& Up)
	{
		const FVector SafeUp = Up.IsNearlyZero() ? FVector::UpVector : Up.GetSafeNormal();
		return FRotationMatrix::MakeFromZ(SafeUp);
	}

	int32 ScenarioToIndex(ESampleTrajectoryScenario Scenario)
	{
		return static_cast<int32>(Scenario);
	}
}

ASampleTrajectoryNiagaraActor::ASampleTrajectoryNiagaraActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASampleTrajectoryNiagaraActor::BeginPlay()
{
	Super::BeginPlay();

	if (NiagaraSystem)
	{
		NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraSystem,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false);
		NiagaraComponent->Deactivate();
	}

	BuildScenarioData();
	RefreshVisualization();

	if (SelectionManager)
	{
		SelectionManager->OnSelectionsChanged.AddDynamic(this, &ASampleTrajectoryNiagaraActor::HandleSelectionsChanged);
		HandleSelectionsChanged(SelectionManager->GetSelections());
	}
}

void ASampleTrajectoryNiagaraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SelectionManager)
	{
		SelectionManager->OnSelectionsChanged.RemoveDynamic(this, &ASampleTrajectoryNiagaraActor::HandleSelectionsChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ASampleTrajectoryNiagaraActor::SetSelectedScenario(ESampleTrajectoryScenario NewScenario)
{
	SelectedScenario = NewScenario;
	RefreshVisualization();
}

void ASampleTrajectoryNiagaraActor::RebuildSampleData()
{
	bScenarioDataBuilt = false;
	BuildScenarioData();
	RefreshVisualization();
}

void ASampleTrajectoryNiagaraActor::RefreshVisualization()
{
	TransferScenarioToNiagara(SelectedScenario);
}

void ASampleTrajectoryNiagaraActor::HandleSelectionsChanged(const TArray<FUserTrajectorySelection>& Selections)
{
	if (Selections.IsEmpty())
	{
		return;
	}

	const int32 TargetId = ScenarioToIndex(SelectedScenario);
	for (const FUserTrajectorySelection& Selection : Selections)
	{
		if (Selection.TrajectoryId == TargetId)
		{
			ApplyUserSelection(Selection);
			return;
		}
	}

	ApplyUserSelection(Selections[0]);
}

void ASampleTrajectoryNiagaraActor::ApplyUserSelection(const FUserTrajectorySelection& Selection)
{
	VisualizationTimeStart = Selection.TimeStart;
	VisualizationTimeEnd = Selection.TimeEnd;
	ColorEncoding = Selection.ColorEncoding;
	DistanceFilter = Selection.DistanceFilter;
	ParticleRadius = Selection.ParticleRadius;
	InnerRadius = Selection.InnerRadius;
	QueryRadius = Selection.QueryRadius;
	VisibilityRadius = Selection.VisibilityRadius;
	TimeRangeSensitivity = Selection.TimeRangeSensitive;
	ParticleVisibility = Selection.bParticlesVisible;

	UpdateUserParametersOnNiagara();
}

void ASampleTrajectoryNiagaraActor::UpdateUserParametersOnNiagara()
{
	if (!NiagaraComponent)
	{
		return;
	}

	const int32 MaxStep = FMath::Max(NumTimeSteps - 1, 0);
	VisualizationTimeStart = FMath::Clamp(VisualizationTimeStart, 0, MaxStep);
	VisualizationTimeEnd = FMath::Clamp(VisualizationTimeEnd, VisualizationTimeStart, MaxStep);

	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"), VisualizationTimeEnd);
	NiagaraComponent->SetVariableInt(FName("ColorEncoding"), static_cast<int32>(ColorEncoding));
	NiagaraComponent->SetVariableInt(FName("DistanceFilter"), static_cast<int32>(DistanceFilter));
	NiagaraComponent->SetVariableFloat(FName("ParticleRadius"), ParticleRadius);
	NiagaraComponent->SetVariableFloat(FName("InnerRadius"), InnerRadius);
	NiagaraComponent->SetVariableFloat(FName("QueryRadius"), QueryRadius);
	NiagaraComponent->SetVariableFloat(FName("VisibilityRadius"), VisibilityRadius);
	NiagaraComponent->SetVariableBool(FName("TimeRangeSensitive"), TimeRangeSensitivity);
	NiagaraComponent->SetVariableBool(FName("ParticleVisibility"), ParticleVisibility);
}

void ASampleTrajectoryNiagaraActor::BuildScenarioData()
{
	if (bScenarioDataBuilt)
	{
		return;
	}

	const int32 ScenarioCount = 6;
	ScenarioCache.SetNum(ScenarioCount);
	for (int32 Index = 0; Index < ScenarioCount; ++Index)
	{
		BuildScenarioData(static_cast<ESampleTrajectoryScenario>(Index));
	}

	bScenarioDataBuilt = true;
}

void ASampleTrajectoryNiagaraActor::BuildScenarioData(ESampleTrajectoryScenario Scenario)
{
	const int32 Steps = FMath::Max(NumTimeSteps, 2);
	FScenarioData Data;
	Data.QueryPointsUnwrapped.SetNum(Steps);
	Data.QueryPointsRaw.SetNum(Steps);

	const FVector AxisStart(0.5f, 0.5f, 0.5f);
	const FVector AxisDelta(4.0f, 4.0f, 4.0f);

	switch (Scenario)
	{
	case ESampleTrajectoryScenario::A:
	case ESampleTrajectoryScenario::B:
	case ESampleTrajectoryScenario::C:
	case ESampleTrajectoryScenario::D:
	{
		FVector Delta = FVector::ZeroVector;
		if (Scenario == ESampleTrajectoryScenario::A)
		{
			Delta = FVector(0.0f, 0.0f, AxisDelta.Z);
		}
		else if (Scenario == ESampleTrajectoryScenario::B)
		{
			Delta = FVector(AxisDelta.X, 0.0f, 0.0f);
		}
		else if (Scenario == ESampleTrajectoryScenario::C)
		{
			Delta = FVector(0.0f, AxisDelta.Y, 0.0f);
		}
		else
		{
			Delta = AxisDelta;
		}

		for (int32 i = 0; i < Steps; ++i)
		{
			const float Alpha = static_cast<float>(i) / static_cast<float>(Steps - 1);
			Data.QueryPointsUnwrapped[i] = AxisStart + Delta * Alpha;
		}
		break;
	}
	case ESampleTrajectoryScenario::E:
	{
		FRandomStream Random(1337);
		FVector Position(
			Random.FRandRange(0.5f, 4.5f),
			Random.FRandRange(0.5f, 4.5f),
			Random.FRandRange(0.5f, 4.5f));
		FVector Direction = Random.VRand();
		FVector TargetDirection = Direction;
		const int32 ChangeInterval = 45;
		const float Speed = 0.02f;
		const float Blend = 0.05f;

		for (int32 i = 0; i < Steps; ++i)
		{
			if (i > 0 && (i % ChangeInterval) == 0)
			{
				TargetDirection = Random.VRand();
			}
			Direction = FMath::Lerp(Direction, TargetDirection, Blend).GetSafeNormal();
			if (i > 0)
			{
				Position += Direction * Speed;
			}
			Data.QueryPointsUnwrapped[i] = Position;
		}
		break;
	}
	case ESampleTrajectoryScenario::F:
	{
		const FVector Start(4.9f, 2.5f, 2.5f);
		const FVector Direction(1.0f, 0.0f, 0.0f);
		const float Speed = 0.03f;

		for (int32 i = 0; i < Steps; ++i)
		{
			Data.QueryPointsUnwrapped[i] = Start + Direction * (Speed * static_cast<float>(i));
		}
		break;
	}
	default:
		break;
	}

	for (int32 i = 0; i < Steps; ++i)
	{
		Data.QueryPointsRaw[i] = WrapToExtent(Data.QueryPointsUnwrapped[i], PeriodicVolumeExtent);
	}

	Data.Results.Empty();
	Data.Results.Reserve(3);

	for (int32 TrajectoryIndex = 0; TrajectoryIndex < 3; ++TrajectoryIndex)
	{
		FSpatialHashQueryResult Result(TrajectoryIndex + 1);
		Result.SamplePoints.Reserve(Steps);

		for (int32 i = 0; i < Steps; ++i)
		{
			const FVector Forward = GetForwardAt(Data.QueryPointsUnwrapped, i);
			const FRotationMatrix Frame = MakeLocalFrame(Forward);

			FVector LocalOffset = FVector::ZeroVector;
			if (TrajectoryIndex == 0)
			{
				const float Angle = 2.0f * PI * (static_cast<float>(i) / static_cast<float>(Steps));
				LocalOffset = FVector(FMath::Cos(Angle) * CircleRadius, FMath::Sin(Angle) * CircleRadius, 0.0f);
			}
			else if (TrajectoryIndex == 1)
			{
				LocalOffset = FVector(StaticRadius, 0.0f, 0.0f);
			}
			else
			{
				const int32 MidpointStep = Steps / 2;
				float Distance = ApproachRadiusMin;
				if (i <= MidpointStep)
				{
					Distance = FMath::Lerp(
						ApproachRadiusMax,
						ApproachRadiusMin,
						static_cast<float>(i) / static_cast<float>(MidpointStep));
				}
				else if (Steps - MidpointStep - 1 > 0)
				{
					Distance = FMath::Lerp(
						ApproachRadiusMin,
						ApproachRadiusMax,
						static_cast<float>(i - MidpointStep) / static_cast<float>(Steps - MidpointStep - 1));
				}
				LocalOffset = FVector(Distance, 0.0f, 0.0f);
			}

			const FVector UnwrappedPosition = Data.QueryPointsUnwrapped[i] + Frame.TransformVector(LocalOffset);
			const FVector RawPosition = WrapToExtent(UnwrappedPosition, PeriodicVolumeExtent);
			FTrajectorySamplePoint Sample;
			Sample.Position = RawPosition;
			Sample.TimeStep = i;
			Sample.Distance = LocalOffset.Length();
			Sample.VolumeIndex = EncodeVolumeIndex(RawPosition, UnwrappedPosition, PeriodicVolumeExtent);

			Result.SamplePoints.Add(MoveTemp(Sample));
		}

		Data.Results.Add(MoveTemp(Result));
	}

	FBox Bounds(EForceInit::ForceInit);
	for (const FVector& Point : Data.QueryPointsUnwrapped)
	{
		Bounds += Point;
	}
	for (const FSpatialHashQueryResult& Result : Data.Results)
	{
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			const FVector Unwrapped = Sample.Position + DecodeVolumeIndexToOffset(Sample.VolumeIndex, PeriodicVolumeExtent);
			Bounds += Unwrapped;
		}
	}

	Data.BoundsMin = Bounds.IsValid ? Bounds.Min : FVector::ZeroVector;
	Data.BoundsMax = Bounds.IsValid ? Bounds.Max : FVector::ZeroVector;

	ScenarioCache[ScenarioToIndex(Scenario)] = MoveTemp(Data);
}

void ASampleTrajectoryNiagaraActor::TransferScenarioToNiagara(ESampleTrajectoryScenario Scenario)
{
	if (!NiagaraComponent)
	{
		return;
	}

	BuildScenarioData();

	const int32 ScenarioIndex = ScenarioToIndex(Scenario);
	if (!ScenarioCache.IsValidIndex(ScenarioIndex))
	{
		return;
	}

	const FScenarioData& Data = ScenarioCache[ScenarioIndex];
	const TArray<FVector>& QueryPointsUnwrapped = Data.QueryPointsUnwrapped;
	const TArray<FVector>& QueryPointsRaw = Data.QueryPointsRaw;
	const TArray<FSpatialHashQueryResult>& Results = Data.Results;

	// Build flat result arrays
	TArray<FVector> ResultPoints;
	TArray<float> ResultDistances;
	TArray<int32> ResultVolumeIndices;
	TArray<int32> ResultTrajectoryIds;
	TArray<float> ResultMinDistances;
	TArray<int32> ResultMinDistanceTimeSteps;
	TArray<int32> ResultTrajStartIndex;
	TArray<int32> ResultStartTime;

	ResultTrajectoryIds.Reserve(Results.Num());
	ResultMinDistances.Reserve(Results.Num());
	ResultMinDistanceTimeSteps.Reserve(Results.Num());
	ResultTrajStartIndex.Reserve(Results.Num());
	ResultStartTime.Reserve(Results.Num());

	for (const FSpatialHashQueryResult& Result : Results)
	{
		ResultTrajectoryIds.Add(Result.TrajectoryId);
		float MinDistance = 0.0f;
		int32 MinDistanceTimeStep = INDEX_NONE;
		bool bHasMinDistance = false;

		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			if (!bHasMinDistance || Sample.Distance < MinDistance)
			{
				MinDistance = Sample.Distance;
				MinDistanceTimeStep = Sample.TimeStep;
				bHasMinDistance = true;
			}
		}

		ResultMinDistances.Add(MinDistance);
		ResultMinDistanceTimeSteps.Add(MinDistanceTimeStep);
		ResultTrajStartIndex.Add(ResultPoints.Num());
		ResultStartTime.Add(Result.SamplePoints.Num() > 0 ? Result.SamplePoints[0].TimeStep : 0);

		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			ResultPoints.Add(Sample.Position);
			ResultDistances.Add(Sample.Distance);
			ResultVolumeIndices.Add(Sample.VolumeIndex);
		}
	}

	// Query volume indices
	TArray<int32> QueryVolumeIndices;
	QueryVolumeIndices.Reserve(QueryPointsRaw.Num());
	for (int32 i = 0; i < QueryPointsRaw.Num(); ++i)
	{
		QueryVolumeIndices.Add(EncodeVolumeIndex(QueryPointsRaw[i], QueryPointsUnwrapped[i], PeriodicVolumeExtent));
	}

	// Query-relative transforms
	TArray<FVector> QueryTranslations;
	TArray<FQuat> QueryRotations;
	if (QueryPointsUnwrapped.Num() > 0)
	{
		QueryTranslations.Reserve(QueryPointsUnwrapped.Num());
		QueryRotations.Reserve(QueryPointsUnwrapped.Num());

		const FVector FirstForward = GetForwardAt(QueryPointsUnwrapped, 0);
		for (int32 i = 0; i < QueryPointsUnwrapped.Num(); ++i)
		{
			const FVector CurForward = GetForwardAt(QueryPointsUnwrapped, i);
			QueryTranslations.Add(QueryPointsUnwrapped[0] - QueryPointsUnwrapped[i]);
			QueryRotations.Add(FQuat::FindBetweenNormals(CurForward, FirstForward));
		}
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryPoints"), QueryPointsRaw);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("ResultPoints"), ResultPoints);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultDistances"), ResultDistances);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryTranslations"), QueryTranslations);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(
		NiagaraComponent, FName("QueryRotations"), QueryRotations);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajectoryIds"), ResultTrajectoryIds);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultMinDistances"), ResultMinDistances);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultMinDistanceTimeSteps"), ResultMinDistanceTimeSteps);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajStartIndex"), ResultTrajStartIndex);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultStartTime"), ResultStartTime);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultVolumeIndices"), ResultVolumeIndices);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("QueryVolumeIndices"), QueryVolumeIndices);

	const int32 QueryTimeStart = 0;
	const int32 QueryTimeEnd = FMath::Max(NumTimeSteps - 1, 0);
	VisualizationTimeStart = FMath::Clamp(VisualizationTimeStart, QueryTimeStart, QueryTimeEnd);
	VisualizationTimeEnd = FMath::Clamp(VisualizationTimeEnd, VisualizationTimeStart, QueryTimeEnd);

	NiagaraComponent->SetVariableFloat(FName("InnerRadius"), InnerRadius);
	NiagaraComponent->SetVariableFloat(FName("QueryRadius"), QueryRadius);
	NiagaraComponent->SetVariableFloat(FName("VisibilityRadius"), VisibilityRadius);
	NiagaraComponent->SetVariableFloat(FName("ParticleRadius"), ParticleRadius);
	NiagaraComponent->SetVariableInt(FName("QueryTimeStart"), QueryTimeStart);
	NiagaraComponent->SetVariableInt(FName("QueryTimeEnd"), QueryTimeEnd);
	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"), VisualizationTimeEnd);
	NiagaraComponent->SetVariableInt(FName("ColorEncoding"), static_cast<int32>(ColorEncoding));
	NiagaraComponent->SetVariableInt(FName("DistanceFilter"), static_cast<int32>(DistanceFilter));
	NiagaraComponent->SetVariableVec3(FName("BoundsMin"), Data.BoundsMin);
	NiagaraComponent->SetVariableVec3(FName("BoundsMax"), Data.BoundsMax);
	NiagaraComponent->SetVariableVec3(FName("PeriodicVolumeExtent"), PeriodicVolumeExtent);
	NiagaraComponent->SetVariableBool(FName("TimeRangeSensitive"), TimeRangeSensitivity);
	NiagaraComponent->SetVariableBool(FName("ParticleVisibility"), ParticleVisibility);

	NiagaraComponent->DeactivateImmediate();
	NiagaraComponent->Activate(true);
}
