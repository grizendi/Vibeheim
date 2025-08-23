#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Data/WorldGenTypes.h"
#include "Data/InstancePersistence.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"

/**
 * Test file-based persistence for struct serialization compatibility
 * Validates that structs can be saved to and loaded from disk correctly
 */
class FFilePersistenceTest : public FAutomationTestBase
{
public:
    FFilePersistenceTest(const FString& InName, const bool bInComplexTask)
        : FAutomationTestBase(InName, bInComplexTask)
    {
    }

    virtual uint32 GetTestFlags() const override
    {
        return EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter;
    }

    virtual bool SuppressLogWarnings() override
    {
        return true;
    }

    virtual FString GetBeautifiedTestName() const override
    {
        return "Vibeheim.WorldGen.FilePersistence";
    }

protected:
    virtual bool RunTest(const FString& Parameters) override;

private:
    void TestHeightfieldModificationFilePersistence();
    void TestInstanceJournalFilePersistence();
    void TestPOIDataFilePersistence();
    void TestPCGInstanceDataFilePersistence();
    void TestCompressedSerialization();
    void TestBackwardCompatibility();
    
    FString GetTestDataDirectory() const;
    void CleanupTestFiles();
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FFilePersistenceTest, FFilePersistenceTest, 
    "Vibeheim.WorldGen.FilePersistence", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFilePersistenceTest::RunTest(const FString& Parameters)
{
    // Ensure test directory exists
    FString TestDir = GetTestDataDirectory();
    IFileManager::Get().MakeDirectory(*TestDir, true);
    
    // Run all file persistence tests
    TestHeightfieldModificationFilePersistence();
    TestInstanceJournalFilePersistence();
    TestPOIDataFilePersistence();
    TestPCGInstanceDataFilePersistence();
    TestCompressedSerialization();
    TestBackwardCompatibility();
    
    // Cleanup test files
    CleanupTestFiles();
    
    return true;
}

void FFilePersistenceTest::TestHeightfieldModificationFilePersistence()
{
    // Create test data
    TArray<FHeightfieldModification> OriginalModifications;
    
    for (int32 i = 0; i < 5; ++i)
    {
        FHeightfieldModification Mod;
        Mod.Center = FVector2D(i * 100.0f, i * 200.0f);
        Mod.Radius = 50.0f + i * 10.0f;
        Mod.Strength = 0.5f + i * 0.1f;
        Mod.Operation = static_cast<EHeightfieldOperation>(i % 4);
        Mod.AffectedTile = FTileCoord(i, i + 1);
        OriginalModifications.Add(Mod);
    }
    
    // Save to file
    FString FilePath = GetTestDataDirectory() / TEXT("HeightfieldMods.dat");
    TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*FilePath));
    TestTrue("File writer should be created", FileWriter.IsValid());
    
    if (FileWriter.IsValid())
    {
        *FileWriter << OriginalModifications;
        FileWriter->Close();
    }
    
    // Load from file
    TArray<FHeightfieldModification> LoadedModifications;
    TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*FilePath));
    TestTrue("File reader should be created", FileReader.IsValid());
    
    if (FileReader.IsValid())
    {
        *FileReader << LoadedModifications;
        FileReader->Close();
    }
    
    // Validate data integrity
    TestEqual("Number of modifications should match", LoadedModifications.Num(), OriginalModifications.Num());
    
    for (int32 i = 0; i < OriginalModifications.Num(); ++i)
    {
        const FHeightfieldModification& Original = OriginalModifications[i];
        const FHeightfieldModification& Loaded = LoadedModifications[i];
        
        TestEqual(FString::Printf(TEXT("ModificationId[%d] should be preserved"), i), 
            Loaded.ModificationId, Original.ModificationId);
        TestEqual(FString::Printf(TEXT("Center[%d] should match"), i), 
            Loaded.Center, Original.Center);
        TestEqual(FString::Printf(TEXT("Radius[%d] should match"), i), 
            Loaded.Radius, Original.Radius);
        TestEqual(FString::Printf(TEXT("Operation[%d] should match"), i), 
            Loaded.Operation, Original.Operation);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FHeightfieldModification file persistence test passed"));
}

