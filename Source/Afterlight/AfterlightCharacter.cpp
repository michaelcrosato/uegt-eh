#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AAfterlightCharacter::AAfterlightCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(28,90);
    GetCharacterMovement()->MaxWalkSpeed = 235;
    GetCharacterMovement()->MaxStepHeight = 25;
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
    GetCharacterMovement()->SetCrouchedHalfHeight(56);
    GetCharacterMovement()->MaxWalkSpeedCrouched = 110;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Eyes"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0,0,65));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = 90;
    Hand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandheldLamp"));
    Hand->SetupAttachment(Camera);
    Hand->SetStaticMesh(Afterlight::Shape());
    Hand->SetRelativeLocation(FVector(30,18,-20));
    Hand->SetRelativeScale3D(FVector(0.25f,0.10f,0.11f));
    Hand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Afterlight::Shadow(Hand);
    Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HandheldRayTracedLight"));
    Flashlight->SetupAttachment(Camera);
    Flashlight->SetRelativeLocation(FVector(44,18,-18));
    Flashlight->SetMobility(EComponentMobility::Movable);
    Flashlight->SetIntensity(2200);
    Flashlight->IntensityUnits = ELightUnits::Lumens;
    Flashlight->SetAttenuationRadius(2000);
    Flashlight->SetInnerConeAngle(17);
    Flashlight->SetOuterConeAngle(33);
    Flashlight->SetSourceRadius(1.6f);
    Flashlight->SetLightColor(FLinearColor(0.86f,0.93f,1.0f));
    Flashlight->SetCastShadows(true);
    Flashlight->SetCastRaytracedShadows(ECastRayTracedShadow::Enabled);
    Flashlight->bAllowMegaLights = true;
    Flashlight->MegaLightsShadowMethod = EMegaLightsShadowMethod::RayTracing;
    Flashlight->SetCastVolumetricShadow(true);
    Flashlight->SetVolumetricScatteringIntensity(0.35f);
}

void AAfterlightCharacter::BeginPlay()
{
    Super::BeginPlay();
    Game = Cast<AAfterlightGameMode>(UGameplayStatics::GetGameMode(this));
    if (Game) Game->Player = this;
    Hand->SetMaterial(0,Afterlight::Material(TEXT("Amber")));
    Afterlight::Part(this, Camera, FVector(21,18,-23),FVector(12,12,10),TEXT("Glove"));
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        PC->PlayerCameraManager->ViewPitchMin = -82;
        PC->PlayerCameraManager->ViewPitchMax = 82;
    }
}

bool AAfterlightCharacter::CanAct() const
{
    return Game && Game->bHardwareReady && !Game->bTitle && !Game->bPaused && !Game->bLost && !Game->bWon;
}

