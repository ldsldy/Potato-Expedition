// Fill out your copyright notice in the Description page of Project Settings.


#include "PGFunctionLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/PGAbilitySystemComponent.h"
#include "Character/PGCharacterBase.h"
#include "AbilitySystem/PGCharacterAttributeSet.h"
#include "PGGameplayTags.h"
#include "Interfaces/HeroCombatInterface.h"
#include "Components/Combat/HeroCombatComponent.h"

#include "AbilitySystem/Types/PGGameplayEffectContext.h"
#include "GameplayEffectTypes.h"

UPGAbilitySystemComponent* UPGFunctionLibrary::NativeGetPGASCFromActor(AActor* InActor)
{
    checkf(InActor, TEXT("InActor : %s"), *GetNameSafe(InActor));

    return CastChecked<UPGAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor));
}

UHeroCombatComponent* UPGFunctionLibrary::NativeGetCombatComponentFromActor(AActor* InActor)
{
    check(InActor);

    // 인터페이스를 통해 액터 컴포너트를 가져오려는 시도
    if (IHeroCombatInterface* HeroCombatInterface = Cast<IHeroCombatInterface>(InActor))
    {
        return HeroCombatInterface->GetHeroCombatComponent();
    }

    // 인터페이스가 구현되어 있지 않다면, 액터에서 직접 컴포넌트를 찾아보는 시도
    if (UHeroCombatComponent* HeroCombatComp = InActor->FindComponentByClass<UHeroCombatComponent>())
    {
        return HeroCombatComp;
    }

    return nullptr;
}

bool UPGFunctionLibrary::NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck)
{
    UPGAbilitySystemComponent* ASC = NativeGetPGASCFromActor(InActor);

    return ASC->HasMatchingGameplayTag(TagToCheck);
}

bool UPGFunctionLibrary::IsTargetCharacterHostile(AActor* InInstigator, AActor* InTargetActor)
{
    // 두 태그가 다르면 적대 관계로 간주
    APGCharacterBase* InstigatorCharacter = Cast<APGCharacterBase>(InInstigator);
    APGCharacterBase* TargetCharacter = Cast<APGCharacterBase>(InTargetActor);

    if(!InstigatorCharacter || !TargetCharacter)
    {
        return false;
    }

    return InstigatorCharacter->GetTeamTag() != TargetCharacter->GetTeamTag();
}

const TMap<FGameplayAttribute, FGameplayTag>& UPGFunctionLibrary::GetAttributeTagMap()
{
    // 람다를 이용한 정적 초기화
    // Static이므로 호출 시에 전역에서 딱 한번만 초기화 되고, 이후에는 초기화된 맵을 반환
    static TMap<FGameplayAttribute, FGameplayTag> Map = []()
    {
        // 임시 맵 생성
        TMap<FGameplayAttribute, FGameplayTag> M;

        // Map.Add를 래핑하여 간결하게 사용 가능하도록 람다 함수를 정의
        // 이후 Add만으로 Map에 항목을 추가할 수 있음
        auto Add = [&M](const FGameplayAttribute& Attribute, const FGameplayTag& TagName)
        {
            M.Add(Attribute, TagName);
        };

        // ==========================================================================
        // 여기에 Attribute와 TagName을 추가
        // ==========================================================================
        Add(UPGCharacterAttributeSet::GetMaxHealthAttribute(), PGGameplayTags::Attribute_SetByCaller_MaxHealth);
        Add(UPGCharacterAttributeSet::GetAttackPowerAttribute(), PGGameplayTags::Attribute_SetByCaller_AttackPower);
    
        return M;
    }();

    // 초기화된 맵 반환
    return Map;
}

FGameplayTag UPGFunctionLibrary::GetSetByCallerTagForAttribute(const FGameplayAttribute& Attribute)
{
    if (const FGameplayTag* FoundTag = GetAttributeTagMap().Find(Attribute))
    {
        return *FoundTag;
    }
    
    return FGameplayTag();
}

const int32 UPGFunctionLibrary::ResolveSetPieceCount(const UAbilitySystemComponent* InASC, const FGameplayTag& RequiredSetTag) 
{
    if (!InASC || !RequiredSetTag.IsValid())
    {
        return 0;
    }
    
    // TODO : 세트 효과각 세분과 된다면 교체 지금은 세트 효과가 하나의 태그로 표현된다고 가정(3세트만 존재)
    return InASC->HasMatchingGameplayTag(RequiredSetTag) ? 3 : 0;
}

