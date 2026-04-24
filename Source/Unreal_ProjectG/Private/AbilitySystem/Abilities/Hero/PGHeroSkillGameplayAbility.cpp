#include "AbilitySystem/Abilities/Hero/PGHeroSkillGameplayAbility.h"

#include "AbilitySystem/AbilityTasks/SkillAbilityTask_MeleeTrace.h"
#include "AbilitySystem/AbilityTasks/SkillAbilityTask_SpawnActor.h"
#include "AbilitySystem/AbilityTasks/SkillAbilityTask_Buff.h"
#include "AbilitySystem/AbilityTasks/SkillAbilityTask_Presentation.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "AbilitySystemComponent.h"
#include "DataAssets/Ability/DataAsset_HeroSkillData.h"
#include "DataAssets/Ability/DataAsset_SkillVisualData.h"
#include "AbilitySystem/Effects/GEffect_Cooldown.h"
#include "Character/Unit/SubSystem/PGObjectPoolSubsystem.h"
#include "PGFunctionLibrary.h"
#include "PGGameplayTags.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGHeroSkillAbility, Log, All);

UPGHeroSkillGameplayAbility::UPGHeroSkillGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UPGHeroSkillGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);

    CachedMainActionSequence.Reset();

    SkillData = Cast<UDataAsset_HeroSkillData>(Spec.SourceObject.Get());
    if (!SkillData)
    {
        return;
    }

    const UAbilitySystemComponent* ASC = ActorInfo && ActorInfo->AbilitySystemComponent.IsValid()
        ? ActorInfo->AbilitySystemComponent.Get()
        : nullptr;

    if (ASC)
    {
        SkillData->BuildRuntimeActionSequence(ASC, FMath::Max(1, Spec.Level), CachedMainActionSequence);
    }

    if (CachedMainActionSequence.IsEmpty())
    {
        CachedMainActionSequence = SkillData->ActionSequence;
    }

    for (const FSkillActionRow& ActionRow : CachedMainActionSequence)
    {
        PreloadVisualAssetsFromAction(ActionRow);
    }

    UE_LOG(
        LogPGHeroSkillAbility,
        Log,
        TEXT("[OnGiveAbility] Ability=%s SpecLevel=%d MainActionCount=%d WarmUpCount=%d"),
        *GetNameSafe(this),
        Spec.Level,
        CachedMainActionSequence.Num(),
        SpawnActorWarmUpCount);

    PrewarmSpawnActorPools(CachedMainActionSequence, ActorInfo);
}

void UPGHeroSkillGameplayAbility::PreloadSkillVisualAsset(const UDataAsset_SkillVisualData* VisualAsset) const
{
    if (!VisualAsset)
    {
        return;
    }

    VisualAsset->ProjectileVFX.LoadSynchronous();
    VisualAsset->ProjectileSFX.LoadSynchronous();
    VisualAsset->HitVFX.LoadSynchronous();
    VisualAsset->HitSFX.LoadSynchronous();
    VisualAsset->ImpactVFX.LoadSynchronous();
    VisualAsset->ImpactSFX.LoadSynchronous();
    VisualAsset->PersistentVFX.LoadSynchronous();
    VisualAsset->PersistentSFX.LoadSynchronous();
}

void UPGHeroSkillGameplayAbility::PreloadVisualAssetsFromAction(const FSkillActionRow& ActionRow) const
{
    if (ActionRow.ActionType != ESkillActionType::SpawnActor)
    {
        return;
    }

    PreloadSkillVisualAsset(ActionRow.SpawnableConfig.VisualAsset);
    PreloadSkillVisualAsset(ActionRow.SpawnableConfig.NextSpawn.VisualAsset);
}

