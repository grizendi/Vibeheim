#include "Data/SerializationShims.h"
#include "Data/WorldGenTypes.h"          // FTileCoord, FHeightfieldModification, FPOIData, FPCGInstanceData
#include "Data/InstancePersistence.h"    // FInstanceJournalEntry
#include "Services/PCGWorldService.h"    // FHISMComponentArray
#include "Misc/Guid.h"
#include "UObject/Class.h"

// --- small local helpers (internal linkage) ---
static uint32 VH_HashGuid(const FGuid& G)
{
    uint32 H = 0;
    H = HashCombine(H, GetTypeHash(G.A));
    H = HashCombine(H, GetTypeHash(G.B));
    H = HashCombine(H, GetTypeHash(G.C));
    H = HashCombine(H, GetTypeHash(G.D));
    return H;
}

static uint32 VH_HashVec2(const FVector2D& V)
{
    uint32 H = 0;
    H = HashCombine(H, GetTypeHash(V.X));
    H = HashCombine(H, GetTypeHash(V.Y));
    return H;
}

// ---- GetTypeHash bodies ----
uint32 GetTypeHash(const FTileCoord& C)
{
    return HashCombine(GetTypeHash(C.X), GetTypeHash(C.Y));
}

uint32 GetTypeHash(const FHeightfieldModification& M)
{
    uint32 H = 0;
    H = HashCombine(H, VH_HashGuid(M.ModificationId));
    H = HashCombine(H, VH_HashVec2(M.Center));
    H = HashCombine(H, GetTypeHash(M.Radius));
    H = HashCombine(H, GetTypeHash(static_cast<uint8>(M.Operation)));
    H = HashCombine(H, GetTypeHash(M.AffectedTile)); // calls the FTileCoord overload above
    return H;
}

uint32 GetTypeHash(const FPOIData& P) { return VH_HashGuid(P.POIId); }
uint32 GetTypeHash(const FPCGInstanceData& I) { return VH_HashGuid(I.InstanceId); }
uint32 GetTypeHash(const FInstanceJournalEntry& E) { return VH_HashGuid(E.InstanceId); }

uint32 GetTypeHash(const FHISMComponentArray& A)
{
    uint32 Hash = 0;
    for (const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& Component : A.Components)
    {
        Hash = HashCombine(Hash, GetTypeHash(Component.Get()));
    }
    return Hash;
}

// ---- serializers ----
// NOTE: Keep only one definition of each operator<< in the whole project.

FArchive& operator<<(FArchive& Ar, FTileCoord& C)
{
    C.Serialize(Ar);
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FPCGInstanceData& I)
{
    I.Serialize(Ar);
    return Ar;
}

// HeightfieldModification serializer already exists elsewhere in your project,
// so do NOT define it here again (or you�ll get duplicate-definition/link errors).

FArchive& operator<<(FArchive& Ar, FInstanceJournalEntry& E)
{
    FInstanceJournalEntry::StaticStruct()->SerializeItem(Ar, &E, nullptr);
    return Ar;
}
