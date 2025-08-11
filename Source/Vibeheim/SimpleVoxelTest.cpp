#include "SimpleVoxelTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Modules/ModuleManager.h"

ASimpleVoxelTest::ASimpleVoxelTest()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create a simple mesh component
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
}

void ASimpleVoxelTest::BeginPlay()
{
    Super::BeginPlay();
    
    // Test on begin play
    TestVoxelPlugin();
}

void ASimpleVoxelTest::TestVoxelPlugin()
{
    // Simple test - check if Voxel module is loaded
    bool bVoxelModuleLoaded = FModuleManager::Get().IsModuleLoaded("Voxel");
    
    FString TestMessage = FString::Printf(TEXT("Voxel Plugin Test - Module Loaded: %s"), 
                                         bVoxelModuleLoaded ? TEXT("YES") : TEXT("NO"));
    
    // Print to screen and log
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TestMessage);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("%s"), *TestMessage);
    
    // Try to load the module if not loaded
    if (!bVoxelModuleLoaded)
    {
        FModuleManager::Get().LoadModule("Voxel");
        bool bLoadedNow = FModuleManager::Get().IsModuleLoaded("Voxel");
        
        FString LoadMessage = FString::Printf(TEXT("Attempted to load Voxel module: %s"), 
                                            bLoadedNow ? TEXT("SUCCESS") : TEXT("FAILED"));
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, LoadMessage);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("%s"), *LoadMessage);
    }
}