void UPGHeroSkillGameplayAbility::PrewarmSpawnActorPools(const TArray<FSkillActionRow>& ActionRows, const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (SpawnActorWarmUpCount <= 0)
    {
        UE_LOG(
            LogPGHeroSkillAbility,
            Log,
            TEXT("[PrewarmSpawnActorPools] Skip. Ability=%s WarmUpCount=%d"),
            *GetNameSafe(this),
            SpawnActorWarmUpCount);
        return;
    }

    UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : GetWorld();
    if (!World)
    {
        UE_LOG(LogPGHeroSkillAbility, Warning, TEXT("[PrewarmSpawnActorPools] No valid world. Ability=%s"), *GetNameSafe(this));
        return;
    }

    UPGObjectPoolSubsystem* PoolSubsystem = World->GetSubsystem<UPGObjectPoolSubsystem>();
    if (!PoolSubsystem)
    {
        UE_LOG(LogPGHeroSkillAbility, Warning, TEXT("[PrewarmSpawnActorPools] No pool subsystem. Ability=%s"), *GetNameSafe(this));
        return;
    }

    TSet<UClass*> ClassesToWarmup;
    for (const FSkillActionRow& ActionRow : ActionRows)
    {
        if (ActionRow.ActionType != ESkillActionType::SpawnActor)
        {
            continue;
        }

        if (UClass* SpawnClass = ActionRow.SpawnableConfig.ActorClass)
        {
            ClassesToWarmup.Add(SpawnClass);
        }
        else
        {
            UE_LOG(
                LogPGHeroSkillAbility,
                Warning,
                TEXT("[PrewarmSpawnActorPools] Missing ActorClass. Ability=%s ActionID=%s"),
                *GetNameSafe(this),
                *ActionRow.ActionID.ToString());
        }

        if (UClass* NextSpawnClass = ActionRow.SpawnableConfig.NextSpawn.ActorClass)
        {
            ClassesToWarmup.Add(NextSpawnClass);
        }
        else if (ActionRow.SpawnableConfig.NextSpawn.ActorClass != nullptr)
        {
            UE_LOG(
                LogPGHeroSkillAbility,
                Warning,
                TEXT("[PrewarmSpawnActorPools] NextSpawn ActorClass unresolved. Ability=%s ActionID=%s"),
                *GetNameSafe(this),
                *ActionRow.ActionID.ToString());
        }
        else if (ActionRow.SpawnableConfig.NextSpawn.ActorClass != nullptr)
        {
            UE_LOG(
                LogPGHeroSkillAbility,
                Warning,
                TEXT("[PrewarmSpawnActorPools] NextSpawn ActorClass unresolved. Ability=%s ActionID=%s"),
                *GetNameSafe(this),
                *ActionRow.ActionID.ToString());
        }
    }

    UE_LOG(
        LogPGHeroSkillAbility,
        Log,
        TEXT("[PrewarmSpawnActorPools] Ability=%s UniqueSpawnClasses=%d WarmEach=%d"),
        *GetNameSafe(this),
        ClassesToWarmup.Num(),
        SpawnActorWarmUpCount);

    for (UClass* SpawnClass : ClassesToWarmup)
    {
        UE_LOG(
            LogPGHeroSkillAbility,
            Log,
            TEXT("[PrewarmSpawnActorPools] Prewarm Class=%s Count=%d"),
            *GetNameSafe(SpawnClass),
            SpawnActorWarmUpCount);
        PoolSubsystem->PrewarmPool(SpawnClass, SpawnActorWarmUpCount);
    }
}

void UPGHeroSkillGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // 먼저 커밋 체크, 커밋 체크 실패 시 어빌리티 종료
    if(!CommitCheck(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    //==============================================================================
    // 어빌리티 변수 초기화
    bCommittedThisActivation = false;

    UDataAsset_HeroSkillData* ActiveSkillData = SkillData.Get();
    if (!ActiveSkillData)
    {
        ActiveSkillData = Cast<UDataAsset_HeroSkillData>(GetCurrentSourceObject());
        SkillData = ActiveSkillData;
    }

    if (!ActiveSkillData || ActiveSkillData->ActionSequence.IsEmpty())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    RuntimeActionSequence = CachedMainActionSequence;
    bBlockedMoveInputThisActivation = false;

    // 풀바디 모션인 경우 이동 입력 차단 태그 추가
    if (ActiveSkillData->BodyMode == EHeroSkillBodyMode::FullBody)
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ASC->AddLooseGameplayTag(PGGameplayTags::State_InputBlock_Move);
            bBlockedMoveInputThisActivation = true;
        }
    }
    //==============================================================================

    if (RuntimeActionSequence.IsEmpty())
    {
        if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ActiveSkillData->BuildRuntimeActionSequence(ASC, GetAbilityLevel(), RuntimeActionSequence);
        }
    }

    if (RuntimeActionSequence.IsEmpty())
    {
        RuntimeActionSequence = ActiveSkillData->ActionSequence;
    }

    if (CachedMainActionSequence.IsEmpty() && !RuntimeActionSequence.IsEmpty())
    {
        CachedMainActionSequence = RuntimeActionSequence;
    }

    if (RuntimeActionSequence.IsEmpty())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    CurrentActionIndex = 0;

    UHeroCombatComponent* CombatComp = UPGFunctionLibrary::NativeGetCombatComponentFromActor(GetAvatarActorFromActorInfo());
    bAutoMode = CombatComp ? CombatComp->IsAutoMode() : false;

    //==========================================================================================================
    // 스폰 프로젝타일의 타깃 액터를 위한 연출 타이밍 함수와 변수들. 현재는 이 경우의 수 하나 뿐이라 이렇게 구현했지만, 추후에 다른 연출 타이밍이 추가된다면 더 범용적으로 바꿀 필요가 있을듯
    // 스킬 프레젠테이션 데이터가 존재한다면 스킬 프레젠테이션 태스크 생성 및 실행
    bPresentationStarted = false;
    bDelayPresentationUntilTargetConfirm = ShouldDelayPresentationStart();

    if (!bDelayPresentationUntilTargetConfirm)
    {
        TryStartPresentationTask();
    }
    //==========================================================================================================

    ExecuteNextAction();
}

void UPGHeroSkillGameplayAbility::ExecuteNextAction()
{
    if (!RuntimeActionSequence.IsValidIndex(CurrentActionIndex))
    {
        EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, false);
        return;
    }

    const FSkillActionRow& CurrentAction = RuntimeActionSequence[CurrentActionIndex];

    switch (CurrentAction.ActionType)
    {
    case ESkillActionType::MeleeTrace:
    {
        USkillAbilityTask_MeleeTrace* Task = USkillAbilityTask_MeleeTrace::Create(this, CurrentAction);
        Task->OnCompleted.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnActionCompleted);
        Task->OnCancelled.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnActionCancelled);
        Task->OnRuntimeEvent.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnTaskRuntimeEvent);
        Task->ReadyForActivation();
        break;
    }

    case ESkillActionType::SpawnActor:
    {
        USkillAbilityTask_SpawnActor* Task = USkillAbilityTask_SpawnActor::Create(
            this,
            CurrentAction,
            bAutoMode,
            FGameplayAbilityTargetDataHandle(),
            false);
        Task->OnCompleted.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnActionCompleted);
        Task->OnCancelled.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnActionCancelled);
        Task->OnRuntimeEvent.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnTaskRuntimeEvent);
        Task->ReadyForActivation();
        break;
    }

    case ESkillActionType::Buff:
    {
        USkillAbilityTask_Buff* Task = USkillAbilityTask_Buff::Create(this, CurrentAction);
        Task->OnCompleted.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnActionCompleted);
        Task->OnCancelled.AddDynamic(this, &UPGHeroSkillGameplayAbility::OnActionCancelled);
        Task->ReadyForActivation();
        break;
    }

    default:
        ++CurrentActionIndex;
        ExecuteNextAction();
        break;
    }
}

