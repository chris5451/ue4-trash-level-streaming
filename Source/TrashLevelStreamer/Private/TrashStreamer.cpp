// Fill out your copyright notice in the Description page of Project Settings.
#include "TrashStreamer.h"
#include "Engine/LevelStreaming.h"
#include "ContentStreaming.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "UObject/ReferenceChainSearch.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "EngineGlobals.h"
#include "Engine/Level.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "UObject/ObjectRedirector.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingPersistent.h"
#include "Engine/LevelStreamingVolume.h"
#include "LevelUtils.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif
#include "Engine/LevelStreamingDynamic.h"
#include "Components/BrushComponent.h"
#include "Engine/CoreSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "SceneInterface.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Serialization/LoadTimeTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogLevelStreaming, Log, All);

#define LOCTEXT_NAMESPACE "World"

//int32 ULevelStreamingDynamic::UniqueLevelInstanceId = 0;


FTrashStreamLevelAction::FTrashStreamLevelAction(bool bIsLoading, const FName& InLevelName, bool bIsMakeVisibleAfterLoad, bool bInShouldBlock, const FLatentActionInfo& InLatentInfo, UWorld* World)
	: bLoading(bIsLoading)
	, bMakeVisibleAfterLoad(bIsMakeVisibleAfterLoad)
	, bShouldBlock(bInShouldBlock)
	, LevelName(InLevelName)
	, LatentInfo(InLatentInfo)
{
	ULevelStreaming* LocalLevel = FindAndCacheLevelStreamingObject(LevelName, World);
	Level = LocalLevel;
	ActivateLevel(LocalLevel);
}

void FTrashStreamLevelAction::UpdateOperation(FLatentResponse& Response)
{
	ULevelStreaming* LevelStreamingObject = Level.Get(); // to avoid confusion.
	const bool bIsLevelValid = LevelStreamingObject != nullptr;
	UE_LOG(LogLevelStreaming, Verbose, TEXT("FTrashStreamLevelAction::UpdateOperation() LevelName %s, bIsLevelValid %d"), *LevelName.ToString(), (int32)bIsLevelValid);
	if (bIsLevelValid)
	{
		bool bIsOperationFinished = UpdateLevel(LevelStreamingObject);
		Response.FinishAndTriggerIf(bIsOperationFinished, LatentInfo.ExecutionFunction, LatentInfo.Linkage, LatentInfo.CallbackTarget);
	}
	else
	{
		Response.FinishAndTriggerIf(true, LatentInfo.ExecutionFunction, LatentInfo.Linkage, LatentInfo.CallbackTarget);
	}
}

#if WITH_EDITOR
FString FTrashStreamLevelAction::GetDescription() const
{
	return FString::Printf(TEXT("Streaming Level in progress...(%s)"), *LevelName.ToString());
}
#endif

/**
* Helper function to potentially find a level streaming object by name
*
* @param	LevelName							Name of level to search streaming object for in case Level is NULL
* @return	level streaming object or NULL if none was found
*/
ULevelStreaming* FTrashStreamLevelAction::FindAndCacheLevelStreamingObject(const FName LevelName, UWorld* InWorld)
{
	// Search for the level object by name.
	if (LevelName != NAME_None)
	{
		FString SearchPackageName = MakeSafeLevelName(LevelName, InWorld);
		if (FPackageName::IsShortPackageName(SearchPackageName))
		{
			// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
			SearchPackageName = TEXT("/") + SearchPackageName;
		}

		for (ULevelStreaming* LevelStreaming : InWorld->GetStreamingLevels())
		{
			// We check only suffix of package name, to handle situations when packages were saved for play into a temporary folder
			// Like Saved/Autosaves/PackageName
			if (LevelStreaming &&
				LevelStreaming->GetWorldAssetPackageName().EndsWith(SearchPackageName, ESearchCase::IgnoreCase))
			{
				return LevelStreaming;
			}
		}
	}

	return NULL;
}

/**
 * Given a level name, returns a level name that will work with Play on Editor or Play on Console
 *
 * @param	InLevelName		Raw level name (no UEDPIE or UED<console> prefix)
 * @param	InWorld			World in which to check for other instances of the name
 */
