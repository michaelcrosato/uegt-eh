#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "DLSSLibrary.h"
#include "StreamlineLibraryDLSSG.h"
#include "StreamlineLibraryReflex.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "DynamicRHI.h"
#include "RenderUtils.h"
#include "UnrealClient.h"

AAfterlightGameMode::AAfterlightGameMode()
{
    DefaultPawnClass=AAfterlightCharacter::StaticClass();
    HUDClass=AAfterlightHUD::StaticClass();
    PrimaryActorTick.bCanEverTick=true;
}

void AAfterlightGameMode::BeginPlay()
{
    Super::BeginPlay();
    bAudit=FParse::Param(FCommandLine::Get(),TEXT("AfterlightAudit")) || FParse::Param(FCommandLine::Get(),TEXT("AfterlightNoRTAudit"));
    bProfileMode=FParse::Param(FCommandLine::Get(),TEXT("AfterlightProfile"));
    // Legacy main-facility benchmarks retain their fixed start; the separate
    // orientation audit exercises the same new start as a normal player.
    bOrientationComplete=(bAudit && !FParse::Param(FCommandLine::Get(),TEXT("AfterlightOrientationAudit"))) || bProfileMode;
    bHardwareReady=GDynamicRHI && GDynamicRHI->GetInterfaceType()==ERHIInterfaceType::D3D12 && IsRayTracingEnabled();
    if (!bHardwareReady) HardwareMessage=TEXT("AFTERLIGHT requires a hardware ray tracing GPU, DirectX 12 and Shader Model 6. Enable DX12 and update your graphics driver.");
    GConfig->GetInt(TEXT("Afterlight"),TEXT("QualityPreset"),QualityPreset,GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("Afterlight"),TEXT("MouseSensitivity"),MouseSensitivity,GGameUserSettingsIni);
    GConfig->GetBool(TEXT("Afterlight"),TEXT("FrameGeneration"),bFrameGeneration,GGameUserSettingsIni);
    QualityPreset=FMath::Clamp(QualityPreset,0,2);
    MouseSensitivity=FMath::Clamp(MouseSensitivity,0.1f,2.5f);
    if(bAudit) { QualityPreset=0; bFrameGeneration=false; }
    ApplyGraphics();

    Facility=GetWorld()->SpawnActor<AFacility>();
    Facility->Build(this);
    SetCircuit(3,false);
    Warden=GetWorld()->SpawnActor<AWarden>(FVector(3350,-1320,110),FRotator(0,90,0));
    Warden->Game=this;
    Warden->BuildBody();
    Player=Cast<AAfterlightCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (Player)
    {
        Player->Game=this;
        Player->SetActorLocation(bOrientationComplete ? FVector(-650,0,92) : FVector(-4610,0,92));
        OrientationPreviousPosition=Player->GetActorLocation();
        if (auto* PC=Cast<APlayerController>(Player->GetController())) PC->SetControlRotation(FRotator(0,0,0));
        ApplyGraphics();
    }
    Attenuation=NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate=true;
    Attenuation->Attenuation.bSpatialize=true;
    Attenuation->Attenuation.AttenuationShapeExtents=FVector(180);
    Attenuation->Attenuation.FalloffDistance=2100;
    Attenuation->Attenuation.bEnableOcclusion=true;
    Attenuation->Attenuation.OcclusionLowPassFilterFrequency=700;
    Attenuation->Attenuation.OcclusionVolumeAttenuation=0.35f;
    for (const FString Name : {TEXT("Drone"),TEXT("Step"),TEXT("Warden"),TEXT("Switch"),TEXT("Break"),TEXT("Pickup"),TEXT("Alarm"),TEXT("Caught"),TEXT("Escape")})
        Sounds.Add(Name,LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Audio/A_%s.A_%s"),*Name,*Name)));
    if (Sounds[TEXT("Drone")]) Ambience=UGameplayStatics::SpawnSound2D(this,Sounds[TEXT("Drone")],0.18f);
    Notice=TEXT("You are the last technician on Sublevel 09. Find a way to the surface.");
    NoticeTime=9;
    if (bAudit || bProfileMode) { bAuditFreezeAI=true; if(bOrientationComplete) StartRun(); bShowTelemetry=true; }
    UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT render contract: HWRT=%d DX12=%d DLSS=%d RR=%d FGSupport=%d FG=%d MegaLights=%d LumenHWRT=%d"),bHardwareReady,GDynamicRHI && GDynamicRHI->GetInterfaceType()==ERHIInterfaceType::D3D12,bDLSS,bRayReconstruction,bFrameGenerationSupported,bFrameGeneration,Afterlight::IntCVar(TEXT("r.MegaLights.EnableForProject")),Afterlight::IntCVar(TEXT("r.Lumen.HardwareRayTracing")));
}

void AAfterlightGameMode::ApplyGraphics()
{
    bDLSS=UDLSSLibrary::IsDLSSSupported();
    UDLSSLibrary::EnableDLSS(bDLSS);
    if (bDLSS)
    {
        bool bSupported=false,bFixed=false;
        float Optimal=66.6667f,Min=50,Max=100,Sharpness=0;
        const UDLSSMode Mode=QualityPreset==2 ? UDLSSMode::DLAA : QualityPreset==1 ? UDLSSMode::Balanced : UDLSSMode::Quality;
        UDLSSLibrary::GetDLSSModeInformation(Mode,FVector2D(2560,1440),bSupported,Optimal,bFixed,Min,Max,Sharpness);
        Afterlight::CVar(TEXT("r.ScreenPercentage"),bSupported ? Optimal : 66.6667f);
    }
    else Afterlight::CVar(TEXT("r.ScreenPercentage"),QualityPreset==2 ? 100 : 66.6667f);
    // UE 5.8 rejects implicit writes at constructor priority in a cooked game.
    // Establish explicit priorities before NVIDIA's Blueprint helpers mutate these CVars.
    Afterlight::CVar(TEXT("r.Lumen.Reflections.BilateralFilter"),bDLSS && UDLSSLibrary::IsDLSSRRSupported() ? 0 : 1);
    UDLSSLibrary::EnableDLSSRR(bDLSS && UDLSSLibrary::IsDLSSRRSupported());
    bRayReconstruction=UDLSSLibrary::IsDLSSRREnabled();
    bFrameGenerationSupported=UStreamlineLibraryDLSSG::IsDLSSGModeSupported(EStreamlineDLSSGMode::On2X);
    bFrameGeneration=bFrameGeneration && bFrameGenerationSupported;
    Afterlight::CVar(TEXT("r.Streamline.DLSSG.Enable"),bFrameGeneration ? 1 : 0);
    Afterlight::CVar(TEXT("r.Streamline.DLSSG.FramesToGenerate"),1);
    UStreamlineLibraryDLSSG::SetDLSSGMode(bFrameGeneration ? EStreamlineDLSSGMode::On2X : EStreamlineDLSSGMode::Off);
    if (UStreamlineLibraryReflex::IsReflexSupported()) UStreamlineLibraryReflex::SetReflexMode(EStreamlineReflexMode::Enabled);
    Afterlight::CVar(TEXT("r.MegaLights.DownsampleMode"),QualityPreset==2 ? 0 : QualityPreset==1 ? 2 : 1);
    Afterlight::CVar(TEXT("r.MegaLights.NumSamplesPerPixel"),4);
    Afterlight::CVar(TEXT("r.Lumen.HardwareRayTracing.LightingMode"),QualityPreset==2 ? 1 : 2);
    Afterlight::CVar(TEXT("r.Lumen.Reflections.MaxBounces"),QualityPreset==2 ? 4 : 2);
    Afterlight::CVar(TEXT("r.Lumen.Reflections.DownsampleFactor"),1);
    Afterlight::CVar(TEXT("r.Lumen.Reflections.DownsampleCheckerboard"),0);
    if(Player)
    {
        Player->Camera->PostProcessSettings.bOverride_LumenFinalGatherQuality=true;
        Player->Camera->PostProcessSettings.LumenFinalGatherQuality=QualityPreset==2 ? 2 : 1;
    }
    if (!bTitle) Notify(QualityPreset==2 ? TEXT("SHOWCASE / DLAA / FULL-RESOLUTION RAY-TRACED LIGHTING") : QualityPreset==1 ? TEXT("SMOOTH / DLSS BALANCED / HARDWARE RAY TRACING") : TEXT("QUALITY / DLSS QUALITY / HARDWARE RAY TRACING"),3);
}

void AAfterlightGameMode::SaveSettings()
{
    GConfig->SetInt(TEXT("Afterlight"),TEXT("QualityPreset"),QualityPreset,GGameUserSettingsIni);
    GConfig->SetFloat(TEXT("Afterlight"),TEXT("MouseSensitivity"),MouseSensitivity,GGameUserSettingsIni);
    GConfig->SetBool(TEXT("Afterlight"),TEXT("FrameGeneration"),bFrameGeneration,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}

void AAfterlightGameMode::StartRun()
{
    if (!bHardwareReady) return;
    bTitle=false; bPaused=false; bHelp=false;
    Notify(bOrientationComplete ? TEXT("Find the PLANT ACCESS CARD in Records and the CERAMIC FUSE in Workshop.") : TEXT("ARRIVAL CHECK / Move with WASD, look with the mouse. Find the amber check-in panel."),8);
}

void AAfterlightGameMode::RestartRun()
{
    SaveSettings();
    UGameplayStatics::OpenLevel(this,FName(TEXT("Sublevel09")));
}

void AAfterlightGameMode::SetPaused(bool bValue)
{
    // Freeze gameplay, leave rendering alive for stable temporal lighting and settings.
    bPaused=bValue;
}

void AAfterlightGameMode::Notify(const FString& T,float Seconds) { Notice=T; NoticeTime=Seconds; }
void AAfterlightGameMode::Noise(FVector P,float Strength) { NoisePosition=P; NoiseStrength=Strength; NoiseTime=2.5f; }

void AAfterlightGameMode::Sound(const FString& Name,FVector P,float V,bool bSpatial)
{
    auto* Found=Sounds.Find(Name);
    if (!Found || !*Found) return;
    if (bSpatial) UGameplayStatics::PlaySoundAtLocation(this,*Found,P,FRotator::ZeroRotator,V,1,0,Attenuation);
    else UGameplayStatics::PlaySound2D(this,*Found,V);
}

AFacilityLight* AAfterlightGameMode::AddLight(FVector P,FLinearColor C,float L,int32 Circuit,FRotator R)
{
    auto* Light=GetWorld()->SpawnActor<AFacilityLight>();
    Light->Configure(P,C,L,Circuit,R);
    Lights.Add(Light);
    return Light;
}

AFacilityDevice* AAfterlightGameMode::AddDevice(EDeviceKind K,FName Id,FVector P,FVector S,int32 C,float Y)
{
    auto* D=GetWorld()->SpawnActor<AFacilityDevice>();
    D->Configure(K,Id,P,S,C,Y); Devices.Add(D); return D;
}

void AAfterlightGameMode::ToggleCircuit(int32 C)
{
    if (C<0 || C>=NumCircuits) return;
    SetCircuit(C,!Circuits[C]);
    Notify(Circuits[C] ? TEXT("CIRCUIT ONLINE") : TEXT("CIRCUIT ISOLATED / DARKNESS CONCEALS YOU"),2.5f);
}
void AAfterlightGameMode::SetCircuit(int32 C,bool bOn)
{
    if (C<0 || C>=NumCircuits) return;
    Circuits[C]=bOn;
    for (AFacilityLight* L:Lights) if (L->Circuit==C) { L->bCircuitOn=bOn; L->ApplyState(); }
}

float AAfterlightGameMode::LightExposure(FVector Position) const
{
    float Total=0;
    for (AFacilityLight* L:Lights)
    {
        if (!L->IsLit()) continue;
        FVector Source=L->Light->GetComponentLocation();
        const float D=FVector::Distance(Source,Position);
        if (D>850) continue;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LightExposure),false,L);
        Params.AddIgnoredActor(Player);
        Params.AddIgnoredActor(Warden);
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit,Source,Position,ECC_Visibility,Params)) Total+=FMath::Square(1-D/950.f)*0.9f;
    }
    return FMath::Clamp(Total,0.f,1.f);
}

