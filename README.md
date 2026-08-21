# ArcCast — Trajectory & Landing Preview

**Unreal Engine 5.8 · one runtime module · Win64 / Mac / Linux**

The throw arc you can actually ship: bounce-chained trajectory prediction with a landing verdict, a
splash ring conformed to the geometry, and occlusion-aware drawing — on `UCanvas`, so it survives a
Shipping build.

The engine already predicts the path. It just cannot draw it: the only renderer is `DrawDebugLine`,
and `ENABLE_DRAW_DEBUG` is 0 in Shipping. ArcCast draws through `AHUD` instead — no material, no
mesh, no Niagara, no asset of any kind.

**ArcCast does not throw anything.** No projectile, no damage, no replication. It shows where the
thing your game is about to launch would end up.

---

## Quick start

1. Copy `ArcCast/` into your project's `Plugins/` folder, restart the editor.
2. Add the **ArcCast** component to the actor that aims. Attach it to the hand or the muzzle.
3. Leave `Profile` empty — the built-in `Grenade` profile is used, so an arc appears immediately.

The arc is visible in PIE and in a packaged build through whatever HUD class you already have, and
in the editor viewport with no play session — for that last one press `G` for Game View, because the
editor path hangs off the engine's `Game` show flag.

To throw for real, hand `Get Launch Location` and `Get Launch Velocity` to your spawn — then the
projectile follows the arc that was drawn.

---

## What it does past the engine's own prediction

* **Bounce chain** — the simulation continues past the first hit; later stretches draw thinner and
  fainter.
* **Landing verdict** — `Valid` / `TooSteep` / `Blocked` / `OutOfRange` / `NoGround`, colouring the
  arc and broadcast as an event. With the navigation check on, it is a teleport target indicator.
* **Conformed splash ring** — every ring point is probed onto the surface, so the ring lies on
  stairs and slopes instead of hovering through them.
* **Occlusion-aware drawing** — hidden stretches are drawn faint and dashed, not removed.
* **Prediction with no drawing** — `Predict Arc` returns the whole result and draws nothing.

---

## Console commands

`ArcCast.Test` · `ArcCast.Bounces <n>` · `ArcCast.Occlusion 0|1` ·
`ArcCast.Style solid|dashed|dots|clear` · `ArcCast.Clear` · `ArcCast.Stats [0|1]`

---

## Documentation

Full documentation: [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md)
Store description and honest limits: [`Docs/Fab-Store-Description.md`](Docs/Fab-Store-Description.md)

Online: <https://github.com/SimulatedFlow/ue-plugin-ArcCast>
Support: <mailto:teufelsilvan@gmail.com>

Copyright 2026 Simulated Flow. All Rights Reserved.

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

_This plugin does not have its own Fab listing yet — the store link above is where everything we currently sell lives._

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Simulated Flow. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
