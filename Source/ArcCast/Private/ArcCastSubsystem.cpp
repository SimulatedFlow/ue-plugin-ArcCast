// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ArcCastSubsystem.h"

#include "ArcCastLog.h"
#include "ArcCastProfile.h"
#include "ArcCastSettings.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CollisionQueryParams.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GlobalRenderResources.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "SceneView.h"
#include "WorldCollision.h"

// -------------------------------------------------------------------------------------------------------
// Console commands
// -------------------------------------------------------------------------------------------------------

namespace ArcCastConsole
{
	static void ForEachSubsystem(UWorld* World, TFunctionRef<void(UArcCastSubsystem&)> Func)
	{
		TArray<UArcCastSubsystem*, TInlineAllocator<4>> Found;

		if (World)
		{
			if (UArcCastSubsystem* Subsystem = World->GetSubsystem<UArcCastSubsystem>())
			{
				Found.AddUnique(Subsystem);
			}
		}

		// A console command typed into an editor viewport arrives with no world at all. Falling back to
		// every world context is what makes ArcCast.Occlusion 0 work while the level is being built,
		// which is the situation the switches are most useful in.
		if (Found.Num() == 0 && GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World())
				{
					if (UArcCastSubsystem* Subsystem = Context.World()->GetSubsystem<UArcCastSubsystem>())
					{
						Found.AddUnique(Subsystem);
					}
				}
			}
		}

		for (UArcCastSubsystem* Subsystem : Found)
		{
			Func(*Subsystem);
		}
	}

	static bool ParseBool(const TArray<FString>& Args, bool bDefault)
	{
		if (Args.Num() == 0)
		{
			return bDefault;
		}
		return Args[0].ToBool() || Args[0] == TEXT("1");
	}

	static FAutoConsoleCommandWithWorldAndArgs GTest(
		TEXT("ArcCast.Test"),
		TEXT("ArcCast.Test - throw a demonstration arc from the current view with the default profile."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachSubsystem(World, [](UArcCastSubsystem& Subsystem)
			{
				const int32 Id = Subsystem.SpawnTestArc();
				if (Id == 0)
				{
					UE_LOG(LogArcCast, Warning,
						TEXT("ArcCast.Test: no view seen yet. The arc is thrown from the camera, so the viewport has to have drawn at least one frame."));
				}
			});
		}));

	static FAutoConsoleCommandWithWorldAndArgs GBounces(
		TEXT("ArcCast.Bounces"),
		TEXT("ArcCast.Bounces <n> - force the bounce count on every arc, or -1 to hand it back to the profiles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Bounces = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
			ForEachSubsystem(World, [Bounces](UArcCastSubsystem& Subsystem) { Subsystem.SetBounceOverride(Bounces); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GOcclusion(
		TEXT("ArcCast.Occlusion"),
		TEXT("ArcCast.Occlusion 0|1 - camera-to-arc occlusion traces. Off, the occlusion trace count in ArcCast.Stats drops to zero."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bOn = ParseBool(Args, true);
			ForEachSubsystem(World, [bOn](UArcCastSubsystem& Subsystem) { Subsystem.SetOcclusionAware(bOn); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStyle(
		TEXT("ArcCast.Style"),
		TEXT("ArcCast.Style solid|dashed|dots|clear - force a line style on every arc, or hand it back to the profiles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0 || Args[0].Equals(TEXT("clear"), ESearchCase::IgnoreCase))
			{
				ForEachSubsystem(World, [](UArcCastSubsystem& Subsystem) { Subsystem.ClearStyleOverride(); });
				return;
			}

			EArcCastLineStyle Style = EArcCastLineStyle::Solid;
			if (Args[0].Equals(TEXT("dashed"), ESearchCase::IgnoreCase))
			{
				Style = EArcCastLineStyle::Dashed;
			}
			else if (Args[0].Equals(TEXT("dots"), ESearchCase::IgnoreCase))
			{
				Style = EArcCastLineStyle::Dots;
			}
			else if (!Args[0].Equals(TEXT("solid"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogArcCast, Warning, TEXT("ArcCast.Style: unknown style '%s'. Use solid, dashed, dots or clear."), *Args[0]);
				return;
			}

			ForEachSubsystem(World, [Style](UArcCastSubsystem& Subsystem) { Subsystem.SetStyleOverride(Style); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GClear(
		TEXT("ArcCast.Clear"),
		TEXT("ArcCast.Clear - drop every registered arc."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachSubsystem(World, [](UArcCastSubsystem& Subsystem) { Subsystem.ClearArcs(); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GStats(
		TEXT("ArcCast.Stats"),
		TEXT("ArcCast.Stats [0|1] - print the measured cost, and with an argument show or hide the on-screen box."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0)
			{
				ForEachSubsystem(World, [](UArcCastSubsystem& Subsystem) { Subsystem.LogStats(); });
				return;
			}
			const bool bShow = ParseBool(Args, true);
			ForEachSubsystem(World, [bShow](UArcCastSubsystem& Subsystem) { Subsystem.SetShowStats(bShow); });
		}));
}

// -------------------------------------------------------------------------------------------------------
// Lifetime
// -------------------------------------------------------------------------------------------------------

UArcCastSubsystem* UArcCastSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UArcCastSubsystem>() : nullptr;
}

void UArcCastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CreateBuiltinProfiles();

	// Applies the settings, binds the AHUD hook and registers the editor-viewport second path.
	ApplySettings();
}

void UArcCastSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (EditorDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(EditorDrawHandle);
		EditorDrawHandle.Reset();
	}
#endif

	if (HudPostRenderHandle.IsValid())
	{
		AHUD::OnHUDPostRender.Remove(HudPostRenderHandle);
		HudPostRenderHandle.Reset();
	}

	Arcs.Reset();
	BuiltinProfiles.Reset();
	ResolvedDefaultProfile = nullptr;
	LastPath.Reset();

	Super::Deinitialize();
}

bool UArcCastSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Editor is in here on purpose. An aiming preview that only exists once the game is running cannot be
	// placed, framed or filmed while the level is being built - and that is half of what a designer does
	// with it.
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

TStatId UArcCastSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcCastSubsystem, STATGROUP_Tickables);
}

void UArcCastSubsystem::RebindHudDelegate()
{
	const bool bWantBound = bEnabled && bAutoHUDDraw;

	if (bWantBound && !HudPostRenderHandle.IsValid())
	{
		HudPostRenderHandle = AHUD::OnHUDPostRender.AddUObject(this, &UArcCastSubsystem::OnAnyHUDPostRender);
	}
	else if (!bWantBound && HudPostRenderHandle.IsValid())
	{
		AHUD::OnHUDPostRender.Remove(HudPostRenderHandle);
		HudPostRenderHandle.Reset();
	}
}

void UArcCastSubsystem::ApplySettings()
{
	const UArcCastSettings& Settings = UArcCastSettings::Get();

	bEnabled = Settings.bEnabled;
	bOcclusionAware = Settings.bOcclusionAware;
	OccludedOpacity = FMath::Clamp(Settings.OccludedOpacity, 0.0f, 1.0f);
	OcclusionEveryNthPoint = FMath::Max(1, Settings.OcclusionEveryNthPoint);
	OcclusionEndOffset = FMath::Max(0.0f, Settings.OcclusionEndOffset);
	MaxSimStepsPerFrame = FMath::Max(16, Settings.MaxSimStepsPerFrame);
	MaxPointsPerArc = FMath::Max(8, Settings.MaxPointsPerArc);
	GlobalOpacity = FMath::Clamp(Settings.GlobalOpacity, 0.0f, 1.0f);
	bAutoHUDDraw = Settings.bAutoSpawnHUDComponent;
	bDrawInEditorViewport = Settings.bDrawInEditorViewport;
	bShowStats = bShowStats || Settings.bShowStatsByDefault;

	// The default profile is a soft pointer so a project that never uses one never loads one. Resolving it
	// here, once, keeps the per-frame path free of synchronous loads.
	ResolvedDefaultProfile = Settings.DefaultProfile.IsNull() ? nullptr : Settings.DefaultProfile.LoadSynchronous();

	RebindHudDelegate();

#if WITH_EDITOR
	if (bDrawInEditorViewport && !EditorDrawHandle.IsValid())
	{
		EditorDrawHandle = UDebugDrawService::Register(
			TEXT("Game"),
			FDebugDrawDelegate::CreateUObject(this, &UArcCastSubsystem::OnEditorViewportDraw));
	}
	else if (!bDrawInEditorViewport && EditorDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(EditorDrawHandle);
		EditorDrawHandle.Reset();
	}
#endif
}

// -------------------------------------------------------------------------------------------------------
// Built-in profiles
// -------------------------------------------------------------------------------------------------------

void UArcCastSubsystem::CreateBuiltinProfiles()
{
	BuiltinProfiles.Reset();

	auto MakeProfile = [this](FName Name) -> UArcCastProfile*
	{
		UArcCastProfile* Profile = NewObject<UArcCastProfile>(this, UArcCastProfile::StaticClass(), Name, RF_Transient);
		BuiltinProfiles.Add(Name, Profile);
		return Profile;
	};

	// Grenade: slow, bounces twice, has a blast radius. The archetype the plugin is named after.
	if (UArcCastProfile* Grenade = MakeProfile(TEXT("Grenade")))
	{
		Grenade->LaunchSpeed = 1200.0f;
		Grenade->ProjectileRadius = 9.0f;
		Grenade->MaxBounces = 2;
		Grenade->Restitution = 0.45f;
		Grenade->Friction = 0.2f;
		Grenade->MaxSimTime = 4.0f;
		Grenade->SplashRadius = 320.0f;
		Grenade->bDrawRing = true;
		Grenade->LineStyle = EArcCastLineStyle::Solid;
	}

	// Teleport: fast, no bounce, no ring, and the landing point has to be on the navigation mesh. This is
	// the ability / VR target indicator, and the only difference from the grenade is four numbers.
	if (UArcCastProfile* Teleport = MakeProfile(TEXT("Teleport")))
	{
		Teleport->LaunchSpeed = 1800.0f;
		Teleport->ProjectileRadius = 12.0f;
		Teleport->MaxBounces = 0;
		Teleport->MaxSimTime = 3.0f;
		Teleport->SplashRadius = 0.0f;
		Teleport->bDrawRing = false;
		Teleport->bRequireNavMesh = true;
		Teleport->bDrawEndMarker = true;
		Teleport->EndMarkerSize = 22.0f;
		Teleport->LineStyle = EArcCastLineStyle::Dashed;
		Teleport->DashScrollSpeed = 90.0f;
	}

	// Arrow: fast and flat, stops where it sticks, and marks the spot rather than ringing it.
	if (UArcCastProfile* Arrow = MakeProfile(TEXT("Arrow")))
	{
		Arrow->LaunchSpeed = 4000.0f;
		Arrow->GravityScale = 0.7f;
		Arrow->ProjectileRadius = 2.0f;
		Arrow->MaxBounces = 0;
		Arrow->MaxSimTime = 2.5f;
		Arrow->SplashRadius = 0.0f;
		Arrow->bDrawRing = false;
		Arrow->bDrawEndMarker = true;
		Arrow->EndMarkerSize = 12.0f;
		Arrow->LineThickness = 2.0f;
		Arrow->LineStyle = EArcCastLineStyle::Solid;
		Arrow->FixedStepSeconds = 1.0f / 60.0f;
	}

	// Bouncy: five bounces and almost no loss, which is what the per-bounce fade was written for.
	if (UArcCastProfile* Bouncy = MakeProfile(TEXT("Bouncy")))
	{
		Bouncy->LaunchSpeed = 1500.0f;
		Bouncy->ProjectileRadius = 14.0f;
		Bouncy->MaxBounces = 5;
		Bouncy->Restitution = 0.8f;
		Bouncy->Friction = 0.05f;
		Bouncy->MaxSimTime = 6.0f;
		Bouncy->SplashRadius = 200.0f;
		Bouncy->bDrawRing = true;
		Bouncy->BounceFadePerHit = 0.72f;
		Bouncy->LineStyle = EArcCastLineStyle::Dots;
	}
}

const UArcCastProfile* UArcCastSubsystem::GetBuiltinProfile(FName ProfileName) const
{
	const TObjectPtr<UArcCastProfile>* Found = BuiltinProfiles.Find(ProfileName);
	return Found ? Found->Get() : nullptr;
}

const UArcCastProfile* UArcCastSubsystem::ResolveProfile(const UArcCastProfile* Requested) const
{
	if (Requested)
	{
		return Requested;
	}

	if (ResolvedDefaultProfile)
	{
		return ResolvedDefaultProfile;
	}

	const UArcCastSettings& Settings = UArcCastSettings::Get();
	if (const UArcCastProfile* Builtin = GetBuiltinProfile(Settings.DefaultBuiltinProfile))
	{
		return Builtin;
	}

	// Last resort so no caller ever has to null-check a profile: the class default object. Its values are
	// the same ones the Grenade profile starts from.
	return GetDefault<UArcCastProfile>();
}

// -------------------------------------------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------------------------------------------

float UArcCastSubsystem::ComputeBudgetedStep() const
{
	// Estimate the frame's cost before paying it: every arc's flight time over its own step size. If the
	// total runs past the budget the step is scaled up for every arc at once, so a busy frame spends
	// accuracy instead of milliseconds. Nothing is dropped and nothing is deferred - a preview that
	// stutters or vanishes under load is worse than one that is slightly coarser.
	float LongestTime = 0.0f;
	float SmallestStep = MAX_flt;
	int32 EstimatedSteps = 0;

	for (const TPair<int32, FActiveArc>& Pair : Arcs)
	{
		const UArcCastProfile* Profile = ResolveProfile(Pair.Value.Request.Profile);
		if (!Profile)
		{
			continue;
		}

		const float MaxTime = Pair.Value.Request.MaxSimTimeOverride > 0.0f
			? Pair.Value.Request.MaxSimTimeOverride
			: Profile->MaxSimTime;
		const float Step = FMath::Max(0.002f, Profile->FixedStepSeconds);

		LongestTime = FMath::Max(LongestTime, MaxTime);
		SmallestStep = FMath::Min(SmallestStep, Step);
		EstimatedSteps += FMath::CeilToInt(MaxTime / Step);
	}

	if (EstimatedSteps <= 0 || SmallestStep == MAX_flt)
	{
		return 1.0f / 30.0f;
	}

	if (EstimatedSteps <= MaxSimStepsPerFrame)
	{
		return SmallestStep;
	}

	const float Scale = static_cast<float>(EstimatedSteps) / static_cast<float>(FMath::Max(1, MaxSimStepsPerFrame));
	return SmallestStep * Scale;
}

void UArcCastSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Stats.ResetFrame();
	Stats.ActiveArcs = Arcs.Num();

	if (!bEnabled || Arcs.Num() == 0)
	{
		Stats.EffectiveStepSeconds = 0.0f;
		return;
	}

	// Advanced here rather than from a render callback so the dashes also scroll in an editor viewport,
	// where no game time is running.
	DashPhase += DeltaTime * 60.0f;

	const float BudgetedStep = ComputeBudgetedStep();
	Stats.EffectiveStepSeconds = BudgetedStep;

	const double SimStart = FPlatformTime::Seconds();

	FSimCounters Counters;
	for (TPair<int32, FActiveArc>& Pair : Arcs)
	{
		FActiveArc& Arc = Pair.Value;

		const UArcCastProfile* Profile = ResolveProfile(Arc.Request.Profile);
		const float ProfileStep = Profile ? FMath::Max(0.002f, Profile->FixedStepSeconds) : (1.0f / 30.0f);
		const float Step = FMath::Max(ProfileStep, BudgetedStep);

		Stats.bBudgetCoarsened |= (Step > ProfileStep + UE_KINDA_SMALL_NUMBER);

		SimulateInternal(Arc.Request, Step, Arc.Path, Counters);
		Arc.Path.Id = Pair.Key;

		LastPath = Arc.Path;
		LastVerdict = Arc.Path.Verdict;
	}

	Stats.SimMilliseconds = static_cast<float>((FPlatformTime::Seconds() - SimStart) * 1000.0);
	Stats.SimSteps = Counters.Steps;
	Stats.SimTraces = Counters.SimTraces;
	Stats.RingTraces = Counters.RingTraces;
	Stats.TotalTraces = Counters.SimTraces + Counters.RingTraces;
}

// -------------------------------------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------------------------------------

FArcCastPath UArcCastSubsystem::SimulateArc(const FArcCastRequest& Request)
{
	FArcCastPath Path;

	if (!bEnabled)
	{
		return Path;
	}

	const UArcCastProfile* Profile = ResolveProfile(Request.Profile);
	const float Step = Profile ? FMath::Max(0.002f, Profile->FixedStepSeconds) : (1.0f / 30.0f);

	FSimCounters Counters;
	SimulateInternal(Request, Step, Path, Counters);

	LastPath = Path;
	LastVerdict = Path.Verdict;

	return Path;
}

int32 UArcCastSubsystem::ShowArc(int32 Id, const FArcCastRequest& Request)
{
	if (Id <= 0)
	{
		Id = NextArcId++;
	}

	FActiveArc& Arc = Arcs.FindOrAdd(Id);
	Arc.Request = Request;

	// Simulate straight away so the first frame after ShowArc already has something to draw, and so a
	// caller that only wants the verdict does not have to wait a tick for it.
	const UArcCastProfile* Profile = ResolveProfile(Request.Profile);
	const float Step = Profile ? FMath::Max(0.002f, Profile->FixedStepSeconds) : (1.0f / 30.0f);

	FSimCounters Counters;
	SimulateInternal(Arc.Request, Step, Arc.Path, Counters);
	Arc.Path.Id = Id;

	LastPath = Arc.Path;
	LastVerdict = Arc.Path.Verdict;

	return Id;
}

void UArcCastSubsystem::HideArc(int32 Id)
{
	Arcs.Remove(Id);

	if (TestArcId == Id)
	{
		TestArcId = 0;
	}
}

void UArcCastSubsystem::ClearArcs()
{
	Arcs.Reset();
	TestArcId = 0;
	Stats.ActiveArcs = 0;
}

bool UArcCastSubsystem::GetArc(int32 Id, FArcCastPath& OutPath) const
{
	if (const FActiveArc* Arc = Arcs.Find(Id))
	{
		OutPath = Arc->Path;
		return true;
	}

	OutPath.Reset();
	return false;
}

int32 UArcCastSubsystem::SpawnTestArc()
{
	if (CachedViewForward.IsNearlyZero())
	{
		return 0;
	}

	const UArcCastProfile* Profile = ResolveProfile(nullptr);

	FArcCastRequest Request;
	Request.StartLocation = CachedViewLocation + CachedViewForward * 80.0f;
	Request.LaunchVelocity = CachedViewForward * (Profile ? Profile->LaunchSpeed : 1400.0f);

	TestArcId = ShowArc(TestArcId, Request);
	return TestArcId;
}

// -------------------------------------------------------------------------------------------------------
// Simulation
// -------------------------------------------------------------------------------------------------------

void UArcCastSubsystem::SimulateInternal(const FArcCastRequest& Request, float StepSeconds, FArcCastPath& OutPath, FSimCounters& Counters) const
{
	OutPath.Reset();

	const UWorld* World = GetWorld();
	const UArcCastProfile* ProfilePtr = ResolveProfile(Request.Profile);
	if (!World || !ProfilePtr)
	{
		return;
	}

	const UArcCastProfile& Profile = *ProfilePtr;

	const float MaxTime = Request.MaxSimTimeOverride > 0.0f ? Request.MaxSimTimeOverride : Profile.MaxSimTime;
	const int32 MaxBounces = BounceOverride >= 0
		? BounceOverride
		: (Request.MaxBouncesOverride >= 0 ? Request.MaxBouncesOverride : Profile.MaxBounces);

	const float Step = FMath::Max(0.002f, StepSeconds);
	const float Radius = FMath::Max(0.0f, Profile.ProjectileRadius);
	const bool bSphere = Radius > UE_KINDA_SMALL_NUMBER;
	const FCollisionShape Shape = bSphere ? FCollisionShape::MakeSphere(Radius) : FCollisionShape();
	const ECollisionChannel Channel = Profile.CollisionChannel.GetValue();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcCastSimulate), Profile.bTraceComplex);
	for (const TObjectPtr<AActor>& Ignored : Request.IgnoredActors)
	{
		if (Ignored)
		{
			Params.AddIgnoredActor(Ignored.Get());
		}
	}

	const float GravityZ = World->GetGravityZ() * Profile.GravityScale;

	FVector Position = Request.StartLocation;
	FVector Velocity = Request.LaunchVelocity;

	OutPath.Points.Add(Position);
	OutPath.BounceIndex.Add(0);

	int32 Bounce = 0;
	float Time = 0.0f;
	float Distance = 0.0f;
	bool bOutOfRange = false;
	bool bStartedPenetrating = false;
	bool bHadImpact = false;
	FHitResult FinalHit;

	while (Time < MaxTime)
	{
		if (OutPath.Points.Num() >= MaxPointsPerArc)
		{
			break;
		}

		const float Dt = FMath::Min(Step, MaxTime - Time);

		// Semi-implicit Euler: gravity first, then position. It is stable at the step sizes a preview
		// uses and it is what UProjectileMovementComponent does, which matters more than elegance - the
		// preview has to agree with the thing it is previewing.
		FVector NextVelocity = Velocity;
		NextVelocity.Z += GravityZ * Dt;
		const FVector NextPosition = Position + NextVelocity * Dt;

		FHitResult Hit;
		const bool bHit = bSphere
			? World->SweepSingleByChannel(Hit, Position, NextPosition, FQuat::Identity, Channel, Shape, Params)
			: World->LineTraceSingleByChannel(Hit, Position, NextPosition, Channel, Params);

		++Counters.Steps;
		++Counters.SimTraces;
		++OutPath.StepCount;

		if (!bHit)
		{
			Distance += FVector::Dist(Position, NextPosition);
			Position = NextPosition;
			Velocity = NextVelocity;
			Time += Dt;

			OutPath.Points.Add(Position);
			OutPath.BounceIndex.Add(static_cast<uint8>(FMath::Min(Bounce, 255)));

			if (Request.MaxRange > 0.0f && Distance > Request.MaxRange)
			{
				bOutOfRange = true;
				break;
			}
			continue;
		}

		// A sweep that starts inside geometry reports bStartPenetrating and a meaningless location. The
		// honest answer is Blocked at the launch point, not a one-centimetre arc.
		if (Hit.bStartPenetrating)
		{
			bStartedPenetrating = true;
			bHadImpact = true;
			FinalHit = Hit;
			break;
		}

		const FVector HitLocation = Hit.Location;
		Distance += FVector::Dist(Position, HitLocation);
		Time += Dt * FMath::Clamp(static_cast<float>(Hit.Time), 0.0f, 1.0f);

		OutPath.Points.Add(HitLocation);
		OutPath.BounceIndex.Add(static_cast<uint8>(FMath::Min(Bounce, 255)));

		bHadImpact = true;
		FinalHit = Hit;

		if (Request.MaxRange > 0.0f && Distance > Request.MaxRange)
		{
			bOutOfRange = true;
			break;
		}

		if (Bounce >= MaxBounces)
		{
			break;
		}

		// Mirror the velocity about the impact normal, then damp the two halves separately: Restitution
		// takes from the part going into the surface, Friction from the part sliding along it. Two
		// numbers, and they are the difference between a grenade and a rubber ball.
		const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		const FVector NormalVelocity = FVector::DotProduct(NextVelocity, Normal) * Normal;
		const FVector TangentVelocity = NextVelocity - NormalVelocity;
		const FVector OutVelocity = TangentVelocity * (1.0f - FMath::Clamp(Profile.Friction, 0.0f, 1.0f))
			- NormalVelocity * FMath::Clamp(Profile.Restitution, 0.0f, 1.0f);

		++Bounce;

		if (OutVelocity.Size() < Profile.RestSpeed)
		{
			break;
		}

		Velocity = OutVelocity;

		// Nudge off the surface so the next sweep does not immediately re-hit the face we just left. A
		// sphere sweep already reports the centre, so the nudge only has to clear floating point noise.
		Position = HitLocation + Normal * (bSphere ? 0.5f : 1.0f);

		OutPath.Points.Add(Position);
		OutPath.BounceIndex.Add(static_cast<uint8>(FMath::Min(Bounce, 255)));
	}

	OutPath.TotalTime = Time;
	OutPath.TotalDistance = Distance;
	OutPath.BounceCount = Bounce;
	OutPath.bHasResult = OutPath.Points.Num() >= 2;

	if (bHadImpact)
	{
		OutPath.ImpactPoint = FinalHit.ImpactPoint;
		OutPath.ImpactNormal = FinalHit.ImpactNormal.GetSafeNormal();
		OutPath.ImpactActor = FinalHit.GetActor();
	}
	else
	{
		OutPath.ImpactPoint = OutPath.Points.Num() > 0 ? OutPath.Points.Last() : Request.StartLocation;
		OutPath.ImpactNormal = FVector::UpVector;
		OutPath.ImpactActor = nullptr;
	}

	OutPath.Verdict = EvaluateVerdict(Request, Profile, bHadImpact, bOutOfRange, bStartedPenetrating, OutPath, Counters);

	BuildSplashRing(Request, Profile, OutPath, Counters);
}

EArcCastVerdict UArcCastSubsystem::EvaluateVerdict(const FArcCastRequest& Request, const UArcCastProfile& Profile, bool bHadImpact, bool bOutOfRange, bool bStartedPenetrating, FArcCastPath& InOutPath, FSimCounters& Counters) const
{
	if (bStartedPenetrating)
	{
		return EArcCastVerdict::Blocked;
	}

	if (bOutOfRange)
	{
		return EArcCastVerdict::OutOfRange;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return EArcCastVerdict::NoGround;
	}

	FVector GroundPoint = InOutPath.ImpactPoint;
	FVector GroundNormal = InOutPath.ImpactNormal;

	if (!bHadImpact)
	{
		// The arc ran out of time in mid-air. Probe straight down: this is what turns "the preview just
		// stops" into "there is nothing under there", which is the answer a player actually needs.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcCastGroundProbe), Profile.bTraceComplex);
		for (const TObjectPtr<AActor>& Ignored : Request.IgnoredActors)
		{
			if (Ignored)
			{
				Params.AddIgnoredActor(Ignored.Get());
			}
		}

		const FVector ProbeStart = InOutPath.Points.Num() > 0 ? InOutPath.Points.Last() : Request.StartLocation;
		const FVector ProbeEnd = ProbeStart - FVector(0.0f, 0.0f, FMath::Max(0.0f, Profile.GroundProbeDistance));

		FHitResult GroundHit;
		++Counters.RingTraces;
		if (!World->LineTraceSingleByChannel(GroundHit, ProbeStart, ProbeEnd, Profile.CollisionChannel.GetValue(), Params))
		{
			return EArcCastVerdict::NoGround;
		}

		GroundPoint = GroundHit.ImpactPoint;
		GroundNormal = GroundHit.ImpactNormal.GetSafeNormal();

		InOutPath.ImpactPoint = GroundPoint;
		InOutPath.ImpactNormal = GroundNormal;
		InOutPath.ImpactActor = GroundHit.GetActor();
	}

	const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(GroundNormal.Z), -1.0f, 1.0f)));
	if (SlopeDegrees > Profile.MaxWalkableSlopeAngle)
	{
		return EArcCastVerdict::TooSteep;
	}

	if (Profile.bRequireNavMesh)
	{
		// Guarded on purpose. A project with no navigation system - and there are plenty, this is a
		// throwing preview, not an AI plugin - must not crash and must not be told everything is fine.
		UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (!NavSystem)
		{
			return EArcCastVerdict::NoGround;
		}

		FNavLocation Projected;
		if (!NavSystem->ProjectPointToNavigation(GroundPoint, Projected, Profile.NavProjectExtent))
		{
			return EArcCastVerdict::Blocked;
		}

		// Snap the reported landing point to the navigable one, so a teleport that follows the preview
		// arrives exactly where the ring was drawn.
		InOutPath.ImpactPoint = Projected.Location;
	}

	return EArcCastVerdict::Valid;
}

void UArcCastSubsystem::BuildSplashRing(const FArcCastRequest& Request, const UArcCastProfile& Profile, FArcCastPath& InOutPath, FSimCounters& Counters) const
{
	InOutPath.RingPoints.Reset();

	const float RequestedRadius = Request.SplashRadiusOverride >= 0.0f
		? Request.SplashRadiusOverride
		: Profile.GetEffectiveSplashRadius();

	if (RequestedRadius <= UE_KINDA_SMALL_NUMBER || !InOutPath.bHasResult || !Profile.bDrawRing)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 Segments = FMath::Clamp(Profile.RingSegments, 3, 128);
	const FVector Normal = InOutPath.ImpactNormal.GetSafeNormal();

	// Build the circle in the plane of the impact normal, so a ring on a ramp starts out tilted with the
	// ramp rather than being a flat disc that has to be dragged into place.
	FVector Tangent = FVector::CrossProduct(Normal, FVector::UpVector);
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::CrossProduct(Normal, FVector::ForwardVector);
	}
	Tangent = Tangent.GetSafeNormal();
	const FVector Bitangent = FVector::CrossProduct(Normal, Tangent).GetSafeNormal();

	const float ConformDistance = FMath::Max(0.0f, Profile.RingConformDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcCastRingConform), Profile.bTraceComplex);
	for (const TObjectPtr<AActor>& Ignored : Request.IgnoredActors)
	{
		if (Ignored)
		{
			Params.AddIgnoredActor(Ignored.Get());
		}
	}

	InOutPath.RingPoints.Reserve(Segments + 1);

	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = (2.0f * UE_PI * static_cast<float>(Index % Segments)) / static_cast<float>(Segments);
		FVector Point = InOutPath.ImpactPoint
			+ (Tangent * FMath::Cos(Angle) + Bitangent * FMath::Sin(Angle)) * RequestedRadius;

		if (ConformDistance > 0.0f)
		{
			// One trace per ring point. This is what makes the ring lie on the stairs instead of hovering
			// through them, and it is the single most visible thing in a still screenshot. The price is
			// counted in ArcCast.Stats under Ring, so nobody has to guess what it costs.
			const FVector TraceStart = Point + FVector(0.0f, 0.0f, ConformDistance);
			const FVector TraceEnd = Point - FVector(0.0f, 0.0f, ConformDistance);

			FHitResult ConformHit;
			++Counters.RingTraces;
			if (World->LineTraceSingleByChannel(ConformHit, TraceStart, TraceEnd, Profile.CollisionChannel.GetValue(), Params))
			{
				Point = ConformHit.ImpactPoint + ConformHit.ImpactNormal.GetSafeNormal() * Profile.RingSurfaceOffset;
			}
		}

		InOutPath.RingPoints.Add(Point);
	}
}

