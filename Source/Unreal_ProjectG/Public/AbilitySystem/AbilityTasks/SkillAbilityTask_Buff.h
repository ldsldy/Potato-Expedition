// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/AbilityTasks/SkillAbilityTask.h"
#include "DataAssets/Ability/DataAsset_HeroSkillData.h"
#include "SkillAbilityTask_Buff.generated.h"

UCLASS()
class UNREAL_PROJECTG_API USkillAbilityTask_Buff : public USkillAbilityTask
{
    GENERATED_BODY()

public:
    static USkillAbilityTask_Buff* Create(
        UGameplayAbility* OwningAbility,
        const FSkillActionRow& ActionRow,
        const FGameplayAbilityTargetDataHandle& InTriggerTargetData = FGameplayAbilityTargetDataHandle());

    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    bool ApplyBuffOnce();
    void CompleteTask();
    void CancelTask();

    UFUNCTION()
    void OnMontageFinished();

    UFUNCTION()
    void OnMontageCancelled();

    UFUNCTION()
    void OnBuffEventReceived(FGameplayEventData Payload);

    bool ResolveCenterLocation(FVector& OutCenter) const;
    bool IsTargetAllowed(AActor* Candidate) const;
    void ApplyEffectsToTargets(const TArray<AActor*>& Targets) const;

    // ESkillTargetPolicy 수집 방식별로 타겟 수집을 담당하는 헬퍼 함수들
    void GatherTargets(const FVector& CenterLocation, TArray<AActor*>& OutTargets) const;
    float GetTargetHealth(const AActor* Target) const;
    float GetTargetMaxHealth(const AActor* Target) const;
    float ResolveTargetHealthPercentage(const AActor* Target) const;

    // 버프를 적용한 타깃들을 확인하기 위한 디버그 함수
    void DebugDrawAppliedTargets(const TArray<AActor*>& Targets) const;
private:
    FSkillActionRow ActionRowData;
    FGameplayAbilityTargetDataHandle TriggerTargetData;
    bool bWaitMontageFinish = false;
    bool bBuffApplied = false;
    bool bTaskResolved = false;
};
