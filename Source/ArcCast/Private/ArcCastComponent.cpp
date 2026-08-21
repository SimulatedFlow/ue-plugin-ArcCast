// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ArcCastComponent.h"

#include "ArcCastProfile.h"
#include "ArcCastSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UArcCastComponent::UArcCastComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// UActorComponent::bTickInEditor, not FTickFunction::bTickInEditor - the tick function has no such
	// member in 5.8, and this is the flag that actually keeps the component running in an editor viewport
	// with no PIE session. Without it the arc is frozen exactly where a designer places the thrower.
	bTickInEditor = true;

	bWantsInitializeComponent = false;
	bAutoActivate = true;
}

void UArcCastComponent::OnRegister()
{
	Super::OnRegister();

	// Registering in OnRegister rather than BeginPlay is what makes the preview appear the moment the
	// component is dropped onto an actor in the editor, with no play session.
	if (bPreviewEnabled)
	{
		RefreshArc();
	}
}

void UArcCastComponent::OnUnregister()
{
	ReleaseArc();
	Super::OnUnregister();
}

void UArcCastComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bPreviewEnabled)
	{
		RefreshArc();
	}
}

void UArcCastComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseArc();
	Super::EndPlay(EndPlayReason);
}

void UArcCastComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPreviewEnabled)
	{
		return;
	}

	RefreshArc();
}

FVector UArcCastComponent::GetLaunchLocation() const
{
	const FVector Direction = GetLaunchVelocity().GetSafeNormal();
	return GetComponentLocation() + Direction * FMath::Max(0.0f, LaunchOffset);
}

FVector UArcCastComponent::GetLaunchVelocity() const
{
	FVector Direction = AimDirectionOverride.IsNearlyZero()
		? GetForwardVector()
		: AimDirectionOverride.GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	float Speed = LaunchSpeedOverride;
	if (Speed <= 0.0f)
	{
		const UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(this);
		const UArcCastProfile* Resolved = Subsystem ? Subsystem->ResolveProfile(Profile) : Profile.Get();
		Speed = Resolved ? Resolved->LaunchSpeed : 1400.0f;
	}

	return Direction * Speed;
}

FArcCastRequest UArcCastComponent::BuildRequest() const
{
	FArcCastRequest Request;
	Request.LaunchVelocity = GetLaunchVelocity();
	Request.StartLocation = GetLaunchLocation();
	Request.Profile = Profile;
	Request.MaxBouncesOverride = MaxBouncesOverride;
	Request.MaxRange = MaxRange;

	// The owner goes in without being asked. A preview that begins by hitting the character holding it is
	// the single most common way this kind of feature is wired up wrong.
	if (AActor* Owner = GetOwner())
	{
		Request.IgnoredActors.Add(Owner);
	}

	for (const TObjectPtr<AActor>& Extra : AdditionalIgnoredActors)
	{
		if (Extra)
		{
			Request.IgnoredActors.AddUnique(Extra);
		}
	}

	return Request;
}

void UArcCastComponent::RefreshArc()
{
	UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	ArcId = Subsystem->ShowArc(ArcId, BuildRequest());
	Subsystem->GetArc(ArcId, CachedPath);

	if (!bHasBroadcastVerdict || CachedPath.Verdict != LastBroadcastVerdict)
	{
		LastBroadcastVerdict = CachedPath.Verdict;
		bHasBroadcastVerdict = true;
		OnVerdictChanged.Broadcast(LastBroadcastVerdict);
	}

	AActor* ImpactActor = CachedPath.GetImpactActor();
	if (!bHasBroadcastImpactActor || LastBroadcastImpactActor.Get() != ImpactActor)
	{
		LastBroadcastImpactActor = ImpactActor;
		bHasBroadcastImpactActor = true;
		OnImpactActorChanged.Broadcast(ImpactActor);
	}
}

void UArcCastComponent::ReleaseArc()
{
	if (ArcId == 0)
	{
		return;
	}

	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(this))
	{
		Subsystem->HideArc(ArcId);
	}

	ArcId = 0;
	CachedPath.Reset();
}

void UArcCastComponent::SetPreviewEnabled(bool bInEnabled)
{
	if (bPreviewEnabled == bInEnabled)
	{
		return;
	}

	bPreviewEnabled = bInEnabled;

	if (bPreviewEnabled)
	{
		RefreshArc();
	}
	else
	{
		ReleaseArc();
	}
}

void UArcCastComponent::SetProfile(UArcCastProfile* InProfile)
{
	Profile = InProfile;
}

void UArcCastComponent::SetAimDirection(FVector InWorldDirection)
{
	AimDirectionOverride = InWorldDirection;
}

void UArcCastComponent::SetLaunchSpeedOverride(float InSpeed)
{
	LaunchSpeedOverride = FMath::Max(0.0f, InSpeed);
}

void UArcCastComponent::SetMaxBouncesOverride(int32 InBounces)
{
	MaxBouncesOverride = InBounces < 0 ? -1 : InBounces;
}

bool UArcCastComponent::GetImpactPointAndNormal(FVector& OutPoint, FVector& OutNormal) const
{
	OutPoint = CachedPath.ImpactPoint;
	OutNormal = CachedPath.ImpactNormal;
	return CachedPath.bHasResult;
}