FString AAfterlightGameMode::Objective() const
{
    if(!bOrientationComplete) return OrientationHint();
    if (!bHasCard) return TEXT("Find the plant access card  /  RECORDS");
    if (!bGenerator && !bHasFuse) return TEXT("Find a ceramic fuse  /  WORKSHOP");
    if (!bGenerator) return TEXT("Install the ceramic fuse  /  AUXILIARY PLANT");
    if (!bPressureVented) return TEXT("Release coolant pressure  /  PUMP ROOM");
    if (!bEvacuation) return TEXT("Call the surface lift  /  EAST HALL");
    if (EvacuationTime>0) return FString::Printf(TEXT("Survive until the lift returns  /  %02d SEC"),FMath::CeilToInt(EvacuationTime));
    return TEXT("The lift is open. Get inside  /  EAST HALL");
}

FString AAfterlightGameMode::AreaName(FVector P) const
{
    if(P.X<-3000) return TEXT("00A / ARRIVAL CHAMBER");
    if(P.X<-2000) return TEXT("00B / LIGHT LAB");
    if(P.X<-900) return TEXT("00C / SERVICE GALLERY");
    if (P.X>4320 && FMath::Abs(P.Y)<260) return TEXT("SURFACE LIFT");
    if (P.Y>300) return P.X>3600 ? TEXT("05 / PUMP ROOM") : P.X>2100 ? TEXT("03 / AUXILIARY PLANT") : TEXT("01 / RECORDS");
    if (P.Y<-300) return P.X>2500 ? TEXT("04 / OBSERVATION") : TEXT("02 / WORKSHOP");
    return TEXT("09 / TRANSFER HALL");
}

