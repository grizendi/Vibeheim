#include "WorldGenManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogWorldGenManager);

AWorldGenManager::AWorldGenManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    
    // Initialize default values
    bIsInitialized = false;
    bIsReady = false;
    bAutoTrackPlayer = true;
    PlayerTrackingInterval = 1.0f;
    PlayerTrackingTimer = 0.0f;
    bEnableDebugLogging = false;
    DebugLoggingInterval = 10.0f;
    DebugLoggingTimer = 0.0f;
    
    // Create subsystem components
    ConfigManager = CreateDefaultSubobject<UWorldGenConfigManager>(TEXT("ConfigManager"));
    VoxelPluginAdapter = CreateDefaultSubobject<UVoxelPluginAdapter>(TEXT("VoxelPluginAdapter"));
    ChunkStreamingManager = CreateDefaultSubobject<UChunkStreamingManager>(TEXT("ChunkStreamingManager"));
    
    // Create biome system
    BiomeSystem = MakeUnique<FBiomeSystem>();
    
    // Create POI system
    POISystem = MakeUnique<FPOISystem>();
    
    // Create noise generator
    NoiseGenerator = MakeUnique<FNoiseGenerator>();
}

void AWorldGenManager::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogWorldGenManager, Log, TEXT("WorldGenManager BeginPlay - Starting initialization"));
    
    // Initialize the world generation system
    if (!InitializeWorldGeneration())
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("Failed to initialize world generation system"));
        return;
    }
    
    // Auto-set player anchor if enabled
    if (bAutoTrackPlayer)
    {
        AutoSetPlayerAnchor();
    }
    
    UE_LOG(LogWorldGenManager, Log, TEXT("WorldGenManager initialization completed successfully"));
}

void AWorldGenManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!bIsReady)
    {
        return;
    }
    
    // Update player anchor tracking
    UpdatePlayerAnchorTracking(DeltaTime);
    
    // Update streaming
    UpdateStreaming(DeltaTime);
    
    // Debug logging
    if (bEnableDebugLogging)
    {
        DebugLoggingTimer += DeltaTime;
        if (DebugLoggingTimer >= DebugLoggingInterval)
        {
            LogSystemStatus();
            DebugLoggingTimer = 0.0f;
        }
    }
}

void AWorldGenManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UE_LOG(LogWorldGenManager, Log, TEXT("WorldGenManager EndPlay - Shutting down"));
    
    // Clean up systems
    bIsReady = false;
    bIsInitialized = false;
    PlayerAnchor = nullptr;
    
    Super::EndPlay(EndPlayReason);
}

bool AWorldGenManager::InitializeWorldGeneration(const FWorldGenSettings& CustomSettings)
{
    if (bIsInitialized)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("World generation already initialized"));
        return true;
    }
    
    UE_LOG(LogWorldGenManager, Log, TEXT("Initializing world generation system..."));
    
    // Load configuration
    if (!LoadConfiguration())
    {
        HandleInitializationFailure(TEXT("ConfigManager"), TEXT("Failed to load world generation configuration"));
        return false;
    }
    
    // Use custom settings if provided, otherwise use loaded config
    FWorldGenSettings SettingsToUse = CustomSettings.Seed != 1337 ? CustomSettings : ConfigManager->GetSettings();
    
    // Initialize subsystems
    if (!InitializeSubsystems())
    {
        HandleInitializationFailure(TEXT("Subsystems"), TEXT("Failed to initialize world generation subsystems"));
        return false;
    }
    
    // Initialize voxel plugin adapter
    if (!VoxelPluginAdapter->Initialize(SettingsToUse))
    {
        HandleInitializationFailure(TEXT("VoxelPluginAdapter"), TEXT("Failed to initialize voxel plugin adapter"));
        return false;
    }
    
    // Initialize noise generator
    NoiseGenerator->Initialize(SettingsToUse);
    UE_LOG(LogWorldGenManager, Log, TEXT("NoiseGenerator initialized successfully"));
    
    // Initialize biome system
    BiomeSystem->Initialize(SettingsToUse);
    UE_LOG(LogWorldGenManager, Log, TEXT("BiomeSystem initialized successfully"));
    
    // Initialize POI system
    POISystem->Initialize(SettingsToUse, NoiseGenerator.Get(), BiomeSystem.Get());
    UE_LOG(LogWorldGenManager, Log, TEXT("POI System initialized successfully"));
    
    // Initialize chunk streaming manager
    if (!ChunkStreamingManager->Initialize(SettingsToUse, VoxelPluginAdapter))
    {
        HandleInitializationFailure(TEXT("ChunkStreamingManager"), TEXT("Failed to initialize chunk streaming manager"));
        return false;
    }
    
    bIsInitialized = true;
    bIsReady = true;
    
    UE_LOG(LogWorldGenManager, Log, TEXT("World generation system initialized successfully with seed: %lld"), SettingsToUse.Seed);
    
    return true;
}

