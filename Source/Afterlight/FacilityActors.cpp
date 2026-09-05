#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Components/RectLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AFacilityLight::AFacilityLight()
{
    PrimaryActorTick.bCanEverTick = false;
    Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Housing"));
    SetRootComponent(Housing);
    Housing->SetStaticMesh(Afterlight::Shape());
    Housing->SetCollisionProfileName(TEXT("BlockAll"));
    Housing->SetMobility(EComponentMobility::Movable);
    Lens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Diffuser"));
    Lens->SetupAttachment(Housing);
    Lens->SetStaticMesh(Afterlight::Shape());
    Lens->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light = CreateDefaultSubobject<URectLightComponent>(TEXT("RayTracedAreaLight"));
    Light->SetupAttachment(Housing);
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetCastShadows(true);
    Light->SetCastRaytracedShadows(ECastRayTracedShadow::Enabled);
    Light->bAllowMegaLights = true;
    Light->MegaLightsShadowMethod = EMegaLightsShadowMethod::RayTracing;
    Light->SetCastVolumetricShadow(true);
    Light->IntensityUnits = ELightUnits::Lumens;
    Light->SetAttenuationRadius(1050);
    Light->SetSourceWidth(110);
    Light->SetSourceHeight(14);
    Light->SetVolumetricScatteringIntensity(0.65f);
    Afterlight::Shadow(Housing);
    Afterlight::Shadow(Lens);
}

void AFacilityLight::Configure(FVector Location, FLinearColor Color, float Lumens, int32 InCircuit, FRotator Rotation)
{
    Circuit = InCircuit;
    RatedLumens = Lumens;
    LampColor = Color;
    SetActorLocationAndRotation(Location, Rotation);
    // Geometry is scaled independently so the light transform retains physical units.
    Housing->SetWorldScale3D(FVector(0.12f,1.26f,0.24f));
    Housing->SetMaterial(0, Afterlight::Material(TEXT("DarkMetal")));
    Lens->SetAbsolute(false, false, true);
    Lens->SetWorldScale3D(FVector(0.024f,1.10f,0.14f));
    Lens->SetRelativeLocation(FVector(57,0,0));
    Light->SetAbsolute(false, false, true);
    Light->SetRelativeLocation(FVector(70,0,0));
    Light->SetWorldScale3D(FVector::OneVector);
    LensMaterial = UMaterialInstanceDynamic::Create(Afterlight::Material(TEXT("Lamp")), this);
    Lens->SetMaterial(0, LensMaterial);
    Light->SetLightColor(Color);
    ApplyState();
}

void AFacilityLight::ApplyState()
{
    const bool bOn = IsLit();
    Light->SetVisibility(bOn);
    Light->SetIntensity(bOn ? RatedLumens : 0);
    if (LensMaterial)
    {
        LensMaterial->SetVectorParameterValue(TEXT("Tint"), bBroken ? FLinearColor(0.02f,0.025f,0.025f) : LampColor);
        LensMaterial->SetScalarParameterValue(TEXT("Glow"), bOn ? 18.f : 0.f);
    }
}

void AFacilityLight::Toggle()
{
    if (bBroken) return;
    bSwitchedOn = !bSwitchedOn;
    ApplyState();
}

void AFacilityLight::Smash()
{
    if (bBroken) return;
    bBroken = true;
    ApplyState();
    // Three rigid shards remain as physical, ray-traced geometry, without a physics simulation.
    for (int32 I = 0; I < 3; ++I)
    {
        auto* Shard = Afterlight::Part(this, Housing, FVector(65,I*20-20,0), FVector(3,15,5), TEXT("Metal"));
        Shard->SetAbsolute(false, false, true);
        Shard->SetWorldScale3D(FVector(0.03f,0.15f,0.05f));
        Shard->SetRelativeRotation(FRotator(I*19,0,I*28));
    }
}

