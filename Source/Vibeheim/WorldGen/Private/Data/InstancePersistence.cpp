#include "Data/InstancePersistence.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Services/PCGWorldService.h"
#include "Utils/HashUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogInstancePersistence, Log, All);

// FTileInstanceJournal implementation

void FTileInstanceJournal::UpdateChecksum()
{
	// Calculate checksum based on all journal entries
	uint32 NewChecksum = 0;
	
	for (const FInstanceJournalEntry& Entry : Entries)
	{
		uint32 EntryHash = 0;
		EntryHash = HashCombine(EntryHash, GetTypeHash(Entry.InstanceId));
		EntryHash = HashCombine(EntryHash, GetTypeHash(static_cast<uint8>(Entry.Operation)));
		EntryHash = HashCombine(EntryHash, GetTypeHash(Entry.Timestamp));
		EntryHash = HashCombine(EntryHash, GetTypeHash(Entry.bIsPOI));
		
		NewChecksum = HashCombine(NewChecksum, EntryHash);
	}
	
	// Convert to int32 for Blueprint compatibility (checksum field is now int32)
	Checksum = static_cast<int32>(NewChecksum);
}

bool FTileInstanceJournal::ValidateIntegrity() const
{
	// Create a temporary copy to calculate checksum
	FTileInstanceJournal TempJournal = *this;
	uint32 OriginalChecksum = TempJournal.Checksum;
	TempJournal.UpdateChecksum();
	
	return OriginalChecksum == TempJournal.Checksum;
}

bool FTileInstanceJournal::CompressEntries(TArray<uint8>& OutCompressedData) const
{
	if (Entries.Num() == 0)
	{
		OutCompressedData.Empty();
		return true;
	}

	// Serialize entries to binary data first
	FMemoryWriter Writer(OutCompressedData);
	FObjectAndNameAsStringProxyArchive StringArchive(Writer, true);
	
	// Write journal header
	int32 EntryCount = Entries.Num();
	StringArchive << EntryCount;
	StringArchive << JournalVersion;
	
	// Write all entries
	for (const FInstanceJournalEntry& Entry : Entries)
	{
		// Write entry data
		FGuid TempGuid = Entry.InstanceId;
		StringArchive << TempGuid;
		
		uint8 OperationType = static_cast<uint8>(Entry.Operation);
		StringArchive << OperationType;
		StringArchive << const_cast<int64&>(Entry.Timestamp);
		StringArchive << const_cast<bool&>(Entry.bIsPOI);
		StringArchive << const_cast<int32&>(Entry.Version);
		
		// Serialize instance data
		if (Entry.Operation == EInstanceOperation::Add || Entry.Operation == EInstanceOperation::Modify)
		{
			if (Entry.bIsPOI)
			{
				// Serialize POI data
				StringArchive << const_cast<FPOIData&>(Entry.POIData);
			}
			else
			{
				// Serialize instance data
				StringArchive << const_cast<FPCGInstanceData&>(Entry.InstanceData);
			}
		}
	}

	// TODO: Add actual compression using UE5's compression system
	// For now, we store uncompressed data but the infrastructure is ready
	UE_LOG(LogInstancePersistence, Log, TEXT("Compressed journal entries for tile (%d, %d): %d entries, %d bytes"), 
		TileCoord.X, TileCoord.Y, Entries.Num(), OutCompressedData.Num());

	return true;
}