FString FTrashStreamLevelAction::MakeSafeLevelName(const FName& InLevelName, UWorld* InWorld)
{
	// Special case for PIE, the PackageName gets mangled.
	if (!InWorld->StreamingLevelsPrefix.IsEmpty())
	{
		FString PackageName = FPackageName::GetShortName(InLevelName);
		if (!PackageName.StartsWith(InWorld->StreamingLevelsPrefix))
		{
			PackageName = InWorld->StreamingLevelsPrefix + PackageName;
		}

		if (!FPackageName::IsShortPackageName(InLevelName))
		{
			PackageName = FPackageName::GetLongPackagePath(InLevelName.ToString()) + TEXT("/") + PackageName;
		}

		return PackageName;
	}

	return InLevelName.ToString();
}
/**
* Handles "Activated" for single ULevelStreaming object.
*
* @param	LevelStreamingObject	LevelStreaming object to handle "Activated" for.
*/
void FTrashStreamLevelAction::ActivateLevel(ULevelStreaming* LevelStreamingObject)
{
	if (LevelStreamingObject)
	{
		// Loading.
		if (bLoading)
		{
			UE_LOG(LogStreaming, Log, TEXT("Streaming in level %s (%s)..."), *LevelStreamingObject->GetName(), *LevelStreamingObject->GetWorldAssetPackageName());
			LevelStreamingObject->SetShouldBeLoaded(true);
			LevelStreamingObject->SetShouldBeVisible(LevelStreamingObject->GetShouldBeVisibleFlag() || bMakeVisibleAfterLoad);
			LevelStreamingObject->bShouldBlockOnLoad = bShouldBlock;
		}
		// Unloading.
		else
		{
			UE_LOG(LogStreaming, Log, TEXT("Streaming out level %s (%s)..."), *LevelStreamingObject->GetName(), *LevelStreamingObject->GetWorldAssetPackageName());
			LevelStreamingObject->SetShouldBeLoaded(false);
			LevelStreamingObject->SetShouldBeVisible(false);
			LevelStreamingObject->bShouldBlockOnUnload = bShouldBlock;
		}

		// If we have a valid world
		if (UWorld* LevelWorld = LevelStreamingObject->GetWorld())
		{
			const bool bShouldBeLoaded = LevelStreamingObject->ShouldBeLoaded();
			const bool bShouldBeVisible = LevelStreamingObject->ShouldBeVisible();

			UE_LOG(LogLevel, Log, TEXT("ActivateLevel %s %i %i %i"),
				*LevelStreamingObject->GetWorldAssetPackageName(),
				bShouldBeLoaded,
				bShouldBeVisible,
				bShouldBlock);

			// Notify players of the change
			for (FConstPlayerControllerIterator Iterator = LevelWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				if (APlayerController* PlayerController = Iterator->Get())
				{
					if (PlayerController->IsLocalController()) {
						PlayerController->LevelStreamingStatusChanged(
							LevelStreamingObject,
							bShouldBeLoaded,
							bShouldBeVisible,
							bShouldBlock,
							INDEX_NONE);
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogLevel, Warning, TEXT("Failed to find streaming level object associated with '%s'"), *LevelName.ToString());
	}
}

/**
* Handles "UpdateOp" for single ULevelStreaming object.
*
* @param	LevelStreamingObject	LevelStreaming object to handle "UpdateOp" for.
*
* @return true if operation has completed, false if still in progress
*/
bool FTrashStreamLevelAction::UpdateLevel(ULevelStreaming* LevelStreamingObject)
{
	// No level streaming object associated with this sequence.
	if (LevelStreamingObject == nullptr)
	{
		return true;
	}
	// Level is neither loaded nor should it be so we finished (in the sense that we have a pending GC request) unloading.
	else if ((LevelStreamingObject->GetLoadedLevel() == nullptr) && !LevelStreamingObject->ShouldBeLoaded())
	{
		return true;
	}
	// Level shouldn't be loaded but is as background level streaming is enabled so we need to fire finished event regardless.
	else if (LevelStreamingObject->GetLoadedLevel() && !LevelStreamingObject->ShouldBeLoaded() && !GUseBackgroundLevelStreaming)
	{
		return true;
	}
	// Level is both loaded and wanted so we finished loading.
	else if (LevelStreamingObject->GetLoadedLevel() && LevelStreamingObject->ShouldBeLoaded()
		// Make sure we are visible if we are required to be so.
		&& (!bMakeVisibleAfterLoad || LevelStreamingObject->GetLoadedLevel()->bIsVisible))
	{
		return true;
	}

	// Loading/ unloading in progress.
	return false;
}