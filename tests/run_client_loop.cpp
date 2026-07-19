#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include <iostream>
#include <fstream>
using namespace apb;
int main(){
  std::ofstream log(R"(C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer\client_loop.log)");
  auto L=[&](const std::string& s){ std::cout<<s<<"\n"; log<<s<<"\n"; };
  WorldService w;
  if(!w.InitFromDataDir(R"(D:\APBReloaded\Content\Data)")) return 1;
  L("INIT items="+std::to_string(w.catalog.items.size())+" districts="+std::to_string(w.catalog.districts.size())+" scripts="+std::to_string(w.mission_scripts.scripts.size()));
  w.RegisterAccount("player1","pass"); w.LoginAccount("player1","pass"); L("LOGIN ok");
  w.EnterWorld("W1"); L("WORLD W1");
  w.CreateCharacter("Viper", Faction::Criminal); L("CHAR Viper Criminal");
  w.EquipClothing("torso","Clothing_Crim_Hoodie_T1",3,0,"tag");
  w.EquipClothing("legs","Clothing_Crim_Jeans_T1",1,0,"");
  w.SaveCharacterConfig(); L("CUSTOMIZATION saved");
  w.JoinDistrict("Financial","Viper"); L("DISTRICT "+w.district->session_id+" map="+w.district->map_name);
  auto chunks=w.StreamChunksNear(0,0); L("STREAM chunks="+std::to_string(chunks.size()));
  std::string wid,vid;
  for(auto& kv:w.catalog.items){ if(kv.second.category=="Weapon"&&wid.empty()) wid=kv.first; if(kv.second.category=="Vehicle"&&vid.empty()) vid=kv.first; }
  CombatantState me{"Viper",Faction::Criminal,1000,0,0,true};
  CombatantState foe{"EnforcerBot",Faction::Enforcer,200,2,0,true};
  auto shot=w.FireWeapon(wid,me,foe,2,0);
  L("SHOOT weapon="+wid+" dmg="+std::to_string(shot.damage)+" hit="+(shot.hit?"1":"0"));
  w.SpawnVehicle(vid); w.PossessVehicle("Viper"); L("DRIVE vehicle="+vid+" possessed="+(w.vehicle->possessed?"1":"0"));
  w.ExitVehicle();
  double t0=w.threat.points;
  foe.health=1; foe.alive=true; w.FireWeapon(wid,me,foe,2,0);
  L("THREAT "+std::to_string(t0)+"->"+std::to_string(w.threat.points)+" pressure="+std::to_string(w.OppositionPressure())+" bots="+std::to_string(w.threat.CurrentTier().bot_count));
  w.StartMissionScript("");
  L("MISSION "+w.mission->title+" stages="+std::to_string(w.mission->StageCount())+" contact="+w.mission->contact_id);
  int s=0; while(w.mission->status==MissionStatus::Active && s++<80) w.AdvanceMission(1.0);
  L("MISSION_STATUS "+std::to_string((int)w.mission->status)+" threat="+std::to_string(w.threat.points));
  std::string buyId; for(auto& kv:w.catalog.items) if(kv.second.armas_listed){buyId=kv.first;break;}
  auto br=w.ArmasBuy(buyId); L(std::string("ARMAS ")+(br.ok?"ok":br.error)+" item="+buyId+" g1c="+std::to_string(w.character->g1c));
  auto ar=w.AuctionList(buyId,1,500); L(std::string("AUCTION_LIST ")+(ar.ok?"ok":ar.error)+" id="+std::to_string(ar.listing_id));
  // second peer sim
  WorldService host=w;
  host.JoinDistrictAsPeer(w.district->session_id,"PeerTwo");
  L("MP peers="+std::to_string(host.district->players.size()));
  for(auto& line:w.log) log<<"LOG "<<line<<"\n";
  L("CLIENT_LOOP_OK");
  return 0;
}