// -------------------------------------------------------------------------------------------------------
// Drawing
// -------------------------------------------------------------------------------------------------------

bool UArcCastSubsystem::ProjectPoint(const UCanvas* Canvas, const FVector& World, FVector2D& OutScreen, float& OutDepth)
{
	if (!Canvas || !Canvas->SceneView)
	{
		return false;
	}

	// FSceneView::Project keeps the unprojected W in the plane's W component, which is the view depth.
	// UCanvas::Project throws that away and clamps, and without it a point behind the camera silently
	// projects to a mirrored position somewhere on screen - a wildly wrong arc, drawn confidently.
	const FPlane Projected = Canvas->SceneView->Project(World);
	OutDepth = static_cast<float>(Projected.W);

	if (Projected.W <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float HalfX = static_cast<float>(Canvas->ClipX) * 0.5f;
	const float HalfY = static_cast<float>(Canvas->ClipY) * 0.5f;

	OutScreen.X = HalfX + static_cast<float>(Projected.X) * HalfX;
	OutScreen.Y = HalfY - static_cast<float>(Projected.Y) * HalfY;
	return true;
}

bool UArcCastSubsystem::ProjectSegment(const UCanvas* Canvas, const FVector& A, const FVector& B, FVector2D& OutA, FVector2D& OutB)
{
	if (!Canvas || !Canvas->SceneView)
	{
		return false;
	}

	const FPlane PA = Canvas->SceneView->Project(A);
	const FPlane PB = Canvas->SceneView->Project(B);

	const double WA = PA.W;
	const double WB = PB.W;
	constexpr double NearW = 1.0;

	if (WA <= NearW && WB <= NearW)
	{
		return false;
	}

	// W is linear in the world position, so the crossing point of the near plane can be interpolated
	// exactly. Clipping here rather than dropping the segment is what keeps an arc that starts behind the
	// camera - a third-person over-the-shoulder throw, every time - from losing its first stretch.
	FVector ClippedA = A;
	FVector ClippedB = B;

	if (WA <= NearW)
	{
		const double T = (NearW - WA) / (WB - WA);
		ClippedA = FMath::Lerp(A, B, T);
	}
	else if (WB <= NearW)
	{
		const double T = (NearW - WB) / (WA - WB);
		ClippedB = FMath::Lerp(B, A, T);
	}

	float DepthA = 0.0f;
	float DepthB = 0.0f;
	return ProjectPoint(Canvas, ClippedA, OutA, DepthA) && ProjectPoint(Canvas, ClippedB, OutB, DepthB);
}

void UArcCastSubsystem::CacheView(const UCanvas* Canvas)
{
	if (Canvas && Canvas->SceneView)
	{
		CachedViewLocation = Canvas->SceneView->ViewMatrices.GetViewOrigin();
		CachedViewForward = Canvas->SceneView->GetViewDirection();
	}
}

void UArcCastSubsystem::DrawArcs(UCanvas* Canvas)
{
	if (!bEnabled || !Canvas || !Canvas->Canvas || !Canvas->SceneView)
	{
		return;
	}

	CacheView(Canvas);
	LastDrawFrame = GFrameCounter;

	const double DrawStart = FPlatformTime::Seconds();

	for (TPair<int32, FActiveArc>& Pair : Arcs)
	{
		DrawSingleArc(Canvas, Pair.Value);
	}

	Stats.DrawMilliseconds = static_cast<float>((FPlatformTime::Seconds() - DrawStart) * 1000.0);
	Stats.TotalTraces = Stats.SimTraces + Stats.RingTraces + Stats.OcclusionTraces;

	if (bShowStats)
	{
		DrawStatsBox(Canvas);
	}
}

void UArcCastSubsystem::UpdateOcclusion(FArcCastPath& InOutPath, const FArcCastRequest& Request, const UArcCastProfile& Profile, const FVector& ViewLocation)
{
	const int32 PointCount = InOutPath.Points.Num();

	InOutPath.Occluded.Reset(PointCount);
	InOutPath.Occluded.AddZeroed(PointCount);

	if (!bOcclusionAware || PointCount == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcCastOcclusion), Profile.bTraceComplex);
	for (const TObjectPtr<AActor>& Ignored : Request.IgnoredActors)
	{
		if (Ignored)
		{
			Params.AddIgnoredActor(Ignored.Get());
		}
	}

	const int32 EveryNth = FMath::Max(1, OcclusionEveryNthPoint);
	const ECollisionChannel Channel = Profile.CollisionChannel.GetValue();

	uint8 Carried = 0;
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		if (Index % EveryNth == 0)
		{
			const FVector Target = InOutPath.Points[Index];
			FVector ToTarget = Target - ViewLocation;
			const float Length = static_cast<float>(ToTarget.Size());

			if (Length <= OcclusionEndOffset)
			{
				Carried = 0;
			}
			else
			{
				ToTarget /= Length;
				const FVector TraceEnd = ViewLocation + ToTarget * (Length - OcclusionEndOffset);

				FHitResult Hit;
				++Stats.OcclusionTraces;
				Carried = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, Channel, Params) ? 1 : 0;
			}
		}

		InOutPath.Occluded[Index] = Carried;
	}
}

