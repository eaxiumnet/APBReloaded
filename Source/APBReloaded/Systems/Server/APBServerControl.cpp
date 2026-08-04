#include "APBServerControl.h"
#include "APBDistrictDirectory.h"
#include "APBPorts.h"
#include "APBRelayProtocol.h"
#include "APBSecretProvider.h"
#include "APBWorldGameMode.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "SocketTypes.h"
#include "Sockets.h"

#include <atomic>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
	constexpr int32 RelayListenBacklog = 16;
	constexpr int32 RelayReceiveChunkBytes = 4096;
	constexpr float RelayPollSleepSeconds = 0.01f;
	constexpr int64 TravelReservationTimeoutMs = 15000;

	int64 RelayNowMs()
	{
		const FDateTime Now = FDateTime::UtcNow();
		return Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond();
	}

	FString TravelReservationHost()
	{
		FString Host;
		FParse::Value(FCommandLine::Get(), TEXT("DistrictHost="), Host);
		return Host.IsEmpty() ? TEXT("127.0.0.1") : Host;
	}

	struct FRelayClient
	{
		FSocket* Socket = nullptr;
		FString Peer;
		std::string ReceiveBuffer;
		std::string SendBuffer;
		std::deque<std::string> RequestHistory;
		std::unordered_set<std::string> RequestIds;
		std::deque<std::string> JtiHistory;
		std::unordered_set<std::string> Jtis;
		std::deque<std::string> OutboundFrames;
		int32 NumericId = 0;
		int64 LastActivityMs = 0;
	};

	struct FRelayDistrictConfig
	{
		FString Host;
		FString DistrictId;
		int32 RelayPort = 0;
		int32 NumericId = 0;
		int32 DistrictPort = 0;
		FString TargetDistrictEpoch;
	};

	bool ResolveJoinableDistrictName(const FString& RequestedDistrictId,
		FString& OutCanonicalDistrictId, FString& OutError)
	{
		FString CatalogText;
		const FString CatalogPath = FPaths::ProjectContentDir() / TEXT("Data/districts.json");
		if (!FFileHelper::LoadFileToString(CatalogText, *CatalogPath))
		{
			OutError = TEXT("district_catalog_unavailable");
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Catalog;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CatalogText);
		if (!FJsonSerializer::Deserialize(Reader, Catalog))
		{
			OutError = TEXT("district_catalog_invalid");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Entry : Catalog)
		{
			const TSharedPtr<FJsonObject> Object = Entry.IsValid() ? Entry->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}

			FString CatalogId;
			if (!Object->TryGetStringField(TEXT("id"), CatalogId) ||
				!CatalogId.Equals(RequestedDistrictId, ESearchCase::IgnoreCase))
			{
				continue;
			}

			bool bJoinable = false;
			if (!Object->TryGetBoolField(TEXT("joinable"), bJoinable) || !bJoinable)
			{
				OutError = TEXT("district_not_joinable");
				return false;
			}

			OutCanonicalDistrictId = CatalogId;
			return true;
		}

		OutError = TEXT("district_catalog_mismatch");
		return false;
	}

	bool ResolveDistrictRelayConfig(const FString& ResolvedDistrictId, FRelayDistrictConfig& OutConfig,
		FString& OutError)
	{
		FString RequestedDistrictId;
		int32 RelayPort = 0;
		int32 NumericId = 0;
		int32 DistrictPort = 0;
		if (!FParse::Value(FCommandLine::Get(), TEXT("RelayHost="), OutConfig.Host) || OutConfig.Host.IsEmpty() ||
			!FParse::Value(FCommandLine::Get(), TEXT("RelayPort="), RelayPort) ||
			!FParse::Value(FCommandLine::Get(), TEXT("DistrictId="), RequestedDistrictId) ||
			!FParse::Value(FCommandLine::Get(), TEXT("NumericId="), NumericId) ||
			!FParse::Value(FCommandLine::Get(), TEXT("Port="), DistrictPort))
		{
			OutError = TEXT("missing_launch_argument");
			return false;
		}
		if (RelayPort < 1 || RelayPort > 65535 || NumericId < 1 || DistrictPort < 1 || DistrictPort > 65535 ||
			DistrictPort == apb::ports::World || DistrictPort == apb::ports::Relay)
		{
			OutError = TEXT("invalid_launch_value");
			return false;
		}

		FString CanonicalDistrictId;
		if (!ResolveJoinableDistrictName(RequestedDistrictId, CanonicalDistrictId, OutError))
		{
			return false;
		}
		if (!ResolvedDistrictId.IsEmpty() && !CanonicalDistrictId.Equals(ResolvedDistrictId, ESearchCase::IgnoreCase))
		{
			OutError = TEXT("district_map_mismatch");
			return false;
		}

		OutConfig.DistrictId = CanonicalDistrictId;
		OutConfig.RelayPort = RelayPort;
		OutConfig.NumericId = NumericId;
		OutConfig.DistrictPort = DistrictPort;
		return true;
	}
}

class FAPBRelayListener final : public FRunnable
{
public:
	FAPBRelayListener(ISocketSubsystem* InSocketSubsystem, const uint16 InPort,
		std::string InExpectedAuth)
		: SocketSubsystem(InSocketSubsystem)
		, Port(InPort)
		, ExpectedAuth(MoveTemp(InExpectedAuth))
	{
	}

	virtual ~FAPBRelayListener() override
	{
		Shutdown();
	}

	bool Start()
	{
		if (!SocketSubsystem || ExpectedAuth.empty())
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_LISTEN_FAILED reason=missing_socket_or_auth"));
			return false;
		}

		ListenSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("APBRelayListen"),
			FNetworkProtocolTypes::IPv4);
		if (!ListenSocket)
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_LISTEN_FAILED reason=create_socket"));
			return false;
		}

		TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
		Address->SetAnyAddress();
		Address->SetPort(Port);
		if (!ListenSocket->SetRecvErr() || !ListenSocket->SetNonBlocking(true) ||
			!ListenSocket->Bind(*Address) || !ListenSocket->Listen(RelayListenBacklog))
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_LISTEN_FAILED reason=bind_or_listen port=%d"), Port);
			CloseSockets();
			return false;
		}

		Thread = FRunnableThread::Create(this, TEXT("APBRelayListener"));
		if (!Thread)
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_LISTEN_FAILED reason=create_thread port=%d"), Port);
			CloseSockets();
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("RELAY_LISTEN port=%d"), Port);
		return true;
	}

	void Shutdown()
	{
		bStopRequested.store(true, std::memory_order_release);
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
		CloseSockets();
	}

	virtual uint32 Run() override
	{
		while (!bStopRequested.load(std::memory_order_acquire))
		{
			AcceptPendingConnections();
			PollClients();
			PruneStaleDistricts();
			FPlatformProcess::Sleep(RelayPollSleepSeconds);
		}
		return 0;
	}

	virtual void Stop() override
	{
		bStopRequested.store(true, std::memory_order_release);
	}

	bool QueueToDistrict(const int32 NumericId, const apb::RelayMessage& Message)
	{
		const std::string Encoded = apb::RelayCodec::Encode(Message);
		if (Encoded.empty()) return false;
		FScopeLock Lock(&ClientLock);
		for (FRelayClient& Client : Clients)
		{
			if (Client.NumericId == NumericId)
			{
				if (Client.OutboundFrames.size() >= apb::kRelayMaxQueueDepth)
				{
					UE_LOG(LogTemp, Warning, TEXT("RELAY_REJECT reason=send_queue_full peer=%s"), *Client.Peer);
					return false;
				}
				Client.OutboundFrames.push_back(Encoded);
				return true;
			}
		}
		return false;
	}

	bool DequeueInbound(apb::RelayMessage& OutMessage)
	{
		FScopeLock Lock(&InboundLock);
		if (InboundMessages.empty()) return false;
		OutMessage = MoveTemp(InboundMessages.front());
		InboundMessages.pop_front();
		return true;
	}

	bool ResolveLeastLoaded(const FString& DistrictId, const int32 MaxPlayers,
		const TMap<int32, int32>& PendingReservations,
		FString& OutHost, int32& OutPort, int32& OutNumericId, FString& OutEpoch, FString& OutError) const
	{
		FScopeLock Lock(&DirectoryLock);
		const std::vector<apb::DistrictNode> Nodes = Directory.ListAlive();
		const apb::DistrictNode* BestNode = nullptr;
		for (const apb::DistrictNode& Node : Nodes)
		{
			if (!DistrictId.Equals(UTF8_TO_TCHAR(Node.district.c_str()), ESearchCase::IgnoreCase))
			{
				continue;
			}
			const int32 PendingCount = PendingReservations.FindRef(Node.numeric_id);
			if (Node.player_count + PendingCount >= MaxPlayers)
			{
				continue;
			}
			if (!BestNode || Node.player_count + PendingCount <
				BestNode->player_count + PendingReservations.FindRef(BestNode->numeric_id))
			{
				BestNode = &Node;
			}
		}
		if (!BestNode)
		{
			const apb::DistrictNode* LiveNode = Directory.LeastLoaded(TCHAR_TO_UTF8(*DistrictId));
			OutError = LiveNode ? TEXT("over_capacity") : TEXT("no_live_node");
			return false;
		}
		OutHost = TravelReservationHost();
		OutPort = BestNode->port;
		OutNumericId = BestNode->numeric_id;
		return true;
	}

