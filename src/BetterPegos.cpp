#include "API/ARK/Ark.h"

struct FOutParmRec
{
    UProperty*& PropertyField() { return *GetNativePointerField<UProperty**>(this, "FOutParmRec.Property"); }
    uint8*& PropAddrField() { return *GetNativePointerField<uint8**>(this, "FOutParmRec.PropAddr"); }
    FOutParmRec*& NextOutParmField() { return *GetNativePointerField<FOutParmRec**>(this, "FOutParmRec.NextOutParm"); }
};

struct FFrame
{
    UFunction*& NodeField() { return *GetNativePointerField<UFunction**>(this, "FFrame.Node"); }
    uint8*& LocalsField() { return *GetNativePointerField<uint8**>(this, "FFrame.Locals"); }
    FOutParmRec*& OutParmsField() { return *GetNativePointerField<FOutParmRec**>(this, "FFrame.OutParms"); }
};

UClass* CryoClass;
UFunction* PegoCanStealFunc;
UProperty* ItemProp;
UProperty* CanStealProp;

DECLARE_HOOK(UObject_ProcessInternal, void, UObject*, FFrame*, void* const);
void Hook_UObject_ProcessInternal(UObject* _this, FFrame* Stack, void* const Result)
{
    UObject_ProcessInternal_original(_this, Stack, Result);

    if (Stack->NodeField() != PegoCanStealFunc)
        return;

    UPrimalItem* item = *(UPrimalItem**)(Stack->LocalsField() + ItemProp->Offset_InternalField());
    if (!item->IsA(CryoClass))
        return;

    bool* canSteal = nullptr;
    for (FOutParmRec* rec = Stack->OutParmsField(); rec; rec = rec->NextOutParmField()) {
        if (rec->PropertyField() == CanStealProp) {
            canSteal = (bool*)rec->PropAddrField();
            break;
        }
    }
    if (canSteal)
        *canSteal = false;
}

// Called when the server is ready
void OnServerReady()
{
    CryoClass = Globals::FindClass("BlueprintGeneratedClass /Game/Extinction/CoreBlueprints/Weapons/PrimalItem_WeaponEmptyCryopod.PrimalItem_WeaponEmptyCryopod_C");
    if (!CryoClass) {
        Log::GetLog()->error("UClass not found: PrimalItem_WeaponEmptyCryopod_C");
        return;
    }

    UClass* pegoClass = Globals::FindClass("BlueprintGeneratedClass /Game/PrimalEarth/Dinos/Pegomastax/Pegomastax_Character_BP.Pegomastax_Character_BP_C");
    if (!pegoClass) {
        Log::GetLog()->error("UClass not found: Pegomastax_Character_BP_C");
        return;
    }

    PegoCanStealFunc = pegoClass->FindFunctionByName(FName("CanStealItem", EFindName::FNAME_Find), EIncludeSuperFlag::ExcludeSuper);
    if (!PegoCanStealFunc) {
        Log::GetLog()->error("UFunction not found: Pegomastax_Character_BP_C::CanStealItem()");
        return;
    }

    for (UProperty* prop = PegoCanStealFunc->PropertyLinkField(); prop; prop = prop->PropertyLinkNextField()) {
        if (prop->NameField().ToString().Compare("Item") == 0)
            ItemProp = prop;
        else if (prop->NameField().ToString().Compare("canSteal") == 0)
            CanStealProp = prop;
    }
    if (!ItemProp)
        Log::GetLog()->error("UProperty not found: Item");
    if (!CanStealProp)
        Log::GetLog()->error("UProperty not found: canSteal");
    if (!ItemProp || !CanStealProp)
        PegoCanStealFunc = nullptr;
}

// Hook that triggers once when the server is ready
DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);
void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* _this)
{
    Log::GetLog()->info("Hook_AShooterGameMode_BeginPlay()");
    AShooterGameMode_BeginPlay_original(_this);

    // Call OnServerReady() for post-"server ready" initialization
    OnServerReady();
}

// Called by ArkServerApi when the plugin is loaded
extern "C" __declspec(dllexport) void Plugin_Init()
{
    Log::Get().Init(PROJECT_NAME);

    ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay", Hook_AShooterGameMode_BeginPlay,
        &AShooterGameMode_BeginPlay_original);
    ArkApi::GetHooks().SetHook("UObject.ProcessInternal", Hook_UObject_ProcessInternal,
        &UObject_ProcessInternal_original);

    // If the server is ready, call OnServerReady() for post-"server ready" initialization
    if (ArkApi::GetApiUtils().GetStatus() == ArkApi::ServerStatus::Ready)
        OnServerReady();
}

// Called by ArkServerApi when the plugin is unloaded
extern "C" __declspec(dllexport) void Plugin_Unload()
{
    ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", Hook_AShooterGameMode_BeginPlay);
    ArkApi::GetHooks().DisableHook("UObject.ProcessInternal", Hook_UObject_ProcessInternal);
}