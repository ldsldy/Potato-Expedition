#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/AbilityTasks/SkillAbilityTask.h"
#include "DataAssets/Ability/DataAsset_HeroSkillData.h"
#include "SkillAbilityTask_SpawnActor.generated.h"

class ASkillActor;

UCLASS()
class UNREAL_PROJECTG_API USkillAbilityTask_SpawnActor : public USkillAbilityTask
{
    GENERATED_BODY()

public:
    // Task 생성 함수
    static USkillAbilityTask_SpawnActor* Create(
        UGameplayAbility* OwningAbility,
        const FSkillActionRow& ActionRow,   // 데이터 드리븐 정보
        bool bIsAutoMode,                   // 플레이어 오토모드 여부
        const FGameplayAbilityTargetDataHandle& InTriggerTargetData = FGameplayAbilityTargetDataHandle(), // 타깃 데이터 핸들(있다면 Location 정보 활용)
        bool bInEmitRuntimeEvent = true);   // 런타임 이벤트 발송 여부 

    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    void StartWaitTargetData();

    UFUNCTION()
    void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetDataHandle);

    UFUNCTION()
    void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetDataHandle);

    FVector ResolveAutoTargetLocation() const;
    bool TryResolveSpawnFromTriggerData(FVector& OutLocation, FRotator& OutRotation) const;

    void PlayMontageOrSpawn();

    UFUNCTION()
    void OnMontageFinished();

    UFUNCTION()
    void OnMontageCancelled();

    UFUNCTION()
    void OnSpawnEventReceived(FGameplayEventData Payload);

    void SpawnActorAtLocation(const FVector& Location, const FRotator& Rotation);

private:
    FSkillActionRow CachedActionRow;
    bool bAutoMode = false;
    bool bEmitRuntimeEvent = true;
    bool bWaitMontageFinish = false;
    bool bActorSpawned = false;
    bool bTaskResolved = false;

    FGameplayAbilityTargetDataHandle TriggerTargetData;

    FVector CachedSpawnLocation = FVector::ZeroVector;
    FRotator CachedSpawnRotation = FRotator::ZeroRotator;
};
