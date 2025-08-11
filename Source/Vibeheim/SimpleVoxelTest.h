#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimpleVoxelTest.generated.h"

UCLASS()
class VIBEHEIM_API ASimpleVoxelTest : public AActor
{
    GENERATED_BODY()
    
public:    
    ASimpleVoxelTest();

protected:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "Voxel Test")
    void TestVoxelPlugin();

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent* MeshComponent;
};