// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryQueryNiagaraActor.h"
#include "VisibleSampleBoxComponent.h"
#include "UObject/UnrealType.h"
#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "VRLogManager.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterfaceArrayInt.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

struct FPreparedTrajectoryDisplay
{
	TArray<FVector> ResultPoints;
	TArray<float> ResultDistances;
	TArray<float> ResultDisplayDistances;
	TArray<float> ResultVelocities;
	TArray<int32> ResultVolumeIndices;
	TArray<int32> ResultTrajectoryIds;
	TArray<float> ResultMinDistances;
	TArray<int32> ResultMinDistanceTimeSteps;
	TArray<int32> ResultTrajStartIndex;
	TArray<int32> ResultStartTime;
	TArray<float> QueryVelocities;
	TArray<int32> QueryVolumeIndices;
	TArray<FVector> QueryTranslations;
	TArray<FQuat> QueryRotations;
	TArray<FVector> RawQueryPoints;
	TArray<int32> QuerySampleTimeSteps;
	TArray<int32> ResultSampleTimeSteps;
	TArray<int32> ResultQuerySampleIndices;
	TArray<float> ResultLastDistances;

	float VelocityMin = 0.0f;
	float VelocityMax = 0.0f;
	int32 Stride = 1;
	FVector BoundsMin = FVector::ZeroVector;
	FBox ObjectBounds{ForceInit};
	FBox RotatedObjectBounds{ForceInit};
	FVector BoundsMax = FVector::ZeroVector;
	FString Error;
};

// ─── File-local helpers ───────────────────────────────────────────────────────

/**
 * Decode a packed periodic volume index into a world-space offset vector.
 *
 * The index uses two's-complement byte packing per axis:
 *   Bits  7.. 0 = ix,  Bits 15.. 8 = iy,  Bits 23..16 = iz
 * Each signed component is multiplied by the corresponding box-extent to
 * produce the translation that must be added to a raw sample position to
 * place it in the correct periodic image.
 *
 * Returns FVector::ZeroVector when VolumeIndex == 0 (original box) or Extent
 * is zero (non-periodic dataset), so it is safe to call unconditionally.
 */