bool UPGHeroSkillGameplayAbility::ConvertEventTagToTrigger(const FGameplayTag& EventTag, EHeroSkillEventTrigger& OutTrigger)
{
    if (EventTag.MatchesTagExact(PGGameplayTags::Shared_Event_MeleeHit) || EventTag.MatchesTagExact(PGGameplayTags::Event_Trigger_OnHit))
    {
        OutTrigger = EHeroSkillEventTrigger::OnHit;
        return true;
    }

    if (EventTag.MatchesTagExact(PGGameplayTags::Event_Trigger_OnKill))
    {
        OutTrigger = EHeroSkillEventTrigger::OnKill;
        return true;
    }

    return false;
}

void UPGHeroSkillGameplayAbility::OnTaskRuntimeEvent(FGameplayTag EventTag, FGameplayAbilityTargetDataHandle TargetData)
{
    //==========================================================================================================
    // 스폰 프로젝타일의 타깃 액터를 위한 연출 타이밍 함수와 변수들. 현재는 이 경우의 수 하나 뿐이라 이렇게 구현했지만, 추후에 다른 연출 타이밍이 추가된다면 더 범용적으로 바꿀 필요가 있을듯
    // 연출 타이밍이 타겟 확정일 때, 프레젠테이션이 시작되지 않은 상태에서 타겟 확정 이벤트가 들어오면 프레젠테이션 시작
    if (EventTag.MatchesTagExact(PGGameplayTags::Event_Trigger_OnTargetDataReady))
    {
        if (bDelayPresentationUntilTargetConfirm)
        {
            TryStartPresentationTask();
            bDelayPresentationUntilTargetConfirm = false;
        }
        return;
    }
    //==========================================================================================================
    
    // 만약 이벤트가 Commit 이벤트라면 먼저 적용하고 return
    if(EventTag.MatchesTagExact(PGGameplayTags::Event_Trigger_OnCommit))
    {
        if (!TryCommitAbilityOnce())
        {
            // 커밋에 실패했다면 어빌리티 종료, 커밋에 성공했다면 계속 진행
            EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
        }
        return;
    }

    //Trigger를 Tag에서 Enum타입으로 변환, 변환 실패 시(일치하는 태그를 잧지 못하 걍우) 무시
    EHeroSkillEventTrigger Trigger = EHeroSkillEventTrigger::OnHit;
    if (!ConvertEventTagToTrigger(EventTag, Trigger))
    {
        return;
    }

    ExecuteEventTrigger(Trigger, EventTag, TargetData);
}

void UPGHeroSkillGameplayAbility::ExecuteEventTrigger(EHeroSkillEventTrigger Trigger, const FGameplayTag& SourceEventTag, const FGameplayAbilityTargetDataHandle& TargetData)
{
    if (!SkillData)
    {
        return;
    }

    const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC)
    {
        return;
    }

    TArray<FSkillActionRow> EventActions;
    SkillData->BuildRuntimeEventActionSequence(ASC, GetAbilityLevel(), EventActions, Trigger, SourceEventTag);

    for (const FSkillActionRow& ActionRow : EventActions)
    {
        ExecuteEventAction(ActionRow, TargetData);
    }
}

void UPGHeroSkillGameplayAbility::ExecuteEventAction(const FSkillActionRow& ActionRow, const FGameplayAbilityTargetDataHandle& TargetData)
{
    switch (ActionRow.ActionType)
    {
    case ESkillActionType::SpawnActor:
    {
        USkillAbilityTask_SpawnActor* Task = USkillAbilityTask_SpawnActor::Create(
            this,
            ActionRow,
            bAutoMode,
            TargetData,
            false);
        Task->ReadyForActivation();
        break;
    }

    case ESkillActionType::MeleeTrace:
    {
        USkillAbilityTask_MeleeTrace* Task = USkillAbilityTask_MeleeTrace::Create(this, ActionRow);
        Task->ReadyForActivation();
        break;
    }

    case ESkillActionType::Buff:
    {
        USkillAbilityTask_Buff* Task = USkillAbilityTask_Buff::Create(this, ActionRow, TargetData);
        Task->ReadyForActivation();
        break;
    }

    default:
        break;
    }
}

