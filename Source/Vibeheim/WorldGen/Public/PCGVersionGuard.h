// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * PCGVersionGuard.h
 * 
 * Engine version and PCG API availability guard for UE 5.6 migration.
 * This header MUST be included by every translation unit that touches PCG APIs.
 * 
 * Purpose:
 * - Enforce UE 5.6.x requirement at compile time
 * - Verify PCG module availability
 * - Provide VHM_PCG_ENABLED flag for conditional compilation
 */

// Strict UE 5.6.x requirement
#ifndef ENGINE_MAJOR_VERSION
    #error "ENGINE_MAJOR_VERSION not defined - check engine headers"
#endif

#ifndef ENGINE_MINOR_VERSION
    #error "ENGINE_MINOR_VERSION not defined - check engine headers"
#endif

static_assert(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 6, 
    "Vibeheim PCG integration requires UE 5.6.x only. Current version is incompatible.");

// Verify PCG module is available
#if !__has_include("PCGSubsystem.h")
    #error "PCG module headers not found - ensure PCG plugin is enabled in .uproject"
#endif

#ifndef WITH_PCG
    #error "WITH_PCG not defined - PCG plugin must be enabled"
#endif

// Define VHM_PCG_ENABLED flag for conditional compilation
// Server builds can override this to 0 in Build.cs to use HISM-only path
#ifndef VHM_PCG_ENABLED
    #define VHM_PCG_ENABLED 1
#endif

// Compile-time verification that we can use the scheduler API
#if VHM_PCG_ENABLED
    #include "PCGSubsystem.h"
    
    // Verify UE 5.6 scheduler API is available
    // This will fail at compile time if the API signature changes
    namespace PCGVersionGuard_Private
    {
        // Test that ScheduleGraph exists with expected signature
        template<typename T>
        struct HasScheduleGraph
        {
        private:
            template<typename U>
            static auto Test(int) -> decltype(
                std::declval<U>().ScheduleGraph(
                    std::declval<UPCGComponent*>(),
                    std::declval<FPCGTaskId>(),
                    std::declval<const FPCGStackContext&>()
                ),
                std::true_type{}
            );
            
            template<typename>
            static std::false_type Test(...);
            
        public:
            static constexpr bool Value = decltype(Test<T>(0))::value;
        };
        
        static_assert(HasScheduleGraph<UPCGSubsystem>::Value,
            "UPCGSubsystem::ScheduleGraph API not found - verify UE 5.6 PCG plugin version");
    }
#endif // VHM_PCG_ENABLED

/**
 * Usage Notes:
 * 
 * 1. Include this header in any file that uses PCG APIs
 * 2. Wrap PCG-specific code with #if VHM_PCG_ENABLED
 * 3. Provide HISM fallback in #else blocks
 * 4. For server targets, add to Build.cs:
 *    PublicDefinitions.Add("VHM_PCG_ENABLED=0");
 * 
 * Example:
 * 
 *   #include "PCGVersionGuard.h"
 *   
 *   #if VHM_PCG_ENABLED
 *       // PCG scheduler path
 *       UPCGSubsystem* PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
 *       // ...
 *   #else
 *       // HISM fallback path
 *       return GenerateFallbackContent(...);
 *   #endif
 */
