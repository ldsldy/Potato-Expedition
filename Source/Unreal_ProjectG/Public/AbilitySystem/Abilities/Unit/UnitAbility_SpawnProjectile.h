// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PGUnitGameplayAbility.h"
#include "UnitAbility_SpawnProjectile.generated.h"

/**
 * 
 */
UCLASS()
class UNREAL_PROJECTG_API UUnitAbility_SpawnProjectile : public UPGUnitGameplayAbility
{
	GENERATED_BODY()
	
    virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    // 프로젝타일 스폰 함수
    UFUNCTION(BlueprintCallable, Category = "Ability|Projectile Attack")
    void SpawnProjectile(FGameplayEventData InEventData);

    UFUNCTION()
    void OnMontageFinished();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Projectile Attack")
    FUnitSpawnProjectileAbilityConfig UnitSpawnProjectileConfig;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Projectile Attack", meta = (ClampMin = "0"))
    int32 PoolPrewarmCount = 12;
};