private:
	friend class UAPBServerControl;

	std::vector<FAPBDistrictPopulationSnapshot> SnapshotDistrictPopulations() const
	{
		FScopeLock Lock(&DirectoryLock);
		return MakeDistrictPopulationSnapshots(Directory.AggregateByDistrict());
	}

	static bool IsDirectoryMessage(const apb::RelayVerb Verb)
	{
		return Verb == apb::RelayVerb::Register || Verb == apb::RelayVerb::Heartbeat ||
			Verb == apb::RelayVerb::ReportLoad || Verb == apb::RelayVerb::PlayerJoined ||
			Verb == apb::RelayVerb::PlayerLeft;
	}

	void AcceptPendingConnections()
	{
		if (!ListenSocket)
		{
			return;
		}

		const int64 NowMs = RelayNowMs();
		const int64 NowSec = NowMs / 1000;
		if (NowSec != LastAcceptSecond)
		{
			LastAcceptSecond = NowSec;
			AcceptsThisSecond = 0;
		}

		bool bPendingConnection = false;
		while (!bStopRequested.load(std::memory_order_acquire) &&
			ListenSocket->HasPendingConnection(bPendingConnection) && bPendingConnection)
		{
			if (AcceptsThisSecond >= 10)
			{
				break;
			}

			FScopeLock Lock(&ClientLock);
			if (Clients.Num() >= 64)
			{
				break;
			}
			int32 PreAuthCount = 0;
			for (const FRelayClient& Client : Clients)
			{
				if (Client.NumericId == 0) PreAuthCount++;
			}
			if (PreAuthCount >= 16)
			{
				break;
			}

			TSharedRef<FInternetAddr> PeerAddress = SocketSubsystem->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
			FSocket* ClientSocket = ListenSocket->Accept(*PeerAddress,
				FString::Printf(TEXT("APBRelayClient_%d"), NextClientId++));
			if (!ClientSocket)
			{
				break;
			}

			if (!ClientSocket->SetNonBlocking(true) || !ClientSocket->SetNoDelay(true))
			{
				ClientSocket->Close();
				SocketSubsystem->DestroySocket(ClientSocket);
				continue;
			}

			FRelayClient& Client = Clients.AddDefaulted_GetRef();
			Client.Socket = ClientSocket;
			Client.Peer = PeerAddress->ToString(true);
			UE_LOG(LogTemp, Log, TEXT("RELAY_ACCEPT peer=%s"), *Client.Peer);
			
			AcceptsThisSecond++;
			bPendingConnection = false;
		}
	}

	void PollClients()
	{
		FScopeLock Lock(&ClientLock);
		DrainOutboundFrames();
		for (int32 Index = Clients.Num() - 1; Index >= 0; --Index)
		{
			if (!ReadClient(Clients[Index]) || !FlushClient(Clients[Index]))
			{
				CloseClient(Index);
			}
		}
	}

	void DrainOutboundFrames()
	{
		for (FRelayClient& Client : Clients)
		{
			while (!Client.OutboundFrames.empty())
			{
				const std::string& Frame = Client.OutboundFrames.front();
				if (Client.SendBuffer.size() + Frame.size() > apb::kRelayMaxFrameBytes * apb::kRelayMaxQueueDepth)
				{
					UE_LOG(LogTemp, Warning, TEXT("RELAY_REJECT reason=send_queue_full peer=%s"), *Client.Peer);
					Client.OutboundFrames.clear();
					break;
				}
				Client.SendBuffer.append(Frame);
				Client.OutboundFrames.pop_front();
			}
		}
	}

	bool ReadClient(FRelayClient& Client)
	{
		std::vector<std::string> Frames;
		uint32 PendingBytes = 0;
		while (Client.Socket->HasPendingData(PendingBytes) && PendingBytes > 0)
		{
			uint8 Buffer[RelayReceiveChunkBytes];
			const int32 BytesToRead = FMath::Min<int32>(RelayReceiveChunkBytes, static_cast<int32>(PendingBytes));
			int32 BytesRead = 0;
			if (!Client.Socket->Recv(Buffer, BytesToRead, BytesRead) || BytesRead <= 0)
			{
				return false;
			}

			Client.ReceiveBuffer.append(reinterpret_cast<const char*>(Buffer), static_cast<size_t>(BytesRead));
			if (!ExtractFrames(Client, Frames))
			{
				return false;
			}
		}

		if (!Frames.empty())
		{
			ProcessFrames(Client, Frames, RelayNowMs());
		}

		const ESocketConnectionState State = Client.Socket->GetConnectionState();
		return State != SCS_NotConnected && State != SCS_ConnectionError;
	}

	bool ExtractFrames(FRelayClient& Client, std::vector<std::string>& OutFrames)
	{
		while (true)
		{
			const size_t Newline = Client.ReceiveBuffer.find('\n');
			if (Newline == std::string::npos)
			{
				if (Client.ReceiveBuffer.size() > apb::kRelayMaxFrameBytes)
				{
					RejectOversize(Client);
					return false;
				}
				return true;
			}

			const size_t FrameSize = Newline + 1;
			std::string Frame = Client.ReceiveBuffer.substr(0, FrameSize);
			Client.ReceiveBuffer.erase(0, FrameSize);
			if (!apb::RelayCodec::IsFrameSizeValid(Frame))
			{
				RejectOversize(Client, Frame);
				return false;
			}
			if (OutFrames.size() >= apb::kRelayMaxQueueDepth)
			{
				LogReject(Client, apb::RelayRejectReason::QueueFull);
				return false;
			}
			OutFrames.push_back(MoveTemp(Frame));
		}
	}

	void ProcessFrames(FRelayClient& Client, const std::vector<std::string>& Frames, const int64 NowMs)
	{
		Client.LastActivityMs = NowMs;
		apb::RelayValidationOptions Opts;
		Opts.now_ms = NowMs;
		Opts.expected_auth = ExpectedAuth;
		Opts.require_auth = true;
		Opts.is_world_server = true;
		Opts.expected_numeric_id = Client.NumericId;
		apb::RelayInbox Inbox(Opts);
		for (const std::string& Frame : Frames)
		{
			std::string StreamFrame = Frame;
			const std::vector<apb::RelayMessage> StreamMessages = apb::RelayCodec::DecodeStream(StreamFrame);
			if (StreamMessages.size() == 1)
			{
				const apb::RelayMessage& Message = StreamMessages.front();
				if (Client.RequestIds.find(Message.request_id) != Client.RequestIds.end())
				{
					LogReject(Client, apb::RelayRejectReason::DuplicateRequestId);
					continue;
				}
				if (Message.verb == apb::RelayVerb::ExpectTicket &&
					Client.Jtis.find(Message.jti) != Client.Jtis.end())
				{
					LogReject(Client, apb::RelayRejectReason::ReplayJti);
					continue;
				}
			}

			const apb::RelayRejectReason Reason = Inbox.Submit(Frame);
			if (Reason != apb::RelayRejectReason::None)
			{
				LogReject(Client, Reason);
			}
		}

		apb::RelayMessage Message;
		while (Inbox.Pop(Message))
		{
			Remember(Client.RequestHistory, Client.RequestIds, Message.request_id);
			if (Message.verb == apb::RelayVerb::ExpectTicket)
			{
				Remember(Client.JtiHistory, Client.Jtis, Message.jti);
			}
			HandleMessage(Client, Message, NowMs);
		}
	}

	void HandleMessage(FRelayClient& Client, const apb::RelayMessage& Message, const int64 NowMs)
	{		if (Message.verb == apb::RelayVerb::Return || Message.verb == apb::RelayVerb::ChatRelay ||
			Message.verb == apb::RelayVerb::SocialRequest)
		{
			FScopeLock Lock(&InboundLock);
			if (InboundMessages.size() >= apb::kRelayMaxQueueDepth)
			{
				LogReject(Client, apb::RelayRejectReason::QueueFull);
				return;
			}
			InboundMessages.push_back(Message);
			return;
		}
		if (Message.verb == apb::RelayVerb::PlayerJoined || Message.verb == apb::RelayVerb::PlayerLeft)
		{
			// Route to WorldGameMode for reservation/admission tracking only.
			// Do NOT apply to Directory here — ReportLoad (absolute GetNumPlayers) is the
			// sole population authority. Mixing absolute ReportLoad with relative PlayerJoined
			// deltas on the same player_count field causes non-deterministic over-count.
			FScopeLock Lock(&InboundLock);
			if (InboundMessages.size() >= apb::kRelayMaxQueueDepth)
			{
				LogReject(Client, apb::RelayRejectReason::QueueFull);
				return;
			}
			InboundMessages.push_back(Message);
			return;
		}
			bool bApplied = false;
		if (Message.verb == apb::RelayVerb::Register)
		{
			const FString RequestedDistrict = UTF8_TO_TCHAR(Message.district.c_str());
			FString CanonicalDistrict;
			FString RejectReason;
			if (!ResolveJoinableDistrictName(RequestedDistrict, CanonicalDistrict, RejectReason))
			{
				bApplied = false;
			}
			else if (Message.numeric_id <= 0)
			{
				RejectReason = TEXT("invalid_numeric_id");
			}
			else if (Message.port < 1 || Message.port > 65535 ||
				Message.port == apb::ports::World || Message.port == apb::ports::Relay)
			{
				RejectReason = TEXT("invalid_or_reserved_port");
			}
			else
			{
				FScopeLock Lock(&DirectoryLock);
				const std::string CanonicalDistrictUtf8 = TCHAR_TO_UTF8(*CanonicalDistrict);
				const apb::DistrictNode* ExistingNode = Directory.Find(Message.numeric_id);
				if (ExistingNode && (ExistingNode->district != CanonicalDistrictUtf8 ||
					ExistingNode->port != Message.port))
				{
					RejectReason = TEXT("numeric_id_collision");
				}
				else
				{
					for (const apb::DistrictNode& Node : Directory.ListAlive())
					{
						if (Node.numeric_id != Message.numeric_id && Node.port == Message.port)
						{
							RejectReason = TEXT("port_collision");
							break;
						}
					}
				}

				if (RejectReason.IsEmpty())
				{
					bApplied = Directory.Register(CanonicalDistrictUtf8, Message.numeric_id, Message.port, Message.target_district_epoch, NowMs);
					if (!bApplied)
					{
						RejectReason = TEXT("directory_register_failed");
					}
				}
			}
			if (bApplied)
			{
				Client.NumericId = Message.numeric_id;
				RejectReason = TEXT("none");
				
				for (FRelayClient& OtherClient : Clients)
				{
					if (&OtherClient != &Client && OtherClient.NumericId == Client.NumericId)
					{
						// Duplicate numeric_id: only evict a prior socket that has gone quiet.
						// A healthy duplicate is a ghost process that must NOT be allowed to
						// wedge the live district into a reconnect ping-pong (its register
						// kills the live socket, the live reconnect kills the ghost, ad infinitum).
						// QueueToDistrict broadcasts to every matching socket, so a retained
						// healthy duplicate still receives relay traffic.
						const int64 ActivityAgeMs = OtherClient.LastActivityMs > 0
							? NowMs - OtherClient.LastActivityMs : INT64_MAX;
						const bool bStale = ActivityAgeMs > apb::kRelayHeartbeatIntervalMs * 3;
						UE_CLOG(bStale, LogTemp, Warning,
							TEXT("RELAY_DUPLICATE_CLIENT peer=%s numeric_id=%d stale=1 age_ms=%lld"),
							*OtherClient.Peer, OtherClient.NumericId,
							ActivityAgeMs == INT64_MAX ? -1 : ActivityAgeMs);
						UE_CLOG(!bStale, LogTemp, Log,
							TEXT("RELAY_DUPLICATE_CLIENT peer=%s numeric_id=%d stale=0 age_ms=%lld"),
							*OtherClient.Peer, OtherClient.NumericId,
							ActivityAgeMs == INT64_MAX ? -1 : ActivityAgeMs);
						if (bStale && OtherClient.Socket)
						{
							OtherClient.Socket->Close();
						}
					}
				}
			}
			UE_LOG(LogTemp, Log, TEXT("RELAY_REGISTER district=%s numeric_id=%d ok=%d port=%d reason=%s"),
				*CanonicalDistrict, Message.numeric_id, bApplied ? 1 : 0, Message.port, *RejectReason);
			QueueResponse(Client, apb::RelayCodec::MakeRegisterAck(Message.numeric_id, bApplied,
				Message.request_id, NowMs, ExpectedAuth));
			return;
		}

		if (IsDirectoryMessage(Message.verb))
		{
			FScopeLock Lock(&DirectoryLock);
			Directory.Apply(Message, NowMs);
		}
	}

	void QueueResponse(FRelayClient& Client, const apb::RelayMessage& Response)
	{
		const std::string Encoded = apb::RelayCodec::Encode(Response);
		if (Encoded.empty() || Client.SendBuffer.size() + Encoded.size() >
			(apb::kRelayMaxFrameBytes * apb::kRelayMaxQueueDepth))
		{
			UE_LOG(LogTemp, Warning, TEXT("RELAY_REJECT reason=send_queue_full peer=%s"), *Client.Peer);
			return;
		}
		Client.SendBuffer.append(Encoded);
	}

	bool FlushClient(FRelayClient& Client)
	{
		while (!Client.SendBuffer.empty())
		{
			int32 BytesSent = 0;
			if (!Client.Socket->Send(reinterpret_cast<const uint8*>(Client.SendBuffer.data()),
				static_cast<int32>(Client.SendBuffer.size()), BytesSent))
			{
				return Client.Socket->GetConnectionState() == SCS_Connected;
			}
			if (BytesSent <= 0)
			{
				return true;
			}
			Client.SendBuffer.erase(0, static_cast<size_t>(BytesSent));
		}
		return true;
	}

	void RejectOversize(FRelayClient& Client, const std::string& Frame = std::string())
	{
		const std::string& Candidate = Frame.empty() ? Client.ReceiveBuffer : Frame;
		apb::RelayInbox Inbox({RelayNowMs(), ExpectedAuth, true});
		LogReject(Client, Inbox.Submit(Candidate));
	}

	void LogReject(const FRelayClient& Client, const apb::RelayRejectReason Reason) const
	{
		UE_LOG(LogTemp, Warning, TEXT("RELAY_REJECT reason=%s peer=%s"),
			UTF8_TO_TCHAR(apb::RelayRejectReasonName(Reason)), *Client.Peer);
	}

	void Remember(std::deque<std::string>& History, std::unordered_set<std::string>& Values,
		const std::string& Value)
	{
		History.push_back(Value);
		Values.insert(Value);
		while (History.size() > apb::kRelayMaxQueueDepth)
		{
			Values.erase(History.front());
			History.pop_front();
		}
	}

	void PruneStaleDistricts()
	{
		const int64 NowMs = RelayNowMs();
		if (NowMs - LastPruneMs < 1000)
		{
			return;
		}
		LastPruneMs = NowMs;
		FScopeLock Lock(&DirectoryLock);
		const int32 EvictedCount = Directory.PruneStale(NowMs);
		if (EvictedCount > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("RELAY_EVICT stale=%d"), EvictedCount);
		}
	}

	void CloseClient(const int32 Index)
	{
		if (!Clients.IsValidIndex(Index))
		{
			return;
		}
		if (Clients[Index].Socket)
		{
			Clients[Index].Socket->Close();
			SocketSubsystem->DestroySocket(Clients[Index].Socket);
		}
		Clients.RemoveAtSwap(Index);
	}

	void CloseSockets()
	{
		if (bSocketsClosed)
		{
			return;
		}
		bSocketsClosed = true;

		int32 ClosedCount = 0;
		for (FRelayClient& Client : Clients)
		{
			if (Client.Socket)
			{
				Client.Socket->Close();
				SocketSubsystem->DestroySocket(Client.Socket);
				++ClosedCount;
			}
		}
		Clients.Empty();
		if (ListenSocket)
		{
			ListenSocket->Close();
			SocketSubsystem->DestroySocket(ListenSocket);
			ListenSocket = nullptr;
			++ClosedCount;
		}
		UE_LOG(LogTemp, Log, TEXT("RELAY_SHUTDOWN sockets_closed=%d"), ClosedCount);
	}

	ISocketSubsystem* SocketSubsystem = nullptr;
	FSocket* ListenSocket = nullptr;
	FRunnableThread* Thread = nullptr;
	TArray<FRelayClient> Clients;
	FCriticalSection ClientLock;
	FCriticalSection InboundLock;
	mutable FCriticalSection DirectoryLock;
	apb::DistrictDirectory Directory;
	std::deque<apb::RelayMessage> InboundMessages;

	int64 LastAcceptSecond = 0;
	int32 AcceptsThisSecond = 0;
	uint16 Port = 0;
	std::string ExpectedAuth;
	std::atomic<bool> bStopRequested = false;
	int32 NextClientId = 1;
	int64 LastPruneMs = 0;
	bool bSocketsClosed = false;
};

