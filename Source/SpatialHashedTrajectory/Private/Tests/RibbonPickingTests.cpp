#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "TrajectoryQueryNiagaraActor.h"
#include "ConfirmTrajectorySelectionAsyncAction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrajectoryRibbonPickingTest,
                                 "TrajectoryBLab.Query.RibbonPicking.Confirmation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrajectoryRibbonPickingTest::RunTest(const FString &Parameters)
{
	auto *World = UWorld::CreateWorld(EWorldType::Game, false);
	auto *Actor = World->SpawnActor<ATrajectoryQueryNiagaraActor>();
	Actor->UpdateInspectableTrajectories(true, {41, 9});
	Actor->RibbonPickingSampleCount = 8;
	const auto Pose = [&]() { Actor->UpdateTrajectorySelector(FVector(100, 200, 300), 2); };
	const auto Press = [&](uint64 Frame) {
		Actor->UpdateRibbonConfirmation(false, Frame);
		Pose();
		Actor->UpdateRibbonConfirmation(true, Frame);
	};
	const auto Reply = [&](uint64 Frame, int32 Index) {
		Actor->ReceiveRibbonPick(Actor->GetVisibleBoundsRevision(), Actor->TrajectorySelectorSession, Frame,
		                         Index, FVector(100, 200, 300));
	};
	Pose();
	Reply(10, 1);
	TestEqual(TEXT("Proximity previews the nearest candidate"), Actor->CandidateTrajectoryIndex, 1);
	const auto InitialRevision = Actor->GetVisibleBoundsRevision();
	Reply(11, 2);
	TestEqual(TEXT("Preview can switch candidates"), Actor->CandidateTrajectoryIndex, 2);
	TestEqual(TEXT("Preview does not invalidate display bounds"), Actor->GetVisibleBoundsRevision(),
	          InitialRevision);
	Reply(12, INDEX_NONE);
	TestEqual(TEXT("A miss clears the preview"), Actor->CandidateTrajectoryIndex, INDEX_NONE);
	Reply(13, 1);
	const uint64 OldSession = Actor->TrajectorySelectorSession;
	Actor->UpdateTrajectorySelector(FVector(100, 200, 300), 3);
	TestEqual(TEXT("Changing radius clears the candidate"), Actor->CandidateTrajectoryIndex, INDEX_NONE);
	Actor->ReceiveRibbonPick(Actor->GetVisibleBoundsRevision(), OldSession, 14, 2, FVector(100, 200, 300));
	TestEqual(TEXT("Old-radius hits cannot restore the candidate"), Actor->CandidateTrajectoryIndex, INDEX_NONE);

	Actor->ReceiveRibbonPick(Actor->GetVisibleBoundsRevision(), Actor->TrajectorySelectorSession, 14, 2,
	                         FVector::ZeroVector);
	TestEqual(TEXT("A distant old fingertip pose cannot highlight a candidate"),
	          Actor->CandidateTrajectoryIndex, INDEX_NONE);
	TestEqual(TEXT("Proximity without confirmation never selects"), Actor->SelectedTrajectoryIndex,
	          INDEX_NONE);
	Press(20);
	Reply(20, 1);
	TestEqual(TEXT("Press selects the GPU winner"), Actor->SelectedTrajectoryIndex, 1);
	Actor->UpdateRibbonConfirmation(true, 21);
	Reply(21, 2);
	TestEqual(TEXT("Holding confirmation cannot select again"), Actor->SelectedTrajectoryIndex, 1);
	Press(30);
	Reply(30, INDEX_NONE);
	Reply(31, 2);
	TestEqual(TEXT("A miss consumes the press and preserves selection"), Actor->SelectedTrajectoryIndex, 1);
	Press(40);
	const uint64 OldRevision = Actor->GetVisibleBoundsRevision();
	Actor->SetVisualizationTimeRange(3, 4);
	Actor->ReceiveRibbonPick(OldRevision, Actor->TrajectorySelectorSession, 40, 2, FVector(100, 200, 300));
	TestEqual(TEXT("Filter changes reject stale hits"), Actor->SelectedTrajectoryIndex, 1);
	Press(50);
	Actor->UpdateTrajectorySelector(FVector::ZeroVector, 0, false);
	Reply(50, 2);
	TestEqual(TEXT("Deactivation removes candidate width immediately"), Actor->CandidateTrajectoryIndex,
	          INDEX_NONE);
	TestEqual(TEXT("Deactivation cancels pending confirmation"), Actor->SelectedTrajectoryIndex, 1);
	Press(60);
	Reply(59, 2);
	Reply(60, 0);
	TestEqual(TEXT("A press can select the focus and ignores older frames"), Actor->SelectedTrajectoryIndex,
	          0);
	Press(70);
	Actor->UpdateInspectableTrajectories(true, {9, 41});
	Reply(70, 1);
	TestEqual(TEXT("Reordered display invalidates pending pick indices"), Actor->SelectedTrajectoryIndex, 0);
	Actor->UpdateRibbonConfirmation(false, 80);
	Actor->UpdateTrajectorySelector(FVector::ZeroVector, -1);
	Actor->UpdateRibbonConfirmation(true, 80);
	Reply(80, 2);
	TestEqual(TEXT("Invalid radius cannot select"), Actor->SelectedTrajectoryIndex, 0);
	Press(90);
	Actor->TrajectorySelectorUpdateTime -= 1.0;
	Actor->RefreshRibbonCandidate();
	Reply(90, 2);
	TestEqual(TEXT("Expired selector clears preview"), Actor->CandidateTrajectoryIndex, INDEX_NONE);
	TestEqual(TEXT("Expired selector cannot consume delayed hits"), Actor->SelectedTrajectoryIndex, 0);
	const auto StartAsync = [&]() {
		Pose();
		Actor->PendingRibbonConfirmationFrame = 0;
		Actor->LastRibbonPickFrame = 0;
		auto* Action = NewObject<UConfirmTrajectorySelectionAsyncAction>();
		Action->Actor = Actor;
		Action->Activate();
		return Action;
	};
	auto* Success = StartAsync();
	const uint64 RequestFrame = Actor->PendingRibbonConfirmationFrame;
	Reply(RequestFrame - 1, 2);
	TestFalse(TEXT("Pre-request hover cannot complete async confirmation"), Success->bFinished);
	Reply(RequestFrame, 1);
	TestTrue(TEXT("Accepted readback completes the action"), Success->bFinished);
	TestEqual(TEXT("Async confirmation commits before completion"), Actor->SelectedTrajectoryIndex, 1);
	TestFalse(TEXT("Completed action detaches from actor"), Actor->AsyncRibbonConfirmation.IsValid());
	auto* SameSelection = StartAsync();
	Reply(Actor->PendingRibbonConfirmationFrame, 1);
	TestTrue(TEXT("Reselecting the same trajectory completes"), SameSelection->bFinished);
	auto* Miss = StartAsync();
	Reply(Actor->PendingRibbonConfirmationFrame, INDEX_NONE);
	TestTrue(TEXT("Miss terminates the request"), Miss->bFinished);
	TestEqual(TEXT("Async miss preserves selection"), Actor->SelectedTrajectoryIndex, 1);
	auto* Cancelled = StartAsync();
	Actor->UpdateTrajectorySelector(FVector::ZeroVector, 0, false);
	Cancelled->CheckRequest(0);
	TestTrue(TEXT("Disabling selector terminates async request"), Cancelled->bFinished);
	auto* Changed = StartAsync();
	Actor->SetVisualizationTimeRange(5, 6);
	Changed->CheckRequest(0);
	TestTrue(TEXT("Display change terminates async request"), Changed->bFinished);
	auto* TimedOut = StartAsync();
	TimedOut->Deadline = 0;
	TimedOut->CheckRequest(0);
	TestTrue(TEXT("Missing GPU readback times out"), TimedOut->bFinished);
	TestEqual(TEXT("Timeout clears pending confirmation"), Actor->PendingRibbonConfirmationFrame, uint64(0));
	auto* First = StartAsync();
	auto* Duplicate = NewObject<UConfirmTrajectorySelectionAsyncAction>();
	Duplicate->Actor = Actor;
	Duplicate->Activate();
	TestTrue(TEXT("Concurrent confirmation fails immediately"), Duplicate->bFinished);
	TestTrue(TEXT("Concurrent request preserves original owner"), Actor->AsyncRibbonConfirmation.Get() == First);
	First->Finish(INDEX_NONE, -1, TEXT("Test cleanup"));
	auto* Destroyed = StartAsync();
	Actor->Destroy();
	Destroyed->CheckRequest(0);
	TestTrue(TEXT("Actor destruction terminates async request"), Destroyed->bFinished);
	World->DestroyWorld(false);
	return true;
}
#endif
