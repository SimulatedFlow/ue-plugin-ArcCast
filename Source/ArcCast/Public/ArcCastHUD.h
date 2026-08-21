// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArcCastHUD.generated.h"

class UArcCastHUDComponent;

/**
 * The ready-made HUD class: set it as your game mode's HUD Class and the arcs are on screen.
 *
 * Deliberately AHUD-based. Draw HUD runs in a cooked Shipping build, which is the whole argument of this
 * plugin - the engine's own preview is drawn with DrawDebugLine, ENABLE_DRAW_DEBUG is 0 in Shipping, and
 * the aiming aid the player was promised is not in the build the player gets. Canvas through AHUD is the
 * same path the crosshair takes, so it ships.
 *
 * Already have your own HUD class? Do not replace it - either leave bAutoSpawnHUDComponent on and change
 * nothing at all, or add UArcCastHUDComponent to your HUD and call Draw Arcs (Canvas) from your own Draw
 * HUD. All three paths end in the same subsystem call, and the frame guard keeps them from stacking.
 */
UCLASS(meta = (DisplayName = "ArcCast HUD"))
class ARCCAST_API AArcCastHUD : public AHUD
{
	GENERATED_BODY()

public:
	AArcCastHUD();

	// AHUD interface
	virtual void DrawHUD() override;

	/** Draw the arcs when this HUD renders. Off makes this behave like a plain AHUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	bool bDrawArcs = true;

protected:
	/** Present so a Blueprint child of this class can reach the same call the C++ path uses. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ArcCast")
	TObjectPtr<UArcCastHUDComponent> ArcComponent;
};
