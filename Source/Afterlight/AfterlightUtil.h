#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "HAL/IConsoleManager.h"

namespace Afterlight
{
    inline UMaterialInterface* Material(const FString& Name)
    {
        return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Materials/M_%s.M_%s"), *Name, *Name));
    }
    inline UStaticMesh* Shape(bool bCylinder = false)
    {
        UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr, bCylinder ? TEXT("/Game/Geometry/SM_Pipe.SM_Pipe") : TEXT("/Game/Geometry/SM_Block.SM_Block"));
        // The fallback only bootstraps the editor before GenerateAssets has run.
        return Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, bCylinder ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Cube.Cube"));
    }
    inline void Shadow(UPrimitiveComponent* Component)
    {
        Component->SetCastShadow(true);
        Component->SetVisibleInRayTracing(true);
        Component->bAffectDynamicIndirectLighting = true;
    }
    inline UStaticMeshComponent* Part(AActor* Actor, USceneComponent* Parent, FVector Position, FVector Size, const FString& Mat, bool bCylinder = false)
    {
        auto* Mesh = NewObject<UStaticMeshComponent>(Actor);
        Mesh->SetupAttachment(Parent);
        Mesh->SetStaticMesh(Shape(bCylinder));
        Mesh->SetMaterial(0, Material(Mat));
        Mesh->SetRelativeLocation(Position);
        Mesh->SetRelativeScale3D(Size / 100.f);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetMobility(EComponentMobility::Movable);
        Shadow(Mesh);
        Mesh->RegisterComponent();
        return Mesh;
    }
    inline void CVar(const TCHAR* Name, float Value)
    {
        if (auto* V = IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(Value, ECVF_SetByConsole);
    }
    inline int32 IntCVar(const TCHAR* Name)
    {
        auto* V = IConsoleManager::Get().FindConsoleVariable(Name);
        return V ? V->GetInt() : -1;
    }
}
