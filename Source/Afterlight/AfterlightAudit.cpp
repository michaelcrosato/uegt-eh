#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Slate/SceneViewport.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "HAL/PlatformFileManager.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "DLSSLibrary.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StreamlineLibraryDLSSG.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"

namespace
{
    bool bAuditReloadPending=false;
    TArray<FString> SavedPasses, SavedFailures;
    TArray<float> SavedFrameTimes;
    float ObservedFGRenderFPS=0,ObservedFGPresentFPS=0,ObservedSmoothFPS=0,ObservedShowcaseFPS=0;
    double SmoothSeconds=0,ShowcaseSeconds=0,FGSeconds=0,FGPresentSum=0;
    int32 SmoothSamples=0,ShowcaseSamples=0,FGSamples=0,FGGeneratedSamples=0,FGFocusedSamples=0;
}

void AAfterlightGameMode::AuditTick(float Dt)
{
    if(!Player || !Warden) return;
    AuditClock+=Dt;
    // Sample before asynchronous screenshot readback stalls either render or presentation.
    if(AuditClock>1 && AuditClock<4)
    {
        if(AuditPhase==55) { SmoothSeconds+=Dt; ++SmoothSamples; }
        if(AuditPhase==21) { ShowcaseSeconds+=Dt; ++ShowcaseSamples; }
        if(AuditPhase==22)
        {
            float FPS=0; int32 Frames=0; UStreamlineLibraryDLSSG::GetDLSSGFrameTiming(FPS,Frames);
            FGSeconds+=Dt; FGPresentSum+=FPS; ++FGSamples; FGGeneratedSamples+=Frames>1 ? 1 : 0;
            FGFocusedSamples+=FApp::HasFocus() ? 1 : 0;
        }
    }
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
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->SetActorLocation(P);
        Cast<APlayerController>(Player->GetController())->SetControlRotation(R);
        Player->Camera->SetWorldRotation(R);
    };
    auto Shot=[this](const FString& Name)->bool
    {
        if (AuditShotPhase!=AuditPhase)
        {
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("Evidence");
            IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/Name+TEXT(".png"),false,false);
            AuditShotPhase=AuditPhase;
            AuditShotTime=AuditClock;
            return false;
        }
        // Requests are asynchronous. Preserve the captured state through readback.
        return AuditClock-AuditShotTime>0.65f;
    };
    auto Key=[this](FKey K,EInputEvent Event)
    {
        Cast<APlayerController>(Player->GetController())->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released ? 0.f : 1.f));
    };
    auto FocusWindow=[]()
    {
        // The SDK also checks native Windows focus independently of FApp.
        if(GEngine && GEngine->GameViewport)
        {
            TSharedPtr<SWindow> Window=GEngine->GameViewport->GetWindow();
            if(!Window && GEngine->GameViewport->GetGameViewport())
                Window=GEngine->GameViewport->GetGameViewport()->FindWindow();
            UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT audit window: found=%d native=%d"),Window.IsValid(),Window && Window->GetNativeWindow().IsValid());
            if(Window)
            {
                Window->BringToFront(true);
                Window->HACK_ForceToFront();
                if(Window->GetNativeWindow()) Window->GetNativeWindow()->SetWindowFocus();
            }
        }
    };
    auto HoldForward=[this,&Key]()
    {
        if(AuditInputPhase!=AuditPhase) { Key(EKeys::W,IE_Pressed); AuditInputPhase=AuditPhase; }
    };
    if(FParse::Param(FCommandLine::Get(),TEXT("AfterlightNoRTAudit")))
    {
        if(AuditPhase==0 && AuditClock>7)
        {
            if(!Shot(TEXT("15-ray-tracing-required"))) return;
            Check(!bHardwareReady && bTitle && !Player->CanAct(),TEXT("non-RT runtime displays refusal and disables gameplay"));
            Player->Confirm();
            Check(bTitle && !Player->CanAct(),TEXT("Enter cannot bypass hardware ray tracing requirement"));
            WriteAudit(); AuditPhase=101; AuditClock=0;
        }
        else if(AuditPhase==101 && AuditClock>2) FPlatformMisc::RequestExitWithStatus(false,AuditFailures.IsEmpty() ? 0 : 1);
        return;
    }
    if(!bAuditReloadPending && AuditPhase==0 && FParse::Param(FCommandLine::Get(),TEXT("AfterlightGameplayOnly")))
    {
        AuditPhase=16; AuditClock=0;
        AuditStartPosition=FVector(-500,0,92);
        Camera(AuditStartPosition,FRotator::ZeroRotator);
        for(AFacilityDevice* D:Devices) if(D->Kind==EDeviceKind::SecurityDoor) D->Open();
    }
    if(AuditPhase==0 && AuditClock>12 && FParse::Param(FCommandLine::Get(),TEXT("AfterlightFGOnly")))
    {
        FocusWindow();
        Notify(TEXT("FRAME GENERATION TEST / Click this game window and keep it in the foreground."),300);
        AuditPhase=90; AuditClock=0;
    }
    if(bAuditReloadPending && AuditPhase==0)
    {
        AuditPasses=SavedPasses; AuditFailures=SavedFailures; AuditFrameMs=SavedFrameTimes;
        Check(!bWon && !bLost && !bHasCard && !bHasFuse && !bGenerator && !bPressureVented && !bEvacuation && BrokenLights==0,TEXT("level reload resets every objective and ending"));
        bool Clean=true;
        for(AFacilityLight* L:Lights) Clean &= !L->bBroken && L->bSwitchedOn;
        for(AFacilityDevice* D:Devices) Clean &= !D->bUsed && !D->bOpen;
        Check(Clean && !Circuits[3] && Circuits[0] && Circuits[1] && Circuits[2] && Circuits[4] && Circuits[5],TEXT("retry restores pickups, doors, fixtures and starting circuits"));
        Check(FMath::IsNearlyEqual(MouseSensitivity,0.75f) && QualityPreset==0,TEXT("user settings persist across actual level reload"));
        bAuditReloadPending=false;
        bTitle=true; AuditPhase=100; AuditClock=0;
        return;
    }
    if(AuditPhase==0 && AuditClock>16)
    {
        if(!Shot(TEXT("01-transfer-hall"))) return;
        Check(bHardwareReady,TEXT("D3D12 hardware ray tracing active"));
        Check(bDLSS && UDLSSLibrary::IsDLSSEnabled(),TEXT("DLSS Super Resolution enabled"));
        Check(bRayReconstruction,TEXT("DLSS Ray Reconstruction enabled"));
        Check(Afterlight::IntCVar(TEXT("r.Lumen.Reflections.BilateralFilter"))==0 && Afterlight::IntCVar(TEXT("r.Lumen.Reflections.Temporal"))==0,TEXT("Ray Reconstruction receives raw reflections without double denoising"));
        Check(Afterlight::IntCVar(TEXT("r.MegaLights.EnableForProject"))==1 && Afterlight::IntCVar(TEXT("r.MegaLights.HardwareRayTracing"))==1,TEXT("MegaLights HWRT direct lighting"));
        Check(Afterlight::IntCVar(TEXT("r.Lumen.HardwareRayTracing"))==1 && Afterlight::IntCVar(TEXT("r.Lumen.HardwareRayTracing.LightingMode"))==2,TEXT("Lumen HWRT hit lighting"));
        Check(Afterlight::IntCVar(TEXT("r.MegaLights.ScreenTraces"))==0 && Afterlight::IntCVar(TEXT("r.Lumen.Reflections.ScreenTraces"))==0 && Afterlight::IntCVar(TEXT("r.Lumen.ScreenProbeGather.ScreenTraces"))==0,TEXT("screen-space lighting fallbacks disabled"));
        Check(Afterlight::IntCVar(TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HardwareRayTracing"))==1,TEXT("short-range contact occlusion also uses hardware rays"));
        bool LightContract=true,MeshContract=true,MaterialsValid=true,SurfaceCacheReady=true;
        for(AFacilityLight* L:Lights) LightContract &= L->Light->CastShadows && L->Light->bAllowMegaLights && L->Light->MegaLightsShadowMethod==EMegaLightsShadowMethod::RayTracing;
        for(const auto& Pair:Facility->Batches)
        {
            MeshContract &= Pair.Value->CastShadow && Pair.Value->bVisibleInRayTracing;
            UMaterialInterface* M=Pair.Value->GetMaterial(0);
            MaterialsValid &= M && M->GetName().StartsWith(TEXT("M_")) && M->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes) && M->CheckMaterialUsage_Concurrent(MATUSAGE_Nanite);
            UStaticMesh* Mesh=Pair.Value->GetStaticMesh();
            const auto* RenderData=Mesh->GetRenderData();
            const uint32 SourceTriangles=RenderData && RenderData->NaniteResourcesPtr.Get() ? RenderData->NaniteResourcesPtr.Get()->NumInputTriangles : 0;
            // Compare the cooked source-triangle metadata directly with the fallback render data.
            const bool Valid=Mesh->HasValidNaniteData() && SourceTriangles>0 && uint32(Mesh->GetNumTriangles(0))==SourceTriangles;
            SurfaceCacheReady &= Valid;
            if(!Valid) UE_LOG(LogTemp,Warning,TEXT("AFTERLIGHT mesh audit %s: Nanite=%d fallback triangles=%d source triangles=%u"),*Mesh->GetName(),Mesh->HasValidNaniteData(),Mesh->GetNumTriangles(0),SourceTriangles);
        }
        bool AllPhysicalMeshes=true;
        for(TActorIterator<AActor> A(GetWorld());A;++A)
        {
            TInlineComponentArray<UStaticMeshComponent*> Meshes; A->GetComponents(Meshes);
            for(UStaticMeshComponent* M:Meshes) if(M->GetStaticMesh()) AllPhysicalMeshes &= M->CastShadow && M->bVisibleInRayTracing;
        }
        Check(LightContract && Player->Flashlight->CastShadows,TEXT("every fixture and handheld light casts ray-traced shadows"));
        Check(MeshContract,TEXT("all environment batches cast shadows and exist in RT scene"));
        Check(AllPhysicalMeshes,TEXT("props, fixtures, tool and enemy meshes also cast shadows and exist in RT scene"));
        Check(MaterialsValid,TEXT("all instanced materials support their actual vertex factory"));
        Check(SurfaceCacheReady,TEXT("Nanite Lumen surface data with full-fidelity RT fallback meshes"));
        Check(!Device(EDeviceKind::SecurityDoor)->Use(this) && !Device(EDeviceKind::SecurityDoor)->bOpen,TEXT("security rejects missing card"));
        Check(!Device(EDeviceKind::Generator)->Use(this),TEXT("generator rejects missing fuse"));
        Check(!Device(EDeviceKind::Valve)->Use(this),TEXT("pressure valve requires generator"));
        Check(!Device(EDeviceKind::Evacuation)->Use(this),TEXT("lift rejects incomplete interlocks"));
        NoticeTime=0;
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase>=1 && AuditPhase<=5)
    {
        if(AuditClock>1 && AuditClock<5) AuditFrameMs.Add(Dt*1000);
        if(AuditClock>6)
        {
            static const FVector Positions[]={FVector(450,520,92),FVector(1350,-620,92),FVector(2900,620,92),FVector(3950,1120,92),FVector(3350,-620,92)};
            static const FRotator Rotations[]={FRotator(-4,60,0),FRotator(-4,-65,0),FRotator(0,90,0),FRotator(-2,25,0),FRotator(-2,-48,0)};
            if(!Shot(FString::Printf(TEXT("%02d-room"),AuditPhase+1))) return;
            Camera(Positions[AuditPhase-1],Rotations[AuditPhase-1]);
            ++AuditPhase; AuditClock=0;
        }
    }
    else if(AuditPhase==6 && AuditClock>6)
    {
        if(!Shot(TEXT("07-observation"))) return;
        Camera(FVector(-650,0,92),FRotator(0,0,0));
        for(int I=0;I<6;++I) SetCircuit(I,false);
        Player->bFlashlightOn=false; Player->Flashlight->SetVisibility(false);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==7 && AuditClock>7)
    {
        if(!Shot(TEXT("08-blackout"))) return;
        bool Black=true;
        for(AFacilityLight* L:Lights) Black &= !L->IsLit() && !L->Light->IsVisible() && L->Light->Intensity==0;
        Check(Black && !Player->Flashlight->IsVisible(),TEXT("complete controllable blackout"));
        for(int I=0;I<6;++I) SetCircuit(I,true);
        Lights[0]->Smash(); Lights[0]->Toggle(); SetCircuit(0,false); SetCircuit(0,true);
        Check(Lights[0]->bBroken && !Lights[0]->IsLit() && Lights[0]->Light->Intensity==0,TEXT("broken lamp cannot be resurrected by switch or circuit"));
        Player->bFlashlightOn=true; Player->Flashlight->SetVisibility(true);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==8 && AuditClock>5)
    {
        if(!Shot(TEXT("09-restored"))) return;
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
        if(!Shot(TEXT("10-escape"))) return;
        Check(bWon,TEXT("entering lift produces escape ending"));
        bWon=false;
        Camera(FVector(-500,0,92),FRotator(0,0,0));
        AuditStartPosition=Player->GetActorLocation();
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==16)
    {
        HoldForward();
        if(AuditClock>1.5f)
        {
            Key(EKeys::W,IE_Released);
            UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT move probe: pos=%s start=%s canact=%d mode=%d tick=%d active=%d velocity=%s acceleration=%s floor=%d"),*Player->GetActorLocation().ToString(),*AuditStartPosition.ToString(),Player->CanAct(),int32(Player->GetCharacterMovement()->MovementMode),Player->GetCharacterMovement()->IsComponentTickEnabled(),Player->GetCharacterMovement()->IsActive(),*Player->GetVelocity().ToString(),*Player->GetCharacterMovement()->GetCurrentAcceleration().ToString(),Player->GetCharacterMovement()->CurrentFloor.IsWalkableFloor());
            Check(Player->GetActorLocation().X>AuditStartPosition.X+260,TEXT("first-person movement advances through physical world"));
            Camera(FVector(-600,0,92),FRotator(0,180,0));
            ++AuditPhase; AuditClock=0;
        }
    }
    else if(AuditPhase==17)
    {
        HoldForward();
        if(AuditClock>2)
        {
            Key(EKeys::W,IE_Released);
            UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT wall probe: pos=%s"),*Player->GetActorLocation().ToString());
            Check(Player->GetActorLocation().X>-864 && Player->GetActorLocation().X<-830,TEXT("capsule collision stops movement at solid wall"));
            Camera(FVector(450,0,92),FRotator(0,90,0));
            ++AuditPhase; AuditClock=0;
        }
    }
    else if(AuditPhase==18)
    {
        HoldForward();
        if(AuditClock>3.5f)
        {
            Key(EKeys::W,IE_Released);
            UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT doorway probe: pos=%s canact=%d"),*Player->GetActorLocation().ToString(),Player->CanAct());
            Check(Player->GetActorLocation().Y>700,TEXT("player physically traverses doorway into Records"));
            bool RoutesClear=true;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(AuditTraversal),false,Player);
            Params.AddIgnoredActor(Warden);
            for(int32 A=0;A<NavPoints.Num();++A) for(int32 B:NavLinks[A]) if(B>A)
            {
                FHitResult Hit;
                const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,NavPoints[A],NavPoints[B],FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(28,86),Params);
                if(Blocked) UE_LOG(LogTemp,Warning,TEXT("AFTERLIGHT route blocked %d->%d by %s"),A,B,*GetNameSafe(Hit.GetComponent()));
                RoutesClear &= !Blocked;
            }
            Check(RoutesClear,TEXT("every room navigation edge has player-sized clearance after unlock"));
            Camera(FVector(0,0,92),FRotator(0,0,0));
            Key(EKeys::LeftShift,IE_Pressed);
            AuditStartPosition=Player->GetActorLocation();
            ++AuditPhase; AuditClock=0;
        }
    }
    else if(AuditPhase==19)
    {
        HoldForward();
        if(AuditClock>1.5f)
        {
            Key(EKeys::W,IE_Released);
            Check(Player->GetActorLocation().X>AuditStartPosition.X+470 && Player->Stamina<0.9f,TEXT("sprinting moves faster and spends stamina"));
            Key(EKeys::LeftShift,IE_Released); Key(EKeys::LeftControl,IE_Pressed);
            ++AuditPhase; AuditClock=0;
        }
    }
    else if(AuditPhase==20 && AuditClock>0.5f)
    {
        Check(Player->bIsCrouched && Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()<60,TEXT("crouch changes physical player capsule"));
        Key(EKeys::LeftControl,IE_Released);
        AuditPhase=50; AuditClock=0;
    }
    else if(AuditPhase==50 && AuditClock>0.5f)
    {
        const FVector L=Lights[1]->GetActorLocation();
        Camera(FVector(L.X-150,L.Y,92),FRotator::ZeroRotator);
        const FRotator R=(L-Player->Camera->GetComponentLocation()).Rotation();
        Cast<APlayerController>(Player->GetController())->SetControlRotation(R);
        Player->Camera->SetWorldRotation(R);
        Check(Player->TraceInteract()==Lights[1],TEXT("fixture is reachable by actual aim trace"));
        Key(EKeys::E,IE_Pressed); Key(EKeys::E,IE_Released);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==51 && AuditClock>0.5f)
    {
        Check(!Lights[1]->bSwitchedOn && !Lights[1]->IsLit(),TEXT("E key switches aimed fixture off"));
        Key(EKeys::E,IE_Pressed); Key(EKeys::E,IE_Released);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==52 && AuditClock>0.5f)
    {
        Check(Lights[1]->bSwitchedOn && Lights[1]->IsLit(),TEXT("E key restores aimed fixture"));
        Key(EKeys::LeftMouseButton,IE_Pressed); Key(EKeys::LeftMouseButton,IE_Released);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==53 && AuditClock>0.8f)
    {
        Check(Lights[1]->bBroken && !Lights[1]->IsLit() && BrokenLights==1 && NoiseTime>0,TEXT("mouse strike destroys fixture and creates audible distraction"));
        SetCircuit(0,false); SetCircuit(0,true); Lights[1]->Toggle();
        Check(!Lights[1]->IsLit(),TEXT("input-destroyed fixture stays dark after power restoration"));
        AFacilityDevice* Breaker=Device(EDeviceKind::Breaker);
        const bool WasOn=Circuits[Breaker->Circuit];
        Use(Breaker);
        Check(Circuits[Breaker->Circuit]!=WasOn,TEXT("physical breaker interaction toggles its room circuit"));
        SetCircuit(Breaker->Circuit,WasOn);
        Key(EKeys::F,IE_Pressed); Key(EKeys::F,IE_Released);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==54 && AuditClock>0.5f)
    {
        Check(!Player->bFlashlightOn && !Player->Flashlight->IsVisible(),TEXT("F key extinguishes handheld lamp"));
        Key(EKeys::F,IE_Pressed); Key(EKeys::F,IE_Released);
        // Try to use the generator through its enclosing north wall.
        AFacilityDevice* Generator=Device(EDeviceKind::Generator);
        FVector P=Generator->GetActorLocation()+FVector(-160,-500,0); P.Z=92;
        Camera(P,FRotator::ZeroRotator);
        const FRotator R=(Generator->GetActorLocation()-Player->Camera->GetComponentLocation()).Rotation();
        Cast<APlayerController>(Player->GetController())->SetControlRotation(R);
        Player->Camera->SetWorldRotation(R);
        Check(Player->TraceInteract()!=Generator,TEXT("out-of-reach mission equipment cannot be used remotely"));
        Camera(FVector(-500,0,92),FRotator::ZeroRotator);
        QualityPreset=1; ApplyGraphics();
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==55 && AuditClock>5)
    {
        if(!Shot(TEXT("16-smooth"))) return;
        ObservedSmoothFPS=SmoothSeconds>0 ? SmoothSamples/SmoothSeconds : 0;
        Check(bHardwareReady && bRayReconstruction && Afterlight::IntCVar(TEXT("r.ScreenPercentage"))<60,TEXT("Smooth retains HWRT and Ray Reconstruction with DLSS Balanced"));
        QualityPreset=2; ApplyGraphics();
        AuditPhase=21; AuditClock=0;
    }
    else if(AuditPhase==21 && AuditClock>4)
    {
        if(!Shot(TEXT("11-showcase"))) return;
        ObservedShowcaseFPS=ShowcaseSeconds>0 ? ShowcaseSamples/ShowcaseSeconds : 0;
        Check(Afterlight::IntCVar(TEXT("r.ScreenPercentage"))==100 && Afterlight::IntCVar(TEXT("r.MegaLights.DownsampleMode"))==0 && bHardwareReady,TEXT("Showcase enables DLAA and full-resolution hardware ray tracing"));
        FocusWindow();
        QualityPreset=0; bFrameGeneration=true; ApplyGraphics();
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==90)
    {
        // Interactive verification must not fabricate focus. Allow the user to
        // activate the actual window when Windows declines the foreground request.
        if(FApp::HasFocus())
        {
            bFrameGeneration=true; ApplyGraphics();
            AuditPhase=22; AuditClock=0;
        }
        else if(AuditClock>300)
        {
            Check(false,TEXT("Frame Generation verification requires the game window in the foreground"));
            WriteAudit(); AuditPhase=101; AuditClock=0;
        }
    }
    else if(AuditPhase==22 && AuditClock>5)
    {
        if(!Shot(TEXT("12-frame-generation"))) return;
        int32 Frames=0; float FPS=0;
        UStreamlineLibraryDLSSG::GetDLSSGFrameTiming(FPS,Frames);
        ObservedFGRenderFPS=FGSeconds>0 ? FGSamples/FGSeconds : 0;
        ObservedFGPresentFPS=FGSamples>0 ? FGPresentSum/FGSamples : 0;
        UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT FG observed: presented=%.2f render=%.2f generated samples=%d/%d mode=%d"),ObservedFGPresentFPS,ObservedFGRenderFPS,FGGeneratedSamples,FGSamples,int32(UStreamlineLibraryDLSSG::GetDLSSGMode()));
        UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT FG context: focus=%d enable=%d frames=%d viewOverride=%d viewIndex=%d vsync=%d"),FApp::HasFocus(),Afterlight::IntCVar(TEXT("r.Streamline.DLSSG.Enable")),Afterlight::IntCVar(TEXT("r.Streamline.DLSSG.FramesToGenerate")),Afterlight::IntCVar(TEXT("r.Streamline.ViewIdOverride")),Afterlight::IntCVar(TEXT("r.Streamline.ViewIndexToTag")),Afterlight::IntCVar(TEXT("r.VSync")));
        Check(FGSamples>30 && FGFocusedSamples==FGSamples,TEXT("Frame Generation samples run with actual foreground window focus"));
        Check(bFrameGenerationSupported && UStreamlineLibraryDLSSG::GetDLSSGMode()==EStreamlineDLSSGMode::On2X && FGGeneratedSamples>30 && ObservedFGPresentFPS>1.45f*ObservedFGRenderFPS,TEXT("2x DLSS Frame Generation actually presents additional frames"));
        if(FParse::Param(FCommandLine::Get(),TEXT("AfterlightFGOnly")))
        {
            WriteAudit(); AuditPhase=101; AuditClock=0; return;
        }
        bFrameGeneration=false; ApplyGraphics();
        SetPaused(true); AuditStartValue=RunTime; AuditStartPosition=Warden->GetActorLocation();
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==23 && AuditClock>1)
    {
        Check(FMath::IsNearlyEqual(RunTime,AuditStartValue) && Warden->GetActorLocation().Equals(AuditStartPosition),TEXT("pause freezes gameplay while renderer stays live"));
        SetPaused(false);
        for(int I=0;I<6;++I) SetCircuit(I,false);
        Player->bFlashlightOn=false; Player->Flashlight->SetVisibility(false);
        Camera(FVector(0,0,92),FRotator(0,0,0));
        Warden->SetActorLocation(FVector(1000,0,110));
        Warden->Suspicion=0; Warden->bHunting=false; Warden->Path.Reset();
        GracePeriod=0; bAuditFreezeAI=false; NoiseTime=0;
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==24 && AuditClock>3)
    {
        Check(Warden->CanSeePlayer() && !Warden->bHunting && Warden->Suspicion<0.1f,TEXT("quiet player in darkness avoids visual detection"));
        Player->ToggleFlashlight();
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==25 && AuditClock>2.5f)
    {
        Check(Warden->bHunting && Warden->Suspicion>0.55f,TEXT("visible handheld light triggers active pursuit"));
        Player->ToggleFlashlight();
        Camera(FVector(1400,-680,92),FRotator(0,90,0));
        Check(!Warden->CanSeePlayer(),TEXT("solid room walls block enemy sight"));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==26 && AuditClock>8)
    {
        Check(!Warden->bHunting && !bLost,TEXT("breaking sight and concealing position ends pursuit"));
        Warden->SetActorLocation(FVector(1400,0,110));
        Warden->Path.Reset(); Warden->Suspicion=0; Warden->bHunting=false;
        Noise(FVector(1400,-980,92),1);
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==27 && AuditClock>2)
    {
        Check(Warden->GetActorLocation().Y<-100,TEXT("enemy hears noise and navigates a doorway toward it"));
        Player->ToggleFlashlight();
        Camera(FVector(1400,-1040,92),FRotator(0,90,0));
        ++AuditPhase; AuditClock=0;
    }
    else if(AuditPhase==28 && (bLost || AuditClock>10))
    {
        if(!Shot(TEXT("13-captured"))) return;
        Check(bLost,TEXT("enemy physically reaches player and produces capture ending"));
        bAuditFreezeAI=true;
        QualityPreset=0; MouseSensitivity=0.75f; SaveSettings();
        SavedPasses=AuditPasses; SavedFailures=AuditFailures; SavedFrameTimes=AuditFrameMs;
        bAuditReloadPending=true;
        RestartRun();
    }
    else if(AuditPhase==100 && AuditClock>5)
    {
        if(!Shot(TEXT("14-title-after-retry"))) return;
        Key(EKeys::Enter,IE_Pressed); Key(EKeys::Enter,IE_Released);
        AuditPhase=102; AuditClock=0;
    }
    else if(AuditPhase==102 && AuditClock>0.3f)
    {
        Check(!bTitle && Player->CanAct(),TEXT("Enter key binding starts a fresh run after actual retry"));
        Check(Player->GetCharacterMovement()->MovementMode==MOVE_Walking,TEXT("fresh possession initializes walking after retry"));
        WriteAudit();
        AuditPhase=101; AuditClock=0;
    }
    else if((AuditPhase==101 && AuditClock>2) || AuditClock>140)
    {
        if(AuditClock>140) { Check(false,TEXT("audit phase timed out")); WriteAudit(); }
        FPlatformMisc::RequestExitWithStatus(false,AuditFailures.IsEmpty() ? 0 : 1);
    }
}

