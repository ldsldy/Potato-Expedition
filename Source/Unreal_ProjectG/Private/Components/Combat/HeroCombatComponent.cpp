// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Combat/HeroCombatComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "PGGameplayTags.h"
#include "Character/PGCharacterBase.h"
#include "Interfaces/HeroCombatInterface.h"
#include "GameFramework/CharacterMovementComponent.h"

UHeroCombatComponent::UHeroCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.SetTickFunctionEnable(false);
}

void UHeroCombatComponent::SetCombatMode(EHeroCombatMode NewCombatMode)
{
    if (CombatMode == NewCombatMode) return;
    CombatMode = NewCombatMode;
    switch (CombatMode)
    {
    case EHeroCombatMode::Manual:
        ActivateManualCombat();
        break;
    case EHeroCombatMode::Auto:
        ActivateAutoCombat();
        break;
    case EHeroCombatMode::None:
        DeactivateCombat();
        break;
    default:
        break;
    }
}

void UHeroCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    OwningCharacter = Cast<ACharacter>(GetOwner());
}

void UHeroCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwningCharacter || CombatMode == EHeroCombatMode::None || !CurrentTarget.IsValid())
    {
        SetComponentTickEnabled(false); // 전투 모드가 None이면 Tick 비활성화
        return;
    }

    FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
        OwningCharacter->GetActorLocation(),
        CurrentTarget->GetActorLocation()
    );

    const FRotator CurrentControlRot = OwningCharacter->GetControlRotation();
    const FRotator TargetRot = FMath::RInterpTo(CurrentControlRot, LookAtRot, DeltaTime, 10.f);

    OwningCharacter->GetController()->SetControlRotation(FRotator(TargetRot.Pitch, TargetRot.Yaw, 0.f));
    OwningCharacter->SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
}

void UHeroCombatComponent::ActivateManualCombat()
{
    CombatMode = EHeroCombatMode::Manual;
    StartCombat();
    SetComponentTickEnabled(false); // Manual 모드에서는 타깃이 있을 때만 Tick이 활성화 되어야 하므로, 일단 비활성화 상태로 시작
}

void UHeroCombatComponent::ActivateAutoCombat()
{
    CombatMode = EHeroCombatMode::Auto;
    StartCombat();
    SetComponentTickEnabled(true);
}

void UHeroCombatComponent::DeactivateCombat()
{
    CombatMode = EHeroCombatMode::None;
    CurrentTarget = nullptr;
    StopCombat();
    SetComponentTickEnabled(false);
}

void UHeroCombatComponent::StartCombat()
{
    StartDetectTimer();
    StartBasicAttackTimer();
}
void UHeroCombatComponent::StopCombat()
{
    StopDetectTimer();
    StopBasicAttackTimer();
}

void UHeroCombatComponent::StartDetectTimer()
{
    // 반복하여 UpdateDetection 함수를 호출하는 타이머 설정
    if (UWorld* World = GetWorld())
    {
        if (!World->GetTimerManager().IsTimerActive(DetectTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                DetectTimerHandle,
                this,
                &UHeroCombatComponent::UpdateDetection,
                DetectInterval,
                true
            );
        }
    }
}

void UHeroCombatComponent::StopDetectTimer()
{
    // 탐색 타이머 중지
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DetectTimerHandle);
    }
}

void UHeroCombatComponent::StartBasicAttackTimer()
{
    if (UWorld* World = GetWorld())
    {
        const float AttackInterval = GetBasickAttackInterval();
        if (AttackInterval > 0.f)
        {
            World->GetTimerManager().SetTimer(
                BasicAttackTimerHandle,
                this,
                &UHeroCombatComponent::HandleBasicAttack,
                AttackInterval,
                true
            );
        }
    }
}

void UHeroCombatComponent::StopBasicAttackTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BasicAttackTimerHandle);
    }
}

void UHeroCombatComponent::RefreshBasicAttackTimer()
{
    StopBasicAttackTimer();
    StartBasicAttackTimer();
}

void UHeroCombatComponent::UpdateDetection()
{
    if (!OwningCharacter || CombatMode == EHeroCombatMode::None)
    {
        SetComponentTickEnabled(false); // 전투 모드가 None이면 Tick 비활성화
        return;
    }

    // 현재 타깃은 가장 가까운 적으로 변경
    CurrentTarget = FindNearestEnemy();
    DebugCurrentTarget();

    // 유효한 타깃이 없으면 틱을 종료하면서 캐릭터 회전을 Movement 방향으로 돌려놓음
    // Auto 모드라면 항상 틱이 활성화 되어 있어야 하지만, Manual 모드에서는 타깃이 없을 때 틱을 비활성화하여 불필요한 연산을 줄임
    if (!CurrentTarget.IsValid() && CombatMode == EHeroCombatMode::Manual)
    {
        SetComponentTickEnabled(false);
        if (OwningCharacter->GetCharacterMovement())
        {
            OwningCharacter->GetCharacterMovement()->bOrientRotationToMovement = true; // Movement 방향으로 회전하도록 설정
            OwningCharacter->GetCharacterMovement()->MaxWalkSpeed = 500.f;  // TODO: 기본 이동 속도는 캐릭터 데이터에서 가져오도록 변경 필요
        }
        return;
    }
    else
    {
        SetComponentTickEnabled(true); // 유효한 타깃이 있으면 Tick 활성화, 없으면 비활성화
        OwningCharacter->GetCharacterMovement()->bOrientRotationToMovement = false; // 회전을 캐릭터 방향으로 고정
        OwningCharacter->GetCharacterMovement()->MaxWalkSpeed = 300.f;  // TODO: 전투 이동 속도는 캐릭터 데이터에서 가져오도록 변경 필요
    }
}

