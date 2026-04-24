// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Types/PGEnumTypes.h"
#include "Types/AbilityConfig.h"
#include "ActiveGameplayEffectHandle.h"
#include "PGFunctionLibrary.generated.h"

class UPGAbilitySystemComponent;
class UHeroCombatComponent;
struct FGameplayEffectSpecHandle;
struct FGameplayAttribute;
struct FGameplayEffectContextHandle;
struct FGameplayCueParameters;
/**
 * 
 */
UCLASS()
class UNREAL_PROJECTG_API UPGFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
    static UPGAbilitySystemComponent* NativeGetPGASCFromActor(AActor* InActor);

    static UHeroCombatComponent* NativeGetCombatComponentFromActor(AActor* InActor);

    // 타겟 액터가 특정 태그를 가지고 있는지 확인하는 네이티브 함수
    static bool NativeDoesActorHaveTag(AActor* InActor, FGameplayTag TagToCheck);

    // 타겟 액터가 적대적인지 확인하는 함수
    UFUNCTION(BlueprintCallable, Category = "ProjectG|FunctionLibrary")
    static bool IsTargetCharacterHostile(AActor* InInstigator, AActor* InTargetActor);

    // Map<Attribute, FGameplayTag>로 등록된 전체 매핑 반환 (Effect 초기화용)
    static const TMap<FGameplayAttribute, FGameplayTag>& GetAttributeTagMap();

    // Attribute와 매핑된 SetByCaller 태그를 반환하는 함수
    static FGameplayTag GetSetByCallerTagForAttribute(const FGameplayAttribute& Attribute);

    // ASC에서 SetTag의 갯수를 조회하는 함수
    static const int32 ResolveSetPieceCount(const UAbilitySystemComponent* InASC, const FGameplayTag& RequiredSetTag);

    // ====================================================================================================
    // Effect 관련 헬퍼 함수들
    // ====================================================================================================

    // 게임플레이 이펙트 스펙 핸들을 타겟 액터에 적용하는 함수
    UFUNCTION(BlueprintCallable, Category = "ProjectG|FunctionLibrary")
    static FActiveGameplayEffectHandle ApplyGameplayEffectSpecHandleToTargetActor(AActor* InInstigator, AActor* InTargetActor, const FGameplayEffectSpecHandle& InSpecHandle);

    UFUNCTION(BlueprintCallable, Category = "ProjectG|FunctionLibrary", meta = (DisplayName = "Apply Gameplay Effect Spec Handle From Instigator To Target Actor", ExpandEnumAsExecs = "OutSuccessType"))
    static FActiveGameplayEffectHandle BP_ApplyGameplayEffectSpecHandleToTargetActor(AActor* InInstigator, AActor* InTargetActor, const FGameplayEffectSpecHandle& InSpecHandle, EPGSuccessType& OutSuccessType);

    UFUNCTION(BlueprintCallable, Category = "ProjectG|FunctionLibrary")
    static void NativeRemoveActiveGameplayEffectFromTarget(AActor* TargetActor, const FActiveGameplayEffectHandle& EffectHandle);

    //================================================================================================
    // 다양한 파라미터를 받아서 이펙트 스펙 핸들을 생성하는 헬퍼 함수들
    //================================================================================================

    // 이펙트 클래스만 받아서 이펙트 스펙 핸들을 생성하는 함수
    UFUNCTION(BlueprintPure, Category = "ProjectG|FunctionLibrary")
    static FGameplayEffectSpecHandle MakeOutgoingGameplayEffectSpec(UAbilitySystemComponent* SourceASC, TSubclassOf<UGameplayEffect> InEffectClass, AActor* Instigator,UObject* SourceObject, int32 InAbilityLevel = 1);

    // 스킬 배율과 함께 이펙트 클래스 받아서 이펙트 스펙 핸들을 생성하는 함수
    UFUNCTION(BlueprintPure, Category = "ProjectG|FunctionLibrary")
    static FGameplayEffectSpecHandle MakeOutgoingGameplayEffectSpecWithMultiplier(UAbilitySystemComponent* SourceASC, TSubclassOf<UGameplayEffect> InEffectClass, float InSkillMultiplier, AActor* Instigator,UObject* SourceObject, int32 InAbilityLevel = 1);
    
    // EffectConfig을 받아서 이펙트 스펙 핸들을 생성하는 함수
    UFUNCTION(BlueprintPure, Category = "ProjectG|FunctionLibrary")
    static FGameplayEffectSpecHandle MakeOutgoingGameplayEffectSpecFromEffectConfig(UAbilitySystemComponent* SourceASC, const FEffectConfig& InEffectConfig, AActor* Instigator, UObject* SourceObject, int32 InAbilityLevel = 1);

    // EffectConfig 배열을 받아서 이펙트 스펙 핸들 배열을 생성하는 함수
    UFUNCTION(BlueprintPure, Category = "ProjectG|FunctionLibrary")
    static TArray<FGameplayEffectSpecHandle> MakeOutgoingGameplayEffectSpecsFromEffectConfigs(UPGAbilitySystemComponent* SourceASC, const TArray<FEffectConfig>& EffectConfigs, AActor* Instigator, UObject* SourceObject, int32 InAbilityLevel = 1);

    //================================================================================================
    // 게임플레이 큐 관련 함수들
    //================================================================================================
    // Actor Cue: GE Context에 태그 정보를 주입
    static FGameplayEffectContextHandle  NativeAddActorCueIntoSpecHandle(FGameplayEffectSpecHandle& InOutSpecHandle, const FEffectConfig& InEffectConfig);

    // Static Cue: GE Context에 태그 정보를 주입하여 실행
    static void NativeExecuteStaticCue(AActor* TargetActor, const FEffectConfig& InEffectConfig, const FGameplayEffectContextHandle& EffectContext);

    //================================================================================================
    // 게임플레이 큐 관련 헬퍼 함수들
    //================================================================================================

    UFUNCTION(BlueprintPure, Category = "PG|Cue")
    static FGameplayTag GetCueVariantTagFromContext(const FGameplayEffectContextHandle& InContext, bool& bHasValidPGContext);

    UFUNCTION(BlueprintPure, Category = "PG|Cue")
    static FGameplayTag GetCueVariantTagFromCueParams(const FGameplayCueParameters& InCueParams, bool& bHasValidPGContext);


};
