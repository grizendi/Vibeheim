#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "../Data/WorldGenSettings.h"
#include "../WorldGenConfigManager.h"

/**
 * Unit tests for WorldGen settings validation and JSON parsing
 */

// Test settings validation with valid parameters
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsValidationTest, "WorldGen.Settings.Validation.Valid", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Test settings validation with invalid parameters
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsValidationInvalidTest, "WorldGen.Settings.Validation.Invalid", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Test JSON serialization and deserialization
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenSettingsJSONTest, "WorldGen.Settings.JSON.SerializeDeserialize", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Test configuration manager functionality
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenConfigManagerTest, "WorldGen.ConfigManager.LoadSave", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

// Test configuration manager with invalid files
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldGenConfigManagerInvalidTest, "WorldGen.ConfigManager.InvalidFile", 
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)