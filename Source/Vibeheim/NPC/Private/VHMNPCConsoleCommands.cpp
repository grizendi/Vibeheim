#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#if __has_include("NavMesh/NavMeshBoundsVolume.h")
#include "NavMesh/NavMeshBoundsVolume.h"
#define VHM_HAS_NAV_BOUNDS 1
#elif __has_include("AI/Navigation/NavMeshBoundsVolume.h")
#include "AI/Navigation/NavMeshBoundsVolume.h"
#define VHM_HAS_NAV_BOUNDS 1
#else
#define VHM_HAS_NAV_BOUNDS 0
#endif

#include "VHMNPCLogging.h"
#include "VHMNPCVars.h"
#include "VHMNPCCharacter.h"
#include "VHMResourceActor.h"
#include "VHMSpeciesDataAsset.h"

// Simple console-driven test environment for the NPC Needs system.
// Spawns a few resources + NPCs in the current world for manual validation.

DEFINE_LOG_CATEGORY_STATIC(LogVHMNPCConsole, Log, All);

namespace VHMNPCTestEnv
{
    static TArray<TWeakObjectPtr<AActor>> SpawnedActors;

    static bool IsEngineReady()
    {
        return GEngine != nullptr && IsInGameThread();
    }

    static UWorld* GetAnyWorld()
    {
        if (!GEngine) return nullptr;
        const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
        for (const FWorldContext& Ctx : Contexts)
        {
            UWorld* W = Ctx.World();
            if (!W) continue;
            const EWorldType::Type WT = W->WorldType;
            if (WT == EWorldType::PIE || WT == EWorldType::Game || WT == EWorldType::GameRPC || WT == EWorldType::GamePreview)
            {
                return W;
            }
        }
        for (const FWorldContext& Ctx : Contexts)
        {
            UWorld* W = Ctx.World();
            if (!W) continue;
            const EWorldType::Type WT = W->WorldType;
            if (WT == EWorldType::Editor || WT == EWorldType::EditorPreview)
            {
                return W;
            }
        }
        return nullptr;
    }

    static void Track(AActor* Actor)
    {
        if (Actor)
        {
            Actor->Tags.AddUnique(FName(TEXT("VHMTestEnv")));
            SpawnedActors.Add(Actor);
        }
    }