void UArcCastSubsystem::DrawSingleArc(UCanvas* Canvas, FActiveArc& Arc)
{
	const UArcCastProfile* ProfilePtr = ResolveProfile(Arc.Request.Profile);
	if (!ProfilePtr || !Arc.Path.bHasResult)
	{
		return;
	}

	const UArcCastProfile& Profile = *ProfilePtr;

	UpdateOcclusion(Arc.Path, Arc.Request, Profile, CachedViewLocation);

	FLinearColor BaseColor = Profile.GetVerdictColor(Arc.Path.Verdict);
	BaseColor.A *= GlobalOpacity;

	const EArcCastLineStyle ProfileStyle = bHasStyleOverride ? StyleOverride : Profile.LineStyle;
	const float MinSegment = FMath::Max(0.0f, Profile.MinScreenSegmentPixels);

	// Negative so a positive DashScrollSpeed makes the dashes travel from the hand towards the landing
	// point rather than back up the arc.
	float RunLength = -DashPhase * (Profile.DashScrollSpeed / 60.0f);
	FVector2D PendingStart = FVector2D::ZeroVector;
	bool bHasPending = false;
	uint8 PendingBounce = 0;
	bool bPendingOccluded = false;

	for (int32 Index = 0; Index + 1 < Arc.Path.Points.Num(); ++Index)
	{
		FVector2D ScreenA;
		FVector2D ScreenB;
		if (!ProjectSegment(Canvas, Arc.Path.Points[Index], Arc.Path.Points[Index + 1], ScreenA, ScreenB))
		{
			bHasPending = false;
			continue;
		}

		const uint8 BounceHere = Arc.Path.BounceIndex.IsValidIndex(Index) ? Arc.Path.BounceIndex[Index] : 0;
		const bool bOccludedHere = Arc.Path.Occluded.IsValidIndex(Index) && Arc.Path.Occluded[Index] != 0;

		if (!bHasPending || BounceHere != PendingBounce || bOccludedHere != bPendingOccluded)
		{
			PendingStart = ScreenA;
			PendingBounce = BounceHere;
			bPendingOccluded = bOccludedHere;
			bHasPending = true;
		}

		// Merge sub-pixel segments into the next one. At a 1/30 s step a near-vertical arc produces a
		// dozen points inside one pixel, and stroking each of them separately costs draw calls and turns
		// the dash pattern into mush.
		if (FVector2D::Distance(PendingStart, ScreenB) < MinSegment && Index + 2 < Arc.Path.Points.Num())
		{
			continue;
		}

		const float Fade = FMath::Pow(FMath::Clamp(Profile.BounceFadePerHit, 0.05f, 1.0f), static_cast<float>(PendingBounce));

		FLinearColor Color = BaseColor;
		Color.A *= Fade;

		EArcCastLineStyle Style = ProfileStyle;
		if (bPendingOccluded)
		{
			// Not hidden - faded and broken up. The player has to be able to see that the arc carries on
			// behind the wall; what they must not see is it shining through the wall as if it were glass.
			Color.A *= OccludedOpacity;
			Style = EArcCastLineStyle::Dashed;
		}

		const float Thickness = Profile.LineThickness * FMath::Max(0.35f, Fade);

		StrokeSegment(Canvas, PendingStart, ScreenB, Color, Thickness, Style, Profile, RunLength);
		bHasPending = false;
	}

	// The splash ring, already conformed to the geometry, drawn as a projected polyline.
	if (Arc.Path.RingPoints.Num() >= 2)
	{
		FLinearColor RingColor = BaseColor;
		float RingRun = 0.0f;

		for (int32 Index = 0; Index + 1 < Arc.Path.RingPoints.Num(); ++Index)
		{
			FVector2D ScreenA;
			FVector2D ScreenB;
			if (!ProjectSegment(Canvas, Arc.Path.RingPoints[Index], Arc.Path.RingPoints[Index + 1], ScreenA, ScreenB))
			{
				continue;
			}

			StrokeSegment(Canvas, ScreenA, ScreenB, RingColor, Profile.RingThickness, EArcCastLineStyle::Solid, Profile, RingRun);
		}
	}

	if (Profile.bDrawEndMarker)
	{
		FVector2D ScreenImpact;
		float Depth = 0.0f;
		if (ProjectPoint(Canvas, Arc.Path.ImpactPoint, ScreenImpact, Depth))
		{
			DrawEndMarker(Canvas, ScreenImpact, BaseColor, Profile.EndMarkerSize, FMath::Max(1.0f, Profile.LineThickness));
		}
	}
}

