#include "APBVerifiedAssetRegistry.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "APBCrypto.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"

#include <array>
#include <string>

namespace
{
	bool IsValidObjectPath(const FString& ObjectPath)
	{
		return ObjectPath.StartsWith(TEXT("/Game/"))
			&& ObjectPath.Contains(TEXT("."))
			&& !ObjectPath.Contains(TEXT(".."))
			&& !ObjectPath.Contains(TEXT("\\"))
			&& !ObjectPath.Contains(TEXT(" "));
	}

	bool IsSupportedClass(const FString& ClassName)
	{
		static const TSet<FString> SupportedClasses = {
			TEXT("StaticMesh"), TEXT("SkeletalMesh"), TEXT("Texture2D"),
			TEXT("Material"), TEXT("MaterialInstanceConstant"), TEXT("SoundWave"),
			TEXT("SoundCue"), TEXT("AnimSequence"), TEXT("MediaSource")
		};
		return SupportedClasses.Contains(ClassName);
	}

	bool IsSupportedSourceBuild(const FString& SourceBuild)
	{
		static const TSet<FString> SupportedSourceBuilds = {
			TEXT("retail"), TEXT("2011"), TEXT("2011+retail"), TEXT("apbdb")
		};
		return SupportedSourceBuilds.Contains(SourceBuild);
	}

	bool IsAllowlistProvenanceBound(const TArray<uint8>& AllowlistBytes, const FString& ManifestPath)
	{
		FString ManifestText;
		TSharedPtr<FJsonObject> Manifest;
		const bool bManifestRead = FFileHelper::LoadFileToString(ManifestText, *ManifestPath);
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestText);
		if (!bManifestRead || !FJsonSerializer::Deserialize(Reader, Manifest) || !Manifest.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Registrations = nullptr;
		if (!Manifest->TryGetArrayField(TEXT("registrations"), Registrations) || !Registrations)
		{
			return false;
		}

		FString ExpectedHash;
		for (const TSharedPtr<FJsonValue>& Value : *Registrations)
		{
			const TSharedPtr<FJsonObject> Registration = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Catalog;
			if (Registration.IsValid()
				&& Registration->TryGetStringField(TEXT("catalog"), Catalog)
				&& Catalog == TEXT("Content/Data/verified_asset_allowlist.json"))
			{
				Registration->TryGetStringField(TEXT("source_hash"), ExpectedHash);
				break;
			}
		}
		if (ExpectedHash.IsEmpty())
		{
			return false;
		}

		if (AllowlistBytes.Num() == 0)
		{
			return false;
		}
		const std::array<uint8_t, 32> Digest = apb::sha256(AllowlistBytes.GetData(), static_cast<size_t>(AllowlistBytes.Num()));
		const std::string Hex = apb::hex_encode(Digest.data(), Digest.size());
		return ExpectedHash.Equals(UTF8_TO_TCHAR(Hex.c_str()), ESearchCase::IgnoreCase);
	}

	bool IsValidSourceLocator(const FString& SourceBuild, const FString& SourceLocator)
	{
		FString Normalized = SourceLocator;
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
		Normalized = Normalized.ToLower();
		if (SourceBuild == TEXT("retail"))
		{
			return Normalized.StartsWith(TEXT("${retail_steam}/"))
				|| Normalized.StartsWith(TEXT("retail "))
				|| Normalized.StartsWith(TEXT("d:/apbreloaded/content/extracted/"));
		}
		if (SourceBuild == TEXT("2011")) return Normalized.Contains(TEXT("2011"));
		if (SourceBuild == TEXT("2011+retail")) return Normalized.Contains(TEXT("2011")) || Normalized.Contains(TEXT("retail"));
		if (SourceBuild == TEXT("apbdb")) return Normalized.Contains(TEXT("apbdb"));
		return false;
	}
}

void UAPBVerifiedAssetRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bStrictEnforcement = FParse::Param(FCommandLine::Get(), TEXT("APBStrictAssetAllowlist"));
	bManifestOverride = false;
	ManifestPath = FPaths::ProjectContentDir() / TEXT("Data/verified_asset_allowlist.json");
	FString OverridePath;
	if (FParse::Value(FCommandLine::Get(), TEXT("APBAllowlistOverride="), OverridePath) && !OverridePath.IsEmpty())
	{
		if (!FPaths::IsSamePath(OverridePath, ManifestPath))
		{
			ManifestPath = OverridePath;
			bManifestOverride = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("VERIFIED_ASSET_REGISTRY_OVERRIDE_IGNORED reason=override_equals_canonical path=%s"), *OverridePath);
		}
	}
	const FString ProvenanceManifestPath = FPaths::ProjectContentDir() / TEXT("Data/catalog_provenance_manifest.json");

	TArray<uint8> AllowlistBytes;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const bool bRead = FFileHelper::LoadFileToArray(AllowlistBytes, *ManifestPath) && AllowlistBytes.Num() > 0;
	if (bRead)
	{
		FFileHelper::BufferToString(Text, AllowlistBytes.GetData(), AllowlistBytes.Num());
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	const bool bParsed = bRead && FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
	bManifestLoaded = false;
	AllowedEntries.Reset();

	if (bParsed)
	{
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		bManifestLoaded = Root->TryGetArrayField(TEXT("entries"), Entries);
		if (bManifestLoaded && Entries)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Entries)
			{
				const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
				FString ObjectPath;
				FString ClassName;
				FString SourceBuild;
				FString SourceLocator;
				const bool bValidEntry = Entry.IsValid()
					&& Entry->TryGetStringField(TEXT("object_path"), ObjectPath)
					&& Entry->TryGetStringField(TEXT("class"), ClassName)
					&& Entry->TryGetStringField(TEXT("source_build"), SourceBuild)
					&& Entry->TryGetStringField(TEXT("source_locator"), SourceLocator)
					&& !SourceLocator.IsEmpty()
					&& IsValidSourceLocator(SourceBuild, SourceLocator)
					&& IsValidObjectPath(ObjectPath)
					&& IsSupportedClass(ClassName)
					&& IsSupportedSourceBuild(SourceBuild);
				if (!bValidEntry || AllowedEntries.Contains(ObjectPath))
				{
					bManifestLoaded = false;
					AllowedEntries.Reset();
					break;
				}
				FAllowedEntry Allowed;
				Allowed.Class = FName(*ClassName);
				Allowed.SourceBuild = SourceBuild;
				AllowedEntries.Add(ObjectPath, MoveTemp(Allowed));
			}
		}
		else
		{
			bManifestLoaded = false;
		}
	}

	if (bManifestLoaded && !bManifestOverride && !IsAllowlistProvenanceBound(AllowlistBytes, ProvenanceManifestPath))
	{
		bManifestLoaded = false;
		AllowedEntries.Reset();
		UE_LOG(LogTemp, Error, TEXT("VERIFIED_ASSET_REGISTRY_FAIL reason=allowlist_provenance_mismatch path=%s"), *ManifestPath);
	}

	UE_LOG(LogTemp, Log,
		TEXT("VERIFIED_ASSET_REGISTRY_INIT strict=%d manifest=%d entries=%d path=%s override=%d"),
		bStrictEnforcement ? 1 : 0, bManifestLoaded ? 1 : 0, AllowedEntries.Num(), *ManifestPath,
		bManifestOverride ? 1 : 0);
	if (bStrictEnforcement && !bManifestLoaded)
	{
		UE_LOG(LogTemp, Error, TEXT("VERIFIED_ASSET_REGISTRY_FAIL reason=manifest_unavailable path=%s"), *ManifestPath);
	}
}

bool UAPBVerifiedAssetRegistry::IsAllowed(const FString& ObjectPath, const FName& ExpectedClass, FString* OutReason) const
{
	const FAllowedEntry* Allowed = AllowedEntries.Find(ObjectPath);
	if (!bManifestLoaded)
	{
		if (OutReason) *OutReason = TEXT("manifest_unavailable");
		return false;
	}
	if (!Allowed)
	{
		if (OutReason) *OutReason = TEXT("unlisted_path");
		return false;
	}
	if (Allowed->Class != ExpectedClass)
	{
		if (OutReason) *OutReason = TEXT("wrong_class");
		return false;
	}
	if (OutReason) *OutReason = TEXT("allowlisted");
	return true;
}

bool UAPBVerifiedAssetRegistry::GetFirstAllowedStaticMeshEntry(FString& OutObjectPath) const
{
	OutObjectPath.Reset();
	for (const TPair<FString, FAllowedEntry>& Pair : AllowedEntries)
	{
		if (Pair.Value.Class != UStaticMesh::StaticClass()->GetFName()) continue;
		if (OutObjectPath.IsEmpty() || Pair.Key < OutObjectPath)
		{
			OutObjectPath = Pair.Key;
		}
	}
	return !OutObjectPath.IsEmpty();
}

