#pragma once
#include "CoreMinimal.h"

struct FTileCoord;
struct FHeightfieldModification;
struct FPOIData;
struct FPCGInstanceData;
struct FInstanceJournalEntry;
struct FHISMComponentArray;

uint32 GetTypeHash(const FTileCoord& C);
uint32 GetTypeHash(const FHeightfieldModification& M);
uint32 GetTypeHash(const FPOIData& P);
uint32 GetTypeHash(const FPCGInstanceData& I);
uint32 GetTypeHash(const FInstanceJournalEntry& E);
uint32 GetTypeHash(const FHISMComponentArray& A);

class FArchive;
FArchive& operator<<(FArchive& Ar, FTileCoord& C);
FArchive& operator<<(FArchive& Ar, FHeightfieldModification& M);
FArchive& operator<<(FArchive& Ar, FPOIData& P);
FArchive& operator<<(FArchive& Ar, FPCGInstanceData& I);
FArchive& operator<<(FArchive& Ar, FInstanceJournalEntry& E);
