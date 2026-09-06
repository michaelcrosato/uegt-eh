#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    bool bOrientationReloadPending=false;
    TArray<FString> OrientationSavedPasses,OrientationSavedFailures;
    float SavedDuration=0,SavedMaxMove=0;
    int32 SavedSamples=0,SavedMoving=0;
}

void AAfterlightGameMode::OrientationAuditTick(float Dt)
{
    if(!Player || !Warden) return;
    AuditClock+=Dt;
    auto* PC=Cast<APlayerController>(Player->GetController());
    auto Check=[this](bool Pass,const FString& Name)
    {
        (Pass ? AuditPasses : AuditFailures).Add(Name);
        UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT ORIENTATION %s: %s"),Pass ? TEXT("PASS") : TEXT("FAIL"),*Name);
    };
    auto Key=[PC](FKey K,EInputEvent Event)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released ? 0.f : 1.f));
    };
    auto Tap=[&Key](FKey K) { Key(K,IE_Pressed); Key(K,IE_Released); };
    auto Aim=[this,PC](FVector Target)
    {
        const FRotator R=(Target-Player->Camera->GetComponentLocation()).Rotation();
        PC->SetControlRotation(R); Player->Camera->SetWorldRotation(R);
    };
    auto Next=[this,&Key]()
    {
        Key(EKeys::W,IE_Released);
        ++OrientationAuditStep; AuditClock=0;
        UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT orientation step %d; player=%s"),OrientationAuditStep,*Player->GetActorLocation().ToString());
    };
    auto Walk=[this,PC,&Key](FVector2D Target)
    {
        FVector D(Target.X-Player->GetActorLocation().X,Target.Y-Player->GetActorLocation().Y,0);
        if(D.Size2D()<28) { Key(EKeys::W,IE_Released); return true; }
        const FRotator R(0,D.Rotation().Yaw,0);
        PC->SetControlRotation(R); Player->Camera->SetWorldRotation(R);
        Key(EKeys::W,IE_Pressed);
        return false;
    };
    auto Use=[this,&Aim,&Tap,&Check](FName Id)
    {
        AFacilityDevice* Found=nullptr;
        for(AFacilityDevice* D:Devices) if(D->Id==Id) Found=D;
        Check(Found!=nullptr,TEXT("orientation device exists: ")+Id.ToString());
        if(!Found) return;
        Aim(Found->GetActorLocation());
        Check(Player->TraceInteract()==Found,TEXT("real aim trace reaches: ")+Id.ToString());
        Tap(EKeys::E);
    };
    auto Shot=[this](const FString& Name)
    {
        if(AuditShotPhase!=OrientationAuditStep)
        {
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("Evidence");
            IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/Name+TEXT(".png"),false,false);
            AuditShotPhase=OrientationAuditStep; AuditShotTime=AuditClock;
            return false;
        }
        return AuditClock-AuditShotTime>0.65f;
    };

    if(bOrientationReloadPending)
    {
        // The new movement component must get its first floor query before
        // checking walking; BeginPlay alone can still report initial falling.
        if(AuditClock<0.5f) return;
        AuditPasses=OrientationSavedPasses; AuditFailures=OrientationSavedFailures;
        OrientationAuditDuration=SavedDuration; OrientationMotionSamples=SavedSamples;
        OrientationMovingSamples=SavedMoving; OrientationMaxMove=SavedMaxMove;
        Check(bTitle && !bOrientationComplete && !bArrivalChecked && !bOrientationBlackout && !bOrientationBeam && !bOrientationCrouch && !bOrientationSprint,TEXT("actual retry resets every orientation lesson and restores title"));
        Check(Player->GetActorLocation().X<-4500 && Player->GetCharacterMovement()->MovementMode==MOVE_Walking,TEXT("actual retry respawns a walking player inside arrival chamber"));
        bool DoorsReset=OrientationDoors.Num()==3;
        for(AFacilityDevice* D:OrientationDoors) DoorsReset &= !D->bOpen && D->Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
        Check(DoorsReset && TrainingLight && !TrainingLight->bBroken && Circuits[6] && Circuits[7] && Circuits[8],TEXT("retry restores training light, room circuits and all three gates"));
        bOrientationReloadPending=false;
        WriteAudit(); OrientationAuditStep=99; AuditClock=0;
    }
    else if(OrientationAuditStep==0 && AuditClock>10)
    {
        if(!Shot(TEXT("orientation-01-arrival"))) return;
        Check(bTitle && !bOrientationComplete && Player->GetActorLocation().X<-4500,TEXT("normal starting position is inside arrival room, not main hallway"));
        Check(bHardwareReady && bDLSS && bRayReconstruction,TEXT("orientation uses real hardware RT, DLSS and Ray Reconstruction"));
        Check(PC->PlayerInput && PC->PlayerInput->DebugExecBindings.IsEmpty(),TEXT("game controls have no inherited wireframe or lighting debug shortcuts"));
        Check(!UseOrientationPanel(TEXT("OrientationExit")) && !OrientationDoors[2]->bOpen,TEXT("main hallway cannot open before orientation requirements"));
        Tap(EKeys::Enter); Next();
    }
    else if(OrientationAuditStep==1 && AuditClock>0.5f)
    {
        Check(!bTitle && Player->CanAct(),TEXT("Enter starts the arrival-room orientation through the real binding"));
        const FVector Before=Warden->GetActorLocation();
        const float PreviousGrace=GracePeriod;
        GracePeriod=0; bAuditFreezeAI=false; Warden->Path={FVector(3350,-1000,110)};
        Warden->Tick(0.2f);
        Check(Warden->GetActorLocation().Equals(Before),TEXT("orientation suppresses the Warden even after its old grace period"));
        GracePeriod=PreviousGrace; bAuditFreezeAI=true; Warden->Path.Reset();
        Next();
    }
    else if(OrientationAuditStep==2 && Walk(FVector2D(-3300,170))) Next();
    else if(OrientationAuditStep==3 && AuditClock>0.6f) { Use(TEXT("OrientationCheckIn")); Next(); }
    else if(OrientationAuditStep==4 && AuditClock>0.3f)
    {
        if(!bArrivalChecked || !OrientationDoors[0]->bOpen) { Check(false,TEXT("E releases arrival shutter after walking to check-in")); WriteAudit(); OrientationAuditStep=99; AuditClock=0; return; }
        if(Walk(FVector2D(-3150,0))) { Check(OrientationWalkDistance>180,TEXT("arrival lesson records actual physical walking")); Next(); }
    }
    else if(OrientationAuditStep==5 && Walk(FVector2D(-2780,0))) Next();
    else if(OrientationAuditStep==6 && Walk(FVector2D(-2750,180)))
    {
        if(AuditInputPhase!=6) { Tap(EKeys::F); AuditInputPhase=6; AuditClock=0; return; }
        Aim(FVector(-2500,-220,170));
        if(AuditClock<2 || !Shot(TEXT("orientation-02-colored-light"))) return;
        Check(!OrientationDoors[0]->bOpen,TEXT("light-lab entrance seals behind player to prevent blackout light leakage"));
        Use(TEXT("TrainingBreaker")); Next();
    }
    else if(OrientationAuditStep==7 && AuditClock>0.5f)
    {
        Check(!Circuits[7],TEXT("E at lab breaker cuts its actual light circuit"));
        Check(!Player->bFlashlightOn,TEXT("F exposes the colored area lighting before cutting the circuit"));
        Aim(FVector(-2500,-220,170)); Next();
    }
    else if(OrientationAuditStep==8 && AuditClock>2.8f)
    {
        if(!Shot(TEXT("orientation-03-blackout"))) return;
        Check(bOrientationBlackout && !Player->bFlashlightOn,TEXT("F creates and registers a settled room-wide blackout"));
        Tap(EKeys::F); Next();
    }
    else if(OrientationAuditStep==9 && Walk(FVector2D(-2500,180)))
    {
        Check(bOrientationBeam && Player->bFlashlightOn,TEXT("F restores handheld light after the blackout lesson"));
        Aim(TrainingLight->GetActorLocation());
        Check(Player->TraceInteract()==TrainingLight,TEXT("low test fixture is reachable by real first-person aim"));
        Tap(EKeys::LeftMouseButton); Next();
    }
    else if(OrientationAuditStep==10 && AuditClock>1)
    {
        Aim(FVector(-2500,-220,170));
        if(!Shot(TEXT("orientation-04-beam-shadow"))) return;
        Check(TrainingLight->bBroken && OrientationDoors[1]->bOpen,TEXT("LMB destruction completes the light lesson and opens the gallery"));
        Next();
    }
    else if(OrientationAuditStep==11 && Walk(FVector2D(-2140,0))) Next();
    else if(OrientationAuditStep==12 && Walk(FVector2D(-1660,0)))
    {
        Aim(FVector(-1450,-408,190));
        if(AuditClock<3 || !Shot(TEXT("orientation-05-reflections"))) return;
        Tap(EKeys::F4); // Exercise a formerly conflicting function key as well.
        Key(EKeys::LeftControl,IE_Pressed); Next();
    }
    else if(OrientationAuditStep==13 && Walk(FVector2D(-1360,0)))
    {
        Check(Player->bIsCrouched && bOrientationCrouch,TEXT("Ctrl physically carries the crouched capsule under the low pipe"));
        Key(EKeys::LeftControl,IE_Released); Next();
    }
    else if(OrientationAuditStep==14 && AuditClock>0.4f) { Key(EKeys::LeftShift,IE_Pressed); Next(); }
    else if(OrientationAuditStep==15 && Walk(FVector2D(-1110,180)))
    {
        Key(EKeys::LeftShift,IE_Released);
        Check(bOrientationSprint && !Player->bIsCrouched,TEXT("Shift performs real sprint movement after the crouch passage"));
        Use(TEXT("OrientationExit")); Next();
    }
    else if(OrientationAuditStep==16 && AuditClock>0.5f)
    {
        Check(OrientationDoors[2]->bOpen,TEXT("exit panel opens final gate after all control lessons")); Next();
    }
    else if(OrientationAuditStep==17 && Walk(FVector2D(-1060,0))) Next();
    else if(OrientationAuditStep==18 && Walk(FVector2D(-650,0))) Next();
    else if(OrientationAuditStep==19 && AuditClock>0.4f)
    {
        Check(bOrientationComplete && Player->GetActorLocation().X>-800,TEXT("player physically reaches the original transfer hall after all three rooms"));
        OrientationAuditDuration=OrientationSeconds;
        Check(OrientationAuditDuration>=30 && OrientationAuditDuration<=60,TEXT("guided physical intro route completes within the 30-60 second pacing target"));
        Check(GracePeriod>RunTime+29 && !Circuits[6] && !Circuits[7] && !Circuits[8],TEXT("hall entry starts fresh enemy grace and switches off the completed intro"));
        Check(!OrientationDoors[2]->bOpen && OrientationDoors[2]->Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision,TEXT("completed intro seals behind the player"));
        // Teleports begin only AFTER the physical intro route has completed.
        Player->SetActorLocation(FVector(-3500,0,92));
        Warden->SetActorLocation(FVector(1000,0,110)); Warden->SetActorRotation(FRotator::ZeroRotator);
        Warden->Path={FVector(1700,0,110)}; Warden->RepathClock=-100; Warden->Suspicion=0; Warden->bHunting=false;
        NoiseTime=0; GracePeriod=0; bAuditFreezeAI=false;
        AuditStartPosition=Warden->GetActorLocation(); Next();
    }
    else if(OrientationAuditStep==20)
    {
        const float Distance=FVector::Dist2D(Warden->GetActorLocation(),AuditStartPosition);
        AuditStartPosition=Warden->GetActorLocation();
        if(AuditClock>0.15f && AuditClock<2.2f)
        {
            ++OrientationMotionSamples;
            if(Distance>0.001f) ++OrientationMovingSamples;
            OrientationMaxMove=FMath::Max(OrientationMaxMove,Distance);
        }
        if(AuditClock>2.5f)
        {
            bAuditFreezeAI=true;
            Check(OrientationMotionSamples>45 && OrientationMovingSamples>=OrientationMotionSamples*0.95f,TEXT("Warden changes position on every sampled render frame instead of 10 Hz steps"));
            Check(Warden->GetActorLocation().X>1250,TEXT("smooth patrol retains its intended world-space speed"));
            bool RateIndependent=true;
            for(int32 Hz : {30,60,120})
            {
                Warden->SetActorLocation(FVector(1000,0,110)); Warden->Path={FVector(1600,0,110)};
                for(int32 I=0;I<Hz;++I) Warden->MoveAlongPath(1.f/Hz);
                RateIndependent &= FMath::IsNearlyEqual(Warden->GetActorLocation().X,1118.f,0.3f);
            }
            Check(RateIndependent,TEXT("locomotion covers equal distance at 30, 60 and 120 Hz"));
            Warden->SetActorLocation(FVector(1000,0,110)); Warden->Path={FVector(1040,0,110),FVector(1040,200,110)};
            Warden->MoveAlongPath(1);
            Check(Warden->GetActorLocation().Equals(FVector(1040,78,110),0.3f),TEXT("movement budget carries across waypoints without stop-start pauses"));
            Warden->SetActorLocation(FVector(-730,0,110)); Warden->bHunting=true; Warden->Path={FVector(-1100,0,110)};
            Warden->MoveAlongPath(1);
            Check(Warden->GetActorLocation().X>-870 && Warden->Path.IsEmpty(),TEXT("per-frame movement still sweeps against closed-door collision"));
            Warden->bHunting=false;
            const FVector Before=Warden->GetActorLocation();
            bPaused=true; bAuditFreezeAI=false; Warden->Tick(0.2f);
            Check(Warden->GetActorLocation().Equals(Before),TEXT("pause still freezes the smoothly moving Warden"));
            bPaused=false; bAuditFreezeAI=true;
            OrientationSavedPasses=AuditPasses; OrientationSavedFailures=AuditFailures;
            SavedDuration=OrientationAuditDuration; SavedSamples=OrientationMotionSamples;
            SavedMoving=OrientationMovingSamples; SavedMaxMove=OrientationMaxMove;
            bOrientationReloadPending=true; RestartRun();
        }
    }
    else if(OrientationAuditStep==99 && AuditClock>2) FPlatformMisc::RequestExitWithStatus(false,AuditFailures.IsEmpty() ? 0 : 1);

    if(AuditClock>25 && OrientationAuditStep!=99)
    {
        Check(false,FString::Printf(TEXT("orientation step %d timed out at %s"),OrientationAuditStep,*Player->GetActorLocation().ToString()));
        Key(EKeys::W,IE_Released); Key(EKeys::LeftShift,IE_Released); Key(EKeys::LeftControl,IE_Released);
        WriteAudit(); OrientationAuditStep=99; AuditClock=0;
    }
}