class FAPBRelayDistrictClient final : public FRunnable
{
public:
	FAPBRelayDistrictClient(ISocketSubsystem* InSocketSubsystem, FRelayDistrictConfig InConfig,
		std::string InExpectedAuth)
		: SocketSubsystem(InSocketSubsystem)
		, Config(MoveTemp(InConfig))
		, ExpectedAuth(MoveTemp(InExpectedAuth))
	{
	}

	virtual ~FAPBRelayDistrictClient() override
	{
		Shutdown();
	}

	bool Start()
	{
		if (!SocketSubsystem || ExpectedAuth.empty())
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_CLIENT_FAILED reason=missing_socket_or_auth"));
			return false;
		}
		Thread = FRunnableThread::Create(this, TEXT("APBRelayDistrictClient"));
		if (!Thread)
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_CLIENT_FAILED reason=create_thread"));
			return false;
		}
		return true;
	}

	void Shutdown()
	{
		bStopRequested.store(true, std::memory_order_release);
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
			Thread = nullptr;
		}
		CloseSocket();
		if (!bShutdownLogged.exchange(true, std::memory_order_acq_rel))
		{
			UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_SHUTDOWN"));
		}
	}

	void SetPopulation(const int32 InPopulation)
	{
		Population.store(FMath::Max(0, InPopulation), std::memory_order_release);
	}

	bool QueueToWorld(const apb::RelayMessage& Message)
	{
		const std::string Encoded = apb::RelayCodec::Encode(Message);
		if (Encoded.empty()) return false;
		FScopeLock Lock(&QueueLock);
		if (OutboundFrames.size() >= apb::kRelayMaxQueueDepth) return false;
		OutboundFrames.push_back(Encoded);
		return true;
	}

	bool DequeueInbound(apb::RelayMessage& OutMessage)
	{
		FScopeLock Lock(&QueueLock);
		if (InboundMessages.empty()) return false;
		OutMessage = MoveTemp(InboundMessages.front());
		InboundMessages.pop_front();
		return true;
	}

	virtual uint32 Run() override
	{
		int64 NextConnectMs = 0;
		while (!bStopRequested.load(std::memory_order_acquire))
		{
			const int64 NowMs = RelayNowMs();
			if (!Socket)
			{
				if (NowMs >= NextConnectMs)
				{
					if (!StartConnect())
					{
						ScheduleReconnect(NowMs, NextConnectMs);
					}
				}
			}
			else if (!bConnected)
			{
				if (Socket->GetConnectionState() == SCS_Connected)
				{
					OnConnected(NowMs);
				}
				else if (NowMs - ConnectStartedMs >= apb::kRelayRequestTimeoutMs ||
					Socket->GetConnectionState() == SCS_ConnectionError ||
					Socket->GetConnectionState() == SCS_NotConnected)
				{
					CloseSocket();
					ScheduleReconnect(NowMs, NextConnectMs);
				}
			}
			else if (!PumpConnection(NowMs))
			{
				CloseSocket();
				ScheduleReconnect(NowMs, NextConnectMs);
			}
			FPlatformProcess::Sleep(RelayPollSleepSeconds);
		}
		return 0;
	}

	virtual void Stop() override
	{
		bStopRequested.store(true, std::memory_order_release);
	}

