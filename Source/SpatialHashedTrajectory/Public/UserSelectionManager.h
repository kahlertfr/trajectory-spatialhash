// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UserSelectionManager.generated.h"

// ─── Data Type ────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ETrajectoryColorEncoding : uint8
{
	Time UMETA(DisplayName = "Time"),
	Distance UMETA(DisplayName = "Distance"),
	DistanceCategory UMETA(DisplayName = "Distance Category"),
	VolumeImage UMETA(DisplayName = "Volume Image"),
	Velocity UMETA(DisplayName = "Velocity"),
	SelectorHighlight UMETA(DisplayName = "Selector Highlight")
};

UENUM(BlueprintType)
enum class ETrajectoryDistanceFilter : uint8
{
	Inside UMETA(DisplayName = "Inside"),
	InsideAndOutside UMETA(DisplayName = "InsideAndOutside"),
	InsideAndPass UMETA(DisplayName = "InsideAndPass"),
	All UMETA(DisplayName = "All"),
	None UMETA(DisplayName = "None"),
	Outside UMETA(DisplayName = "Outside"),
	Pass UMETA(DisplayName = "Pass"),
	PassAndOutside UMETA(DisplayName = "PassAndOutside")

};

/**
 * Represents one user trajectory selection.
 *
 * Stores the trajectory to examine together with the time window and three
 * query radii used for spatial hash lookups.
 */
USTRUCT(BlueprintType)
struct FUserTrajectorySelection
{
	GENERATED_BODY()

	/** Identifier of the selected trajectory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	int32 TrajectoryId = 0;

	/** First time step of the selection range (inclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	int32 TimeStart = 0;

	/** Last time step of the selection range (inclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	int32 TimeEnd = 0;

	/** Whether the selection should be sensitive to the configured time range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	bool TimeRangeSensitive = false;

	/** Inner query radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	float InnerRadius = 0.f;

	/** Query radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	float QueryRadius = 0.f;

	/** Visibility radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	float VisibilityRadius = 0.f;

	/** Particle radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	float ParticleRadius = 0.f;

	/** Color encoding mode for rendering/analysis of this trajectory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	ETrajectoryColorEncoding ColorEncoding = ETrajectoryColorEncoding::Time;

	/** Distance-based filter mode to apply to this trajectory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	ETrajectoryDistanceFilter DistanceFilter = ETrajectoryDistanceFilter::All;

	/** Whether particles are visible for this selected trajectory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	bool bParticlesVisible = true;

	/** Whether this selection should render without shadow contribution */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	bool NoShadow = false;

	/** Whether this selection should render without shadow contribution */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "User Selection")
	bool ObjectViewWithRotation = false;
};

// ─── Delegate ─────────────────────────────────────────────────────────────────

/** Broadcast whenever the set of user selections changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserSelectionsChanged,
	const TArray<FUserTrajectorySelection>&, Selections);

// ─── Manager ──────────────────────────────────────────────────────────────────

/**
 * Manages a dynamic list of user trajectory selections and notifies registered
 * listeners whenever that list is modified.
 *
 * ## Typical Blueprint workflow
 *
 * 1. Create one UUserSelectionManager instance (e.g. in Game Mode / Game State).
 * 2. Blueprints that want to react to selection changes either:
 *    - bind to the **OnSelectionsChanged** assignable delegate, **or**
 *    - implement the **ITrajectorySelectionListener** interface and call
 *      **RegisterListener** on the manager.
 * 3. Blueprints that want to *supply* selections implement
 *    **ITrajectorySelectionProvider** and call **RegisterProvider**.
 *    Calling **RefreshFromProviders** then pulls the combined selections from
 *    every registered provider and notifies all listeners in one shot.
 */
UCLASS(BlueprintType, Blueprintable, Category = "User Selection")
class SPATIALHASHEDTRAJECTORY_API UUserSelectionManager : public UObject
{
	GENERATED_BODY()

public:

	// ─── Delegate ─────────────────────────────────────────────────────────────

	/**
	 * Broadcast to all listeners whenever the selections array changes.
	 * Bind to this from any Blueprint to be notified without registering as a
	 * formal listener.
	 */
	UPROPERTY(BlueprintAssignable, Category = "User Selection")
	FOnUserSelectionsChanged OnSelectionsChanged;

	// ─── Selection Access ─────────────────────────────────────────────────────

	/** Returns a copy of all current selections. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "User Selection")
	TArray<FUserTrajectorySelection> GetSelections() const;

	/**
	 * Find the selection whose TrajectoryId matches the supplied value.
	 *
	 * @param TrajectoryId  The trajectory identifier to search for.
	 * @param OutSelection  Receives the matching selection when found.
	 * @return true if a matching selection was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "User Selection")
	bool GetSelectionByTrajectoryId(int32 TrajectoryId, FUserTrajectorySelection& OutSelection) const;

	// ─── Selection Mutation ───────────────────────────────────────────────────

	/** Append a new selection and notify all listeners. */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void AddSelection(FUserTrajectorySelection Selection);

	/**
	 * Replace the selection at Index with the supplied value and notify all
	 * listeners.
	 * @return false if Index is out of range, true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	bool UpdateSelection(int32 Index, FUserTrajectorySelection Selection);

	/**
	 * Replace the selection whose TrajectoryId matches the supplied value and
	 * notify all listeners.
	 *
	 * @param TrajectoryId  The trajectory identifier to search for.
	 * @param Selection     The new selection values to store.
	 * @return true if a matching selection was found and updated, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	bool UpdateSelectionByTrajectoryId(int32 TrajectoryId, FUserTrajectorySelection Selection);

	/**
	 * Remove the selection at Index and notify all listeners.
	 * @return false if Index is out of range, true on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	bool RemoveSelection(int32 Index);

	/** Remove all selections and notify all listeners. */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void ClearSelections();

	// ─── Listener Registration ────────────────────────────────────────────────

	/**
	 * Register Object as a listener that will have
	 * ITrajectorySelectionListener::OnUserSelectionsChanged called on it
	 * whenever the selections change.
	 *
	 * Object must implement ITrajectorySelectionListener; the call is a no-op
	 * otherwise.  Duplicate registrations are silently ignored.
	 * Listeners are held as weak pointers and cleaned up automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void RegisterListener(UObject* Listener);

	/** Remove a previously registered listener. */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void UnregisterListener(UObject* Listener);

	// ─── Provider Registration ────────────────────────────────────────────────

	/**
	 * Register Object as a selection provider.
	 *
	 * Object must implement ITrajectorySelectionProvider; the call is a no-op
	 * otherwise.  Duplicate registrations are silently ignored.
	 * Providers are held as weak pointers and cleaned up automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void RegisterProvider(UObject* Provider);

	/** Remove a previously registered provider. */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void UnregisterProvider(UObject* Provider);

	/**
	 * Pull selections from every registered provider and replace the current
	 * selection list with the combined result.  Triggers OnSelectionsChanged
	 * exactly once after all providers have been polled.
	 *
	 * Stale (garbage-collected) providers are silently removed during the sweep.
	 */
	UFUNCTION(BlueprintCallable, Category = "User Selection")
	void RefreshFromProviders();

private:

	/** The current list of user selections. */
	UPROPERTY()
	TArray<FUserTrajectorySelection> Selections;

	/** Weak references to registered listener objects. */
	TArray<TWeakObjectPtr<UObject>> RegisteredListeners;

	/** Weak references to registered provider objects. */
	TArray<TWeakObjectPtr<UObject>> RegisteredProviders;

	/**
	 * Broadcast OnSelectionsChanged and call
	 * ITrajectorySelectionListener::Execute_OnUserSelectionsChanged on every
	 * live registered listener.  Stale listeners are removed.
	 */
	void NotifyListeners();
};
