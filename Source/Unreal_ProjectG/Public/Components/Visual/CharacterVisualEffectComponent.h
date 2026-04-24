// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "CharacterVisualEffectComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct FCharacterMeshMaterialCache
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterialInstances;

    UPROPERTY()
    bool bInitialized = false;
};

/**
 * 
 */
UCLASS()
class UNREAL_PROJECTG_API UCharacterVisualEffectComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()
	
public:
    UCharacterVisualEffectComponent();
    
    virtual void BeginPlay() override;

    // 오너의 메시와 머티리얼을 캐싱
    UFUNCTION(BlueprintCallable)
    void InitializeMaterialCachedIfNeeded();

    // 히트 리액트 효과 활성화/비활성화
    UFUNCTION(BlueprintCallable)
    void SetHitReactEnabled(bool bEnabled);

    // AOE 하이라이트 효과 활성화/비활성화
    UFUNCTION(BlueprintCallable)
    void SetAOEHighlightEnabled(bool bEnabled);

    // 모든 하이라이트 효과 초기 상태로 리셋
    UFUNCTION(BlueprintCallable)
    void ResetVisualEffectState();

private:
    // 메시의 머티리얼 하이라이트 상태를 업데이트
    void RefreshVisualState();

private:
    UPROPERTY()
    TMap<TObjectPtr<UMeshComponent>, FCharacterMeshMaterialCache> MeshMaterialCache;

    bool bCacheOnBeginPlay = false;
    bool bHitReactEnabled;
    bool bAOEHighlightEnabled;
};