void AWorldGenManager::SetPlayerAnchor(AActor* NewPlayerAnchor)
{
    if (PlayerAnchor != NewPlayerAnchor)
    {
        PlayerAnchor = NewPlayerAnchor;
        
        if (PlayerAnchor)
        {
            UE_LOG(LogWorldGenManager, Log, TEXT("Player anchor set to: %s"), *PlayerAnchor->GetName());
            
            // Update streaming manager
            if (ChunkStreamingManager)
            {
                ChunkStreamingManager->SetPlayerAnchor(PlayerAnchor);
            }
            
            // Start world building around new anchor
            if (VoxelPluginAdapter && bIsReady)
            {
                VoxelPluginAdapter->BuildWorldAsync(PlayerAnchor);
            }
        }
        else
        {
            UE_LOG(LogWorldGenManager, Log, TEXT("Player anchor cleared"));
        }
    }
}

bool AWorldGenManager::RebuildChunk(const FIntVector& ChunkCoordinate)
{
    if (!bIsReady || !VoxelPluginAdapter)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot rebuild chunk - world generation not ready"));
        return false;
    }
    
    UE_LOG(LogWorldGenManager, Log, TEXT("Rebuilding chunk at coordinate: %s"), *ChunkCoordinate.ToString());
    
    bool bSuccess = VoxelPluginAdapter->RebuildChunkAsync(ChunkCoordinate);
    if (!bSuccess)
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("Failed to queue chunk rebuild for coordinate: %s"), *ChunkCoordinate.ToString());
    }
    
    return bSuccess;
}

const FWorldGenSettings& AWorldGenManager::GetWorldGenSettings() const
{
    if (ConfigManager)
    {
        return ConfigManager->GetSettings();
    }
    
    // Return a static default if config manager is not available
    static FWorldGenSettings DefaultSettings;
    return DefaultSettings;
}

bool AWorldGenManager::UpdateWorldGenSettings(const FWorldGenSettings& NewSettings)
{
    if (!ConfigManager)
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("Cannot update settings - ConfigManager not available"));
        return false;
    }
    
    if (!ConfigManager->UpdateSettings(NewSettings))
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("Failed to validate and update world generation settings"));
        return false;
    }
    
    UE_LOG(LogWorldGenManager, Log, TEXT("World generation settings updated successfully"));
    
    // If systems are already initialized, we may need to reinitialize with new settings
    if (bIsInitialized)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Settings updated after initialization - some changes may require restart"));
    }
    
    return true;
}

void AWorldGenManager::GetStreamingStats(int32& OutLoadedChunks, int32& OutGeneratingChunks, 
                                        float& OutAverageGenerationTime, float& OutP95GenerationTime) const
{
    if (ChunkStreamingManager)
    {
        ChunkStreamingManager->GetStreamingStats(OutLoadedChunks, OutGeneratingChunks, OutAverageGenerationTime, OutP95GenerationTime);
    }
    else
    {
        OutLoadedChunks = OutGeneratingChunks = 0;
        OutAverageGenerationTime = OutP95GenerationTime = 0.0f;
    }
}

FBiomeEvaluation AWorldGenManager::EvaluateBiomeAtLocation(const FVector& WorldLocation) const
{
    if (BiomeSystem.IsValid())
    {
        return BiomeSystem->EvaluateBiome(WorldLocation.X, WorldLocation.Y);
    }
    
    // Return default evaluation if biome system is not available
    return FBiomeEvaluation();
}

