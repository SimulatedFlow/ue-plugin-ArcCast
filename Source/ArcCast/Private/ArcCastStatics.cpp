// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ArcCastStatics.h"

#include "ArcCastProfile.h"
#include "ArcCastSettings.h"
#include "ArcCastSubsystem.h"

#define LOCTEXT_NAMESPACE "ArcCast"

namespace ArcCastStaticsLocal
{
	static FArcCastRequest MakeRequest(
		FVector StartLocation,
		FVector LaunchVelocity,
		UArcCastProfile* Profile,
		const TArray<AActor*>& IgnoredActors,
		float MaxRange)
	{
		FArcCastRequest Request;
		Request.StartLocation = StartLocation;
		Request.LaunchVelocity = LaunchVelocity;
		Request.Profile = Profile;
		Request.MaxRange = FMath::Max(0.0f, MaxRange);

		Request.IgnoredActors.Reserve(IgnoredActors.Num());
		for (AActor* Actor : IgnoredActors)
		{
			if (Actor)
			{
				Request.IgnoredActors.AddUnique(Actor);
			}
		}

		return Request;
	}
}

FArcCastPath UArcCastStatics::PredictArc(
	const UObject* WorldContextObject,
	FVector StartLocation,
	FVector LaunchVelocity,
	UArcCastProfile* Profile,
	const TArray<AActor*>& IgnoredActors,
	float MaxRange)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		return Subsystem->SimulateArc(
			ArcCastStaticsLocal::MakeRequest(StartLocation, LaunchVelocity, Profile, IgnoredActors, MaxRange));
	}

	return FArcCastPath();
}

FArcCastPath UArcCastStatics::PredictArcFromDirection(
	const UObject* WorldContextObject,
	FVector StartLocation,
	FVector Direction,
	float LaunchSpeed,
	UArcCastProfile* Profile,
	const TArray<AActor*>& IgnoredActors,
	float MaxRange)
{
	UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject);
	if (!Subsystem)
	{
		return FArcCastPath();
	}

	float Speed = LaunchSpeed;
	if (Speed <= 0.0f)
	{
		const UArcCastProfile* Resolved = Subsystem->ResolveProfile(Profile);
		Speed = Resolved ? Resolved->LaunchSpeed : 1400.0f;
	}

	const FVector Velocity = Direction.GetSafeNormal() * Speed;
	return Subsystem->SimulateArc(
		ArcCastStaticsLocal::MakeRequest(StartLocation, Velocity, Profile, IgnoredActors, MaxRange));
}

int32 UArcCastStatics::ShowArcFrom(
	const UObject* WorldContextObject,
	FVector StartLocation,
	FVector LaunchVelocity,
	UArcCastProfile* Profile,
	const TArray<AActor*>& IgnoredActors,
	float MaxRange,
	int32 ArcId)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		return Subsystem->ShowArc(
			ArcId,
			ArcCastStaticsLocal::MakeRequest(StartLocation, LaunchVelocity, Profile, IgnoredActors, MaxRange));
	}

	return 0;
}

void UArcCastStatics::HideArc(const UObject* WorldContextObject, int32 ArcId)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		Subsystem->HideArc(ArcId);
	}
}

void UArcCastStatics::ClearArcs(const UObject* WorldContextObject)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		Subsystem->ClearArcs();
	}
}

bool UArcCastStatics::GetArc(const UObject* WorldContextObject, int32 ArcId, FArcCastPath& OutPath)
{
	if (const UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		return Subsystem->GetArc(ArcId, OutPath);
	}

	OutPath.Reset();
	return false;
}

EArcCastVerdict UArcCastStatics::GetLastVerdict(const UObject* WorldContextObject)
{
	if (const UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		return Subsystem->GetLastVerdict();
	}

	return EArcCastVerdict::NoGround;
}

bool UArcCastStatics::GetImpactPointAndNormal(const FArcCastPath& Path, FVector& OutPoint, FVector& OutNormal)
{
	OutPoint = Path.ImpactPoint;
	OutNormal = Path.ImpactNormal;
	return Path.bHasResult;
}

AActor* UArcCastStatics::GetArcImpactActor(const FArcCastPath& Path)
{
	return Path.GetImpactActor();
}

void UArcCastStatics::SetArcCastEnabled(bool bEnabled)
{
	UArcCastSettings::SetEnabled(bEnabled);
}

bool UArcCastStatics::IsArcCastEnabled()
{
	return UArcCastSettings::Get().bEnabled;
}

FArcCastStats UArcCastStatics::GetArcCastStats(const UObject* WorldContextObject)
{
	if (const UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		return Subsystem->GetStats();
	}

	return FArcCastStats();
}

void UArcCastStatics::SetShowArcCastStats(const UObject* WorldContextObject, bool bShow)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetShowStats(bShow);
	}
}

void UArcCastStatics::SetBounceOverride(const UObject* WorldContextObject, int32 MaxBounces)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetBounceOverride(MaxBounces);
	}
}

void UArcCastStatics::SetLineStyleOverride(const UObject* WorldContextObject, EArcCastLineStyle Style)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		Subsystem->SetStyleOverride(Style);
	}
}

void UArcCastStatics::ClearLineStyleOverride(const UObject* WorldContextObject)
{
	if (UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		Subsystem->ClearStyleOverride();
	}
}

UArcCastProfile* UArcCastStatics::GetBuiltinProfile(const UObject* WorldContextObject, FName ProfileName)
{
	if (const UArcCastSubsystem* Subsystem = UArcCastSubsystem::Get(WorldContextObject))
	{
		return const_cast<UArcCastProfile*>(Subsystem->GetBuiltinProfile(ProfileName));
	}

	return nullptr;
}

FText UArcCastStatics::VerdictToText(EArcCastVerdict Verdict)
{
	switch (Verdict)
	{
	case EArcCastVerdict::Valid:
		return LOCTEXT("VerdictValid", "Valid");
	case EArcCastVerdict::TooSteep:
		return LOCTEXT("VerdictTooSteep", "Too steep");
	case EArcCastVerdict::Blocked:
		return LOCTEXT("VerdictBlocked", "Blocked");
	case EArcCastVerdict::OutOfRange:
		return LOCTEXT("VerdictOutOfRange", "Out of range");
	case EArcCastVerdict::NoGround:
	default:
		return LOCTEXT("VerdictNoGround", "No ground");
	}
}

#undef LOCTEXT_NAMESPACE