bool FTileInstanceJournal::DecompressEntries(const TArray<uint8>& InCompressedData)
{
	if (InCompressedData.Num() == 0)
	{
		Entries.Empty();
		return true;
	}

	// TODO: Add actual decompression when compression is implemented
	// For now, read uncompressed data
	FMemoryReader Reader(const_cast<TArray<uint8>&>(InCompressedData));
	FObjectAndNameAsStringProxyArchive StringArchive(Reader, true);
	
	// Read journal header
	int32 EntryCount = 0;
	StringArchive << EntryCount;
	StringArchive << JournalVersion;
	
	if (EntryCount < 0 || EntryCount > 100000) // Sanity check
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Invalid entry count in compressed data: %d"), EntryCount);
		return false;
	}
	
	Entries.Empty(EntryCount);
	
	// Read all entries
	for (int32 i = 0; i < EntryCount; i++)
	{
		FInstanceJournalEntry Entry;
		
		// Read entry data
		StringArchive << Entry.InstanceId;
		
		uint8 OperationType;
		StringArchive << OperationType;
		Entry.Operation = static_cast<EInstanceOperation>(OperationType);
		
		StringArchive << Entry.Timestamp;
		StringArchive << Entry.bIsPOI;
		StringArchive << Entry.Version;
		
		// Read instance data based on operation type
		if (Entry.Operation == EInstanceOperation::Add || Entry.Operation == EInstanceOperation::Modify)
		{
			if (Entry.bIsPOI)
			{
				StringArchive << Entry.POIData;
			}
			else
			{
				StringArchive << Entry.InstanceData;
			}
		}
		
		Entries.Add(Entry);
	}

	UE_LOG(LogInstancePersistence, Log, TEXT("Decompressed journal entries for tile (%d, %d): %d entries"), 
		TileCoord.X, TileCoord.Y, Entries.Num());

	return true;
}

TArray<FPCGInstanceData> FTileInstanceJournal::GetActiveInstances() const
{
	TMap<FGuid, FPCGInstanceData> InstanceMap;
	
	// Apply all journal entries in chronological order
	for (const FInstanceJournalEntry& Entry : Entries)
	{
		if (Entry.bIsPOI)
		{
			continue; // Skip POI entries
		}
		
		switch (Entry.Operation)
		{
			case EInstanceOperation::Add:
			case EInstanceOperation::Modify:
				InstanceMap.Add(Entry.InstanceId, Entry.InstanceData);
				break;
				
			case EInstanceOperation::Remove:
				InstanceMap.Remove(Entry.InstanceId);
				break;
		}
	}
	
	// Convert map to array
	TArray<FPCGInstanceData> ActiveInstances;
	InstanceMap.GenerateValueArray(ActiveInstances);
	
	return ActiveInstances;
}

TArray<FPOIData> FTileInstanceJournal::GetActivePOIs() const
{
	TMap<FGuid, FPOIData> POIMap;
	
	// Apply all journal entries in chronological order
	for (const FInstanceJournalEntry& Entry : Entries)
	{
		if (!Entry.bIsPOI)
		{
			continue; // Skip regular instance entries
		}
		
		switch (Entry.Operation)
		{
			case EInstanceOperation::Add:
			case EInstanceOperation::Modify:
				POIMap.Add(Entry.InstanceId, Entry.POIData);
				break;
				
			case EInstanceOperation::Remove:
				POIMap.Remove(Entry.InstanceId);
				break;
		}
	}
	
	// Convert map to array
	TArray<FPOIData> ActivePOIs;
	POIMap.GenerateValueArray(ActivePOIs);
	
	return ActivePOIs;
}

void FTileInstanceJournal::CompactJournal()
{
	if (Entries.Num() == 0)
	{
		return;
	}

	// Track the final state of each instance/POI
	TMap<FGuid, FInstanceJournalEntry> FinalStates;
	TSet<FGuid> RemovedItems;
	
	// Process entries to find final states
	for (const FInstanceJournalEntry& Entry : Entries)
	{
		switch (Entry.Operation)
		{
			case EInstanceOperation::Add:
			case EInstanceOperation::Modify:
				FinalStates.Add(Entry.InstanceId, Entry);
				RemovedItems.Remove(Entry.InstanceId);
				break;
				
			case EInstanceOperation::Remove:
				FinalStates.Remove(Entry.InstanceId);
				RemovedItems.Add(Entry.InstanceId);
				break;
		}
	}
	
	// Build compacted entries list
	TArray<FInstanceJournalEntry> CompactedEntries;
	
	// Add all items with final states (Convert Add to final state, keep original timestamp)
	for (auto& StatePair : FinalStates)
	{
		FInstanceJournalEntry CompactedEntry = StatePair.Value;
		CompactedEntry.Operation = EInstanceOperation::Add; // Treat as fresh add
		CompactedEntries.Add(CompactedEntry);
	}
	
	// Add removal entries for items that were ultimately removed
	for (const FGuid& RemovedId : RemovedItems)
	{
		FInstanceJournalEntry RemovalEntry;
		RemovalEntry.InstanceId = RemovedId;
		RemovalEntry.Operation = EInstanceOperation::Remove;
		RemovalEntry.Timestamp = FDateTime::Now().ToUnixTimestamp();
		CompactedEntries.Add(RemovalEntry);
	}
	
	// Sort by timestamp to maintain chronological order
	CompactedEntries.Sort([](const FInstanceJournalEntry& A, const FInstanceJournalEntry& B) {
		return A.Timestamp < B.Timestamp;
	});
	
	int32 OriginalCount = Entries.Num();
	Entries = MoveTemp(CompactedEntries);
	LastModified = FDateTime::Now().ToUnixTimestamp();
	UpdateChecksum();
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Compacted journal for tile (%d, %d): %d -> %d entries"), 
		TileCoord.X, TileCoord.Y, OriginalCount, Entries.Num());
}

