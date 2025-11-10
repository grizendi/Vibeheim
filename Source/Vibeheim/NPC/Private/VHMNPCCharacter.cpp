#include "VHMNPCCharacter.h"

#include "AIController.h"
#include "Components/ActorComponent.h"
#if __has_include("Navigation/NavigationQueryFilter.h")
#include "Navigation/NavigationQueryFilter.h"
#elif __has_include("NavFilters/NavigationQueryFilter.h")
#include "NavFilters/NavigationQueryFilter.h"
#endif
#if __has_include("StateTree/StateTreeComponent.h")
#include "StateTree/StateTreeComponent.h"
#include "StateTree/StateTree.h"
#define VHM_HAS_STATETREE 1
#elif __has_include("StateTreeComponent.h")
#include "StateTreeComponent.h"
#include "StateTree.h"
#define VHM_HAS_STATETREE 1
#else
#define VHM_HAS_STATETREE 0
#endif
#include "VHMNeedsComponent.h"
#include "VHMNPCLogging.h"
#include "VHMNPCAIController.h"
#include "VHMSpeciesDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/ConstructorHelpers.h"

AVHMNPCCharacter::AVHMNPCCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;

    NeedsComponent = CreateDefaultSubobject<UVHMNeedsComponent>(TEXT("NeedsComponent"));
#if VHM_HAS_STATETREE
    StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));
#else
    StateTreeComponent = CreateDefaultSubobject<UActorComponent>(TEXT("StateTreeComponent"));
#endif

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AVHMNPCAIController::StaticClass();

    // Visual-only fallback so spawned NPCs are visible in test scenes without skeletal assets.
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    VisualMesh->SetupAttachment(GetCapsuleComponent());
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VisualMesh->SetGenerateOverlapEvents(false);
    VisualMesh->SetCastShadow(false);
    VisualMesh->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
    VisualMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.6f));

    // Default to Engine basic shape if no override is provided
    if (!DefaultVisualMesh.IsValid())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMeshObj(TEXT("/Engine/BasicShapes/Capsule.Capsule"));
        if (DefaultMeshObj.Succeeded())
        {
            VisualMesh->SetStaticMesh(DefaultMeshObj.Object);
        }
    }
}

void AVHMNPCCharacter::BeginPlay()
{
    Super::BeginPlay();

    InitializeNeedsFromSpecies(ResolveSpeciesData());
    ApplySpeciesNavigationFilter(GetController());

#if VHM_HAS_STATETREE
    // If a StateTree component exists, ensure it has an asset and is running.
    if (UStateTreeComponent* STComp = Cast<UStateTreeComponent>(StateTreeComponent))
    {
        UStateTree* AssignedTree = nullptr;

        // Prefer an explicitly set asset on this instance.
        if (DefaultStateTreeAsset.IsValid())
        {
            AssignedTree = DefaultStateTreeAsset.Get();
        }
        else if (DefaultStateTreeAsset.ToSoftObjectPath().IsValid())
        {
            AssignedTree = DefaultStateTreeAsset.LoadSynchronous();
        }

        // Fallback: try a conventional asset path if none was provided.
        if (AssignedTree == nullptr)
        {
            const FSoftObjectPath FallbackPath(TEXT("/Game/NPC/StateTrees/ST_NPC_Needs.ST_NPC_Needs"));
            if (FallbackPath.IsValid())
            {
                AssignedTree = Cast<UStateTree>(FallbackPath.TryLoad());
            }
        }

        if (AssignedTree)
        {
            STComp->SetStateTree(AssignedTree);
            STComp->StartLogic();
            UE_LOG(LogVHMStateTree, Verbose, TEXT("Started StateTree '%s' on NPC '%s'."), *AssignedTree->GetName(), *GetName());
        }
        else
        {
            UE_LOG(LogVHMStateTree, Warning, TEXT("No StateTree asset assigned or found for NPC '%s'. StateTree will not run."), *GetName());
        }
    }
#endif // VHM_HAS_STATETREE

    // Hide visual fallback if a real skeletal mesh is assigned or if an override exists and is loaded
    if (VisualMesh)
    {
        bool bUseFallback = true;

        // Use explicit override mesh if set
        if (DefaultVisualMesh.IsValid())
        {
            UStaticMesh* MeshAsset = DefaultVisualMesh.Get();
            if (!MeshAsset && DefaultVisualMesh.ToSoftObjectPath().IsValid())
            {
                MeshAsset = DefaultVisualMesh.LoadSynchronous();
            }
            if (MeshAsset)
            {
                VisualMesh->SetStaticMesh(MeshAsset);
            }
        }

        // If a skeletal mesh exists on the Character's Mesh component, prefer that and hide fallback
        if (GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr)
        {
            bUseFallback = false;
        }

        VisualMesh->SetHiddenInGame(!bUseFallback);
    }
}

void AVHMNPCCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    ApplySpeciesNavigationFilter(NewController);
}

void AVHMNPCCharacter::InitializeNeedsFromSpecies(const UVHMSpeciesDataAsset* InSpecies)
{
    if (!NeedsComponent)
    {
        UE_LOG(LogVHMNPC, Error, TEXT("AVHMNPCCharacter missing NeedsComponent on %s"), *GetName());
        return;
    }

    NeedsComponent->InitializeFromSpecies(InSpecies);

    if (!InSpecies && !bLoggedMissingSpeciesWarning)
    {
        UE_LOG(LogVHMNPC, Warning, TEXT("AVHMNPCCharacter %s initialized without species data; using defaults."), *GetName());
        bLoggedMissingSpeciesWarning = true;
    }
    else if (InSpecies)
    {
        bLoggedMissingSpeciesWarning = false;
    }

    ApplySpeciesNavigationFilter(GetController());
}

void AVHMNPCCharacter::ApplySpeciesNavigationFilter(AController* InController)
{
    if (!InController)
    {
        return;
    }

    const UVHMSpeciesDataAsset* Species = ResolveSpeciesData();
    if (!Species)
    {
        return;
    }

    const UClass* FilterClass = Species->Config.NavQueryFilter.Get();
    if (FilterClass == nullptr)
    {
        return;
    }

    if (AVHMNPCAIController* NPCController = Cast<AVHMNPCAIController>(InController))
    {
        NPCController->SetSpeciesNavigationFilter(Species->Config.NavQueryFilter);
    }
}

const UVHMSpeciesDataAsset* AVHMNPCCharacter::ResolveSpeciesData() const
{
    if (NeedsComponent && NeedsComponent->SpeciesData)
    {
        return NeedsComponent->SpeciesData;
    }

    return DefaultSpeciesData;
}