private:
	bool StartConnect()
	{
		TSharedPtr<FInternetAddr> Address = SocketSubsystem->GetAddressFromString(Config.Host);
		if (!Address.IsValid() || !Address->IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_CONNECT_FAILED reason=invalid_host host=%s"), *Config.Host);
			return false;
		}
		Address->SetPort(Config.RelayPort);
		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("APBRelayDistrictClient"),
			FNetworkProtocolTypes::IPv4);
		if (!Socket || !Socket->SetNonBlocking(true) || !Socket->SetNoDelay(true) || !Socket->Connect(*Address))
		{
			CloseSocket();
			return false;
		}
		ConnectStartedMs = RelayNowMs();
		UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_CONNECT host=%s port=%d"), *Config.Host, Config.RelayPort);
		return true;
	}

	void OnConnected(const int64 NowMs)
	{
		bConnected = true;
		ReconnectAttempt = 0;
		ReceiveBuffer.clear();
		SendBuffer.clear();
		LastHeartbeatMs = NowMs;
		QueueMessage(apb::RelayCodec::MakeRegister(TCHAR_TO_UTF8(*Config.DistrictId), Config.NumericId,
			Config.DistrictPort, NextRequestId(TEXT("register")), NowMs, ExpectedAuth));
	}

	bool PumpConnection(const int64 NowMs)
	{
		DrainOutboundFrames();
		if (!ReadMessages(NowMs) || !FlushMessages())
		{
			return false;
		}
		if (NowMs - LastHeartbeatMs >= apb::kRelayHeartbeatIntervalMs)
		{
			const int64 Sequence = ++HeartbeatSequence;
			QueueMessage(apb::RelayCodec::MakeHeartbeat(Config.NumericId, Sequence,
				NextRequestId(TEXT("heartbeat")), NowMs, ExpectedAuth));
			QueueMessage(apb::RelayCodec::MakeReportLoad(Config.NumericId,
				Population.load(std::memory_order_acquire), NextRequestId(TEXT("report_load")), NowMs, ExpectedAuth));
			LastHeartbeatMs = NowMs;
			UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_HEARTBEAT seq=%lld"), Sequence);
		}
		const ESocketConnectionState State = Socket->GetConnectionState();
		return State != SCS_NotConnected && State != SCS_ConnectionError;
	}

	void DrainOutboundFrames()
	{
		FScopeLock Lock(&QueueLock);
		while (!OutboundFrames.empty())
		{
			const std::string& Frame = OutboundFrames.front();
			if (SendBuffer.size() + Frame.size() > apb::kRelayMaxFrameBytes * apb::kRelayMaxQueueDepth)
			{
				UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REJECT reason=send_queue_full"));
				OutboundFrames.clear();
				return;
			}
			SendBuffer.append(Frame);
			OutboundFrames.pop_front();
		}
	}

	bool ReadMessages(const int64 NowMs)
	{
		while (Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::Zero()))
		{
			uint8 Buffer[RelayReceiveChunkBytes];
			int32 BytesRead = 0;
			if (!Socket->Recv(Buffer, RelayReceiveChunkBytes, BytesRead) || BytesRead <= 0)
			{
				return false;
			}
			ReceiveBuffer.append(reinterpret_cast<const char*>(Buffer), static_cast<size_t>(BytesRead));
			if (ReceiveBuffer.size() > apb::kRelayMaxFrameBytes && ReceiveBuffer.find('\n') == std::string::npos)
			{
				UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REJECT reason=oversize"));
				return false;
			}
		}

		// Submit the RAW newline-delimited wire frames to the inbox. Decoding a frame and
		// re-encoding it corrupts the auth: Encode() treats a populated message.auth (which after
		// Decode holds the received HMAC hex) as the HMAC *key* and recomputes a different value,
		// so every authenticated inbound message (RegisterAck, Handoff, Return, Social*) would fail
		// validation. Validate the bytes exactly as they arrived, mirroring the listener path.
		apb::RelayInbox Inbox({NowMs, ExpectedAuth, true});
		size_t Consumed = 0;
		while (true)
		{
			const size_t Newline = ReceiveBuffer.find('\n', Consumed);
			if (Newline == std::string::npos) break;
			const std::string Frame = ReceiveBuffer.substr(Consumed, Newline - Consumed);
			Consumed = Newline + 1;
			if (Frame.empty()) continue;
			const apb::RelayRejectReason Reason = Inbox.Submit(Frame);
			if (Reason != apb::RelayRejectReason::None)
			{
				UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REJECT reason=%s"),
					UTF8_TO_TCHAR(apb::RelayRejectReasonName(Reason)));
			}
		}
		if (Consumed > 0) ReceiveBuffer.erase(0, Consumed);
		if (ReceiveBuffer.size() > apb::kRelayMaxFrameBytes) ReceiveBuffer.clear();

		apb::RelayMessage Accepted;
		while (Inbox.Pop(Accepted))
		{
			HandleIncoming(Accepted);
		}
		return true;
	}

	void HandleIncoming(const apb::RelayMessage& Message)
	{
		if (Message.verb == apb::RelayVerb::RegisterAck)
		{
			if (Message.ok && Message.numeric_id == Config.NumericId)
			{
				UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_REGISTERED district=%s numeric_id=%d"),
					*Config.DistrictId, Config.NumericId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REGISTER_REJECTED numeric_id=%d"), Message.numeric_id);
			}
			return;
		}
		if (Message.verb == apb::RelayVerb::ExpectTicket)
		{
			UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_EXPECT_TICKET jti=%s"), UTF8_TO_TCHAR(Message.jti.c_str()));
			return;
		}
		if (Message.verb == apb::RelayVerb::Handoff)
		{
			FScopeLock Lock(&QueueLock);
			if (InboundMessages.size() >= apb::kRelayMaxQueueDepth)
			{
				UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REJECT reason=queue_full"));
				return;
			}
			InboundMessages.push_back(Message);
			return;
		}
		if (Message.verb == apb::RelayVerb::ChatRelay ||
			Message.verb == apb::RelayVerb::SocialResult ||
			Message.verb == apb::RelayVerb::SocialProjection ||
			Message.verb == apb::RelayVerb::SocialChat)
		{
			FScopeLock Lock(&QueueLock);
			if (InboundMessages.size() >= apb::kRelayMaxQueueDepth)
			{
				UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REJECT reason=queue_full"));
				return;
			}
			InboundMessages.push_back(Message);
			if (Message.verb == apb::RelayVerb::ChatRelay)
			{
				UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_CHAT_RECEIVED from=%s"), UTF8_TO_TCHAR(Message.from.c_str()));
			}
			else if (Message.verb == apb::RelayVerb::SocialResult)
			{
				UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_SOCIAL_RESULT character=%s status=%s"),
					UTF8_TO_TCHAR(Message.character.c_str()), UTF8_TO_TCHAR(Message.social_status.c_str()));
			}
		}
	}

	void QueueMessage(const apb::RelayMessage& Message)
	{
		const std::string Encoded = apb::RelayCodec::Encode(Message);
		if (Encoded.empty() || SendBuffer.size() + Encoded.size() >
			(apb::kRelayMaxFrameBytes * apb::kRelayMaxQueueDepth))
		{
			UE_LOG(LogTemp, Warning, TEXT("RELAY_CLIENT_REJECT reason=send_queue_full"));
			return;
		}
		SendBuffer.append(Encoded);
	}

	bool FlushMessages()
	{
		while (!SendBuffer.empty())
		{
			int32 BytesSent = 0;
			if (!Socket->Send(reinterpret_cast<const uint8*>(SendBuffer.data()),
				static_cast<int32>(SendBuffer.size()), BytesSent))
			{
				return Socket->GetConnectionState() == SCS_Connected;
			}
			if (BytesSent <= 0)
			{
				return true;
			}
			SendBuffer.erase(0, static_cast<size_t>(BytesSent));
		}
		return true;
	}

	void ScheduleReconnect(const int64 NowMs, int64& OutNextConnectMs)
	{
		const int32 Attempt = ReconnectAttempt++;
		const int64 DelayMs = apb::RelayReconnectDelayMs(Attempt);
		OutNextConnectMs = NowMs + DelayMs;
		UE_LOG(LogTemp, Log, TEXT("RELAY_CLIENT_RECONNECT attempt=%d delay_ms=%lld"), Attempt + 1, DelayMs);
	}

	std::string NextRequestId(const TCHAR* Verb)
	{
		const FString RequestId = FString::Printf(TEXT("district-%d-%s-%lld"),
			Config.NumericId, Verb, ++RequestSequence);
		return TCHAR_TO_UTF8(*RequestId);
	}

	void CloseSocket()
	{
		bConnected = false;
		if (Socket)
		{
			Socket->Close();
			SocketSubsystem->DestroySocket(Socket);
			Socket = nullptr;
		}
	}

	ISocketSubsystem* SocketSubsystem = nullptr;
	FRelayDistrictConfig Config;
	std::string ExpectedAuth;
	FSocket* Socket = nullptr;
	FRunnableThread* Thread = nullptr;
	std::string ReceiveBuffer;
	std::string SendBuffer;
	std::deque<std::string> OutboundFrames;
	std::deque<apb::RelayMessage> InboundMessages;
	FCriticalSection QueueLock;
	std::atomic<bool> bStopRequested = false;
	std::atomic<bool> bShutdownLogged = false;
	std::atomic<int32> Population = 0;
	bool bConnected = false;
	int32 ReconnectAttempt = 0;
	int64 ConnectStartedMs = 0;
	int64 LastHeartbeatMs = 0;
	int64 HeartbeatSequence = 0;
	int64 RequestSequence = 0;
};

