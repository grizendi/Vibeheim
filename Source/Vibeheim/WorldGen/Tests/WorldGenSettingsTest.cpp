#include "WorldGenSettingsTest.h"
#include "../Data/WorldGenSettings.h"
#include "../WorldGenConfigManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

bool FWorldGenSettingsValidationTest::RunTest(const FString& Parameters)
{
    // Test valid default settings
    FWorldGenSettings ValidSettings;
    TestTrue("Default settings should be valid", ValidSettings.ValidateSettings());

    // Test valid custom settings
    FWorldGenSettings CustomSettings;
    CustomSettings.VoxelSizeCm = 100.0f;
    CustomSettings.ChunkSize = 64;
    CustomSettings.LOD0Radius = 3;
    CustomSettings.LOD1Radius = 6;
    CustomSettings.LOD2Radius = 10;
    CustomSettings.BiomeBlendMeters = 50.0f;
    CustomSettings.SaveFlushMs = 5000;
    CustomSettings.MeadowsScale = 0.005f;
    CustomSettings.BlackForestScale = 0.007f;
    CustomSettings.SwampScale = 0.003f;

    TestTrue("Custom valid settings should be valid", CustomSettings.ValidateSettings());

    return true;
}

bool FWorldGenSettingsValidationInvalidTest::RunTest(const FString& Parameters)
{
    // Test invalid VoxelSizeCm
    FWorldGenSettings InvalidVoxelSize;
    InvalidVoxelSize.VoxelSizeCm = 0.5f; // Too small
    TestFalse("VoxelSizeCm too small should be invalid", InvalidVoxelSize.ValidateSettings());

    InvalidVoxelSize.VoxelSizeCm = 250.0f; // Too large
    TestFalse("VoxelSizeCm too large should be invalid", InvalidVoxelSize.ValidateSettings());

    // Test invalid ChunkSize
    FWorldGenSettings InvalidChunkSize;
    InvalidChunkSize.ChunkSize = 4; // Too small
    TestFalse("ChunkSize too small should be invalid", InvalidChunkSize.ValidateSettings());

    InvalidChunkSize.ChunkSize = 256; // Too large
    TestFalse("ChunkSize too large should be invalid", InvalidChunkSize.ValidateSettings());

    // Test invalid LOD radius ordering
    FWorldGenSettings InvalidLODOrder;
    InvalidLODOrder.LOD0Radius = 5;
    InvalidLODOrder.LOD1Radius = 3; // Should be greater than LOD0
    InvalidLODOrder.LOD2Radius = 8;
    TestFalse("Invalid LOD radius ordering should be invalid", InvalidLODOrder.ValidateSettings());

    // Test invalid BiomeBlendMeters
    FWorldGenSettings InvalidBiomeBlend;
    InvalidBiomeBlend.BiomeBlendMeters = 0.5f; // Too small
    TestFalse("BiomeBlendMeters too small should be invalid", InvalidBiomeBlend.ValidateSettings());

    InvalidBiomeBlend.BiomeBlendMeters = 150.0f; // Too large
    TestFalse("BiomeBlendMeters too large should be invalid", InvalidBiomeBlend.ValidateSettings());

    // Test invalid noise scales
    FWorldGenSettings InvalidNoiseScale;
    InvalidNoiseScale.MeadowsScale = 0.00005f; // Too small
    TestFalse("MeadowsScale too small should be invalid", InvalidNoiseScale.ValidateSettings());

    InvalidNoiseScale.MeadowsScale = 0.0025f; // Reset to valid
    InvalidNoiseScale.BlackForestScale = 0.02f; // Too large
    TestFalse("BlackForestScale too large should be invalid", InvalidNoiseScale.ValidateSettings());

    return true;
}

bool FWorldGenSettingsJSONTest::RunTest(const FString& Parameters)
{
    // Create test settings
    FWorldGenSettings OriginalSettings;
    OriginalSettings.Seed = 12345;
    OriginalSettings.WorldGenVersion = 2;
    OriginalSettings.PluginSHA = "test_sha_123";
    OriginalSettings.VoxelSizeCm = 75.0f;
    OriginalSettings.ChunkSize = 48;
    OriginalSettings.MaxLOD = 4;
    OriginalSettings.LOD0Radius = 3;
    OriginalSettings.LOD1Radius = 6;
    OriginalSettings.LOD2Radius = 12;
    OriginalSettings.bCollisionUpToLOD1 = false;
    OriginalSettings.BiomeBlendMeters = 30.0f;
    OriginalSettings.SaveFlushMs = 4000;
    OriginalSettings.MeadowsScale = 0.004f;
    OriginalSettings.BlackForestScale = 0.006f;
    OriginalSettings.SwampScale = 0.003f;

    // Test file path
    FString TestFilePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Test_WorldGenSettings.json"));

    // Save to JSON
    TestTrue("Should save settings to JSON", OriginalSettings.SaveToJSON(TestFilePath));
    TestTrue("JSON file should exist after save", FPlatformFileManager::Get().GetPlatformFile().FileExists(*TestFilePath));

    // Load from JSON
    FWorldGenSettings LoadedSettings;
    TestTrue("Should load settings from JSON", LoadedSettings.LoadFromJSON(TestFilePath));

    // Verify all values match
    TestEqual("Seed should match", LoadedSettings.Seed, OriginalSettings.Seed);
    TestEqual("WorldGenVersion should match", LoadedSettings.WorldGenVersion, OriginalSettings.WorldGenVersion);
    TestEqual("PluginSHA should match", LoadedSettings.PluginSHA, OriginalSettings.PluginSHA);
    TestEqual("VoxelSizeCm should match", LoadedSettings.VoxelSizeCm, OriginalSettings.VoxelSizeCm);
    TestEqual("ChunkSize should match", LoadedSettings.ChunkSize, OriginalSettings.ChunkSize);
    TestEqual("MaxLOD should match", LoadedSettings.MaxLOD, OriginalSettings.MaxLOD);
    TestEqual("LOD0Radius should match", LoadedSettings.LOD0Radius, OriginalSettings.LOD0Radius);
    TestEqual("LOD1Radius should match", LoadedSettings.LOD1Radius, OriginalSettings.LOD1Radius);
    TestEqual("LOD2Radius should match", LoadedSettings.LOD2Radius, OriginalSettings.LOD2Radius);
    TestEqual("bCollisionUpToLOD1 should match", LoadedSettings.bCollisionUpToLOD1, OriginalSettings.bCollisionUpToLOD1);
    TestEqual("BiomeBlendMeters should match", LoadedSettings.BiomeBlendMeters, OriginalSettings.BiomeBlendMeters);
    TestEqual("SaveFlushMs should match", LoadedSettings.SaveFlushMs, OriginalSettings.SaveFlushMs);
    TestEqual("MeadowsScale should match", LoadedSettings.MeadowsScale, OriginalSettings.MeadowsScale);
    TestEqual("BlackForestScale should match", LoadedSettings.BlackForestScale, OriginalSettings.BlackForestScale);
    TestEqual("SwampScale should match", LoadedSettings.SwampScale, OriginalSettings.SwampScale);

    // Clean up test file
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*TestFilePath);

    return true;
}