static FVector DecodePeriodicVolumeIndexToOffset(int32 VolumeIndex, const FVector& Extent)
{
	if (VolumeIndex == 0 || Extent.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	int32 ix = VolumeIndex & 0xFF;
	int32 iy = (VolumeIndex >> 8)  & 0xFF;
	int32 iz = (VolumeIndex >> 16) & 0xFF;
	// Sign-extend from unsigned byte (0-255) to signed integer (-127..127).
	if (ix >= 128) ix -= 256;
	if (iy >= 128) iy -= 256;
	if (iz >= 128) iz -= 256;
	return FVector(ix * Extent.X, iy * Extent.Y, iz * Extent.Z);
}


static FPreparedTrajectoryDisplay PrepareTrajectoryDisplay(
	const TArray<FVector>& QueryPoints, const TArray<FVector>& CachedQueryPointsRaw,
	const TArray<FSpatialHashQueryResult>& Results, FVector CachedPeriodicExtent,
	int32 QueryTimeStart, int32 QueryTimeEnd, int32 MinimumStride, int32 MaximumSamples)
{
	check(!IsInGameThread());
	FPreparedTrajectoryDisplay Data;
	FBox FullBounds(EForceInit::ForceInit);
	for (const FVector& Point : QueryPoints) FullBounds += Point;
	// ── Build flat result arrays ──────────────────────────────────────────────

	// ResultPoints: all sample positions concatenated in trajectory order
	auto& ResultPoints = Data.ResultPoints;

	// ResultDistances: distance from the query point for each sample (parallel to ResultPoints)
	auto& ResultDistances = Data.ResultDistances;
	auto& ResultVelocities = Data.ResultVelocities;

	// ResultVolumeIndices: per-sample periodic volume index (parallel to ResultPoints).
	// 0 = original simulation box; non-zero = periodic image.
	// Decoded in HLSL via DecodeVolumeIndex / GetVolumeWorldOffset (see PERIODIC_VOLUME_INDEX.md).
	auto& ResultVolumeIndices = Data.ResultVolumeIndices;

	// Per-trajectory metadata arrays (one entry per result trajectory)
	auto& ResultTrajectoryIds = Data.ResultTrajectoryIds;
	auto& ResultMinDistances = Data.ResultMinDistances;
	auto& ResultMinDistanceTimeSteps = Data.ResultMinDistanceTimeSteps;
	auto& ResultTrajStartIndex = Data.ResultTrajStartIndex;
	auto& ResultStartTime = Data.ResultStartTime;

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
		Data.ResultLastDistances.Add(Result.SamplePoints.IsEmpty() ? 0.0f : Result.SamplePoints.Last().Distance);

		TArray<FVector> UnwrappedPositions;
		TArray<int32> TimeSteps;
		UnwrappedPositions.Reserve(Result.SamplePoints.Num());
		TimeSteps.Reserve(Result.SamplePoints.Num());

		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			Data.ResultSampleTimeSteps.Add(Sample.TimeStep);
			ResultPoints.Add(Sample.Position);
			ResultDistances.Add(Sample.Distance);
			ResultVolumeIndices.Add(Sample.VolumeIndex);

			UnwrappedPositions.Add(Sample.Position + DecodePeriodicVolumeIndexToOffset(Sample.VolumeIndex, CachedPeriodicExtent));
			// Match Niagara's temporal endpoint clamping before display reduction.
			const int32 FocusIndex = FMath::Clamp(Sample.TimeStep - QueryTimeStart, 0, QueryPoints.Num() - 1);
			Data.ResultDisplayDistances.Add(QueryPoints.IsValidIndex(FocusIndex)
				? FVector::Distance(UnwrappedPositions.Last(), QueryPoints[FocusIndex]) : FLT_MAX);
			FullBounds += UnwrappedPositions.Last();
			TimeSteps.Add(Sample.TimeStep);
		}

		if (UnwrappedPositions.Num() == 1)
		{
			ResultVelocities.Add(0.0f);
		}
		else if (UnwrappedPositions.Num() > 1)
		{
			for (int32 i = 0; i < UnwrappedPositions.Num(); ++i)
			{
				const int32 FromIndex = (i < UnwrappedPositions.Num() - 1) ? i : (i - 1);
				const int32 ToIndex = (i < UnwrappedPositions.Num() - 1) ? (i + 1) : i;
				const int32 DeltaTime = FMath::Abs(TimeSteps[ToIndex] - TimeSteps[FromIndex]);
				const float DeltaDistance = FVector::Distance(UnwrappedPositions[ToIndex], UnwrappedPositions[FromIndex]);
				ResultVelocities.Add(DeltaTime > 0 ? (DeltaDistance / static_cast<float>(DeltaTime)) : 0.0f);
			}
		}
	}

	auto& QueryVelocities = Data.QueryVelocities;
	QueryVelocities.Reserve(QueryPoints.Num());
	if (QueryPoints.Num() == 1)
	{
		QueryVelocities.Add(0.0f);
	}
	else if (QueryPoints.Num() > 1)
	{
		for (int32 i = 0; i < QueryPoints.Num(); ++i)
		{
			const int32 FromIndex = (i < QueryPoints.Num() - 1) ? i : (i - 1);
			const int32 ToIndex = (i < QueryPoints.Num() - 1) ? (i + 1) : i;
			const int32 FromTimeStep = FMath::Min(QueryTimeStart + FromIndex, QueryTimeEnd);
			const int32 ToTimeStep = FMath::Min(QueryTimeStart + ToIndex, QueryTimeEnd);
			const int32 DeltaTime = FMath::Abs(ToTimeStep - FromTimeStep);
			const float DeltaDistance = FVector::Distance(QueryPoints[ToIndex], QueryPoints[FromIndex]);
			QueryVelocities.Add(DeltaTime > 0 ? (DeltaDistance / static_cast<float>(DeltaTime)) : 0.0f);
		}
	}

	float& VelocityMin = Data.VelocityMin;
	float& VelocityMax = Data.VelocityMax;
	bool bHasVelocity = false;
	const auto AccumulateVelocityRange = [&VelocityMin, &VelocityMax, &bHasVelocity](const TArray<float>& Velocities)
	{
		for (float Velocity : Velocities)
		{
			if (!bHasVelocity)
			{
				VelocityMin = Velocity;
				VelocityMax = Velocity;
				bHasVelocity = true;
				continue;
			}

			VelocityMin = FMath::Min(VelocityMin, Velocity);
			VelocityMax = FMath::Max(VelocityMax, Velocity);
		}
	};
	AccumulateVelocityRange(QueryVelocities);
	AccumulateVelocityRange(ResultVelocities);

	const int32 NumResultPoints = ResultPoints.Num();
	const int32 NumResultDistances = ResultDistances.Num();
	const int32 NumResultVelocities = ResultVelocities.Num();
	const int32 NumResultVolumeIndices = ResultVolumeIndices.Num();

	const bool bSampleArraysAligned =
		NumResultPoints == NumResultDistances &&
		NumResultPoints == Data.ResultDisplayDistances.Num() &&
		NumResultPoints == NumResultVelocities &&
		NumResultPoints == NumResultVolumeIndices;
	const int32 NumResultTrajectoryIds = ResultTrajectoryIds.Num();
	const int32 NumResultMinDistances = ResultMinDistances.Num();
	const int32 NumResultMinDistanceTimeSteps = ResultMinDistanceTimeSteps.Num();
	const int32 NumResultTrajStartIndices = ResultTrajStartIndex.Num();
	const int32 NumResultStartTimes = ResultStartTime.Num();

	const bool bMetadataArraysAligned =
		NumResultTrajectoryIds == NumResultMinDistances &&
		NumResultTrajectoryIds == NumResultMinDistanceTimeSteps &&
		NumResultTrajectoryIds == NumResultTrajStartIndices &&
		NumResultTrajectoryIds == NumResultStartTimes;
	if (!bSampleArraysAligned || !bMetadataArraysAligned)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ATrajectoryQueryNiagaraActor: Skipping Niagara transfer due to array alignment error. ")
			TEXT("SampleAligned=%d (Points=%d, Distances=%d, Velocities=%d, VolumeIndices=%d), ")
			TEXT("MetadataAligned=%d (Ids=%d, MinDist=%d, MinTime=%d, StartIdx=%d, StartTime=%d)."),
			bSampleArraysAligned ? 1 : 0,
			NumResultPoints, NumResultDistances, NumResultVelocities, NumResultVolumeIndices,
			bMetadataArraysAligned ? 1 : 0,
			NumResultTrajectoryIds, NumResultMinDistances, NumResultMinDistanceTimeSteps,
			NumResultTrajStartIndices, NumResultStartTimes);
		Data.Error = TEXT("Sample attribute alignment failed"); return Data;
	}

	// ── Build query volume index array ───────────────────────────────────────
	//
	// Parallel to QueryPoints (which Niagara receives as raw/wrapped coordinates).
	// Each element encodes how many periodic box-lengths must be added to the raw
	// position to place it in the same continuous image as the unwrapped trajectory.
	//
	// When non-periodic, all indices are 0 (no offset needed).
	// When periodic, CachedQueryPointsRaw holds the original wrapped positions and
	// QueryPoints (the parameter) holds the unwrapped positions used for transforms.
	Data.RawQueryPoints = CachedQueryPointsRaw.IsEmpty() ? QueryPoints : CachedQueryPointsRaw;
	const TArray<FVector>& RawQueryPoints = Data.RawQueryPoints;

	auto& QueryVolumeIndices = Data.QueryVolumeIndices;
	QueryVolumeIndices.Reserve(RawQueryPoints.Num());
	if (!CachedPeriodicExtent.IsNearlyZero() && !CachedQueryPointsRaw.IsEmpty())
	{
		// CachedQueryPointsRaw and CachedQueryPoints (the unwrapped version passed as
		// QueryPoints) are always built together in FireAsyncQueriesWithCallback and
		// must have the same length.
		ensure(CachedQueryPointsRaw.Num() == QueryPoints.Num());

		for (int32 i = 0; i < RawQueryPoints.Num(); ++i)
		{
			const FVector& Raw       = RawQueryPoints[i];
			const FVector& Unwrapped = QueryPoints[i];
			// Compute the per-axis hop count: how many box-lengths to ADD to Raw
			// to reach the Unwrapped image (identical to ComputeVolumeIndex(Raw, Unwrapped)).
			const int32 ix = (CachedPeriodicExtent.X > 0.0f)
				? FMath::RoundToInt((Unwrapped.X - Raw.X) / CachedPeriodicExtent.X) : 0;
			const int32 iy = (CachedPeriodicExtent.Y > 0.0f)
				? FMath::RoundToInt((Unwrapped.Y - Raw.Y) / CachedPeriodicExtent.Y) : 0;
			const int32 iz = (CachedPeriodicExtent.Z > 0.0f)
				? FMath::RoundToInt((Unwrapped.Z - Raw.Z) / CachedPeriodicExtent.Z) : 0;
			QueryVolumeIndices.Add((ix & 0xFF) | ((iy & 0xFF) << 8) | ((iz & 0xFF) << 16));
		}
	}
	else
	{
		QueryVolumeIndices.Init(0, RawQueryPoints.Num());
	}

	// ── Build query-relative transform arrays ────────────────────────────────
	//
	// Transforms are computed from the UNWRAPPED query positions (the QueryPoints
	// parameter) so that translations and rotations are continuous across periodic
	// boundaries.  The shader reconstructs the unwrapped world position as:
	//   UnwrappedWorldPos[i] = RawQueryPoints[i] + GetVolumeWorldOffset(QueryVolumeIndices[i], PeriodicVolumeExtent)
	// which equals QueryPoints[i], so the transforms remain consistent.
	//
	// For each query sample point i:
	//   QueryTranslations[i] = QueryPoints[0] - QueryPoints[i]
	//   QueryRotations[i]    = rotation that aligns the forward vector at i to
	//                          the forward vector at 0.
	//
	// The forward vector at point i is the normalised direction from
	// QueryPoints[i] to QueryPoints[i+1].  For the last point the direction
	// from the penultimate point is reused.  When only one point is present
	// FVector::ForwardVector is used as a neutral fallback.

	auto& QueryTranslations = Data.QueryTranslations;
	auto& QueryRotations = Data.QueryRotations;

	if (QueryPoints.Num() > 0)
	{
		QueryTranslations.Reserve(QueryPoints.Num());
		QueryRotations.Reserve(QueryPoints.Num());

		// Returns the normalised forward vector for sample index i.
		auto GetForwardAt = [&QueryPoints](int32 i) -> FVector
		{
			if (QueryPoints.Num() == 1)
			{
				return FVector::ForwardVector;
			}
			// Clamp so the last point reuses the direction of the penultimate one.
			const int32 From = FMath::Min(i,     QueryPoints.Num() - 2);
			const int32 To   = FMath::Min(i + 1, QueryPoints.Num() - 1);
			const FVector Dir = (QueryPoints[To] - QueryPoints[From]).GetSafeNormal();
			return Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
		};

		const FVector FirstForward = GetForwardAt(0);

		for (int32 i = 0; i < QueryPoints.Num(); ++i)
		{
			const FVector CurForward = GetForwardAt(i);
			QueryTranslations.Add(QueryPoints[0] - QueryPoints[i]);
			//if (i >0)
			//	UE_LOG(LogTemp, Log,
			//	TEXT("%d with %f"), i,  QueryTranslations[i].Length() - QueryTranslations[i-1].Length());

			QueryRotations.Add(FQuat::FindBetweenNormals(CurForward, FirstForward));
		}
	}


	// Match Niagara's object view before reducing samples: unwrap, translate,
	// and optionally apply the inverse uploaded quaternion about the first query point.
	if (!QueryPoints.IsEmpty())
	{
		const FVector Pivot = QueryPoints[0];
		Data.ObjectBounds += Pivot;
		Data.RotatedObjectBounds += Pivot;
		for (const FSpatialHashQueryResult& Result : Results)
		{
			for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
			{
				const int32 Index = FMath::Clamp(Sample.TimeStep - QueryTimeStart, 0, QueryPoints.Num() - 1);
				const FVector Position = Sample.Position + DecodePeriodicVolumeIndexToOffset(Sample.VolumeIndex, CachedPeriodicExtent);
				const FVector Aligned = Position + QueryTranslations[Index];
				Data.ObjectBounds += Aligned;
				Data.RotatedObjectBounds += Pivot + QueryRotations[Index].Inverse().RotateVector(Aligned - Pivot);
			}
		}
	}

	for (int32 Index = 0; Index < QueryPoints.Num(); ++Index)
	{
		Data.QuerySampleTimeSteps.Add(FMath::Min(QueryTimeStart + Index, QueryTimeEnd));
	}
	const auto KeepTime = [QueryTimeStart](int32 Time, int64 Stride)
	{
		return (static_cast<int64>(Time) - QueryTimeStart) % Stride == 0;
	};
	int64 Stride = FMath::Max(1, MinimumStride);
	for (;;)
	{
		int64 Count = 0;
		for (int32 Time : Data.QuerySampleTimeSteps) Count += KeepTime(Time, Stride);
		for (int32 Time : Data.ResultSampleTimeSteps) Count += KeepTime(Time, Stride);
		if (MaximumSamples <= 0 || Count <= MaximumSamples) break;
		if (Stride > MAX_int32 / 2)
		{
			Data.Error = TEXT("The sample budget cannot be satisfied by shared temporal reduction");
			return Data;
		}
		Stride *= 2;
	}
	Data.Stride = static_cast<int32>(Stride);
	TArray<int32> QueryIndices;
	TMap<int32, int32> FocusAtTime;
	for (int32 Index = 0; Index < Data.QuerySampleTimeSteps.Num(); ++Index)
	{
		if (KeepTime(Data.QuerySampleTimeSteps[Index], Stride))
		{
			FocusAtTime.Add(Data.QuerySampleTimeSteps[Index], QueryIndices.Num());
			QueryIndices.Add(Index);
		}
	}
	const auto Select = [](auto& Array, const TArray<int32>& Indices)
	{
		using FArray = typename TDecay<decltype(Array)>::Type;
		FArray Selected;
		Selected.Reserve(Indices.Num());
		for (int32 Index : Indices) Selected.Add(Array[Index]);
		Array = MoveTemp(Selected);
	};
	Select(Data.RawQueryPoints, QueryIndices);
	Select(Data.QuerySampleTimeSteps, QueryIndices);
	Select(Data.QueryVolumeIndices, QueryIndices);
	Select(Data.QueryVelocities, QueryIndices);
	Select(Data.QueryTranslations, QueryIndices);
	Select(Data.QueryRotations, QueryIndices);
	TArray<int32> ResultIndices, TrajectoryIndices, Starts, StartTimes;
	for (int32 Trajectory = 0; Trajectory < Data.ResultTrajectoryIds.Num(); ++Trajectory)
	{
		const int32 Start = Data.ResultTrajStartIndex[Trajectory];
		const int32 End = Trajectory + 1 < Data.ResultTrajectoryIds.Num()
			? Data.ResultTrajStartIndex[Trajectory + 1] : Data.ResultPoints.Num();
		const int32 ReducedStart = ResultIndices.Num();
		for (int32 Index = Start; Index < End; ++Index)
		{
			if (!KeepTime(Data.ResultSampleTimeSteps[Index], Stride)) continue;
			ResultIndices.Add(Index);
			const int32* FocusIndex = FocusAtTime.Find(Data.ResultSampleTimeSteps[Index]);
			Data.ResultQuerySampleIndices.Add(FocusIndex ? *FocusIndex : INDEX_NONE);
		}
		if (ResultIndices.Num() > ReducedStart)
		{
			TrajectoryIndices.Add(Trajectory);
			Starts.Add(ReducedStart);
			StartTimes.Add(Data.ResultSampleTimeSteps[ResultIndices[ReducedStart]]);
		}
	}
	Select(Data.ResultPoints, ResultIndices);
	Select(Data.ResultDistances, ResultIndices);
	Select(Data.ResultDisplayDistances, ResultIndices);
	Select(Data.ResultVelocities, ResultIndices);
	Select(Data.ResultVolumeIndices, ResultIndices);
	Select(Data.ResultSampleTimeSteps, ResultIndices);
	Select(Data.ResultTrajectoryIds, TrajectoryIndices);
	Select(Data.ResultMinDistances, TrajectoryIndices);
	Select(Data.ResultMinDistanceTimeSteps, TrajectoryIndices);
	Select(Data.ResultLastDistances, TrajectoryIndices);
	Data.ResultTrajStartIndex = MoveTemp(Starts);
	Data.ResultStartTime = MoveTemp(StartTimes);
	Data.BoundsMin = FullBounds.IsValid ? FullBounds.Min : FVector::ZeroVector;
	Data.BoundsMax = FullBounds.IsValid ? FullBounds.Max : FVector::ZeroVector;
	return Data;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectViewBoundsTest,
	"TrajectoryBLab.Query.ObjectViewBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectViewBoundsTest::RunTest(const FString& Parameters)
{
	const auto Data = Async(EAsyncExecution::ThreadPool, []()
	{
		// Nonzero pivot, a turn, and a periodic image exercise all alignment steps.
		TArray<FVector> Focus = { FVector(10, 20, 0), FVector(12, 20, 0), FVector(12, 22, 0) };
		FSpatialHashQueryResult Result(1);
		Result.SamplePoints = { FTrajectorySamplePoint(FVector(13, 22, 0), 101, 0.0f) };
		Result.SamplePoints[0].VolumeIndex = 1;
		return PrepareTrajectoryDisplay(Focus, Focus, {Result}, FVector(100,100,100), 100, 102, 2, 0);
	}).Get();
	TestTrue(TEXT("Translated bounds use unwrapped positions and the query at the sample time"),
		Data.ObjectBounds.Max.Equals(FVector(111, 22, 0)));
	TestTrue(TEXT("Rotation matches Niagara inverse quaternion around nonzero reference pivot"),
		Data.RotatedObjectBounds.Min.Equals(FVector(8, 20, 0)) &&
		Data.RotatedObjectBounds.Max.Equals(FVector(10, 121, 0)));
	TestEqual(TEXT("Full bounds survive reduction of the only result sample"), Data.ResultPoints.Num(), 0);
	auto* Actor = NewObject<ATrajectoryQueryNiagaraActor>();
	Actor->NiagaraComponent = NewObject<UNiagaraComponent>(Actor);
	auto* Box = NewObject<UBoxComponent>(Actor);
	Actor->ResultBoundsMin = FVector::ZeroVector;
	Actor->ResultBoundsMax = FVector(10, 20, 30);
	Actor->ObjectViewBounds = FBox(FVector::ZeroVector, FVector(2, 4, 6));
	Actor->RotatedObjectViewBounds = FBox(FVector(-2, -4, 0), FVector(2, 4, 6));
	Actor->ObjectViewWithRotation = false;
	const FTransform Reference(FVector(100, 200, 30));
	Box->SetWorldTransform(Reference);
	const FTransform Origin = Actor->RefreshObjectViewCollisionBox(Box, nullptr, Reference, FVector(60), true);
	TestTrue(TEXT("Object center uses aligned bounds and a fixed bottom anchor"), Origin.GetLocation().Equals(FVector(92, 224, 14)));
	TestTrue(TEXT("Box receives local half extents in display units"), Box->GetUnscaledBoxExtent().Equals(FVector(2, 6, 4)));
	const FTransform Repeated = Actor->RefreshObjectViewCollisionBox(Box, nullptr, Origin, FVector(60), false);
	TestTrue(TEXT("Repeated refresh does not accumulate offsets"), Repeated.Equals(Origin));
	const FTransform Grab(FQuat(FVector::UpVector, 0.7), FVector(80, 40, 20), FVector(1.5));
	Box->SetWorldTransform(Origin * Grab);
	Actor->ObjectViewWithRotation = true;
	const FTransform RotatedOrigin = Actor->RefreshObjectViewCollisionBox(Box, nullptr, Origin, FVector(60), false);
	TestTrue(TEXT("Rotation toggle preserves interaction transform"), Box->GetComponentTransform().Equals(RotatedOrigin * Grab));
	// Shift the aligned bounds vertically without changing their height. The bottom
	// and the box center must stay fixed; rendered samples receive the inverse shift.
	Box->SetWorldTransform(Reference);
	Actor->ObjectViewWithRotation = false;
	Actor->ObjectViewBounds = FBox(FVector(0, 10, 0), FVector(2, 14, 6));
	const FTransform Anchored = Actor->RefreshObjectViewCollisionBox(Box, nullptr, Reference, FVector(60), true);
	TestTrue(TEXT("Vertical alignment does not move the bottom anchor"), Anchored.GetLocation().Equals(FVector(92, 224, 14)));
	TestEqual(TEXT("Bottom stays at original path origin below path half-height"),
		Anchored.GetLocation().Z - Box->GetUnscaledBoxExtent().Z, 10.0);
	TestTrue(TEXT("Placement is published directly to Niagara without a grab update"),
		FVector(Actor->NiagaraComponent->GetOverrideParameters().GetParameterValue<FVector3f>(
			FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("User.ObjectViewPlacementOffset")))).Equals(FVector(0, 0, -20)));
	TestTrue(TEXT("Legacy grab helper does not apply placement twice"), Actor->GetObjectViewRenderTransform(Grab).Equals(Grab));
	const FVector Original(2, 3, 40);
	const FVector Rendered = Grab.TransformPosition(Original + Actor->ObjectViewPlacementOffset);
	TestTrue(TEXT("Probe inverse removes placement after undoing grabbing"),
		(Grab.InverseTransformPosition(Rendered) - Actor->ObjectViewPlacementOffset).Equals(Original));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplayedDistanceTest,
	"TrajectoryBLab.Query.DisplayedDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDisplayedDistanceTest::RunTest(const FString& Parameters)
{
	const auto Data = Async(EAsyncExecution::ThreadPool, []()
	{
		TArray<FVector> Focus = { FVector(0, 8.5, 0), FVector(0, 9, 0), FVector(0, 9.5, 0) };
		FSpatialHashQueryResult Result(1);
		Result.SamplePoints = { FTrajectorySamplePoint(FVector(0, 5, 0), 100, 3.5f),
			FTrajectorySamplePoint(FVector(0, 4.5, 0), 101, 4.5f),
			FTrajectorySamplePoint(FVector(0, 4, 0), 102, 5.5f) };
		for (auto& Sample : Result.SamplePoints) Sample.VolumeIndex = 1 << 8;
		return PrepareTrajectoryDisplay(Focus, Focus, {Result}, FVector(5.5, 11, 5.5), 100, 102, 2, 0);
	}).Get();
	TestEqual(TEXT("Displayed distances stay aligned after reduction"), Data.ResultDisplayDistances.Num(), Data.ResultPoints.Num());
	TestEqual(TEXT("Visibility measures the selected image"), Data.ResultDisplayDistances[0], 7.5f);
	TestEqual(TEXT("Same timestep focus lookup survives stride two"), Data.ResultDisplayDistances[1], 5.5f);
	TestEqual(TEXT("Analytical distance is preserved"), Data.ResultDistances[0], 3.5f);
	TestEqual(TEXT("Analytical minimum is preserved"), Data.ResultMinDistances[0], 3.5f);
	TestTrue(TEXT("A radius of four rejects a distant image despite periodic proximity"),
		Data.ResultDistances[0] <= 4 && Data.ResultDisplayDistances[0] > 4);
	const auto NonPeriodic = Async(EAsyncExecution::ThreadPool, []()
	{
		FSpatialHashQueryResult Result(2);
		Result.SamplePoints.Add(FTrajectorySamplePoint(FVector(3, 4, 0), 100, 5));
		return PrepareTrajectoryDisplay({FVector::ZeroVector}, {}, {Result}, FVector::ZeroVector, 100, 100, 1, 0);
	}).Get();
	TestEqual(TEXT("Nonperiodic distance is unchanged"), NonPeriodic.ResultDisplayDistances[0], NonPeriodic.ResultDistances[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSharedTrajectorySamplingTest,
	"TrajectoryBLab.Query.SharedTemporalSampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSharedTrajectorySamplingTest::RunTest(const FString& Parameters)
{
	const auto Prepare = [](int32 Stride, int32 Limit)
	{
		return Async(EAsyncExecution::ThreadPool, [Stride, Limit]()
		{
			TArray<FVector> Focus = {FVector(0,0,0), FVector(1,0,0), FVector(4,0,0), FVector(9,0,0), FVector(16,0,0)};
			FSpatialHashQueryResult Context(17);
			Context.SamplePoints = {FTrajectorySamplePoint(FVector(1,1,0), 11, 4.0f),
				FTrajectorySamplePoint(FVector(4,1,0), 12, 0.25f),
				FTrajectorySamplePoint(FVector(16,1,0), 14, 8.0f),
				FTrajectorySamplePoint(FVector(25,1,0), 15, 2.0f)};
			return PrepareTrajectoryDisplay(Focus, Focus, {Context}, FVector::ZeroVector, 10, 14, Stride, Limit);
		}).Get();
	};
	const auto Reduced = Prepare(2, 0);
	TestTrue(TEXT("Preparation succeeds"), Reduced.Error.IsEmpty());
	TestEqual(TEXT("Focus sample count"), Reduced.RawQueryPoints.Num(), 3);
	TestEqual(TEXT("Unequal starts and missing times use shared grid"), Reduced.ResultSampleTimeSteps[0], 12);
	TestEqual(TEXT("Result maps to same retained focus time"), Reduced.ResultQuerySampleIndices[0], 1);
	TestEqual(TEXT("Full-resolution velocity survives reduction"), Reduced.QueryVelocities[0], 1.0f);
	TestEqual(TEXT("Original final distance retained despite removed endpoint"), Reduced.ResultLastDistances[0], 2.0f);
	TestEqual(TEXT("Original minimum retained"), Reduced.ResultMinDistances[0], 0.25f);
	const auto Capped = Prepare(1, 3);
	TestTrue(TEXT("Budget includes focus and context"), Capped.RawQueryPoints.Num() + Capped.ResultPoints.Num() <= 3);
	TestEqual(TEXT("Automatic doubling"), Capped.Stride, 4);
	const auto Full = Prepare(1, 0);
	TestEqual(TEXT("Stride one preserves all samples"), Full.RawQueryPoints.Num() + Full.ResultPoints.Num(), 9);
	const auto Impossible = Prepare(1, 1);
	// There is only one focus sample on the origin; at a sufficiently large stride this fits.
	TestTrue(TEXT("Very small feasible budget"), Impossible.Error.IsEmpty());
	const auto Coincident = Async(EAsyncExecution::ThreadPool, []()
	{
		TArray<FVector> Focus = {FVector::ZeroVector};
		FSpatialHashQueryResult Context(8);
		Context.SamplePoints.Add(FTrajectorySamplePoint(FVector::ZeroVector, 10, 0.0f));
		return PrepareTrajectoryDisplay(Focus, Focus, {Context}, FVector::ZeroVector, 10, 10, 1, 1);
	}).Get();
	TestFalse(TEXT("Impossible coincident-time cap is rejected"), Coincident.Error.IsEmpty());
	const auto Empty = Async(EAsyncExecution::ThreadPool, []()
	{
		return PrepareTrajectoryDisplay({}, {}, {}, FVector::ZeroVector, 10, 10, 1, 1);
	}).Get();
	TestTrue(TEXT("Empty result is safe"), Empty.Error.IsEmpty());
	return true;
}
#endif

// The cleanup contract is independent of Niagara's internal CPU copies.
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuerySamplingCleanupTest,
	"TrajectoryBLab.Query.SamplingCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQuerySamplingCleanupTest::RunTest(const FString& Parameters)
{
	auto* Actor = NewObject<ATrajectoryQueryNiagaraActor>();
	Actor->CachedQueryPoints.Add(FVector::ZeroVector);
	Actor->CachedQueryPointsRaw.Add(FVector::ZeroVector);
	Actor->CachedResults.Add(FSpatialHashQueryResult(5));
	Actor->CachedResultsIndex.Add(5, 0);
	Actor->CachedQueryPositionIndices.Add(0);
	TSharedPtr<TArray<FSpatialHashQueryResult>, ESPMode::ThreadSafe> Batch = MakeShared<TArray<FSpatialHashQueryResult>, ESPMode::ThreadSafe>();
	Batch->Add(FSpatialHashQueryResult(7));
	Actor->CachedResultBatches.Add(Batch);
	TWeakPtr<TArray<FSpatialHashQueryResult>, ESPMode::ThreadSafe> WeakBatch = Batch;
	Batch.Reset();
	TestTrue(TEXT("Complete batches are retained before cleanup"), WeakBatch.IsValid());
	Actor->ReleaseQueryCPUData();
	TestFalse(TEXT("Cleanup releases complete batches"), WeakBatch.IsValid());
	TestEqual(TEXT("Focus allocation released"), Actor->CachedQueryPoints.GetAllocatedSize(), SIZE_T(0));
	TestEqual(TEXT("Raw focus allocation released"), Actor->CachedQueryPointsRaw.GetAllocatedSize(), SIZE_T(0));
	TestEqual(TEXT("Result allocation released"), Actor->CachedResults.GetAllocatedSize(), SIZE_T(0));
	TestEqual(TEXT("Batch references released"), Actor->CachedResultBatches.GetAllocatedSize(), SIZE_T(0));
	return true;
}
#endif

ATrajectoryQueryNiagaraActor::ATrajectoryQueryNiagaraActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Manager = nullptr;
	NiagaraComponent = nullptr;
	NiagaraSystem = nullptr;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;
	VisualizationTimeStart = QueryTimeStart;
	VisualizationTimeEnd   = QueryTimeEnd;
	ColorEncoding = ETrajectoryColorEncoding::Time;
	QueryRadius = OuterQueryRadius;
}

void ATrajectoryQueryNiagaraActor::BeginPlay()
{
	Super::BeginPlay();

	// Spawn the Niagara component from the assigned system asset if provided.
	if (NiagaraSystem)
	{
		NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraSystem,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false  // do not auto-activate – we push data first
		);
		NiagaraComponent->SetVariableObject(TEXT("User.VisibleBoundsReceiver"),this);
		NiagaraComponent->Deactivate();
	}
}

void ATrajectoryQueryNiagaraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Invalidate pending table loads and stop active query workers.
	CancelQuery();
	Super::EndPlay(EndPlayReason);
}

