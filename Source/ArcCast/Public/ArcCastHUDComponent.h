// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ArcCastHUDComponent.generated.h"

class UCanvas;

/**
 * Already have your own HUD class? Add this to it and call Draw Arcs (Canvas) from your own Draw HUD.
 *
 * That is the whole component. It exists because "set your game mode's HUD class to ours" is the reason
 * most drop-in HUD features never get used - a project that has spent a year on its own HUD is not going
 * to swap it out for a preview.
 *
 * Not needed at all when bAutoSpawnHUDComponent is left on: the subsystem then draws from the delegate
 * the engine broadcasts after every HUD render. Use this when you want the arcs drawn at a defined point
 * in your own Draw HUD - underneath your crosshair, say, rather than on top of it.
 */
UCLASS(ClassGroup = (ArcCast), meta = (BlueprintSpawnableComponent, DisplayName = "ArcCast HUD"))
class ARCCAST_API UArcCastHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcCastHUDComponent();

	/** Draw every registered arc onto this canvas. One line, in Draw HUD. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void DrawArcs(UCanvas* Canvas);
};
