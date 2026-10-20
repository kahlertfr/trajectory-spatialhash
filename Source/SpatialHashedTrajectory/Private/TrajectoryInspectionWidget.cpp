#include "TrajectoryInspectionWidget.h"
#include "TrajectoryQueryNiagaraActor.h"
#include "TrajectorySpatialPicker.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Components/WidgetComponent.h"

TSharedRef<SWidget> UTrajectoryInspectionWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Root = WidgetTree->ConstructWidget<UVerticalBox>();
		WidgetTree->RootWidget = Root;
		SelectionLabel = WidgetTree->ConstructWidget<UTextBlock>();
		Root->AddChildToVerticalBox(SelectionLabel);
		IndexSlider = WidgetTree->ConstructWidget<USlider>();
		IndexSlider->SetStepSize(1.f);
		IndexSlider->MouseUsesStep = true;
		IndexSlider->OnValueChanged.AddDynamic(this, &UTrajectoryInspectionWidget::SelectIndex);
		IndexSlider->OnMouseCaptureEnd.AddDynamic(this, &UTrajectoryInspectionWidget::SelectCurrentIndex);
		IndexSlider->OnControllerCaptureEnd.AddDynamic(this, &UTrajectoryInspectionWidget::SelectCurrentIndex);
		Root->AddChildToVerticalBox(IndexSlider)->SetPadding(FMargin(0, 8));
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Root->AddChildToVerticalBox(Row);
		OnlySelected = WidgetTree->ConstructWidget<UCheckBox>();
		auto* ToggleLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ToggleLabel->SetText(NSLOCTEXT("TrajectoryInspection", "Only", "Only selected"));
		OnlySelected->SetContent(ToggleLabel);
		OnlySelected->OnCheckStateChanged.AddDynamic(this, &UTrajectoryInspectionWidget::SetOnlySelected);
		Row->AddChildToHorizontalBox(OnlySelected)->SetPadding(FMargin(0, 0, 20, 0));
		ClearButton = WidgetTree->ConstructWidget<UButton>();
		auto* ClearLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ClearLabel->SetText(NSLOCTEXT("TrajectoryInspection", "Clear", "Clear selection"));
		ClearButton->SetContent(ClearLabel);
		ClearButton->OnClicked.AddDynamic(this, &UTrajectoryInspectionWidget::ClearSelection);
		Row->AddChildToHorizontalBox(ClearButton);
		auto* PickingRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Root->AddChildToVerticalBox(PickingRow)->SetPadding(FMargin(0, 8, 0, 0));
		HandSelectorButton = WidgetTree->ConstructWidget<UButton>();
		HandSelectorLabel = WidgetTree->ConstructWidget<UTextBlock>();
		HandSelectorButton->SetContent(HandSelectorLabel);
		HandSelectorButton->OnClicked.AddDynamic(this, &UTrajectoryInspectionWidget::ToggleHandSelector);
		PickingRow->AddChildToHorizontalBox(HandSelectorButton)->SetPadding(FMargin(0, 0, 12, 0));
		ConfirmButton = WidgetTree->ConstructWidget<UButton>();
		auto* ConfirmLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ConfirmLabel->SetText(NSLOCTEXT("TrajectoryInspection", "Confirm", "Confirm"));
		ConfirmButton->SetContent(ConfirmLabel);
		ConfirmButton->OnClicked.AddDynamic(this, &UTrajectoryInspectionWidget::ConfirmPicking);
		PickingRow->AddChildToHorizontalBox(ConfirmButton)->SetPadding(FMargin(0, 0, 12, 0));
		CancelButton = WidgetTree->ConstructWidget<UButton>();
		auto* CancelLabel = WidgetTree->ConstructWidget<UTextBlock>();
		CancelLabel->SetText(NSLOCTEXT("TrajectoryInspection", "Cancel", "Cancel picking"));
		CancelButton->SetContent(CancelLabel);
		CancelButton->OnClicked.AddDynamic(this, &UTrajectoryInspectionWidget::CancelPicking);
		PickingRow->AddChildToHorizontalBox(CancelButton);
	}
	RefreshControls();
	return Super::RebuildWidget();
}

void UTrajectoryInspectionWidget::SetVisualizationActor(ATrajectoryQueryNiagaraActor* Actor)
{
	if (VisualizationActor) VisualizationActor->OnTrajectoryInspectionChanged.RemoveDynamic(this, &UTrajectoryInspectionWidget::RefreshControls);
	VisualizationActor = Actor;
	if (VisualizationActor) VisualizationActor->OnTrajectoryInspectionChanged.AddUniqueDynamic(this, &UTrajectoryInspectionWidget::RefreshControls);
	RefreshControls();
}

void UTrajectoryInspectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisualizationActor(VisualizationActor);
}

void UTrajectoryInspectionWidget::NativeDestruct()
{
	if (VisualizationActor) VisualizationActor->OnTrajectoryInspectionChanged.RemoveDynamic(this, &UTrajectoryInspectionWidget::RefreshControls);
	Super::NativeDestruct();
}