void ATrajectoryQueryNiagaraActor::CancelQuery()
{
	if (Manager)
	{
		Manager->CancelActiveQuery();
	}
	++VisibleBoundsRevision;
	++QueryGeneration;
	bPreparationInProgress = false;
	bDisplayDirty = false;
	PendingQueryComplete.Unbind();
	ReleaseQueryCPUData();
	bQueryInProgress = false;
	FSimpleDelegate Failure = PendingQueryFailure;
	PendingQueryFailure.Unbind();
	Failure.ExecuteIfBound();
}

bool ATrajectoryQueryNiagaraActor::InitializeManager()
{
	if (DatasetDirectory.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ATrajectoryQueryNiagaraActor: DatasetDirectory is not set."));
		return false;
	}

	if (!Manager)
	{
		Manager = NewObject<USpatialHashTableManager>(this);
	}

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("... Initialized spatial query with cell size %f."), CellSize);


	return true;
}

// ─── Public BlueprintCallable entry points ────────────────────────────────────

void ATrajectoryQueryNiagaraActor::TransferDataToNiagara()
{
	if (bFinalDisplayTransferred) return;
	if (CachedQueryPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: TransferDataToNiagara called before RunQuery has completed – no data to transfer."));
		return;
	}
	TransferResultsToNiagara(CachedQueryPoints, CachedResults);
}