void UArcCastSubsystem::StrokeSegment(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness, EArcCastLineStyle Style, const UArcCastProfile& Profile, float& InOutRunLength)
{
	const float Length = static_cast<float>(FVector2D::Distance(A, B));
	if (Length <= UE_KINDA_SMALL_NUMBER || Color.A <= 0.001f)
	{
		InOutRunLength += Length;
		return;
	}

	auto Stroke = [Canvas, &Color, Thickness, this](const FVector2D& From, const FVector2D& To)
	{
		FCanvasLineItem Line(From, To);
		Line.SetColor(Color);
		Line.LineThickness = Thickness;
		Line.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Line);
		++Stats.LinesDrawn;
	};

	if (Style == EArcCastLineStyle::Solid)
	{
		Stroke(A, B);
		InOutRunLength += Length;
		return;
	}

	const float DashLength = (Style == EArcCastLineStyle::Dots)
		? FMath::Max(1.0f, Thickness)
		: FMath::Max(1.0f, Profile.DashLengthPixels);
	const float GapLength = FMath::Max(1.0f, Profile.DashGapPixels);
	const float Period = DashLength + GapLength;

	const FVector2D Direction = (B - A) / Length;

	// The run length carries across segments so the dashes stay continuous along the whole arc instead of
	// restarting at every integration point, and adding the scroll phase to it is what makes them travel.
	float Cursor = -FMath::Fmod(InOutRunLength, Period);
	int32 Guard = 0;

	while (Cursor < Length && Guard++ < 512)
	{
		const float DashStart = FMath::Max(Cursor, 0.0f);
		const float DashEnd = FMath::Min(Cursor + DashLength, Length);

		if (DashEnd > DashStart)
		{
			Stroke(A + Direction * DashStart, A + Direction * DashEnd);
		}

		Cursor += Period;
	}

	InOutRunLength += Length;
}

