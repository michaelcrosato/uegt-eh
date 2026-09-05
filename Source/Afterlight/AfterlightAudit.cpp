#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "DLSSLibrary.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"

void AAfterlightGameMode::AuditTick(float Dt)
{
    if(!Player || !Warden) return;
    AuditClock+=Dt;
    auto Check=[this](bool Pass,const FString& Name)
    {
        (Pass ? AuditPasses : AuditFailures).Add(Name);
        UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT AUDIT %s: %s"),Pass ? TEXT("PASS") : TEXT("FAIL"),*Name);
    };
    auto Device=[this](EDeviceKind Kind)->AFacilityDevice*
    {
        for(AFacilityDevice* D:Devices) if(D->Kind==Kind) return D;
        return nullptr;
    };
    auto Use=[this,&Check](AFacilityDevice* D)
    {
        if(!D) { Check(false,TEXT("missing device")); return; }
        FVector P=D->GetActorLocation()+D->GetActorForwardVector()*185;
        P.Z=92;
        Player->SetActorLocation(P);
        const FRotator R=(D->GetActorLocation()-Player->Camera->GetComponentLocation()).Rotation();
        Cast<APlayerController>(Player->GetController())->SetControlRotation(R);
        Player->Camera->SetWorldRotation(R);
        Check(Player->TraceInteract()==D,TEXT("visible interaction: ")+D->Id.ToString());
        Player->Interact();
    };
    auto Camera=[this](FVector P,FRotator R)
    {
        Player->SetActorLocation(P);
        Cast<APlayerController>(Player->GetController())->SetControlRotation(R);
        Player->Camera->SetWorldRotation(R);
    };
    auto Shot=[this](const FString& Name)
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Evidence");
        IFileManager::Get().MakeDirectory(*Dir,true);
        FScreenshotRequest::RequestScreenshot(Dir/Name+TEXT(".png"),false,false);
    };
    if(AuditPhase==0 && AuditClock>16)
    {
        Check(bHardwareReady,TEXT("D3D12 hardware ray tracing active"));
        Check(bDLSS && UDLSSLibrary::IsDLSSEnabled(),TEXT("DLSS Super Resolution enabled"));
        Check(bRayReconstruction,TEXT("DLSS Ray Reconstruction enabled"));
        Check(Afterlight::IntCVar(TEXT("r.MegaLights.EnableForProject"))==1 && Afterlight::IntCVar(TEXT("r.MegaLights.HardwareRayTracing"))==1,TEXT("MegaLights HWRT direct lighting"));
        Check(Afterlight::IntCVar(TEXT("r.Lumen.HardwareRayTracing"))==1 && Afterlight::IntCVar(TEXT("r.Lumen.HardwareRayTracing.LightingMode"))==2,TEXT("Lumen HWRT hit lighting"));
        Check(Afterlight::IntCVar(TEXT("r.MegaLights.ScreenTraces"))==0 && Afterlight::IntCVar(TEXT("r.Lumen.Reflections.ScreenTraces"))==0 && Afterlight::IntCVar(TEXT("r.Lumen.ScreenProbeGather.ScreenTraces"))==0,TEXT("screen-space lighting fallbacks disabled"));
        bool LightContract=true,MeshContract=true,MaterialsValid=true,SurfaceCacheReady=true;
        for(AFacilityLight* L:Lights) LightContract &= L->Light->CastShadows && L->Light->bAllowMegaLights && L->Light->MegaLightsShadowMethod==EMegaLightsShadowMethod::RayTracing;
        for(const auto& Pair:Facility->Batches)
        {
            MeshContract &= Pair.Value->CastShadow && Pair.Value->bVisibleInRayTracing;
            UMaterialInterface* M=Pair.Value->GetMaterial(0);
            MaterialsValid &= M && M->GetName().StartsWith(TEXT("M_")) && M->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes) && M->CheckMaterialUsage_Concurrent(MATUSAGE_Nanite);
            SurfaceCacheReady &= Pair.Value->GetStaticMesh()->HasValidNaniteData() && Pair.Value->GetStaticMesh()->GetNaniteSettings().FallbackRelativeError==0;
        }
        Check(LightContract && Player->Flashlight->CastShadows,TEXT("every fixture and handheld light casts ray-traced shadows"));
        Check(MeshContract,TEXT("all environment batches cast shadows and exist in RT scene"));
        Check(MaterialsValid,TEXT("all instanced materials support their actual vertex factory"));
        Check(SurfaceCacheReady,TEXT("Nanite Lumen surface data with full-fidelity RT fallback meshes"));
        Check(!Device(EDeviceKind::SecurityDoor)->Use(this) && !Device(EDeviceKind::SecurityDoor)->bOpen,TEXT("security rejects missing card"));
        Check(!Device(EDeviceKind::Generator)->Use(this),TEXT("generator rejects missing fuse"));
        Check(!Device(EDeviceKind::Valve)->Use(this),TEXT("pressure valve requires generator"));
        Check(!Device(EDeviceKind::Evacuation)->Use(this),TEXT("lift rejects incomplete interlocks"));
        NoticeTime=0;
        Shot(TEXT("01-transfer-hall"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase>=1 && AuditPhase<=5)
    {
        if(AuditClock>1 && AuditClock<5) AuditFrameMs.Add(Dt*1000);
        if(AuditClock>6)
        {
            static const FVector Positions[]={FVector(450,520,92),FVector(1350,-620,92),FVector(2900,620,92),FVector(3950,1120,92),FVector(3350,-620,92)};
            static const FRotator Rotations[]={FRotator(-4,60,0),FRotator(-4,-65,0),FRotator(0,90,0),FRotator(-2,25,0),FRotator(-2,-48,0)};
            Shot(FString::Printf(TEXT("%02d-room"),AuditPhase+1));
            Camera(Positions[AuditPhase-1],Rotations[AuditPhase-1]);
            ++AuditPhase; AuditClock=0;
        }
    }
    else if(AuditPhase==6 && AuditClock>6)
    {
        Shot(TEXT("07-observation"));
        Camera(FVector(-650,0,92),FRotator(0,0,0));
        for(int I=0;I<6;++I) SetCircuit(I,false);
        Player->bFlashlightOn=false; Player->Flashlight->SetVisibility(false);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==7 && AuditClock>7)
    {
        bool Black=true;
        for(AFacilityLight* L:Lights) Black &= !L->IsLit() && !L->Light->IsVisible() && L->Light->Intensity==0;
        Check(Black && !Player->Flashlight->IsVisible(),TEXT("complete controllable blackout"));
        Shot(TEXT("08-blackout"));
        for(int I=0;I<6;++I) SetCircuit(I,true);
        Lights[0]->Smash(); Lights[0]->Toggle(); SetCircuit(0,false); SetCircuit(0,true);
        Check(Lights[0]->bBroken && !Lights[0]->IsLit() && Lights[0]->Light->Intensity==0,TEXT("broken lamp cannot be resurrected by switch or circuit"));
        Player->bFlashlightOn=true; Player->Flashlight->SetVisibility(true);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==8 && AuditClock>5)
    {
        Shot(TEXT("09-restored"));
        Use(Device(EDeviceKind::Card)); Check(bHasCard,TEXT("card acquired through interaction trace"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==9 && AuditClock>1)
    {
        Use(Device(EDeviceKind::Fuse)); Check(bHasFuse,TEXT("fuse acquired through interaction trace"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==10 && AuditClock>1)
    {
        Use(Device(EDeviceKind::SecurityDoor)); Check(Device(EDeviceKind::SecurityDoor)->bOpen,TEXT("plant opens with access card"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==11 && AuditClock>1)
    {
        Use(Device(EDeviceKind::Generator)); Check(bGenerator && !bHasFuse,TEXT("fuse installs and restores plant"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==12 && AuditClock>1)
    {
        Use(Device(EDeviceKind::Valve)); Check(bPressureVented,TEXT("powered pressure valve vents"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==13 && AuditClock>1)
    {
        Use(Device(EDeviceKind::Evacuation)); Check(bEvacuation,TEXT("complete interlocks begin lift countdown"));
        Camera(FVector(4020,0,92),FRotator(0,0,0));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==14 && bEvacuation && EvacuationTime<=0 && AuditClock>36)
    {
        Check(Device(EDeviceKind::Lift)->bOpen,TEXT("lift opens after survival countdown"));
        Camera(FVector(4640,0,92),FRotator(0,0,0));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==15 && AuditClock>2)
    {
        Check(bWon,TEXT("entering lift produces escape ending"));
        Shot(TEXT("10-escape"));
        WriteAudit();
        ++AuditPhase; AuditClock=0;
    }
    else if((AuditPhase==16 && AuditClock>2) || AuditClock>140)
    {
        if(AuditClock>140) { Check(false,TEXT("audit phase timed out")); WriteAudit(); }
        FPlatformMisc::RequestExit(false);
    }
}

void AAfterlightGameMode::WriteAudit()
{
    TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("gpu"),GRHIAdapterName);
    Root->SetStringField(TEXT("timestamp_utc"),FDateTime::UtcNow().ToIso8601());
    Root->SetBoolField(TEXT("passed"),AuditFailures.IsEmpty());
    Root->SetBoolField(TEXT("hardware_rt"),bHardwareReady);
    Root->SetBoolField(TEXT("dlss_sr"),bDLSS);
    Root->SetBoolField(TEXT("dlss_rr"),bRayReconstruction);
    Root->SetBoolField(TEXT("frame_generation_supported"),bFrameGenerationSupported);
    Root->SetBoolField(TEXT("frame_generation_enabled"),bFrameGeneration);
    Root->SetNumberField(TEXT("fixture_count"),Lights.Num());
    Root->SetNumberField(TEXT("navigation_nodes"),NavPoints.Num());
    Root->SetNumberField(TEXT("peak_process_physical_mb"),FPlatformMemory::GetStats().PeakUsedPhysical/(1024.0*1024));
    Root->SetNumberField(TEXT("machine_ram_gb"),FPlatformMemory::GetStats().TotalPhysical/(1024.0*1024*1024));
    TArray<TSharedPtr<FJsonValue>> Pass,Fail;
    for(const auto& S:AuditPasses) Pass.Add(MakeShared<FJsonValueString>(S));
    for(const auto& S:AuditFailures) Fail.Add(MakeShared<FJsonValueString>(S));
    Root->SetArrayField(TEXT("checks_passed"),Pass); Root->SetArrayField(TEXT("checks_failed"),Fail);
    if(!AuditFrameMs.IsEmpty())
    {
        AuditFrameMs.Sort(); double Sum=0; for(float V:AuditFrameMs) Sum+=V;
        Root->SetNumberField(TEXT("mean_render_frame_ms"),Sum/AuditFrameMs.Num());
        Root->SetNumberField(TEXT("mean_render_fps"),1000/(Sum/AuditFrameMs.Num()));
        Root->SetNumberField(TEXT("p95_render_frame_ms"),AuditFrameMs[FMath::FloorToInt((AuditFrameMs.Num()-1)*0.95f)]);
        Root->SetNumberField(TEXT("p99_render_frame_ms"),AuditFrameMs[FMath::FloorToInt((AuditFrameMs.Num()-1)*0.99f)]);
        Root->SetNumberField(TEXT("sample_count"),AuditFrameMs.Num());
    }
    FString Json;
    FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("Evidence");
    IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("runtime-audit.json")));
    UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT AUDIT COMPLETE: %d passed, %d failed"),AuditPasses.Num(),AuditFailures.Num());
}