void ATrajectoryQueryNiagaraActor::RunQueryAndUpdateNiagara()
{
	FireAsyncQueriesWithCallback(
		FSimpleDelegate());
}

void ATrajectoryQueryNiagaraActor::SetVisualizationTimeRange(int32 NewTimeStart, int32 NewTimeEnd)
{
	++VisibleBoundsRevision;
	VisualizationTimeStart = NewTimeStart;
	VisualizationTimeEnd   = NewTimeEnd;

	if (!NiagaraComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: SetVisualizationTimeRange called before a Niagara component is available."));
		return;
	}

	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"),   VisualizationTimeEnd);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Visualization time range updated to [%d, %d]."),
		VisualizationTimeStart, VisualizationTimeEnd);
}


void ATrajectoryQueryNiagaraActor::UpdateUserParameter(int32 NewTimeStart, int32 NewTimeEnd, ETrajectoryColorEncoding NewColorEncoding, ETrajectoryDistanceFilter NewDistanceFilter, float NewParticleRadius, float NewInnerRadius, float NewQueryRadius, float NewVisibilityRadius, bool NewSensitivity, bool NewVisibility, bool NewNoShadow, bool NewObjectViewWithRotation)
{
	VisualizationTimeStart = NewTimeStart;
	VisualizationTimeEnd = NewTimeEnd;
	ColorEncoding = NewColorEncoding;
	++VisibleBoundsRevision;
	DistanceFilter = NewDistanceFilter;
	ParticleRadius = NewParticleRadius;
	InnerRadius = NewInnerRadius;
	QueryRadius = NewQueryRadius;
	VisibilityRadius = NewVisibilityRadius;
	TimeRangeSensitivity = NewSensitivity;
	ParticleVisibility = NewVisibility;
	NoShadow = NewNoShadow;
	ObjectViewWithRotation = NewObjectViewWithRotation;

	if (!NiagaraComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: SetVisualizationTimeRange called before a Niagara component is available."));
		return;
	}

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
	NiagaraComponent->SetVariableBool(FName("NoShadow"), NoShadow);
	NiagaraComponent->SetVariableBool(FName("ObjectViewWithRotation"), ObjectViewWithRotation);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Updated User parameter."));
}