void UArcCastSubsystem::DrawEndMarker(UCanvas* Canvas, const FVector2D& Center, const FLinearColor& Color, float Size, float Thickness)
{
	const float Half = FMath::Max(1.0f, Size) * 0.5f;

	auto Stroke = [Canvas, &Color, Thickness, this](const FVector2D& From, const FVector2D& To)
	{
		FCanvasLineItem Line(From, To);
		Line.SetColor(Color);
		Line.LineThickness = Thickness;
		Line.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Line);
		++Stats.LinesDrawn;
	};

	// A diamond rather than a cross: it stays readable on top of the ring it usually sits inside.
	const FVector2D Top(Center.X, Center.Y - Half);
	const FVector2D Right(Center.X + Half, Center.Y);
	const FVector2D Bottom(Center.X, Center.Y + Half);
	const FVector2D Left(Center.X - Half, Center.Y);

	Stroke(Top, Right);
	Stroke(Right, Bottom);
	Stroke(Bottom, Left);
	Stroke(Left, Top);
}

// -------------------------------------------------------------------------------------------------------
// Statistics box
// -------------------------------------------------------------------------------------------------------

void UArcCastSubsystem::DrawStatsBox(UCanvas* Canvas)
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float LineHeight = 15.0f;
	const float PaddingX = 10.0f;
	const float PaddingY = 8.0f;
	const float BoxWidth = 268.0f;
	constexpr int32 LineCount = 8;
	const float BoxHeight = PaddingY * 2.0f + LineHeight * LineCount;

	const float OriginX = 24.0f;
	const float OriginY = 24.0f;

	FCanvasTileItem Background(
		FVector2D(OriginX, OriginY),
		GWhiteTexture,
		FVector2D(BoxWidth, BoxHeight),
		FLinearColor(0.02f, 0.03f, 0.05f, 0.72f));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);

	float Y = OriginY + PaddingY;

	auto DrawLine = [&](const FString& Text, const FLinearColor& Color)
	{
		FCanvasTextItem Item(FVector2D(OriginX + PaddingX, Y), FText::FromString(Text), Font, Color);
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Canvas->DrawItem(Item);
		Y += LineHeight;
	};

	const FLinearColor Heading(0.55f, 0.82f, 1.0f, 1.0f);
	const FLinearColor Body(0.86f, 0.88f, 0.92f, 1.0f);

	const UEnum* VerdictEnum = StaticEnum<EArcCastVerdict>();
	const FString VerdictName = VerdictEnum
		? VerdictEnum->GetNameStringByValue(static_cast<int64>(LastVerdict))
		: TEXT("?");

	DrawLine(TEXT("ArcCast"), Heading);
	DrawLine(FString::Printf(TEXT("Arcs %d   Verdict %s"), Stats.ActiveArcs, *VerdictName), Body);
	DrawLine(FString::Printf(TEXT("Steps %d   Step %.1f ms%s"),
		Stats.SimSteps,
		Stats.EffectiveStepSeconds * 1000.0f,
		Stats.bBudgetCoarsened ? TEXT("  (coarsened)") : TEXT("")), Body);
	DrawLine(FString::Printf(TEXT("Traces %d   sim %d  ring %d  occl %d"),
		Stats.TotalTraces, Stats.SimTraces, Stats.RingTraces, Stats.OcclusionTraces), Body);
	DrawLine(FString::Printf(TEXT("Sim %.3f ms   Draw %.3f ms"), Stats.SimMilliseconds, Stats.DrawMilliseconds), Body);
	DrawLine(FString::Printf(TEXT("Lines %d   Occlusion %s"),
		Stats.LinesDrawn, bOcclusionAware ? TEXT("on") : TEXT("off")), Body);
	DrawLine(FString::Printf(TEXT("Flight %.2f s   Distance %.0f cm"), LastPath.TotalTime, LastPath.TotalDistance), Body);
	DrawLine(FString::Printf(TEXT("Bounces %d   Points %d"), LastPath.BounceCount, LastPath.Points.Num()), Body);
}