FString AFacilityLight::Prompt() const
{
    if (bBroken) return TEXT("BROKEN FIXTURE");
    if (!bCircuitOn) return TEXT("E  TOGGLE FIXTURE  /  CIRCUIT OFF     LMB  BREAK");
    return bSwitchedOn ? TEXT("E  SWITCH OFF     LMB  BREAK") : TEXT("E  SWITCH ON     LMB  BREAK");
}

AFacilityDevice::AFacilityDevice()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Mesh);
    Mesh->SetStaticMesh(Afterlight::Shape());
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetMobility(EComponentMobility::Movable);
    Afterlight::Shadow(Mesh);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(Mesh);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetTextRenderColor(FColor(216,229,209));
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Afterlight::Shadow(Label);
}

void AFacilityDevice::Configure(EDeviceKind InKind, FName InId, FVector Location, FVector Size, int32 InCircuit, float Yaw)
{
    Kind = InKind;
    Id = InId;
    Circuit = InCircuit;
    ClosedLocation = Location;
    SetActorLocationAndRotation(Location, FRotator(0,Yaw,0));
    Mesh->SetWorldScale3D(Size/100.f);
    const bool bDoor = Kind == EDeviceKind::SecurityDoor || Kind == EDeviceKind::Lift;
    Mesh->SetMaterial(0, Afterlight::Material(bDoor ? TEXT("Teal") : Kind == EDeviceKind::Fuse ? TEXT("Ceramic") : TEXT("Amber")));
    Label->SetAbsolute(false,false,true);
    Label->SetWorldScale3D(FVector::OneVector);
    Label->SetRelativeLocation(FVector(52,0,0));
    Label->SetWorldSize(bDoor ? 22 : 9);
    Label->SetTextMaterial(Afterlight::Material(TEXT("Text")));
    const TCHAR* Caption = TEXT("");
    switch (Kind)
    {
        case EDeviceKind::Card: Caption = TEXT("09\nACCESS"); break;
        case EDeviceKind::Fuse: Caption = TEXT("63A"); break;
        case EDeviceKind::SecurityDoor: Caption = TEXT("PLANT\nRESTRICTED"); break;
        case EDeviceKind::Generator: Caption = TEXT("AUX\nPOWER"); break;
        case EDeviceKind::Valve: Caption = TEXT("VENT\nPRESSURE"); break;
        case EDeviceKind::Evacuation: Caption = TEXT("LIFT\nCALL"); break;
        case EDeviceKind::Lift: Caption = TEXT("09\nSURFACE LIFT"); break;
        case EDeviceKind::Breaker: Caption = TEXT("LIGHTS"); break;
        case EDeviceKind::Note: Caption = TEXT("SHIFT\nREPORT"); break;
    }
    Label->SetText(FText::FromString(Caption));
}

void AFacilityDevice::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bOpen && DoorTravel < 340)
    {
        DoorTravel = FMath::Min(340.f, DoorTravel + Dt*700);
        // Quantized shutter motion is deliberate: no skeletal or smooth door animation.
        SetActorLocation(ClosedLocation + FVector(0,0,FMath::FloorToFloat(DoorTravel/30.f)*30.f));
    }
}

