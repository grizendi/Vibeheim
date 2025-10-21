#pragma once

#include "GameplayTagContainer.h"

namespace VHMNPCTags
{
    VIBEHEIM_API FGameplayTag ResourceFood();
    VIBEHEIM_API FGameplayTag ResourceWater();
    VIBEHEIM_API FGameplayTag ResourceShelter();

    VIBEHEIM_API FGameplayTag NeedHunger();
    VIBEHEIM_API FGameplayTag NeedThirst();
    VIBEHEIM_API FGameplayTag NeedEnergy();
    VIBEHEIM_API FGameplayTag NeedHealth();
}
