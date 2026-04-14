// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "EquipmentsStorageComponent.generated.h"

class UDataAsset_WeaponData;
class UDataAsset_ArmorData;
class UDataAsset_AccessoryData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponAbilitiesActivate);
/**
 * 
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREAL_PROJECTG_API UEquipmentsStorageComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()
	
public:
    UFUNCTION(BlueprintCallable, Category = "Combat")
    TArray<FGameplayAbilitySpecHandle> GetSkillAbilitySpecHandles() const { return GrantedWeaponAbilitySpecHandles; }

    //게임 시작시 호출
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void Startup(UDataAsset_WeaponData* InWeaponData, UDataAsset_ArmorData* InArmorData, UDataAsset_AccessoryData* InAccessoryData);

    // ==============================================================
    // 장비 장착 함수들
    // ==============================================================
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipHeroWeapon(UDataAsset_WeaponData* InWeaponData);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipHeroArmor(UDataAsset_ArmorData* InArmorData);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void EquipHeroAccessory(UDataAsset_AccessoryData* InAccessoryData);

public:
    TWeakObjectPtr<UStaticMeshComponent> CachedWeaponMeshComponent;
    
    // 나중에 태그나 다른 타입도 고려
    UPROPERTY(BlueprintReadWrite, Category = "Combat|Property")
    bool bWeaponEquipped = false;

    FOnWeaponAbilitiesActivate OnWeaponAbilitiesActivate;

protected:
    // 어빌리티 스펙 핸들 저장 배열
    UPROPERTY(VisibleAnywhere, Category = "Combat|Data")
    TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilitySpecHandles;
};