void FFilePersistenceTest::TestInstanceJournalFilePersistence()
{
    // Create test journal
    FTileInstanceJournal OriginalJournal(FTileCoord(5, 10));
    
    // Add some entries
    for (int32 i = 0; i < 3; ++i)
    {
        FInstanceJournalEntry Entry;
        Entry.Operation = static_cast<EInstanceOperation>(i % 3);
        Entry.Timestamp = FDateTime::Now().ToUnixTimestamp() + i;
        Entry.bIsPOI = (i % 2 == 0);
        Entry.Version = 1;
        
        OriginalJournal.AddEntry(Entry);
    }
    
    // Save to file
    FString FilePath = GetTestDataDirectory() / TEXT("InstanceJournal.dat");
    TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*FilePath));
    TestTrue("File writer should be created", FileWriter.IsValid());
    
    if (FileWriter.IsValid())
    {
        *FileWriter << OriginalJournal;
        FileWriter->Close();
    }
    
    // Load from file
    FTileInstanceJournal LoadedJournal;
    TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*FilePath));
    TestTrue("File reader should be created", FileReader.IsValid());
    
    if (FileReader.IsValid())
    {
        *FileReader << LoadedJournal;
        FileReader->Close();
    }
    
    // Validate data integrity
    TestEqual("TileCoord should match", LoadedJournal.TileCoord, OriginalJournal.TileCoord);
    TestEqual("Number of entries should match", LoadedJournal.Entries.Num(), OriginalJournal.Entries.Num());
    
    for (int32 i = 0; i < OriginalJournal.Entries.Num(); ++i)
    {
        const FInstanceJournalEntry& Original = OriginalJournal.Entries[i];
        const FInstanceJournalEntry& Loaded = LoadedJournal.Entries[i];
        
        TestEqual(FString::Printf(TEXT("InstanceId[%d] should be preserved"), i), 
            Loaded.InstanceId, Original.InstanceId);
        TestEqual(FString::Printf(TEXT("Operation[%d] should match"), i), 
            Loaded.Operation, Original.Operation);
        TestEqual(FString::Printf(TEXT("Timestamp[%d] should match"), i), 
            Loaded.Timestamp, Original.Timestamp);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FInstanceJournalEntry file persistence test passed"));
}void FF
ilePersistenceTest::TestPOIDataFilePersistence()
{
    // Create test POI data
    TArray<FPOIData> OriginalPOIs;
    
    for (int32 i = 0; i < 3; ++i)
    {
        FPOIData POI;
        POI.POIName = FString::Printf(TEXT("TestPOI_%d"), i);
        POI.Location = FVector(i * 100.0f, i * 200.0f, i * 50.0f);
        POI.Rotation = FRotator(i * 10.0f, i * 20.0f, i * 5.0f);
        POI.Scale = FVector(1.0f + i * 0.1f);
        POI.OriginBiome = static_cast<EBiomeType>((i % 4) + 1);
        POI.bIsSpawned = (i % 2 == 0);
        OriginalPOIs.Add(POI);
    }
    
    // Save to file using custom serialization
    FString FilePath = GetTestDataDirectory() / TEXT("POIData.dat");
    TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*FilePath));
    TestTrue("File writer should be created", FileWriter.IsValid());
    
    if (FileWriter.IsValid())
    {
        int32 NumPOIs = OriginalPOIs.Num();
        *FileWriter << NumPOIs;
        
        for (FPOIData& POI : OriginalPOIs)
        {
            POI.Serialize(*FileWriter);
        }
        FileWriter->Close();
    }
    
    // Load from file using custom serialization
    TArray<FPOIData> LoadedPOIs;
    TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*FilePath));
    TestTrue("File reader should be created", FileReader.IsValid());
    
    if (FileReader.IsValid())
    {
        int32 NumPOIs = 0;
        *FileReader << NumPOIs;
        
        LoadedPOIs.Reserve(NumPOIs);
        for (int32 i = 0; i < NumPOIs; ++i)
        {
            FPOIData POI;
            POI.Serialize(*FileReader);
            LoadedPOIs.Add(POI);
        }
        FileReader->Close();
    }
    
    // Validate data integrity
    TestEqual("Number of POIs should match", LoadedPOIs.Num(), OriginalPOIs.Num());
    
    for (int32 i = 0; i < OriginalPOIs.Num(); ++i)
    {
        const FPOIData& Original = OriginalPOIs[i];
        const FPOIData& Loaded = LoadedPOIs[i];
        
        TestEqual(FString::Printf(TEXT("POIId[%d] should be preserved"), i), 
            Loaded.POIId, Original.POIId);
        TestEqual(FString::Printf(TEXT("POIName[%d] should match"), i), 
            Loaded.POIName, Original.POIName);
        TestEqual(FString::Printf(TEXT("Location[%d] should match"), i), 
            Loaded.Location, Original.Location);
        TestEqual(FString::Printf(TEXT("OriginBiome[%d] should match"), i), 
            Loaded.OriginBiome, Original.OriginBiome);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPOIData file persistence test passed"));
}

