#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/WorldSettings.h"

AFacility::AFacility()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FacilityRoot")));
    PrimaryActorTick.bCanEverTick = false;
}

void AFacility::Box(FVector P, FVector S, const FString& Mat, FRotator R)
{
    const FString Key = Mat+TEXT("_Box");
    auto& Batch = Batches.FindOrAdd(Key);
    if (!Batch)
    {
        Batch = NewObject<UInstancedStaticMeshComponent>(this);
        Batch->SetupAttachment(RootComponent);
        Batch->SetStaticMesh(Afterlight::Shape());
        Batch->SetMaterial(0,Afterlight::Material(Mat));
        Batch->SetMobility(EComponentMobility::Movable);
        Batch->SetCollisionProfileName(TEXT("BlockAll"));
        Afterlight::Shadow(Batch);
        Batch->RegisterComponent();
    }
    Batch->AddInstance(FTransform(R,P,S/100.f),true);
}

void AFacility::Cylinder(FVector P, FVector S, const FString& Mat, FRotator R)
{
    const FString Key = Mat+TEXT("_Cylinder");
    auto& Batch = Batches.FindOrAdd(Key);
    if (!Batch)
    {
        Batch = NewObject<UInstancedStaticMeshComponent>(this);
        Batch->SetupAttachment(RootComponent);
        Batch->SetStaticMesh(Afterlight::Shape(true));
        Batch->SetMaterial(0,Afterlight::Material(Mat));
        Batch->SetMobility(EComponentMobility::Movable);
        Batch->SetCollisionProfileName(TEXT("BlockAll"));
        Afterlight::Shadow(Batch);
        Batch->RegisterComponent();
    }
    Batch->AddInstance(FTransform(R,P,S/100.f),true);
}

void AFacility::Sign(const FString& T, FVector P, float S, float Yaw, FColor Color)
{
    auto* Text = NewObject<UTextRenderComponent>(this);
    Text->SetupAttachment(RootComponent);
    Text->SetWorldLocationAndRotation(P,FRotator(0,Yaw,0));
    Text->SetWorldSize(S);
    Text->SetText(FText::FromString(T));
    Text->SetTextRenderColor(Color);
    Text->SetTextMaterial(Afterlight::Material(TEXT("Text")));
    Text->SetHorizontalAlignment(EHTA_Center);
    Text->SetVerticalAlignment(EVRTA_TextCenter);
    Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Afterlight::Shadow(Text);
    Text->RegisterComponent();
}

void AFacility::WallY(float X, float Y, float Length, float DoorX)
{
    auto Panel = [this,Y](float Center,float W)
    {
        if (W<=0) return;
        Box(FVector(Center,Y,180),FVector(W,28,360),TEXT("Wall"));
        for (int Side : {-1,1})
        {
            Box(FVector(Center,Y+Side*17,74),FVector(W,7,104),TEXT("Teal"));
            Box(FVector(Center,Y+Side*22,132),FVector(W,4,5),TEXT("Amber"));
            Box(FVector(Center,Y+Side*20,18),FVector(W,8,28),TEXT("DarkMetal"));
            for (float A=Center-W/2+80;A<Center+W/2;A+=180)
            {
                Box(FVector(A,Y+Side*17,235),FVector(3,5,170),TEXT("Seam"));
                for (int Z : {160,310}) Box(FVector(A-9,Y+Side*20,Z),FVector(4,4,4),TEXT("Metal"));
            }
        }
    };
    if (DoorX > X-Length/2 && DoorX < X+Length/2)
    {
        const float L = DoorX-110 - (X-Length/2);
        const float R = X+Length/2-(DoorX+110);
        Panel(X-Length/2+L/2,L);
        Panel(DoorX+110+R/2,R);
        Box(FVector(DoorX,Y,327),FVector(220,36,66),TEXT("DarkMetal"));
        for (int Side : {-1,1}) Box(FVector(DoorX+Side*113,Y,146),FVector(18,50,292),TEXT("Metal"));
    }
    else Panel(X,Length);
}

