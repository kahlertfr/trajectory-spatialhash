#include "SpatialHashTableManager.h"
#include "TrajectoryQueryNiagaraActor.h"
#include "TrajectoryDataStructures.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	template<typename T>
	void AppendBinary(TArray<uint8>& Bytes, const T& Value)
	{
		Bytes.Append(reinterpret_cast<const uint8*>(&Value), sizeof(T));
	}

	bool WriteContextFixture(const FString& Directory)
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
		FDatasetMetaBinary Dataset{};
		FMemory::Memcpy(Dataset.Magic, "TDSH", 4);
		Dataset.FormatVersion = 1;
		Dataset.LastTimeStep = 2;
		Dataset.TimeStepIntervalSize = 3;
		Dataset.TrajectoryCount = 3;
		Dataset.FirstTrajectoryId = 7;
		Dataset.LastTrajectoryId = 9;
		for (int32 Axis = 0; Axis < 3; ++Axis) Dataset.BBoxMax[Axis] = 100.0f;
		TArray<uint8> Bytes;
		AppendBinary(Bytes, Dataset);
		if (!FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, TEXT("dataset-meta.bin")))) return false;
		Bytes.Reset();
		for (uint64 Id = 7; Id <= 9; ++Id)
		{
			FTrajectoryMetaBinary Meta{};
			Meta.TrajectoryId = Id;
			Meta.EndTimeStep = 2;
			AppendBinary(Bytes, Meta);
		}
		if (!FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, TEXT("dataset-trajmeta.bin")))) return false;
		Bytes.Reset();
		FDataBlockHeaderBinary Header{};
		FMemory::Memcpy(Header.Magic, "TDDB", 4);
		Header.FormatVersion = 1;
		Header.TimeStepIntervalSize = 3;
		Header.TrajectoryEntryCount = 3;
		Header.DataSectionOffset = sizeof(Header);
		AppendBinary(Bytes, Header);
		for (uint64 Id = 7; Id <= 9; ++Id)
		{
			FTrajectoryEntryHeaderBinary Entry{};
			Entry.TrajectoryId = Id;
			Entry.ValidSampleCount = Id == 8 ? 2 : 3;
			AppendBinary(Bytes, Entry);
			for (int32 Time = 0; Time < 3; ++Time)
			{
				const float X = Id == 7 ? 0.1f * (Time + 1) : Id == 8 ? 0.4f + Time * 0.1f : 99.0f;
				const FVector3f Position(Id == 8 && Time == 1 ? NAN : X, 0, 0);
				AppendBinary(Bytes, Position);
			}
		}
		return FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, TEXT("shard-0.bin")));
	}

	class FContextScenarios : public IAutomationLatentCommand
	{
	public:
		FAutomationTestBase* Test;
		USpatialHashTableManager* Manager;
		FString Directory;
		bool bWaiting = false;
		int32 Stage = 0;
		double Started = FPlatformTime::Seconds();
		FContextScenarios(FAutomationTestBase* InTest, USpatialHashTableManager* InManager, FString InDirectory)
			: Test(InTest), Manager(InManager), Directory(MoveTemp(InDirectory)) {}

		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Started > 60.0)
			{
				Test->AddError(TEXT("Context filter scenario timed out"));
				Manager->CancelActiveQuery();
				Manager->RemoveFromRoot();
				return true;
			}
			if (bWaiting) return false;
			if (Stage == 0 || Stage == 5)
			{
				bWaiting = true;
				const bool bMetadataOnly = Stage == 0;
				Manager->LoadHashTablesAsync(Directory, 10, 0, 2, true,
					FOnSpatialHashLoadComplete::CreateLambda([Self = StaticCastSharedRef<FContextScenarios>(AsShared()), bMetadataOnly](const FSpatialHashLoadResult& Result)
				{
					Self->Test->TestTrue(TEXT("Preparation succeeds"), Result.ErrorMessage.IsEmpty());
					if (bMetadataOnly)
					{
						Self->Test->TestEqual(TEXT("All-context preparation loads no hashes"), Result.LoadedCount, 0);
						Self->Test->TestTrue(TEXT("Metadata supplies periodic extent"), Result.DatasetExtent.Equals(FVector(100)));
						Self->Test->TestFalse(TEXT("All-context preparation creates no hashes"),
							IFileManager::Get().DirectoryExists(*FPaths::Combine(Self->Directory, TEXT("spatial_hashing"))));
					}
					++Self->Stage;
					Self->bWaiting = false;
				}), bMetadataOnly);
				return false;
			}
			if (Stage == 13)
			{
				Manager->RemoveFromRoot();
				IFileManager::Get().DeleteDirectory(*Directory, false, true);
				return true;
			}
			bWaiting = true;
			const bool bAll = Stage < 5 || Stage == 8 || Stage == 9 || Stage == 11;
			const bool bWhole = Stage == 2 || Stage >= 9;
			const bool bPeriodic = Stage == 4;
			const int64 Excluded = Stage == 3 || Stage == 7 ? -1 : 7;
			Manager->SetPeriodicVolume(bPeriodic, FVector(100));
			const bool bShortFocus = Stage >= 9;
			const int32 ShortFocusTime = Stage >= 11 ? 2 : 0;
			const TArray<FVector> Query = bShortFocus
				? TArray<FVector>{FVector(0.1f * (ShortFocusTime + 1), 0, 0)}
				: TArray<FVector>{FVector(0.1f, 0, 0), FVector(0.2f, 0, 0), FVector(0.3f, 0, 0)};
			const TArray<int32> Times = bShortFocus ? TArray<int32>{ShortFocusTime}
				: Stage == 8 ? TArray<int32>{5, 6, 7} : TArray<int32>{0, 1, 2};
			auto Results = MakeShared<TMap<int32, FSpatialHashQueryResult>>();
			Manager->QueryPositionsBatchedAsync(Directory, Query, Times, 1, 10, 1, Excluded, bWhole,
				FOnSpatialHashBatchResult::CreateLambda([Self = StaticCastSharedRef<FContextScenarios>(AsShared()), Results, bAll, bPeriodic, Excluded, bShortFocus, Times]
				(const TArray<FSpatialHashQueryResult>& Batch, bool bFinal, int32 Candidates, int32 Handled)
				{
					Self->Test->TestTrue(TEXT("Callbacks run on game thread"), IsInGameThread());
					Self->Test->TestTrue(TEXT("At most one trajectory per batch"), Batch.Num() <= 1);
					for (const auto& Result : Batch)
					{
						Results->Add(Result.TrajectoryId, Result);
						for (const auto& Sample : Result.SamplePoints)
						{
							Self->Test->TestTrue(TEXT("Result sample is inside the query trajectory's time range"),
								Sample.TimeStep >= Times[0] && Sample.TimeStep <= Times.Last());
						}
					}
					if (!bFinal) return;
					const int32 Expected = Self->Stage == 8 ? 0 : (bAll ? 2 : 1) + (Excluded < 0 ? 1 : 0);
					Self->Test->TestEqual(TEXT("Correct result membership"), Results->Num(), Expected);
					Self->Test->TestTrue(TEXT("Excluded focus never appears"), Excluded < 0 || !Results->Contains(7));
					if (Self->Stage != 8)
					{
						const auto* Sparse = Results->Find(8);
						Self->Test->TestNotNull(TEXT("Near sparse trajectory retained"), Sparse);
						if (Sparse) Self->Test->TestEqual(TEXT("Retain only available samples within focus times"), Sparse->SamplePoints.Num(), bShortFocus ? 1 : 2);
						if (bShortFocus && Results->Contains(9))
						{
							Self->Test->TestEqual(TEXT("Whole context history is clipped to short focus"), (*Results)[9].SamplePoints.Num(), 1);
						}
						Self->Test->TestEqual(TEXT("Distant trajectory retained only in all-context mode"), Results->Contains(9), bAll);
						if (bPeriodic && Results->Contains(9))
						{
							Self->Test->TestTrue(TEXT("Periodic minimum-image distance preserved"), (*Results)[9].SamplePoints[0].Distance < 2);
						}
					}
					Self->Test->TestTrue(TEXT("Query did not fail"), Handled >= 0);
					++Self->Stage;
					Self->bWaiting = false;
				}), bAll);
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContextFilterIdentityTest, "SpatialHash.ContextFilter.DatasetIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FContextFilterIdentityTest::RunTest(const FString& Parameters)
{
	auto* Actor = GetMutableDefault<ATrajectoryQueryNiagaraActor>();
	TGuardValue<FString> Context(Actor->DatasetDirectory, FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ContextFixture")));
	TGuardValue<FString> Focus(Actor->QueryDatasetDirectory, Actor->DatasetDirectory + TEXT("/../ContextFixture/"));
	TGuardValue<int64> Id(Actor->QueryTrajectoryId, 7);
	TestEqual(TEXT("Normalized dataset identity excludes focus"), Actor->GetExcludedContextTrajectoryId(), 7LL);
	Actor->QueryDatasetDirectory = Actor->DatasetDirectory + TEXT("Other");
	TestEqual(TEXT("Same ID from another dataset is retained"), Actor->GetExcludedContextTrajectoryId(), -1LL);
	Actor->QueryDatasetDirectory.Empty();
	TestEqual(TEXT("Unknown source does not exclude an unrelated ID"), Actor->GetExcludedContextTrajectoryId(), -1LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContextFilterPipelineTest, "SpatialHash.ContextFilter.BatchedPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FContextFilterPipelineTest::RunTest(const FString& Parameters)
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ContextFilter"), FGuid::NewGuid().ToString());
	if (!WriteContextFixture(Directory))
	{
		AddError(TEXT("Could not write context fixture"));
		return false;
	}
	auto* Manager = NewObject<USpatialHashTableManager>();
	Manager->AddToRoot();
	ADD_LATENT_AUTOMATION_COMMAND(FContextScenarios(this, Manager, Directory));
	return true;
}
#endif