bool FWorldGenConfigManagerTest::RunTest(const FString& Parameters)
{
    // Create test config manager
    UWorldGenConfigManager* ConfigManager = NewObject<UWorldGenConfigManager>();
    TestNotNull("ConfigManager should be created", ConfigManager);

    // Test default initialization
    TestTrue("ConfigManager should have valid default configuration", ConfigManager->IsConfigurationValid());

    // Test settings update
    FWorldGenSettings NewSettings = ConfigManager->GetSettings();
    NewSettings.Seed = 9999;
    NewSettings.VoxelSizeCm = 60.0f;

    TestTrue("Should update settings with valid values", ConfigManager->UpdateSettings(NewSettings));
    TestEqual("Updated seed should match", ConfigManager->GetSettings().Seed, static_cast<int64>(9999));
    TestEqual("Updated VoxelSizeCm should match", ConfigManager->GetSettings().VoxelSizeCm, 60.0f);

    // Test invalid settings update
    FWorldGenSettings InvalidSettings = NewSettings;
    InvalidSettings.VoxelSizeCm = -10.0f; // Invalid
    TestFalse("Should reject invalid settings", ConfigManager->UpdateSettings(InvalidSettings));

    // Test reset to defaults
    ConfigManager->ResetToDefaults();
    TestEqual("Reset should restore default seed", ConfigManager->GetSettings().Seed, static_cast<int64>(1337));

    // Test file operations
    FString TestConfigPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Test_WorldGenConfig.json"));
    
    TestTrue("Should save configuration to file", ConfigManager->SaveConfiguration(TestConfigPath));
    TestTrue("Config file should exist", FPlatformFileManager::Get().GetPlatformFile().FileExists(*TestConfigPath));

    // Modify settings and reload
    NewSettings.Seed = 5555;
    ConfigManager->UpdateSettings(NewSettings);
    TestEqual("Modified seed should be set", ConfigManager->GetSettings().Seed, static_cast<int64>(5555));

    TestTrue("Should load configuration from file", ConfigManager->LoadConfiguration(TestConfigPath));
    TestEqual("Loaded seed should match saved value", ConfigManager->GetSettings().Seed, static_cast<int64>(1337));

    // Clean up
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*TestConfigPath);

    return true;
}

bool FWorldGenConfigManagerInvalidTest::RunTest(const FString& Parameters)
{
    UWorldGenConfigManager* ConfigManager = NewObject<UWorldGenConfigManager>();

    // Test loading non-existent file
    FString NonExistentPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NonExistent_Config.json"));
    TestTrue("Should handle non-existent file gracefully", ConfigManager->LoadConfiguration(NonExistentPath));
    TestTrue("Should create default config file", FPlatformFileManager::Get().GetPlatformFile().FileExists(*NonExistentPath));

    // Test loading invalid JSON
    FString InvalidJSONPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Invalid_Config.json"));
    FString InvalidJSON = TEXT("{ invalid json content }");
    FFileHelper::SaveStringToFile(InvalidJSON, *InvalidJSONPath);

    TestFalse("Should reject invalid JSON", ConfigManager->LoadConfiguration(InvalidJSONPath));

    // Test loading JSON with invalid values
    FString InvalidValuesPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("InvalidValues_Config.json"));
    FString InvalidValuesJSON = TEXT("{ \"VoxelSizeCm\": -50.0, \"ChunkSize\": 500 }");
    FFileHelper::SaveStringToFile(InvalidValuesJSON, *InvalidValuesPath);

    TestFalse("Should reject JSON with invalid values", ConfigManager->LoadConfiguration(InvalidValuesPath));

    // Clean up test files
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*NonExistentPath);
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*InvalidJSONPath);
    FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*InvalidValuesPath);

    return true;
}