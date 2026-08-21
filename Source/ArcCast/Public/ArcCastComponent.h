// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "ArcCastTypes.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "ArcCastComponent.generated.h"

class UArcCastProfile;

/** Broadcast when the landing verdict changes - not every frame, only on a change. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcCastVerdictChangedSignature, EArcCastVerdict, NewVerdict);

/** Broadcast when the arc starts or stops landing on a different actor. May carry null. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcCastImpactActorChangedSignature, AActor*, NewImpactActor);

/**
 * The five-minute path: hang this on a hand, a weapon or a socket, give it a profile, switch the preview
 * on. It aims down its own forward vector and keeps an arc registered with the subsystem for as long as
 * the preview is enabled.
 *
 * The two events are why this is not just a convenience wrapper. A crosshair that has to recolour when
 * the landing goes from valid to too steep would otherwise poll the verdict every frame from Blueprint,
 * which is both wasteful and the sort of thing that quietly gets left in a shipped build. On Verdict
 * Changed fires once, when it changes.
 *
 * Ticks in the editor on purpose: bTickInEditor is set in the constructor, so an arc can be aimed and
 * framed in the viewport with no PIE session running.
 */
UCLASS(ClassGroup = (ArcCast), meta = (BlueprintSpawnableComponent, DisplayName = "ArcCast"))
class ARCCAST_API UArcCastComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UArcCastComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ----------------------------------------------------------------------------------------------
	// Setup
	// ----------------------------------------------------------------------------------------------

	/** Flight behaviour and appearance. Empty falls back to the project's default profile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArcCast")
	TObjectPtr<UArcCastProfile> Profile;

	/** Show the arc. Off unregisters it immediately - nothing is simulated and nothing is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ArcCast")
	bool bPreviewEnabled = true;

	/**
	 * Aim along this world-space direction instead of the component's own forward vector.
	 *
	 * Left at zero the component aims where it points, which is what a socket on a hand is for. Set it
	 * when the aim comes from somewhere else - the camera, a controller, an AI's chosen throw solution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	FVector AimDirectionOverride = FVector::ZeroVector;

	/** Above zero: use this launch speed in cm/s instead of the profile's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast", meta = (ClampMin = "0.0", UIMax = "6000.0"))
	float LaunchSpeedOverride = 0.0f;

	/** Below zero: use the profile's bounce count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast", meta = (ClampMin = "-1", UIMax = "8"))
	int32 MaxBouncesOverride = -1;

	/** Path length in cm past which the landing is reported OutOfRange. Zero means unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast", meta = (ClampMin = "0.0", UIMax = "20000.0"))
	float MaxRange = 0.0f;

	/** Also ignore these actors, on top of the owner - a held weapon, a vehicle, a shield. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast")
	TArray<TObjectPtr<AActor>> AdditionalIgnoredActors;

	/** Launch offset along the aim direction in cm, so the arc does not start inside the thrower's chest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCast", meta = (ClampMin = "0.0", UIMax = "500.0"))
	float LaunchOffset = 0.0f;

	// ----------------------------------------------------------------------------------------------
	// Events
	// ----------------------------------------------------------------------------------------------

	/** The landing verdict changed. Bind this instead of polling Get Verdict every frame. */
	UPROPERTY(BlueprintAssignable, Category = "ArcCast")
	FArcCastVerdictChangedSignature OnVerdictChanged;

	/** The arc started or stopped landing on a different actor. Carries null when it lands on nothing. */
	UPROPERTY(BlueprintAssignable, Category = "ArcCast")
	FArcCastImpactActorChangedSignature OnImpactActorChanged;

	// ----------------------------------------------------------------------------------------------
	// Runtime
	// ----------------------------------------------------------------------------------------------

	/** Show or hide this component's arc. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void SetPreviewEnabled(bool bInEnabled);

	/** Is the preview showing? */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	bool IsPreviewEnabled() const { return bPreviewEnabled; }

	/** Swap the profile. Takes effect on the next tick. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void SetProfile(UArcCastProfile* InProfile);

	/** Aim along a world-space direction. Pass a zero vector to go back to the component's forward vector. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void SetAimDirection(FVector InWorldDirection);

	/** Launch speed override in cm/s. Zero or less hands it back to the profile. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void SetLaunchSpeedOverride(float InSpeed);

	/** Bounce count override. Below zero hands it back to the profile. */
	UFUNCTION(BlueprintCallable, Category = "ArcCast")
	void SetMaxBouncesOverride(int32 InBounces);

	/** The path as of the last tick. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	FArcCastPath GetPath() const { return CachedPath; }

	/** The landing verdict as of the last tick. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	EArcCastVerdict GetVerdict() const { return CachedPath.Verdict; }

	/** Landing point and surface normal as of the last tick. False when nothing has been simulated. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	bool GetImpactPointAndNormal(FVector& OutPoint, FVector& OutNormal) const;

	/** What the arc currently lands on, or null. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	AActor* GetImpactActor() const { return CachedPath.GetImpactActor(); }

	/** The launch velocity this component would use right now - hand it straight to your throwing code. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	FVector GetLaunchVelocity() const;

	/** The launch point this component would use right now. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	FVector GetLaunchLocation() const;

	/** The id this component's arc is registered under, or 0 when the preview is off. */
	UFUNCTION(BlueprintPure, Category = "ArcCast")
	int32 GetArcId() const { return ArcId; }

private:
	/** Assemble the request from the component's own state. Shared by the tick and the two pure getters. */
	FArcCastRequest BuildRequest() const;

	/** Register or refresh the arc, then fire the two events if anything changed. */
	void RefreshArc();

	/** Drop the arc from the subsystem. Safe to call when nothing is registered. */
	void ReleaseArc();

	/** Id handed out by the subsystem. 0 means "not registered". */
	int32 ArcId = 0;

	FArcCastPath CachedPath;

	EArcCastVerdict LastBroadcastVerdict = EArcCastVerdict::NoGround;
	bool bHasBroadcastVerdict = false;

	TWeakObjectPtr<AActor> LastBroadcastImpactActor;
	bool bHasBroadcastImpactActor = false;
};