void UTrajectoryInspectionWidget::RefreshControls()
{
	if (!IndexSlider) return;
	TGuardValue<bool> Guard(bRefreshing, true);
	const bool bValidActor = IsValid(VisualizationActor);
	const int32 Count = bValidActor ? VisualizationActor->GetInspectableTrajectoryCount() : 0;
	const int32 Index = bValidActor ? VisualizationActor->SelectedTrajectoryIndex : INDEX_NONE;
	const bool bPicking = bValidActor && VisualizationActor->bSpatialPickingActive;
	HandSelectorButton->SetIsEnabled(Count > 0);
	HandSelectorLabel->SetText(ATrajectorySpatialPicker::FindForTarget(VisualizationActor)
		? NSLOCTEXT("TrajectoryInspection", "DisableHand", "Disable hand selector")
		: NSLOCTEXT("TrajectoryInspection", "EnableHand", "Enable hand selector"));
	ConfirmButton->SetIsEnabled(bPicking && VisualizationActor->CandidateTrajectoryIndex != INDEX_NONE);
	CancelButton->SetIsEnabled(bPicking);
	IndexSlider->SetMinValue(0);
	IndexSlider->SetMaxValue(FMath::Max(1, Count - 1));
	IndexSlider->SetValue(Index == INDEX_NONE ? 0 : Index);
	IndexSlider->SetIsEnabled(Count > 0);
	OnlySelected->SetIsEnabled(Count > 0);
	OnlySelected->SetIsChecked(bValidActor && VisualizationActor->bShowOnlySelectedTrajectory);
	ClearButton->SetIsEnabled(Index != INDEX_NONE);
	int64 Id = -1;
	if (bValidActor && VisualizationActor->GetInspectableTrajectoryId(Index, Id))
	{
		const FString Identity = Id >= 0 ? FString::Printf(TEXT("ID %lld"), Id) : TEXT("synthetic focus");
		SelectionLabel->SetText(FText::FromString(FString::Printf(TEXT("Trajectory %d / %d (%s%s)"), Index, Count - 1,
			Index == 0 ? TEXT("focus, ") : TEXT(""), *Identity)));
	}
	else SelectionLabel->SetText(Count > 0
		? NSLOCTEXT("TrajectoryInspection", "None", "Trajectory: none selected")
		: NSLOCTEXT("TrajectoryInspection", "Empty", "Trajectory: no results"));
	if (bPicking)
	{
		const int32 Candidate = VisualizationActor->CandidateTrajectoryIndex;
		if (VisualizationActor->GetInspectableTrajectoryId(Candidate, Id))
			SelectionLabel->SetText(FText::FromString(FString::Printf(TEXT("Preview trajectory %d (ID %lld) - confirm or cancel"), Candidate, Id)));
		else SelectionLabel->SetText(NSLOCTEXT("TrajectoryInspection", "PickingNoHit", "Picking: move the index tip onto a trajectory"));
	}
}

void UTrajectoryInspectionWidget::ToggleHandSelector()
{
	if (!IsValid(VisualizationActor)) return;
	if (auto* Tool = ATrajectorySpatialPicker::FindForTarget(VisualizationActor)) Tool->Disable();
	else ATrajectorySpatialPicker::EnableForTarget(VisualizationActor);
	RefreshControls();
}

void UTrajectoryInspectionWidget::ConfirmPicking() { if (IsValid(VisualizationActor)) VisualizationActor->ConfirmSpatialPicking(); }
void UTrajectoryInspectionWidget::CancelPicking() { if (IsValid(VisualizationActor)) VisualizationActor->CancelSpatialPicking(); }

void UTrajectoryInspectionWidget::SelectIndex(float Value)
{
	if (bRefreshing || !IsValid(VisualizationActor) || !FMath::IsFinite(Value)) return;
	const int32 Count = VisualizationActor->GetInspectableTrajectoryCount();
	if (Count > 0) VisualizationActor->SelectTrajectoryByIndex(FMath::Clamp(FMath::RoundToInt(Value), 0, Count - 1));
}

void UTrajectoryInspectionWidget::SelectCurrentIndex() { if (IndexSlider) SelectIndex(IndexSlider->GetValue()); }
void UTrajectoryInspectionWidget::SetOnlySelected(bool bChecked) { if (!bRefreshing && IsValid(VisualizationActor)) VisualizationActor->SetShowOnlySelectedTrajectory(bChecked); }
void UTrajectoryInspectionWidget::ClearSelection() { if (IsValid(VisualizationActor)) VisualizationActor->ClearTrajectoryInspection(); }

void UTrajectoryInspectionWidget::BindInspectionControls(AActor* ControlsActor, ATrajectoryQueryNiagaraActor* Actor)
{
	if (!IsValid(ControlsActor)) return;
	TInlineComponentArray<UWidgetComponent*> Components(ControlsActor);
	for (auto* Component : Components)
	{
		Component->InitWidget();
		auto* Root = Component->GetUserWidgetObject();
		if (!Root) continue;
		if (auto* Inspection = Cast<UTrajectoryInspectionWidget>(Root)) Inspection->SetVisualizationActor(Actor);
		if (Root->WidgetTree) Root->WidgetTree->ForEachWidget([Actor](UWidget* Widget)
		{
			if (auto* Inspection = Cast<UTrajectoryInspectionWidget>(Widget)) Inspection->SetVisualizationActor(Actor);
		});
	}
}