void AAfterlightCharacter::SetupPlayerInputComponent(UInputComponent* I)
{
    Super::SetupPlayerInputComponent(I);
    I->BindAxis(TEXT("MoveForward"),this,&AAfterlightCharacter::MoveForward);
    I->BindAxis(TEXT("MoveRight"),this,&AAfterlightCharacter::MoveRight);
    I->BindAxis(TEXT("Turn"),this,&AAfterlightCharacter::Turn);
    I->BindAxis(TEXT("LookUp"),this,&AAfterlightCharacter::Look);
    I->BindAction(TEXT("Interact"),IE_Pressed,this,&AAfterlightCharacter::Interact);
    I->BindAction(TEXT("Flashlight"),IE_Pressed,this,&AAfterlightCharacter::ToggleFlashlight);
    I->BindAction(TEXT("Strike"),IE_Pressed,this,&AAfterlightCharacter::Strike);
    I->BindAction(TEXT("Sprint"),IE_Pressed,this,&AAfterlightCharacter::SprintOn);
    I->BindAction(TEXT("Sprint"),IE_Released,this,&AAfterlightCharacter::SprintOff);
    I->BindAction(TEXT("Crouch"),IE_Pressed,this,&AAfterlightCharacter::CrouchOn);
    I->BindAction(TEXT("Crouch"),IE_Released,this,&AAfterlightCharacter::CrouchOff);
    I->BindAction(TEXT("Pause"),IE_Pressed,this,&AAfterlightCharacter::Pause);
    I->BindAction(TEXT("Confirm"),IE_Pressed,this,&AAfterlightCharacter::Confirm);
    I->BindAction(TEXT("Restart"),IE_Pressed,this,&AAfterlightCharacter::Restart);
    I->BindAction(TEXT("Help"),IE_Pressed,this,&AAfterlightCharacter::Help);
    I->BindAction(TEXT("Quality"),IE_Pressed,this,&AAfterlightCharacter::Quality);
    I->BindAction(TEXT("FrameGen"),IE_Pressed,this,&AAfterlightCharacter::FrameGen);
    I->BindAction(TEXT("Telemetry"),IE_Pressed,this,&AAfterlightCharacter::Telemetry);
    I->BindAction(TEXT("Photo"),IE_Pressed,this,&AAfterlightCharacter::Photo);
    I->BindAction(TEXT("Quit"),IE_Pressed,this,&AAfterlightCharacter::Quit);
    I->BindAction(TEXT("SensitivityDown"),IE_Pressed,this,&AAfterlightCharacter::SensitivityDown);
    I->BindAction(TEXT("SensitivityUp"),IE_Pressed,this,&AAfterlightCharacter::SensitivityUp);
}

void AAfterlightCharacter::MoveForward(float V) { if (CanAct()) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V); }
void AAfterlightCharacter::MoveRight(float V) { if (CanAct()) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V); }
void AAfterlightCharacter::Turn(float V) { if (CanAct()) AddControllerYawInput(V*Game->MouseSensitivity); }
void AAfterlightCharacter::Look(float V) { if (CanAct()) AddControllerPitchInput(V*Game->MouseSensitivity); }

AActor* AAfterlightCharacter::TraceInteract() const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(Interact),false,this);
    if (GetWorld()->LineTraceSingleByChannel(Hit,Camera->GetComponentLocation(),Camera->GetComponentLocation()+Camera->GetForwardVector()*460,ECC_Visibility,Params))
        return Hit.GetActor();
    return nullptr;
}

void AAfterlightCharacter::Interact()
{
    if (!CanAct()) return;
    AActor* Target = TraceInteract();
    if (auto* L = Cast<AFacilityLight>(Target))
    {
        if (L->bBroken) return;
        L->Toggle();
        Game->Sound(TEXT("Switch"),L->GetActorLocation(),0.4f);
    }
    else if (auto* D = Cast<AFacilityDevice>(Target)) D->Use(Game);
}

void AAfterlightCharacter::ToggleFlashlight()
{
    if (!CanAct()) return;
    bFlashlightOn = !bFlashlightOn;
    Flashlight->SetVisibility(bFlashlightOn);
    Game->Sound(TEXT("Switch"),GetActorLocation(),0.25f,false);
}

void AAfterlightCharacter::Strike()
{
    if (!CanAct() || StrikeCooldown > 0) return;
    StrikeCooldown = 0.6f;
    StrikePose = 0.25f;
    if (auto* L = Cast<AFacilityLight>(TraceInteract()))
    {
        if (!L->bBroken)
        {
            L->Smash();
            ++Game->BrokenLights;
            Game->Sound(TEXT("Break"),L->GetActorLocation(),0.7f);
            Game->Noise(L->GetActorLocation(),1);
            Game->Notify(TEXT("FIXTURE DESTROYED.  The noise carries."),2.5f);
        }
    }
    else
    {
        Game->Sound(TEXT("Switch"),GetActorLocation(),0.3f,false);
        Game->Notify(TEXT("Aim the pulse tool at a light fixture within 4.6 metres."),2);
    }
}