void UAPBServerControl::Init(AAPBWorldGameMode* InMode)
{
	Mode = InMode;
	bWorldServerRole = FParse::Param(FCommandLine::Get(), TEXT("WorldServer"));
	if (!bWorldServerRole)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("APBServerControl role=WorldServer"));
	const FString& Secret = FAPBSecretProvider::RelaySecret();
	if (Secret.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("RELAY_LISTEN_FAILED reason=missing_ticket_secret"));
		return;
	}

	int32 RelayPort = apb::ports::Relay;
	FParse::Value(FCommandLine::Get(), TEXT("RelayPort="), RelayPort);
	if (RelayPort < 1 || RelayPort > 65535)
	{
		UE_LOG(LogTemp, Error, TEXT("RELAY_LISTEN_FAILED reason=invalid_port value=%d"), RelayPort);
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	RelayListener = new FAPBRelayListener(SocketSubsystem, static_cast<uint16>(RelayPort), TCHAR_TO_UTF8(*Secret));
	if (!RelayListener->Start())
	{
		delete RelayListener;
		RelayListener = nullptr;
	}
}

void UAPBServerControl::InitDistrict(const FString& ResolvedDistrictId, const FString& DistrictEpoch)
{
	bWorldServerRole = FParse::Param(FCommandLine::Get(), TEXT("WorldServer"));
	if (bWorldServerRole)
	{
		return;
	}

	FRelayDistrictConfig DistrictConfig;
	FString ConfigError;
	if (!ResolveDistrictRelayConfig(ResolvedDistrictId, DistrictConfig, ConfigError))
	{
		const FString CommandLine(FCommandLine::Get());
		const bool bRelayRequested = CommandLine.Contains(TEXT("RelayHost="));
		if (bRelayRequested)
		{
			UE_LOG(LogTemp, Error, TEXT("RELAY_CLIENT_FAILED reason=%s"), *ConfigError);
		}
		return;
	}
	DistrictConfig.TargetDistrictEpoch = DistrictEpoch;

	const FString& Secret = FAPBSecretProvider::RelaySecret();
	if (Secret.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("RELAY_CLIENT_FAILED reason=missing_ticket_secret"));
		return;
	}

	DistrictNumericId = DistrictConfig.NumericId;
	RelayDistrictClient = new FAPBRelayDistrictClient(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM),
		MoveTemp(DistrictConfig), TCHAR_TO_UTF8(*Secret));
	if (!RelayDistrictClient->Start())
	{
		delete RelayDistrictClient;
		RelayDistrictClient = nullptr;
	}
}

