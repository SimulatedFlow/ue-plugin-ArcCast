// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ArcCastTypes.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcCastSubsystem.generated.h"

class AActor;
class AHUD;
class APlayerController;
class UArcCastProfile;
class UCanvas;
class UFont;

/**
 * The whole plugin. One per world: it simulates the arcs, holds them, and draws them.
 *
 * Two things live here that the engine's own prediction does not do.
 *
 *   1. The simulation continues past the first hit. UGameplayStatics::PredictProjectilePath stops there;
 *      ArcCast mirrors the velocity about the impact normal, damps it with Restitution and Friction, and
 *      carries on until MaxBounces, RestSpeed or MaxSimTime says otherwise. Every point remembers which
 *      bounce it belongs to, so later stretches can be drawn thinner and fainter.
 *   2. The landing gets a verdict. Ground probe, slope against MaxWalkableSlopeAngle, optionally a
 *      navigation projection - and the verdict colours the arc and the ring.
 *
 * And one thing that is only about drawing: nothing here calls DrawDebugLine. The whole preview is
 * canvas polylines projected with FSceneView, which is what makes it survive a Shipping build. See the
 * comment above DrawArcs.
 *
 * SimulateArc is deliberately separable from all of it. A project that only wants to know where a throw
 * would land - an AI deciding whether a grenade is worth it, a validator on the server - calls that one
 * function, gets an FArcCastPath and a verdict, and never registers anything for drawing.
 */