void AFacilityDevice::Open()
{
    bOpen = true;
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FString AFacilityDevice::Prompt(const AAfterlightGameMode* G) const
{
    if (bOpen) return TEXT("");
    switch (Kind)
    {
        case EDeviceKind::Card: return TEXT("E  TAKE PLANT ACCESS CARD");
        case EDeviceKind::Fuse: return TEXT("E  TAKE CERAMIC FUSE");
        case EDeviceKind::SecurityDoor: return G->bHasCard ? TEXT("E  UNLOCK PLANT SHUTTER") : TEXT("ACCESS CARD REQUIRED  /  RECORDS");
        case EDeviceKind::Generator: return G->bGenerator ? TEXT("AUXILIARY POWER ONLINE") : G->bHasFuse ? TEXT("E  INSTALL FUSE / RESTORE POWER") : TEXT("CERAMIC FUSE REQUIRED  /  WORKSHOP");
        case EDeviceKind::Valve: return G->bPressureVented ? TEXT("COOLANT PRESSURE RELEASED") : TEXT("E  VENT COOLANT PRESSURE");
        case EDeviceKind::Evacuation: return G->bEvacuation ? TEXT("EVACUATION SEQUENCE RUNNING") : TEXT("E  CALL SURFACE LIFT");
        case EDeviceKind::Lift: return TEXT("LIFT SEALED  /  USE CALL CONSOLE");
        case EDeviceKind::Breaker: return G->Circuits[Circuit] ? TEXT("E  CUT ROOM LIGHTS") : TEXT("E  RESTORE ROOM LIGHTS");
        case EDeviceKind::Note: return TEXT("E  READ SHIFT REPORT");
    }
    return TEXT("");
}

bool AFacilityDevice::Use(AAfterlightGameMode* G)
{
    if (!G || bOpen) return false;
    switch (Kind)
    {
        case EDeviceKind::Card:
            if (bUsed) return false;
            G->bHasCard = true;
            G->Notify(TEXT("PLANT ACCESS ACQUIRED.  Something moved in the observation room."),6);
            G->GracePeriod = FMath::Min(G->GracePeriod, G->RunTime + 8);
            G->SetCircuit(4,false);
            break;
        case EDeviceKind::Fuse:
            if (bUsed) return false;
            G->bHasFuse = true;
            G->Notify(TEXT("CERAMIC FUSE ACQUIRED.  Install it at the plant's auxiliary generator."),6);
            break;
        case EDeviceKind::SecurityDoor:
            if (!G->bHasCard) { G->Notify(TEXT("Plant access is locked. The night supervisor kept a card in RECORDS.")); return false; }
            Open();
            G->Noise(GetActorLocation(),0.65f);
            G->Notify(TEXT("PLANT UNLOCKED.  Restore auxiliary power. Keep your light low."));
            break;
        case EDeviceKind::Generator:
            if (G->bGenerator) return false;
            if (!G->bHasFuse) { G->Notify(TEXT("The fuse socket is empty. Search the WORKSHOP.")); return false; }
            G->bGenerator = true;
            G->bHasFuse = false;
            G->SetCircuit(3,true);
            G->Noise(GetActorLocation(),1);
            G->Notify(TEXT("AUXILIARY POWER ONLINE.  Vent the coolant in the PUMP ROOM."),6);
            break;
        case EDeviceKind::Valve:
            if (G->bPressureVented) return false;
            if (!G->bGenerator) { G->Notify(TEXT("The pressure interlock needs auxiliary power from the PLANT.")); return false; }
            G->bPressureVented = true;
            G->Noise(GetActorLocation(),0.8f);
            G->Notify(TEXT("PRESSURE RELEASED.  The surface lift can now be called from the east hall."),6);
            break;
        case EDeviceKind::Evacuation:
            if (G->bEvacuation) return false;
            if (!G->bGenerator || !G->bPressureVented) { G->Notify(TEXT("LIFT INTERLOCK: auxiliary power and safe coolant pressure required.")); return false; }
            G->bEvacuation = true;
            G->Noise(GetActorLocation(),1);
            G->Notify(TEXT("LIFT ASCENDING.  Survive until it returns. Darkness will conceal you."),7);
            G->Sound(TEXT("Alarm"),GetActorLocation(),0.5f);
            break;
        case EDeviceKind::Lift: G->Notify(TEXT("Call the lift from the amber console beside the shutter.")); return false;
        case EDeviceKind::Breaker: G->ToggleCircuit(Circuit); break;
        case EDeviceKind::Note:
            G->bHelp = true;
            G->SetPaused(true);
            G->Notify(TEXT("SHIFT REPORT 09 / The Warden follows light and noise. Cut a circuit, crouch, and move away."),12);
            break;
    }
    bUsed = true;
    if (Kind == EDeviceKind::Card || Kind == EDeviceKind::Fuse)
    {
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        G->Sound(TEXT("Pickup"),GetActorLocation(),0.55f,false);
    }
    else G->Sound(TEXT("Switch"),GetActorLocation(),0.65f);
    return true;
}