void AAfterlightGameMode::WriteAudit()
{
    TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("gpu"),GRHIAdapterName);
    Root->SetStringField(TEXT("timestamp_utc"),FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("audit_scope"),FParse::Param(FCommandLine::Get(),TEXT("AfterlightNoRTAudit")) ? TEXT("no-RT refusal") : FParse::Param(FCommandLine::Get(),TEXT("AfterlightFGOnly")) ? TEXT("focused frame generation regression") : FParse::Param(FCommandLine::Get(),TEXT("AfterlightGameplayOnly")) ? TEXT("focused gameplay regression") : TEXT("full runtime integration"));
    Root->SetBoolField(TEXT("passed"),AuditFailures.IsEmpty());
    Root->SetBoolField(TEXT("hardware_rt"),bHardwareReady);
    Root->SetBoolField(TEXT("automation_foreground_focus_override"),false);
    Root->SetBoolField(TEXT("dlss_sr"),bDLSS);
    Root->SetBoolField(TEXT("dlss_rr"),bRayReconstruction);
    Root->SetBoolField(TEXT("frame_generation_supported"),bFrameGenerationSupported);
    Root->SetBoolField(TEXT("frame_generation_enabled"),bFrameGeneration);
    Root->SetNumberField(TEXT("fg_observed_render_fps"),ObservedFGRenderFPS);
    Root->SetNumberField(TEXT("fg_observed_present_fps"),ObservedFGPresentFPS);
    Root->SetNumberField(TEXT("fg_sample_count"),FGSamples);
    Root->SetNumberField(TEXT("fg_samples_with_generated_frames"),FGGeneratedSamples);
    Root->SetNumberField(TEXT("fg_samples_with_foreground_focus"),FGFocusedSamples);
    Root->SetNumberField(TEXT("smooth_observed_render_fps"),ObservedSmoothFPS);
    Root->SetNumberField(TEXT("showcase_observed_render_fps"),ObservedShowcaseFPS);
    Root->SetStringField(TEXT("preset_comparison_scope"),TEXT("three-second stationary hallway samples after warm-up, before screenshot readback; not the multi-room benchmark"));
    Root->SetNumberField(TEXT("fixture_count"),Lights.Num());
    Root->SetNumberField(TEXT("navigation_nodes"),NavPoints.Num());
    Root->SetStringField(TEXT("benchmark_preset"),TEXT("Quality / DLSS Quality / FG off"));
    Root->SetStringField(TEXT("benchmark_scope"),TEXT("five fixed camera samples; shader warm-up and transitions excluded; AI frozen during camera samples"));
    int32 Width=0,Height=0; Cast<APlayerController>(Player->GetController())->GetViewportSize(Width,Height);
    Root->SetNumberField(TEXT("output_width"),Width);
    Root->SetNumberField(TEXT("output_height"),Height);
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
    const FString Report=FParse::Param(FCommandLine::Get(),TEXT("AfterlightNoRTAudit")) ? TEXT("no-rt-audit.json") : FParse::Param(FCommandLine::Get(),TEXT("AfterlightFGOnly")) ? TEXT("frame-generation-audit.json") : TEXT("runtime-audit.json");
    FFileHelper::SaveStringToFile(Json,*(Dir/Report));
    UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT AUDIT COMPLETE: %d passed, %d failed"),AuditPasses.Num(),AuditFailures.Num());
}
