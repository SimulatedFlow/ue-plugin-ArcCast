// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ArcCastHUDComponent.h"

#include "ArcCastSubsystem.h"
#include "Engine/World.h"

UArcCastHUDComponent::UArcCastHUDComponent()
{
	// Nothing to tick: the subsystem owns the arcs, this component only forwards a canvas.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = false;
}

void UArcCastHUDComponent::DrawArcs(UCanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		if (UArcCastSubsystem* Subsystem = World->GetSubsystem<UArcCastSubsystem>())
		{
			Subsystem->DrawArcs(Canvas);
		}
	}
}
