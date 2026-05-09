#include "AbilityEntryCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Types/AbilityConfig.h"
#include "ClassViewerModule.h"
#include "PropertyCustomizationHelpers.h"

// 어빌리티 클래스 헤더
#include "AbilitySystem/Abilities/Player/HeroAbility_SpawnPet.h"
#include "AbilitySystem/Abilities/Unit/UnitAbility_BaseMeleeAttack.h"
#include "AbilitySystem/Abilities/Unit/UnitAbility_SpawnProjectile.h"
#include "AbilitySystem/Abilities/Unit/UnitAbility_SpawnActor.h"
#include "AbilitySystem/Abilities/Shared/SharedAbility_BuffAura.h"


// 어빌리티 클래스 -> Config 클래스 매핑 테이블
// 새 어빌리티 혹은 Config 추가 시 이곳에 추가
static TMap<UClass*, UScriptStruct*> GetAbilityToConfigMap()
{
    TMap<UClass*, UScriptStruct*> Map;

    // UClass*는 StaticClass()로, UScriptStruct*는 StaticStruct()로 매핑
    // 스킬 클래스 추가 시 이곳에 매핑 추가
    Map.Add(UHeroAbility_SpawnPet::StaticClass(), FHeroSpawnPetAbilityConfig::StaticStruct());
    Map.Add(UUnitAbility_BaseMeleeAttack::StaticClass(), FUnitBaseMeleeAttackAbilityConfig::StaticStruct());
    Map.Add(UUnitAbility_SpawnProjectile::StaticClass(), FUnitSpawnProjectileAbilityConfig::StaticStruct());
    Map.Add(UUnitAbility_SpawnActor::StaticClass(), FUnitSpawnActorAbilityConfig::StaticStruct());
    Map.Add(USharedAbility_BuffAura::StaticClass(), FSharedBuffAuraAbilityConfig::StaticStruct());

    return Map;
}

// 헤더 부분 커스터마이징
void FAbilityEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils)
{
    HeaderRow.NameContent()
    [
        PropertyHandle->CreatePropertyNameWidget()
        // 글씨 색상 변경 등
        //SNew(STextBlock)
        //.Text(FText::FromString("커스텀 이름"))
        //.ColorAndOpacity(FLinearColor::Red)
    ];
}

// 자식 부분 커스터마이징
void FAbilityEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& Utils)
{
    // 초기화 플래그 On
    bIsInitializing = true;

    // 패널 업데이트용 Utils 저장
    PropertyUtilities = Utils.GetPropertyUtilities();

    // FAbilityEntry의 AbilityClass와 AbilityConfig 핸들 가져오기
    AbilityClassHandle = PropertyHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FAbilityEntry, AbilityClass));
    AbilityConfigHandle = PropertyHandle->GetChildHandle(
        GET_MEMBER_NAME_CHECKED(FAbilityEntry, AbilityConfig));

    // 현재 AbilityClass를 캐싱
    CachedAbilityClass = GetCurrentAbilityClass();

    // 진입점에서 현재 AbilityClass에 맞는 Config 클래스 필터링
    UpdateConfigStructFilter();

    // AbilityClass 변경 감지 -> Config 초기화
    // CreateSP는 객체를 TWeakPtr로 보관하고 호출 시 Pin()으로 유효성을 확인하므로, 
    // 객체가 소멸된 뒤에는 콜백을 실행하지 않아 댕글링 호출을 방지한다.
     AbilityClassHandle->SetOnPropertyValueChanged(
        FSimpleDelegate::CreateSP(this, &FAbilityEntryCustomization::OnAbilityClassChanged)); 
    //AbilityClassHandle->SetOnPropertyValueChanged(
        //FSimpleDelegate::CreateRaw(this, &FAbilityEntryCustomization::OnAbilityClassChanged));

    TSharedPtr<FBlueprintOnlyAbilityClassFilter> ClassFilter 
        = MakeShared<FBlueprintOnlyAbilityClassFilter>();
    ClassFilter->AllowedChildrenOfClasses.Add(UPGGameplayAbility::StaticClass());

    TArray<TSharedRef<IClassViewerFilter>> ClassFilters;
    ClassFilters.Add(ClassFilter.ToSharedRef());

    ChildBuilder.AddCustomRow(NSLOCTEXT("AbilityEntry", "AbilityClass", "Ability Class"))
        .NameContent()
        [
            AbilityClassHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        .MinDesiredWidth(TOptional<float>(250.f))
        [
            SNew(SClassPropertyEntryBox)
                // 베이스 클래스 지정
                .MetaClass(UPGGameplayAbility::StaticClass())
                //블루프린트 전용
                .IsBlueprintBaseOnly(true)
                // 추상 클래스 제외
                .AllowAbstract(false)
                // None 선택 허용
                .AllowNone(true)
                // 커스텀 클래스 필터 적용
                .ClassViewerFilters(ClassFilters)
                // 현재 선택된 클래스 반환
                .SelectedClass(this, &FAbilityEntryCustomization::GetCurrentAbilityClass)
                // 클래스 선택 시 호출
                .OnSetClass(this, &FAbilityEntryCustomization::OnAbilityClassSelected)
        ];

    ChildBuilder.AddProperty(AbilityConfigHandle.ToSharedRef())
        .Visibility(TAttribute<EVisibility>::Create(
            TAttribute<EVisibility>::FGetter::CreateSP(
                this, &FAbilityEntryCustomization::GetConfigVisibility))); // AbilityClass에 맞는 Config가 있을 때만 표시

    // 초기화 플래그 Off
    bIsInitializing = false;
}