void AFacility::WallX(float X, float Y, float Length, float DoorY)
{
    auto Panel = [this,X](float Center,float W)
    {
        if (W<=0) return;
        Box(FVector(X,Center,180),FVector(28,W,360),TEXT("Wall"));
        for (int Side : {-1,1})
        {
            Box(FVector(X+Side*17,Center,74),FVector(7,W,104),TEXT("Teal"));
            Box(FVector(X+Side*22,Center,132),FVector(4,W,5),TEXT("Amber"));
            Box(FVector(X+Side*20,Center,18),FVector(8,W,28),TEXT("DarkMetal"));
            for (float A=Center-W/2+80;A<Center+W/2;A+=180) Box(FVector(X+Side*17,A,235),FVector(5,3,170),TEXT("Seam"));
        }
    };
    if (DoorY > Y-Length/2 && DoorY < Y+Length/2)
    {
        const float L = DoorY-110-(Y-Length/2);
        const float R = Y+Length/2-(DoorY+110);
        Panel(Y-Length/2+L/2,L);
        Panel(DoorY+110+R/2,R);
        Box(FVector(X,DoorY,327),FVector(36,220,66),TEXT("DarkMetal"));
        for (int Side : {-1,1}) Box(FVector(X,DoorY+Side*113,146),FVector(50,18,292),TEXT("Metal"));
    }
    else Panel(Y,Length);
}

void AFacility::Room(FVector2D C, FVector2D Size, int32 Circuit, const FString& Name)
{
    Box(FVector(C,-22),FVector(Size.X,Size.Y,38),TEXT("Concrete"));
    Box(FVector(C,381),FVector(Size.X,Size.Y,42),TEXT("DarkMetal"));
    for (float X = C.X-Size.X/2+100; X < C.X+Size.X/2; X+=200)
        for (float Y = C.Y-Size.Y/2+100; Y < C.Y+Size.Y/2; Y+=200)
        {
            const float W=FMath::Min(198.f,C.X+Size.X/2-(X-100)-2);
            const float H=FMath::Min(198.f,C.Y+Size.Y/2-(Y-100)-2);
            Box(FVector(X-100+W/2,Y-100+H/2,-2),FVector(W,H,4), ((int32(X+Y)/200)%5==0) ? TEXT("FloorWet") : TEXT("Floor"));
        }
    for (float X=C.X-Size.X/2+130;X<C.X+Size.X/2;X+=360)
    {
        Box(FVector(X,C.Y,351),FVector(18,Size.Y,18),TEXT("Metal"));
        for (int Side : {-1,1}) Box(FVector(X,C.Y+Side*(Size.Y/2-26),180),FVector(24,24,360),TEXT("DarkMetal"));
    }
    const FLinearColor Color = Circuit == 2 ? FLinearColor(1,0.56f,0.21f) : Circuit == 3 ? FLinearColor(0.38f,0.8f,1) : FLinearColor(0.74f,0.91f,0.87f);
    for (int I=0;I<3;++I) OwnerGame->AddLight(FVector(C.X+(I-1)*Size.X*0.27f,C.Y,336),Color,2000,Circuit);
    Sign(Name,FVector(C.X,C.Y+Size.Y/2-24,258),40,-90);
    // Surface-mounted pipes remain full geometry in the RT scene.
    for (int I=0;I<3;++I) Cylinder(FVector(C.X,C.Y-Size.Y/2+52+I*24,320),FVector(14,14,Size.X-50),I==0 ? TEXT("Amber") : TEXT("Metal"),FRotator(90,0,0));
}

void AFacility::Shelf(FVector P, float Yaw, int32 Seed)
{
    const FRotator R(0,Yaw,0);
    auto B=[&](FVector V,FVector S,const FString& M) { Box(P+R.RotateVector(V),S,M,R); };
    for (int X : {-1,1}) for (int Y : {-1,1}) B(FVector(X*83,Y*26,105),FVector(7,7,210),TEXT("Metal"));
    FRandomStream Rand(Seed);
    for (int Z : {18,83,150,208})
    {
        B(FVector(0,0,Z),FVector(180,65,6),TEXT("Metal"));
        if (Z>180) continue;
        for (int J=0;J<3;++J)
        {
            const float H=Rand.FRandRange(28,51);
            B(FVector(-57+J*56,Rand.FRandRange(-5,5),Z+3+H/2),FVector(44,45,H),J%2==0 ? TEXT("Cardboard") : TEXT("Teal"));
            B(FVector(-57+J*56,-23,Z+H*0.6f),FVector(24,1,7),TEXT("Ceramic"));
        }
    }
}