void UArcCastSubsystem::LogStats() const
{
	const UEnum* VerdictEnum = StaticEnum<EArcCastVerdict>();
	const FString VerdictName = VerdictEnum
		? VerdictEnum->GetNameStringByValue(static_cast<int64>(LastVerdict))
		: TEXT("?");

	UE_LOG(LogArcCast, Display, TEXT("ArcCast.Stats -----------------------------------------"));
	UE_LOG(LogArcCast, Display, TEXT("  Active arcs      %d"), Stats.ActiveArcs);
	UE_LOG(LogArcCast, Display, TEXT("  Last verdict     %s"), *VerdictName);
	UE_LOG(LogArcCast, Display, TEXT("  Steps / frame    %d  (budget %d, step %.2f ms%s)"),
		Stats.SimSteps, MaxSimStepsPerFrame, Stats.EffectiveStepSeconds * 1000.0f,
		Stats.bBudgetCoarsened ? TEXT(", coarsened") : TEXT(""));
	UE_LOG(LogArcCast, Display, TEXT("  Traces / frame   %d  (sim %d, ring %d, occlusion %d)"),
		Stats.TotalTraces, Stats.SimTraces, Stats.RingTraces, Stats.OcclusionTraces);
	UE_LOG(LogArcCast, Display, TEXT("  Simulate         %.3f ms"), Stats.SimMilliseconds);
	UE_LOG(LogArcCast, Display, TEXT("  Draw             %.3f ms  (%d canvas lines)"), Stats.DrawMilliseconds, Stats.LinesDrawn);
	UE_LOG(LogArcCast, Display, TEXT("  Occlusion aware  %s"), bOcclusionAware ? TEXT("on") : TEXT("off"));
	UE_LOG(LogArcCast, Display, TEXT("  Last flight      %.2f s over %.0f cm, %d bounces, %d points"),
		LastPath.TotalTime, LastPath.TotalDistance, LastPath.BounceCount, LastPath.Points.Num());
	UE_LOG(LogArcCast, Display, TEXT("-------------------------------------------------------"));
}

