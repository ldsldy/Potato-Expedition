// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/SkillActor/SkillActor.h"
#include "BuffAuraActor.generated.h"

class UDecalComponent;

/**
 * 
 */
UCLASS()
class UNREAL_PROJECTG_API ABuffAuraActor : public ASkillActor
{
	GENERATED_BODY()

public:
    ABuffAuraActor();
    virtual void InitFromConfig(const FHeroSpawnableConfig& InConfig, int32 InAbilityLevel = 1) override;

protected:
    virtual void OnActivatedFromPool_Implementation() override;
    virtual void OnReturnedToPool_Implementation() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void OnAuraBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnAuraEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    bool IsValidAuraTarget(AActor* TargetActor) const;
    void ApplyAuraToTarget(AActor* TargetActor);
    void RemoveAuraFromTarget(AActor* TargetActor);
    void RemoveAllAuraEffects();

private:
    TMap<TWeakObjectPtr<AActor>, TArray<FActiveGameplayEffectHandle>> ActiveAuraEffectsByTarget;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Components")
    TObjectPtr<UDecalComponent> AuraDecalComponent;
};