bool AWorldGenManager::AutoSetPlayerAnchor()
{
    // Try to find the first player controller and its pawn
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PlayerController && PlayerController->GetPawn())
    {
        SetPlayerAnchor(PlayerController->GetPawn());
        return true;
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("Could not auto-set player anchor - no player pawn found"));
    return false;
}

bool AWorldGenManager::InitializeSubsystems()
{
    // Validate that all required components are available
    if (!ConfigManager)
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("ConfigManager component is null"));
        return false;
    }
    
    if (!VoxelPluginAdapter)
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("VoxelPluginAdapter component is null"));
        return false;
    }
    
    if (!ChunkStreamingManager)
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("ChunkStreamingManager component is null"));
        return false;
    }
    
    if (!BiomeSystem.IsValid())
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("BiomeSystem is not valid"));
        return false;
    }
    
    // Check if VoxelPluginLegacy is available
    if (!UVoxelPluginAdapter::IsVoxelPluginAvailable())
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("VoxelPluginLegacy is not available or not properly loaded"));
        return false;
    }
    
    UE_LOG(LogWorldGenManager, Log, TEXT("All subsystems validated successfully"));
    return true;
}

bool AWorldGenManager::LoadConfiguration()
{
    if (!ConfigManager)
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("ConfigManager is null - cannot load configuration"));
        return false;
    }
    
    // Try to load default configuration
    if (!ConfigManager->LoadDefaultConfiguration())
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Failed to load default configuration - using defaults"));
        ConfigManager->ResetToDefaults();
    }
    
    // Validate loaded configuration
    if (!ConfigManager->IsConfigurationValid())
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("Loaded configuration is invalid"));
        return false;
    }
    
    const FWorldGenSettings& Settings = ConfigManager->GetSettings();
    UE_LOG(LogWorldGenManager, Log, TEXT("Configuration loaded successfully - Seed: %lld, Version: %d"), 
           Settings.Seed, Settings.WorldGenVersion);
    
    return true;
}

void AWorldGenManager::HandleInitializationFailure(const FString& FailedSystem, const FString& ErrorMessage)
{
    UE_LOG(LogWorldGenManager, Error, TEXT("Initialization failure in %s: %s"), *FailedSystem, *ErrorMessage);
    
    // Log current seed and any relevant context for debugging
    if (ConfigManager && ConfigManager->IsConfigurationValid())
    {
        const FWorldGenSettings& Settings = ConfigManager->GetSettings();
        UE_LOG(LogWorldGenManager, Error, TEXT("Failure context - Seed: %lld, Version: %d"), 
               Settings.Seed, Settings.WorldGenVersion);
    }
    
    // Set flags to indicate failure
    bIsInitialized = false;
    bIsReady = false;
    
    // In a production environment, you might want to:
    // 1. Show an error message to the user
    // 2. Attempt fallback generation
    // 3. Disable world generation features gracefully
    
    UE_LOG(LogWorldGenManager, Error, TEXT("World generation system is not available due to initialization failure"));
}

void AWorldGenManager::UpdatePlayerAnchorTracking(float DeltaTime)
{
    if (!bAutoTrackPlayer)
    {
        return;
    }
    
    PlayerTrackingTimer += DeltaTime;
    if (PlayerTrackingTimer >= PlayerTrackingInterval)
    {
        // Check if we need to update the player anchor
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PlayerController && PlayerController->GetPawn())
        {
            AActor* CurrentPlayerPawn = PlayerController->GetPawn();
            if (PlayerAnchor != CurrentPlayerPawn)
            {
                SetPlayerAnchor(CurrentPlayerPawn);
            }
        }
        else if (PlayerAnchor != nullptr)
        {
            // Player pawn is no longer available
            UE_LOG(LogWorldGenManager, Warning, TEXT("Player pawn no longer available - clearing anchor"));
            SetPlayerAnchor(nullptr);
        }
        
        PlayerTrackingTimer = 0.0f;
    }
}

