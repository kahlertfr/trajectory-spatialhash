// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserSelectionManager.h"
#include "TrajectorySelectionListener.h"
#include "TrajectorySelectionProvider.h"

// ─── Selection Access ─────────────────────────────────────────────────────────

TArray<FUserTrajectorySelection> UUserSelectionManager::GetSelections() const
{
	return Selections;
}

// ─── Selection Mutation ───────────────────────────────────────────────────────

void UUserSelectionManager::AddSelection(FUserTrajectorySelection Selection)
{
	Selections.Add(MoveTemp(Selection));
	NotifyListeners();
}

bool UUserSelectionManager::UpdateSelection(int32 Index, FUserTrajectorySelection Selection)
{
	if (!Selections.IsValidIndex(Index))
	{
		return false;
	}
	Selections[Index] = MoveTemp(Selection);
	NotifyListeners();
	return true;
}

bool UUserSelectionManager::RemoveSelection(int32 Index)
{
	if (!Selections.IsValidIndex(Index))
	{
		return false;
	}
	Selections.RemoveAt(Index);
	NotifyListeners();
	return true;
}

void UUserSelectionManager::ClearSelections()
{
	Selections.Empty();
	NotifyListeners();
}

// ─── Listener Registration ────────────────────────────────────────────────────

void UUserSelectionManager::RegisterListener(UObject* Listener)
{
	if (!Listener || !Listener->Implements<UTrajectorySelectionListener>())
	{
		return;
	}

	for (const TWeakObjectPtr<UObject>& Existing : RegisteredListeners)
	{
		if (Existing.Get() == Listener)
		{
			return; // already registered
		}
	}
	RegisteredListeners.Add(Listener);
}

void UUserSelectionManager::UnregisterListener(UObject* Listener)
{
	RegisteredListeners.RemoveAll([Listener](const TWeakObjectPtr<UObject>& Ptr)
	{
		return Ptr.Get() == Listener;
	});
}

// ─── Provider Registration ────────────────────────────────────────────────────

void UUserSelectionManager::RegisterProvider(UObject* Provider)
{
	if (!Provider || !Provider->Implements<UTrajectorySelectionProvider>())
	{
		return;
	}

	for (const TWeakObjectPtr<UObject>& Existing : RegisteredProviders)
	{
		if (Existing.Get() == Provider)
		{
			return; // already registered
		}
	}
	RegisteredProviders.Add(Provider);
}

void UUserSelectionManager::UnregisterProvider(UObject* Provider)
{
	RegisteredProviders.RemoveAll([Provider](const TWeakObjectPtr<UObject>& Ptr)
	{
		return Ptr.Get() == Provider;
	});
}

void UUserSelectionManager::RefreshFromProviders()
{
	Selections.Empty();

	for (int32 i = RegisteredProviders.Num() - 1; i >= 0; --i)
	{
		UObject* Provider = RegisteredProviders[i].Get();
		if (!Provider)
		{
			RegisteredProviders.RemoveAt(i);
			continue;
		}

		TArray<FUserTrajectorySelection> Provided =
			ITrajectorySelectionProvider::Execute_GetProvidedSelections(Provider);

		Selections.Append(MoveTemp(Provided));
	}

	NotifyListeners();
}

// ─── Internal ─────────────────────────────────────────────────────────────────

void UUserSelectionManager::NotifyListeners()
{
	// Broadcast the assignable delegate (Blueprint event bindings)
	OnSelectionsChanged.Broadcast(Selections);

	// Call the interface method on all registered listener objects
	for (int32 i = RegisteredListeners.Num() - 1; i >= 0; --i)
	{
		UObject* Listener = RegisteredListeners[i].Get();
		if (!Listener)
		{
			RegisteredListeners.RemoveAt(i);
			continue;
		}
		ITrajectorySelectionListener::Execute_OnUserSelectionsChanged(Listener, Selections);
	}
}
