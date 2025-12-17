#pragma once

#include "CoreMinimal.h"
#include "Data/WorldGenTypes.h"
#include "HAL/FileManager.h"
#include "Compression/CompressedBuffer.h"
#include "Misc/Compression.h" 
#include "InstancePersistence.generated.h"

struct FPCGGenerationData;

/**
 * Instance modification operation types for journal tracking
 */
UENUM(BlueprintType)
enum class EInstanceOperation : uint8
{
	Add = 0,        // Instance was spawned
	Remove = 1,     // Instance was destroyed/removed
	Modify = 2,     // Instance properties were modified
	MAX UMETA(Hidden)
};

/**
 * Journal entry for tracking instance modifications
 * Compact structure designed for efficient storage and compression
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FInstanceJournalEntry
{
	GENERATED_BODY()

	// Unique instance identifier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	FGuid InstanceId = FGuid();

	// Operation performed on this instance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	EInstanceOperation Operation = EInstanceOperation::Add;

	// Timestamp of the operation (for ordering and conflict resolution)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	int64 Timestamp = 0;

	// Instance data (only relevant for Add/Modify operations)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	FPCGInstanceData InstanceData;

	// POI data (if this entry represents a POI)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	FPOIData POIData;

	// Whether this entry represents a POI vs regular instance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	bool bIsPOI = false;

	// Version number for save compatibility
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	int32 Version = 1;

	FInstanceJournalEntry()
	{
		InstanceId = FGuid::NewGuid();
		Timestamp = FDateTime::Now().ToUnixTimestamp();
		ensureMsgf(InstanceId.IsValid(), TEXT("FInstanceJournalEntry: InstanceId must be valid after construction"));
	}

	FInstanceJournalEntry(const FPCGInstanceData& InInstanceData, EInstanceOperation InOperation)
		: InstanceId(InInstanceData.InstanceId), Operation(InOperation), bIsPOI(false)
	{
		InstanceData = InInstanceData;
		Timestamp = FDateTime::Now().ToUnixTimestamp();
		ensureMsgf(InstanceId.IsValid(), TEXT("FInstanceJournalEntry: InstanceId must be valid after construction from PCGInstanceData"));
	}

	FInstanceJournalEntry(const FPOIData& InPOIData, EInstanceOperation InOperation)
		: InstanceId(InPOIData.POIId), Operation(InOperation), bIsPOI(true)
	{
		POIData = InPOIData;
		Timestamp = FDateTime::Now().ToUnixTimestamp();
		ensureMsgf(InstanceId.IsValid(), TEXT("FInstanceJournalEntry: InstanceId must be valid after construction from POIData"));
	}
};

// TStructOpsTypeTraits for FInstanceJournalEntry
// WithZeroConstructor = true: Uses in-class FGuid() initializer for UE5.6 reflection compatibility
template<>
struct TStructOpsTypeTraits<FInstanceJournalEntry> : public TStructOpsTypeTraitsBase2<FInstanceJournalEntry>
{
	enum
	{
		WithZeroConstructor = true,      // Uses in-class FGuid() initializer for reflection compatibility
		WithSerializer = false            // No custom serialization
	};
};

/**
 * Compressed instance journal for a single tile
 * Contains all instance modifications for efficient storage and replay
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FTileInstanceJournal
{
	GENERATED_BODY()

	// Tile coordinate this journal belongs to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	FTileCoord TileCoord;

	// All journal entries for this tile (chronologically ordered)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	TArray<FInstanceJournalEntry> Entries;

	// Checksum for data integrity validation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	int32 Checksum = 0;

	// Journal version for save compatibility
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	int32 JournalVersion = 1;

	// Last modification time for caching and dirty tracking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journal")
	int64 LastModified = 0;

	FTileInstanceJournal()
	{
		LastModified = FDateTime::Now().ToUnixTimestamp();
	}

	explicit FTileInstanceJournal(FTileCoord InTileCoord)
		: TileCoord(InTileCoord)
	{
		LastModified = FDateTime::Now().ToUnixTimestamp();
	}

	// Add journal entry and update metadata
	void AddEntry(const FInstanceJournalEntry& Entry)
	{
		Entries.Add(Entry);
		LastModified = FDateTime::Now().ToUnixTimestamp();
		UpdateChecksum();
	}

	// Calculate and update checksum for integrity validation
	void UpdateChecksum();

	// Validate journal integrity
	bool ValidateIntegrity() const;

	// Compress journal entries for storage
	bool CompressEntries(TArray<uint8>& OutCompressedData) const;

	// Decompress journal entries from storage
	bool DecompressEntries(const TArray<uint8>& InCompressedData);

	// Get all active instances after applying journal
	TArray<FPCGInstanceData> GetActiveInstances() const;

	// Get all active POIs after applying journal
	TArray<FPOIData> GetActivePOIs() const;

	// Compact journal by removing redundant entries
	void CompactJournal();
};

/**
 * Instance persistence manager for handling .inst file operations
 * Manages journal-based persistence and replay functionality
 */