void AWorldGenManager::UpdateStreaming(float DeltaTime)
{
    // Update voxel plugin adapter streaming
    if (VoxelPluginAdapter)
    {
        VoxelPluginAdapter->TickStreaming(DeltaTime);
    }
    
    // Update chunk streaming manager
    if (ChunkStreamingManager)
    {
        ChunkStreamingManager->UpdateStreaming(DeltaTime);
    }
}

void AWorldGenManager::LogSystemStatus() const
{
    if (!bEnableDebugLogging)
    {
        return;
    }
    
    UE_LOG(LogWorldGenManager, Log, TEXT("=== WorldGenManager Status ==="));
    UE_LOG(LogWorldGenManager, Log, TEXT("Initialized: %s, Ready: %s"), 
           bIsInitialized ? TEXT("Yes") : TEXT("No"),
           bIsReady ? TEXT("Yes") : TEXT("No"));
    
    if (PlayerAnchor)
    {
        FVector PlayerLocation = PlayerAnchor->GetActorLocation();
        UE_LOG(LogWorldGenManager, Log, TEXT("Player Anchor: %s at %s"), 
               *PlayerAnchor->GetName(), *PlayerLocation.ToString());
    }
    else
    {
        UE_LOG(LogWorldGenManager, Log, TEXT("Player Anchor: None"));
    }
    
    // Log streaming stats
    int32 LoadedChunks, GeneratingChunks;
    float AvgTime, P95Time;
    GetStreamingStats(LoadedChunks, GeneratingChunks, AvgTime, P95Time);
    
    UE_LOG(LogWorldGenManager, Log, TEXT("Streaming Stats - Loaded: %d, Generating: %d, Avg: %.2fms, P95: %.2fms"),
           LoadedChunks, GeneratingChunks, AvgTime, P95Time);
    
    UE_LOG(LogWorldGenManager, Log, TEXT("=== End Status ==="));
}

TArray<FPOIPlacementResult> AWorldGenManager::GeneratePOIsForChunk(const FIntVector& ChunkCoordinate)
{
    if (!bIsReady || !POISystem.IsValid())
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot generate POIs - world generation not ready or POI system not available"));
        return TArray<FPOIPlacementResult>();
    }
    
    return POISystem->GeneratePOIsForChunk(ChunkCoordinate, GetWorld());
}

TArray<FPOIInstance> AWorldGenManager::GetPOIsInChunk(const FIntVector& ChunkCoordinate) const
{
    if (POISystem.IsValid())
    {
        return POISystem->GetPOIsInChunk(ChunkCoordinate);
    }
    
    return TArray<FPOIInstance>();
}

TArray<FPOIInstance> AWorldGenManager::GetAllActivePOIs() const
{
    if (POISystem.IsValid())
    {
        return POISystem->GetAllActivePOIs();
    }
    
    return TArray<FPOIInstance>();
}

void AWorldGenManager::AddPOISpawnRule(const FPOISpawnRule& SpawnRule)
{
    if (POISystem.IsValid())
    {
        POISystem->AddPOISpawnRule(SpawnRule);
        UE_LOG(LogWorldGenManager, Log, TEXT("Added POI spawn rule: %s"), *SpawnRule.POITypeName);
    }
    else
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot add POI spawn rule - POI system not available"));
    }
}

bool AWorldGenManager::RemovePOISpawnRule(const FString& POITypeName)
{
    if (POISystem.IsValid())
    {
        bool bRemoved = POISystem->RemovePOISpawnRule(POITypeName);
        if (bRemoved)
        {
            UE_LOG(LogWorldGenManager, Log, TEXT("Removed POI spawn rule: %s"), *POITypeName);
        }
        return bRemoved;
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot remove POI spawn rule - POI system not available"));
    return false;
}

void AWorldGenManager::GetPOIPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                                           int32& OutFailedPlacements, float& OutAverageAttemptsPerPOI) const
{
    if (POISystem.IsValid())
    {
        POISystem->GetPlacementStats(OutTotalAttempts, OutSuccessfulPlacements, OutFailedPlacements, OutAverageAttemptsPerPOI);
    }
    else
    {
        OutTotalAttempts = OutSuccessfulPlacements = OutFailedPlacements = 0;
        OutAverageAttemptsPerPOI = 0.0f;
    }
}