// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Types/PGGameplayEffectContext.h"

UScriptStruct* FPGGameplayEffectContext::GetScriptStruct() const
{
    return StaticStruct();
}

FGameplayEffectContext* FPGGameplayEffectContext::Duplicate() const
{
    FPGGameplayEffectContext* NewContext = new FPGGameplayEffectContext();
    *NewContext = *this;

    if (GetHitResult())
    {
        NewContext->AddHitResult(*GetHitResult(), true);
    }

    return NewContext;
}

bool FPGGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    // FGameplayEffectContext의 NetSerialize를 먼저 호출하여 기본 필드들을 직렬화/역직렬화합니다.
    // 직렬화/역직렬화를 통해 데이터를 패킷에 담을 수 있는 바이트 스트림으로 변환
    const bool bSuperResult = FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

    // FGameplayTag은 자체적으로 NetSerialize 함수를 구현하고 있으므로, 이를 호출하여 직렬화/역직렬화합니다.
    bool bTagSerializeSuccess = true;
    CueVariantTag.NetSerialize(Ar, Map, bTagSerializeSuccess);

    // 두 직렬화 결과를 모두 고려하여 최종 성공 여부를 결정합니다.
    bOutSuccess = bOutSuccess && bSuperResult && bTagSerializeSuccess;
    return true;
}
