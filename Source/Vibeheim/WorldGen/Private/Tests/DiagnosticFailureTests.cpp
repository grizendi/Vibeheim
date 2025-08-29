// DiagnosticFailureTests.cpp - Diagnostic tests for remaining integration test failures
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/ConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDiagnosticTests, Log, All);

class FDiagnosticFailureTests
{
public:
    static void RunTerrainPersistenceDiagnostic()
    {
        UE_LOG(LogDiagnosticTests, Warning, TEXT("=== TERRAIN PERSISTENCE DIAGNOSTIC ==="));
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Diagnostic tests have been disabled due to API changes."));
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Use 'wg.IntegrationTest' for comprehensive testing."));
    }
    
    static void RunPCGContentDiagnostic()
    {
        UE_LOG(LogDiagnosticTests, Warning, TEXT("=== PCG CONTENT GENERATION DIAGNOSTIC ==="));
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Diagnostic tests have been disabled due to API changes."));
        UE_LOG(LogDiagnosticTests, Warning, TEXT("Use 'wg.IntegrationTest' for comprehensive testing."));
    }
};

// Console commands
static FAutoConsoleCommand DiagnosticTerrainPersistenceCmd(
    TEXT("wg.DiagnosticTerrainPersistence"),
    TEXT("Run diagnostic test for terrain persistence checksum mismatch"),
    FConsoleCommandDelegate::CreateStatic(&FDiagnosticFailureTests::RunTerrainPersistenceDiagnostic)
);

static FAutoConsoleCommand DiagnosticPCGContentCmd(
    TEXT("wg.DiagnosticPCGContent"),
    TEXT("Run diagnostic test for PCG content generation failure"),
    FConsoleCommandDelegate::CreateStatic(&FDiagnosticFailureTests::RunPCGContentDiagnostic)
);