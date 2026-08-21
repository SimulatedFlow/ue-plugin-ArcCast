# ArcCast — Trajectory & Landing Preview

**Unreal Engine 5.8 · one runtime module · Win64 / Mac / Linux · no third-party code**

The throw arc you can actually ship: bounce-chained trajectory prediction with a landing verdict,
a conformed splash ring and occlusion-aware drawing — on `UCanvas`, so it survives a Shipping build.

**ArcCast does not throw anything.** It spawns no projectile, deals no damage, replicates nothing.
It shows where the thing your game is about to launch would end up. Everything below assumes you
already have a throwing system, or are about to write one.

---

## Contents

1. [Requirements, engine and supported platforms](#1-requirements-engine-and-supported-platforms)
2. [Install and first arc — five minutes](#2-install-and-first-arc--five-minutes)
3. [Why the engine's own preview disappears in Shipping](#3-why-the-engines-own-preview-disappears-in-shipping)
4. [The three drawing paths](#4-the-three-drawing-paths)
5. [Profiles](#5-profiles)
6. [The landing verdict, and wiring it to a crosshair](#6-the-landing-verdict-and-wiring-it-to-a-crosshair)
7. [Prediction with no drawing at all](#7-prediction-with-no-drawing-at-all)
8. [The splash ring](#8-the-splash-ring)
9. [Occlusion-aware drawing](#9-occlusion-aware-drawing)
10. [Making the preview agree with your projectile](#10-making-the-preview-agree-with-your-projectile)
11. [Class and API overview](#11-class-and-api-overview)
12. [Code examples](#12-code-examples)
13. [Console commands](#13-console-commands)
14. [Project settings](#14-project-settings)
15. [Cost, budget and limits](#15-cost-budget-and-limits)
16. [Troubleshooting](#16-troubleshooting)
17. [The demo map](#17-the-demo-map)
18. [Support](#18-support)

---

## 1. Requirements, engine and supported platforms

| | |
|---|---|
| **Engine version** | Unreal Engine **5.8** (`"EngineVersion": "5.8.0"` in `ArcCast.uplugin`) |
| **Modules** | One: `ArcCast`, `Type: Runtime`, `LoadingPhase: PreDefault` |
| **Supported platforms** | **Win64, Mac, Linux** — the module's `PlatformAllowList` |
| **Engine dependencies** | `Core`, `CoreUObject`, `Engine`, `DeveloperSettings` (public); `SlateCore`, `RenderCore`, `NavigationSystem` (private) |
| **Third-party code** | None. No external libraries, no bundled binaries beyond the compiled module |
| **Plugin dependencies** | None. ArcCast does not require any other plugin, Fab or engine |
| **Project type** | Blueprint-only projects fully supported; full C++ source included |
| **Build configurations** | Verified building for `UnrealEditor Development`, `UnrealGame Development` and `UnrealGame Shipping` |
| **Content** | `CanContainContent: true`. The demo content under `Content/ArcCast/` is optional example material |

### Notes on the platform list

The drawing path is `UCanvas` through `AHUD`, and the simulation is `UWorld::SweepSingleByChannel`.
Neither is platform-specific and neither uses a rendering feature beyond canvas line items, so the
plugin has no platform-specific code at all. The `PlatformAllowList` is limited to the three desktop
platforms because those are the ones the plugin is verified on; nothing in the source prevents it from
compiling elsewhere.

`NavigationSystem` is a **private** dependency and is only reached when a profile sets
`bRequireNavMesh`. A project with no navigation system, or with no built navigation data, loads and
runs normally — it just gets an honest `NoGround` verdict instead of a teleport target. See
[section 6](#6-the-landing-verdict-and-wiring-it-to-a-crosshair).

### What is *not* a dependency, on purpose

* **No UMG.** The preview is canvas drawing. The demo HUD in `Content/ArcCast/UI/` is example
  material, not the product.
* **No `UnrealEd`.** Everything ships. The editor-viewport draw is a `UDebugDrawService` second path
  behind `WITH_EDITOR`, not an editor module.
* **No Niagara, no `ProceduralMeshComponent`.** The arc needs no material, no mesh and no asset of
  any kind — that is the reason it runs in a fresh project immediately and costs nothing to cook.

---

## 2. Install and first arc — five minutes

1. Copy `ArcCast/` into your project's `Plugins/` folder and restart the editor.
   (In a C++ project, regenerate project files if the editor asks.)
2. Open the actor that should aim — a character, a weapon, a turret.
3. **Add Component → ArcCast.** Attach it to the hand socket, the muzzle, or wherever the throw
   starts. The component aims down its own **forward vector**.
4. Leave `Profile` empty for now. The plugin ships four built-in profiles in code, and the project
   default is `Grenade`, so an arc appears immediately — no asset needed.
5. That is it. **Press `G` for Game View** and the arc is visible **in the editor viewport**, with
   no play session, because the component sets `bTickInEditor`. Rotate the component and watch the
   arc swing. (Game View is required — see [section 9](#the-editor-viewport).)

Press Play. The arc is drawn through `AHUD` whatever your HUD class is, because
`bAutoSpawnHUDComponent` is on by default (see [section 4](#4-the-three-drawing-paths)).

To read the landing point in Blueprint:

```
Event Tick
  └─> ArcCast (component) → Get Impact Point And Normal  →  Out Point, Out Normal, Return Value
```

To throw for real, take the exact numbers the preview used:

```
Get Launch Location   →  spawn transform
Get Launch Velocity   →  Projectile Movement Component · Velocity
```

Using those two nodes is what guarantees the projectile follows the arc that was drawn.

---

## 3. Why the engine's own preview disappears in Shipping

This is worth stating plainly, because it is the reason this plugin exists.

`UGameplayStatics::PredictProjectilePath` already gives you the prediction. It fills an
`FPredictProjectilePathResult` with the whole point list, and it is a perfectly good function.

The problem is the drawing. Its only renderer is the `DrawDebugType` parameter, which routes to
`DrawDebugLine` — and `DrawDebugLine` is behind `ENABLE_DRAW_DEBUG`, which the engine defines as **0
in a Shipping configuration**. Every debug-drawn line, sphere and arc compiles out. The aiming aid
your player was promised is present in every build you tested with and absent from the build you
ship.

The usual workarounds each cost something:

| Workaround | What it costs |
|---|---|
| Spline mesh along the points | A mesh, a material, and a `USplineMeshComponent` per segment |
| Niagara ribbon | A Niagara system the customer has to import, configure and cook |
| Decal or plane for the landing marker | A material, and a landing marker that clips into slopes |
| Leave `ENABLE_DRAW_DEBUG` on in Shipping | Every debug drawer in the project ships with it |

ArcCast takes the fourth route: draw on `UCanvas` from `AHUD::DrawHUD`. That is the same path the
crosshair and the ammo counter take. It is not compiled out, it needs no asset of any kind, and it
works in a project that has never opened the material editor.

**`DrawDebugLine` does not appear anywhere in this plugin's source** — not even behind a debug flag.
The one editor-only exception is `UDebugDrawService`, which is used to obtain a *canvas* in an editor
viewport with no PIE running; it is documented at the call site as a second path and it draws through
the same canvas code as everything else.

---

## 4. The three drawing paths

All three end in `UArcCastSubsystem::DrawArcs(UCanvas*)`. A frame guard makes sure the arcs are never
drawn twice in one frame, so having two of them enabled is harmless.

### a) Automatic — change nothing (default)

`Project Settings → Plugins → ArcCast → Auto Spawn HUD Component` (on by default). The subsystem
hooks `AHUD::OnHUDPostRender`, the delegate the engine broadcasts after every HUD render, and draws
on the canvas it is handed. Your HUD class is not touched, not subclassed, and not replaced. This
works in a cooked Shipping build.

### b) `AArcCastHUD` — the ready-made HUD class

Set your game mode's **HUD Class** to `ArcCastHUD`. Useful in a prototype that has no HUD yet.
`bDrawArcs` on the class turns it off again.

### c) `UArcCastHUDComponent` — your HUD, your draw order

Already have a HUD class you are not giving up? Add **ArcCast HUD** as a component to it and call
`Draw Arcs (Canvas)` from your own `DrawHUD` / `Event Receive Draw HUD`. This is the path to use when
the arcs must sit at a defined point in your draw order — under the crosshair rather than over it.

```cpp
void AMyHUD::DrawHUD()
{
    Super::DrawHUD();
    ArcCastComponent->DrawArcs(Canvas);   // wherever you want it in the order
    DrawMyCrosshair();
}
```

### The editor viewport

With `Draw In Editor Viewport` on (default), arcs are also drawn in an editor viewport with **no PIE
session running**, so a level designer can place a thrower and see its arc while building the level.
That path goes through `UDebugDrawService`, which is compiled out of a cooked build — it is a
convenience for authoring, never something a game should rely on.

**The viewport has to be in Game View.** Press `G` (or *Show → Game View*). The draw delegate is
registered under the engine's `Game` show flag, and that flag is off in an ordinary editor
perspective viewport — so with the flag off, nothing is drawn and nothing is even computed. This is
not a bug and not a missing player controller: Game View is the mode you frame and film in anyway,
which is what this path is for. If you run `ArcCast.Test` before the viewport has drawn one frame in
Game View, the log says *"no view seen yet"* — that is the same fact, seen from the other side,
because the arc is thrown from the cached camera position.

---

## 5. Profiles

`UArcCastProfile` is a Data Asset that carries **flight behaviour and appearance together**. A
grenade arc and a teleport arc do not only fly differently, they have to look different, and
splitting that across two assets just means every project re-pairs them by hand.

**Create one:** right-click in the Content Browser → *Miscellaneous → Data Asset → ArcCast Profile*,
or duplicate one of the shipped assets in `Content/ArcCast/Profiles/` (`DA_Arc_Grenade`,
`DA_Arc_Teleport`, `DA_Arc_Arrow`, `DA_Arc_Bouncy`).

### Built-in profiles

Four profiles exist in code, so the plugin previews something the minute it is enabled. Reach them
from Blueprint with `Get Builtin Profile`, or name one under
`Project Settings → Plugins → ArcCast → Default Builtin Profile`.

| Name | Behaviour |
|---|---|
| `Grenade` | 1200 cm/s, radius 9, **2 bounces**, restitution 0.45, 320 cm splash ring, solid line |
| `Teleport` | 1800 cm/s, **0 bounces**, no ring, **`bRequireNavMesh`**, scrolling dashed line, end marker |
| `Arrow` | 4000 cm/s, gravity ×0.7, radius 2, **0 bounces**, no ring, small end marker, 1/60 s step |
| `Bouncy` | 1500 cm/s, **5 bounces**, restitution 0.8, friction 0.05, dotted line, slow per-bounce fade |

### The fields that matter most

**Flight**

| Field | Default | Notes |
|---|---|---|
| `LaunchSpeed` | 1400 | Only used when a caller gives a direction instead of a velocity |
| `GravityScale` | 1.0 | Must match your projectile's `Projectile Gravity Scale` |
| `ProjectileRadius` | 8.0 | Sphere-swept. `0` makes it a line trace — faster, and it slips through gaps a real grenade could not |
| `MaxBounces` | 2 | `0` reproduces the engine's own "stop at first hit" |
| `Restitution` | 0.45 | Normal velocity kept across a bounce |
| `Friction` | 0.2 | Tangential velocity lost across a bounce |
| `MaxSimTime` | 4.0 s | Hard stop. Bounds the cost of an arc thrown at the sky |
| `RestSpeed` | 60 | Below this, a bounce is treated as coming to rest |
| `FixedStepSeconds` | 1/30 | Integration step. The per-frame budget may coarsen it |
| `CollisionChannel` | `Visibility` | A dedicated channel is better than `Visibility` in a real project |
| `bTraceComplex` | `false` | Per-triangle sweeps. Slower, and rarely what a thrown object wants |

**Landing** — `GroundProbeDistance` (200), `MaxWalkableSlopeAngle` (45°), `bRequireNavMesh` (off),
`NavProjectExtent`.

**Ring** — `SplashRadius` (300), `bDrawRing`, `RingSegments` (32), `RingConformDistance` (150),
`RingSurfaceOffset` (4), `RingThickness`.

**Appearance** — `LineStyle` (Solid / Dashed / Dots), `LineThickness`, `DashLengthPixels`,
`DashGapPixels`, `DashScrollSpeed`, `ValidColor` / `WarnColor` / `InvalidColor`, `BounceFadePerHit`,
`bDrawEndMarker`, `EndMarkerSize`, `MinScreenSegmentPixels`.

`BounceFadePerHit` is applied once per bounce already survived, to both opacity **and** thickness, so
the eye reads the order of events without any legend.

---

## 6. The landing verdict, and wiring it to a crosshair

Every path comes back with an `EArcCastVerdict`:

| Verdict | Meaning | Colour |
|---|---|---|
| `Valid` | Ground found, slope walkable, navigation check (if any) passed | `ValidColor` — green |
| `TooSteep` | There is ground, but it leans further from up than `MaxWalkableSlopeAngle` | `WarnColor` — amber |
| `Blocked` | The launch point starts inside geometry, or the navigation projection failed | `InvalidColor` — red |
| `OutOfRange` | The path ran past the request's `MaxRange` before landing | `WarnColor` — amber |
| `NoGround` | Nothing under the end of the arc within `GroundProbeDistance` — a pit, a ledge, the sky. Also the answer when a profile asks for navigation in a project that has none | `InvalidColor` — red |

`TooSteep` and `OutOfRange` are amber rather than red on purpose: the throw is legal, the *landing*
is not.

The verdict colours the arc and the ring automatically. To recolour something of your own, bind the
event instead of polling:

```
ArcCast (component) → On Verdict Changed (New Verdict)
  └─> Switch on EArcCastVerdict
        Valid      → Set Crosshair Colour (green)
        TooSteep   → Set Crosshair Colour (amber)
        default    → Set Crosshair Colour (red)
```

`On Impact Actor Changed` fires the same way when the arc starts or stops landing on a different
actor — for a name plate, a highlight, or a friendly-fire warning. Both fire on change only, not
every frame.

### Teleport targeting

Set `bRequireNavMesh` on the profile. The landing point then also has to project onto the navigation
mesh, and the reported `ImpactPoint` is snapped to the navigable point — so a teleport that follows
the preview arrives exactly where the marker was drawn. This is the whole difference between a throw
arc and a teleport indicator.

**A project with no navigation system does not break.** The verdict comes back `NoGround`, honestly,
and nothing is drawn green.

---

## 7. Prediction with no drawing at all

Half of ArcCast has nothing to do with pixels. `Predict Arc` simulates one arc and hands back the
result — bounces, landing point, verdict, flight time, path length, ring points — and **registers
nothing and draws nothing**. It answers on the calling frame and it is safe on a dedicated server
where no canvas exists.

```
Predict Arc
    World Context   Self
    Start Location  Get Launch Location
    Launch Velocity Get Launch Velocity
    Profile         DA_Arc_Grenade
    Ignored Actors  [ Self ]
    Max Range       2500
        → Path (Verdict, Impact Point, Total Time, Total Distance, Bounce Count, Points…)
```

Uses that need no preview at all:

* an AI deciding whether a grenade would actually reach the cover it is aiming at;
* a server-side check that the client's claimed throw was possible;
* an ability that refuses to fire when the landing would be `TooSteep`;
* a designer tool that scores throwing positions in a level.

`PredictArc` and the drawn arc run the exact same code, so they agree on the landing point.

---

## 8. The splash ring

At the landing point ArcCast builds a circle of `RingSegments` points in the plane of the impact
normal, then **probes each point individually** up and down by `RingConformDistance` and moves it to
the surface it finds.

That is why the ring lies **on the stairs** and follows a slope, instead of hovering through them as
a flat disc. It costs one trace per ring point per recomputation, and the price is reported
separately in `ArcCast.Stats` under `ring` so nobody has to guess.

Turn it off with `bDrawRing`, or set `SplashRadius` to 0. Cheaper settings: fewer `RingSegments`, or
`RingConformDistance = 0` for a flat ring with no traces at all.

---

## 9. Occlusion-aware drawing

With `bOcclusionAware` on (default), every `OcclusionEveryNthPoint`-th point is traced from the
camera. Blocked stretches are **not removed** — they are drawn at `OccludedOpacity` (0.25) and forced
to dashed.

That distinction is the point. The player has to be able to see that the arc carries on behind the
wall; what they must not see is it shining through the wall as though the wall were glass.

The trace stops `OcclusionEndOffset` (8 cm) short of each point. Without that, every point lying *on*
a surface — the landing point, the entire splash ring — reports itself occluded by the very thing it
is sitting on, and the ring goes permanently faint.

Switch it off with `ArcCast.Occlusion 0` or `Set Occlusion Aware`. The occlusion trace count in
`ArcCast.Stats` drops to zero and the arcs go back to drawing through geometry.

---

## 10. Making the preview agree with your projectile

A prediction is a prediction. If your projectile flies under different physics than the preview
simulated, you get a different path — and that is not a bug in either of them.

These values must agree:

| ArcCast (profile / request) | Your projectile |
|---|---|
| `LaunchVelocity` from the request | `UProjectileMovementComponent::Velocity` at spawn |
| `StartLocation` from the request | The spawn transform's location |
| `GravityScale` | `Projectile Gravity Scale` |
| `ProjectileRadius` | The collision shape's radius |
| `CollisionChannel` | The channel the projectile actually blocks against |
| `Restitution` | `Bounciness` |
| `Friction` | `Friction` |
| `MaxBounces` | However many bounces the projectile survives before it explodes |

The safe way to make them agree is not to copy numbers at all: read `Get Launch Location` and
`Get Launch Velocity` off the component and hand those exact values to the spawn.

ArcCast's integrator is semi-implicit Euler, the same scheme `UProjectileMovementComponent` uses, so
matching the values above gets you the same path to within the step size.

**What ArcCast does not model:** air resistance, wind, drag, the Magnus effect, penetration,
homing, and any custom movement code of your own. A projectile with drag will fall short of its
preview, by more the further it flies.

---

## 11. Class and API overview

| Class | Base | What it is for |
|---|---|---|
| `UArcCastSubsystem` | `UTickableWorldSubsystem` | The engine of the plugin: simulates, holds and draws the arcs. One per world. Ticks in the editor |
| `UArcCastComponent` | `USceneComponent` | The five-minute path. Hang it on a hand, a weapon or a socket; it keeps one arc updated and broadcasts verdict changes |
| `UArcCastProfile` | `UPrimaryDataAsset` | Flight behaviour *and* appearance in one asset |
| `UArcCastStatics` | `UBlueprintFunctionLibrary` | Every feature from Blueprint, without C++ |
| `UArcCastSettings` | `UDeveloperSettings` | Project-wide defaults plus runtime setters |
| `AArcCastHUD` | `AHUD` | Ready-made HUD class for a project that has none yet |
| `UArcCastHUDComponent` | `UActorComponent` | Add to *your* HUD and call `DrawArcs` from your own `DrawHUD` |

Types: `FArcCastRequest` (input), `FArcCastPath` (result), `FArcCastStats` (measured cost),
`EArcCastVerdict`, `EArcCastLineStyle`.

### `UArcCastComponent` (Scene Component)

| Node | Purpose |
|---|---|
| `Set Preview Enabled (bool)` | Show/hide this component's arc |
| `Set Profile (Profile)` | Swap the profile |
| `Set Aim Direction (Vector)` | Aim along a world direction; zero returns to the forward vector |
| `Set Launch Speed Override (float)` | Speed in cm/s; ≤ 0 hands it back to the profile |
| `Set Max Bounces Override (int)` | Bounce count; < 0 hands it back to the profile |
| `Get Path` / `Get Verdict` | Last tick's result |
| `Get Impact Point And Normal` | Landing point and surface normal |
| `Get Impact Actor` | What it lands on, or null |
| `Get Launch Velocity` / `Get Launch Location` | **Hand these to your throwing code** |
| `Get Arc Id` | The subsystem id this component's arc lives under |
| `Is Preview Enabled` | Current state |
| `On Verdict Changed` | Event — fires on change, not per frame |
| `On Impact Actor Changed` | Event — fires on change, not per frame |

Properties: `Profile`, `bPreviewEnabled`, `AimDirectionOverride`, `LaunchSpeedOverride`,
`MaxBouncesOverride`, `MaxRange`, `AdditionalIgnoredActors`, `LaunchOffset`.

The owning actor is always ignored by the sweep without being asked.

### `UArcCastStatics` (Blueprint library)

| Node | Purpose |
|---|---|
| `Predict Arc` | Simulate once, draw nothing |
| `Predict Arc From Direction` | The same, from a direction and a speed |
| `Show Arc From` | Register an arc for drawing; returns its id |
| `Hide Arc` / `Clear Arcs` | Stop drawing one / all |
| `Get Arc` | The path registered under an id |
| `Get Last Verdict` | Verdict of the most recent path in this world |
| `Get Impact Point And Normal` | Read a path's landing point |
| `Get Arc Impact Actor` | Read a path's impact actor (held weakly) |
| `Set Arc Cast Enabled` / `Is Arc Cast Enabled` | Master switch |
| `Get Arc Cast Stats` | Last frame's measured cost |
| `Set Show Arc Cast Stats` | The on-screen box |
| `Set Bounce Override` | Force a bounce count on every arc; −1 clears |
| `Set Line Style Override` / `Clear Line Style Override` | Force a style on every arc |
| `Get Builtin Profile` | `Grenade`, `Teleport`, `Arrow`, `Bouncy` |
| `Verdict To Text` | Readable verdict, for a HUD line |

### `UArcCastSubsystem` (world subsystem)

`Simulate Arc`, `Show Arc`, `Hide Arc`, `Clear Arcs`, `Get Arc`, `Get Active Arc Count`,
`Get Last Verdict`, `Get Last Path`, `Draw Arcs (Canvas)`, `Get Stats`.

In C++: `UArcCastSubsystem::Get(WorldContextObject)`.

### `FArcCastRequest`

`StartLocation`, `LaunchVelocity`, `Profile`, `IgnoredActors`, `MaxBouncesOverride` (−1 = profile),
`MaxSimTimeOverride` (≤ 0 = profile), `SplashRadiusOverride` (< 0 = profile), `MaxRange`
(0 = unlimited, measured **along the path**, not as a straight line).

### `FArcCastPath`

`Points`, `BounceIndex`, `Occluded`, `RingPoints`, `ImpactPoint`, `ImpactNormal`, `Verdict`,
`TotalTime`, `TotalDistance`, `BounceCount`, `StepCount`, `Id`, `bHasResult`. The impact actor is
held weakly and read with `Get Arc Impact Actor`.

`Points` and `BounceIndex` are parallel arrays: `BounceIndex[i]` is how many bounces had happened
when the segment leaving `Points[i]` was integrated.

### `FArcCastStats`

`ActiveArcs`, `SimSteps`, `SimTraces`, `RingTraces`, `OcclusionTraces`, `TotalTraces`, `LinesDrawn`,
`SimMilliseconds`, `DrawMilliseconds`, `EffectiveStepSeconds`, `bBudgetCoarsened`.

---

## 12. Code examples

### Adding the plugin to a C++ module

`ArcCast` is a normal runtime module. Add it to your own module's dependencies:

```csharp
// MyGame.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine", "InputCore",
    "ArcCast",
});
```

Headers: `ArcCastSubsystem.h`, `ArcCastComponent.h`, `ArcCastStatics.h`, `ArcCastProfile.h`,
`ArcCastTypes.h`.

### a) A weapon with a preview, and a throw that matches it

```cpp
#include "ArcCastComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AGrenadeLauncher::AGrenadeLauncher()
{
    ArcPreview = CreateDefaultSubobject<UArcCastComponent>(TEXT("ArcPreview"));
    ArcPreview->SetupAttachment(RootComponent, TEXT("Muzzle"));
    ArcPreview->bPreviewEnabled = false;   // only while aiming
    ArcPreview->LaunchOffset    = 20.0f;   // clear of the muzzle geometry
}

void AGrenadeLauncher::StartAiming()
{
    ArcPreview->SetPreviewEnabled(true);
}

void AGrenadeLauncher::Throw()
{
    // The two values the preview actually used. Spawning with anything else is how a
    // projectile ends up somewhere the player did not aim.
    const FVector SpawnLocation = ArcPreview->GetLaunchLocation();
    const FVector LaunchVelocity = ArcPreview->GetLaunchVelocity();

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (AGrenade* Grenade = GetWorld()->SpawnActor<AGrenade>(
            GrenadeClass, SpawnLocation, LaunchVelocity.Rotation(), Params))
    {
        Grenade->GetProjectileMovement()->Velocity = LaunchVelocity;
    }

    ArcPreview->SetPreviewEnabled(false);
}
```

### b) Reacting to the verdict instead of polling it

```cpp
void AGrenadeLauncher::BeginPlay()
{
    Super::BeginPlay();
    ArcPreview->OnVerdictChanged.AddDynamic(this, &AGrenadeLauncher::HandleVerdictChanged);
}

void AGrenadeLauncher::HandleVerdictChanged(EArcCastVerdict NewVerdict)
{
    bThrowAllowed = (NewVerdict == EArcCastVerdict::Valid);
    OnCrosshairStateChanged.Broadcast(NewVerdict);   // recolour once, not every frame
}
```

### c) Prediction with no drawing — an AI deciding whether the throw is worth it

```cpp
#include "ArcCastStatics.h"

bool UBTTask_ThrowGrenade::WouldReachCover(AAIController* Controller,
                                           const FVector& CoverLocation) const
{
    APawn* Pawn = Controller->GetPawn();
    const FVector Start = Pawn->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    const FVector Velocity = SolveThrowVelocity(Start, CoverLocation);

    const FArcCastPath Path = UArcCastStatics::PredictArc(
        Pawn, Start, Velocity, GrenadeProfile, { Pawn }, /*MaxRange=*/3000.f);

    // Nothing was registered and nothing was drawn - this is a pure query, safe on a
    // dedicated server where no canvas exists.
    return Path.bHasResult
        && Path.Verdict == EArcCastVerdict::Valid
        && FVector::Dist(Path.ImpactPoint, CoverLocation) < AcceptableRadius;
}
```

### d) The same through the subsystem, with the full request struct

```cpp
#include "ArcCastSubsystem.h"

if (UArcCastSubsystem* Arcs = UArcCastSubsystem::Get(this))
{
    FArcCastRequest Request;
    Request.StartLocation       = MuzzleLocation;
    Request.LaunchVelocity      = AimDirection * 1600.f;
    Request.Profile             = MyProfile;
    Request.IgnoredActors       = { this };
    Request.MaxBouncesOverride  = 0;        // stop at the first hit, like the engine does
    Request.SplashRadiusOverride = 450.f;   // this one ability has a bigger blast
    Request.MaxRange            = 2500.f;

    const FArcCastPath Path = Arcs->SimulateArc(Request);
    if (Path.Verdict == EArcCastVerdict::TooSteep)
    {
        RefuseWithReason(NSLOCTEXT("MyGame", "TooSteep", "You cannot land there."));
    }
}
```

### e) Driving an arc by hand, without the component

Useful when the aim comes from somewhere the component cannot see — a spline, a cursor in an RTS, a
solved lob to a target actor.

```cpp
void AAbilityTargeter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bTargeting)
    {
        if (ArcId != 0)
        {
            UArcCastStatics::HideArc(this, ArcId);
            ArcId = 0;
        }
        return;
    }

    // Pass the id back in every frame: the arc is replaced in place instead of piling up.
    ArcId = UArcCastStatics::ShowArcFrom(
        this, GetCastOrigin(), SolveVelocityToCursor(), AbilityProfile, { GetOwner() },
        /*MaxRange=*/0.f, ArcId);
}
```

### f) Drawing from your own HUD class

```cpp
// MyHUD.h
UPROPERTY(VisibleAnywhere)
TObjectPtr<UArcCastHUDComponent> ArcCastComponent;

// MyHUD.cpp
AMyHUD::AMyHUD()
{
    ArcCastComponent = CreateDefaultSubobject<UArcCastHUDComponent>(TEXT("ArcCast"));
}

void AMyHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawWorldSpaceMarkers();
    ArcCastComponent->DrawArcs(Canvas);   // arcs above the markers...
    DrawCrosshair();                      // ...and below the crosshair
}
```

If you take this path, switch `Auto Spawn HUD Component` off in the project settings — not because it
would double-draw (the frame guard prevents that) but so the draw order is unambiguous.

### g) Reading the measured cost

```cpp
const FArcCastStats Stats = UArcCastStatics::GetArcCastStats(this);
UE_LOG(LogTemp, Log, TEXT("ArcCast: %d arcs, %d steps, %d traces (%d occl), sim %.2f ms, draw %.2f ms%s"),
    Stats.ActiveArcs, Stats.SimSteps, Stats.TotalTraces, Stats.OcclusionTraces,
    Stats.SimMilliseconds, Stats.DrawMilliseconds,
    Stats.bBudgetCoarsened ? TEXT(" (coarsened)") : TEXT(""));
```

### h) Changing settings at runtime

```cpp
#include "ArcCastSettings.h"

UArcCastSettings::SetOcclusionAware(false);   // tight frame: drop the camera traces
UArcCastSettings::SetGlobalOpacity(0.5f);     // dim the preview during a cutscene
UArcCastSettings::SetEnabled(false);          // simulate nothing, draw nothing
```

These push into every live world immediately and deliberately do **not** rewrite `DefaultGame.ini`.

---

## 13. Console commands

| Command | Effect |
|---|---|
| `ArcCast.Test` | Throw a demonstration arc from the current view with the default profile |
| `ArcCast.Bounces <n>` | Force the bounce count on every arc; `-1` hands it back to the profiles |
| `ArcCast.Occlusion 0\|1` | Camera-to-arc occlusion traces |
| `ArcCast.Style solid\|dashed\|dots\|clear` | Force a line style on every arc |
| `ArcCast.Clear` | Drop every registered arc |
| `ArcCast.Stats` | Print steps, traces (split by kind) and milliseconds to the log |
| `ArcCast.Stats 0\|1` | Show or hide the on-screen statistics box |

Typed into an editor viewport with no PIE running, these reach every live world — which is where the
switches are most useful.

---

## 14. Project settings

`Project Settings → Plugins → ArcCast`, stored in `DefaultGame.ini`.

| Setting | Default | Notes |
|---|---|---|
| `bEnabled` | `true` | Master switch: simulates nothing, draws nothing |
| `DefaultProfile` | empty | Soft pointer; empty falls back to the built-in below |
| `DefaultBuiltinProfile` | `Grenade` | `Grenade`, `Teleport`, `Arrow`, `Bouncy` |
| `bOcclusionAware` | `true` | The part that costs traces |
| `OccludedOpacity` | `0.25` | Opacity of a stretch the camera cannot see |
| `OcclusionEveryNthPoint` | `2` | Halves the occlusion trace count |
| `OcclusionEndOffset` | `8` cm | Stops a surface occluding its own ring |
| `MaxSimStepsPerFrame` | `300` | Over budget, the step is coarsened — nothing is dropped |
| `MaxPointsPerArc` | `1024` | Absolute ceiling per arc |
| `GlobalOpacity` | `1.0` | Dims everything ArcCast draws |
| `bAutoSpawnHUDComponent` | `true` | Draw through `AHUD::OnHUDPostRender`; ships |
| `bDrawInEditorViewport` | `true` | Editor-only second path; compiled out of a cooked build |
| `bShowStatsByDefault` | `false` | Same as `ArcCast.Stats 1` |

Every one of these has a `BlueprintCallable` runtime setter on `UArcCastSettings` that pushes the
value into every live world. Runtime setters deliberately do **not** rewrite `DefaultGame.ini`.

---

## 15. Cost, budget and limits

### What one arc costs

| Work | Traces |
|---|---|
| Integration | one sweep per step — `MaxSimTime / FixedStepSeconds`, so 120 at 4 s and 1/30 s, fewer once it lands |
| Ground probe | one line trace, and only when the arc never hit anything |
| Splash ring | one line trace per ring point, so 33 at `RingSegments = 32` |
| Occlusion | `Points / OcclusionEveryNthPoint` line traces per **drawn frame** |

Drawing is canvas line items — no draw call per segment, no material, no mesh, no allocation per
frame beyond the point arrays.

### The budget

`MaxSimStepsPerFrame` (300) is a hard cap on integration steps across **every live arc** in a frame.
Over budget, ArcCast **coarsens the step size for that frame** rather than dropping arcs or deferring
them to the next frame. A busy frame costs accuracy, not frame time — a preview that stutters or
vanishes under load is worse than one that is slightly coarser. The stats box says `(coarsened)` when
it happens.

### Measure it yourself

`ArcCast.Stats` prints steps, traces split into `sim` / `ring` / `occlusion`, simulate milliseconds,
draw milliseconds and canvas line count. Turning `bOcclusionAware` off makes the occlusion count fall
to zero, and you can watch it happen in the same session. **Do not take a number from this page —
take it from your own scene.**

### Limits

* One arc per `UArcCastComponent`. Several arcs at once means several components, or `Show Arc From`
  with your own ids.
* The preview is **local**. It is not replicated and should not be — it belongs on the controlling
  client. See the store description for why.
* `BounceIndex` is a byte, so bounce 255 is the ceiling. `MaxBounces` clamps to 8 in the editor.
* The splash ring is conformed with vertical traces, so it does not wrap around a vertical wall or an
  overhang; it lies on whatever is directly above or below each ring point.
* Occlusion is a single line trace per sampled point, not a visibility volume: a thin railing between
  the camera and the arc will mark a stretch occluded.
* The editor-viewport draw path only runs while the viewport is in **Game View** (`G`), because it
  is registered under the engine's `Game` show flag. In an ordinary perspective viewport it draws
  nothing.

---

## 16. Troubleshooting

**No arc at all.**
Check `Project Settings → Plugins → ArcCast → bEnabled`, then that the component's `bPreviewEnabled`
is on. Then run `ArcCast.Test` — if the test arc appears, the problem is in how the component is
aimed, not in the drawing.

**Arc in PIE but not in the editor viewport.**
**Press `G` for Game View first** — that is the answer nine times out of ten. The editor path hangs
off the `Game` show flag, which is off in an ordinary perspective viewport. If it still does not
appear: `bDrawInEditorViewport` must be on, and the component must have `bTickInEditor` (it does by
default; a Blueprint actor that recreates the component may have lost the flag).

**Arc in the editor viewport but not in a packaged build.**
That is the `bDrawInEditorViewport` path, which is editor-only by design. The shipping path is
`bAutoSpawnHUDComponent`, `AArcCastHUD`, or `UArcCastHUDComponent` — see
[section 4](#4-the-three-drawing-paths).

**The arc starts by hitting the thrower.**
Put the actor in `AdditionalIgnoredActors`, or raise `LaunchOffset`. The owning actor is already
ignored; a separately-spawned weapon actor is not.

**Everything is drawn faint and dashed.**
Occlusion is finding something between the camera and the arc. Try `ArcCast.Occlusion 0` to confirm,
then check that the arc's collision channel is not being blocked by an invisible volume the camera
sits behind.

**The ring floats above or sinks into the floor.**
`RingConformDistance` too small to reach the surface, or `RingSurfaceOffset` too large. On a very
uneven surface, raise `RingSegments`.

**Everything is `NoGround` with `bRequireNavMesh` on.**
The project has no navigation system, or no built navigation data at the landing point. That is the
honest answer, not a failure — build navigation, or turn `bRequireNavMesh` off.

**The projectile does not follow the arc.**
Section 10. Nine times out of ten it is `GravityScale` against `Projectile Gravity Scale`, or a
launch velocity that was recomputed instead of taken from `Get Launch Velocity`.

**The arc flickers between two colours on a slope.**
The landing point is sitting exactly on `MaxWalkableSlopeAngle`. Move the angle a couple of degrees
away from the surface you are testing on.

---

## 17. The demo map

Everything below ships inside the plugin, under `Content/ArcCast/`, so it travels with the plugin
folder and needs no project setup.

Open **`/ArcCast/ArcCast/Maps/L_ArcCastDemo`** and press Play. The map is a small throwing range: a
flat pad, a flight of stairs, a 50-degree ramp, a wall the arc passes behind, and an open ledge.

| Asset | What it is for |
|---|---|
| `Maps/L_ArcCastDemo` | The throwing range. Its World Settings point at `BP_ArcCastDemoGameMode`. |
| `Blueprints/BP_ArcCastThrower` | A pedestal with a barrel and a **`UArcCastComponent`** on it. The four `Shot*` events aim the component by setting its relative rotation; the four `Profile*` events swap the profile asset. This is the only actor in the demo that touches the plugin. |
| `Blueprints/BP_ArcCastDemoDirector` | On Begin Play: takes the demo camera as the view target, spawns the panel widget and switches the player controller to UI input. |
| `Blueprints/BP_ArcCastDemoHUD` | A Blueprint child of **`AArcCastHUD`** — the ready-made HUD class, set as the game mode's HUD class. |
| `Blueprints/BP_ArcCastDemoGameMode` | Wires the HUD class in. Nothing else. |
| `UI/WBP_ArcCastPanel` | The on-screen panel. Its buttons call the thrower's events; its Tick reads `Get Verdict` off the component and prints it through `Verdict to Text`. |
| `Profiles/DA_Arc_*` | Grenade, Teleport, Arrow and Bouncy as **assets** — the same four shapes as the built-in code profiles, but editable. Duplicate one to start your own. |
| `Materials/` | `M_ArcCastSurface` plus the instances the range geometry uses. Nothing here is required by the plugin. |

**What each button shows**

- **Landing Pad** — a `Valid` landing: green arc, splash ring conformed to the pad, one bounce.
- **Stairs** — the ring following the treads instead of floating as a flat disc.
- **Steep Ramp** — `TooSteep`: the arc and the ring turn amber because the surface leans past the
  profile's `MaxWalkableSlopeAngle`.
- **Over the Ledge** — `NoGround`: the arc turns red and the stretch that falls behind the geometry
  is drawn dashed and faint rather than shining through it.
- **Grenade / Teleport / Arrow / Bouncy** — the same aim through four profile assets.
- **Stats Box** — toggles the on-screen cost readout (`Set Show ArcCast Stats`, same as
  `ArcCast.Stats 1`).
- **Occlusion** — toggles `bOcclusionAware`; watch `occl` in the stats box fall to zero.

The demo takes no screenshots, spawns no projectiles and writes nothing to disk. Deleting
`Content/ArcCast/` removes the whole demo and leaves the plugin fully functional — the four built-in
code profiles mean nothing depends on the shipped assets.

---

## 18. Support

Documentation: <https://github.com/SimulatedFlow/ue-plugin-ArcCast>
Support: <mailto:teufelsilvan@gmail.com>

Copyright 2026 Silvan Teufel. All Rights Reserved.
