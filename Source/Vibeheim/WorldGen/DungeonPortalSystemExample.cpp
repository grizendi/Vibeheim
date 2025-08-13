#include "DungeonPortalSystemExample.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DungeonPortalSystem.h"

ADungeonPortalSystemExample::ADungeonPortalSystemExample()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Set up example custom portal rule
    CustomPortalRule.PortalTypeName = TEXT("CustomDungeonPortal");
    CustomPortalRule.TargetLevelName = TEXT("CustomDungeonLevel");
    CustomPortalRule.MinSpacing = 1200.0f;
    CustomPortalRule.MaxSlope = 20.0f;
    CustomPortalRule.MinAltitude = 50.0f;
    CustomPortalRule.MaxAltitude = 300.0f;
    CustomPortalRule.SpawnProbability = 0.01f;
    CustomPortalRule.AllowedBiomes = {TEXT("Meadows"), TEXT("BlackForest")};
    CustomPortalRule.FlattenRadius = 30.0f;
    CustomPortalRule.InteractionRadius = 6.0f;
}

void ADungeonPortalSystemExample::BeginPlay()
{
    Super::BeginPlay();
    
    // Find the world generation manager
    WorldGenManager = Cast<AWorldGenManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AWorldGenManager::StaticClass()));
    
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("DungeonPortalSystemExample: Could not find WorldGenManager in the world"));
    }
}

void ADungeonPortalSystemExample::CreateCustomPortalRule()
{
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldGenManager not available"));
        return;
    }
    
    // Add the custom portal rule to the system
    WorldGenManager->AddPortalSpawnRule(CustomPortalRule);
    
    UE_LOG(LogTemp, Log, TEXT("Added custom portal rule: %s targeting %s"), 
           *CustomPortalRule.PortalTypeName, *CustomPortalRule.TargetLevelName);
    
    // Print to screen for easy debugging
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, 
            FString::Printf(TEXT("Added custom portal rule: %s"), *CustomPortalRule.PortalTypeName));
    }
}

void ADungeonPortalSystemExample::ListAllActivePortals()
{
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldGenManager not available"));
        return;
    }
    
    TArray<FDungeonPortal> ActivePortals = WorldGenManager->GetAllActivePortals();
    
    UE_LOG(LogTemp, Log, TEXT("Found %d active portals:"), ActivePortals.Num());
    
    for (int32 i = 0; i < ActivePortals.Num(); ++i)
    {
        const FDungeonPortal& Portal = ActivePortals[i];
        UE_LOG(LogTemp, Log, TEXT("  %d: %s at %s -> %s (Biome: %s)"), 
               i + 1, 
               *Portal.PortalTypeName, 
               *Portal.WorldLocation.ToString(), 
               *Portal.TargetLevelName,
               *Portal.BiomeName);
    }
    
    // Print summary to screen
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, 
            FString::Printf(TEXT("Found %d active portals (see log for details)"), ActivePortals.Num()));
    }
}

void ADungeonPortalSystemExample::FindPortalsNearLocation(const FVector& Location, float SearchRadius)
{
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldGenManager not available"));
        return;
    }
    
    TArray<FDungeonPortal> AllPortals = WorldGenManager->GetAllActivePortals();
    TArray<FDungeonPortal> NearbyPortals;
    
    for (const FDungeonPortal& Portal : AllPortals)
    {
        float Distance = FVector::Dist(Location, Portal.WorldLocation);
        if (Distance <= SearchRadius)
        {
            NearbyPortals.Add(Portal);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Found %d portals within %.1f units of %s:"), 
           NearbyPortals.Num(), SearchRadius, *Location.ToString());
    
    for (int32 i = 0; i < NearbyPortals.Num(); ++i)
    {
        const FDungeonPortal& Portal = NearbyPortals[i];
        float Distance = FVector::Dist(Location, Portal.WorldLocation);
        UE_LOG(LogTemp, Log, TEXT("  %d: %s at distance %.1f -> %s"), 
               i + 1, *Portal.PortalTypeName, Distance, *Portal.TargetLevelName);
    }
    
    // Print to screen
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, 
            FString::Printf(TEXT("Found %d portals within %.1f units"), NearbyPortals.Num(), SearchRadius));
    }
}

void ADungeonPortalSystemExample::TogglePortalAtLocation(const FVector& Location, float SearchRadius)
{
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldGenManager not available"));
        return;
    }
    
    // Find portal actors near the location
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADungeonPortalActor::StaticClass(), FoundActors);
    
    ADungeonPortalActor* ClosestPortal = nullptr;
    float ClosestDistance = SearchRadius;
    
    for (AActor* Actor : FoundActors)
    {
        ADungeonPortalActor* PortalActor = Cast<ADungeonPortalActor>(Actor);
        if (PortalActor)
        {
            float Distance = FVector::Dist(Location, PortalActor->GetActorLocation());
            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestPortal = PortalActor;
            }
        }
    }
    
    if (ClosestPortal)
    {
        const FDungeonPortal& PortalData = ClosestPortal->GetPortalData();
        bool bNewActiveState = !PortalData.bIsActive;
        
        ClosestPortal->SetPortalActive(bNewActiveState);
        
        UE_LOG(LogTemp, Log, TEXT("Toggled portal %s to %s"), 
               *PortalData.PortalTypeName, bNewActiveState ? TEXT("active") : TEXT("inactive"));
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, bNewActiveState ? FColor::Green : FColor::Red, 
                FString::Printf(TEXT("Portal %s: %s"), *PortalData.PortalTypeName, 
                bNewActiveState ? TEXT("ACTIVATED") : TEXT("DEACTIVATED")));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No portal found within %.1f units of %s"), SearchRadius, *Location.ToString());
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("No portal found in range"));
        }
    }
}

void ADungeonPortalSystemExample::ShowPortalStatistics()
{
    if (!WorldGenManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("WorldGenManager not available"));
        return;
    }
    
    int32 TotalAttempts, SuccessfulPlacements, FailedPlacements;
    float AverageAttemptsPerPortal;
    
    WorldGenManager->GetPortalPlacementStats(TotalAttempts, SuccessfulPlacements, FailedPlacements, AverageAttemptsPerPortal);
    
    UE_LOG(LogTemp, Log, TEXT("Portal Placement Statistics:"));
    UE_LOG(LogTemp, Log, TEXT("  Total Attempts: %d"), TotalAttempts);
    UE_LOG(LogTemp, Log, TEXT("  Successful Placements: %d"), SuccessfulPlacements);
    UE_LOG(LogTemp, Log, TEXT("  Failed Placements: %d"), FailedPlacements);
    UE_LOG(LogTemp, Log, TEXT("  Average Attempts per Portal: %.2f"), AverageAttemptsPerPortal);
    
    float SuccessRate = (TotalAttempts > 0) ? (float(SuccessfulPlacements) / float(TotalAttempts)) * 100.0f : 0.0f;
    UE_LOG(LogTemp, Log, TEXT("  Success Rate: %.1f%%"), SuccessRate);
    
    // Print to screen
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan, 
            FString::Printf(TEXT("Portal Stats: %d successful, %d failed (%.1f%% success rate)"), 
            SuccessfulPlacements, FailedPlacements, SuccessRate));
    }
}