void AFacility::Desk(FVector P,float Yaw)
{
    FRotator R(0,Yaw,0);
    auto B=[&](FVector V,FVector S,const FString& M) { Box(P+R.RotateVector(V),S,M,R); };
    B(FVector(0,0,88),FVector(190,85,9),TEXT("Ceramic"));
    for (int X : {-1,1}) B(FVector(X*76,0,43),FVector(16,70,86),TEXT("Metal"));
    B(FVector(-50,19,116),FVector(64,12,48),TEXT("DarkMetal"));
    B(FVector(-50,12,116),FVector(56,2,38),TEXT("Screen"));
    B(FVector(-50,-19,97),FVector(66,24,5),TEXT("DarkMetal"));
    for (int I=0;I<8;++I) B(FVector(-76+I*7,-20,101),FVector(4,16,2),TEXT("Ceramic"));
    B(FVector(12,130,50),FVector(60,55,10),TEXT("Teal"));
    B(FVector(12,155,91),FVector(60,9,65),TEXT("Teal"));
    for (int X : {-1,1}) for(int Y : {-1,1}) B(FVector(12+X*23,130+Y*20,24),FVector(5,5,48),TEXT("Metal"));
}

void AFacility::Build(AAfterlightGameMode* G)
{
    OwnerGame = G;
    GetWorld()->GetWorldSettings()->bForceNoPrecomputedLighting = true;
    Room(FVector2D(1700,0),FVector2D(5200,520),0,TEXT(""));
    // The central hall has alternating pools of warm and cold light.
    for (AFacilityLight* L : G->Lights) if (L->Circuit==0) { L->RatedLumens=900; L->ApplyState(); }
    for (int I=0;I<9;++I)
    {
        const float X=-550+I*550;
        G->AddLight(FVector(X,I%2 ? -165:165,325),I%3==0 ? FLinearColor(1,0.45f,0.14f) : FLinearColor(0.35f,0.78f,0.86f),1600,0);
        Box(FVector(X,0,305),FVector(24,498,22),TEXT("DarkMetal"));
        for (int Y : {-1,1})
        {
            Box(FVector(X,Y*224,150),FVector(24,24,300),TEXT("Metal"));
            Box(FVector(X,Y*221,102),FVector(26,26,25),TEXT("Amber"));
        }
        // Grate slats create legible moving shadows from the handheld light.
        for (int J=0;J<7;++J) Box(FVector(X-70+J*22,125,309),FVector(5,180,9),TEXT("Metal"));
    }
    WallX(-900,0,520);
    WallY(-600,260,600);
    WallY(450,260,1500,450);
    WallY(1700,260,1000);
    WallY(2900,260,1400,2900);
    WallY(3950,260,700);
    WallY(-100,-260,1600);
    WallY(1400,-260,1400,1400);
    WallY(2350,-260,500);
    WallY(3350,-260,1500,3350);
    WallY(4200,-260,200);

    for (float X=-650;X<4150;X+=110)
    {
        Box(FVector(X,-150,0.6f),FVector(65,9,1),TEXT("Amber"));
        Box(FVector(X,185,0.6f),FVector(80,4,1),TEXT("Ceramic"));
    }
    Sign(TEXT("SUBLEVEL"),FVector(-867,0,259),24,0);
    Sign(TEXT("09"),FVector(-866,0,187),112,0,FColor(222,159,66));
    Sign(TEXT("NO DAYLIGHT BELOW THIS POINT"),FVector(-865,0,89),12,0);
    Sign(TEXT("RECORDS  /  01"),FVector(450,222,327),24,-90);
    Sign(TEXT("WORKSHOP  /  02"),FVector(1400,-222,327),24,90);
    Sign(TEXT("PLANT  /  03"),FVector(2900,222,327),24,-90);
    Sign(TEXT("OBSERVATION  /  04"),FVector(3350,-222,327),20,90);
    Sign(TEXT("SURFACE LIFT  >"),FVector(2250,222,213),25,-90,FColor(221,165,77));
    Sign(TEXT("THE LIGHT REMEMBERS YOU"),FVector(1900,222,205),16,-90,FColor(106,125,123));
    G->AddDevice(EDeviceKind::Note,TEXT("ShiftReport"),FVector(-400,213,155),FVector(7,60,80),0,-90);
    G->AddDevice(EDeviceKind::Breaker,TEXT("HallBreaker"),FVector(-80,-222,145),FVector(14,30,46),0,90);

    Room(FVector2D(450,930),FVector2D(1500,1340),1,TEXT("01  /  RECORDS"));
    WallX(-300,930,1340); WallX(1200,930,1340); WallY(450,1600,1500);
    Desk(FVector(790,1240,0),180);
    Shelf(FVector(-210,770,0),90,17);
    Shelf(FVector(-210,1130,0),90,31);
    Shelf(FVector(1100,720,0),90,43);
    for (int I=0;I<5;++I)
    {
        Box(FVector(30+I*112,1510,120),FVector(99,130,240),TEXT("Teal"));
        for (int J=0;J<5;++J)
        {
            Box(FVector(30+I*112,1442,25+J*45),FVector(89,4,39),TEXT("Metal"));
            Box(FVector(30+I*112,1438,35+J*45),FVector(28,5,4),TEXT("Ceramic"));
        }
    }
    G->AddDevice(EDeviceKind::Card,TEXT("PlantCard"),FVector(750,1240,110),FVector(4,16,23),1,-90);
    G->AddDevice(EDeviceKind::Breaker,TEXT("RecordsBreaker"),FVector(590,298,145),FVector(14,30,46),1,90);
    Sign(TEXT("NIGHT SUPERVISOR"),FVector(796,1577,204),22,-90);

    Room(FVector2D(1400,-1000),FVector2D(1400,1480),2,TEXT("02  /  WORKSHOP"));
    WallX(700,-1000,1480); WallX(2100,-1000,1480); WallY(1400,-1740,1400);
    Desk(FVector(1750,-1490,0),0);
    Shelf(FVector(800,-1050,0),90,8);
    Shelf(FVector(2000,-850,0),90,22);
    Box(FVector(1350,-1350,48),FVector(140,100,96),TEXT("DarkMetal"));
    Box(FVector(1350,-1350,100),FVector(155,112,12),TEXT("Metal"));
    Cylinder(FVector(1350,-1350,127),FVector(53,53,40),TEXT("Teal"));
    G->AddDevice(EDeviceKind::Fuse,TEXT("CeramicFuse"),FVector(1810,-1460,107),FVector(25,12,20),2,90);
    G->AddDevice(EDeviceKind::Breaker,TEXT("WorkshopBreaker"),FVector(1540,-298,145),FVector(14,30,46),2,-90);
    Sign(TEXT("SPARE FUSE / 63A"),FVector(1790,-1717,194),24,90,FColor(236,190,100));
    for(int I=0;I<6;++I) Box(FVector(2038,-1320+I*55,199),FVector(12,12,75-I*6),TEXT("Metal"));

    Room(FVector2D(2900,1060),FVector2D(1400,1600),3,TEXT("03  /  AUXILIARY PLANT"));
    WallX(2200,1060,1600); WallY(2900,1860,1400); WallX(3600,1060,1600,1120);
    G->AddDevice(EDeviceKind::SecurityDoor,TEXT("PlantDoor"),FVector(2900,260,138),FVector(20,194,275),3,-90);
    for (int I=0;I<3;++I)
    {
        const float X=2440+I*325;
        Box(FVector(X,1640,102),FVector(260,270,204),TEXT("DarkMetal"));
        Box(FVector(X,1499,103),FVector(230,16,150),TEXT("Metal"));
        for (int J=0;J<7;++J) Box(FVector(X,1488,48+J*19),FVector(190,9,6),TEXT("Seam"));
        Cylinder(FVector(X,1650,270),FVector(80,80,130),TEXT("Metal"));
        Box(FVector(X,1475,225),FVector(180,12,12),TEXT("Amber"));
    }
    G->AddDevice(EDeviceKind::Generator,TEXT("Generator"),FVector(2430,1340,145),FVector(30,84,95),3,-90);
    G->AddDevice(EDeviceKind::Breaker,TEXT("PlantBreaker"),FVector(3040,298,145),FVector(14,30,46),3,90);
    Sign(TEXT("FUSE INPUT"),FVector(2390,1837,292),23,-90,FColor(232,180,85));
    Sign(TEXT("PUMP ROOM  >"),FVector(3565,1120,315),19,180);

    Room(FVector2D(4300,1240),FVector2D(1400,1240),5,TEXT("05  /  PRESSURE CONTROL"));
    WallY(4300,620,1400); WallY(4300,1860,1400); WallX(5000,1240,1240);
    for (int I=0;I<3;++I)
    {
        Cylinder(FVector(3930+I*380,1660,140),FVector(205,205,280),TEXT("Teal"));
        for(int Z : {45,240}) Cylinder(FVector(3930+I*380,1660,Z),FVector(221,221,14),TEXT("Metal"));
        Cylinder(FVector(3930+I*380,1660,316),FVector(40,40,74),TEXT("Metal"));
    }
    G->AddDevice(EDeviceKind::Valve,TEXT("PressureValve"),FVector(4820,1010,145),FVector(35,85,90),5,180);
    G->AddDevice(EDeviceKind::Breaker,TEXT("PumpBreaker"),FVector(3650,975,145),FVector(14,30,46),5,0);
    Sign(TEXT("VENT BEFORE EVACUATION"),FVector(4975,1110,260),20,180,FColor(228,173,80));

    Room(FVector2D(3350,-1050),FVector2D(1500,1580),4,TEXT("04  /  OBSERVATION"));
    WallX(2600,-1050,1580); WallX(4100,-1050,1580); WallY(3350,-1840,1500);
    Box(FVector(4069,-1050,175),FVector(8,870,240),TEXT("Mirror"));
    for (int Z : {49,301}) Box(FVector(4062,-1050,Z),FVector(18,910,12),TEXT("Metal"));
    for (int Y : {-1495,-605}) Box(FVector(4062,Y,175),FVector(18,12,264),TEXT("Metal"));
    Desk(FVector(2840,-1510,0),0);
    for(int I=0;I<4;++I)
    {
        Box(FVector(2710,-500-I*190,135),FVector(130,140,270),TEXT("DarkMetal"));
        for(int J=0;J<9;++J) Box(FVector(2780,-500-I*190,30+J*27),FVector(10,115,7),TEXT("Metal"));
    }
    G->AddDevice(EDeviceKind::Breaker,TEXT("ObservationBreaker"),FVector(3490,-298,145),FVector(14,30,46),4,-90);
    Sign(TEXT("DO NOT LEAVE IT IN THE LIGHT"),FVector(3350,-1817,246),20,90,FColor(170,117,75));

    Room(FVector2D(4700,0),FVector2D(800,520),0,TEXT(""));
    WallY(4700,260,800); WallY(4700,-260,800); WallX(5100,0,520);
    Box(FVector(4300,0,325),FVector(40,520,70),TEXT("DarkMetal"));
    G->AddDevice(EDeviceKind::Lift,TEXT("SurfaceLift"),FVector(4300,0,140),FVector(20,468,280),0,180);
    G->AddDevice(EDeviceKind::Evacuation,TEXT("LiftConsole"),FVector(4140,-214,145),FVector(20,62,88),0,90);
    Sign(TEXT("SURFACE"),FVector(5076,0,255),38,180,FColor(231,180,94));
    Sign(TEXT("ONE WAY OUT"),FVector(5075,0,180),19,180);

    // Navigation is an explicit small graph through actual door openings.
    const int H0=G->AddNav(FVector(-550,0,110));
    const int H1=G->AddNav(FVector(450,0,110));
    const int H2=G->AddNav(FVector(1400,0,110));
    const int H3=G->AddNav(FVector(2200,0,110));
    const int H4=G->AddNav(FVector(2900,0,110));
    const int H5=G->AddNav(FVector(3350,0,110));
    const int H6=G->AddNav(FVector(4020,0,110));
    G->LinkNav(H0,H1); G->LinkNav(H1,H2); G->LinkNav(H2,H3); G->LinkNav(H3,H4); G->LinkNav(H4,H5); G->LinkNav(H5,H6);
    auto Branch=[G](int From,TArray<FVector> Points)
    {
        for (const FVector& P:Points) { int N=G->AddNav(P); G->LinkNav(From,N); From=N; }
        return From;
    };
    Branch(H1,{FVector(450,440,110),FVector(450,1030,110),FVector(790,1030,110)});
    Branch(H2,{FVector(1400,-440,110),FVector(1400,-1040,110),FVector(1770,-1220,110)});
    int Plant=Branch(H4,{FVector(2900,480,110),FVector(2900,1120,110)});
    Branch(Plant,{FVector(2480,1120,110)});
    Branch(Plant,{FVector(3800,1120,110),FVector(4450,1120,110),FVector(4780,1250,110)});
    Branch(H5,{FVector(3350,-440,110),FVector(3350,-1110,110),FVector(3350,-1530,110)});

    auto* Post=GetWorld()->SpawnActor<APostProcessVolume>();
    Post->bUnbound=true;
    auto& P=Post->Settings;
    P.bOverride_AutoExposureMethod=true; P.AutoExposureMethod=AEM_Manual;
    P.bOverride_AutoExposureBias=true; P.AutoExposureBias=-3.8f;
    P.bOverride_AutoExposureApplyPhysicalCameraExposure=true; P.AutoExposureApplyPhysicalCameraExposure=false;
    P.bOverride_BloomIntensity=true; P.BloomIntensity=0.38f;
    P.bOverride_VignetteIntensity=true; P.VignetteIntensity=0.27f;
    P.bOverride_MotionBlurAmount=true; P.MotionBlurAmount=0;
    P.bOverride_SceneFringeIntensity=true; P.SceneFringeIntensity=0;
    P.bOverride_FilmGrainIntensity=true; P.FilmGrainIntensity=0.04f;
    P.bOverride_LumenSceneLightingQuality=true; P.LumenSceneLightingQuality=2;
    P.bOverride_LumenFinalGatherQuality=true; P.LumenFinalGatherQuality=2;
    P.bOverride_LumenReflectionQuality=true; P.LumenReflectionQuality=2;
    P.bOverride_LumenSceneLightingUpdateSpeed=true; P.LumenSceneLightingUpdateSpeed=4;
    P.bOverride_LumenFinalGatherLightingUpdateSpeed=true; P.LumenFinalGatherLightingUpdateSpeed=4;

    auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>();
    Fog->SetActorLocation(FVector(0,0,-100));
    auto* F=Fog->GetComponent();
    F->SetFogDensity(0.025f);
    F->SetFogHeightFalloff(0.12f);
    F->SetFogInscatteringColor(FLinearColor::Black);
    F->SetVolumetricFog(true);
    F->SetVolumetricFogAlbedo(FColor(190,206,210));
    F->SetVolumetricFogEmissive(FLinearColor::Black);
    F->SetVolumetricFogExtinctionScale(0.5f);
    F->SetVolumetricFogScatteringDistribution(0.3f);
    F->SetVolumetricFogDistance(6500);
    UE_LOG(LogTemp,Display,TEXT("AFTERLIGHT facility: %d geometry batches, %d lights, %d devices, %d navigation nodes"),Batches.Num(),G->Lights.Num(),G->Devices.Num(),G->NavPoints.Num());
}
