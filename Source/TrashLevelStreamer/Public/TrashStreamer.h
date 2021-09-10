#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Engine/LatentActionManager.h"
#include "LatentActions.h"
#include "Engine/LevelStreaming.h"
#include "TrashStreamer.generated.h"

class ALevelScriptActor;
class ALevelStreamingVolume;
class ULevel;
class ULevelStreaming;

// Stream Level Action
class TRASHLEVELSTREAMER_API FTrashStreamLevelAction : public FPendingLatentAction
{
public:
	bool			bLoading;
	bool			bMakeVisibleAfterLoad;
	bool			bShouldBlock;
	TWeakObjectPtr<ULevelStreaming> Level;
	FName			LevelName;

	FLatentActionInfo LatentInfo;

	FTrashStreamLevelAction(bool bIsLoading, const FName& InLevelName, bool bIsMakeVisibleAfterLoad, bool bShouldBlock, const FLatentActionInfo& InLatentInfo, UWorld* World);

	/**
	 * Given a level name, returns level name that will work with Play on Editor or Play on Console
	 *
	 * @param	InLevelName		Raw level name (no UEDPIE or UED<console> prefix)
	 */
	static FString MakeSafeLevelName(const FName& InLevelName, UWorld* InWorld);

	/**
	 * Helper function to potentially find a level streaming object by name and cache the result
	 *
	 * @param	LevelName							Name of level to search streaming object for in case Level is NULL
	 * @return	level streaming object or NULL if none was found
	 */
	static ULevelStreaming* FindAndCacheLevelStreamingObject(const FName LevelName, UWorld* InWorld);

	/**
	 * Handles "Activated" for single ULevelStreaming object.
	 *
	 * @param	LevelStreamingObject	LevelStreaming object to handle "Activated" for.
	 */
	void ActivateLevel(ULevelStreaming* LevelStreamingObject);
	/**
	 * Handles "UpdateOp" for single ULevelStreaming object.
	 *
	 * @param	LevelStreamingObject	LevelStreaming object to handle "UpdateOp" for.
	 *
	 * @return true if operation has completed, false if still in progress
	 */
	bool UpdateLevel(ULevelStreaming* LevelStreamingObject);

	virtual void UpdateOperation(FLatentResponse& Response) override;

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override;
#endif
};

// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTrashLevelStreamingLoadedStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTrashLevelStreamingVisibilityStatus);

UCLASS()
class TRASHLEVELSTREAMER_API UTrashStreamer : public ULevelStreaming
{
	GENERATED_BODY()
	
};