void FAbilityEntryCustomization::UpdateConfigStructFilter()
{
    if (!AbilityConfigHandle.IsValid() || !AbilityConfigHandle->IsValidHandle()) return;

    FProperty* ConfigProperty = AbilityConfigHandle->GetProperty();
    if (!ConfigProperty) return;

    // 현재 선택된 어빌리티 클래스에 맞는 Config 구조체가 아니면 AbilityConfig 필터링
    UScriptStruct* RequiredStruct = GetRequiredConfigStruct();
    if (!RequiredStruct) return;

    // ConfigProperty에 BaseStruct 메타데이터 설정 -> DetailsView에서 이 메타데이터를 읽어서 필터링
    ConfigProperty->SetMetaData(TEXT("BaseStruct"), *RequiredStruct->GetPathName());
}

void FAbilityEntryCustomization::OnAbilityClassChanged()
{
    // IsValid()는 포인터가 NUllL이 아닌지만 체크, IsValidHandle()은 핸들이 유효한지 체크
    if (!AbilityConfigHandle.IsValid() || !AbilityConfigHandle->IsValidHandle()) return;
    if (!AbilityClassHandle.IsValid() || !AbilityClassHandle->IsValidHandle()) return;

    // 초기화 중이면 로직 실행하지 않음
    if (bIsInitializing) return;

    // 실제 클래스가 변경 되었는지를 확인
    const UClass* NewClass = GetCurrentAbilityClass();
    if (NewClass == CachedAbilityClass) return;
    // 실제 클래스가 변경되었으므로 캐싱된 클래스 업데이트
    CachedAbilityClass = NewClass;

    UpdateConfigStructFilter();

    // FInstancedStruct로 SetValueFromFormmatedString("None")으로 초기화
    TSharedPtr<IPropertyHandle> LocalHandle = AbilityConfigHandle; // 람다 캡처용 로컬 변수
    GEditor->GetTimerManager()->SetTimerForNextTick([LocalHandle]()
        {
            if(LocalHandle.IsValid() && LocalHandle->IsValidHandle())
            {
                LocalHandle->SetValueFromFormattedString(TEXT("None"));
            }
        });

    // 패널 새로고침
    if (PropertyUtilities.IsValid())
    {
        PropertyUtilities->RequestForceRefresh();
    }
}

UScriptStruct* FAbilityEntryCustomization::GetRequiredConfigStruct() const
{
    if (!AbilityClassHandle.IsValid() || !AbilityClassHandle->IsValidHandle()) return FAbilityConfig::StaticStruct();

    //현재 선택된 AbilityClass 가져오기
    UObject* AbilityClassObj = nullptr;
    AbilityClassHandle->GetValue(AbilityClassObj);
    UClass* AbilityClass = Cast<UClass>(AbilityClassObj);

    if (!AbilityClass) return FAbilityConfig::StaticStruct();

    // 매핑 테이블에서 Config 클래스 찾기
    const TMap<UClass*, UScriptStruct*> AbilityToConfigMap = GetAbilityToConfigMap();

    // 정확한 클래스 또는 부모 클래스 기준으로 검색
    for(const auto& Pair : AbilityToConfigMap)
    {
        if (AbilityClass == Pair.Key) return Pair.Value;
    }

    // 더 깊은 부모 클래스를 우선 매칭
    UClass* BestMatchKey = nullptr;
    UScriptStruct* BestMatchValue = nullptr;
    for(const auto& Pair : AbilityToConfigMap)
    {
        if (AbilityClass->IsChildOf(Pair.Key))
        {
            if (!BestMatchKey || Pair.Key->IsChildOf(BestMatchKey))
            {
                BestMatchKey = Pair.Key;
                BestMatchValue = Pair.Value;
            }
        }
    }

    return BestMatchValue ? BestMatchValue : FAbilityConfig::StaticStruct();
}

EVisibility FAbilityEntryCustomization::GetConfigVisibility() const
{
    // AbilityClass가 유효하지 않으면 Config 숨김
    if (!AbilityClassHandle.IsValid()) return EVisibility::Collapsed;
    
    UObject* AbilityClassObj = nullptr;
    AbilityClassHandle->GetValue(AbilityClassObj);

    // 어빌리티 클래스에 매핑되는 Config가 있다면 표시, 없으면 숨김
    return AbilityClassObj ? EVisibility::Visible : EVisibility::Collapsed;
}

const UClass* FAbilityEntryCustomization::GetCurrentAbilityClass() const
{
    if (!AbilityClassHandle.IsValid()) return nullptr;

    UObject* AbilityClassObj = nullptr;
    AbilityClassHandle->GetValue(AbilityClassObj);
    return Cast<UClass>(AbilityClassObj);
}

void FAbilityEntryCustomization::OnAbilityClassSelected(const UClass* NewClass)
{
    if (!AbilityClassHandle.IsValid()) return;

    AbilityClassHandle->SetValueFromFormattedString(
        NewClass ? NewClass->GetPathName() : TEXT("None"));
}

