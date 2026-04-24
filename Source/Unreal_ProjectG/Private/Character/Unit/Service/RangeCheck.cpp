// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Unit/Service/RangeCheck.h"
#include "Character/Unit/AI/UnitDetourCrowdAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/Unit/SubSystem/UnitSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "GameplayTagAssetInterface.h"
#include "PGGameplayTags.h"
#include "DrawDebugHelpers.h"

URangeCheck::URangeCheck()
{
    NodeName = "RangeCheck";
    Interval = 0.5f;
    RandomDeviation = 0.1f;
}

void URangeCheck::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AUnitDetourCrowdAIController* AIC = Cast<AUnitDetourCrowdAIController>(OwnerComp.GetAIOwner());

    APawn* ControllPawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr;
    if (!ControllPawn)
    {
        return;
    }

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (!Blackboard)
    {
        return;
    }

    UUnitSubsystem* UnitSubSystem = GetWorld()->GetSubsystem<UUnitSubsystem>();
    if (!UnitSubSystem)
    {
        return;
    }

    float Range = Blackboard->GetValueAsFloat(DetectRangeKey.SelectedKeyName);
    float RangeSq = Range * Range;

    float AttackRange = Blackboard->GetValueAsFloat(AttackRangeKey.SelectedKeyName);
    float AttackRangeSq = AttackRange * AttackRange;

    IGameplayTagAssetInterface* UnitInterface = Cast<IGameplayTagAssetInterface>(ControllPawn);
    if (!UnitInterface)
    {
        return;
    }

    FGameplayTag EnemyTag = UnitInterface->HasMatchingGameplayTag(PGGameplayTags::Unit_Side_Ally)
        ? PGGameplayTags::Unit_Side_Foe
        : PGGameplayTags::Unit_Side_Ally;

    const TArray<TWeakObjectPtr<AActor>>& EnemyList = UnitSubSystem->GetUnitsByTeam(EnemyTag);

    AActor* TargetEnemy = nullptr;
    float MinDistSq = RangeSq;
    bool bInDetectRange = false;
    bool bInAttackRange = false;

    FVector MyLocation = ControllPawn->GetActorLocation();

    // [추가됨] 내 캐릭터의 캡슐 반경 구하기
    float MyRadius = 0.0f;
    UCapsuleComponent* MyCapsule = ControllPawn->FindComponentByClass<UCapsuleComponent>();
    if (MyCapsule)
    {
        MyRadius = MyCapsule->GetScaledCapsuleRadius();
    }

    for (const TWeakObjectPtr<AActor>& EnemyPtr : EnemyList)
    {
        AActor* Enemy = EnemyPtr.Get();
        if (!Enemy) continue;

        // [수정됨] 1. Z축을 무시한 2D 평면상의 '중심점 간 거리'
        float CenterToCenterDist = FVector::DistXY(MyLocation, Enemy->GetActorLocation());

        // [추가됨] 2. 적의 캡슐 반경 구하기
        float EnemyRadius = 0.0f;
        UCapsuleComponent* EnemyCapsule = Enemy->FindComponentByClass<UCapsuleComponent>();
        if (EnemyCapsule)
        {
            EnemyRadius = EnemyCapsule->GetScaledCapsuleRadius();
        }

        // [추가됨] 3. 가장자리 간 거리 산출 (캡슐이 겹쳤을 때 음수가 되는 것 방지)
        float EdgeToEdgeDist = FMath::Max(0.0f, CenterToCenterDist - MyRadius - EnemyRadius);

        // [수정됨] 4. 거리를 제곱값으로 변환하여 비교
        float DistSq = EdgeToEdgeDist * EdgeToEdgeDist;

        if (DistSq <= MinDistSq)
        {
            MinDistSq = DistSq;
            TargetEnemy = Enemy;
            bInDetectRange = true;
        }
    }

    if (bInDetectRange && TargetEnemy)
    {
        if (MinDistSq <= AttackRangeSq)
        {
            bInAttackRange = true;
        }
    }

    Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, TargetEnemy);
    Blackboard->SetValueAsBool(IsInDetectRangeKey.SelectedKeyName, bInDetectRange);
    Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, bInAttackRange);

    //DrawDebugCircle(
    //    GetWorld(),
    //    ControllPawn->GetActorLocation(),
    //    AttackRange,
    //    48,
    //    FColor::Red,
    //    false,
    //    Interval + 0.1f,
    //    0,
    //    2.0f,
    //    FVector(1, 0, 0),
    //    FVector(0, 1, 0),
    //    false
    //);
    //DrawDebugCircle(
    //    GetWorld(),
    //    ControllPawn->GetActorLocation(),
    //    Range,
    //    48,
    //    FColor::Green,
    //    false,
    //    Interval + 0.1f,
    //    0,
    //    2.0f,
    //    FVector(1, 0, 0),
    //    FVector(0, 1, 0),
    //    false
    //);

    if (TargetEnemy)
    {
        // [수정됨] 시작점과 끝점을 명시적으로 분리
        FVector StartLoc = ControllPawn->GetActorLocation();
        FVector EndLoc = TargetEnemy->GetActorLocation();

        // [수정됨] 디버그 라인이 수평으로 그려지도록, 끝점의 Z(높이) 값을 시작점과 동일하게 맞춤
        EndLoc.Z = StartLoc.Z;

        //DrawDebugLine(
        //    GetWorld(),
        //    StartLoc,
        //    EndLoc,
        //    FColor::Yellow, // 타겟 연결선은 노란색
        //    false,
        //    Interval + 0.1f,
        //    0,
        //    1.5f
        //);
    }

    if (AIC)
    {
        AIC->SetUnitState(bInDetectRange ? EUnitState::Combat : EUnitState::Move);
    }
} 
