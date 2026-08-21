// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "ArcCastTypes.generated.h"

class AActor;
class UArcCastProfile;

/**
 * What the simulation thinks of the place the arc ends up.
 *
 * This is the second thing the engine's own prediction does not give you. PredictProjectilePath hands
 * back a point list and a hit result; whether landing there is *allowed* is a question it never asks.
 * A grenade indicator wants to know it, a teleport indicator lives on it, and an ability targeter needs
 * it before it will let the player commit. The verdict colours the arc and the splash ring, and it is
 * broadcast by UArcCastComponent so a crosshair can recolour without polling.
 */
UENUM(BlueprintType)
enum class EArcCastVerdict : uint8
{
	/** Ground was found under the end of the arc, the slope is walkable, and any navigation check passed. */
	Valid,

	/** There is ground, but its normal leans further from world up than the profile's walkable angle. */
	TooSteep,

	/**
	 * The landing spot exists but is refused: either the launch point itself starts inside geometry, or
	 * the profile asks for navigation and the point does not project onto the navigation mesh.
	 */
	Blocked,

	/** The arc travelled further than the request's MaxRange before it landed anywhere. */
	OutOfRange,

	/**
	 * Nothing under the end of the arc within GroundProbeDistance - a pit, a ledge, the open sky. Also
	 * the honest answer when a profile asks for navigation in a project that has no navigation system.
	 */
	NoGround
};

/** How a stretch of arc is stroked on the canvas. Occluded stretches are always forced to Dashed. */
UENUM(BlueprintType)
enum class EArcCastLineStyle : uint8
{
	/** One unbroken polyline. Cheapest, and the right default for a grenade arc. */
	Solid,

	/** Dashes of DashLengthPixels with DashGapPixels between them, scrolling at DashScrollSpeed. */
	Dashed,

	/** Short pips - a dashed line with the dash shortened to roughly the line thickness. */
	Dots
};

/**
 * Everything the simulation needs for one arc. Fill it, hand it to the subsystem, get an FArcCastPath.
 *
 * The request carries a velocity, not an angle and a speed, because that is what a throwing system
 * already has at the moment it is about to spawn the projectile - and matching that value is what makes
 * the preview agree with the throw. UArcCastComponent builds the velocity for you from a direction and
 * the profile's LaunchSpeed when you would rather not.
 */
USTRUCT(BlueprintType)
struct ARCCAST_API FArcCastRequest
{
	GENERATED_BODY()

	/** Where the projectile leaves the hand, the muzzle or the socket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	FVector StartLocation = FVector::ZeroVector;

	/** Launch velocity in cm/s. This is the number that has to match your throwing system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	FVector LaunchVelocity = FVector::ZeroVector;

	/** Flight behaviour and appearance. Empty falls back to the project's default profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	TObjectPtr<UArcCastProfile> Profile = nullptr;

	/** Never swept against. The thrower belongs in here, or the arc starts by hitting its own owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	TArray<TObjectPtr<AActor>> IgnoredActors;

	/** Below zero: use the profile's MaxBounces. Zero: stop at the first hit, like the engine does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast|Overrides", meta = (ClampMin = "-1", UIMax = "8"))
	int32 MaxBouncesOverride = -1;

	/** At or below zero: use the profile's MaxSimTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast|Overrides", meta = (ClampMin = "-1.0", UIMax = "20.0"))
	float MaxSimTimeOverride = -1.0f;

	/** At or below zero: use the profile's SplashRadius. Exactly zero on the profile means no ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast|Overrides", meta = (ClampMin = "-1.0", UIMax = "2000.0"))
	float SplashRadiusOverride = -1.0f;

	/**
	 * Path length in cm past which the arc is reported OutOfRange. Zero means unlimited.
	 *
	 * Measured along the flight path, not as a straight line to the landing point, so a bouncing arc that
	 * ends up close by can still run out of range - which is what an ability with a throw budget wants.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast|Overrides", meta = (ClampMin = "0.0", UIMax = "20000.0"))
	float MaxRange = 0.0f;
};

/**
 * The answer: the polyline, the landing point, the verdict, and the conformed splash ring.
 *
 * Points and BounceIndex are parallel arrays; BounceIndex[i] is how many bounces had already happened
 * when the segment starting at Points[i] was integrated, which is what lets later stretches be drawn
 * thinner and fainter. Occluded[i] is filled in at draw time, once a camera position exists.
 */
USTRUCT(BlueprintType)
struct ARCCAST_API FArcCastPath
{
	GENERATED_BODY()

