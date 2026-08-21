// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ArcCastHUD.h"

#include "ArcCastHUDComponent.h"

AArcCastHUD::AArcCastHUD()
{
	ArcComponent = CreateDefaultSubobject<UArcCastHUDComponent>(TEXT("ArcCastDraw"));
}

void AArcCastHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bDrawArcs && ArcComponent)
	{
		ArcComponent->DrawArcs(Canvas);
	}
}