void FFilePersistenceTest::TestPCGInstanceDataFilePersistence()
{
    // Create test instance data
    TArray<FPCGInstanceData> OriginalInstances;
    
    for (int32 i = 0; i < 4; ++i)
    {
        FPCGInstanceData Instance;
        Instance.Location = FVector(i * 50.0f, i * 75.0f, i * 25.0f);
        Instance.Rotation = FRotator(i * 15.0f, i * 30.0f, i * 7.5f);
        Instance.Scale = FVector(0.8f + i * 0.05f);
        Instance.bIsActive = (i % 2 == 1);
        Instance.OwningTile = FTileCoord(i, i * 2);
        OriginalInstances.Add(Instance);
    }
    
    // Save to file using custom serialization
    FString FilePath = GetTestDataDirectory() / TEXT("PCGInstanceData.dat");
    TUniquePtr<FArchive> FileWriter(IFileManager::Get().CreateFileWriter(*FilePath));
    TestTrue("File writer should be created", FileWriter.IsValid());
    
    if (FileWriter.IsValid())
    {
        int32 NumInstances = OriginalInstances.Num();
        *FileWriter << NumInstances;
        
        for (FPCGInstanceData& Instance : OriginalInstances)
        {
            Instance.Serialize(*FileWriter);
        }
        FileWriter->Close();
    }
    
    // Load from file using custom serialization
    TArray<FPCGInstanceData> LoadedInstances;
    TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*FilePath));
    TestTrue("File reader should be created", FileReader.IsValid());
    
    if (FileReader.IsValid())
    {
        int32 NumInstances = 0;
        *FileReader << NumInstances;
        
        LoadedInstances.Reserve(NumInstances);
        for (int32 i = 0; i < NumInstances; ++i)
        {
            FPCGInstanceData Instance;
            Instance.Serialize(*FileReader);
            LoadedInstances.Add(Instance);
        }
        FileReader->Close();
    }
    
    // Validate data integrity
    TestEqual("Number of instances should match", LoadedInstances.Num(), OriginalInstances.Num());
    
    for (int32 i = 0; i < OriginalInstances.Num(); ++i)
    {
        const FPCGInstanceData& Original = OriginalInstances[i];
        const FPCGInstanceData& Loaded = LoadedInstances[i];
        
        TestEqual(FString::Printf(TEXT("InstanceId[%d] should be preserved"), i), 
            Loaded.InstanceId, Original.InstanceId);
        TestEqual(FString::Printf(TEXT("Location[%d] should match"), i), 
            Loaded.Location, Original.Location);
        TestEqual(FString::Printf(TEXT("bIsActive[%d] should match"), i), 
            Loaded.bIsActive, Original.bIsActive);
        TestEqual(FString::Printf(TEXT("OwningTile[%d] should match"), i), 
            Loaded.OwningTile, Original.OwningTile);
    }
    
    UE_LOG(LogTemp, Log, TEXT("FPCGInstanceData file persistence test passed"));
}

