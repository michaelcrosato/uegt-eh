#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Camera/CameraComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void AFacility::BuildOrientation()
{
    auto* G=OwnerGame.Get();
    Room(FVector2D(-3900,0),FVector2D(1800,880),6,TEXT("00A / ARRIVAL"));
    Room(FVector2D(-2500,0),FVector2D(1000,880),7,TEXT("00B / LIGHT LAB"));
    Room(FVector2D(-1450,0),FVector2D(1100,880),8,TEXT("00C / SERVICE GALLERY"));
    WallX(-4800,0,880);
    WallX(-3000,0,880,0);
    WallX(-2000,0,880,0);
    for(int Side : {-1,1})
    {
        WallY(-3900,Side*440,1800);
        WallY(-2500,Side*440,1000);
        WallY(-1450,Side*440,1100);
    }
    for(float X=-4600;X<-960;X+=110)
        Box(FVector(X,-105,0.6f),FVector(62,8,1),TEXT("Amber"));
    for(int I=0;I<3;++I)
        G->OrientationDoors.Add(G->AddDevice(EDeviceKind::OrientationDoor,FName(*FString::Printf(TEXT("OrientationDoor%d"),I)),FVector(I==0 ? -3000 : I==1 ? -2000 : -900,0,146),FVector(34,224,300),6+I,180));

    // Arrival: a warm, readable space with shadow-casting lockers and equipment.
    for(int I=0;I<8;++I)
    {
        Box(FVector(-4630+I*150,-343,120),FVector(108,146,240),TEXT("Teal"));
        Box(FVector(-4630+I*150,-266,121),FVector(96,8,219),TEXT("Metal"));
        Box(FVector(-4603+I*150,-258,127),FVector(9,8,31),TEXT("Amber"));
        for(int J=0;J<4;++J) Box(FVector(-4630+I*150,-260,184+J*10),FVector(58,5,3),TEXT("Seam"));
    }
    Box(FVector(-3640,263,38),FVector(310,75,14),TEXT("Ceramic"));
    for(int Side : {-1,1}) Box(FVector(-3640+Side*115,263,16),FVector(20,65,32),TEXT("DarkMetal"));
    Box(FVector(-4430,263,38),FVector(310,75,14),TEXT("Ceramic"));
    for(int Side : {-1,1}) Box(FVector(-4430+Side*115,263,16),FVector(20,65,32),TEXT("DarkMetal"));
    G->AddDevice(EDeviceKind::OrientationPanel,TEXT("OrientationCheckIn"),FVector(-3200,396,145),FVector(18,62,80),6,-90);
    Sign(TEXT("WASD / MOVE     MOUSE / LOOK"),FVector(-3490,413,242),19,-90);
    Sign(TEXT("CHECK IN  >"),FVector(-3200,413,213),23,-90,FColor(236,183,87));
    Sign(TEXT("LIGHT LAB"),FVector(-3035,0,315),23,180);

    // Light lab: colored bounce, a block-built calibration dummy, and a grille
    // whose real geometry casts changing shadows as the handheld lamp moves.
    Box(FVector(-2530,-265,24),FVector(220,205,48),TEXT("DarkMetal"));
    Box(FVector(-2530,-265,53),FVector(232,217,10),TEXT("Metal"));
    Box(FVector(-2530,-265,142),FVector(65,95,85),TEXT("Ceramic"));
    Box(FVector(-2530,-265,205),FVector(57,64,40),TEXT("Teal"));
    for(int Side : {-1,1})
    {
        Box(FVector(-2530,-265+Side*65,135),FVector(28,27,95),TEXT("Amber"));
        Box(FVector(-2530,-265+Side*25,82),FVector(27,28,48),TEXT("Metal"));
    }
    for(int I=0;I<9;++I) Box(FVector(-2740+I*38,-103,224),FVector(9,9,130),TEXT("Metal"));
    Box(FVector(-2588,-103,292),FVector(345,16,12),TEXT("DarkMetal"));
    G->AddDevice(EDeviceKind::Breaker,TEXT("TrainingBreaker"),FVector(-2760,396,145),FVector(14,42,58),7,-90);
    G->TrainingLight=G->AddLight(FVector(-2220,180,270),FLinearColor(1,0.40f,0.12f),1800,7);
    Sign(TEXT("E / CUT LIGHTS"),FVector(-2760,413,221),19,-90,FColor(236,183,87));
    Sign(TEXT("LMB / BREAK TEST FIXTURE"),FVector(-2036,242,218),16,180);
    Sign(TEXT("NO LIGHT MEANS NO LIGHT"),FVector(-2500,-413,267),18,90);
    Sign(TEXT("SERVICE GALLERY"),FVector(-2035,0,315),20,180);

    // Gallery: a long mirror and wet floor reveal view-dependent ray-traced
    // reflections. The full-width low pipe is a real crouch-only obstruction.
    Box(FVector(-1500,-412,186),FVector(730,8,210),TEXT("Mirror"));
    for(int Z : {76,296}) Box(FVector(-1500,-404,Z),FVector(755,16,12),TEXT("Metal"));
    for(int X : {-1871,-1129}) Box(FVector(X,-404,186),FVector(12,16,232),TEXT("Metal"));
    Box(FVector(-1430,110,0.5f),FVector(890,290,1),TEXT("FloorWet"));
    Cylinder(FVector(-1470,0,162),FVector(50,50,880),TEXT("Amber"),FRotator(0,0,90));
    for(int Side : {-1,1}) Box(FVector(-1470,Side*414,80),FVector(70,36,160),TEXT("DarkMetal"));
    Sign(TEXT("CTRL / CROUCH UNDER PIPE"),FVector(-1600,413,219),19,-90,FColor(236,183,87));
    Sign(TEXT("SHIFT / RUN TO THE EXIT PANEL"),FVector(-1160,413,239),15,-90);
    G->AddDevice(EDeviceKind::OrientationPanel,TEXT("OrientationExit"),FVector(-1070,396,145),FVector(18,62,80),8,-90);
    G->AddLight(FVector(-1160,230,307),FLinearColor(1,0.45f,0.13f),1400,8);
    Sign(TEXT("09 / TRANSFER HALL"),FVector(-936,0,315),20,180);
    for(AFacilityLight* L:G->Lights)
    {
        if(L->Circuit==6) { L->LampColor=FLinearColor(1,0.72f,0.38f); L->RatedLumens=1400; }
        if(L->Circuit==7 && L!=G->TrainingLight) { L->LampColor=L->GetActorLocation().X<-2500 ? FLinearColor(0.28f,0.65f,1) : FLinearColor(1,0.48f,0.15f); L->RatedLumens=900; }
        if(L->Circuit==8 && L->GetActorLocation().Z>320) { L->LampColor=FLinearColor(0.30f,0.77f,1); L->RatedLumens=1500; }
        if(L->Circuit>=6) { L->Light->SetLightColor(L->LampColor); L->ApplyState(); }
    }
}

