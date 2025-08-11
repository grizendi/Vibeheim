#pragma once

/**
 * Core WorldGen module header that includes all essential interfaces and data structures
 * Include this header to access the complete WorldGen API
 */

// Core module
#include "WorldGenModule.h"

// Configuration management
#include "WorldGenConfigManager.h"

// Service interfaces
#include "Interfaces/IVoxelWorldService.h"
#include "Interfaces/IVoxelEditService.h"
#include "Interfaces/IVoxelSaveService.h"

// Data structures
#include "Data/WorldGenSettings.h"
#include "Data/VoxelEditOp.h"
#include "Data/BiomeData.h"