UStaticMesh* UAPBVerifiedAssetRegistry::LoadStaticMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	FString Reason;
	if (!IsAllowed(ObjectPath, UStaticMesh::StaticClass()->GetFName(), &Reason))
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_REJECT path=%s class=StaticMesh reason=%s context=%s"),
			*ObjectPath, *Reason, *Context);
		return nullptr;
	}

	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RUNTIME_ALLOWLIST_LOAD_CONTEXT_NONE path=%s context=%s"), *ObjectPath, *Context);
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_LOAD_FAIL path=%s class=StaticMesh context=%s"),
			*ObjectPath, *Context);
		return nullptr;
	}
	UE_LOG(LogTemp, Verbose,
		TEXT("RUNTIME_ALLOWLIST_ALLOW path=%s class=StaticMesh context=%s"), *ObjectPath, *Context);
	return Mesh;
}

USkeletalMesh* UAPBVerifiedAssetRegistry::LoadSkeletalMesh(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	FString Reason;
	if (!IsAllowed(ObjectPath, USkeletalMesh::StaticClass()->GetFName(), &Reason))
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_REJECT path=%s class=SkeletalMesh reason=%s context=%s"),
			*ObjectPath, *Reason, *Context);
		return nullptr;
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RUNTIME_ALLOWLIST_LOAD_CONTEXT_NONE path=%s context=%s"), *ObjectPath, *Context);
	}
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *ObjectPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_LOAD_FAIL path=%s class=SkeletalMesh context=%s"), *ObjectPath, *Context);
		return nullptr;
	}
	UE_LOG(LogTemp, Verbose,
		TEXT("RUNTIME_ALLOWLIST_ALLOW path=%s class=SkeletalMesh context=%s"), *ObjectPath, *Context);
	return Mesh;
}

UTexture2D* UAPBVerifiedAssetRegistry::LoadTexture2D(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	FString Reason;
	if (!IsAllowed(ObjectPath, UTexture2D::StaticClass()->GetFName(), &Reason))
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_REJECT path=%s class=Texture2D reason=%s context=%s"),
			*ObjectPath, *Reason, *Context);
		return nullptr;
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RUNTIME_ALLOWLIST_LOAD_CONTEXT_NONE path=%s context=%s"), *ObjectPath, *Context);
	}
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_LOAD_FAIL path=%s class=Texture2D context=%s"), *ObjectPath, *Context);
		return nullptr;
	}
	UE_LOG(LogTemp, Verbose,
		TEXT("RUNTIME_ALLOWLIST_ALLOW path=%s class=Texture2D context=%s"), *ObjectPath, *Context);
	return Texture;
}

UMaterialInterface* UAPBVerifiedAssetRegistry::LoadMaterialInterface(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	FString Reason;
	FString MaterialReason;
	const bool bAllowed = IsAllowed(ObjectPath, FName(TEXT("Material")), &MaterialReason)
		|| IsAllowed(ObjectPath, FName(TEXT("MaterialInstanceConstant")), &Reason);
	if (!bAllowed)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_REJECT path=%s class=MaterialInterface reason=%s context=%s"),
			*ObjectPath, *Reason, *Context);
		return nullptr;
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RUNTIME_ALLOWLIST_LOAD_CONTEXT_NONE path=%s context=%s"), *ObjectPath, *Context);
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
	if (!Material)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_LOAD_FAIL path=%s class=MaterialInterface context=%s"), *ObjectPath, *Context);
		return nullptr;
	}
	UE_LOG(LogTemp, Verbose,
		TEXT("RUNTIME_ALLOWLIST_ALLOW path=%s class=MaterialInterface context=%s"), *ObjectPath, *Context);
	return Material;
}

USoundBase* UAPBVerifiedAssetRegistry::LoadSoundBase(UWorld* World, const FString& ObjectPath, const FString& Context)
{
	FString Reason;
	FString SoundReason;
	const bool bAllowed = IsAllowed(ObjectPath, FName(TEXT("SoundWave")), &SoundReason)
		|| IsAllowed(ObjectPath, FName(TEXT("SoundCue")), &Reason);
	if (!bAllowed)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_REJECT path=%s class=SoundBase reason=%s context=%s"),
			*ObjectPath, *Reason, *Context);
		return nullptr;
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RUNTIME_ALLOWLIST_LOAD_CONTEXT_NONE path=%s context=%s"), *ObjectPath, *Context);
	}
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *ObjectPath);
	if (!Sound)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RUNTIME_ALLOWLIST_LOAD_FAIL path=%s class=SoundBase context=%s"), *ObjectPath, *Context);
		return nullptr;
	}
	UE_LOG(LogTemp, Verbose,
		TEXT("RUNTIME_ALLOWLIST_ALLOW path=%s class=SoundBase context=%s"), *ObjectPath, *Context);
	return Sound;
}