// ─── Core async pipeline ──────────────────────────────────────────────────────

int64 ATrajectoryQueryNiagaraActor::GetExcludedContextTrajectoryId() const
{
	if (QueryTrajectoryId < 0 || QueryDatasetDirectory.IsEmpty() || DatasetDirectory.IsEmpty())
	{
		return -1;
	}
	FString FocusDirectory = FPaths::ConvertRelativePathToFull(QueryDatasetDirectory);
	FString ContextDirectory = FPaths::ConvertRelativePathToFull(DatasetDirectory);
	FPaths::NormalizeDirectoryName(FocusDirectory);
	FPaths::NormalizeDirectoryName(ContextDirectory);
	FPaths::CollapseRelativeDirectories(FocusDirectory);
	FPaths::CollapseRelativeDirectories(ContextDirectory);
	return FPaths::IsSamePath(FocusDirectory, ContextDirectory) ? QueryTrajectoryId : -1;
}

bool ATrajectoryQueryNiagaraActor::FireAsyncQueriesWithCallback(
	FSimpleDelegate OnComplete,
	FSimpleDelegate OnFailure,
	bool bIncludeWholeResultTrajectorySamples)
{
	if (bQueryInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("ATrajectoryQueryNiagaraActor: A query is already running; cancel it before starting another."));
		OnFailure.ExecuteIfBound();
		return false;
	}
	if (!NiagaraComponent ||
		!UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayInt32>(NiagaraComponent, TEXT("QuerySampleTimeSteps")) ||
		!UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayInt32>(NiagaraComponent, TEXT("ResultSampleTimeSteps")) ||
		!UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayFloat>(NiagaraComponent, TEXT("ResultLastDistances")))
	{
		UE_LOG(LogTemp, Error, TEXT("The assigned query Niagara system needs the synchronized sampling array contract. Run UpgradeQuerySampling or migrate the custom system."));
		OnFailure.ExecuteIfBound();
		return false;
	}
	if (QueryPositions.IsEmpty() || QueryTimeStart > QueryTimeEnd || !InitializeManager())
	{
		OnFailure.ExecuteIfBound();
		return false;
	}
	if (!NiagaraComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("Query display requires an initialized Niagara component."));
		OnFailure.ExecuteIfBound();
		return false;
	}

	RequestedSampleStride = FMath::Max(1, SampleStride);
	if (QueryTrajectoryId >= 0 && QueryDatasetDirectory.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Set QueryDatasetDirectory with QueryTrajectoryId to identify the focus trajectory."));
		OnFailure.ExecuteIfBound();
		return false;
	}
	ActiveContextFilterMode = ContextFilterMode;
	ActiveExcludedTrajectoryId = GetExcludedContextTrajectoryId();
	ActiveContextExtent = FVector::ZeroVector;
	RequestedMaximumSamples = FMath::Max(0, MaximumDisplayedSamples);
	ActiveQueryTimeStart = QueryTimeStart;
	ActiveQueryTimeEnd = QueryTimeEnd;
	EffectiveSampleStride = RequestedSampleStride;
	DisplayedSampleCount = 0;
	bAllBatchesProcessed = false;
	bFinalDisplayTransferred = false;
	PendingQueryComplete = OnComplete;
	bQueryInProgress = true;
	PendingQueryFailure = OnFailure;
	const uint64 Generation = ++VisibleBoundsRevision;
	++QueryGeneration;
	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);
	// Protect against editable request properties changing while loading.
	const FString RequestedDirectory = DatasetDirectory;
	const float RequestedCellSize = CellSize;
	const int32 RequestedStart = QueryTimeStart;
	const int32 RequestedEnd = QueryTimeEnd;
	const TArray<FVector> RequestedPositions = QueryPositions;
	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
		TEXT("... Preparing context data asynchronously for t=[%d,%d]."), QueryTimeStart, QueryTimeEnd);
	Manager->LoadHashTablesAsync(DatasetDirectory, CellSize, QueryTimeStart, QueryTimeEnd, true,
		FOnSpatialHashLoadComplete::CreateLambda(
			[WeakThis, Generation, OnComplete, OnFailure, bIncludeWholeResultTrajectorySamples,
			 RequestedDirectory, RequestedCellSize, RequestedStart, RequestedEnd, RequestedPositions]
			(const FSpatialHashLoadResult& LoadResult)
		{
			ATrajectoryQueryNiagaraActor* This = WeakThis.Get();
			if (!This || This->QueryGeneration != Generation)
			{
				return;
			}
			const bool bAllContext = This->ActiveContextFilterMode == ETrajectoryContextFilterMode::AllContextTrajectories;
			if ((!bAllContext && LoadResult.LoadedCount == 0) || LoadResult.FailedCount > 0 || !LoadResult.ErrorMessage.IsEmpty() ||
				This->DatasetDirectory != RequestedDirectory || This->CellSize != RequestedCellSize ||
				This->QueryTimeStart != RequestedStart || This->QueryTimeEnd != RequestedEnd ||
				This->QueryPositions != RequestedPositions)
			{
				UE_LOG(LogTemp, Error, TEXT("ATrajectoryQueryNiagaraActor: Hash-table preparation failed or request changed: %s"),
					*LoadResult.ErrorMessage);
				This->CancelQuery();
				return;
			}
			This->ActiveContextExtent = LoadResult.DatasetExtent;
			This->StartPreparedQuery(OnComplete, LoadResult.DatasetFirstTimeStep,
				LoadResult.DatasetLastTimeStep, bIncludeWholeResultTrajectorySamples);
		}), ActiveContextFilterMode == ETrajectoryContextFilterMode::AllContextTrajectories);
	return true;
}

