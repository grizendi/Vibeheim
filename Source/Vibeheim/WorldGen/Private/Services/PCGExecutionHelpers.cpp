#include "Services/PCGExecutionHelpers.h"

#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "PCGGraphExecutionInspection.h"
#include "Utils/PCGExtraCapture.h"

FVibeheimPCGInputElement::FVibeheimPCGInputElement(TSharedRef<const FPCGDataCollection> InData)
	: InputData(MoveTemp(InData))
{
}

bool FVibeheimPCGInputElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	Context->OutputData = *InputData;
	return true;
}

UVibeheimPCGExecutionSource::FExecutionState::FExecutionState(UVibeheimPCGExecutionSource& InOwner)
	: Owner(InOwner)
{
}

UPCGData* UVibeheimPCGExecutionSource::FExecutionState::GetSelfData() const
{
	return Owner.ParameterData.Get();
}

int32 UVibeheimPCGExecutionSource::FExecutionState::GetSeed() const
{
	return Owner.Seed;
}

FString UVibeheimPCGExecutionSource::FExecutionState::GetDebugName() const
{
	return Owner.DebugName.IsEmpty() ? TEXT("VibeheimPCGExecution") : Owner.DebugName;
}

UWorld* UVibeheimPCGExecutionSource::FExecutionState::GetWorld() const
{
	return Owner.World;
}

bool UVibeheimPCGExecutionSource::FExecutionState::HasAuthority() const
{
	return true;
}

FTransform UVibeheimPCGExecutionSource::FExecutionState::GetTransform() const
{
	return Owner.Transform;
}

FBox UVibeheimPCGExecutionSource::FExecutionState::GetBounds() const
{
	return Owner.Bounds;
}

UPCGGraph* UVibeheimPCGExecutionSource::FExecutionState::GetGraph() const
{
	if (Owner.GraphInstance.IsValid())
	{
		return Owner.GraphInstance->GetGraph();
	}

	return Owner.Graph;
}

UPCGGraphInstance* UVibeheimPCGExecutionSource::FExecutionState::GetGraphInstance() const
{
	return Owner.GraphInstance.Get();
}

void UVibeheimPCGExecutionSource::FExecutionState::Cancel()
{
	Owner.bCancelled = true;
}

void UVibeheimPCGExecutionSource::FExecutionState::OnGraphExecutionAborted(bool /*bQuiet*/, bool /*bCleanupUnusedResources*/)
{
	Owner.bAborted = true;
}

#if WITH_EDITOR
const PCGUtils::FExtraCapture& UVibeheimPCGExecutionSource::FExecutionState::GetExtraCapture() const
{
	check(Owner.ExtraCapture);
	return *Owner.ExtraCapture;
}

PCGUtils::FExtraCapture& UVibeheimPCGExecutionSource::FExecutionState::GetExtraCapture()
{
	if (!Owner.ExtraCapture)
	{
		Owner.ExtraCapture = new PCGUtils::FExtraCapture();
	}

	return *Owner.ExtraCapture;
}

const FPCGGraphExecutionInspection& UVibeheimPCGExecutionSource::FExecutionState::GetInspection() const
{
	check(Owner.Inspection);
	return *Owner.Inspection;
}

FPCGGraphExecutionInspection& UVibeheimPCGExecutionSource::FExecutionState::GetInspection()
{
	if (!Owner.Inspection)
	{
		Owner.Inspection = new FPCGGraphExecutionInspection();
	}

	return *Owner.Inspection;
}
#endif // WITH_EDITOR

void UVibeheimPCGExecutionSource::Initialize(
	UWorld* InWorld,
	UPCGGraph* InGraph,
	UPCGParamData* InParameterData,
	UPCGPointData* InPointData,
	int32 InSeed,
	const FString& InDebugName,
	const FTransform& InTransform,
	const FBox& InBounds)
{
	World = InWorld;
	Graph = InGraph;
	Seed = InSeed;
	DebugName = InDebugName;
	Transform = InTransform;
	Bounds = InBounds;
	bCancelled = false;
	bAborted = false;

	ParameterData = TStrongObjectPtr<UPCGParamData>(InParameterData);
	PointData = TStrongObjectPtr<UPCGPointData>(InPointData);

	GraphInstance.Reset();
	if (Graph)
	{
		if (TObjectPtr<UPCGGraphInterface> GraphInterface = UPCGGraphInstance::CreateInstance(this, Graph))
		{
			if (UPCGGraphInstance* Instance = Cast<UPCGGraphInstance>(GraphInterface.Get()))
			{
				GraphInstance = TStrongObjectPtr<UPCGGraphInstance>(Instance);
			}
		}
	}

#if WITH_EDITOR
	if (!ExtraCapture)
	{
		ExtraCapture = new PCGUtils::FExtraCapture();
	}

	if (!Inspection)
	{
		Inspection = new FPCGGraphExecutionInspection();
	}
#endif
}

void UVibeheimPCGExecutionSource::Release()
{
	GraphInstance.Reset();
	ParameterData.Reset();
	PointData.Reset();
	World = nullptr;
	Graph = nullptr;
	Bounds = FBox(EForceInit::ForceInit);
	Transform = FTransform::Identity;
	Seed = 0;
	DebugName.Reset();
	bCancelled = false;
	bAborted = false;

#if WITH_EDITOR
	delete ExtraCapture;
	ExtraCapture = nullptr;

	delete Inspection;
	Inspection = nullptr;
#endif
}

void UVibeheimPCGExecutionSource::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}