// -------------------------------------------------------------------------------------------------------
// Draw hooks
// -------------------------------------------------------------------------------------------------------

void UArcCastSubsystem::OnAnyHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	if (!bEnabled || !bAutoHUDDraw || !HUD || !Canvas)
	{
		return;
	}

	if (HUD->GetWorld() != GetWorld())
	{
		return;
	}

	// The HUD component or AArcCastHUD already drew this frame - do not stack a second arc on top of it.
	if (LastDrawFrame == GFrameCounter)
	{
		return;
	}

	DrawArcs(Canvas);
}

#if WITH_EDITOR
void UArcCastSubsystem::OnEditorViewportDraw(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!bEnabled || !bDrawInEditorViewport || !Canvas || !Canvas->SceneView)
	{
		return;
	}

	// The debug draw service is engine-global: every world's subsystem is called for every viewport that
	// draws. Without this the arcs of a PIE session are drawn into the editor viewport as well.
	const UWorld* ViewWorld = nullptr;
	if (Canvas->SceneView->Family && Canvas->SceneView->Family->Scene)
	{
		ViewWorld = Canvas->SceneView->Family->Scene->GetWorld();
	}
	if (ViewWorld != GetWorld())
	{
		return;
	}

	// A game HUD already drew this frame or last. Two paths, one arc.
	if (GFrameCounter - LastDrawFrame <= 1)
	{
		CacheView(Canvas);
		return;
	}

	DrawArcs(Canvas);
}
#endif