// ====================================================================================================
// Effect 관련 헬퍼 함수들
// ====================================================================================================

FActiveGameplayEffectHandle UPGFunctionLibrary::ApplyGameplayEffectSpecHandleToTargetActor(AActor* InInstigator, AActor* InTargetActor, const FGameplayEffectSpecHandle& InSpecHandle)
{
    UPGAbilitySystemComponent* TargetASC = NativeGetPGASCFromActor(InTargetActor);
    UPGAbilitySystemComponent* SourceASC = NativeGetPGASCFromActor(InInstigator);

    checkf(TargetASC, TEXT("TargetActor의 AbilitySystemComponent가 없습니다. TargetActor : %s"), *GetNameSafe(InTargetActor));
    checkf(SourceASC, TEXT("InstigatorActor의 AbilitySystemComponent가 없습니다. InstigatorActor : %s"), *GetNameSafe(InInstigator)); 
    checkf(InSpecHandle.IsValid(), TEXT("InSpecHandle가 유효하지 않습니다. TargetActor : %s"), *GetNameSafe(InTargetActor));

    FActiveGameplayEffectHandle ActivateGameplayEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*InSpecHandle.Data, TargetASC);

    return ActivateGameplayEffectHandle;
}

FActiveGameplayEffectHandle UPGFunctionLibrary::BP_ApplyGameplayEffectSpecHandleToTargetActor(AActor* InInstigator, AActor* InTargetActor, const FGameplayEffectSpecHandle& InSpecHandle, EPGSuccessType& OutSuccessType)
{
	FActiveGameplayEffectHandle ActivateGameplayEffectHandle = ApplyGameplayEffectSpecHandleToTargetActor(InInstigator, InTargetActor, InSpecHandle);

    OutSuccessType = ActivateGameplayEffectHandle.IsValid() ? EPGSuccessType::Successful : EPGSuccessType::Failed;

	return ActivateGameplayEffectHandle;
}

void UPGFunctionLibrary::NativeRemoveActiveGameplayEffectFromTarget(AActor* TargetActor, const FActiveGameplayEffectHandle& EffectHandle)
{
    // TargetActor의 AbilitySystemComponent 가져오기
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

    checkf(TargetASC, TEXT("TargetActor의 AbilitySystemComponent가 없습니다. TargetActor : %s"), *GetNameSafe(TargetActor));
    checkf(EffectHandle.IsValid(), TEXT("EffectHandle이 유효하지 않습니다. TargetActor : %s"), *GetNameSafe(TargetActor));

    TargetASC->RemoveActiveGameplayEffect(EffectHandle);
}

FGameplayEffectSpecHandle UPGFunctionLibrary::MakeOutgoingGameplayEffectSpec(UAbilitySystemComponent* SourceASC, TSubclassOf<UGameplayEffect> InEffectClass, AActor* Instigator, UObject* SourceObject, int32 InAbilityLevel)
{
    check(InEffectClass);

    // 게임 플레이 이펙트 컨텍스트 생성
    // 게임 플레이 이펙트 컨텍스트는 이펙트가 어디서 왔는지, 누가 적용했는지 등의 정보를 담고 있음
    FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
    ContextHandle.AddSourceObject(SourceObject);
    ContextHandle.AddInstigator(Instigator, Instigator);

    // 게임 플레이 이펙트 스펙 생성
    // 게임 플레이 이펙트 스펙은 이펙트의 구체적인 속성들을 담고 있음
    FGameplayEffectSpecHandle EffectSpecHandle = SourceASC->MakeOutgoingSpec(
        InEffectClass,
        InAbilityLevel,
        ContextHandle);

    return EffectSpecHandle;
}

FGameplayEffectSpecHandle UPGFunctionLibrary::MakeOutgoingGameplayEffectSpecWithMultiplier(UAbilitySystemComponent* SourceASC, TSubclassOf<UGameplayEffect> InEffectClass, float InSkillMultiplier, AActor* Instigator, UObject* SourceObject, int32 InAbilityLevel)
{
    check(InEffectClass);

    // 게임 플레이 이펙트 스펙 생성
    // 게임 플레이 이펙트 스펙은 이펙트의 구체적인 속성들을 담고 있음
    FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingGameplayEffectSpec(SourceASC, InEffectClass, Instigator, SourceObject, InAbilityLevel);

    // 이펙트 스펙에 SetByCaller 매개변수 설정하여 스킬 배율값을 전달
    EffectSpecHandle.Data->SetSetByCallerMagnitude(
        PGGameplayTags::Shared_SetByCaller_SkillMultiplier,
        InSkillMultiplier
    );

    return EffectSpecHandle;
}

