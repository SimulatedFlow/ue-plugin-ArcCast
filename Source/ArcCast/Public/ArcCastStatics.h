// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ArcCastTypes.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcCastStatics.generated.h"

class AActor;
class UArcCastProfile;

/**
 * Everything ArcCast can do, without writing a line of C++.
 *
 * The two halves of the plugin are both here on purpose. Predict Arc answers "where would this land?"
 * and draws nothing - it is what an AI, a server-side validator or an ability's range check wants. Show
 * Arc From registers an arc for drawing. A project can use either without ever touching the other.
 */
UCLASS(meta = (DisplayName = "ArcCast"))
class ARCCAST_API UArcCastStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Simulate one arc and hand back the result. Nothing is registered and nothing is drawn.
	 *
	 * Bounces, landing verdict, splash ring points and flight time all come back in the path, on the
	 * calling frame. This is the function to call from an AI that wants to know whether a grenade would
	 * actually reach the cover it is aiming at.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IgnoredActors", AdvancedDisplay = "IgnoredActors,MaxRange"))
	static FArcCastPath PredictArc(
		const UObject* WorldContextObject,
		FVector StartLocation,
		FVector LaunchVelocity,
		UArcCastProfile* Profile,
		const TArray<AActor*>& IgnoredActors,
		float MaxRange = 0.0f);

	/** The same prediction, expressed as a direction and a speed instead of a velocity. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IgnoredActors", AdvancedDisplay = "IgnoredActors,MaxRange"))
	static FArcCastPath PredictArcFromDirection(
		const UObject* WorldContextObject,
		FVector StartLocation,
		FVector Direction,
		float LaunchSpeed,
		UArcCastProfile* Profile,
		const TArray<AActor*>& IgnoredActors,
		float MaxRange = 0.0f);

	/**
	 * Register an arc for drawing and keep it updated until it is hidden.
	 *
	 * Pass an ArcId of 0 to be given a fresh one, and feed that id back in on later frames to move the
	 * same arc rather than piling up new ones.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "IgnoredActors", AdvancedDisplay = "IgnoredActors,MaxRange,ArcId"))
	static int32 ShowArcFrom(
		const UObject* WorldContextObject,
		FVector StartLocation,
		FVector LaunchVelocity,
		UArcCastProfile* Profile,
		const TArray<AActor*>& IgnoredActors,
		float MaxRange = 0.0f,
		int32 ArcId = 0);

	/** Stop drawing the arc under this id. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static void HideArc(const UObject* WorldContextObject, int32 ArcId);

	/** Drop every registered arc in this world. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static void ClearArcs(const UObject* WorldContextObject);

	/** The path currently registered under this id. False when there is none. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static bool GetArc(const UObject* WorldContextObject, int32 ArcId, FArcCastPath& OutPath);

	/** Verdict of the most recently computed path in this world. */
	UFUNCTION(BlueprintPure, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static EArcCastVerdict GetLastVerdict(const UObject* WorldContextObject);

	/** Landing point and surface normal of a path. False when the path holds no result. */
	UFUNCTION(BlueprintPure, Category = "ArcCast", meta = (AutoCreateRefTerm = "Path"))
	static bool GetImpactPointAndNormal(const FArcCastPath& Path, FVector& OutPoint, FVector& OutNormal);

	/** What a path landed on, or null. The path holds this weakly, so it is safe on a stale copy. */
	UFUNCTION(BlueprintPure, Category = "ArcCast", meta = (AutoCreateRefTerm = "Path"))
	static AActor* GetArcImpactActor(const FArcCastPath& Path);

	/** Master switch for the whole plugin. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	static void SetArcCastEnabled(bool bEnabled);

	/** Is the plugin switched on? */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	static bool IsArcCastEnabled();

	/** Last frame's measured cost in this world - the numbers ArcCast.Stats prints. */
	UFUNCTION(BlueprintPure, Category = "ArcCast|Stats", meta = (WorldContext = "WorldContextObject"))
	static FArcCastStats GetArcCastStats(const UObject* WorldContextObject);

	/** Show or hide the on-screen statistics box. Same as ArcCast.Stats 0|1. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast|Stats", meta = (WorldContext = "WorldContextObject"))
	static void SetShowArcCastStats(const UObject* WorldContextObject, bool bShow);

	/** Force a bounce count on every arc in this world, or -1 to hand it back to the profiles. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static void SetBounceOverride(const UObject* WorldContextObject, int32 MaxBounces);

	/** Force a line style on every arc in this world. Same as ArcCast.Style. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static void SetLineStyleOverride(const UObject* WorldContextObject, EArcCastLineStyle Style);

	/** Hand the line style back to each arc's own profile. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static void ClearLineStyleOverride(const UObject* WorldContextObject);

	/** One of the four code-side profiles: Grenade, Teleport, Arrow or Bouncy. Null if unknown. */
	UFUNCTION(BlueprintPure, Category = "ArcCast", meta = (WorldContext = "WorldContextObject"))
	static UArcCastProfile* GetBuiltinProfile(const UObject* WorldContextObject, FName ProfileName);

	/** Readable name of a verdict, for a HUD line. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	static FText VerdictToText(EArcCastVerdict Verdict);
};