// -------------------------------------------------------------------------------------------------------
// Runtime knobs
// -------------------------------------------------------------------------------------------------------

void UArcCastSubsystem::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	RebindHudDelegate();
}

void UArcCastSubsystem::SetOcclusionAware(bool bInOcclusionAware)
{
	bOcclusionAware = bInOcclusionAware;
}

void UArcCastSubsystem::SetOccludedOpacity(float InOpacity)
{
	OccludedOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
}

void UArcCastSubsystem::SetOcclusionEveryNthPoint(int32 InEveryNth)
{
	OcclusionEveryNthPoint = FMath::Max(1, InEveryNth);
}

void UArcCastSubsystem::SetMaxSimStepsPerFrame(int32 InMaxSteps)
{
	MaxSimStepsPerFrame = FMath::Max(16, InMaxSteps);
}

void UArcCastSubsystem::SetGlobalOpacity(float InOpacity)
{
	GlobalOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
}

void UArcCastSubsystem::SetShowStats(bool bInShow)
{
	bShowStats = bInShow;
}

void UArcCastSubsystem::SetBounceOverride(int32 InBounces)
{
	BounceOverride = InBounces < 0 ? -1 : FMath::Min(InBounces, 255);
}

void UArcCastSubsystem::SetStyleOverride(EArcCastLineStyle InStyle)
{
	bHasStyleOverride = true;
	StyleOverride = InStyle;
}

void UArcCastSubsystem::ClearStyleOverride()
{
	bHasStyleOverride = false;
}