// UInstancePersistenceManager implementation

UInstancePersistenceManager::UInstancePersistenceManager()
{
	PersistenceDirectory = FPaths::ProjectSavedDir() / TEXT("WorldGen") / TEXT("InstanceData");
}

bool UInstancePersistenceManager::Initialize(const FWorldGenConfig& Settings)
{
	WorldGenSettings = Settings;
	
	// Create persistence directory if it doesn't exist
	if (!EnsurePersistenceDirectory())
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Failed to create persistence directory: %s"), *PersistenceDirectory);
		return false;
	}
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Instance persistence manager initialized with directory: %s"), *PersistenceDirectory);
	return true;
}

bool UInstancePersistenceManager::SaveTileJournal(FTileCoord TileCoord)
{
	double StartTime = FPlatformTime::Seconds();
	
	const FTileInstanceJournal* Journal = JournalCache.Find(TileCoord);
	if (!Journal)
	{
		// No journal exists, nothing to save
		return true;
	}
	
	FString FilePath = GetTileJournalPath(TileCoord);
	
	// Serialize journal to binary data
	TArray<uint8> SerializedData;
	if (!SerializeJournal(*Journal, SerializedData))
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Failed to serialize journal for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return false;
	}
	
	// Write to file
	if (!FFileHelper::SaveArrayToFile(SerializedData, *FilePath))
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Failed to save journal file: %s"), *FilePath);
		return false;
	}
	
	// Remove from dirty set
	DirtyJournals.Remove(TileCoord);
	
	double EndTime = FPlatformTime::Seconds();
	TotalSaveTimeMs += (EndTime - StartTime) * 1000.0f;
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Saved journal for tile (%d, %d) to %s (%d entries, %d bytes)"), 
		TileCoord.X, TileCoord.Y, *FilePath, Journal->Entries.Num(), SerializedData.Num());
	
	return true;
}

bool UInstancePersistenceManager::LoadTileJournal(FTileCoord TileCoord)
{
	double StartTime = FPlatformTime::Seconds();
	
	FString FilePath = GetTileJournalPath(TileCoord);
	
	// Check if file exists
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		// No persistent data exists, create empty journal
		FTileInstanceJournal EmptyJournal(TileCoord);
		JournalCache.Add(TileCoord, EmptyJournal);
		return true;
	}
	
	// Load binary data from file
	TArray<uint8> SerializedData;
	if (!FFileHelper::LoadFileToArray(SerializedData, *FilePath))
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Failed to load journal file: %s"), *FilePath);
		return false;
	}
	
	// Deserialize journal
	FTileInstanceJournal LoadedJournal(TileCoord);
	if (!DeserializeJournal(SerializedData, LoadedJournal))
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Failed to deserialize journal for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		return false;
	}
	
	// Validate journal integrity
	if (!LoadedJournal.ValidateIntegrity())
	{
		UE_LOG(LogInstancePersistence, Warning, TEXT("Journal integrity validation failed for tile (%d, %d)"), TileCoord.X, TileCoord.Y);
		// Continue loading but log the issue
	}
	
	// Add to cache
	JournalCache.Add(TileCoord, LoadedJournal);
	TotalJournalEntries += LoadedJournal.Entries.Num();
	
	double EndTime = FPlatformTime::Seconds();
	TotalLoadTimeMs += (EndTime - StartTime) * 1000.0f;
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Loaded journal for tile (%d, %d) from %s (%d entries)"), 
		TileCoord.X, TileCoord.Y, *FilePath, LoadedJournal.Entries.Num());
	
	return true;
}