UCLASS(BlueprintType, Blueprintable)
class VIBEHEIM_API UInstancePersistenceManager : public UObject
{
	GENERATED_BODY()

public:
	UInstancePersistenceManager();

	// Initialize persistence manager with world generation settings
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool Initialize(const FWorldGenConfig& Settings);

	// Save tile instance journal to disk
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool SaveTileJournal(FTileCoord TileCoord);

	// Load tile instance journal from disk
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool LoadTileJournal(FTileCoord TileCoord);

	// Add instance modification to journal
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool AddInstanceOperation(FTileCoord TileCoord, const FPCGInstanceData& InstanceData, EInstanceOperation Operation);

	// Add POI modification to journal
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool AddPOIOperation(FTileCoord TileCoord, const FPOIData& POIData, EInstanceOperation Operation);

	// Replay journal and reconcile with PCG-generated content
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool ReplayTileJournal(FTileCoord TileCoord, class UPCGWorldService* PCGService, FPCGGenerationData* InOutGenerationData = nullptr);

	// Get current journal for a tile (C++ only, not Blueprint callable due to pointer return)
	const FTileInstanceJournal* GetTileJournal(FTileCoord TileCoord) const;

	// Get journal data for Blueprint use (returns by value)
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	FTileInstanceJournal GetTileJournalData(FTileCoord TileCoord) const;

	// Check if tile has persistent modifications
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool HasPersistentModifications(FTileCoord TileCoord) const;

	// Compact all journals to remove redundant entries
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	void CompactAllJournals();

	// Get persistence statistics
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	FString GetPersistenceStats() const;

	// Validate all journal files integrity
	UFUNCTION(BlueprintCallable, Category = "Persistence")
	bool ValidateAllJournals(TArray<FString>& OutErrors) const;

	// Register baseline content for a tile to filter baked instances from persistence.
	void RegisterBaseContent(FTileCoord TileCoord, const TArray<FPCGInstanceData>& BaseInstances, const TArray<FPOIData>& BasePOIs);

	// Mark a tile save as degraded when delta application cannot be safely applied.
	void FlagTileAsDegraded(FTileCoord TileCoord, const FString& Reason);

	// Deterministic delta application helper for property testing.
	struct FInstanceDeltaApplicationResult
	{
		bool bSuccess = true;
		bool bSkippedBaseEntries = false;
		TArray<FPCGInstanceData> FinalInstances;
		TMap<FGuid, FPOIData> FinalPOIs;
	};

	FInstanceDeltaApplicationResult ComputeDeltaApplication(
		FTileCoord TileCoord,
		const TArray<FInstanceJournalEntry>& Entries,
		const TArray<FPCGInstanceData>& BaseInstances,
		const TMap<FGuid, FPOIData>& BasePOIs) const;

private:
	// World generation configuration
	UPROPERTY()
	FWorldGenConfig WorldGenSettings;