void ATrajectoryQueryNiagaraActor::StartPreparedQuery(
	FSimpleDelegate OnComplete, int32 DatasetFirstTimeStep, int32 DatasetLastTimeStep,
	bool bIncludeWholeResultTrajectorySamples)
{
	// Apply the periodic volume setting from this actor to the manager so
	// that all subsequent queries use the correct boundary conditions.
	const FVector Extent = ActiveContextFilterMode == ETrajectoryContextFilterMode::AllContextTrajectories &&
		PeriodicVolume.Extent.IsNearlyZero() ? ActiveContextExtent : PeriodicVolume.Extent;
	Manager->SetPeriodicVolume(PeriodicVolume.bIsPeriodic, Extent);

	// Cache the resolved extent so TransferResultsToNiagara can pass it to
	// Niagara as PeriodicVolumeExtent (required by the HLSL volume-index helper).
	CachedPeriodicExtent = Manager->GetResolvedPeriodicExtent(CellSize);

	// ── Reset cached state so progressive updates start clean ─────────────────
	// CachedQueryPointsRaw stores the original (wrapped) query positions so that
	// Niagara's QueryPoints array stays consistent with ResultPoints (both raw).
	// CachedQueryPoints stores the unwrapped (continuous) positions and is used
	// for transform computation (QueryTranslations / QueryRotations) and for the
	// query side of the bounding box.
	CachedQueryPointsRaw = QueryPositions;
	CachedQueryPoints = PeriodicVolume.bIsPeriodic
		? Manager->GetUnwrappedPositions(QueryPositions, CellSize)
		: QueryPositions;
	CachedResults.Empty();
	CachedResultBatches.Empty();
	CachedResultsIndex.Empty();
	CachedQueryPositionIndices.Empty();
	bBoundsValid    = false;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;

	// Note: VisualizationTimeStart, VisualizationTimeEnd and ColorEncoding are
	// intentionally NOT reset here.  They represent independent user-facing
	// controls that persist across queries, allowing the shader to keep the
	// currently selected view while new data arrives.

	const int32 NumPositions  = QueryPositions.Num();
	const int32 TimeRangeSize = QueryTimeEnd - QueryTimeStart + 1;

	if (NumPositions > TimeRangeSize)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: QueryPositions.Num() (%d) exceeds the time range [%d,%d] (%d steps). "
			     "Positions beyond index %d map to timesteps outside the loaded range and will return empty results."),
			NumPositions, QueryTimeStart, QueryTimeEnd, TimeRangeSize, TimeRangeSize - 1);
	}

	// Build the per-sample timestep array: position[i] corresponds to timestep
	// QueryTimeStart + i, clamped to QueryTimeEnd.
	TArray<int32> QueryTimeSteps;
	QueryTimeSteps.Reserve(NumPositions);
	for (int32 i = 0; i < NumPositions; ++i)
	{
		QueryTimeSteps.Add(FMath::Min(QueryTimeStart + i, QueryTimeEnd));
	}

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Starting batched query – %d positions, outer radius %.2f, "
		     "t=[%d,%d], batch size=%d."),
		NumPositions, OuterQueryRadius, QueryTimeStart, QueryTimeEnd, BatchSize);

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
		TEXT("... Starting batched query with %d positions, radius %.2f, t=[%d,%d], batch size=%d."),
		NumPositions, OuterQueryRadius, QueryTimeStart, QueryTimeEnd, BatchSize);

	int32 OutsideDatasetSamples = 0;
	for (int32 Time : QueryTimeSteps)
	{
		OutsideDatasetSamples += Time < DatasetFirstTimeStep || Time > DatasetLastTimeStep ? 1 : 0;
	}
	const uint64 Generation = QueryGeneration;
	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);
	TSharedRef<int32> BatchCounter             = MakeShared<int32>(0);
	TSharedRef<int32> TotalCandidatesProcessed = MakeShared<int32>(0);

	// Delegate called on the game thread after each batch of candidates is
	// processed.  Progressively merges results into the cache and pushes them
	// to Niagara so the user sees updates as they arrive.
	FOnSpatialHashBatchResult BatchCallback = FOnSpatialHashBatchResult::CreateLambda(
		[WeakThis, Generation, OnComplete, BatchCounter, TotalCandidatesProcessed, NumPositions,
		 OutsideDatasetSamples, DatasetFirstTimeStep, DatasetLastTimeStep]
		(const TArray<FSpatialHashQueryResult>& BatchResults, bool bIsFinalBatch,
		 int32 TotalCandidatesPhase1, int32 HandledQuerySamples)
		{
			ATrajectoryQueryNiagaraActor* This = WeakThis.Get();
			if (!This || This->QueryGeneration != Generation)
			{
				return;
			}

			const int32 CurrentBatch = ++(*BatchCounter);
			if (HandledQuerySamples < 0)
			{
				This->CancelQuery();
				return;
			}

			// On the first batch, log Phase 1 summary to VRLogger so the user
			// can see how many query samples were handled and how many candidate
			// trajectories were identified.
			if (CurrentBatch == 1)
			{
				const int32 UnhandledSamples = NumPositions - HandledQuerySamples - OutsideDatasetSamples;

				if (OutsideDatasetSamples > 0)
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						TEXT("... %d query samples outside target dataset [%d,%d]; no target data exists for those times."),
						OutsideDatasetSamples, DatasetFirstTimeStep, DatasetLastTimeStep);
				}

				This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
					TEXT("Phase 1 - Gather candidates"));

				This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
					TEXT("... %d/%d query samples handled, %d candidate trajectories found."),
					HandledQuerySamples, NumPositions, TotalCandidatesPhase1);

				if (UnhandledSamples > 0)
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						TEXT("WARNING: %d query sample(s) had no loaded hash table and were skipped – "
						     "check timestep coverage for gaps in the visualization."),
						UnhandledSamples);
				}

				This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						This->ActiveContextFilterMode == ETrajectoryContextFilterMode::AllContextTrajectories
							? TEXT("Phase 2 - Prepare all context trajectories")
							: TEXT("Phase 2 - Check for Nearest Neighbors"));
			}

			*TotalCandidatesProcessed += BatchResults.Num();

			This->AppendBatchResults(BatchResults);
			if (!bIsFinalBatch) This->TransferResultsToNiagara(This->CachedQueryPoints, This->CachedResults, true);

			UE_LOG(LogTemp, Log, TEXT("QueryPositionsBatchedAsync: Batch %d – %d trajectories in results (bFinal=%d)"),
				CurrentBatch, BatchResults.Num(), bIsFinalBatch ? 1 : 0);

			This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
				TEXT("... Batch %d - %d new trajectories found"),
				CurrentBatch, BatchResults.Num());

			if (bIsFinalBatch)
			{
				UE_LOG(LogTemp, Log,
					TEXT("ATrajectoryQueryNiagaraActor: All batches complete – %d trajectories found in total."),
					This->CachedResultsIndex.Num());

				This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
					TEXT("Nearest Neighbor Filtering: %d trajectories found in total."),
					This->CachedResultsIndex.Num());

				// Sanity-check: verify that the number of accumulated result
				// trajectories does not exceed the Phase 1 candidate count.
				if (*TotalCandidatesProcessed > TotalCandidatesPhase1)
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						TEXT("WARNING: Result trajectory count (%d) exceeds Phase 1 candidate count (%d) – "
						     "possible duplicate handling."),
						*TotalCandidatesProcessed, TotalCandidatesPhase1);
				}

				This->bAllBatchesProcessed = true;
				This->TransferResultsToNiagara(This->CachedQueryPoints, This->CachedResults, true);
			}
		}
	);

	Manager->QueryPositionsBatchedAsync(
		DatasetDirectory,
		QueryPositions,
		QueryTimeSteps,
		OuterQueryRadius,
		CellSize,
		BatchSize,
		ActiveExcludedTrajectoryId,
		bIncludeWholeResultTrajectorySamples,
		MoveTemp(BatchCallback),
		ActiveContextFilterMode == ETrajectoryContextFilterMode::AllContextTrajectories
	);
}