void UPGHeroSkillGameplayAbility::OnActionCompleted(FGameplayAbilityTargetDataHandle TargetData)
{
    ++CurrentActionIndex;
    ExecuteNextAction();
}

void UPGHeroSkillGameplayAbility::OnActionCancelled(FGameplayAbilityTargetDataHandle TargetData)
{
    EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, true);
}

void UPGHeroSkillGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!bWasCancelled)
    {
        // 스킬이 정상 종료라면 쿨타임 적용 시도
        TryCommitAbilityOnce();
    }

    RuntimeActionSequence.Reset();

    if (bBlockedMoveInputThisActivation)
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ASC->RemoveLooseGameplayTag(PGGameplayTags::State_InputBlock_Move);
            ASC->RemoveLooseGameplayTag(PGGameplayTags::State_InputBlock_ScreenTouch);
        }
    }

    CurrentActionIndex = 0;
    bAutoMode = false;
    bCommittedThisActivation = false;
    bBlockedMoveInputThisActivation = false;

    bPresentationStarted = false;
    bDelayPresentationUntilTargetConfirm = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


// =========================================================================================
// 쿨타임 관련 로직
// 쿨타임을 Duration만 받아와서 어빌리티 내부에서 이펙트에 SetByCaller로 전달하도록 변경
// 쿨타임 태그도 어빌리티의 태그를 보고 메인 스킬인지 서브 스킬인지 판단해서 적절한 태그를 반환하도록 변경
// =========================================================================================
bool UPGHeroSkillGameplayAbility::TryCommitAbilityOnce()
{
    // 이미 발동 했거나 커밋에 성공하면 true 반환, 커밋에 실패하면 false 반환
    if (bCommittedThisActivation)
    {
        return true;
    }

    if(CommitAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo()))
    {
        bCommittedThisActivation = true;
        return true;
    }

    return false;
}

const FGameplayAbilitySpec* UPGHeroSkillGameplayAbility::ResolveAbilitySpec(
    const FGameplayAbilitySpecHandle& Handle,
    const FGameplayAbilityActorInfo* ActorInfo) const
{
    const FGameplayAbilitySpecHandle SpecHandle = Handle.IsValid() ? Handle : CurrentSpecHandle;
    if (!SpecHandle.IsValid())
    {
        return nullptr;
    }

    const FGameplayAbilityActorInfo* ResolvedActorInfo = ActorInfo ? ActorInfo : CurrentActorInfo;
    if (!ResolvedActorInfo || !ResolvedActorInfo->AbilitySystemComponent.IsValid())
    {
        return nullptr;
    }

    return ResolvedActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(SpecHandle);
}

EHeroSkillType UPGHeroSkillGameplayAbility::GetHeroSkillType() const
{
    const FGameplayAbilitySpec* Spec = ResolveAbilitySpec();
    if (!Spec)
    {
        return EHeroSkillType::None;
    }

    const FGameplayTagContainer& DynamicTags = Spec->GetDynamicSpecSourceTags();
    if (DynamicTags.HasTag(PGGameplayTags::Input_ActiveSkill_MainSkill))
    {
        return EHeroSkillType::MainSkill;
    }
    else if (DynamicTags.HasTag(PGGameplayTags::Input_ActiveSkill_SubSkill))
    {
        return EHeroSkillType::SubSkill;
    }
    else
    {
        return EHeroSkillType::None;
    }
}