	/** The flight path in world space, start point first. Two or more points whenever bHasResult is true. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	TArray<FVector> Points;

	/** Bounces already survived at the segment leaving Points[i]. Same length as Points. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	TArray<uint8> BounceIndex;

	/** 1 where the view is blocked between the camera and Points[i]. Filled in by the draw pass. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	TArray<uint8> Occluded;

	/** The splash ring, already conformed to the geometry point by point. Empty when the ring is off. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	TArray<FVector> RingPoints;

	/** Where it lands. With no hit at all this is the ground probe's answer, or the last point in the air. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	FVector ImpactPoint = FVector::ZeroVector;

	/** Surface normal at the landing point. World up when nothing was hit. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	FVector ImpactNormal = FVector::UpVector;

	/** Verdict on the landing point. Drives the colour of both the arc and the ring. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	EArcCastVerdict Verdict = EArcCastVerdict::NoGround;

	/** Simulated flight time in seconds from launch to the last point. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	float TotalTime = 0.0f;

	/** Path length in cm, summed segment by segment - not the straight-line distance. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	float TotalDistance = 0.0f;

	/** How many bounces actually happened, which is at most the effective MaxBounces. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	int32 BounceCount = 0;

	/** Integration steps this path cost. Shows up in ArcCast.Stats and in the budget arithmetic. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	int32 StepCount = 0;

	/** Identifier this path is registered under in the subsystem, or 0 for a one-off prediction. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	int32 Id = 0;

	/** False when the request could not be simulated at all - no world, no profile, no velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast")
	bool bHasResult = false;

	/**
	 * What was hit at the landing point, if anything.
	 *
	 * Weak on purpose: a path can outlive the actor it landed on by a frame or two, and a hard reference
	 * from a cached preview is exactly the kind of thing that keeps a destroyed actor alive. Reach it from
	 * Blueprint with Get Arc Impact Actor.
	 */
	UPROPERTY()
	TWeakObjectPtr<AActor> ImpactActor;

	/** Resolved impact actor, or null. */
	AActor* GetImpactActor() const { return ImpactActor.Get(); }

	void Reset()
	{
		Points.Reset();
		BounceIndex.Reset();
		Occluded.Reset();
		RingPoints.Reset();
		ImpactPoint = FVector::ZeroVector;
		ImpactNormal = FVector::UpVector;
		Verdict = EArcCastVerdict::NoGround;
		TotalTime = 0.0f;
		TotalDistance = 0.0f;
		BounceCount = 0;
		StepCount = 0;
		bHasResult = false;
		ImpactActor = nullptr;
	}
};

/**
 * What the last frame cost. Printed by ArcCast.Stats and drawn in the on-screen box.
 *
 * These are the numbers a buyer asks for, so they are measured rather than asserted: steps integrated,
 * traces fired split by what fired them, and the wall time of the simulation and the draw. Switching
 * bOcclusionAware off has to make OcclusionTraces fall to zero, and you can watch it happen.
 */
USTRUCT(BlueprintType)
struct ARCCAST_API FArcCastStats
{
	GENERATED_BODY()

	/** Arcs currently registered for drawing. One-off predictions are not counted. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 ActiveArcs = 0;

	/** Integration steps across every arc last frame. Compared against MaxSimStepsPerFrame. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 SimSteps = 0;

	/** Sweeps fired by the integrator - one per step. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 SimTraces = 0;

	/** Traces fired to conform the splash ring to the geometry, plus the ground probe. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 RingTraces = 0;

	/** Traces fired from the camera to test occlusion. Zero when bOcclusionAware is off. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 OcclusionTraces = 0;

	/** SimTraces + RingTraces + OcclusionTraces. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 TotalTraces = 0;

	/** Canvas line items submitted last draw. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	int32 LinesDrawn = 0;

	/** Wall time of the whole simulation pass in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	float SimMilliseconds = 0.0f;

	/** Wall time of the canvas pass, occlusion traces included, in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	float DrawMilliseconds = 0.0f;

	/** Step size actually used, after the budget had its say. Larger than the profile's means coarsened. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	float EffectiveStepSeconds = 0.0f;

	/** True when the frame's step estimate ran past MaxSimStepsPerFrame and the step was coarsened. */
	UPROPERTY(BlueprintReadOnly, Category = "ArcCast|Stats")
	bool bBudgetCoarsened = false;

	void ResetFrame()
	{
		SimSteps = 0;
		SimTraces = 0;
		RingTraces = 0;
		OcclusionTraces = 0;
		TotalTraces = 0;
		LinesDrawn = 0;
		SimMilliseconds = 0.0f;
		DrawMilliseconds = 0.0f;
		bBudgetCoarsened = false;
	}
};
