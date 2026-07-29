// APBSocialStore.cpp — M14 (D10) file-backed social persistence.
// Pure C++17 stdlib (std::filesystem/fstream). Delegates clans/friends serialization
// to the services' SaveJson()/LoadJson(), and mail serialization to JsonDomainStore
// (APBPersistence) — one implementation, no schema duplication.
#include "APBSocialStore.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace apb {
namespace {

std::string ReadFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf();
	return ss.str();
}

bool WriteFile(const std::string& path, const std::string& text) {
	const std::filesystem::path target(path);
	const std::filesystem::path temp(path + ".tmp");
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		out << text;
		out.flush();
		if (!out) {
			out.close();
			std::error_code remove_error;
			std::filesystem::remove(temp, remove_error);
			return false;
		}
		out.close();
		if (!out) {
			std::error_code remove_error;
			std::filesystem::remove(temp, remove_error);
			return false;
		}
	}
	std::error_code rename_error;
	std::filesystem::rename(temp, target, rename_error);
	if (rename_error) {
		std::error_code remove_error;
		std::filesystem::remove(temp, remove_error);
		return false;
	}
	return true;
}

} // namespace

bool SocialStore::Init(const std::string& dir) {
	active_ = false;
	if (dir.empty()) return false;
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(dir), ec);
	if (ec) return false;
	dir_ = dir;
	active_ = true;
	return true;
}

std::string SocialStore::ClansPath() const   { return dir_ + "/clans.json"; }
std::string SocialStore::FriendsPath() const { return dir_ + "/friends.json"; }

bool SocialStore::SaveClans(const ClanService& clans) const {
	if (!active_) return false;
	return WriteFile(ClansPath(), clans.SaveJson());
}

bool SocialStore::LoadClans(ClanService& clans) const {
	if (!active_) return false;
	const std::string text = ReadFile(ClansPath());
	if (text.empty()) return false; // missing/empty file = fresh start
	return clans.LoadJson(text);
}

bool SocialStore::SaveFriends(const FriendsService& friends) const {
	if (!active_) return false;
	return WriteFile(FriendsPath(), friends.SaveJson());
}

bool SocialStore::LoadFriends(FriendsService& friends) const {
	if (!active_) return false;
	const std::string text = ReadFile(FriendsPath());
	if (text.empty()) return false;
	return friends.LoadJson(text);
}

std::string SocialStore::MailPath() const { return dir_ + "/mail.json"; }

bool SocialStore::SaveMail(const MailService& mail) const {
	if (!active_) return false;
	JsonDomainStore proxy;
	if (!proxy.Init(dir_)) return false;
	return proxy.SaveMail(mail);
}

bool SocialStore::LoadMail(MailService& mail) const {
	if (!active_) return false;
	JsonDomainStore proxy;
	if (!proxy.Init(dir_)) return false;
	return proxy.LoadMail(mail);
}

} // namespace apb
