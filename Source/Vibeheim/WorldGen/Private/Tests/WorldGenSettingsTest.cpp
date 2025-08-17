#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "WorldGenSettings.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsDefaultsTest, "WorldGen.Settings.Defaults", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldGenSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	// Test that default settings are valid
	UWorldGenSettings* Settings = NewObject<UWorldGenSettings>();
	
	TArray<FString> ValidationErrors;
	bool bValid = Settings->ValidateSettings(ValidationErrors);
	
	TestTrue("Default settings should be valid", bValid);
	TestEqual("Default settings should have no validation errors", ValidationErrors.Num(), 0);
	
	// Test locked values are correctly applied
	TestEqual("TileSizeMeters should be locked to 64", Settings->Settings.TileSizeMeters, 64.0f);
	TestEqual("SampleSpacingMeters should be locked to 1", Settings->Settings.SampleSpacingMeters, 1.0f);
	TestEqual("MaxTerrainHeight should be locked to 120", Settings->Settings.MaxTerrainHeight, 120.0f);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsValidationTest, "WorldGen.Settings.Validation", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldGenSettingsValidationTest::RunTest(const FString& Parameters)
{
	UWorldGenSettings* Settings = NewObject<UWorldGenSettings>();
	
	// Test heightfield resolution validation (must be power of 2)
	Settings->Settings.HeightfieldResolution = 1000; // Not a power of 2
	TArray<FString> ValidationErrors;
	bool bValid = Settings->ValidateSettings(ValidationErrors);
	
	TestFalse("Invalid heightfield resolution should fail validation", bValid);
	TestTrue("Should have validation errors", ValidationErrors.Num() > 0);
	TestTrue("HeightfieldResolution should be corrected to power of 2", 
		(Settings->Settings.HeightfieldResolution & (Settings->Settings.HeightfieldResolution - 1)) == 0);
	
	// Test streaming radius relationships
	Settings->ResetToDefaults();
	Settings->Settings.LoadRadius = 10;
	Settings->Settings.GenerateRadius = 5; // Invalid: Load > Generate
	
	ValidationErrors.Empty();
	bValid = Settings->ValidateSettings(ValidationErrors);
	
	TestFalse("Invalid radius relationship should fail validation", bValid);
	TestTrue("LoadRadius should be corrected", Settings->Settings.LoadRadius <= Settings->Settings.GenerateRadius);
	
	// Test clamping of out-of-range values
	Settings->ResetToDefaults();
	Settings->Settings.VegetationDensity = -1.0f; // Below minimum
	Settings->Settings.MaxHISMInstances = 200000; // Above maximum
	
	ValidationErrors.Empty();
	bValid = Settings->ValidateSettings(ValidationErrors);
	
	TestFalse("Out-of-range values should fail validation", bValid);
	TestTrue("VegetationDensity should be clamped to valid range", 
		Settings->Settings.VegetationDensity >= 0.0f && Settings->Settings.VegetationDensity <= 10.0f);
	TestTrue("MaxHISMInstances should be clamped to valid range", 
		Settings->Settings.MaxHISMInstances >= 100 && Settings->Settings.MaxHISMInstances <= 100000);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsJSONTest, "WorldGen.Settings.JSON", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldGenSettingsJSONTest::RunTest(const FString& Parameters)
{
	UWorldGenSettings* Settings = NewObject<UWorldGenSettings>();
	
	// Create test JSON content
	const FString TestJSON = TEXT(R"({
		"Seed": 12345,
		"WorldGenVersion": 2,
		"GenerateRadius": 7,
		"LoadRadius": 4,
		"ActiveRadius": 2,
		"HeightfieldResolution": 512,
		"VegetationDensity": 1.5,
		"PerfTargets": {
			"TileGenMs": 3.0,
			"PCGMsPerTile": 1.5
		}
	})");
	
	// Save test JSON to temporary file
	const FString TestFilePath = FPaths::ProjectSavedDir() / TEXT("Temp") / TEXT("TestWorldGenSettings.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TestFilePath), true);
	
	bool bSaved = FFileHelper::SaveStringToFile(TestJSON, *TestFilePath);
	TestTrue("Should be able to save test JSON file", bSaved);
	
	// Load settings from test JSON
	bool bLoaded = Settings->LoadFromJSON(TEXT("Saved/Temp/TestWorldGenSettings.json"));
	TestTrue("Should be able to load settings from JSON", bLoaded);
	
	// Verify loaded values
	TestEqual("Seed should be loaded correctly", Settings->Settings.Seed, static_cast<int32>(12345));
	TestEqual("WorldGenVersion should be loaded correctly", Settings->Settings.WorldGenVersion, 2);
	TestEqual("GenerateRadius should be loaded correctly", Settings->Settings.GenerateRadius, 7);
	TestEqual("LoadRadius should be loaded correctly", Settings->Settings.LoadRadius, 4);
	TestEqual("ActiveRadius should be loaded correctly", Settings->Settings.ActiveRadius, 2);
	TestEqual("HeightfieldResolution should be loaded correctly", Settings->Settings.HeightfieldResolution, 512);
	TestEqual("VegetationDensity should be loaded correctly", Settings->Settings.VegetationDensity, 1.5f);
	TestEqual("TileGenTargetMs should be loaded correctly", Settings->Settings.TileGenTargetMs, 3.0f);
	TestEqual("PCGTargetMsPerTile should be loaded correctly", Settings->Settings.PCGTargetMsPerTile, 1.5f);
	
	// Verify locked values are still enforced
	TestEqual("TileSizeMeters should remain locked", Settings->Settings.TileSizeMeters, 64.0f);
	TestEqual("SampleSpacingMeters should remain locked", Settings->Settings.SampleSpacingMeters, 1.0f);
	TestEqual("MaxTerrainHeight should remain locked", Settings->Settings.MaxTerrainHeight, 120.0f);
	
	// Test saving settings back to JSON
	const FString SaveTestPath = FPaths::ProjectSavedDir() / TEXT("Temp") / TEXT("SaveTestWorldGenSettings.json");
	bool bSavedBack = Settings->SaveToJSON(TEXT("Saved/Temp/SaveTestWorldGenSettings.json"));
	TestTrue("Should be able to save settings to JSON", bSavedBack);
	
	// Verify the saved file exists and contains expected content
	FString SavedContent;
	bool bLoadedSaved = FFileHelper::LoadFileToString(SavedContent, *SaveTestPath);
	TestTrue("Should be able to read saved JSON file", bLoadedSaved);
	TestTrue("Saved JSON should contain seed value", SavedContent.Contains(TEXT("12345")));
	TestTrue("Saved JSON should contain performance targets", SavedContent.Contains(TEXT("PerfTargets")));
	
	// Clean up test files
	IFileManager::Get().Delete(*TestFilePath);
	IFileManager::Get().Delete(*SaveTestPath);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsInvalidJSONTest, "WorldGen.Settings.InvalidJSON", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldGenSettingsInvalidJSONTest::RunTest(const FString& Parameters)
{
	UWorldGenSettings* Settings = NewObject<UWorldGenSettings>();
	
	// Test loading from non-existent file
	bool bLoaded = Settings->LoadFromJSON(TEXT("NonExistent/Path/Settings.json"));
	TestFalse("Loading from non-existent file should fail gracefully", bLoaded);
	
	// Test loading invalid JSON
	const FString InvalidJSON = TEXT("{ invalid json content }");
	const FString InvalidJSONPath = FPaths::ProjectSavedDir() / TEXT("Temp") / TEXT("InvalidSettings.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(InvalidJSONPath), true);
	
	FFileHelper::SaveStringToFile(InvalidJSON, *InvalidJSONPath);
	bLoaded = Settings->LoadFromJSON(TEXT("Saved/Temp/InvalidSettings.json"));
	TestFalse("Loading invalid JSON should fail gracefully", bLoaded);
	
	// Test that settings remain valid after failed load
	TArray<FString> ValidationErrors;
	bool bValid = Settings->ValidateSettings(ValidationErrors);
	TestTrue("Settings should remain valid after failed JSON load", bValid);
	
	// Clean up
	IFileManager::Get().Delete(*InvalidJSONPath);
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsSingletonTest, "WorldGen.Settings.Singleton", 
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldGenSettingsSingletonTest::RunTest(const FString& Parameters)
{
	// Test singleton behavior
	UWorldGenSettings* Settings1 = UWorldGenSettings::GetWorldGenSettings();
	UWorldGenSettings* Settings2 = UWorldGenSettings::GetWorldGenSettings();
	
	TestTrue("GetWorldGenSettings should return valid object", IsValid(Settings1));
	TestEqual("GetWorldGenSettings should return same instance", Settings1, Settings2);
	
	// Test that singleton persists changes
	int32 OriginalSeed = Settings1->Settings.Seed;
	Settings1->Settings.Seed = 99999;
	
	UWorldGenSettings* Settings3 = UWorldGenSettings::GetWorldGenSettings();
	TestEqual("Singleton should persist changes", Settings3->Settings.Seed, static_cast<int32>(99999));
	
	// Reset for other tests
	Settings1->Settings.Seed = OriginalSeed;
	
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS