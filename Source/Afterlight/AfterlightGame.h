#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "AfterlightGame.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class URectLightComponent;
class USpotLightComponent;
class UCameraComponent;
class UBoxComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class USoundAttenuation;
class UAudioComponent;
class UFont;
class AAfterlightGameMode;

UENUM()
enum class EDeviceKind : uint8 { Card, Fuse, SecurityDoor, Generator, Valve, Evacuation, Lift, Breaker, Note };

UCLASS()
class AFTERLIGHT_API AFacilityLight : public AActor
{
    GENERATED_BODY()
public:
    AFacilityLight();
    void Configure(FVector Location, FLinearColor Color, float Lumens, int32 InCircuit, FRotator Rotation = FRotator(-90,0,0));
    void ApplyState();
    void Toggle();
    void Smash();
    bool IsLit() const { return bSwitchedOn && bCircuitOn && !bBroken; }
    FString Prompt() const;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Housing;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Lens;
    UPROPERTY() TObjectPtr<URectLightComponent> Light;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LensMaterial;
    int32 Circuit = 0;
    bool bSwitchedOn = true;
    bool bCircuitOn = true;
    bool bBroken = false;
    float RatedLumens = 2000;
    FLinearColor LampColor = FLinearColor::White;
};

UCLASS()
class AFTERLIGHT_API AFacilityDevice : public AActor
{
    GENERATED_BODY()
public:
    AFacilityDevice();
    virtual void Tick(float DeltaSeconds) override;
    void Configure(EDeviceKind InKind, FName InId, FVector Location, FVector Size, int32 InCircuit = 0, float Yaw = 0);
    bool Use(AAfterlightGameMode* Game);
    void Open();
    FString Prompt(const AAfterlightGameMode* Game) const;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
    EDeviceKind Kind = EDeviceKind::Note;
    FName Id;
    int32 Circuit = 0;
    bool bUsed = false;
    bool bOpen = false;
    FVector ClosedLocation;
    float DoorTravel = 0;
};

UCLASS()
class AFTERLIGHT_API AFacility : public AActor
{
    GENERATED_BODY()
public:
    AFacility();
    void Build(AAfterlightGameMode* Game);
    void Box(FVector Position, FVector Size, const FString& Material, FRotator Rotation = FRotator::ZeroRotator);
    void Cylinder(FVector Position, FVector Size, const FString& Material, FRotator Rotation = FRotator::ZeroRotator);
    void Sign(const FString& Text, FVector Position, float Size, float Yaw = 180, FColor Color = FColor(200,214,208));
    void Room(FVector2D Center, FVector2D Size, int32 Circuit, const FString& Name);
    void WallX(float X, float Y, float Length, float DoorY = TNumericLimits<float>::Max());
    void WallY(float X, float Y, float Length, float DoorX = TNumericLimits<float>::Max());
    void Shelf(FVector Position, float Yaw, int32 Seed);
    void Desk(FVector Position, float Yaw);
    UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    UPROPERTY() TObjectPtr<AAfterlightGameMode> OwnerGame;
};

UCLASS()
class AFTERLIGHT_API AAfterlightCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AAfterlightCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void Look(float Value);
    void Interact();
    void ToggleFlashlight();
    void Strike();
    void SprintOn();
    void SprintOff();
    void CrouchOn();
    void CrouchOff();
    void Pause();
    void Confirm();
    void Retry();
    void Help();
    void Quality();
    void FrameGen();
    void Telemetry();
    void Photo();
    void Quit();
    void SensitivityDown();
    void SensitivityUp();
    AActor* TraceInteract() const;
    bool CanAct() const;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<USpotLightComponent> Flashlight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Hand;
    UPROPERTY() TObjectPtr<AAfterlightGameMode> Game;
    bool bFlashlightOn = true;
    bool bSprinting = false;
    float Stamina = 1;
    float StepClock = 0;
    float StrikeCooldown = 0;
    float StrikePose = 0;
    float Exposure = 0;
};

UCLASS()
class AFTERLIGHT_API AWarden : public AActor
{
    GENERATED_BODY()
public:
    AWarden();
    virtual void Tick(float DeltaSeconds) override;
    void BuildBody();
    void UpdateMind(float Dt);
    void PlanPath(FVector Destination);
    bool CanSeePlayer() const;
    UPROPERTY() TObjectPtr<UBoxComponent> Collision;
    UPROPERTY() TObjectPtr<AAfterlightGameMode> Game;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    TArray<FVector> Path;
    FVector LastKnown;
    float MindClock = 0;
    float StepClock = 0;
    float LostClock = 0;
    float RepathClock = 0;
    float FootClock = 0;
    float Suspicion = 0;
    int32 PatrolIndex = 0;
    bool bHunting = false;
};

UCLASS()
class AFTERLIGHT_API AAfterlightHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color, bool bShadow = true);
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Rule(float X, float Y, float W, FLinearColor Color);
    float Scale = 1;
    UPROPERTY() TObjectPtr<UFont> HUDFont;
};

UCLASS()
class AFTERLIGHT_API AAfterlightGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAfterlightGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void StartRun();
    void RestartRun();
    void SetPaused(bool bValue);
    void Notify(const FString& Text, float Seconds = 5);
    void Noise(FVector Position, float Strength);
    void Sound(const FString& Name, FVector Position, float Volume = 1, bool bSpatial = true);
    void ToggleCircuit(int32 Circuit);
    void SetCircuit(int32 Circuit, bool bOn);
    void Lose();
    void Win();
    float LightExposure(FVector Position) const;
    FString Objective() const;
    FString AreaName(FVector Position) const;
    void ApplyGraphics();
    void SaveSettings();
    void AuditTick(float Dt);
    void WriteAudit();
    AFacilityLight* AddLight(FVector Location, FLinearColor Color, float Lumens, int32 Circuit, FRotator Rotation = FRotator(-90,0,0));
    AFacilityDevice* AddDevice(EDeviceKind Kind, FName Id, FVector Location, FVector Size, int32 Circuit = 0, float Yaw = 0);
    int32 AddNav(FVector Position);
    void LinkNav(int32 A, int32 B);
    UPROPERTY() TObjectPtr<AFacility> Facility;
    UPROPERTY() TObjectPtr<AWarden> Warden;
    UPROPERTY() TObjectPtr<AAfterlightCharacter> Player;
    UPROPERTY() TArray<TObjectPtr<AFacilityLight>> Lights;
    UPROPERTY() TArray<TObjectPtr<AFacilityDevice>> Devices;
    UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY() TMap<FString, TObjectPtr<USoundBase>> Sounds;
    UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
    TArray<FVector> NavPoints;
    TArray<TArray<int32>> NavLinks;
    bool Circuits[6] = {true,true,true,true,true,true};
    bool bTitle = true;
    bool bPaused = false;
    bool bHelp = false;
    bool bWon = false;
    bool bLost = false;
    bool bHasCard = false;
    bool bHasFuse = false;
    bool bGenerator = false;
    bool bPressureVented = false;
    bool bEvacuation = false;
    bool bHardwareReady = false;
    bool bDLSS = false;
    bool bRayReconstruction = false;
    bool bFrameGeneration = false;
    bool bFrameGenerationSupported = false;
    bool bShowTelemetry = false;
    bool bPhoto = false;
    bool bAudit = false;
    bool bProfileMode = false;
    bool bAuditFreezeAI = false;
    int32 QualityPreset = 0;
    int32 BrokenLights = 0;
    float MouseSensitivity = 0.65f;
    float RunTime = 0;
    float EvacuationTime = 35;
    float NoticeTime = 0;
    float NoiseTime = 0;
    float NoiseStrength = 0;
    FVector NoisePosition;
    float GracePeriod = 32;
    FString Notice;
    FString HardwareMessage;
    float SmoothedFrameMs = 16;
    float GPUFrameMs = 0;
    float PresentedFPS = 0;
    float AuditClock = 0;
    int32 AuditPhase = 0;
    int32 AuditShotPhase = -1;
    int32 AuditInputPhase = -1;
    float AuditShotTime = 0;
    FVector AuditStartPosition;
    float AuditStartValue = 0;
    TArray<FString> AuditPasses;
    TArray<FString> AuditFailures;
    TArray<float> AuditFrameMs;
};
