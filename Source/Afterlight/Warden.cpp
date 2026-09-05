#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"

AWarden::AWarden()
{
    PrimaryActorTick.bCanEverTick=true;
    Collision=CreateDefaultSubobject<UBoxComponent>(TEXT("WardenCollision"));
    SetRootComponent(Collision);
    Collision->SetBoxExtent(FVector(34,34,108));
    Collision->SetCollisionProfileName(TEXT("Pawn"));
}

void AWarden::BuildBody()
{
    auto B=[this](FVector P,FVector S,const FString& M) { Parts.Add(Afterlight::Part(this,RootComponent,P,S,M)); };
    B(FVector(0,0,30),FVector(59,87,86),TEXT("Teal"));
    B(FVector(5,0,95),FVector(49,58,42),TEXT("Ceramic"));
    B(FVector(31,0,94),FVector(5,45,15),TEXT("DarkMetal"));
    // The Warden has painted eyes, not uncontrollable emissive light sources.
    B(FVector(34,-12,94),FVector(3,9,7),TEXT("Amber"));
    B(FVector(34,12,94),FVector(3,9,7),TEXT("Amber"));
    for (int S : {-1,1})
    {
        B(FVector(0,S*56,15),FVector(25,23,87),TEXT("Metal"));
        B(FVector(5,S*56,-32),FVector(29,26,24),TEXT("Glove"));
        B(FVector(0,S*24,-67),FVector(26,27,80),TEXT("DarkMetal"));
        B(FVector(12,S*24,-103),FVector(46,33,17),TEXT("Metal"));
    }
    B(FVector(32,0,31),FVector(7,52,53),TEXT("DarkMetal"));
    for(int I=0;I<5;++I) B(FVector(37,0,12+I*10),FVector(4,45,3),TEXT("Metal"));
}

bool AWarden::CanSeePlayer() const
{
    if (!Game || !Game->Player) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(WardenSight),false,this);
    Params.AddIgnoredActor(Game->Player);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation()+FVector(0,0,90),Game->Player->Camera->GetComponentLocation(),ECC_Visibility,Params);
}

void AWarden::PlanPath(FVector Destination)
{
    Path.Reset();
    if (!Game || Game->NavPoints.IsEmpty()) return;
    auto Clear=[this](FVector A,FVector B)
    {
        FHitResult Hit;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(WardenRoute),false,this);
        Q.AddIgnoredActor(Game->Player);
        return !GetWorld()->SweepSingleByChannel(Hit,A,B,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeBox(FVector(32,32,75)),Q);
    };
    Destination.Z=110;
    if(Clear(GetActorLocation(),Destination)) { Path.Add(Destination); return; }
    int32 Start=INDEX_NONE,End=INDEX_NONE;
    float SD=FLT_MAX,ED=FLT_MAX;
    for(int32 I=0;I<Game->NavPoints.Num();++I)
    {
        const FVector N=Game->NavPoints[I];
        const float A=FVector::DistSquared(N,GetActorLocation());
        const float B=FVector::DistSquared(N,Destination);
        if(A<SD && Clear(GetActorLocation(),N)) { SD=A; Start=I; }
        if(B<ED && Clear(N,Destination)) { ED=B; End=I; }
    }
    if(Start==INDEX_NONE || End==INDEX_NONE) return;
    TArray<float> Cost; Cost.Init(FLT_MAX,Game->NavPoints.Num());
    TArray<int32> Prev; Prev.Init(INDEX_NONE,Game->NavPoints.Num());
    TArray<bool> Done; Done.Init(false,Game->NavPoints.Num());
    Cost[Start]=0;
    for(int32 Iter=0;Iter<Game->NavPoints.Num();++Iter)
    {
        int32 Best=INDEX_NONE;
        for(int32 I=0;I<Cost.Num();++I) if(!Done[I] && (Best==INDEX_NONE || Cost[I]<Cost[Best])) Best=I;
        if(Best==INDEX_NONE || Cost[Best]==FLT_MAX) break;
        if(Best==End) break;
        Done[Best]=true;
        for(int32 N:Game->NavLinks[Best])
        {
            if(!Clear(Game->NavPoints[Best],Game->NavPoints[N])) continue;
            const float C=Cost[Best]+FVector::Distance(Game->NavPoints[Best],Game->NavPoints[N]);
            if(C<Cost[N]) { Cost[N]=C; Prev[N]=Best; }
        }
    }
    if(Cost[End]==FLT_MAX) return;
    TArray<FVector> Reverse;
    for(int32 N=End;N!=INDEX_NONE;N=Prev[N]) Reverse.Add(Game->NavPoints[N]);
    for(int32 I=Reverse.Num()-1;I>=0;--I) Path.Add(Reverse[I]);
    Path.Add(Destination);
}

void AWarden::UpdateMind(float Dt)
{
    AAfterlightCharacter* P=Game->Player;
    const float Distance=FVector::Dist2D(GetActorLocation(),P->GetActorLocation());
    const bool bSees=Distance<2300 && CanSeePlayer();
    const bool bVisible=bSees && (P->Exposure>0.30f || Distance<185);
    Suspicion=FMath::Clamp(Suspicion+(bVisible ? Dt*(Distance<600 ? 1.1f:0.55f) : -Dt*0.27f),0.f,1.f);
    if(bVisible)
    {
        LastKnown=P->GetActorLocation(); LostClock=0;
        if(Suspicion>0.55f) bHunting=true;
    }
    else LostClock+=Dt;
    if(LostClock>7) bHunting=false;
    if(Game->NoiseTime>0 && FVector::Dist2D(GetActorLocation(),Game->NoisePosition)<Game->NoiseStrength*3200)
    {
        LastKnown=Game->NoisePosition;
        RepathClock=FMath::Max(RepathClock,1.f);
        LostClock=0;
    }
    RepathClock+=Dt;
    if(RepathClock>0.85f)
    {
        RepathClock=0;
        if(bHunting || (Game->NoiseTime>0 && LostClock<1)) PlanPath(LastKnown);
        else if(Path.IsEmpty())
        {
            static const int Patrol[]={5,1,8,2,11,5,22,6,3};
            const int N=Patrol[PatrolIndex++ % UE_ARRAY_COUNT(Patrol)] % Game->NavPoints.Num();
            PlanPath(Game->NavPoints[N]);
        }
    }
    if(Distance<90 && bSees) Game->Lose();
}

void AWarden::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!Game || !Game->Player || Game->bTitle || Game->bPaused || Game->bLost || Game->bWon || Game->bAuditFreezeAI || Game->RunTime<Game->GracePeriod) return;
    MindClock+=Dt; StepClock+=Dt; FootClock+=Dt;
    if(MindClock>=0.1f) { UpdateMind(MindClock); MindClock=0; }
    if(StepClock>=0.10f && !Path.IsEmpty())
    {
        const float Step=FMath::Min(StepClock,0.15f)*(bHunting ? 305 : 118);
        StepClock=0;
        FVector Delta=Path[0]-GetActorLocation(); Delta.Z=0;
        if(Delta.Size2D()<40) { Path.RemoveAt(0); return; }
        const FVector Dir=Delta.GetSafeNormal();
        SetActorRotation(FRotator(0,FMath::RoundToFloat(Dir.Rotation().Yaw/15)*15,0));
        FHitResult Hit;
        SetActorLocation(GetActorLocation()+Dir*Step,true,&Hit);
        if(Hit.bBlockingHit) { Path.Reset(); RepathClock=1; }
        if(FootClock>0.58f) { FootClock=0; Game->Sound(TEXT("Warden"),GetActorLocation(),bHunting ? 0.55f:0.32f); }
    }
}
