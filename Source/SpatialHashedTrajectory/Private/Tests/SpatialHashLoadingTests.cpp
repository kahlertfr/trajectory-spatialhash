#include "SpatialHashTableManager.h"
#include "TrajectoryDataStructures.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FLoadTestState
	{
		USpatialHashTableManager* Manager = nullptr;
		FString FixtureDirectory;
		bool bComplete = false;
		double Started = FPlatformTime::Seconds();
		int32 TicksWhileLoading = 0;
	};

	class FWaitForHashLoad : public IAutomationLatentCommand
	{
	public:
		FWaitForHashLoad(FAutomationTestBase* InTest, TSharedRef<FLoadTestState> InState)
			: Test(InTest), State(InState) {}
		virtual bool Update() override
		{
			++State->TicksWhileLoading;
			if (!State->bComplete && FPlatformTime::Seconds() - State->Started < 120.0)
			{
				return false;
			}
			Test->TestTrue(TEXT("Async load completes"), State->bComplete);
			Test->AddInfo(FString::Printf(TEXT("Game-thread automation ticks while waiting: %d"), State->TicksWhileLoading));
			State->Manager->CancelActiveQuery();
			State->Manager->RemoveFromRoot();
			if (!State->FixtureDirectory.IsEmpty())
			{
				IFileManager::Get().DeleteDirectory(*State->FixtureDirectory, false, true);
			}
			return true;
		}
	private:
		FAutomationTestBase* Test;
		TSharedRef<FLoadTestState> State;
	};

	bool WriteFixture(const FString& Directory, float Extent)
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
		FDatasetMetaBinary Meta{};
		FMemory::Memcpy(Meta.Magic, "TDSH", 4);
		Meta.FormatVersion = 1;
		Meta.FirstTimeStep = 925;
		Meta.LastTimeStep = 1159;
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(&Meta), sizeof(Meta));
		if (!FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(Directory, TEXT("dataset-meta.bin"))) ||
			!FSpatialHashTableBuilder::CreateDirectoryStructure(Directory, 0.1f))
		{
			return false;
		}
		for (int32 Time = Meta.FirstTimeStep; Time <= Meta.LastTimeStep; ++Time)
		{
			FSpatialHashTable Table;
			Table.Header.TimeStep = Time;
			Table.Header.CellSize = 0.1f;
			Table.Header.SetBBoxMax(FVector(Extent));
			if (!Table.SaveToFile(FSpatialHashTableBuilder::GetOutputFilename(Directory, 0.1f, Time)))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialHashAsyncCoverageTest,
	"SpatialHash.Loading.AsyncCoverageAndDatasetSwitch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialHashAsyncCoverageTest::RunTest(const FString& Parameters)
{
	auto State = MakeShared<FLoadTestState>();
	State->Manager = NewObject<USpatialHashTableManager>();
	State->Manager->AddToRoot();
	State->Manager->SetPeriodicVolume(true, FVector::ZeroVector);
	State->FixtureDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SpatialHash"), FGuid::NewGuid().ToString());
	const FString First = FPaths::Combine(State->FixtureDirectory, TEXT("First"));
	const FString Second = FPaths::Combine(State->FixtureDirectory, TEXT("Second"));
	if (!WriteFixture(First, 1.0f) || !WriteFixture(Second, 2.0f))
	{
		AddError(TEXT("Failed to write spatial-hash fixture"));
		State->bComplete = true;
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForHashLoad(this, State));
		return false;
	}
	// Auto-create is deliberately enabled but there are no shards. A rebuild
	// caused by the 100 out-of-range timesteps would fail this test.
	State->Manager->LoadHashTablesAsync(First, 0.1f, 1026, 1259, true,
		FOnSpatialHashLoadComplete::CreateLambda([this, State, Second](const FSpatialHashLoadResult& Result)
	{
		TestTrue(TEXT("Load callback is on the game thread"), IsInGameThread());
		TestEqual(TEXT("All overlapping tables load"), Result.LoadedCount, 134);
		TestEqual(TEXT("Unavailable samples are classified separately"), Result.OutsideDatasetCount, 100);
		TestEqual(TEXT("Outside samples are not failures"), Result.FailedCount, 0);
		TestTrue(TEXT("No rebuild error"), Result.ErrorMessage.IsEmpty());
		TestTrue(TEXT("Last available table is cached"), State->Manager->IsHashTableLoaded(0.1f, 1159));
		TestFalse(TEXT("No nonexistent table cached"), State->Manager->IsHashTableLoaded(0.1f, 1160));
		TestTrue(TEXT("First dataset extent"), State->Manager->GetResolvedPeriodicExtent(0.1f).Equals(FVector(1.0f)));
		State->Manager->LoadHashTablesAsync(Second, 0.1f, 1026, 1259, true,
			FOnSpatialHashLoadComplete::CreateLambda([this, State, Second](const FSpatialHashLoadResult& Switched)
		{
			TestEqual(TEXT("Dataset switch loads matching tables"), Switched.LoadedCount, 134);
			TestTrue(TEXT("Dataset switch does not reuse old entries"), State->Manager->GetResolvedPeriodicExtent(0.1f).Equals(FVector(2.0f)));
			State->Manager->LoadHashTablesAsync(Second, 0.1f, 1160, 1259, true,
				FOnSpatialHashLoadComplete::CreateLambda([this, State](const FSpatialHashLoadResult& Outside)
			{
				TestEqual(TEXT("Disjoint query loads no tables"), Outside.LoadedCount, 0);
				TestEqual(TEXT("All disjoint samples classified"), Outside.OutsideDatasetCount, 100);
				TestEqual(TEXT("No fictitious load failures"), Outside.FailedCount, 0);
				TestEqual(TEXT("Disjoint query has actionable error"), Outside.ErrorMessage,
					FString(TEXT("Query time range does not overlap the target dataset.")));
				State->bComplete = true;
			}));
		}));
	}));
	TestFalse(TEXT("Load completion is deferred"), State->bComplete);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForHashLoad(this, State));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpatialHashRealCoverageTest,
	"SpatialHash.Loading.RealDatasetCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpatialHashRealCoverageTest::RunTest(const FString& Parameters)
{
	FString Directory;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SpatialHashTestDataset="), Directory))
	{
		AddInfo(TEXT("Supply -SpatialHashTestDataset=<particles directory> to test turbulence_case_53 without changing source files."));
		return true;
	}
	auto State = MakeShared<FLoadTestState>();
	State->Manager = NewObject<USpatialHashTableManager>();
	State->Manager->AddToRoot();
	const double Before = FPlatformTime::Seconds();
	// Read-only against the supplied dataset: auto-create is disabled.
	State->Manager->LoadHashTablesAsync(Directory, 0.1f, 1026, 1259, false,
		FOnSpatialHashLoadComplete::CreateLambda([this, State](const FSpatialHashLoadResult& Result)
	{
		TestTrue(TEXT("Real-data callback on game thread"), IsInGameThread());
		TestEqual(TEXT("134 real tables validated and loaded"), Result.LoadedCount, 134);
		TestEqual(TEXT("100 samples beyond particle coverage"), Result.OutsideDatasetCount, 100);
		TestEqual(TEXT("No real tables failed validation"), Result.FailedCount, 0);
		TestTrue(TEXT("Real load succeeds"), Result.ErrorMessage.IsEmpty());
		AddInfo(FString::Printf(TEXT("Real-data async load finished in %.3f seconds"), FPlatformTime::Seconds() - State->Started));
		State->bComplete = true;
	}));
	const double DispatchSeconds = FPlatformTime::Seconds() - Before;
	AddInfo(FString::Printf(TEXT("Game-thread load dispatch returned in %.3f ms"), DispatchSeconds * 1000.0));
	TestTrue(TEXT("Game thread does not wait for real table I/O"), DispatchSeconds < 0.1);
	TestFalse(TEXT("Real load is not synchronous"), State->bComplete);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForHashLoad(this, State));
	return true;
}
#endif