void FFilePersistenceTest::TestCompressedSerialization()
{
    // Create large dataset for compression testing
    TArray<FHeightfieldModification> LargeDataset;
    LargeDataset.Reserve(100);
    
    for (int32 i = 0; i < 100; ++i)
    {
        FHeightfieldModification Mod;
        Mod.Center = FVector2D(FMath::RandRange(-1000.0f, 1000.0f), FMath::RandRange(-1000.0f, 1000.0f));
        Mod.Radius = FMath::RandRange(10.0f, 100.0f);
        Mod.Strength = FMath::RandRange(0.1f, 1.0f);
        Mod.Operation = static_cast<EHeightfieldOperation>(i % 4);
        Mod.AffectedTile = FTileCoord(i / 10, i % 10);
        LargeDataset.Add(Mod);
    }
    
    // Save with compression
    FString CompressedFilePath = GetTestDataDirectory() / TEXT("CompressedMods.dat");
    TUniquePtr<FArchive> CompressedWriter(IFileManager::Get().CreateFileWriter(*CompressedFilePath));
    TestTrue("Compressed file writer should be created", CompressedWriter.IsValid());
    
    if (CompressedWriter.IsValid())
    {
        FArchiveSaveCompressedProxy CompressedArchive(*CompressedWriter, NAME_Zlib);
        CompressedArchive << LargeDataset;
        CompressedArchive.Close();
        CompressedWriter->Close();
    }
    
    // Load with decompression
    TArray<FHeightfieldModification> DecompressedDataset;
    TUniquePtr<FArchive> CompressedReader(IFileManager::Get().CreateFileReader(*CompressedFilePath));
    TestTrue("Compressed file reader should be created", CompressedReader.IsValid());
    
    if (CompressedReader.IsValid())
    {
        FArchiveLoadCompressedProxy DecompressedArchive(*CompressedReader, NAME_Zlib);
        DecompressedArchive << DecompressedDataset;
        DecompressedArchive.Close();
        CompressedReader->Close();
    }
    
    // Validate compressed data integrity
    TestEqual("Compressed dataset size should match", DecompressedDataset.Num(), LargeDataset.Num());
    
    for (int32 i = 0; i < LargeDataset.Num(); ++i)
    {
        TestEqual(FString::Printf(TEXT("Compressed ModificationId[%d] should be preserved"), i), 
            DecompressedDataset[i].ModificationId, LargeDataset[i].ModificationId);
        TestEqual(FString::Printf(TEXT("Compressed Center[%d] should match"), i), 
            DecompressedDataset[i].Center, LargeDataset[i].Center);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Compressed serialization test passed"));
}

void FFilePersistenceTest::TestBackwardCompatibility()
{
    // This test simulates loading data that was saved before the struct initialization fixes
    // In practice, this would involve creating test data files with the old format
    
    // Create a struct with a manually set GUID to simulate old data
    FHeightfieldModification OldFormatMod;
    OldFormatMod.Center = FVector2D(500.0f, 600.0f);
    OldFormatMod.Radius = 75.0f;
    OldFormatMod.Strength = 0.9f;
    OldFormatMod.Operation = EHeightfieldOperation::Flatten;
    OldFormatMod.AffectedTile = FTileCoord(10, 15);
    
    // Manually set a specific GUID to simulate old save data
    FGuid OldGuid = FGuid(0x12345678, 0x9ABCDEF0, 0x11111111, 0x22222222);
    OldFormatMod.ModificationId = OldGuid;
    
    // Save this "old format" data
    FString BackwardCompatFilePath = GetTestDataDirectory() / TEXT("BackwardCompatMods.dat");
    TUniquePtr<FArchive> BackwardWriter(IFileManager::Get().CreateFileWriter(*BackwardCompatFilePath));
    TestTrue("Backward compatibility file writer should be created", BackwardWriter.IsValid());
    
    if (BackwardWriter.IsValid())
    {
        *BackwardWriter << OldFormatMod;
        BackwardWriter->Close();
    }
    
    // Load using current struct implementation
    FHeightfieldModification LoadedOldFormatMod;
    TUniquePtr<FArchive> BackwardReader(IFileManager::Get().CreateFileReader(*BackwardCompatFilePath));
    TestTrue("Backward compatibility file reader should be created", BackwardReader.IsValid());
    
    if (BackwardReader.IsValid())
    {
        *BackwardReader << LoadedOldFormatMod;
        BackwardReader->Close();
    }
    
    // Validate that old GUID values are preserved
    TestEqual("Old format GUID should be preserved", LoadedOldFormatMod.ModificationId, OldGuid);
    TestEqual("Old format Center should match", LoadedOldFormatMod.Center, OldFormatMod.Center);
    TestEqual("Old format Operation should match", LoadedOldFormatMod.Operation, OldFormatMod.Operation);
    
    UE_LOG(LogTemp, Log, TEXT("Backward compatibility test passed"));
}

FString FFilePersistenceTest::GetTestDataDirectory() const
{
    return FPaths::ProjectSavedDir() / TEXT("Tests") / TEXT("SerializationCompatibility");
}

void FFilePersistenceTest::CleanupTestFiles()
{
    FString TestDir = GetTestDataDirectory();
    IFileManager::Get().DeleteDirectory(*TestDir, false, true);
}