bool AAfterlightGameMode::UseOrientationPanel(FName Id)
{
    if(bOrientationComplete) return false;
    if(Id==TEXT("OrientationCheckIn"))
    {
        if(bArrivalChecked) return false;
        if(OrientationWalkDistance<180) { Notify(TEXT("WASD / Walk to the check-in panel, then aim at it and press E."),4); return false; }
        bArrivalChecked=true;
        OrientationDoors[0]->Open();
        Notify(TEXT("CHECK-IN COMPLETE / Enter the light lab. The amber wall breaker controls its lights."),6);
        return true;
    }
    if(Id==TEXT("OrientationExit"))
    {
        if(!OrientationDoors[1]->bOpen || !bOrientationCrouch || !bOrientationSprint) { Notify(OrientationHint(),5); return false; }
        OrientationDoors[2]->Open();
        Notify(TEXT("CHECK COMPLETE / Enter the transfer hall. The Warden follows light and noise."),6);
        return true;
    }
    return false;
}

void AAfterlightGameMode::TickOrientation(float Dt)
{
    if(bOrientationComplete || !Player) return;
    OrientationSeconds+=Dt;
    const FVector P=Player->GetActorLocation();
    // Ignore teleports; only actual walking contributes to the arrival lesson.
    const float Travel=FVector::Dist2D(P,OrientationPreviousPosition);
    if(Travel<500.f*Dt+2) OrientationWalkDistance+=Travel;
    OrientationPreviousPosition=P;
    if(bArrivalChecked && P.X>-2860 && OrientationDoors[0]->bOpen) { OrientationDoors[0]->Close(); SetCircuit(6,false); }
    if(P.X>-2950 && P.X<-2000 && bArrivalChecked)
    {
        bool bDark=!Circuits[7] && !Player->bFlashlightOn;
        for(AFacilityLight* L:Lights) if(L->Circuit==7 && L->IsLit()) bDark=false;
        if(bDark) OrientationBlackoutTime+=Dt;
        else if(!bOrientationBlackout) OrientationBlackoutTime=0;
        if(!bOrientationBlackout && OrientationBlackoutTime>=1.8f)
        {
            bOrientationBlackout=true;
            Notify(TEXT("TOTAL BLACKOUT / F turns your lamp back on. Sweep its beam across the metal grille."),6);
        }
        if(bOrientationBlackout && Player->bFlashlightOn) bOrientationBeam=true;
        if(bOrientationBeam && TrainingLight && TrainingLight->bBroken && !OrientationDoors[1]->bOpen)
        {
            OrientationDoors[1]->Open();
            Notify(TEXT("LIGHT CHECK COMPLETE / Broken lights stay dark. Continue to the service gallery."),6);
            Sound(TEXT("Pickup"),P,0.4f,false);
        }
    }
    if(P.X>-1430 && P.X<-930 && OrientationDoors[1]->bOpen)
    {
        if(Player->bIsCrouched) bOrientationCrouch=true;
        if(bOrientationCrouch && Player->bSprinting && !Player->bIsCrouched && Player->GetVelocity().Size2D()>280) bOrientationSprint=true;
    }
    if(OrientationDoors[2]->bOpen && P.X>-800)
    {
        bOrientationComplete=true;
        OrientationDoors[2]->Close();
        for(int32 C=6;C<NumCircuits;++C) SetCircuit(C,false);
        GracePeriod=RunTime+32;
        NoiseTime=0;
        Notify(TEXT("ORIENTATION COMPLETE / Find the PLANT ACCESS CARD in Records and the CERAMIC FUSE in Workshop."),8);
        UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT orientation complete: %.2f seconds; Warden grace starts now"),OrientationSeconds);
    }
}