bool UInstancePersistenceManager::AddInstanceOperation(FTileCoord TileCoord, const FPCGInstanceData& InstanceData, EInstanceOperation Operation)
{
	FTileInstanceJournal& Journal = GetOrCreateTileJournal(TileCoord);
	
	FInstanceJournalEntry Entry(InstanceData, Operation);
	Journal.AddEntry(Entry);
	
	MarkJournalDirty(TileCoord);
	
	UE_LOG(LogInstancePersistence, Verbose, TEXT("Added instance operation %d for tile (%d, %d), instance %s"), 
		static_cast<int32>(Operation), TileCoord.X, TileCoord.Y, *InstanceData.InstanceId.ToString());
	
	return true;
}

bool UInstancePersistenceManager::AddPOIOperation(FTileCoord TileCoord, const FPOIData& POIData, EInstanceOperation Operation)
{
	FTileInstanceJournal& Journal = GetOrCreateTileJournal(TileCoord);
	
	FInstanceJournalEntry Entry(POIData, Operation);
	Journal.AddEntry(Entry);
	
	MarkJournalDirty(TileCoord);
	
	UE_LOG(LogInstancePersistence, Verbose, TEXT("Added POI operation %d for tile (%d, %d), POI %s"), 
		static_cast<int32>(Operation), TileCoord.X, TileCoord.Y, *POIData.POIId.ToString());
	
	return true;
}

bool UInstancePersistenceManager::ReplayTileJournal(FTileCoord TileCoord, UPCGWorldService* PCGService)
{
	if (!PCGService)
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Cannot replay journal - PCG service is null"));
		return false;
	}
	
	const FTileInstanceJournal* Journal = JournalCache.Find(TileCoord);
	if (!Journal || Journal->Entries.Num() == 0)
	{
		// No persistent modifications to replay
		return true;
	}
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Replaying journal for tile (%d, %d) - %d entries"), 
		TileCoord.X, TileCoord.Y, Journal->Entries.Num());
	
	// Get current PCG-generated content
	FPCGGenerationData GenerationData = PCGService->GenerateBiomeContent(TileCoord, EBiomeType::None, TArray<float>());
	
	// Apply journal entries to modify the generated content
	TMap<FGuid, FPCGInstanceData> InstanceMap;
	TMap<FGuid, FPOIData> POIMap;
	
	// Initialize with PCG-generated instances
	for (const FPCGInstanceData& Instance : GenerationData.GeneratedInstances)
	{
		InstanceMap.Add(Instance.InstanceId, Instance);
	}
	
	// Apply journal operations in chronological order
	for (const FInstanceJournalEntry& Entry : Journal->Entries)
	{
		if (Entry.bIsPOI)
		{
			// Handle POI operations
			switch (Entry.Operation)
			{
				case EInstanceOperation::Add:
				case EInstanceOperation::Modify:
					POIMap.Add(Entry.InstanceId, Entry.POIData);
					// Actually spawn the POI
					PCGService->SpawnPOI(Entry.POIData.Location, Entry.POIData);
					break;
					
				case EInstanceOperation::Remove:
					POIMap.Remove(Entry.InstanceId);
					// TODO: Remove spawned POI actor
					break;
			}
		}
		else
		{
			// Handle instance operations
			switch (Entry.Operation)
			{
				case EInstanceOperation::Add:
				case EInstanceOperation::Modify:
					InstanceMap.Add(Entry.InstanceId, Entry.InstanceData);
					break;
					
				case EInstanceOperation::Remove:
					InstanceMap.Remove(Entry.InstanceId);
					break;
			}
		}
	}
	
	// Update PCG generation data with modified instances
	GenerationData.GeneratedInstances.Empty();
	InstanceMap.GenerateValueArray(GenerationData.GeneratedInstances);
	GenerationData.TotalInstanceCount = GenerationData.GeneratedInstances.Num();
	
	// Update HISM instances to reflect changes
	PCGService->UpdateHISMInstances(TileCoord);
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Journal replay complete for tile (%d, %d) - %d instances, %d POIs"), 
		TileCoord.X, TileCoord.Y, GenerationData.GeneratedInstances.Num(), POIMap.Num());
	
	return true;
}

