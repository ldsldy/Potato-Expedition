// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/SkillAbilityTask.h"
#include "AbilitySystem/Types/PGGameplayEffectContext.h"
#include "GameplayCueFunctionLibrary.h"
#include "GameplayEffect.h"
#include "Types/AbilityConfig.h"
#include "PGGameplayTags.h"

// 헬퍼 함수
 //EffectContextHandle에 이펙트 타입을 담는다.
namespace SkillCueHelper
{
    // 스태틱 큐를 적용할지 판단하는 헬퍼 함수
    static bool ShouldExecuteStaticCue(const FEffectConfig& InEffectConfig)
    {
        return InEffectConfig.AppliedCueTag.IsValid()
            && InEffectConfig.AppliedCueTag.MatchesTag(PGGameplayTags::GameplayCue_SVFX_Static);
    }

    // EffectContextHandle을 복제하면서 CueVariantTag를 담는 헬퍼 함수
    static FGameplayEffectContextHandle DuplicateContextWithVariantTag(
        const FGameplayEffectContextHandle& InContext,
        const FGameplayTag& InCueVariantTag)
    {
        if (!InContext.IsValid()) return FGameplayEffectContextHandle();

        FGameplayEffectContextHandle NewContext = InContext.Duplicate();
        if (!NewContext.IsValid()) return FGameplayEffectContextHandle();

        // 전달할 변형 태그가 없으면 복제본만 반환
        if (!InCueVariantTag.IsValid()) return NewContext;

        FGameplayEffectContext* RawContext = NewContext.Get();
        
        // FGameplayEffectContext를 가져오는데 실패했다면 빈 핸들 반환
        if (!RawContext) return FGameplayEffectContextHandle();

        // RawContext가 FPGGameplayEffectContext의 자식이 아니라면 빈 핸들 반환
        UScriptStruct* ScriptStruct = RawContext->GetScriptStruct();
        if (!ScriptStruct || !ScriptStruct->IsChildOf(FPGGameplayEffectContext::StaticStruct()))
        {
            return FGameplayEffectContextHandle();
        }

        // FPGGameplayEffectContext로 캐스팅해서 CueVariantTag를 담는다.
        static_cast<FPGGameplayEffectContext*>(RawContext)->CueVariantTag = InCueVariantTag;
        return NewContext;
    }
}

FGameplayEffectContextHandle USkillAbilityTask::AddActorCueIntoSpecHandle(FGameplayEffectSpecHandle& InOutSpecHandle, const FEffectConfig& InEffectConfig) const
{
    if (!InOutSpecHandle.IsValid() || !InOutSpecHandle.Data.IsValid()) return FGameplayEffectContextHandle();

    FGameplayEffectSpec& Spec = *InOutSpecHandle.Data.Get();

    FGameplayEffectContextHandle NewContext = SkillCueHelper::DuplicateContextWithVariantTag(Spec.GetContext(), InEffectConfig.CueVariantTag);

    if (NewContext.IsValid())
    {
        Spec.SetContext(NewContext);
        return NewContext;
    }

    return Spec.GetContext();
}

void USkillAbilityTask::ExecuteStaticCue(AActor* TargetActor, const FEffectConfig& InEffectConfig, const FGameplayEffectContextHandle& EffectContext) const
{
    if (!TargetActor) return;

    if (!SkillCueHelper::ShouldExecuteStaticCue(InEffectConfig)) return;

    FGameplayCueParameters CueParams;

    // Context가 있으면 CueVariantTag를 담아 전달, 없으면 빈 Params로 그대로 실행
    FGameplayEffectContextHandle NewContext =
        SkillCueHelper::DuplicateContextWithVariantTag(EffectContext, InEffectConfig.CueVariantTag);

    if (NewContext.IsValid())
    {
        CueParams = FGameplayCueParameters(NewContext);
    }

    CueParams.Location = TargetActor->GetActorLocation();

    if (Ability)
    {
        if (AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo())
        {
            CueParams.Instigator = AvatarActor;
            CueParams.EffectCauser = AvatarActor;
        }
    }

    UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(
        TargetActor,
        InEffectConfig.AppliedCueTag,
        CueParams);
}

void USkillAbilityTask::EmitRuntimeEvent(const FGameplayTag& EventTag, const FGameplayAbilityTargetDataHandle& TargetData)
{
    if (!EventTag.IsValid())
    {
        return;
    }
    OnRuntimeEvent.Broadcast(EventTag, TargetData);
}
