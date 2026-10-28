#pragma once

#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

namespace TrajectoryHemispherePalette
{
// The plane and legend read this same collection directly in their materials.
inline void Upload(UNiagaraComponent* Component)
{
    if (!Component) return;
    auto* Palette = LoadObject<UMaterialParameterCollection>(nullptr,
        TEXT("/SpatialHashedTrajectory/Visualization/MPC_AboveBelowPalette.MPC_AboveBelowPalette"));
    if (!ensureMsgf(Palette, TEXT("Above/below palette asset is missing"))) return;
    auto* Instance = Component->GetWorld() ? Component->GetWorld()->GetParameterCollectionInstance(Palette) : nullptr;
    for (const FName Name : { FName(TEXT("AboveColor")), FName(TEXT("BelowColor")) })
    {
        bool Found = false;
        FLinearColor Color = Palette->GetVectorParameterDefaultValue(Name, Found);
        if (Instance) Instance->GetVectorParameterValue(Name, Color);
        if (ensure(Found)) Component->SetVariableLinearColor(FName(*(TEXT("User.") + Name.ToString())), Color);
    }
}
}
