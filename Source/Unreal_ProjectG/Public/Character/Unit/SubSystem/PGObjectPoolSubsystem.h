// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PGObjectPoolSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FActorPoolArray
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<AActor*> Actors;
};
/**
 * 
 */
UCLASS()
class UNREAL_PROJECTG_API UPGObjectPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
    // 풀에 이미 존재하는 액터만 꺼내기 (없으면 nullptr 반환, 신규 스폰하지 않음)
    UFUNCTION(BlueprintCallable, Category = "Pooling")
    AActor* TryGetPooledActor(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform);

    // 풀에서 액터 꺼내기
    UFUNCTION(BlueprintCallable, Category = "Pooling")
    AActor* GetActorFromPool(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform);

    // 다 쓴 액터를 풀에 반납하기
    UFUNCTION(BlueprintCallable, Category = "Pooling")
    void ReturnActorToPool(AActor* ActorToReturn);

    UFUNCTION(BlueprintCallable, Category = "Pooling")
    void PrewarmPool(TSubclassOf<AActor> ActorClass, int32 PoolSize);
private:
    // 클래스별로 보관하는 풀
    UPROPERTY()
    TMap<UClass*, FActorPoolArray> ActorPool;
};
