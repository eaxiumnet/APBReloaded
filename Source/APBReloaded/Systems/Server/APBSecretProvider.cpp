#include "APBSecretProvider.h"

#include "APBCrypto.h"
#include "APBReloaded.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

#include <cstring>

namespace
{
	constexpr const TCHAR* SecretEnvironmentVariable = TEXT("APB_DEPLOYMENT_SECRET");
	constexpr const TCHAR* SecretFileEnvironmentVariable = TEXT("APB_DEPLOYMENT_SECRET_FILE");

	struct FSecretState
	{
		bool bInitialized = false;
		FString Ticket;
		FString Handoff;
		FString Relay;
		FString Save;
	};

	FSecretState& SecretState()
	{
		static FSecretState State;
		return State;
	}

	FString DeriveSecret(const TArray<uint8>& Root, const ANSICHAR* Purpose)
	{
		const auto Derived = apb::hmac_sha256(
			Root.GetData(), static_cast<size_t>(Root.Num()),
			reinterpret_cast<const uint8_t*>(Purpose), std::strlen(Purpose));
		return UTF8_TO_TCHAR(apb::hex_encode(Derived.data(), Derived.size()).c_str());
	}

	FString DeploymentRole()
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		if (FParse::Param(CommandLine, TEXT("WorldServer")))
		{
			return TEXT("world");
		}

		FString District;
		const FString FullCommandLine(CommandLine);
		if (FParse::Value(CommandLine, TEXT("District="), District)
			|| FParse::Value(CommandLine, TEXT("DistrictId="), District)
			|| FullCommandLine.Contains(TEXT("?listen"), ESearchCase::IgnoreCase))
		{
			return TEXT("district");
		}
		return FString();
	}
}

bool FAPBSecretProvider::Initialize(FString& OutError)
{
	FSecretState& State = SecretState();
	if (State.bInitialized)
	{
		OutError.Reset();
		return true;
	}

	FString Material = FPlatformMisc::GetEnvironmentVariable(SecretEnvironmentVariable);
	FString Source = TEXT("environment");
	if (Material.IsEmpty())
	{
		const FString SecretFile = FPlatformMisc::GetEnvironmentVariable(SecretFileEnvironmentVariable);
		if (SecretFile.IsEmpty())
		{
			OutError = TEXT("missing_secret");
			return false;
		}
		if (!FFileHelper::LoadFileToString(Material, *SecretFile))
		{
			OutError = TEXT("secret_file_unreadable");
			return false;
		}
		Source = TEXT("file");
	}

	Material.TrimStartAndEndInline();
	const std::string MaterialUtf8(TCHAR_TO_UTF8(*Material));
	if (!apb::is_valid_secret_material(MaterialUtf8))
	{
		OutError = TEXT("malformed_secret");
		return false;
	}

	const std::vector<uint8_t> RootBytes = apb::hex_decode(MaterialUtf8);
	TArray<uint8> Root;
	Root.Append(RootBytes.data(), static_cast<int32>(RootBytes.size()));
	State.Ticket = DeriveSecret(Root, "apb/ticket/v1");
	State.Handoff = DeriveSecret(Root, "apb/handoff/v1");
	State.Relay = DeriveSecret(Root, "apb/relay/v1");
	State.Save = DeriveSecret(Root, "apb/save/v1");
	State.bInitialized = true;
	OutError.Reset();
	UE_LOG(LogAPBReloaded, Log, TEXT("DEPLOYMENT_SECRET_PROVIDER_READY source=%s purposes=4"), *Source);
	return true;
}

bool FAPBSecretProvider::PreflightRole()
{
	const FString Role = DeploymentRole();
	if (Role.IsEmpty())
	{
		return true;
	}

	FString Error;
	if (Initialize(Error))
	{
		return true;
	}

	UE_LOG(LogAPBReloaded, Error, TEXT("DEPLOYMENT_SECRET_PROVIDER_HALT reason=%s role=%s listener=not_started"),
		*Error, *Role);
	FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("FAPBSecretProvider::PreflightRole"));
	return false;
}

bool FAPBSecretProvider::IsInitialized()
{
	return SecretState().bInitialized;
}

const FString& FAPBSecretProvider::TicketSecret()
{
	return SecretState().Ticket;
}

const FString& FAPBSecretProvider::HandoffSecret()
{
	return SecretState().Handoff;
}

const FString& FAPBSecretProvider::RelaySecret()
{
	return SecretState().Relay;
}

const FString& FAPBSecretProvider::SaveSecret()
{
	return SecretState().Save;
}