// ─── Result accumulation ──────────────────────────────────────────────────────

void ATrajectoryQueryNiagaraActor::AppendBatchResults(
	const TArray<FSpatialHashQueryResult>& BatchResults)
{
	if (BatchResults.IsEmpty())
	{
		return;
	}

	// Each candidate trajectory appears in at most one batch, so simple
	// appending is safe.  Skip any duplicate trajectory IDs that could arise
	// from rare hash-table overlap edge cases.
	auto UniqueBatch = MakeShared<TArray<FSpatialHashQueryResult>, ESPMode::ThreadSafe>();
	for (const FSpatialHashQueryResult& Result : BatchResults)
	{
		if (CachedResultsIndex.Contains(Result.TrajectoryId))
		{
			continue; // already present from an earlier batch
		}
		CachedResultsIndex.Add(Result.TrajectoryId, CachedResultsIndex.Num());
		UniqueBatch->Add(Result);
	}
	if (!UniqueBatch->IsEmpty()) CachedResultBatches.Add(UniqueBatch);

	// Bounds and all display attributes are computed by the preparation worker.

	bDisplayDirty = true;

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Batch appended – %d new trajectories, %d total, bounds [%s]–[%s]."),
		BatchResults.Num(), CachedResultsIndex.Num(),
		*ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}

void ATrajectoryQueryNiagaraActor::StoreQueryResults(
	const TArray<FVector>& QueryPoints,
	const TArray<FSpatialHashQueryResult>& Results)
{
	CachedQueryPoints = QueryPoints;
	CachedResults     = Results;

	// Compute bounding box over all query + result points.
	FBox Bounds(EForceInit::ForceInit);
	for (const FVector& QP : QueryPoints)
	{
		Bounds += QP;
	}
	for (const FSpatialHashQueryResult& Result : Results)
	{
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			// Use corrected world-space position (raw + volume offset) for the AABB.
			Bounds += Sample.Position + DecodePeriodicVolumeIndexToOffset(Sample.VolumeIndex, CachedPeriodicExtent);
		}
	}

	ResultBoundsMin = Bounds.IsValid ? Bounds.Min : FVector::ZeroVector;
	ResultBoundsMax = Bounds.IsValid ? Bounds.Max : FVector::ZeroVector;

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Results stored – %d trajectories, bounds [%s]–[%s]."),
		Results.Num(), *ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}

void ATrajectoryQueryNiagaraActor::ReleaseQueryCPUData()
{
	CachedResults.Empty();
	CachedResultBatches.Empty();
	CachedResultsIndex.Empty();
	CachedQueryPoints.Empty();
	CachedQueryPointsRaw.Empty();
	CachedQueryPositionIndices.Empty();
}

FTransform ATrajectoryQueryNiagaraActor::RefreshObjectViewCollisionBox(
	UBoxComponent* Box, USceneComponent* Axes, FTransform ReferenceOrigin,
	FVector TargetSize, bool bInitialize)
{
	if (!Box) return ReferenceOrigin;
	const FBox& Bounds = ObjectViewWithRotation ? RotatedObjectViewBounds : ObjectViewBounds;
	if (!Bounds.IsValid) return ReferenceOrigin;
	const FVector PathSize = ResultBoundsMax - ResultBoundsMin;
	const double Scale = PathSize.GetMax() > UE_SMALL_NUMBER ? TargetSize.GetMax() / PathSize.GetMax() : 1.0;
	const FVector Delta = Bounds.GetCenter() - (ResultBoundsMin + ResultBoundsMax) * 0.5;
	const FVector GeometricOffset = FVector(Delta.X, -Delta.Z, Delta.Y) * Scale;
	FVector Offset = GeometricOffset;
	// ReferenceOrigin is the path box center plus the object view's horizontal
	// layout offset. Keep its original bottom fixed and let this box grow upward.
	Offset.Z = (Bounds.GetSize().Y - PathSize.Y) * Scale * 0.5;
	ObjectViewPlacementOffset = FVector(0, 0, Offset.Z - GeometricOffset.Z);
	if (NiagaraComponent)
		NiagaraComponent->SetVariableVec3(TEXT("ObjectViewPlacementOffset"), ObjectViewPlacementOffset);
	FTransform NewOrigin = ReferenceOrigin;
	NewOrigin.AddToTranslation(Offset - (bInitialize ? FVector::ZeroVector : ObjectViewCenterOffset));
	// Keep the current grab transform while changing the collision reference center.
	const FTransform Interaction = ReferenceOrigin.Inverse() * Box->GetComponentTransform();
	Box->SetWorldTransform(NewOrigin * Interaction);
	const FVector Size = Bounds.GetSize();
	Box->SetBoxExtent(FVector(Size.X, Size.Z, Size.Y) * FMath::Abs(Scale) * 0.5, true);
	if (Axes) Axes->SetRelativeLocation(-FVector(PathSize.X, PathSize.Z, PathSize.Y) * Scale * 0.5 - Offset);
	ObjectViewCenterOffset = Offset;
	return NewOrigin;
}

FTransform ATrajectoryQueryNiagaraActor::GetObjectViewRenderTransform(FTransform InteractionTransform) const
{
	// Compatibility with previously saved Blueprints. Placement is now read by
	// Niagara's particle update, independently of conditional grab-matrix updates.
	return InteractionTransform;
}

void ATrajectoryQueryNiagaraActor::TransferResultsToNiagara(
	const TArray<FVector>& QueryPoints, const TArray<FSpatialHashQueryResult>& Results, bool bReactivate)
{
	check(IsInGameThread());
	bDisplayDirty = true;
	if (bPreparationInProgress) return;
	bPreparationInProgress = true;
	bDisplayDirty = false;
	const uint64 Generation = QueryGeneration;
	const int32 Start = ActiveQueryTimeStart, End = ActiveQueryTimeEnd;
	const int32 Stride = EffectiveSampleStride, Limit = RequestedMaximumSamples;
	const FVector Extent = CachedPeriodicExtent;
	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);
	Async(EAsyncExecution::ThreadPool,
		[WeakThis, Generation, Start, End, Stride, Limit, Extent, bReactivate,
		 Points = QueryPoints, Raw = CachedQueryPointsRaw, FullResults = Results,
		 Batches = CachedResultBatches]() mutable
	{
		for (const auto& Batch : Batches) FullResults.Append(*Batch);
		TSharedPtr<FPreparedTrajectoryDisplay, ESPMode::ThreadSafe> Prepared = MakeShared<FPreparedTrajectoryDisplay, ESPMode::ThreadSafe>(
			PrepareTrajectoryDisplay(Points, Raw, FullResults, Extent, Start, End, Stride, Limit));
		Points.Empty(); Raw.Empty(); FullResults.Empty(); Batches.Empty();
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, Prepared = MoveTemp(Prepared), bReactivate]() mutable
		{
			auto* This = WeakThis.Get();
			if (!This || This->QueryGeneration != Generation) return;
			This->bPreparationInProgress = false;
			if (!This->NiagaraComponent)
			{
				This->CancelQuery();
				return;
			}
			if (!Prepared->Error.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("Query display preparation failed: %s"), *Prepared->Error);
				This->CancelQuery();
				return;
			}
			This->UploadPreparedDisplay(*Prepared, bReactivate);
			This->EffectiveSampleStride = Prepared->Stride;
			This->DisplayedSampleCount = Prepared->RawQueryPoints.Num() + Prepared->ResultPoints.Num();
			Prepared.Reset(); // SetNiagaraArray* has copied all application-owned buffers.
			// Publish this valid progressive snapshot before preparing newer batches.
			// A final callback cannot complete until the latest snapshot is uploaded.
			if (This->bDisplayDirty)
			{
				This->TransferResultsToNiagara(This->CachedQueryPoints, This->CachedResults, bReactivate);
				return;
			}
			if (This->bAllBatchesProcessed)
			{
				This->ReleaseQueryCPUData();
				This->QueryPositions.Empty(); // Query input is consumed; next request supplies its focus samples again.
				This->bFinalDisplayTransferred = true;
				This->bQueryInProgress = false;
				This->PendingQueryFailure.Unbind();
				FSimpleDelegate Complete = This->PendingQueryComplete;
				This->PendingQueryComplete.Unbind();
				Complete.ExecuteIfBound();
			}
		});
	});
}