	// In-memory journal cache (TileCoord -> Journal)
	UPROPERTY()
	TMap<FTileCoord, FTileInstanceJournal> JournalCache;

	// Dirty journal tracking for batched saves
	UPROPERTY()
	TSet<FTileCoord> DirtyJournals;

	// Base directory for .inst files
	FString PersistenceDirectory;

	// Compression settings
	bool bUseCompression = true;
	FName CompressionFormat = NAME_Zlib;
	ECompressionFlags CompressionFlags = ECompressionFlags::COMPRESS_None;

	// Performance tracking
	int32 TotalJournalEntries = 0;
	float TotalLoadTimeMs = 0.0f;
	float TotalSaveTimeMs = 0.0f;

	// Baseline baked content for delta filtering
	UPROPERTY()
	TMap<FTileCoord, TSet<FGuid>> BaseInstanceIds;

	UPROPERTY()
	TMap<FTileCoord, TSet<FGuid>> BasePOIIds;

	UPROPERTY()
	TMap<FTileCoord, FString> DegradedTiles;

	// Get file path for tile journal
	FString GetTileJournalPath(FTileCoord TileCoord) const;

	// Serialize journal to binary data
	bool SerializeJournal(const FTileInstanceJournal& Journal, TArray<uint8>& OutData) const;

	// Deserialize journal from binary data
	bool DeserializeJournal(const TArray<uint8>& InData, FTileInstanceJournal& OutJournal) const;

	// Ensure tile journal exists in cache
	FTileInstanceJournal& GetOrCreateTileJournal(FTileCoord TileCoord);

	// Mark journal as dirty for batched saving
	void MarkJournalDirty(FTileCoord TileCoord);

	// Save all dirty journals to disk
	bool SaveDirtyJournals();

	// Create persistence directory if it doesn't exist
	bool EnsurePersistenceDirectory();

	bool IsBaseInstance(FTileCoord TileCoord, const FGuid& InstanceId) const;
	bool IsBasePOI(FTileCoord TileCoord, const FGuid& POIId) const;

	FInstanceDeltaApplicationResult ApplyEntriesDeterministically(
		FTileCoord TileCoord,
		const TArray<FInstanceJournalEntry>& Entries,
		const TArray<FPCGInstanceData>& BaseInstances,
		const TMap<FGuid, FPOIData>& BasePOIs) const;
};

/**
 * Instance modification tracker for efficient change detection
 * Tracks which instances have been added, removed, or modified
 */
USTRUCT(BlueprintType)
struct VIBEHEIM_API FInstanceModificationTracker
{
	GENERATED_BODY()

	// Instances that were added (not in original PCG generation)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	TSet<FGuid> AddedInstances;

	// Instances that were removed from original PCG generation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	TSet<FGuid> RemovedInstances;

	// Instances that were modified from original PCG generation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	TSet<FGuid> ModifiedInstances;

	// POIs that were added (not in original PCG generation)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	TSet<FGuid> AddedPOIs;

	// POIs that were removed from original PCG generation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	TSet<FGuid> RemovedPOIs;

	// Check if instance has any modifications
	bool HasModifications() const
	{
		return AddedInstances.Num() > 0 || RemovedInstances.Num() > 0 || 
			   ModifiedInstances.Num() > 0 || AddedPOIs.Num() > 0 || RemovedPOIs.Num() > 0;
	}

	// Clear all tracking data
	void Reset()
	{
		AddedInstances.Empty();
		RemovedInstances.Empty();
		ModifiedInstances.Empty();
		AddedPOIs.Empty();
		RemovedPOIs.Empty();
	}

	// Get total number of tracked modifications
	int32 GetTotalModificationCount() const
	{
		return AddedInstances.Num() + RemovedInstances.Num() + ModifiedInstances.Num() + 
			   AddedPOIs.Num() + RemovedPOIs.Num();
	}
};