void UHeroCombatComponent::DebugCurrentTarget() const
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return;
#else
    if (!bEnableCurrentTargetDebug || !OwningCharacter)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector OwnerLocation = OwningCharacter->GetActorLocation();
    const FVector OwnerDrawLocation = OwnerLocation + FVector(0.f, 0.f, 50.f);
    const float DebugDuration = FMath::Max(CurrentTargetDebugDuration, 0.01f);

    if (!CurrentTarget.IsValid())
    {
        const FVector ForwardEnd = OwnerDrawLocation + (OwningCharacter->GetActorForwardVector() * 100.f);
        DrawDebugLine(World, OwnerDrawLocation, ForwardEnd, FColor::Red, false, DebugDuration, 0, 1.5f);
        DrawDebugSphere(World, OwnerDrawLocation, 20.f, 12, FColor::Red, false, DebugDuration, 0, 1.5f);
        DrawDebugString(World, OwnerDrawLocation + FVector(0.f, 0.f, 35.f), TEXT("CurrentTarget: None"), nullptr, FColor::Red, DebugDuration, false);
        return;
    }

    const FVector TargetLocation = CurrentTarget->GetActorLocation();
    const FVector TargetDrawLocation = TargetLocation + FVector(0.f, 0.f, 50.f);
    const float Distance = FVector::Dist(OwnerLocation, TargetLocation);
    const FString DebugText = FString::Printf(
        TEXT("CurrentTarget: %s\nDistance: %.1f"),
        *CurrentTarget->GetName(),
        Distance
    );

    DrawDebugLine(World, OwnerDrawLocation, TargetDrawLocation, FColor::Yellow, false, DebugDuration, 0, 1.8f);
    DrawDebugSphere(World, TargetDrawLocation, 35.f, 12, FColor::Green, false, DebugDuration, 0, 1.8f);
    DrawDebugString(World, TargetDrawLocation + FVector(0.f, 0.f, 45.f), DebugText, nullptr, FColor::White, DebugDuration, false);
#endif
}

void UHeroCombatComponent::HandleBasicAttack()
{
    if (!OwningCharacter || !CurrentTarget.IsValid() || CombatMode == EHeroCombatMode::None) return;

    if (!IsTargetInBasicAttackRange())
    {
        return; // 타깃이 기본 공격 범위 내에 없으면 공격하지 않음
    }

    if (CanUseCombatInterface())
    {
        IHeroCombatInterface::Execute_TryExecuteBasicAttack(OwningCharacter);
    }
}

AActor* UHeroCombatComponent::FindNearestEnemy() const
{
    if (!OwningCharacter) return nullptr;

    TArray<AActor*> OverlapActors;
    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn)); // Pawn 타입의 오브젝트만 감지

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(OwningCharacter); // 자신은 감지에서 제외

    // 감지 반경 내의 Pawn 타입 오브젝트들을 감지
    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        OwningCharacter->GetActorLocation(),
        DetectRadius,
        ObjectTypes,
        APGCharacterBase::StaticClass(), // APGCharacterBase 클래스와 그 자식 클래스만 감지
        IgnoredActors,
        OverlapActors
    );

    AActor* ClosestEnemy = nullptr;
    float MinDistanceSquared = FLT_MAX;

    // 감지된 오브젝트들 중에서 가장 가까운 적을 찾음
    for (AActor* Actor : OverlapActors)
    {
        if (IsValidEnemy(Actor))
        {
            float DistanceSquared = OwningCharacter->GetSquaredDistanceTo(Actor);
            if (DistanceSquared < MinDistanceSquared)
            {
                MinDistanceSquared = DistanceSquared;
                ClosestEnemy = Actor;
            }
        }
    }

    return ClosestEnemy;
}

bool UHeroCombatComponent::IsValidEnemy(AActor* TargetActor) const
{
    APGCharacterBase* TargetCharacter = Cast<APGCharacterBase>(TargetActor);
    if (!TargetCharacter)
    {
        return false; // APGCharacterBase가 아닌 경우 유효하지 않은 적
    }

    return TargetCharacter->GetTeamTag().MatchesTag(PGGameplayTags::Unit_Side_Foe); // 적 팀 태그와 일치하는지 확인
}

float UHeroCombatComponent::GetBasickAttackInterval() const
{
    if (!OwningCharacter) return 1.f;

    if (CanUseCombatInterface())
    {
        const float AttackSpeed = IHeroCombatInterface::Execute_GetBasicAttackSpeed(OwningCharacter);
        return AttackSpeed > 0.f ? 1.f / AttackSpeed : 1.f; // 공격 속도가 양수인 경우 간격 계산, 그렇지 않으면 기본값 1초
    }
    return 1.f;
}

float UHeroCombatComponent::GetBasicAttackRange() const
{
    if (CanUseCombatInterface())
    {
        return IHeroCombatInterface::Execute_GetBasicAttackRange(OwningCharacter);
    }
    return 0.f;
}

bool UHeroCombatComponent::IsTargetInBasicAttackRange() const
{
    if (!OwningCharacter || !CurrentTarget.IsValid()) return false;

    const float AttackRange = GetBasicAttackRange();
    const float DistanceSquared = OwningCharacter->GetSquaredDistanceTo(CurrentTarget.Get());

    if (AttackRange <= 0.f)
    {
        return false; // 공격 범위가 0 이하인 경우 항상 범위 밖으로 간주
    }

    // 거리 제곱과 공격 범위 제곱을 비교하여 범위 내 여부 판단 (제곱근 계산 없이 성능 최적화)
    return DistanceSquared <= FMath::Square(AttackRange);
}

bool UHeroCombatComponent::CanUseCombatInterface() const
{
    return OwningCharacter && OwningCharacter->GetClass()->ImplementsInterface(UHeroCombatInterface::StaticClass());
}