const FTileInstanceJournal* UInstancePersistenceManager::GetTileJournal(FTileCoord TileCoord) const
{
	return JournalCache.Find(TileCoord);
}

FTileInstanceJournal UInstancePersistenceManager::GetTileJournalData(FTileCoord TileCoord) const
{
	const FTileInstanceJournal* Journal = JournalCache.Find(TileCoord);
	if (Journal)
	{
		return *Journal;
	}
	
	// Return empty journal if not found
	return FTileInstanceJournal(TileCoord);
}

bool UInstancePersistenceManager::HasPersistentModifications(FTileCoord TileCoord) const
{
	const FTileInstanceJournal* Journal = JournalCache.Find(TileCoord);
	return Journal && Journal->Entries.Num() > 0;
}

void UInstancePersistenceManager::CompactAllJournals()
{
	int32 TotalEntriesBefore = 0;
	int32 TotalEntriesAfter = 0;
	
	for (auto& JournalPair : JournalCache)
	{
		FTileInstanceJournal& Journal = JournalPair.Value;
		TotalEntriesBefore += Journal.Entries.Num();
		
		Journal.CompactJournal();
		TotalEntriesAfter += Journal.Entries.Num();
		
		// Mark as dirty so it gets saved
		MarkJournalDirty(JournalPair.Key);
	}
	
	// Save all dirty journals
	SaveDirtyJournals();
	
	UE_LOG(LogInstancePersistence, Log, TEXT("Compacted all journals: %d -> %d entries (%.1f%% reduction)"), 
		TotalEntriesBefore, TotalEntriesAfter, 
		TotalEntriesBefore > 0 ? (100.0f * (TotalEntriesBefore - TotalEntriesAfter) / TotalEntriesBefore) : 0.0f);
}

FString UInstancePersistenceManager::GetPersistenceStats() const
{
	int32 TotalJournals = JournalCache.Num();
	int32 TotalModifications = 0;
	int32 DirtyCount = DirtyJournals.Num();
	
	for (const auto& JournalPair : JournalCache)
	{
		TotalModifications += JournalPair.Value.Entries.Num();
	}
	
	return FString::Printf(
		TEXT("Instance Persistence Stats:\n")
		TEXT("- Total Journals: %d\n")
		TEXT("- Total Modifications: %d\n")
		TEXT("- Dirty Journals: %d\n")
		TEXT("- Average Load Time: %.2fms\n")
		TEXT("- Average Save Time: %.2fms\n")
		TEXT("- Persistence Directory: %s"),
		TotalJournals, TotalModifications, DirtyCount,
		TotalLoadTimeMs / FMath::Max(1, TotalJournals),
		TotalSaveTimeMs / FMath::Max(1, TotalJournals),
		*PersistenceDirectory
	);
}

