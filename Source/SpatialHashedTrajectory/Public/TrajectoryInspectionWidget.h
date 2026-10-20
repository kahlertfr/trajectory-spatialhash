#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrajectoryInspectionWidget.generated.h"

class ATrajectoryQueryNiagaraActor;
class USlider;
class UTextBlock;
class UCheckBox;
class UButton;

/** Small reusable result-inspection control. Changes only inspection state. */
UCLASS()
class SPATIALHASHEDTRAJECTORY_API UTrajectoryInspectionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Trajectory Inspection")
	void SetVisualizationActor(ATrajectoryQueryNiagaraActor* Actor);

	/** Bind inspection controls in the existing configurator's widget components. */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Inspection")
	static void BindInspectionControls(AActor* ControlsActor, ATrajectoryQueryNiagaraActor* VisualizationActor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void NativeConstruct() override;

private:
	UFUNCTION() void RefreshControls();
	UFUNCTION() void SelectIndex(float Value);
	UFUNCTION() void SetOnlySelected(bool bChecked);
	UFUNCTION() void ClearSelection();
	UFUNCTION() void SelectCurrentIndex();
	UFUNCTION() void ToggleHandSelector();
	UFUNCTION() void ConfirmPicking();
	UFUNCTION() void CancelPicking();
	UPROPERTY(Transient) TObjectPtr<ATrajectoryQueryNiagaraActor> VisualizationActor;
	UPROPERTY(Transient) TObjectPtr<USlider> IndexSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SelectionLabel;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> OnlySelected;
	UPROPERTY(Transient) TObjectPtr<UButton> ClearButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HandSelectorLabel;
	UPROPERTY(Transient) TObjectPtr<UButton> HandSelectorButton;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CancelButton;
	bool bRefreshing = false;
};