void AAfterlightCharacter::SprintOn() { bSprinting = true; }
void AAfterlightCharacter::SprintOff() { bSprinting = false; }
void AAfterlightCharacter::CrouchOn() { if (CanAct()) Crouch(); }
void AAfterlightCharacter::CrouchOff() { UnCrouch(); }
void AAfterlightCharacter::Pause() { if (Game && !Game->bTitle && !Game->bLost && !Game->bWon) { Game->bHelp = false; Game->SetPaused(!Game->bPaused); } }
void AAfterlightCharacter::Confirm() { if (!Game) return; if (Game->bTitle) Game->StartRun(); else if (Game->bWon || Game->bLost) Game->RestartRun(); else if (Game->bPaused) { Game->bHelp = false; Game->SetPaused(false); } }
void AAfterlightCharacter::Restart() { if (Game && (Game->bWon || Game->bLost || Game->bPaused)) Game->RestartRun(); }
void AAfterlightCharacter::Help() { if (Game && !Game->bTitle && !Game->bWon && !Game->bLost) { Game->bHelp = !Game->bHelp; Game->SetPaused(Game->bHelp); } }
void AAfterlightCharacter::Quality() { if (Game) { Game->QualityPreset = (Game->QualityPreset+1)%3; Game->ApplyGraphics(); Game->SaveSettings(); } }
void AAfterlightCharacter::FrameGen() { if (Game) { Game->bFrameGeneration = !Game->bFrameGeneration; Game->ApplyGraphics(); Game->SaveSettings(); } }
void AAfterlightCharacter::Telemetry() { if (Game) Game->bShowTelemetry = !Game->bShowTelemetry; }
void AAfterlightCharacter::Photo() { if (Game && !Game->bTitle && !Game->bWon && !Game->bLost) { Game->bPhoto = !Game->bPhoto; Game->SetPaused(Game->bPhoto); } }
void AAfterlightCharacter::Quit() { if (Game && (Game->bPaused || Game->bTitle || Game->bWon || Game->bLost)) UGameplayStatics::GetPlayerController(this,0)->ConsoleCommand(TEXT("quit")); }
void AAfterlightCharacter::SensitivityDown() { if (Game) { Game->MouseSensitivity = FMath::Max(0.1f, Game->MouseSensitivity-0.1f); Game->SaveSettings(); Game->Notify(FString::Printf(TEXT("MOUSE SENSITIVITY  %.2f"),Game->MouseSensitivity),2); } }
void AAfterlightCharacter::SensitivityUp() { if (Game) { Game->MouseSensitivity = FMath::Min(2.5f, Game->MouseSensitivity+0.1f); Game->SaveSettings(); Game->Notify(FString::Printf(TEXT("MOUSE SENSITIVITY  %.2f"),Game->MouseSensitivity),2); } }

void AAfterlightCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Game) return;
    if (!CanAct()) { GetCharacterMovement()->StopMovementImmediately(); return; }
    StrikeCooldown = FMath::Max(0.f,StrikeCooldown-Dt);
    StrikePose = FMath::Max(0.f,StrikePose-Dt);
    Hand->SetRelativeRotation(FRotator(StrikePose>0.15f ? 20 : 0,0,0));
    const bool bFast = bSprinting && !bIsCrouched && Stamina > 0.03f && GetVelocity().SizeSquared2D() > 100;
    Stamina = FMath::Clamp(Stamina + Dt*(bFast ? -0.16f : 0.115f),0.f,1.f);
    if (Stamina <= 0.01f) bSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = bFast ? 420 : 235;
    const float Speed = GetVelocity().Size2D();
    if (Speed > 30)
    {
        StepClock += Dt;
        if (StepClock > (bFast ? 0.31f : bIsCrouched ? 0.68f : 0.49f))
        {
            StepClock = 0;
            Game->Sound(TEXT("Step"),GetActorLocation(),bIsCrouched ? 0.045f : bFast ? 0.26f : 0.12f,false);
            if (bFast) Game->Noise(GetActorLocation(),0.58f);
        }
    }
    Exposure = FMath::FInterpTo(Exposure,Game->LightExposure(GetActorLocation()+FVector(0,0,35))*(bIsCrouched ? 0.38f : 1.f) + (bFlashlightOn ? 0.55f : 0),Dt,4.f);
}