void UAPBServerControl::SetDistrictPopulation(const int32 PlayerCount)
{
	if (RelayDistrictClient)
	{
		RelayDistrictClient->SetPopulation(PlayerCount);
	}
}

void UAPBServerControl::Shutdown()
{
	if (RelayListener)
	{
		RelayListener->Shutdown();
		delete RelayListener;
		RelayListener = nullptr;
	}
	if (RelayDistrictClient)
	{
		RelayDistrictClient->Shutdown();
		delete RelayDistrictClient;
		RelayDistrictClient = nullptr;
	}
	DistrictNumericId = 0;
}

void UAPBServerControl::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

bool UAPBServerControl::ResolveLiveDistrict(const FString& DistrictId, const int32 MaxPlayers,
	FString& OutHost, int32& OutPort,
	int32& OutNumericId, FString& OutEpoch, FString& OutError) const
{
	if (!RelayListener)
	{
		OutError = TEXT("no_live_node");
		return false;
	}
	return RelayListener->ResolveLeastLoaded(DistrictId, MaxPlayers, PendingReservationsByNode,
		OutHost, OutPort, OutNumericId, OutEpoch, OutError);
}

std::vector<FAPBDistrictPopulationSnapshot> UAPBServerControl::GetLiveDistrictPopulationSnapshot() const
{
	return RelayListener ? RelayListener->SnapshotDistrictPopulations()
		: std::vector<FAPBDistrictPopulationSnapshot>();
}

