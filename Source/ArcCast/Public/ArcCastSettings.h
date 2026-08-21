// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcCastSettings.generated.h"

class UArcCastProfile;

/**
 * Project-wide defaults for the preview.
 * Edit under Project Settings > Plugins > ArcCast; stored in DefaultGame.ini.
 *
 * Everything here is read when a world's subsystem comes up, and everything here can be moved afterwards
 * without touching the config - through the setters on this class, the Blueprint library, or the
 * ArcCast.* console commands. The setters push the new value into every live world, which is what makes
 * a demo HUD button and a console command end up in the same place.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ArcCast"))
class ARCCAST_API UArcCastSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcCastSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	/** Convenience accessor. Never returns null. */
	static const UArcCastSettings& Get();

	// ----------------------------------------------------------------------------------------------
	// Master switch and defaults
	// ----------------------------------------------------------------------------------------------

	/** Off simulates nothing and draws nothing, without any caller having to know. */
	UPROPERTY(Config, EditAnywhere, Category = "ArcCast")
	bool bEnabled = true;

	/**
	 * Profile used by any request that brings none of its own. Leave it empty and the built-in profile
	 * named below is used, so the plugin previews something before a single asset exists.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "ArcCast", meta = (AllowedClasses = "/Script/ArcCast.ArcCastProfile"))
	TSoftObjectPtr<UArcCastProfile> DefaultProfile;

	/** Which built-in profile to fall back on: Grenade, Teleport, Arrow or Bouncy. */
	UPROPERTY(Config, EditAnywhere, Category = "ArcCast")
	FName DefaultBuiltinProfile = FName(TEXT("Grenade"));

	// ----------------------------------------------------------------------------------------------
	// Occlusion
	// ----------------------------------------------------------------------------------------------

	/**
	 * Trace from the camera to the arc and draw the blocked stretches dashed and faint instead of letting
	 * them shine through the wall.
	 *
	 * This is the part that costs traces, so it is the first thing to switch off on a tight frame - and
	 * the cost is measured, not guessed: ArcCast.Stats reports occlusion traces separately.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion")
	bool bOcclusionAware = true;

	/** Opacity multiplier for a stretch the camera cannot see. Zero hides it, which is not the point. */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OccludedOpacity = 0.25f;

	/**
	 * Test every Nth point rather than every point, and carry the answer forward to the points between.
	 * 2 halves the trace count and is invisible at any sane step size; 1 is exact and twice the price.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion", meta = (ClampMin = "1", UIMax = "16"))
	int32 OcclusionEveryNthPoint = 2;

	/**
	 * The occlusion trace stops this many cm short of the arc point.
	 *
	 * Without it every point that lies *on* a surface - the landing point, the whole splash ring - reports
	 * itself as occluded by the very thing it is sitting on, and the ring goes permanently faint.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Occlusion", meta = (ClampMin = "0.0", UIMax = "50.0"))
	float OcclusionEndOffset = 8.0f;

	// ----------------------------------------------------------------------------------------------
	// Budget
	// ----------------------------------------------------------------------------------------------

	/**
	 * Hard cap on integration steps across every live arc in one frame.
	 *
	 * Going over does not drop arcs and does not spill into the next frame - the step size is coarsened
	 * for that frame instead, so a busy frame costs accuracy rather than frame time. 300 steps is ten
	 * arcs at four seconds of flight and a 1/30 s step, which is far more preview than any game shows
	 * at once.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "16", UIMax = "4000"))
	int32 MaxSimStepsPerFrame = 300;

	/** Absolute ceiling on the points one arc may hold, whatever the step size works out to. */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "8", UIMax = "8192"))
	int32 MaxPointsPerArc = 1024;

	// ----------------------------------------------------------------------------------------------
	// Drawing
	// ----------------------------------------------------------------------------------------------

	/** Opacity multiplier over everything ArcCast draws. Lets a project dim the preview globally. */
	UPROPERTY(Config, EditAnywhere, Category = "Drawing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlobalOpacity = 1.0f;

	/**
	 * Draw through AHUD::OnHUDPostRender as well, so any HUD class previews arcs without being changed.
	 *
	 * Nothing is actually spawned: the subsystem hooks the delegate the engine already broadcasts after
	 * every HUD render and draws on the canvas it hands over. Use ArcCastHUD or the HUD component when
	 * you want the draw to sit at a defined point in your own DrawHUD instead - the frame guard makes
	 * sure the arc is never drawn twice.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Drawing")
	bool bAutoSpawnHUDComponent = true;

	/**
	 * Also draw in an editor viewport with no PIE running, through UDebugDrawService.
	 *
	 * This is the second path, not the product path: UDebugDrawService is compiled out of a cooked build,
	 * exactly like the DrawDebugLine problem this plugin exists to solve. It is here so the arc can be
	 * seen, screenshotted and filmed while the level is being built. What ships is the AHUD path.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Drawing")
	bool bDrawInEditorViewport = true;

	/** Show the statistics box as soon as a world comes up. Same as ArcCast.Stats 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Drawing")
	bool bShowStatsByDefault = false;

	// ----------------------------------------------------------------------------------------------
	// Runtime setters
	// ----------------------------------------------------------------------------------------------
	//
	// These write the CDO and push the value into every live world's subsystem. They do not save the
	// config: a value set at runtime is a runtime value, and a plugin that quietly rewrites DefaultGame.ini
	// behind a designer's back is a plugin nobody trusts twice.

	/** Master switch. Off clears nothing - it only stops the simulation and the draw. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetEnabled(bool bInEnabled);

	/** Turn the camera-to-arc occlusion traces on or off. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetOcclusionAware(bool bInOcclusionAware);

	/** Opacity of an occluded stretch, 0..1. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetOccludedOpacity(float InOpacity);

	/** Test every Nth point for occlusion instead of every point. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetOcclusionEveryNthPoint(int32 InEveryNth);

	/** Hard cap on integration steps per frame across every arc. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetMaxSimStepsPerFrame(int32 InMaxSteps);

	/** Opacity multiplier over everything ArcCast draws, 0..1. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetGlobalOpacity(float InOpacity);

	/** Draw in an editor viewport with no PIE running. Has no effect in a cooked build. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetDrawInEditorViewport(bool bInDraw);

	/** Draw through AHUD::OnHUDPostRender so an unmodified HUD class previews arcs. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetAutoSpawnHUDComponent(bool bInAuto);

	/** Profile used by any request that brings none of its own. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Settings")
	static void SetDefaultProfile(UArcCastProfile* InProfile);

private:
	/** Re-read the settings in every live world. Called by every setter above. */
	static void PushToLiveWorlds();
};
