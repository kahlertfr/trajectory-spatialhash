#include "TrajectoryInspectionWidget.h"
#include "TrajectoryQueryNiagaraActor.h"
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
			VisualizationActor->IsInspectableTrajectoryFocus(Index) ? TEXT("focus, ") : TEXT(""), *Identity)));
	}
	else SelectionLabel->SetText(Count > 0
		? NSLOCTEXT("TrajectoryInspection", "None", "Trajectory: none selected")
		: NSLOCTEXT("TrajectoryInspection", "Empty", "Trajectory: no results"));
}

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