bool UAPBServerControl::ReserveLiveDistrict(const uint64 PlayerSessionId, const FString& DistrictId,
	const int32 MaxPlayers, FString& OutHost, int32& OutPort, int32& OutNumericId,
	FString& OutEpoch, FString& OutReservationId,
	FString& OutError)
{
	if (!ResolveLiveDistrict(DistrictId, MaxPlayers, OutHost, OutPort, OutNumericId, OutEpoch, OutError))
	{
		return false;
	}
	if (const FString* ExistingReservation = ReservationByPlayer.Find(PlayerSessionId))
	{
		ReleaseLiveDistrictReservation(*ExistingReservation);
	}
	OutReservationId = FString::Printf(TEXT("travel-%lld"), NextReservationId++);
	ReservationByPlayer.Add(PlayerSessionId, OutReservationId);
	ReservationNodeById.Add(OutReservationId, OutNumericId);
	PendingReservationsByNode.FindOrAdd(OutNumericId)++;
	return true;
}

void UAPBServerControl::ReleaseLiveDistrictReservation(const FString& ReservationId)
{
	if (ReservationId.IsEmpty())
	{
		return;
	}
	for (auto It = ReservationByPlayer.CreateIterator(); It; ++It)
	{
		if (It.Value() == ReservationId)
		{
			It.RemoveCurrent();
			break;
		}
	}
	const int32 NodeId = ReservationNodeById.FindRef(ReservationId);
	ReservationNodeById.Remove(ReservationId);
	if (int32* PendingCount = PendingReservationsByNode.Find(NodeId))
	{
		if (*PendingCount > 0)
		{
			--(*PendingCount);
			if (*PendingCount == 0)
			{
				PendingReservationsByNode.Remove(NodeId);
			}
		}
	}
}

TArray<FString> UAPBServerControl::ReleaseLiveDistrictReservationsForPlayer(const uint64 PlayerSessionId)
{
	TArray<FString> ReleasedReservations;
	if (const FString* ReservationId = ReservationByPlayer.Find(PlayerSessionId))
	{
		ReleasedReservations.Add(*ReservationId);
		ReleaseLiveDistrictReservation(*ReservationId);
	}
	return ReleasedReservations;
}

bool UAPBServerControl::SendRelayToDistrict(const int32 NumericId, const apb::RelayMessage& Message)
{
	return RelayListener && RelayListener->QueueToDistrict(NumericId, Message);
}

bool UAPBServerControl::SendRelayToWorld(const apb::RelayMessage& Message)
{
	return RelayDistrictClient && RelayDistrictClient->QueueToWorld(Message);
}

bool UAPBServerControl::DequeueDistrictRelayMessage(apb::RelayMessage& OutMessage)
{
	return RelayDistrictClient && RelayDistrictClient->DequeueInbound(OutMessage);
}

bool UAPBServerControl::DequeueWorldRelayMessage(apb::RelayMessage& OutMessage)
{
	return RelayListener && RelayListener->DequeueInbound(OutMessage);
}

int32 UAPBServerControl::GetDistrictNumericId() const
{
	return DistrictNumericId;
}

bool UAPBServerControl::LoginRequest(APlayerController* PC,
	const FString& User, const FString& Pass, FString& OutError)
{
	if (!Mode) { OutError = TEXT("no_mode"); return false; }
	return Mode->LoginPlayer(PC, User, Pass, OutError);
}

FString UAPBServerControl::GetCharListJson(APlayerController* PC) const
{
	if (!Mode) return TEXT("[]");
	return Mode->GetCharListJson(PC);
}

FString UAPBServerControl::GetDistrictListJson(APlayerController* PC) const
{
	if (!Mode) return TEXT("[]");
	return Mode->GetDistrictListJson(PC);
}

FString UAPBServerControl::IssueTicketJson(APlayerController* PC,
	const FString& CharName, const FString& DistrictId)
{
	if (!Mode) return TEXT("");
	return Mode->IssueTicketJson(PC, CharName, DistrictId);
}
