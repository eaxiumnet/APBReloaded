#include <cstdlib>
#include "APBCatalog.h"
namespace apb {
namespace {
std::string ReadFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary); if(!in) return {};
	std::ostringstream ss; ss<<in.rdbuf(); return ss.str();
}
}
std::string JsonGetString(const std::string& obj, const std::string& key, const std::string& def) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p=obj.find(pat); if(p==std::string::npos) return def;
	p=obj.find(':', p+pat.size()); if(p==std::string::npos) return def;
	p=obj.find(char(34), p+1); if(p==std::string::npos) return def;
	size_t e=p+1; std::string out;
	while(e<obj.size()){
		if(obj[e]==char(92) && e+1<obj.size()){ out.push_back(obj[e+1]); e+=2; continue; }
		if(obj[e]==char(34)) break;
		out.push_back(obj[e]); ++e;
	}
	return out.empty()?def:out;
}
double JsonGetNumber(const std::string& obj, const std::string& key, double def) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p=obj.find(pat); if(p==std::string::npos) return def;
	p=obj.find(':', p+pat.size()); if(p==std::string::npos) return def; ++p;
	while(p<obj.size()&&isspace((unsigned char)obj[p])) ++p;
	char* end=nullptr; const char* start=obj.c_str()+p;
	double v=strtod(start,&end); if(end==start) return def; return v;
}
std::vector<std::string> JsonSplitObjects(const std::string& text) {
	std::vector<std::string> out; int depth=0; size_t start=std::string::npos;
	for(size_t i=0;i<text.size();++i){
		if(text[i]=='{'){ if(depth==0) start=i; ++depth; }
		else if(text[i]=='}'){ --depth; if(depth==0&&start!=std::string::npos){ out.push_back(text.substr(start,i-start+1)); start=std::string::npos; } }
	}
	return out;
}
bool Catalog::LoadWeaponsJson(const std::string& path){
	std::string text=ReadFile(path); if(text.empty()) return false;
	for(const auto& obj: JsonSplitObjects(text)){
		ItemDef d; d.id=JsonGetString(obj,"id"); if(d.id.empty()) continue;
		d.name=JsonGetString(obj,"name",d.id); d.category="Weapon";
		d.damage=JsonGetNumber(obj,"damage",35); d.clip=(int32_t)JsonGetNumber(obj,"clip",30);
		d.rpm=JsonGetNumber(obj,"rpm",400); d.max_range=JsonGetNumber(obj,"max_range",80);
		d.armas_price=(int64_t)((d.damage>50.0)?d.damage:50.0); d.market_value=d.armas_price/2; d.armas_listed=true;
		items[d.id]=d;
	}
	return true;
}
bool Catalog::LoadVehiclesJson(const std::string& path){
	std::string text=ReadFile(path); if(text.empty()) return false;
	for(const auto& obj: JsonSplitObjects(text)){
		ItemDef d; d.id=JsonGetString(obj,"id"); if(d.id.empty()) continue;
		d.name=JsonGetString(obj,"name",d.id); d.category="Vehicle";
		d.max_range=JsonGetNumber(obj,"max_speed",30);
		d.armas_price=(int64_t)(d.max_range*100); d.market_value=d.armas_price/2; d.armas_listed=true;
		items[d.id]=d;
	}
	return true;
}
bool Catalog::LoadDistrictsJson(const std::string& path){
	std::string text=ReadFile(path); if(text.empty()) return false; districts.clear();
	for(const auto& obj: JsonSplitObjects(text)){
		DistrictInfo d; d.id=JsonGetString(obj,"id"); d.name=JsonGetString(obj,"name",d.id);
		d.map_name=JsonGetString(obj,"map","Lvl_ThirdPerson"); if(d.id.empty()) continue;
		d.joinable = d.name.find("DNT")==std::string::npos; d.max_players=64; districts.push_back(d);
	}
	return !districts.empty();
}
bool Catalog::LoadMissionsJson(const std::string& path){
	std::string text=ReadFile(path); if(text.empty()) return false;
	for(const auto& obj: JsonSplitObjects(text)){
		std::string id=JsonGetString(obj,"id"); std::string title=JsonGetString(obj,"title",id);
		if(!id.empty()) mission_titles[id]=title;
	}
	return !mission_titles.empty();
}
bool Catalog::LoadClothingJson(const std::string& path){
	std::string text=ReadFile(path); if(text.empty()) return false;
	for(const auto& obj: JsonSplitObjects(text)){
		ItemDef d; d.id=JsonGetString(obj,"id"); if(d.id.empty()) continue;
		d.name=JsonGetString(obj,"name",d.id);
		d.category=JsonGetString(obj,"category","Clothing");
		d.armas_price=(int64_t)JsonGetNumber(obj,"armas_price",100);
		d.market_value=d.armas_price/2;
		d.armas_listed=JsonGetNumber(obj,"armas_listed",1)!=0;
		items[d.id]=d;
	}
	return true;
}
bool Catalog::LoadAllFromDir(const std::string& dir){
	source_note="https://apbdb.com/ + Content/Data";
	bool ok=false;
	ok|=LoadWeaponsJson(dir+"/weapons.json");
	ok|=LoadVehiclesJson(dir+"/vehicles.json");
	ok|=LoadDistrictsJson(dir+"/districts.json");
	ok|=LoadMissionsJson(dir+"/missions.json");
	ok|=LoadClothingJson(dir+"/clothing.json");
	return ok;
}
}
