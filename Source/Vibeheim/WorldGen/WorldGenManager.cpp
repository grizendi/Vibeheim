#include "WorldGenManager.h"
#include "WorldGenConsoleCommands.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#endif

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
    
    // Create dungeon portal system
    DungeonPortalSystem = MakeUnique<FDungeonPortalSystem>();
    
    // Create noise generator
    NoiseGenerator = MakeUnique<FNoiseGenerator>();
}

AWorldGenManager::~AWorldGenManager()
{
    UE_LOG(LogWorldGenManager, Warning, TEXT("WorldGenManager destructor - ensuring cleanup"));
    
    // Ensure cleanup happens even if EndPlay wasn't called
    if (bIsInitialized || bIsReady)
    {
        // Shutdown chunk streaming manager first to wait for async tasks
        if (ChunkStreamingManager)
        {
            ChunkStreamingManager->Shutdown();
        }
        
        // Shutdown voxel plugin adapter
        if (VoxelPluginAdapter)
        {
            VoxelPluginAdapter->Shutdown();
        }
        
        bIsReady = false;
        bIsInitialized = false;
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("WorldGenManager destructor complete"));
}

void AWorldGenManager::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogWorldGenManager, Log, TEXT("WorldGenManager BeginPlay - Starting initialization"));
    
    // Register console commands
    FWorldGenConsoleCommands::RegisterCommands();
    
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
    
    // Register for PIE end delegate to ensure cleanup
#if WITH_EDITOR
    if (GEditor)
    {
        FEditorDelegates::EndPIE.AddUObject(this, &AWorldGenManager::OnPIEEnded);
    }
#endif
    
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
    UE_LOG(LogWorldGenManager, Warning, TEXT("WorldGenManager EndPlay - Shutting down (Reason: %d)"), (int32)EndPlayReason);
    
    // Shutdown chunk streaming manager first to wait for async tasks
    if (ChunkStreamingManager)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Shutting down ChunkStreamingManager"));
        ChunkStreamingManager->Shutdown();
        UE_LOG(LogWorldGenManager, Warning, TEXT("ChunkStreamingManager shutdown complete"));
    }
    else
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("ChunkStreamingManager is null - no shutdown needed"));
    }
    
    // Shutdown voxel plugin adapter
    if (VoxelPluginAdapter)
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Shutting down VoxelPluginAdapter"));
        VoxelPluginAdapter->Shutdown();
        UE_LOG(LogWorldGenManager, Warning, TEXT("VoxelPluginAdapter shutdown complete"));
    }
    else
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("VoxelPluginAdapter is null - no shutdown needed"));
    }
    
    // Unregister console commands
    FWorldGenConsoleCommands::UnregisterCommands();
    
    // Unregister PIE delegate
#if WITH_EDITOR
    if (GEditor)
    {
        FEditorDelegates::EndPIE.RemoveAll(this);
    }
#endif
    
    // Clean up systems
    bIsReady = false;
    bIsInitialized = false;
    PlayerAnchor = nullptr;
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("WorldGenManager EndPlay complete"));
    
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
        HandleInitializationFailure(TEXT("VoxelPluginAdapter"), 
                                   FString::Printf(TEXT("Failed to initialize voxel plugin adapter with seed %lld"), SettingsToUse.Seed));
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
    
    // Initialize dungeon portal system
    DungeonPortalSystem->Initialize(SettingsToUse, NoiseGenerator.Get(), BiomeSystem.Get(), POISystem.Get());
    UE_LOG(LogWorldGenManager, Log, TEXT("Dungeon Portal System initialized successfully"));
    
    // Initialize chunk streaming manager
    if (!ChunkStreamingManager->Initialize(SettingsToUse, VoxelPluginAdapter))
    {
        HandleInitializationFailure(TEXT("ChunkStreamingManager"), 
                                   FString::Printf(TEXT("Failed to initialize chunk streaming manager with seed %lld"), SettingsToUse.Seed));
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
        LogStructuredError(TEXT("Cannot rebuild chunk - world generation not ready"), 
                          FString::Printf(TEXT("Chunk: (%d, %d, %d), Ready: %s, AdapterAvailable: %s"), 
                                        ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z,
                                        bIsReady ? TEXT("Yes") : TEXT("No"),
                                        VoxelPluginAdapter ? TEXT("Yes") : TEXT("No")));
        return false;
    }
    
    UE_LOG(LogWorldGenManager, VeryVerbose, TEXT("Rebuilding chunk - Chunk: (%d, %d, %d)"), 
           ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z);
    
    bool bSuccess = VoxelPluginAdapter->RebuildChunkAsync(ChunkCoordinate);
    if (!bSuccess)
    {
        LogStructuredError(TEXT("Failed to queue chunk rebuild"), 
                          FString::Printf(TEXT("Chunk: (%d, %d, %d)"), ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z));
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
    // Log structured error with seed and context
    if (ConfigManager && ConfigManager->IsConfigurationValid())
    {
        const FWorldGenSettings& Settings = ConfigManager->GetSettings();
        LogStructuredError(FString::Printf(TEXT("Initialization failure in %s: %s"), *FailedSystem, *ErrorMessage),
                          FString::Printf(TEXT("Seed: %lld, Version: %d"), Settings.Seed, Settings.WorldGenVersion));
    }
    else
    {
        LogStructuredError(FString::Printf(TEXT("Initialization failure in %s: %s"), *FailedSystem, *ErrorMessage),
                          TEXT("ConfigManager not available or invalid"));
    }
    
    // Attempt graceful degradation
    if (!AttemptGracefulDegradation(FailedSystem, ErrorMessage))
    {
        // Complete failure - set flags to indicate failure
        bIsInitialized = false;
        bIsReady = false;
        
        UE_LOG(LogWorldGenManager, Error, TEXT("World generation system is not available due to initialization failure - no graceful degradation possible"));
    }
}