void ATrajectoryQueryNiagaraActor::UploadPreparedDisplay(const FPreparedTrajectoryDisplay& Data, bool bReactivate)
{
	check(IsInGameThread());
	if (!NiagaraComponent) return;
	++VisibleBoundsRevision;
	ResultBoundsMin = Data.BoundsMin;
	ObjectViewBounds = Data.ObjectBounds;
	RotatedObjectViewBounds = Data.RotatedObjectBounds;
	ResultBoundsMax = Data.BoundsMax;
	const auto& ResultPoints = Data.ResultPoints;
	const auto& ResultDistances = Data.ResultDistances;
	const auto& ResultDisplayDistances = Data.ResultDisplayDistances;
	const auto& ResultVelocities = Data.ResultVelocities;
	const auto& ResultVolumeIndices = Data.ResultVolumeIndices;
	const auto& ResultTrajectoryIds = Data.ResultTrajectoryIds;
	const auto& ResultMinDistances = Data.ResultMinDistances;
	const auto& ResultMinDistanceTimeSteps = Data.ResultMinDistanceTimeSteps;
	const auto& ResultTrajStartIndex = Data.ResultTrajStartIndex;
	const auto& ResultStartTime = Data.ResultStartTime;
	const auto& QueryVelocities = Data.QueryVelocities;
	const auto& QueryVolumeIndices = Data.QueryVolumeIndices;
	const auto& QueryTranslations = Data.QueryTranslations;
	const auto& QueryRotations = Data.QueryRotations;
	const auto& RawQueryPoints = Data.RawQueryPoints;
	const auto& QuerySampleTimeSteps = Data.QuerySampleTimeSteps;
	const auto& ResultSampleTimeSteps = Data.ResultSampleTimeSteps;
	const auto& ResultQuerySampleIndices = Data.ResultQuerySampleIndices;
	const auto& ResultLastDistances = Data.ResultLastDistances;
	const float VelocityMin = Data.VelocityMin, VelocityMax = Data.VelocityMax;
	// ── Transfer to Niagara user parameters ──────────────────────────────────

	// QueryPoints: raw (wrapped) simulation coordinates, consistent with ResultPoints.
	// The shader uses QueryVolumeIndices to reconstruct the correct world position.
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryPoints"), RawQueryPoints);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("QueryVelocities"), QueryVelocities);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("ResultPoints"), ResultPoints);

	// Float array: per-sample distance from the query point (parallel to ResultPoints)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultDistances"), ResultDistances);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultDisplayDistances"), ResultDisplayDistances);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultVelocities"), ResultVelocities);

	// Query-relative transform arrays
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryTranslations"), QueryTranslations);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(
		NiagaraComponent, FName("QueryRotations"), QueryRotations);

	// Integer arrays (Int Array type in Niagara)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajectoryIds"), ResultTrajectoryIds);

	// Float array: minimum distance reached by each trajectory (parallel to ResultTrajectoryIds)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultMinDistances"), ResultMinDistances);

	// Integer array: timestep where each trajectory reaches its minimum distance
	// (parallel to ResultTrajectoryIds / ResultMinDistances).
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultMinDistanceTimeSteps"), ResultMinDistanceTimeSteps);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajStartIndex"), ResultTrajStartIndex);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultStartTime"), ResultStartTime);

	// Per-sample volume index array (parallel to ResultPoints).
	// Encodes the periodic image each sample belongs to; 0 = original box.
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultVolumeIndices"), ResultVolumeIndices);

	// Per-query-point volume index array (parallel to QueryPoints).
	// Encodes how many box-lengths the raw query position must be shifted to
	// reach the unwrapped (continuous) trajectory image.  0 = original box.
	// QueryVolumeIndices[0] is always 0 (first point = reference / anchor).
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("QueryVolumeIndices"), QueryVolumeIndices);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(NiagaraComponent, FName("QuerySampleTimeSteps"), QuerySampleTimeSteps);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(NiagaraComponent, FName("ResultSampleTimeSteps"), ResultSampleTimeSteps);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(NiagaraComponent, FName("ResultQuerySampleIndices"), ResultQuerySampleIndices);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(NiagaraComponent, FName("ResultLastDistances"), ResultLastDistances);
	// Scalar user parameters
	NiagaraComponent->SetVariableFloat(FName("InnerRadius"), InnerRadius);
	NiagaraComponent->SetVariableFloat(FName("QueryRadius"), QueryRadius);
	NiagaraComponent->SetVariableFloat(FName("VisibilityRadius"), VisibilityRadius);
	NiagaraComponent->SetVariableFloat(FName("ParticleRadius"), ParticleRadius);
	NiagaraComponent->SetVariableFloat(FName("VelocityMin"), VelocityMin);
	NiagaraComponent->SetVariableFloat(FName("VelocityMax"), VelocityMax);
	NiagaraComponent->SetVariableBool(FName("NoShadow"), NoShadow);
	NiagaraComponent->SetVariableBool(FName("ObjectViewWithRotation"), ObjectViewWithRotation);
	NiagaraComponent->SetVariableInt(FName("QueryTimeStart"), ActiveQueryTimeStart);
	NiagaraComponent->SetVariableInt(FName("QueryTimeEnd"), ActiveQueryTimeEnd);

	// Visualization range – default to the full query range on initial transfer
	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"),   VisualizationTimeEnd);

	// Color encoding (0 = Timestep, 1 = Velocity)
	NiagaraComponent->SetVariableInt(FName("ColorEncoding"), static_cast<int32>(ColorEncoding));
	NiagaraComponent->SetVariableInt(FName("DistanceFilter"), static_cast<int32>(DistanceFilter));

	// Bounding box – use the stored values computed by StoreQueryResults
	NiagaraComponent->SetVariableVec3(FName("BoundsMin"), ResultBoundsMin);
	NiagaraComponent->SetVariableVec3(FName("BoundsMax"), ResultBoundsMax);

	// Periodic box size (world units per axis).  ZeroVector when non-periodic.
	// Used by the HLSL GetVolumeWorldOffset helper to compute per-sample position
	// offsets from the ResultVolumeIndices array.
	NiagaraComponent->SetVariableVec3(FName("PeriodicVolumeExtent"), CachedPeriodicExtent);

	// Activate the system now that all data has been pushed.
	if (bReactivate)
	{
		// removed old particles immediately
		NiagaraComponent->DeactivateImmediate();
		// NiagaraComponent->ResetSystem();
		NiagaraComponent->Activate(true);
	}

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Niagara system updated – "
		     "%d query points, %d result points across %d trajectories. "
		     "Bounds: [%s] – [%s]."),
		RawQueryPoints.Num(), ResultPoints.Num(), ResultTrajectoryIds.Num(),
		*ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}

void ATrajectoryQueryNiagaraActor::ReceiveVisibleBounds(uint64 Revision, const FBox& Path, const FBox& Object)
{
	if (Revision != VisibleBoundsRevision || IsActorBeingDestroyed())
		return;
	VisiblePathBounds = Path;
	VisibleObjectBounds = Object;
	// The Blueprint retains the reference transforms used to build its grab matrices.
	// Read those references, not the currently grabbed component transforms.
	const auto Apply = [this](const TCHAR* BoxName, const TCHAR* OriginName, const FBox& Bounds)
	{
		const auto* BoxProperty = FindFProperty<FObjectPropertyBase>(GetClass(), BoxName);
		const auto* OriginProperty = FindFProperty<FStructProperty>(GetClass(), OriginName);
		if (!BoxProperty || !OriginProperty || OriginProperty->Struct != TBaseStructure<FTransform>::Get())
			return;
		auto* Box = Cast<UVisibleSampleBoxComponent>(BoxProperty->GetObjectPropertyValue_InContainer(this));
		if (!Box)
			return;
		const FTransform& Origin = *OriginProperty->ContainerPtrToValuePtr<FTransform>(this);
		Box->SetVisibleSampleBounds(Bounds.IsValid ? Bounds.TransformBy(Origin.Inverse()) : FBox(ForceInit));
	};
	Apply(TEXT("PathVisBox"), TEXT("PathVisOrigin"), Path);
	Apply(TEXT("RelativeVisBox"), TEXT("RelativeVisOrigin"), Object);
}