UCLASS(meta = (DisplayName = "ArcCast Subsystem"))
class ARCCAST_API UArcCastSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * True, and it matters. A UTickableWorldSubsystem does not tick in an editor world unless this says
	 * so - the symptom is a subsystem that is alive, holds arcs, and silently never recomputes them.
	 * Without it there is no arc in the editor viewport and therefore no store video.
	 */
	virtual bool IsTickableInEditor() const override { return true; }

	/** The subsystem for this world, or null. Safe with a null world context. */
	static UArcCastSubsystem* Get(const UObject* WorldContextObject);

	// ----------------------------------------------------------------------------------------------
	// Prediction without any drawing
	// ----------------------------------------------------------------------------------------------

	/**
	 * Simulate one arc and hand the result straight back. Nothing is registered, nothing is drawn.
	 *
	 * This is the half of ArcCast that has nothing to do with pixels: "if I threw this, from here, at
	 * that velocity - where does it end up, how long is it in the air, and may I land there?" It answers
	 * on the calling frame, ignores the per-frame budget (a single explicit query is not the thing the
	 * budget is protecting against), and is safe to call on a dedicated server where no canvas exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (AutoCreateRefTerm = "Request"))
	FArcCastPath SimulateArc(const FArcCastRequest& Request);

	// ----------------------------------------------------------------------------------------------
	// Registered arcs
	// ----------------------------------------------------------------------------------------------

	/**
	 * Register or update an arc under Id and keep it recomputed and drawn until it is hidden.
	 *
	 * Pass an Id of 0 to be given a fresh one; the return value is the Id either way. Calling it again
	 * with the same Id replaces the request in place - that is the normal per-frame update path and it
	 * allocates nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (AutoCreateRefTerm = "Request"))
	int32 ShowArc(int32 Id, const FArcCastRequest& Request);

	/** Stop drawing and recomputing the arc under Id. Unknown ids are ignored. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void HideArc(int32 Id);

	/** Drop every registered arc. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void ClearArcs();

	/** The last computed path for Id. False when nothing is registered under it. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	bool GetArc(int32 Id, FArcCastPath& OutPath) const;

	/** How many arcs are registered right now. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	int32 GetActiveArcCount() const { return Arcs.Num(); }

	/**
	 * Verdict of the most recently computed path - registered or one-off.
	 *
	 * Convenient for a single-preview game and wrong for anything with two arcs on screen, where GetArc
	 * with the id you were given is the answer.
	 */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	EArcCastVerdict GetLastVerdict() const { return LastVerdict; }

	/** The most recently computed path, whatever produced it. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	FArcCastPath GetLastPath() const { return LastPath; }

	// ----------------------------------------------------------------------------------------------
	// Drawing
	// ----------------------------------------------------------------------------------------------

	/**
	 * Draw every registered arc onto this canvas. One call, from DrawHUD.
	 *
	 * DrawDebugLine does not appear anywhere below this line, and that is the product. ENABLE_DRAW_DEBUG
	 * is 0 in a Shipping build, so every debug-drawn preview - including the engine's own
	 * EDrawDebugTrace - vanishes the moment the game is packaged for release. Canvas polylines through
	 * AHUD do not: they are the same path the crosshair and the ammo counter take. Everything here goes
	 * through FCanvasLineItem, and world points reach the screen through FSceneView::Project.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void DrawArcs(UCanvas* Canvas);

	// ----------------------------------------------------------------------------------------------
	// Runtime knobs (console commands and the Blueprint library end up here)
	// ----------------------------------------------------------------------------------------------

	/** Re-read UArcCastSettings. Called on Initialize and by every settings setter. */
	void ApplySettings();

	void SetEnabled(bool bInEnabled);
	bool IsEnabled() const { return bEnabled; }

	void SetOcclusionAware(bool bInOcclusionAware);
	bool IsOcclusionAware() const { return bOcclusionAware; }

	void SetOccludedOpacity(float InOpacity);
	void SetOcclusionEveryNthPoint(int32 InEveryNth);
	void SetMaxSimStepsPerFrame(int32 InMaxSteps);
	void SetGlobalOpacity(float InOpacity);
	void SetShowStats(bool bInShow);
	bool IsShowingStats() const { return bShowStats; }

	/** Force MaxBounces on every registered arc. Below zero clears the override. */
	void SetBounceOverride(int32 InBounces);

	/** Force a line style on every arc, whatever its profile says. Cleared with ClearStyleOverride. */
	void SetStyleOverride(EArcCastLineStyle InStyle);
	void ClearStyleOverride();

	/** Last frame's measurements. This is what the store's performance numbers are read off. */
	UFUNCTION(BlueprintPure, Category = "ArcCast|Stats")
	FArcCastStats GetStats() const { return Stats; }

	/** Write the statistics to the log in the same shape the on-screen box uses. */
	void LogStats() const;

	/**
	 * Register a demonstration arc thrown from the current view - what ArcCast.Test does.
	 *
	 * Uses the last camera position and direction seen by a draw pass, so it works from a PIE session and
	 * from an editor viewport alike. Returns the id, or 0 when no view has been seen yet.
	 */
	int32 SpawnTestArc();

	/** Resolve a profile pointer, falling back to the project default and then to a built-in. */
	const UArcCastProfile* ResolveProfile(const UArcCastProfile* Requested) const;

	/** One of the four code-side profiles: Grenade, Teleport, Arrow, Bouncy. Null if the name is unknown. */
	const UArcCastProfile* GetBuiltinProfile(FName ProfileName) const;

private:
	/** A registered arc: the request as the caller left it, and the path as of the last recomputation. */
	struct FActiveArc
	{
		FArcCastRequest Request;
		FArcCastPath Path;
	};

	// -- Simulation ---------------------------------------------------------------------------------

	/** Counters threaded through one simulation so the trace cost can be attributed rather than guessed. */
	struct FSimCounters
	{
		int32 Steps = 0;
		int32 SimTraces = 0;
		int32 RingTraces = 0;
	};

	void SimulateInternal(const FArcCastRequest& Request, float StepSeconds, FArcCastPath& OutPath, FSimCounters& Counters) const;
	EArcCastVerdict EvaluateVerdict(const FArcCastRequest& Request, const UArcCastProfile& Profile, bool bHadImpact, bool bOutOfRange, bool bStartedPenetrating, FArcCastPath& InOutPath, FSimCounters& Counters) const;
	void BuildSplashRing(const FArcCastRequest& Request, const UArcCastProfile& Profile, FArcCastPath& InOutPath, FSimCounters& Counters) const;

	/** Step size for this frame: the profile's, coarsened when the frame's arcs would blow the budget. */
	float ComputeBudgetedStep() const;

	// -- Drawing ------------------------------------------------------------------------------------

	void DrawSingleArc(UCanvas* Canvas, FActiveArc& Arc);
	void UpdateOcclusion(FArcCastPath& InOutPath, const FArcCastRequest& Request, const UArcCastProfile& Profile, const FVector& ViewLocation);
	void DrawStatsBox(UCanvas* Canvas);

	/** World point to canvas pixel. False when the point is behind the near plane. */
	static bool ProjectPoint(const UCanvas* Canvas, const FVector& World, FVector2D& OutScreen, float& OutDepth);

	/** Projects a segment, clipping it against the near plane so an arc leaving the frustum does not flip. */
	static bool ProjectSegment(const UCanvas* Canvas, const FVector& A, const FVector& B, FVector2D& OutA, FVector2D& OutB);

	/** Stroke one screen-space segment in the given style, keeping the dash pattern continuous. */
	void StrokeSegment(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness, EArcCastLineStyle Style, const UArcCastProfile& Profile, float& InOutRunLength);

	void DrawEndMarker(UCanvas* Canvas, const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness);

	/** Remember where the camera is, for the occlusion traces and for ArcCast.Test. */
	void CacheView(const UCanvas* Canvas);

	// -- Hooks --------------------------------------------------------------------------------------

	/** AHUD::OnHUDPostRender, so an unmodified HUD class still previews arcs. Shipping-safe. */
	void OnAnyHUDPostRender(AHUD* HUD, UCanvas* Canvas);

