#include "AfterlightGame.h"
#include "AfterlightUtil.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

void AAfterlightHUD::Text(const FString& V,float X,float Y,float Size,FLinearColor C,bool bShadow)
{
    // Canvas still requires a UFont to select the runtime cache, even with SlateFontInfo.
    if(!HUDFont) HUDFont=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    FCanvasTextItem Item(FVector2D(X*Scale,Y*Scale),FText::FromString(V),HUDFont,C);
    Item.SlateFontInfo=FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(Size*Scale*0.75f));
    if(bShadow) Item.EnableShadow(FLinearColor(0,0,0,0.8f),FVector2D(1,1));
    Canvas->DrawItem(Item);
}
void AAfterlightHUD::Rect(float X,float Y,float W,float H,FLinearColor C) { DrawRect(C,X*Scale,Y*Scale,W*Scale,H*Scale); }
void AAfterlightHUD::Rule(float X,float Y,float W,FLinearColor C) { Rect(X,Y,W,1,C); }

void AAfterlightHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* G=Cast<AAfterlightGameMode>(UGameplayStatics::GetGameMode(this));
    if(!G || !Canvas) return;
    Scale=Canvas->ClipY/1080.f;
    const float W=Canvas->ClipX/Scale;
    const FLinearColor White(0.86f,0.90f,0.87f),Muted(0.43f,0.54f,0.53f),Amber(0.98f,0.60f,0.22f),Red(0.94f,0.25f,0.18f);
    if(G->bPhoto) return;
    if(G->bTitle || G->bPaused || G->bLost || G->bWon || !G->bHardwareReady)
    {
        for(int I=0;I<48;++I) Rect(I*W/48,0,W/48,1080,FLinearColor(0.005f,0.013f,0.014f,FMath::Lerp(0.97f,0.1f,float(I)/47)));
        Rect(74,72,9,30,Amber);
        Text(TEXT("NIGHT SHIFT  /  SUBLEVEL 09"),104,73,24,White);
        Text(TEXT("A F T E R L I G H T"),92,212,93,White);
        Text(TEXT("EVERY LIGHT HAS A PRICE."),98,328,23,Amber);
        Rule(98,381,630,Muted);
        if(!G->bHardwareReady)
        {
            Text(TEXT("HARDWARE RAY TRACING REQUIRED"),98,437,29,Red);
            Text(TEXT("Launch with DirectX 12 on a ray-tracing GPU."),98,494,23,White);
            Text(TEXT("Update your graphics driver and enable Shader Model 6."),98,531,22,White);
            Text(TEXT("Q   QUIT"),98,659,24,Amber);
        }
        else if(G->bWon || G->bLost)
        {
            Text(G->bWon ? TEXT("YOU REACHED THE SURFACE.") : TEXT("THE WARDEN FOUND YOU."),98,428,34,G->bWon ? Amber : Red);
            Text(G->bWon ? TEXT("Behind you, the last light goes out.") : TEXT("It follows light. It remembers noise."),98,489,24,White);
            Text(FString::Printf(TEXT("TIME  %02d:%02d     /     LIGHTS DESTROYED  %02d"),int(G->RunTime)/60,int(G->RunTime)%60,G->BrokenLights),98,559,22,Muted);
            Text(TEXT("ENTER   NEW SHIFT"),98,674,29,Amber);
            Text(TEXT("Q   QUIT"),98,729,21,Muted);
        }
        else if(G->bHelp)
        {
            Text(TEXT("SHIFT REPORT 09"),98,413,29,Amber);
            Text(TEXT("Recover the access card in Records and a fuse in Workshop."),98,473,22,White);
            Text(TEXT("Unlock the Plant. Restore its generator. Vent the Pump Room."),98,513,22,White);
            Text(TEXT("Call the surface lift and stay alive until it returns."),98,553,22,White);
            Text(TEXT("The Warden sees light and hears running or breaking glass."),98,620,22,White);
            Text(TEXT("Cut a circuit, switch off your lamp, crouch, and move away."),98,660,22,White);
            Text(TEXT("Broken lights stay broken. Mission equipment cannot break."),98,700,22,Muted);
            Text(TEXT("ENTER   RETURN TO SHIFT"),98,792,26,Amber);
        }
        else if(G->bPaused)
        {
            Text(TEXT("SHIFT PAUSED"),98,422,31,White);
            Text(TEXT("ENTER   RESUME     /     R   RESTART     /     Q   QUIT"),98,488,23,Amber);
            Text(TEXT("F1   SHIFT REPORT"),98,564,22,White);
            Text(TEXT("F2   RENDER QUALITY     F3   FRAME GENERATION"),98,610,22,White);
            Text(TEXT("[ / ]   MOUSE SENSITIVITY     F4   PERFORMANCE"),98,656,22,White);
            Text(TEXT("F6   FREEZE SCENE / HIDE HUD"),98,702,22,Muted);
        }
        else
        {
            Text(TEXT("The facility is sealed. The night shift is missing."),98,427,25,White);
            Text(TEXT("Restore the lift. Control the light. Find the surface."),98,475,25,White);
            Text(TEXT("Something downstairs is still working."),98,537,24,Muted);
            Text(TEXT("ENTER   BEGIN SHIFT"),98,664,32,Amber);
            Text(TEXT("Q   QUIT"),98,727,21,Muted);
        }
        Rule(98,895,W-196,Muted*0.65f);
        Text(TEXT("WASD  MOVE      MOUSE  LOOK      E  INTERACT      F  LAMP      LMB  BREAK LIGHT"),98,926,20,White);
        Text(TEXT("SHIFT  RUN      CTRL  CROUCH      ESC  PAUSE      F1  HELP"),98,966,19,Muted);
    }
    else if(G->Player)
    {
        Text(TEXT("AFTERLIGHT"),52,44,24,White);
        Rect(52,86,26,3,Amber);
        Text(G->AreaName(G->Player->GetActorLocation()),94,79,18,Muted);
        const float CX=W/2,CY=540;
        AActor* Target=G->Player->TraceInteract();
        FString Prompt;
        if(auto* L=Cast<AFacilityLight>(Target)) Prompt=L->Prompt();
        else if(auto* D=Cast<AFacilityDevice>(Target)) Prompt=D->Prompt(G);
        Rect(CX-2,CY-2,4,4,Prompt.IsEmpty() ? White*0.55f : Amber);
        if(!Prompt.IsEmpty())
        {
            const float PW=FMath::Min(760.f,float(Prompt.Len())*10.6f+36);
            Rect(CX-PW/2,602,PW,45,FLinearColor(0.008f,0.015f,0.017f,0.85f));
            Text(Prompt,CX-PW/2+18,614,19,White);
        }
        Rect(52,929,4,60,Amber);
        Text(TEXT("EVACUATION PROTOCOL"),73,930,15,Muted);
        Text(G->Objective(),73,958,23,White);
        Text(TEXT("E  INTERACT    F  LAMP    F1  REPORT"),52,1027,16,Muted);
        const float Exposure=FMath::Clamp(G->Player->Exposure,0.f,1.f);
        Text(Exposure>0.35f ? TEXT("EXPOSED") : TEXT("CONCEALED"),W-241,921,17,Exposure>0.35f ? Amber : Muted);
        Rect(W-240,951,188,3,FLinearColor(0.1f,0.17f,0.17f));
        Rect(W-240,951,188*Exposure,3,Exposure>0.35f ? Amber : Muted);
        Text(G->Player->bFlashlightOn ? TEXT("LAMP  ON") : TEXT("LAMP  OFF"),W-240,974,16,Muted);
        Rect(W-240,1010,188,3,FLinearColor(0.1f,0.17f,0.17f));
        Rect(W-240,1010,188*G->Player->Stamina,3,White*0.6f);
        if(G->Warden && G->Warden->Suspicion>0.25f)
        {
            const FString Warning=G->Warden->bHunting ? TEXT("IT HAS SEEN YOU") : TEXT("SOMETHING IS LISTENING");
            Text(Warning,CX-130,141,20,Red);
            Rule(CX-145,176,290*G->Warden->Suspicion,Red);
        }
        if(G->NoticeTime>0)
        {
            Rect(52,804,FMath::Min(W-104,1150.f),72,FLinearColor(0.005f,0.013f,0.014f,0.85f));
            Text(G->Notice,72,828,19,White);
        }
    }
    const FString Quality=G->QualityPreset==2 ? TEXT("SHOWCASE") : G->QualityPreset==1 ? TEXT("SMOOTH") : TEXT("QUALITY");
    if(G->bShowTelemetry || G->bTitle || (G->bPaused && !G->bHelp))
    {
        const float TX=W-450;
        Rect(TX-20,48,400,178,FLinearColor(0.004f,0.012f,0.013f,0.85f));
        Text(G->bHardwareReady ? Quality+TEXT(" / HARDWARE RT") : TEXT("HARDWARE RT UNAVAILABLE"),TX,66,21,Amber);
        Text(FString::Printf(TEXT("%.0f RENDER FPS   /   %.1f ms GPU"),1000.f/FMath::Max(0.1f,G->SmoothedFrameMs),G->GPUFrameMs),TX,108,18,White);
        Text(G->bDLSS ? (G->QualityPreset==2 ? TEXT("DLAA") : TEXT("DLSS 4.5 SUPER RESOLUTION")) : TEXT("TSR / DLSS UNAVAILABLE"),TX,140,17,Muted);
        Text(FString::Printf(TEXT("RR %s   /   FG %s   /   %.0f PRESENT FPS"),G->bRayReconstruction ? TEXT("ON"):TEXT("OFF"),G->bFrameGeneration ? TEXT("2X"):TEXT("OFF"),G->PresentedFPS),TX,176,15,Muted);
    }
}
