// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "Types/PGEnumTypes.h"
#include "HeroCombatComponent.generated.h"

/**
 *
 */
UCLASS()
class UNREAL_PROJECTG_API UHeroCombatComponent : public UPawnExtensionComponentBase
{
    GENERATED_BODY()

public:
    UHeroCombatComponent();

    void SetCombatMode(EHeroCombatMode NewCombatMode); // 전투 모드 설정 함수
    EHeroCombatMode GetCombatMode() const { return CombatMode; } // 전투 모드 반환 함수
    bool IsAutoMode() const { return CombatMode == EHeroCombatMode::Auto; } // 자동 모드 여부 반환 함수
    void StartCombat();                               // 전투 시작 함수
    void StopCombat();                                // 전투 중지 함수
protected:
    virtual void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void ActivateManualCombat();    // 수동전투 활성화 함수
    void ActivateAutoCombat();      // 자동전투 활성화 함수
    void DeactivateCombat();        // 전투 비활성화 함수

private:

    void StartDetectTimer();                          // 탐색 타이머 시작 함수
    void StopDetectTimer();                           // 탐색 타이머 중지 함수

    void StartBasicAttackTimer();                     // 기본 공격 타이머 시작 함수
    void StopBasicAttackTimer();                      // 기본 공격 타이머 중지 함수
    void RefreshBasicAttackTimer();                   // 기본 공격 타이머 갱신 함수(공격 속도가 변경 되었을 때)

    void UpdateDetection();
    void DebugCurrentTarget() const;
    void HandleBasicAttack();                         // 기본 공격 처리 함수
    void HandleAutoCombat();                          // 자동 공격 처리 함수

    AActor* FindNearestEnemy() const;                 // 가장 가까운 적을 찾는 함수

    // 인터페이스 관련 함수
    float GetBasickAttackInterval() const;            // 기본 공격 간격 계산 함수
    float GetBasicAttackRange() const;                // 기본 공격 범위 계산 함수
    bool IsTargetInBasicAttackRange() const;          // 현재 타겟이 기본 공격 범위 내에 있는지 확인하는 함수
    bool CanUseCombatInterface() const;               // 전투 인터페이스 사용 가능 여부 확인 함수

    bool IsValidEnemy(AActor* TargetActor) const;     // 유효한 적인지 확인하는 함수

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TWeakObjectPtr<AActor> CurrentTarget; // 현재 타겟
private:
    UPROPERTY()
    TObjectPtr<ACharacter> OwningCharacter;

    UPROPERTY(EditAnywhere)
    float DetectRadius = 500.f; // 적 감지 반경(Target Actor를 잡는 반경)

    UPROPERTY(EditAnywhere)
    float DetectInterval = 0.25f; // 탐색 업데이트 간격

    UPROPERTY(EditAnywhere)
    EHeroCombatMode CombatMode = EHeroCombatMode::None; // 현재 전투 모드

    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bEnableCurrentTargetDebug = false;

    UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.01"))
    float CurrentTargetDebugDuration = 0.3f;

    FTimerHandle DetectTimerHandle; // 탐색 타이머를 관리하는 핸들
    FTimerHandle BasicAttackTimerHandle; // 기본 공격 타이머 핸들

    bool bWasTargetInAttackRange = false; // 타겟이 기본 공격 범위 내에 있었는지 여부를 추적하는 변수
};