const FGameplayTagContainer* UPGHeroSkillGameplayAbility::GetCooldownTags() const
{
    static const FGameplayTagContainer MainSkillTagContainer = []()
        {
            FGameplayTagContainer Tags;
            Tags.AddTag(PGGameplayTags::Ability_Cooldown_MainSkill);
            return Tags;
        }();

    static const FGameplayTagContainer SubSkillTagContainer = []()
        {
            FGameplayTagContainer Tags;
            Tags.AddTag(PGGameplayTags::Ability_Cooldown_SubSkill);
            return Tags;
        }();

    // 현재 어빌리티의 태그 종류에 따라 적절한 태그 컨테이너 반환 
    switch (GetHeroSkillType())
    {
    case EHeroSkillType::MainSkill:
        return &MainSkillTagContainer;
    case EHeroSkillType::SubSkill:
        return &SubSkillTagContainer;
    default:
        return nullptr;
    }
}

void UPGHeroSkillGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // 스킬 데이터에서 쿨타임 Value 가져와서 SetDuration에 설정
    const float CooldownDuration = SkillData ? SkillData->SkillCooldown.GetValueAtLevel(GetAbilityLevel()) : 0.f;

    if(CooldownDuration <= 0.f)
    {
        return;
    }

    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(UGEffect_Cooldown::StaticClass());
    
    if (!SpecHandle.IsValid()) return;

    const FGameplayAbilitySpec* Spec = ResolveAbilitySpec(Handle, ActorInfo);
    if (!Spec)
    {
        return;
    }

    const FGameplayTagContainer& DynamicTags = Spec->GetDynamicSpecSourceTags();

    // 슬롯 태그 주입
    if (DynamicTags.HasTag(PGGameplayTags::Input_ActiveSkill_MainSkill))
    {
        SpecHandle.Data->DynamicGrantedTags.AddTag(PGGameplayTags::Ability_Cooldown_MainSkill);
    }
    else if (DynamicTags.HasTag(PGGameplayTags::Input_ActiveSkill_SubSkill))
    {
        SpecHandle.Data->DynamicGrantedTags.AddTag(PGGameplayTags::Ability_Cooldown_SubSkill);
    }
    else
    {
        return;
    }

    //UE_LOG(LogTemp, Log, TEXT("Applying Cooldown: %f seconds"), CooldownDuration);

    SpecHandle.Data->SetSetByCallerMagnitude(PGGameplayTags::Shared_SetByCaller_Duration, CooldownDuration);

    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

//==========================================================================================================
//==========================================================================================================
// 스폰 프로젝타일의 타깃 액터를 위한 연출 타이밍 함수와 변수들. 현재는 이 경우의 수 하나 뿐이라 이렇게 구현했지만, 추후에 다른 연출 타이밍이 추가된다면 더 범용적으로 바꿀 필요가 있을듯
//==========================================================================================================
void UPGHeroSkillGameplayAbility::TryStartPresentationTask()
{
    if (bPresentationStarted || !SkillData || !SkillData->PresentationData)
    {
        return;
    }

    if (USkillAbilityTask_Presentation* PresentationTask = USkillAbilityTask_Presentation::Create(this, SkillData->PresentationData))
    {
        bPresentationStarted = true;
        PresentationTask->ReadyForActivation();
    }
}

bool UPGHeroSkillGameplayAbility::ShouldDelayPresentationStart() const
{
    // 액션이 스폰액터 - 타겟 지정일 때, 연출 시작을 타겟 확정 이벤트까지 지연
    if (!RuntimeActionSequence.IsValidIndex(0))
    {
        return false;
    }

    const FSkillActionRow& FirstAction = RuntimeActionSequence[0];
    if (FirstAction.ActionType != ESkillActionType::SpawnActor)
    {
        return false;
    }

    const FHeroSpawnableConfig& SpawnConfig = FirstAction.SpawnableConfig;
    return (!bAutoMode && SpawnConfig.SpawnLocationPolicy == ESpawnLocation::AtTargetPoint);
}
