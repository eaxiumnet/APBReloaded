// APBChat.cpp — M7 N5: ChatService routing implementation (pure C++17).
#include "APBChat.h"
#include <cctype>
#include <sstream>

namespace apb {

std::string ChatService::ToLower(const std::string& s) {
	std::string o = s;
	for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return o;
}

std::string ChatService::Trim(const std::string& s) {
	size_t b = 0, e = s.size();
	while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
	while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
	return s.substr(b, e - b);
}

// ---- roster / prefs -------------------------------------------------------

void ChatService::AddPlayer(const std::string& name) {
	if (name.empty()) return;
	roster_.emplace(name, PlayerChatState{});
}

void ChatService::RemovePlayer(const std::string& name) {
	roster_.erase(name);
	send_times_.erase(name);
}

bool ChatService::HasPlayer(const std::string& name) const {
	return roster_.find(name) != roster_.end();
}

PlayerChatState* ChatService::Player(const std::string& name) {
	auto it = roster_.find(name);
	return it == roster_.end() ? nullptr : &it->second;
}

const PlayerChatState* ChatService::Player(const std::string& name) const {
	auto it = roster_.find(name);
	return it == roster_.end() ? nullptr : &it->second;
}

void ChatService::SetPresence(const std::string& name, PresenceStatus s) {
	if (auto* p = Player(name)) p->presence = s;
}
void ChatService::SetProfanityFilter(const std::string& name, bool on) {
	if (auto* p = Player(name)) p->profanity_filter = on;
}
void ChatService::SetGroup(const std::string& name, const std::string& group_id) {
	if (auto* p = Player(name)) p->group_id = group_id;
}
void ChatService::SetClan(const std::string& name, const std::string& clan_id) {
	if (auto* p = Player(name)) p->clan_id = clan_id;
}
void ChatService::SetFaction(const std::string& name, const std::string& faction) {
	if (auto* p = Player(name)) p->faction = faction;
}
void ChatService::AddIgnore(const std::string& name, const std::string& ignored_name) {
	if (auto* p = Player(name)) p->ignored.insert(ToLower(ignored_name));
}
void ChatService::RemoveIgnore(const std::string& name, const std::string& ignored_name) {
	if (auto* p = Player(name)) p->ignored.erase(ToLower(ignored_name));
}

void ChatService::AddProfanity(const std::string& word) {
	if (!word.empty()) profanity_.insert(ToLower(word));
}

// ---- slash-command parsing ------------------------------------------------

ParsedChat ChatService::ParseCommand(const std::string& raw, ChatChannel default_channel) {
	ParsedChat out;
	out.channel = default_channel;
	const std::string trimmed = Trim(raw);
	if (trimmed.empty()) { out.ok = false; return out; }

	if (trimmed[0] != '/') { out.body = trimmed; out.ok = true; return out; }

	// Split leading token.
	size_t sp = trimmed.find_first_of(" \t");
	std::string cmd = ToLower(trimmed.substr(1, (sp == std::string::npos ? std::string::npos : sp - 1)));
	std::string rest = (sp == std::string::npos) ? std::string() : Trim(trimmed.substr(sp + 1));

	auto isCmd = [&](std::initializer_list<const char*> names) {
		for (const char* n : names) if (cmd == n) return true;
		return false;
	};

	if (isCmd({"w", "whisper", "t", "tell", "pm"})) {
		// "/w <name> <msg>" — name is the next token.
		size_t nsp = rest.find_first_of(" \t");
		if (nsp == std::string::npos) { out.ok = false; return out; } // no message body
		out.channel = ChatChannel::Whisper;
		out.target  = rest.substr(0, nsp);
		out.body    = Trim(rest.substr(nsp + 1));
		out.ok      = !out.target.empty() && !out.body.empty();
		return out;
	}
	if (isCmd({"d", "district"}))            out.channel = ChatChannel::District;
	else if (isCmd({"l", "local", "s", "say"})) out.channel = ChatChannel::Local;
	else if (isCmd({"g", "group", "team"}))  out.channel = ChatChannel::Group;
	else if (isCmd({"f", "faction"}))        out.channel = ChatChannel::Faction;
	else if (isCmd({"c", "clan"}))           out.channel = ChatChannel::Clan;
	else if (isCmd({"trade"}))               out.channel = ChatChannel::Trade;
	else { // unknown command: treat whole line as plain text on default channel
		out.body = trimmed; out.ok = true; return out;
	}
	out.body = rest;
	out.ok = !rest.empty();
	return out;
}

// ---- flood control --------------------------------------------------------

bool ChatService::RegisterSendAndCheckFlood(const std::string& sender, int64_t now_ms) {
	auto& dq = send_times_[sender];
	const int64_t cutoff = now_ms - flood_window_ms;
	while (!dq.empty() && dq.front() < cutoff) dq.pop_front();
	if (static_cast<int32_t>(dq.size()) >= flood_limit) return false; // already at limit -> muted
	dq.push_back(now_ms);
	return true;
}

// ---- profanity masking ----------------------------------------------------

std::string ChatService::ApplyProfanity(const std::string& body) const {
	if (profanity_.empty()) return body;
	std::string out;
	out.reserve(body.size());
	std::string word;
	auto flush = [&]() {
		if (!word.empty()) {
			if (profanity_.count(ToLower(word))) out.append(word.size(), '*');
			else out += word;
			word.clear();
		}
	};
	for (char c : body) {
		if (std::isalnum(static_cast<unsigned char>(c))) { word += c; }
		else { flush(); out += c; }
	}
	flush();
	return out;
}

// ---- delivery -------------------------------------------------------------

void ChatService::EmitTo(std::vector<ChatDelivery>& out, const std::string& recipient,
                         const ChatMessage& base) const {
	const PlayerChatState* rp = Player(recipient);
	// System messages ignore prefs; other channels honor recipient ignore/filter.
	if (base.channel != ChatChannel::System && rp && !base.sender.empty()) {
		if (rp->ignored.count(ToLower(base.sender))) return; // recipient ignores sender
	}
	ChatDelivery d;
	d.recipient = recipient;
	d.message = base;
	if (base.channel != ChatChannel::System && rp && rp->profanity_filter) {
		d.message.body = ApplyProfanity(base.body);
	}
	out.push_back(std::move(d));
}

// ---- submit ---------------------------------------------------------------

SubmitResult ChatService::Submit(const std::string& sender, ChatChannel channel,
                                 const std::string& target, const std::string& body,
                                 int64_t now_ms) {
	SubmitResult res;
	const std::string text = Trim(body);
	if (text.empty()) { res.status = ChatResult::Empty; return res; }

	if (!RegisterSendAndCheckFlood(sender, now_ms)) { res.status = ChatResult::Muted; return res; }

	ChatMessage msg;
	msg.channel = channel;
	msg.sender  = sender;
	msg.target  = target;
	msg.body    = text;
	msg.timestamp = now_ms;

	switch (channel) {
	case ChatChannel::Whisper: {
		if (!HasPlayer(target)) { res.status = ChatResult::RecipientOffline; return res; }
		if (const PlayerChatState* tp = Player(target); tp && tp->presence == PresenceStatus::DoNotDisturb) {
			res.status = ChatResult::RecipientBusy; return res;
		}
		EmitTo(res.deliveries, target, msg); // recipient (may be dropped if they ignore sender)
		EmitTo(res.deliveries, sender, msg); // echo to sender's own log
		break;
	}
	case ChatChannel::Group:
	case ChatChannel::Clan:
	case ChatChannel::Faction: {
		const PlayerChatState* sp = Player(sender);
		std::string scope = sp ? (channel == ChatChannel::Group ? sp->group_id
		                        : channel == ChatChannel::Clan  ? sp->clan_id
		                                                        : sp->faction)
		                       : std::string();
		if (scope.empty()) { res.status = ChatResult::BadChannel; return res; }
		for (const auto& kv : roster_) {
			const std::string& other = channel == ChatChannel::Group ? kv.second.group_id
			                          : channel == ChatChannel::Clan ? kv.second.clan_id
			                                                          : kv.second.faction;
			if (other == scope) EmitTo(res.deliveries, kv.first, msg);
		}
		break;
	}
	case ChatChannel::Local:
	case ChatChannel::District:
	case ChatChannel::Trade:
	default: {
		for (const auto& kv : roster_) EmitTo(res.deliveries, kv.first, msg);
		break;
	}
	}
	res.status = ChatResult::Delivered;
	return res;
}

SubmitResult ChatService::SubmitRaw(const std::string& sender, const std::string& raw,
                                    ChatChannel default_channel, int64_t now_ms) {
	ParsedChat p = ParseCommand(raw, default_channel);
	if (!p.ok) { SubmitResult r; r.status = ChatResult::Empty; return r; }
	return Submit(sender, p.channel, p.target, p.body, now_ms);
}

SubmitResult ChatService::SystemBroadcast(const std::string& body, int64_t now_ms) {
	SubmitResult res;
	const std::string text = Trim(body);
	if (text.empty()) { res.status = ChatResult::Empty; return res; }
	ChatMessage msg;
	msg.channel = ChatChannel::System;
	msg.sender.clear();
	msg.body = text;
	msg.timestamp = now_ms;
	for (const auto& kv : roster_) EmitTo(res.deliveries, kv.first, msg);
	res.status = ChatResult::Delivered;
	return res;
}

} // namespace apb