    static FVector FindGroundedLocation(UWorld* World, const FVector& Start, float TraceDistance = 5000.0f)
    {
        if (!World)
        {
            return Start;
        }
        FHitResult Hit;
        const FVector TraceStart = Start + FVector(0, 0, 2000.0f);
        const FVector TraceEnd = Start - FVector(0, 0, TraceDistance);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(VHMNPCTestEnv_FindGround), false);
        if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
        {
            return Hit.ImpactPoint + FVector(0, 0, 5.0f);
        }
        return Start;
    }

    static void TryEnsureNavMesh(UWorld* World, const FVector& Center, const FVector& Extents)
    {
        if (!World) return;

#if VHM_HAS_NAV_BOUNDS
        // If a NavMeshBoundsVolume already exists, do nothing.
        for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
        {
            return;
        }

        // Spawn a basic NavMeshBoundsVolume encompassing the test area. Note: runtime nav generation must be enabled.
        FActorSpawnParameters Params;
        Params.Name = FName(TEXT("VHM_TestEnv_NavBounds"));
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(Center, FRotator::ZeroRotator, Params);
        if (Volume)
        {
            Track(Volume);
            Volume->SetActorScale3D(FVector(Extents.X / 50.0f, Extents.Y / 50.0f, Extents.Z / 50.0f));

            if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
            {
                NavSys->OnNavigationBoundsUpdated(Volume);
            }
            UE_LOG(LogVHMNPCConsole, Log, TEXT("Spawned NavMeshBoundsVolume for test env (runtime nav generation required)."));
        }
        else
        {
            UE_LOG(LogVHMNPCConsole, Warning, TEXT("Failed to spawn NavMeshBoundsVolume; ensure your map has one for pathfinding."));
        }
#else
        UE_LOG(LogVHMNPCConsole, Log, TEXT("NavMeshBoundsVolume header not found; skipping automatic NavBounds spawn. Ensure your map has NavMesh bounds."));
#endif
    }

    static const UVHMSpeciesDataAsset* TryLoadSpecies()
    {
        // Attempt to load a conventional human species asset if it exists.
        const FSoftObjectPath SpeciesPath(TEXT("/Game/NPC/Species/DA_Species_Human.DA_Species_Human"));
        UObject* Obj = SpeciesPath.TryLoad();
        return Cast<UVHMSpeciesDataAsset>(Obj);
    }

    static void SpawnResources(UWorld* World, const FVector& Origin)
    {
        if (!World) return;

        const FVector Offsets[] = {
            FVector(600, 0, 0),   // Food
            FVector(0, 800, 0),   // Water
            FVector(-700, 0, 0)   // Shelter
        };

        // Food (finite, single user)
        {
            const FVector Loc = FindGroundedLocation(World, Origin + Offsets[0]);
            AVHMFoodResourceActor* Food = World->SpawnActor<AVHMFoodResourceActor>(Loc, FRotator::ZeroRotator);
            if (Food)
            {
                Track(Food);
                Food->Tags.AddUnique(FName(TEXT("VHMTestEnv_Food")));
                Food->SetActorLabel(TEXT("VHM_Test_Food"));
            }
        }

        // Water (infinite, many users)
        {
            const FVector Loc = FindGroundedLocation(World, Origin + Offsets[1]);
            AVHMWaterResourceActor* Water = World->SpawnActor<AVHMWaterResourceActor>(Loc, FRotator::ZeroRotator);
            if (Water)
            {
                Track(Water);
                Water->Tags.AddUnique(FName(TEXT("VHMTestEnv_Water")));
                Water->SetActorLabel(TEXT("VHM_Test_Water"));
            }
        }

        // Shelter
        {
            const FVector Loc = FindGroundedLocation(World, Origin + Offsets[2]);
            AVHMShelterResourceActor* Shelter = World->SpawnActor<AVHMShelterResourceActor>(Loc, FRotator::ZeroRotator);
            if (Shelter)
            {
                Track(Shelter);
                Shelter->Tags.AddUnique(FName(TEXT("VHMTestEnv_Shelter")));
                Shelter->SetActorLabel(TEXT("VHM_Test_Shelter"));
            }
        }
    }

    static void SpawnNPCs(UWorld* World, const FVector& Origin, int32 NumNPCs)
    {
        if (!World) return;

        NumNPCs = FMath::Clamp(NumNPCs, 1, 8);
        const UVHMSpeciesDataAsset* Species = TryLoadSpecies();

        const float Radius = 300.0f;
        for (int32 i = 0; i < NumNPCs; ++i)
        {
            const float Angle = (2.0f * PI) * (float(i) / float(NumNPCs));
            const FVector Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;
            const FVector Loc = FindGroundedLocation(World, Origin + Offset);

            AVHMNPCCharacter* NPC = World->SpawnActor<AVHMNPCCharacter>(Loc, FRotator::ZeroRotator);
            if (NPC)
            {
                Track(NPC);
                NPC->Tags.AddUnique(FName(TEXT("VHMTestEnv_NPC")));
                NPC->SetActorLabel(FString::Printf(TEXT("VHM_Test_NPC_%d"), i + 1));

                if (Species)
                {
                    NPC->InitializeNeedsFromSpecies(Species);
                }
            }
        }
    }

    static void SetConvenienceCVars()
    {
        IConsoleManager& CM = IConsoleManager::Get();
        CM.FindConsoleVariable(TEXT("vhm.npc.deterministic"))->Set(1);
        CM.FindConsoleVariable(TEXT("vhm.npc.fast_decay"))->Set(1);
        CM.FindConsoleVariable(TEXT("vhm.npc.show_targets"))->Set(1);
        CM.FindConsoleVariable(TEXT("vhm.npc.search_throttle"))->Set(1.0f);
        CM.FindConsoleVariable(TEXT("vhm.npc.retry_max"))->Set(3);
        UE_LOG(LogVHMNPCConsole, Log, TEXT("Applied test-friendly CVars (deterministic, fast_decay, show_targets)."));
    }

    static void ExecSpawn(const TArray<FString>& Args)
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogVHMNPCConsole, Warning, TEXT("Engine not ready - try after PIE starts."));
            return;
        }

        UWorld* World = GetAnyWorld();
        if (!World)
        {
            UE_LOG(LogVHMNPCConsole, Warning, TEXT("No valid world context found."));
            return;
        }

        int32 NumNPCs = 2;
        if (Args.Num() > 0)
        {
            NumNPCs = FCString::Atoi(*Args[0]);
        }

        const FVector Origin = FVector::ZeroVector;
        TryEnsureNavMesh(World, Origin, FVector(3000, 3000, 1000));
        SpawnResources(World, Origin);
        SpawnNPCs(World, Origin + FVector(0, 0, 30), NumNPCs);
        SetConvenienceCVars();

        UE_LOG(LogVHMNPCConsole, Log, TEXT("Spawned NPC Needs test environment: %d NPC(s) + 3 resources."), NumNPCs);
    }

    static void ExecClear()
    {
        if (!IsEngineReady())
        {
            UE_LOG(LogVHMNPCConsole, Warning, TEXT("Engine not ready - try after PIE starts."));
            return;
        }
        UWorld* World = GetAnyWorld();
        if (!World)
        {
            UE_LOG(LogVHMNPCConsole, Warning, TEXT("No valid world context found."));
            return;
        }

        int32 Removed = 0;
        for (int32 i = SpawnedActors.Num() - 1; i >= 0; --i)
        {
            if (AActor* A = SpawnedActors[i].Get())
            {
                if (IsValid(A))
                {
                    A->Destroy();
                    Removed++;
                }
            }
            SpawnedActors.RemoveAt(i);
        }

        // Also remove any actors tagged from a previous session (if not tracked)
        TArray<AActor*> Tagged;
        UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("VHMTestEnv")), Tagged);
        for (AActor* A : Tagged)
        {
            if (IsValid(A))
            {
                A->Destroy();
                Removed++;
            }
        }

        UE_LOG(LogVHMNPCConsole, Log, TEXT("Cleared NPC Needs test environment (%d actors destroyed)."), Removed);
    }
}

// Console commands
static FAutoConsoleCommand GCmdVHMNPCTestSpawn(
    TEXT("vhm.npc.test_env.spawn"),
    TEXT("Spawn NPC Needs test environment. Usage: vhm.npc.test_env.spawn [NumNPCs]"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&VHMNPCTestEnv::ExecSpawn)
);

static FAutoConsoleCommand GCmdVHMNPCTestClear(
    TEXT("vhm.npc.test_env.clear"),
    TEXT("Clear NPC Needs test environment actors."),
    FConsoleCommandDelegate::CreateStatic(&VHMNPCTestEnv::ExecClear)
);