#if WITH_EDITOR
	/**
	 * Second draw path, editor only: an arc in the viewport with no PIE running.
	 *
	 * The store screenshots and the store video are made here, and that is the entire reason it exists.
	 * UDebugDrawService is compiled out of a cooked build - the same disappearing act that makes the
	 * engine's own preview useless in Shipping - so it can never be the path a game relies on.
	 */
	void OnEditorViewportDraw(UCanvas* Canvas, APlayerController* PlayerController);

	FDelegateHandle EditorDrawHandle;
#endif

	FDelegateHandle HudPostRenderHandle;

	// -- State --------------------------------------------------------------------------------------

	/** Registered arcs by id. A TMap because ids are handed out, not indices, and holes are normal. */
	TMap<int32, FActiveArc> Arcs;

	/** Next id handed out by ShowArc(0, ...). Starts at 1 so 0 can mean "none". */
	int32 NextArcId = 1;

	/** Id used by ArcCast.Test, so a second Test replaces the first rather than stacking. */
	int32 TestArcId = 0;

	FArcCastPath LastPath;
	EArcCastVerdict LastVerdict = EArcCastVerdict::NoGround;

	FArcCastStats Stats;

	/** Scroll phase of the dashes in pixels. Advanced on Tick so it also runs in an editor viewport. */
	float DashPhase = 0.0f;

	/** Cached view, refreshed by every draw pass. Zero-length forward means "no view seen yet". */
	FVector CachedViewLocation = FVector::ZeroVector;
	FVector CachedViewForward = FVector::ZeroVector;

	/** Frame the arcs were last drawn on, so the HUD path and the editor path never double-draw. */
	uint64 LastDrawFrame = 0;

	/** The four code-side profiles, so the plugin previews something with no content installed. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UArcCastProfile>> BuiltinProfiles;

	/** Resolved once from UArcCastSettings::DefaultProfile, which is a soft pointer. */
	UPROPERTY(Transient)
	TObjectPtr<UArcCastProfile> ResolvedDefaultProfile;

	void CreateBuiltinProfiles();

	// -- Settings mirror ----------------------------------------------------------------------------

	bool bEnabled = true;
	bool bOcclusionAware = true;
	float OccludedOpacity = 0.25f;
	int32 OcclusionEveryNthPoint = 2;
	float OcclusionEndOffset = 8.0f;
	int32 MaxSimStepsPerFrame = 300;
	int32 MaxPointsPerArc = 1024;
	float GlobalOpacity = 1.0f;
	bool bAutoHUDDraw = true;
	bool bDrawInEditorViewport = true;
	bool bShowStats = false;

	/** Below zero: every arc keeps its own MaxBounces. Set by ArcCast.Bounces. */
	int32 BounceOverride = -1;

	bool bHasStyleOverride = false;
	EArcCastLineStyle StyleOverride = EArcCastLineStyle::Solid;

	void RebindHudDelegate();
};