bool AWorldGenManager::AttemptGracefulDegradation(const FString& FailedSystem, const FString& ErrorMessage)
{
    UE_LOG(LogWorldGenManager, Warning, TEXT("Attempting graceful degradation for failed system: %s"), *FailedSystem);
    
    // Get current seed for logging context
    int64 CurrentSeed = 1337;
    if (ConfigManager && ConfigManager->IsConfigurationValid())
    {
        CurrentSeed = ConfigManager->GetSettings().Seed;
    }
    
    // Handle specific system failures with fallback options
    if (FailedSystem == TEXT("VoxelPluginAdapter"))
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("VoxelPluginAdapter failed - attempting to continue without voxel features - Seed: %lld"), CurrentSeed);
        
        // Disable voxel-dependent features but allow other systems to continue
        VoxelPluginAdapter = nullptr;
        
        // We can still provide biome evaluation, POI placement logic, etc.
        bIsInitialized = true;
        bIsReady = false; // Not fully ready but partially functional
        
        return true;
    }
    else if (FailedSystem == TEXT("ChunkStreamingManager"))
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("ChunkStreamingManager failed - world generation will work without streaming optimization - Seed: %lld"), CurrentSeed);
        
        // Disable streaming but allow basic world generation
        ChunkStreamingManager = nullptr;
        
        bIsInitialized = true;
        bIsReady = true; // Can still generate world, just without streaming
        
        return true;
    }
    else if (FailedSystem == TEXT("ConfigManager"))
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("ConfigManager failed - using default settings - Seed: %lld"), CurrentSeed);
        
        // Create a default config manager with fallback settings
        ConfigManager = CreateDefaultSubobject<UWorldGenConfigManager>(TEXT("FallbackConfigManager"));
        ConfigManager->ResetToDefaults();
        
        return true; // Can continue with defaults
    }
    else if (FailedSystem == TEXT("Subsystems"))
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Some subsystems failed - attempting partial initialization - Seed: %lld"), CurrentSeed);
        
        // Try to initialize what we can
        bool bPartialSuccess = false;
        
        if (BiomeSystem.IsValid())
        {
            UE_LOG(LogWorldGenManager, Log, TEXT("BiomeSystem available for fallback mode - Seed: %lld"), CurrentSeed);
            bPartialSuccess = true;
        }
        
        if (NoiseGenerator.IsValid())
        {
            UE_LOG(LogWorldGenManager, Log, TEXT("NoiseGenerator available for fallback mode - Seed: %lld"), CurrentSeed);
            bPartialSuccess = true;
        }
        
        if (bPartialSuccess)
        {
            bIsInitialized = true;
            bIsReady = false; // Partial functionality only
            return true;
        }
    }
    
    UE_LOG(LogWorldGenManager, Error, TEXT("No graceful degradation available for failed system: %s - Seed: %lld"), *FailedSystem, CurrentSeed);
    return false;
}

