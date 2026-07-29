#pragma once

#include "CoreMinimal.h"

class APBRELOADED_API FAPBSecretProvider final
{
public:
	static bool Initialize(FString& OutError);
	static bool PreflightRole();
	static bool IsInitialized();

	static const FString& TicketSecret();
	static const FString& HandoffSecret();
	static const FString& RelaySecret();
	static const FString& SaveSecret();
};
