// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PGUnitGameplayAbility.h"
#include "Types/AbilityConfig.h" 
#include "UUnitAbility_SpawnMagic.generated.h"

UCLASS()
class UNREAL_PROJECTG_API UUnitAbility_SpawnMagic : public UPGUnitGameplayAbility
{
    GENERATED_BODY()

public:
    virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 마법 장판/폭발 스폰 함수
    UFUNCTION(BlueprintCallable, Category = "Ability|Magic Attack")
    void SpawnMagic(FGameplayEventData InEventData);

    UFUNCTION()
    void OnMontageFinished();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Magic Attack")
    FUnitSpawnMagicAbilityConfig UnitSpawnMagicConfig;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Magic Attack", meta = (ClampMin = "0"))
    int32 PoolPrewarmCount = 5;
};
