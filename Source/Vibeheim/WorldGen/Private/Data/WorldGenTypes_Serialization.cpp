#include "Data/WorldGenTypes.h"
#include "UObject/Class.h"

FArchive& operator<<(FArchive& Ar, FHeightfieldModification& M)
{
    FHeightfieldModification::StaticStruct()->SerializeItem(Ar, &M, nullptr);
    return Ar;
}