FGameplayEffectSpecHandle UPGFunctionLibrary::MakeOutgoingGameplayEffectSpecFromEffectConfig(UAbilitySystemComponent* SourceASC, const FEffectConfig& InEffectConfig, AActor* Instigator, UObject* SourceObject, int32 InAbilityLevel)
{
    checkf(InEffectConfig.EffectClass, TEXT("EffectConfig의 EffectClass가 유효하지 않습니다."));

    // 게임 플레이 이펙트 스펙 생성
    FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingGameplayEffectSpec(SourceASC, InEffectConfig.EffectClass, Instigator, SourceObject, InAbilityLevel);

    const float MultiplierValue = InEffectConfig.Multiplier.GetValueAtLevel(InAbilityLevel);
    const float BaseAmountValue = InEffectConfig.BaseAmount.GetValueAtLevel(InAbilityLevel);
    const float DurationValue = InEffectConfig.Duration.GetValueAtLevel(InAbilityLevel);

    // EffectConfig에서 설정된 값들을 스펙에 적용
    if (!FMath::IsNearlyZero(MultiplierValue))
    {
        EffectSpecHandle.Data->SetSetByCallerMagnitude(
            PGGameplayTags::Shared_SetByCaller_SkillMultiplier,
            MultiplierValue 
        );
    }

    if (BaseAmountValue > 0.f)
    {
        EffectSpecHandle.Data->SetSetByCallerMagnitude(
            PGGameplayTags::Shared_SetByCaller_BaseAmount,
            BaseAmountValue
        );
    }

    if (DurationValue > 0.f)
    {
        EffectSpecHandle.Data->SetSetByCallerMagnitude(
            PGGameplayTags::Shared_SetByCaller_Duration,
            DurationValue
        );
    }

    return EffectSpecHandle;
}

TArray<FGameplayEffectSpecHandle> UPGFunctionLibrary::MakeOutgoingGameplayEffectSpecsFromEffectConfigs(UPGAbilitySystemComponent* SourceASC, const TArray<FEffectConfig>& EffectConfigs, AActor* Instigator, UObject* SourceObject, int32 InAbilityLevel)
{
    // 서버 체크
    //if(!SourceASC || !SourceASC->IsOwnerActorAuthoritative())
    //{
    //    return TArray<FGameplayEffectSpecHandle>();
    //}

    TArray<FGameplayEffectSpecHandle> EffectSpecHandles;

    for (const FEffectConfig& EffectConfig : EffectConfigs)
    {
        FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingGameplayEffectSpecFromEffectConfig(SourceASC, EffectConfig, Instigator, SourceObject, InAbilityLevel);
        if (EffectSpecHandle.IsValid())
        {
            EffectSpecHandles.Add(EffectSpecHandle);
        }
    }
    return EffectSpecHandles;
}

FGameplayTag UPGFunctionLibrary::GetCueVariantTagFromContext(const FGameplayEffectContextHandle& InContext, bool& bHasValidPGContext)
{
    bHasValidPGContext = false;

    const FGameplayEffectContext* BaseContext = InContext.Get();
    if (!BaseContext)
    {
        return FGameplayTag();
    }

    const UScriptStruct* StructType = BaseContext->GetScriptStruct();
    if (!StructType || !StructType->IsChildOf(FPGGameplayEffectContext::StaticStruct()))
    {
        return FGameplayTag();
    }

    const FPGGameplayEffectContext* PGContext = static_cast<const FPGGameplayEffectContext*>(BaseContext);

    bHasValidPGContext = true;
    return PGContext->CueVariantTag;
}

FGameplayTag UPGFunctionLibrary::GetCueVariantTagFromCueParams(const FGameplayCueParameters& InCueParams, bool& bHasValidPGContext)
{
    return GetCueVariantTagFromContext(InCueParams.EffectContext, bHasValidPGContext);
}
