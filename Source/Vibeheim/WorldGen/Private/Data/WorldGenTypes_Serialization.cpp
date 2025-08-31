#include "Data/WorldGenTypes.h"
#include "UObject/Class.h"

FArchive& operator<<(FArchive& Ar, FHeightfieldModification& M)
{
    FHeightfieldModification::StaticStruct()->SerializeItem(Ar, &M, nullptr);
    return Ar;
}

// Do NOT define GetTypeHash(FTileCoord) here. It lives in SerializationShims.cpp.