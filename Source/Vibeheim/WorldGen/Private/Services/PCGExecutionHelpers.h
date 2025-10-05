#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "PCGGraph.h"
#include "PCGElement.h"
#include "PCGGraphExecutionStateInterface.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"

#include "PCGExecutionHelpers.generated.h"

class UPCGGraph;
class UPCGGraphInstance;
class UPCGParamData;
class UPCGPointData;

namespace PCGUtils
{
	class FExtraCapture;
}

class FPCGGraphExecutionInspection;

/**
 * Lightweight element that injects a predetermined data collection into a scheduled PCG graph.
 */
class FVibeheimPCGInputElement : public IPCGElement
{
public:
	explicit FVibeheimPCGInputElement(TSharedRef<const FPCGDataCollection> InData);

	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

private:
	TSharedRef<const FPCGDataCollection> InputData;
};

/**
 * Minimal execution source/state pair used to drive scheduler-based graph execution outside of PCG components.
 */
UCLASS()
class UVibeheimPCGExecutionSource : public UObject, public IPCGGraphExecutionSource
{
	GENERATED_BODY()

public:
	void Initialize(UWorld* InWorld,
		UPCGGraph* InGraph,
		UPCGParamData* InParameterData,
		UPCGPointData* InPointData,
		int32 InSeed,
		const FString& InDebugName,
		const FTransform& InTransform,
		const FBox& InBounds);

	void Release();

	virtual void BeginDestroy() override;

	virtual IPCGGraphExecutionState& GetExecutionState() override { return ExecutionState; }
	virtual const IPCGGraphExecutionState& GetExecutionState() const override { return ExecutionState; }

	UPCGParamData* GetParameterData() const { return ParameterData.Get(); }
	UPCGPointData* GetPointData() const { return PointData.Get(); }

private:
	class FExecutionState final : public IPCGGraphExecutionState
	{
	public:
		explicit FExecutionState(UVibeheimPCGExecutionSource& InOwner);

		virtual UPCGData* GetSelfData() const override;
		virtual int32 GetSeed() const override;
		virtual FString GetDebugName() const override;
		virtual UWorld* GetWorld() const override;
		virtual bool HasAuthority() const override;
		virtual FTransform GetTransform() const override;
		virtual FBox GetBounds() const override;
		virtual UPCGGraph* GetGraph() const override;
		virtual UPCGGraphInstance* GetGraphInstance() const override;
		virtual void Cancel() override;
		virtual void OnGraphExecutionAborted(bool bQuiet = false, bool bCleanupUnusedResources = true) override;

#if WITH_EDITOR
		virtual const PCGUtils::FExtraCapture& GetExtraCapture() const override;
		virtual PCGUtils::FExtraCapture& GetExtraCapture() override;
		virtual const FPCGGraphExecutionInspection& GetInspection() const override;
		virtual FPCGGraphExecutionInspection& GetInspection() override;
		virtual void RegisterDynamicTracking(const UPCGSettings* InSettings, const TArrayView<TPair<FPCGSelectionKey, bool>>& InDynamicKeysAndCulling) override {}
		virtual void RegisterDynamicTracking(const FPCGSelectionKeyToSettingsMap& InKeysToSettings) override {}
#endif

	private:
		UVibeheimPCGExecutionSource& Owner;
	};

private:
	friend class FExecutionState;

	FExecutionState ExecutionState{ *this };

	TStrongObjectPtr<UPCGGraphInstance> GraphInstance;
	TStrongObjectPtr<UPCGParamData> ParameterData;
	TStrongObjectPtr<UPCGPointData> PointData;

	UWorld* World = nullptr;
	UPCGGraph* Graph = nullptr;
	FTransform Transform = FTransform::Identity;
	FBox Bounds = FBox(EForceInit::ForceInit);
	FString DebugName;
	int32 Seed = 0;
	bool bCancelled = false;
	bool bAborted = false;

#if WITH_EDITOR
	PCGUtils::FExtraCapture* ExtraCapture = nullptr;
	FPCGGraphExecutionInspection* Inspection = nullptr;
#endif
};