void AWorldGenManager::LogStructuredError(const FString& ErrorMessage, const FString& AdditionalContext) const
{
    // Get current seed for context
    int64 CurrentSeed = 1337;
    if (ConfigManager && ConfigManager->IsConfigurationValid())
    {
        CurrentSeed = ConfigManager->GetSettings().Seed;
    }
    
    if (AdditionalContext.IsEmpty())
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("[STRUCTURED_ERROR] %s - Seed: %lld"), *ErrorMessage, CurrentSeed);
    }
    else
    {
        UE_LOG(LogWorldGenManager, Error, TEXT("[STRUCTURED_ERROR] %s - Seed: %lld, Context: %s"), *ErrorMessage, CurrentSeed, *AdditionalContext);
    }
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
    
    // Generate POIs first
    TArray<FPOIPlacementResult> POIResults = POISystem->GeneratePOIsForChunk(ChunkCoordinate, GetWorld());
    
    // Generate dungeon portals for the same chunk
    if (DungeonPortalSystem.IsValid())
    {
        TArray<FPortalPlacementResult> PortalResults = DungeonPortalSystem->GeneratePortalsForChunk(ChunkCoordinate, GetWorld());
        UE_LOG(LogWorldGenManager, Log, TEXT("Generated %d portals for chunk %s"), PortalResults.Num(), *ChunkCoordinate.ToString());
    }
    
    return POIResults;
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

TArray<FPortalPlacementResult> AWorldGenManager::GeneratePortalsForChunk(const FIntVector& ChunkCoordinate)
{
    if (DungeonPortalSystem.IsValid())
    {
        return DungeonPortalSystem->GeneratePortalsForChunk(ChunkCoordinate, GetWorld());
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot generate portals - Dungeon Portal system not available"));
    return TArray<FPortalPlacementResult>();
}

TArray<FDungeonPortal> AWorldGenManager::GetPortalsInChunk(const FIntVector& ChunkCoordinate) const
{
    if (DungeonPortalSystem.IsValid())
    {
        return DungeonPortalSystem->GetPortalsInChunk(ChunkCoordinate);
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot get portals in chunk - Dungeon Portal system not available"));
    return TArray<FDungeonPortal>();
}

TArray<FDungeonPortal> AWorldGenManager::GetAllActivePortals() const
{
    if (DungeonPortalSystem.IsValid())
    {
        return DungeonPortalSystem->GetAllActivePortals();
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot get active portals - Dungeon Portal system not available"));
    return TArray<FDungeonPortal>();
}

void AWorldGenManager::AddPortalSpawnRule(const FPortalSpawnRule& SpawnRule)
{
    if (DungeonPortalSystem.IsValid())
    {
        DungeonPortalSystem->AddPortalSpawnRule(SpawnRule);
    }
    else
    {
        UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot add portal spawn rule - Dungeon Portal system not available"));
    }
}

bool AWorldGenManager::RemovePortalSpawnRule(const FString& PortalTypeName)
{
    if (DungeonPortalSystem.IsValid())
    {
        return DungeonPortalSystem->RemovePortalSpawnRule(PortalTypeName);
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("Cannot remove portal spawn rule - Dungeon Portal system not available"));
    return false;
}

void AWorldGenManager::GetPortalPlacementStats(int32& OutTotalAttempts, int32& OutSuccessfulPlacements, 
                                              int32& OutFailedPlacements, float& OutAverageAttemptsPerPortal) const
{
    if (DungeonPortalSystem.IsValid())
    {
        DungeonPortalSystem->GetPlacementStats(OutTotalAttempts, OutSuccessfulPlacements, OutFailedPlacements, OutAverageAttemptsPerPortal);
    }
    else
    {
        OutTotalAttempts = OutSuccessfulPlacements = OutFailedPlacements = 0;
        OutAverageAttemptsPerPortal = 0.0f;
    }
}

void AWorldGenManager::OnPIEEnded(bool bIsSimulating)
{
    UE_LOG(LogWorldGenManager, Warning, TEXT("PIE ended - forcing immediate shutdown"));
    
    // Force immediate shutdown when PIE ends
    if (bIsInitialized || bIsReady)
    {
        // Shutdown chunk streaming manager first to wait for async tasks
        if (ChunkStreamingManager)
        {
            ChunkStreamingManager->Shutdown();
        }
        
        // Shutdown voxel plugin adapter
        if (VoxelPluginAdapter)
        {
            VoxelPluginAdapter->Shutdown();
        }
        
        bIsReady = false;
        bIsInitialized = false;
    }
    
    UE_LOG(LogWorldGenManager, Warning, TEXT("PIE shutdown complete"));
}