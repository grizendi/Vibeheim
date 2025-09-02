// ComprehensiveDiagnostic.cpp - Comprehensive diagnostic for all remaining failures
#if 0 // DISABLED FOR TEST DIET - Non-essential comprehensive diagnostic
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "HAL/ConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogComprehensiveDiagnostic, Log, All);

// Forward declarations from DiagnosticFailureTests.cpp
class FDiagnosticFailureTests
{
public:
    static void RunTerrainPersistenceDiagnostic();
    static void RunPCGContentDiagnostic();
};

class FComprehensiveDiagnostic
{
public:
    static void RunAllDiagnostics()
    {
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT("=== COMPREHENSIVE DIAGNOSTIC SUITE ==="));
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT("Running diagnostics for remaining integration test failures..."));
        
        // Run terrain persistence diagnostic
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT(""));
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT("1/2: Running Terrain Persistence Diagnostic"));
        FDiagnosticFailureTests::RunTerrainPersistenceDiagnostic();
        
        // Run PCG content diagnostic  
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT(""));
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT("2/2: Running PCG Content Generation Diagnostic"));
        FDiagnosticFailureTests::RunPCGContentDiagnostic();
        
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT(""));
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT("=== COMPREHENSIVE DIAGNOSTIC COMPLETE ==="));
        UE_LOG(LogComprehensiveDiagnostic, Warning, TEXT("Check logs above for detailed analysis of remaining failures"));
    }
};

// Console command for comprehensive diagnostic
static FAutoConsoleCommand ComprehensiveDiagnosticCmd(
    TEXT("wg.DiagnosticAll"),
    TEXT("Run comprehensive diagnostic for all remaining integration test failures"),
    FConsoleCommandDelegate::CreateStatic(&FComprehensiveDiagnostic::RunAllDiagnostics)
);

#endif // DISABLED FOR TEST DIET