FString AAfterlightGameMode::OrientationHint() const
{
    if(!bArrivalChecked) return TEXT("WASD + mouse / Reach the amber CHECK IN panel, then E");
    if(!bOrientationBlackout)
    {
        if(Circuits[7] && Player && Player->bFlashlightOn) return TEXT("F / Lamp OFF to see colored room lighting; then E at the breaker");
        if(Circuits[7]) return TEXT("E / Cut the light lab circuit at its amber wall breaker");
        if(Player && Player->bFlashlightOn) return TEXT("F / Switch your handheld lamp OFF to see total darkness");
        return TEXT("Let the room go dark / then F to relight your handheld lamp");
    }
    if(!bOrientationBeam) return TEXT("F / Lamp ON. Move the beam across the grille's shadows");
    if(TrainingLight && !TrainingLight->bBroken) return TEXT("Aim at the low test fixture beside the exit / LMB to break");
    if(!bOrientationCrouch) return TEXT("CTRL / Crouch and pass under the amber pipe");
    if(!bOrientationSprint) return TEXT("Release CTRL / Hold SHIFT + WASD to run toward the exit");
    if(!OrientationDoors[2]->bOpen) return TEXT("E / Use the amber exit panel to release the transfer hall");
    return TEXT("Walk through the open shutter / enter the transfer hall");
}

FString AAfterlightGameMode::OrientationFeature() const
{
    const float X=Player ? Player->GetActorLocation().X : -3810;
    if(X<-3000) return TEXT("AREA LIGHTS / soft shadows from every piece of geometry");
    if(X<-2000) return TEXT("LIGHT IS PHYSICAL / colored bounce, moving shadows, total blackout");
    return TEXT("RAY-TRACED REFLECTIONS / watch the mirror and wet floor as you move");
}
