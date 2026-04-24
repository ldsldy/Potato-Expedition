#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PGHeroGameplayAbility.h"
#include "DataAssets/Ability/DataAsset_HeroSkillData.h"
#include "PGHeroSkillGameplayAbility.generated.h"

UENUM()
enum class EHeroSkillType : uint8
{
    None            UMETA(DisplayName = "없음"),
    MainSkill       UMETA(DisplayName = "메인 스킬"),
    SubSkill        UMETA(DisplayName = "서브 스킬"),
    BasicAttack     UMETA(DisplayName = "기본 공격"),
};

class UDataAsset_SkillVisualData;


/*
* 히어로 스킬 어빌리티 클래스
* 스킬 데이터 에셋의 정보를 기반으로 스킬 액션을 실행하는 어빌리티
* 스킬 액션은 근접 트레이스, 버프, 액터 소환 등의 동작이고 각 동작이 끝나면 다음 액션이 실행되는 방식으로 구성되어 있음
* 스킬 액션이 실행되는 도중에 특정 이벤트(Hit, Kill 등)이 발생하면 해당 이벤트에 반응하는 액션을 실행하도록 되어 있음.
*/
UCLASS()
class UNREAL_PROJECTG_API UPGHeroSkillGameplayAbility : public UPGHeroGameplayAbility
{
    GENERATED_BODY()

public:
    UPGHeroSkillGameplayAbility();

protected:
    virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    // 쿨다운 게임플레이 이펙트에서 태그를 가져오던 것을 이 어빌리티의 태그를 보고 메인 스킬인지 서브 스킬인지 판단해서 적절한 태그를 반환하도록 변경
    virtual const FGameplayTagContainer* GetCooldownTags() const override;
    // 스킬데이터의 쿨타임 Value와 GetCooldownTags()로 반환된 태그로 이펙트 스펙을 만들어서 적용하도록 변경
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

private:
    const FGameplayAbilitySpec* ResolveAbilitySpec(
        const FGameplayAbilitySpecHandle& Handle = FGameplayAbilitySpecHandle(),
        const FGameplayAbilityActorInfo* ActorInfo = nullptr) const;
    void PreloadSkillVisualAsset(const UDataAsset_SkillVisualData* VisualAsset) const;
    void PreloadVisualAssetsFromAction(const FSkillActionRow& ActionRow) const;
    void PrewarmSpawnActorPools(const TArray<FSkillActionRow>& ActionRows, const FGameplayAbilityActorInfo* ActorInfo) const;

    void ExecuteNextAction();

    // SkillData의 이벤트에서 이벤트 액션 Modifier를 찾아서 액션 배열에 추가하는 함수
    void ExecuteEventTrigger(EHeroSkillEventTrigger Trigger, const FGameplayTag& SourceEventTag, const FGameplayAbilityTargetDataHandle& TargetData);
    
    // 실제 이벤트 실행 함수
    void ExecuteEventAction(const FSkillActionRow& ActionRow, const FGameplayAbilityTargetDataHandle& TargetData);

    // 태그 -> EHeroSkillEventTrigger 변환 함수, 변환 실패 시 false 반환
    static bool ConvertEventTagToTrigger(const FGameplayTag& EventTag, EHeroSkillEventTrigger& OutTrigger);

    UFUNCTION()
    void OnTaskRuntimeEvent(FGameplayTag EventTag, FGameplayAbilityTargetDataHandle TargetData);

    UFUNCTION()
    void OnActionCompleted(FGameplayAbilityTargetDataHandle TargetData);

    UFUNCTION()
    void OnActionCancelled(FGameplayAbilityTargetDataHandle TargetData);

    UFUNCTION()
    EHeroSkillType GetHeroSkillType() const;

    // 이벤트에 반응하여 어빌리티를 Commit하는 함수, 어빌리티에서 딱 한 번만 커밋하도록 체크 포함
    bool TryCommitAbilityOnce();

private:
    int32 CurrentActionIndex = 0;

    UPROPERTY()
    TObjectPtr<UDataAsset_HeroSkillData> SkillData = nullptr;

    UPROPERTY()
    TArray<FSkillActionRow> CachedMainActionSequence;

    UPROPERTY()
    TArray<FSkillActionRow> RuntimeActionSequence;

    UPROPERTY(EditDefaultsOnly, Category = "Skill|Pooling", meta = (ClampMin = "0"))
    int32 SpawnActorWarmUpCount = 8;

    bool bAutoMode = false;

    bool bCommittedThisActivation = false;

    bool bBlockedMoveInputThisActivation = false;

    //==========================================================================================================
    //==========================================================================================================
    // 스폰 프로젝타일의 타깃 액터를 위한 연출 타이밍 함수와 변수들. 현재는 이 경우의 수 하나 뿐이라 이렇게 구현했지만, 추후에 다른 연출 타이밍이 추가된다면 더 범용적으로 바꿀 필요가 있을듯
    //==========================================================================================================
    void TryStartPresentationTask();
    bool ShouldDelayPresentationStart() const;

    bool bPresentationStarted = false;
    bool bDelayPresentationUntilTargetConfirm = false;
};