bool UInstancePersistenceManager::ValidateAllJournals(TArray<FString>& OutErrors) const
{
	OutErrors.Empty();
	
	// Validate cached journals
	for (const auto& JournalPair : JournalCache)
	{
		const FTileInstanceJournal& Journal = JournalPair.Value;
		if (!Journal.ValidateIntegrity())
		{
			OutErrors.Add(FString::Printf(TEXT("Journal integrity validation failed for tile (%d, %d)"), 
				JournalPair.Key.X, JournalPair.Key.Y));
		}
	}
	
	// Validate journal files on disk
	TArray<FString> JournalFiles;
	IFileManager::Get().FindFiles(JournalFiles, *(PersistenceDirectory / TEXT("*.inst")), true, false);
	
	for (const FString& FileName : JournalFiles)
	{
		FString FilePath = PersistenceDirectory / FileName;
		TArray<uint8> FileData;
		
		if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
		{
			OutErrors.Add(FString::Printf(TEXT("Failed to load journal file: %s"), *FileName));
			continue;
		}
		
		// Try to deserialize and validate
		FTileInstanceJournal TempJournal;
		if (!DeserializeJournal(FileData, TempJournal))
		{
			OutErrors.Add(FString::Printf(TEXT("Failed to deserialize journal file: %s"), *FileName));
			continue;
		}
		
		if (!TempJournal.ValidateIntegrity())
		{
			OutErrors.Add(FString::Printf(TEXT("Journal file integrity validation failed: %s"), *FileName));
		}
	}
	
	return OutErrors.Num() == 0;
}

// Private methods

FString UInstancePersistenceManager::GetTileJournalPath(FTileCoord TileCoord) const
{
	return PersistenceDirectory / FString::Printf(TEXT("tile_%d_%d.inst"), TileCoord.X, TileCoord.Y);
}

bool UInstancePersistenceManager::SerializeJournal(const FTileInstanceJournal& Journal, TArray<uint8>& OutData) const
{
	// Use UE5's built-in serialization with compression support
	FMemoryWriter Writer(OutData);
	FObjectAndNameAsStringProxyArchive StringArchive(Writer, true);
	
	// Write file header
	uint32 MagicNumber = 0x494E5354; // 'INST' in hex
	StringArchive << const_cast<uint32&>(MagicNumber);
	
	int32 FileVersion = 1;
	StringArchive << FileVersion;
	
	// Write journal data
	StringArchive << const_cast<FTileInstanceJournal&>(Journal);
	
	return !Writer.IsError();
}

bool UInstancePersistenceManager::DeserializeJournal(const TArray<uint8>& InData, FTileInstanceJournal& OutJournal) const
{
	if (InData.Num() == 0)
	{
		return false;
	}
	
	FMemoryReader Reader(const_cast<TArray<uint8>&>(InData));
	FObjectAndNameAsStringProxyArchive StringArchive(Reader, true);
	
	// Read and validate file header
	uint32 MagicNumber = 0;
	StringArchive << MagicNumber;
	
	if (MagicNumber != 0x494E5354)
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Invalid journal file format - wrong magic number"));
		return false;
	}
	
	int32 FileVersion = 0;
	StringArchive << FileVersion;
	
	if (FileVersion > 1)
	{
		UE_LOG(LogInstancePersistence, Error, TEXT("Unsupported journal file version: %d"), FileVersion);
		return false;
	}
	
	// Read journal data
	StringArchive << OutJournal;
	
	return !Reader.IsError();
}

FTileInstanceJournal& UInstancePersistenceManager::GetOrCreateTileJournal(FTileCoord TileCoord)
{
	FTileInstanceJournal* ExistingJournal = JournalCache.Find(TileCoord);
	if (ExistingJournal)
	{
		return *ExistingJournal;
	}
	
	// Create new journal
	FTileInstanceJournal NewJournal(TileCoord);
	return JournalCache.Add(TileCoord, NewJournal);
}

void UInstancePersistenceManager::MarkJournalDirty(FTileCoord TileCoord)
{
	DirtyJournals.Add(TileCoord);
}

bool UInstancePersistenceManager::SaveDirtyJournals()
{
	bool bAllSaved = true;
	
	for (const FTileCoord& TileCoord : DirtyJournals)
	{
		if (!SaveTileJournal(TileCoord))
		{
			bAllSaved = false;
		}
	}
	
	// DirtyJournals will be cleared by individual SaveTileJournal calls
	return bAllSaved;
}

bool UInstancePersistenceManager::EnsurePersistenceDirectory()
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*PersistenceDirectory))
	{
		return PlatformFile.CreateDirectoryTree(*PersistenceDirectory);
	}
	
	return true;
}
