// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TrashStreamerBpLib.generated.h"

/**
 * 
 */
UCLASS()
class TRASHLEVELSTREAMER_API UTrashStreamerBpLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", Latent = "", LatentInfo = "LatentInfo"), Category = "Game")
	static void LoadTrashStreamLevel(const UObject* WorldContextObject, FName LevelName, bool bMakeVisibleAfterLoad, bool bShouldBlockOnLoad, FLatentActionInfo LatentInfo);
	
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", Latent = "", LatentInfo = "LatentInfo"), Category = "Game")
	static void UnloadTrashStreamLevel(const UObject* WorldContextObject, FName LevelName, FLatentActionInfo LatentInfo, bool bShouldBlockOnUnload);
};
