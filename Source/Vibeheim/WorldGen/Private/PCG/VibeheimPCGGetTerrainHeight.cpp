#include "PCG/VibeheimPCGGetTerrainHeight.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "WorldGenManager.h"
#include "Services/HeightfieldService.h"

UVibeheimPCGGetTerrainHeightSettings::UVibeheimPCGGetTerrainHeightSettings()
{
}

TArray<FPCGPinProperties> UVibeheimPCGGetTerrainHeightSettings::InputPinProperties() const
{
        TArray<FPCGPinProperties> Pins;
        Pins.Emplace(FName(TEXT("In")), EPCGDataType::Point, true);
        return Pins;
}

TArray<FPCGPinProperties> UVibeheimPCGGetTerrainHeightSettings::OutputPinProperties() const
{
        TArray<FPCGPinProperties> Pins;
        Pins.Emplace(FName(TEXT("Out")), EPCGDataType::Point, true, true);
        return Pins;
}

FName UVibeheimPCGGetTerrainHeightSettings::GetDefaultNodeName() const
{
        return FName(TEXT("GetTerrainHeight"));
}

FText UVibeheimPCGGetTerrainHeightSettings::GetDefaultNodeTitle() const
{
        return NSLOCTEXT("VibeheimPCG", "GetTerrainHeightNodeTitle", "Get Terrain Height");
}

#if WITH_EDITOR
FText UVibeheimPCGGetTerrainHeightSettings::GetNodeTooltipText() const
{
        return NSLOCTEXT("VibeheimPCG", "GetTerrainHeightTooltip",
                "Samples the Vibeheim heightfield to annotate points with TerrainHeight/TerrainSlope metadata.");
}
#endif

FPCGElementPtr UVibeheimPCGGetTerrainHeightSettings::CreateElement() const
{
        return MakeShared<FPCGVibeheimGetTerrainHeightElement>();
}

static UHeightfieldService* ResolveHeightfieldService(UWorld* World)
{
        if (!World)
        {
                return nullptr;
        }

        for (TActorIterator<AWorldGenManager> It(World); It; ++It)
        {
                if (AWorldGenManager* Manager = *It)
                {
                        return Manager->GetHeightfieldService();
                }
        }

        return nullptr;
}

bool FPCGVibeheimGetTerrainHeightElement::ExecuteInternal(FPCGContext* Context) const
{
        check(Context);

        const UVibeheimPCGGetTerrainHeightSettings* Settings = Context->GetInputSettings<UVibeheimPCGGetTerrainHeightSettings>();
        if (!Settings)
        {
                return true;
        }

        UPCGComponent* SourceComponent = nullptr;

        if (Context->SourceComponent.IsValid())
        {
                SourceComponent = Context->SourceComponent.Get();
        }
        else if (Context->ExecutionSource.IsValid())
        {
                if (UObject* SourceObject = Context->ExecutionSource.GetObject())
                {
                        SourceComponent = Cast<UPCGComponent>(SourceObject);
                }
        }

        UWorld* World = SourceComponent ? SourceComponent->GetWorld() : nullptr;
        UHeightfieldService* HeightfieldService = ResolveHeightfieldService(World);

        if (!HeightfieldService)
        {
                UE_LOG(LogTemp, Warning, TEXT("GetTerrainHeight: Heightfield service unavailable; passing through input points."));
        }

        const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetAllInputs();
        for (const FPCGTaggedData& Input : Inputs)
        {
                const UPCGPointData* InPointData = Cast<UPCGPointData>(Input.Data);
                if (!InPointData)
                {
                        FPCGTaggedData& Passthrough = Context->OutputData.TaggedData.AddDefaulted_GetRef();
                        Passthrough = Input;
                        continue;
                }

                UPCGPointData* OutPointData = DuplicateObject<UPCGPointData>(InPointData, InPointData->GetOuter());
                if (!OutPointData)
                {
                        continue;
                }

                TArray<FPCGPoint>& Points = OutPointData->GetMutablePoints();
                UPCGMetadata* Metadata = OutPointData->MutableMetadata();

                FPCGMetadataAttribute<float>* HeightAttr = nullptr;
                FPCGMetadataAttribute<float>* SlopeAttr = nullptr;

                if (Metadata)
                {
                        if (!Settings->HeightAttribute.IsNone())
                        {
                                HeightAttr = Metadata->GetMutableTypedAttribute<float>(Settings->HeightAttribute);
                                if (!HeightAttr)
                                {
                                        HeightAttr = Metadata->CreateAttribute<float>(Settings->HeightAttribute, 0.0f, true, true);
                                }
                        }

                        if (!Settings->SlopeAttribute.IsNone())
                        {
                                SlopeAttr = Metadata->GetMutableTypedAttribute<float>(Settings->SlopeAttribute);
                                if (!SlopeAttr)
                                {
                                        SlopeAttr = Metadata->CreateAttribute<float>(Settings->SlopeAttribute, 0.0f, true, true);
                                }
                        }
                }

                for (FPCGPoint& Point : Points)
                {
                        const FVector Location = Point.Transform.GetLocation();
                        const FVector2D WorldXY(Location.X, Location.Y);

                        float HeightSample = Location.Z;
                        float SlopeSample = 0.0f;

                        if (HeightfieldService)
                        {
                                const float SampledHeight = HeightfieldService->SampleHeightWorldXY(WorldXY);
                                if (FMath::IsFinite(SampledHeight))
                                {
                                        HeightSample = SampledHeight;
                                }

                                const float SampledSlope = HeightfieldService->GetSlopeAtLocation(WorldXY);
                                if (FMath::IsFinite(SampledSlope))
                                {
                                        SlopeSample = SampledSlope;
                                }
                        }

                        if (HeightAttr && Point.MetadataEntry != PCGInvalidEntryKey)
                        {
                                HeightAttr->SetValue(Point.MetadataEntry, HeightSample);
                        }

                        if (SlopeAttr && Point.MetadataEntry != PCGInvalidEntryKey)
                        {
                                SlopeAttr->SetValue(Point.MetadataEntry, SlopeSample);
                        }

                        if (Settings->bProjectPointZ && HeightfieldService && FMath::IsFinite(HeightSample))
                        {
                                FVector ProjectedLocation = Location;
                                ProjectedLocation.Z = HeightSample;
                                Point.Transform.SetLocation(ProjectedLocation);
                        }
                }

                FPCGTaggedData& Output = Context->OutputData.TaggedData.AddDefaulted_GetRef();
                Output = Input;
                Output.Data = OutPointData;
        }

        return true;
}
