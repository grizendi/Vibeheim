#include "VHMNPCTags.h"

#include "NativeGameplayTags.h"

namespace VHMNPCTags
{
    UE_DEFINE_GAMEPLAY_TAG(TAG_Resource_Food, "Resource.Food");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Resource_Water, "Resource.Water");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Resource_Shelter, "Resource.Shelter");

    UE_DEFINE_GAMEPLAY_TAG(TAG_Need_Hunger, "Need.Hunger");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Need_Thirst, "Need.Thirst");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Need_Energy, "Need.Energy");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Need_Health, "Need.Health");

    FGameplayTag ResourceFood()
    {
        return TAG_Resource_Food;
    }

    FGameplayTag ResourceWater()
    {
        return TAG_Resource_Water;
    }

    FGameplayTag ResourceShelter()
    {
        return TAG_Resource_Shelter;
    }

    FGameplayTag NeedHunger()
    {
        return TAG_Need_Hunger;
    }

    FGameplayTag NeedThirst()
    {
        return TAG_Need_Thirst;
    }

    FGameplayTag NeedEnergy()
    {
        return TAG_Need_Energy;
    }

    FGameplayTag NeedHealth()
    {
        return TAG_Need_Health;
    }
}

