// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ArcCastTypes.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ArcCastProfile.generated.h"

/**
 * Flight behaviour and appearance in one asset.
 *
 * The two are together on purpose. A grenade arc and a teleport arc do not only fly differently, they
 * have to *look* different - the grenade bounces and needs a splash ring, the teleport goes straight,
 * demands a navigable landing spot and wants no ring at all. Splitting that across a "physics asset" and
 * a "style asset" means every project immediately re-pairs them by hand, so ArcCast ships them paired.
 *
 * Four ready-made profiles exist in code (Grenade, Teleport, Arrow, Bouncy) so the plugin previews
 * something the minute it is enabled, before any asset has been created. Duplicating one of the
 * Content/ArcCast/Profiles assets is the normal way to get your own.
 */
UCLASS(BlueprintType, meta = (DisplayName = "ArcCast Profile"))
class ARCCAST_API UArcCastProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcCastProfile();

	// ----------------------------------------------------------------------------------------------
	// Flight
	// ----------------------------------------------------------------------------------------------

	/**
	 * Speed in cm/s used when a caller gives a direction instead of a velocity - the component path.
	 * Callers that already have a velocity (a throwing system about to spawn a projectile) ignore this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "1.0", UIMax = "6000.0"))
	float LaunchSpeed = 1400.0f;

	/**
	 * World gravity multiplier. 1 is the world's own gravity; a projectile movement component with a
	 * Projectile Gravity Scale of 0.6 needs 0.6 here or the preview will not match the throw.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.0", UIMax = "4.0"))
	float GravityScale = 1.0f;

	/**
	 * Radius in cm swept along the path. Zero makes it a line trace, which is faster and thinner than the
	 * thing it is predicting - a grenade the size of a fist that is predicted as a ray will happily fly
	 * through a gap it could never fit through.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.0", UIMax = "100.0"))
	float ProjectileRadius = 8.0f;

	/**
	 * Bounces simulated after the first hit. Zero reproduces the engine's own behaviour: stop where you
	 * first touch something. Two is enough for a grenade to read as a grenade without the arc becoming
	 * a scribble.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0", UIMax = "8"))
	int32 MaxBounces = 2;

	/** Fraction of the normal velocity kept across a bounce. 0 sticks, 1 bounces forever. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution = 0.45f;

	/** Fraction of the tangential velocity lost across a bounce - how much the surface drags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Friction = 0.2f;

	/** Hard stop on simulated flight time in seconds. Bounds the cost of an arc thrown into open sky. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.1", UIMax = "20.0"))
	float MaxSimTime = 4.0f;

	/** Speed in cm/s below which a bounce is treated as coming to rest instead of carrying on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.0", UIMax = "500.0"))
	float RestSpeed = 60.0f;

	/**
	 * Integration step in seconds. 1/30 is smooth enough to read and cheap enough to run every frame; the
	 * per-frame budget may coarsen it further when several arcs are live at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight", meta = (ClampMin = "0.002", ClampMax = "0.2"))
	float FixedStepSeconds = 1.0f / 30.0f;

	/** Channel the path is swept against. Visibility is the sane default; a dedicated channel is better. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Visibility;

	/** Sweep against complex (per-triangle) collision. Slower, and rarely what a thrown object wants. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
	bool bTraceComplex = false;

	// ----------------------------------------------------------------------------------------------
	// Landing verdict
	// ----------------------------------------------------------------------------------------------

	/** How far down the ground probe reaches from the end of an arc that never hit anything, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0", UIMax = "2000.0"))
	float GroundProbeDistance = 200.0f;

	/** Angle from world up past which a landing surface counts as TooSteep. Matches CharacterMovement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxWalkableSlopeAngle = 45.0f;

	/**
	 * Also require the landing point to project onto the navigation mesh. This is what turns the preview
	 * into a teleport target indicator.
	 *
	 * A project with no navigation system does not break: the verdict comes back NoGround, honestly, and
	 * nothing is drawn green.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing")
	bool bRequireNavMesh = false;

	/** Search box for the navigation projection. Too small and a valid spot on a slope is refused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (EditCondition = "bRequireNavMesh"))
	FVector NavProjectExtent = FVector(100.0f, 100.0f, 200.0f);

	// ----------------------------------------------------------------------------------------------
	// Splash ring
	// ----------------------------------------------------------------------------------------------

	/** Radius of the effect ring at the landing point, in cm. Zero means no ring for this profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring", meta = (ClampMin = "0.0", UIMax = "2000.0"))
	float SplashRadius = 300.0f;

	/** Draw the ring at all. Defaults on, and does nothing when SplashRadius is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring")
	bool bDrawRing = true;

	/** Points the ring is built from. 32 reads as a circle; 12 reads as a decision to save traces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring", meta = (ClampMin = "3", ClampMax = "128"))
	int32 RingSegments = 32;

	/**
	 * How far each ring point is probed up and down to find the surface, in cm.
	 *
	 * This is the difference between a ring that lies on the stairs and a flat disc hovering through them.
	 * It costs one trace per ring point per recomputation - the price is in ArcCast.Stats, under Ring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring", meta = (ClampMin = "0.0", UIMax = "1000.0"))
	float RingConformDistance = 150.0f;

	/** Lift in cm applied to every conformed ring point so it is not buried in the surface it sits on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring", meta = (ClampMin = "0.0", UIMax = "50.0"))
	float RingSurfaceOffset = 4.0f;

	/** Stroke width of the ring in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring", meta = (ClampMin = "0.0", UIMax = "16.0"))
	float RingThickness = 2.0f;

	// ----------------------------------------------------------------------------------------------
	// Appearance
	// ----------------------------------------------------------------------------------------------

	/** How the arc is stroked. Occluded stretches ignore this and are always dashed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	EArcCastLineStyle LineStyle = EArcCastLineStyle::Solid;

	/** Stroke width of the first stretch in pixels. Later bounce stretches thin out with BounceFadePerHit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0", UIMax = "24.0"))
	float LineThickness = 3.0f;

	/** Dash length in screen pixels. Also the gap length for Dots, where the dash shrinks to the stroke. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "1.0", UIMax = "80.0"))
	float DashLengthPixels = 14.0f;

	/** Gap between dashes in screen pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "1.0", UIMax = "80.0"))
	float DashGapPixels = 10.0f;

	/**
	 * Dash scroll speed in pixels per second, running from launch towards the landing point.
	 *
	 * Costs nothing - it is one number added to a modulo - and it is the difference between a still image
	 * of an arc and something that reads as aiming. Zero holds the dashes still.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "-500.0", UIMax = "500.0"))
	float DashScrollSpeed = 40.0f;

	/** Colour for the Valid verdict. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor ValidColor = FLinearColor(0.16f, 0.92f, 0.42f, 1.0f);

	/** Colour for TooSteep and OutOfRange - "you can throw there, you just cannot stand there". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor WarnColor = FLinearColor(1.0f, 0.78f, 0.18f, 1.0f);

	/** Colour for Blocked and NoGround. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor InvalidColor = FLinearColor(0.96f, 0.25f, 0.22f, 1.0f);

	/**
	 * Opacity and thickness multiplier applied once per bounce already survived, so the stretch after the
	 * first bounce is fainter than the one before it and the eye reads the order of events for free.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float BounceFadePerHit = 0.6f;

	/** Draw a marker at the landing point. Redundant when the ring is on, essential when it is off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	bool bDrawEndMarker = true;

	/** Half-size of the end marker in screen pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "1.0", UIMax = "80.0"))
	float EndMarkerSize = 18.0f;

	/** Screen-space segments shorter than this are folded into the next one instead of drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0", UIMax = "20.0"))
	float MinScreenSegmentPixels = 2.0f;

	/** Colour for the given verdict, straight from this profile. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	FLinearColor GetVerdictColor(EArcCastVerdict Verdict) const;

	/** Effective radius for the splash ring, honouring bDrawRing. Zero means "draw nothing". */
	float GetEffectiveSplashRadius() const { return bDrawRing ? FMath::Max(0.0f, SplashRadius) : 0.0f; }

	// UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