void AAfterlightGameMode::Lose()
{
    if (bLost || bWon) return;
    bLost=true;
    Sound(TEXT("Caught"),Player ? Player->GetActorLocation() : FVector::ZeroVector,0.65f,false);
    UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT outcome: captured at %.1f seconds"),RunTime);
}

void AAfterlightGameMode::Win()
{
    if (bLost || bWon) return;
    bWon=true;
    Sound(TEXT("Escape"),FVector::ZeroVector,0.5f,false);
    UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT outcome: escaped at %.1f seconds, lights broken %d"),RunTime,BrokenLights);
}

int32 AAfterlightGameMode::AddNav(FVector P) { NavLinks.Add({}); return NavPoints.Add(P); }
void AAfterlightGameMode::LinkNav(int32 A,int32 B) { NavLinks[A].Add(B); NavLinks[B].Add(A); }

void AAfterlightGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    SmoothedFrameMs=FMath::Lerp(SmoothedFrameMs,Dt*1000.f,0.035f);
    GPUFrameMs=float(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
    int32 Frames=0;
    UStreamlineLibraryDLSSG::GetDLSSGFrameTiming(PresentedFPS,Frames);
    if (bAudit)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("AfterlightOrientationAudit"))) OrientationAuditTick(Dt);
        else AuditTick(Dt);
    }
    if (bProfileMode)
    {
        AuditClock+=Dt;
        if (AuditClock>16 && AuditPhase==0)
        {
            Afterlight::CVar(TEXT("r.ProfileGPU.ShowUI"),0);
            Cast<APlayerController>(Player->GetController())->ConsoleCommand(TEXT("ProfileGPU"),true);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Evidence/profile-quality.png"),false,false);
            ++AuditPhase;
        }
        if (AuditClock>23) FPlatformMisc::RequestExit(false);
    }
    if (bTitle || bPaused || bLost || bWon) return;
    RunTime+=Dt;
    TickOrientation(Dt);
    NoticeTime=FMath::Max(0.f,NoticeTime-Dt);
    NoiseTime=FMath::Max(0.f,NoiseTime-Dt);
    if (bEvacuation && EvacuationTime>0)
    {
        EvacuationTime-=Dt;
        if (EvacuationTime<=0)
        {
            for(AFacilityDevice* D:Devices) if(D->Kind==EDeviceKind::Lift) D->Open();
            Notify(TEXT("SURFACE LIFT READY.  Run to the east end of the hall."),7);
            Sound(TEXT("Pickup"),FVector(4200,0,140),0.7f);
        }
    }
    if (bEvacuation && EvacuationTime<=0 && Player && Player->GetActorLocation().X>4490 && FMath::Abs(Player->GetActorLocation().Y)<220) Win();
}
