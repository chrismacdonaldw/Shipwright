"""Exercise the actual native consumption/codec/grant bodies without game assets."""
import argparse
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


FIXTURE = r'''
#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "KeyConsumption.h"
#include "AutosaveFeedback.h"
using u32=uint32_t; using u8=uint8_t;
@SAVE_DATA@
struct SaveContext {
    int fileNum=0,mapIndex=3;
    struct { int8_t dungeonKeys[19]{}; } inventory;
    struct { uint32_t swch=0; } sceneFlags[110];
    struct { AnchorKeyConsumptionSaveData keyConsumption{};
             struct { uint32_t dungeonKeys[19]{}; } stats; } ship;
} gSaveContext;
struct PlayState { int sceneNum=3,gameplayFrames=100; struct { bool running=true; } state;
                   uint32_t currentSwitches=0; } play;
extern "C" { PlayState* gPlayState=&play; }
bool rando=true,skeleton=false,mq=false,ring=false,fortressRing=false;
bool paused=false,saveLoaded=true,saveExists=true,songTime=true,ocarina=false;
#define IS_RANDO rando
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
enum SceneID { SCENE_FOREST_TEMPLE=3, SCENE_THIEVES_HIDEOUT=12, SCENE_TREASURE_BOX_SHOP=16,
               SCENE_CHAMBER_OF_THE_SAGES=69,SCENE_CUTSCENE_MAP=68 };
enum { QUEST_SONG_TIME,ITEM_OCARINA_TIME=2 };
#define CHECK_QUEST_ITEM(q) songTime
#define INV_CONTENT(item) (ocarina?ITEM_OCARINA_TIME:0)
enum { RAND_INF_HAS_SKELETON_KEY, RSK_KEYRINGS, RSK_KEYRINGS_GERUDO_FORTRESS,
       RSK_GERUDO_FORTRESS, RO_GF_CARPENTERS_FAST, RO_GF_CARPENTERS_FREE };
int carpenterMode=-1;
extern "C" bool Flags_GetRandomizerInf(int) { return skeleton; }
extern "C" void Play_SaveSceneFlags(PlayState* p) { gSaveContext.sceneFlags[p->sceneNum].swch=p->currentSwitches; }
namespace AnchorSceneSwitches { bool Eligible(int scene,int flag) { return scene>=0 && flag>=0 && flag<32; } }
bool ResourceMgr_IsSceneMasterQuest(SceneID) { return mq; }
namespace Rando {
struct Option { int value; explicit operator bool() const { return value!=0; } bool Is(int v)const{return value==v;} };
struct DungeonInfo {
    std::vector<uint8_t> vanillaDoorFlags@VANILLA@,randoDoorFlags@RANDO@,MQDoorFlags@MQ@;
    bool HasKeyRing()const{return ring;}
    std::span<const uint8_t> GetDoorFlagsForQuest(bool)const;
};
struct Dungeons { DungeonInfo forest; DungeonInfo* GetDungeonFromScene(int scene) { return scene==3?&forest:nullptr; } };
struct Context { Dungeons dungeons; static Context* GetInstance(){static Context c;return &c;}
    Dungeons* GetDungeons(){return &dungeons;}
    Option GetOption(int key){return {key==RSK_GERUDO_FORTRESS?carpenterMode:fortressRing};}
};
#define RAND_GET_OPTION(k) (Context::GetInstance()->GetOption(k))
@CATALOGUE@
}
struct SaveManager {
    static SaveManager* Instance;
    using SaveFunc=void(*)(const SaveContext&,int,bool);
    SaveFunc encoder=nullptr; void(*decoder)()=nullptr;
    std::function<void(bool)> init;
    std::vector<SaveContext> snapshots;
    std::vector<std::function<void(bool)>> completions;
    std::string scope,arrayName; size_t arrayIndex=0;
    std::map<std::string,std::vector<uint64_t>> arrays;
    void AddInitFunction(void(*f)(bool)){init=f;}
    void AddLoadFunction(const char*,int,void(*f)()){decoder=f;}
    int AddSaveFunction(const char*,int,SaveFunc f,bool,int){encoder=f;return 1;}
    void SaveFile(int,const std::function<void(bool)>& completion){
        snapshots.push_back(gSaveContext);
        auto completed=std::make_shared<bool>(false);
        completions.push_back([completion,completed](bool success){if(!*completed){*completed=true;completion(success);}});
    }
    bool SaveFile_Exist(int){return saveExists;}
    void SaveData(const std::string&,const std::string& s){scope=s;}
    template<class T> void SaveData(const std::string&,T value){arrays[arrayName].push_back(value);}
    void SaveArray(const std::string& name,size_t size,const std::function<void(size_t)>& f){arrayName=name;arrays[name].clear();for(size_t i=0;i<size;++i)f(i);}
    void LoadData(const std::string&,std::string& s){s=scope;}
    template<class T> void LoadData(const std::string&,T& v){v=static_cast<T>(arrays[arrayName][arrayIndex]);}
    void LoadArray(const std::string& name,size_t size,const std::function<void(size_t)>& f){arrayName=name;for(size_t i=0;i<size;++i){arrayIndex=i;f(i);}}
} manager;
SaveManager* SaveManager::Instance=&manager;
constexpr int SECTION_PARENT_NONE=-1;
struct GameInteractor {
    struct OnLoadFile {}; struct OnGameFrameUpdate {}; struct OnSceneSpawnActors {};
    static GameInteractor* Instance;
    std::function<void(int16_t)> loaded;
    std::function<void()> frame;
    std::function<void()> scene;
    static bool IsGameplayPaused(){return paused;}
    static bool IsSaveLoaded(bool){return saveLoaded;}
    template<class H,class F> void RegisterGameHook(F f){
        if constexpr(std::is_same_v<H,OnLoadFile>)loaded=f;
        else if constexpr(std::is_same_v<H,OnGameFrameUpdate>)frame=f;
        else scene=f;
    }
} interactor;
GameInteractor* GameInteractor::Instance=&interactor;
extern "C" int Play_CanPerformAutomaticSave(void);
@SAVE_POLICY@
extern "C" void Play_PerformSaveWithCompletion(PlayState* p,void(*completion)(int,void*),void* userData){
    Play_SaveSceneFlags(p);
    manager.SaveFile(gSaveContext.fileNum,[completion,userData](bool success){completion(success,userData);});
}
struct RegisterShipInitFunc { explicit RegisterShipInitFunc(const std::function<void()>& f){f();} };
@NATIVE@
enum { RG_FOREST_TEMPLE_SMALL_KEY=100,RG_TREASURE_GAME_SMALL_KEY=110,RG_NONE=0,
       ITEM_KEY_SMALL=1,MOD_NONE=0,ITEM_NONE=0 };
int Return_Item_Entry(int,int){return 0;}
int Return_Item(int,int,int){return 0;}
int RandoGrant(int mapIndex) { const int item=RG_FOREST_TEMPLE_SMALL_KEY,giEntry=0;
@RANDO_GRANT@
return -1;
}
int VanillaGrant() { const int item=ITEM_KEY_SMALL;
@VANILLA_GRANT@
return -1;
}
int assertions=0;
void Check(bool condition,const char* message){++assertions;if(!condition)throw std::runtime_error(message);}
AnchorKeyConsumption::Owner owner;
bool ownerAvailable=true,publishOkay=true;
std::vector<AnchorKeyConsumption::Lock> publications;
bool ReadOwner(AnchorKeyConsumption::Owner& out){out=owner;return ownerAvailable;}
bool Publish(const AnchorKeyConsumption::Lock& lock){publications.push_back(lock);return publishOkay;}
void Fresh(int stock=2,uint32_t legacy=0) {
    for(auto& completion:manager.completions)completion(true);
    manager.init(false);gSaveContext.fileNum=0;gSaveContext.inventory.dungeonKeys[3]=static_cast<int8_t>(stock);
    gSaveContext.sceneFlags[3].swch=legacy;play.currentSwitches=legacy;play.sceneNum=3;gPlayState=&play;
    rando=true;skeleton=ring=fortressRing=mq=false;carpenterMode=-1;
    paused=ocarina=false;saveLoaded=saveExists=songTime=true;play.gameplayFrames=100;
    owner={};owner.generation=1;owner.fileNum=0;std::strcpy(owner.scope,"paired-seed");
    owner.active=owner.admitted=owner.ready=true;ownerAvailable=publishOkay=true;publications.clear();
    manager.snapshots.clear();manager.completions.clear();interactor.loaded(0);
    AnchorKeyConsumption::Binding binding{ReadOwner,Publish};AnchorKeyConsumption::SetBinding(&binding);
}
void Open(int flag){play.currentSwitches|=uint32_t{1}<<flag;AnchorKeyConsumption_Consume(&play,3,static_cast<uint16_t>(flag));}
void RoundTrip() {
    const auto image=gSaveContext;manager.encoder(image,0,true);
    gSaveContext.ship.keyConsumption={};manager.decoder();interactor.loaded(0);
}
int main(){using namespace AnchorKeyConsumption;
    Fresh();Apply({});Open(0);Check(gSaveContext.inventory.dungeonKeys[3]==1,"first local cost");
    Open(0);Check(gSaveContext.inventory.dungeonKeys[3]==1,"same-door local refund");
    Bank bank{};bank[3]=3;Apply(bank);Check(gSaveContext.inventory.dungeonKeys[3]==0,"different lock cost");
    Apply(bank);Check(gSaveContext.inventory.dungeonKeys[3]==0,"duplicate remote bank");
    bank[3]=7;Apply(bank);Check(gSaveContext.ship.keyConsumption.owed[3]==1,"zero-stock debt");
    RoundTrip();Check(gSaveContext.ship.keyConsumption.owed[3]==1,"saved debt reload");
    RandoGrant(3);Check(gSaveContext.inventory.dungeonKeys[3]==0 && gSaveContext.ship.keyConsumption.owed[3]==0,"actual rando grant repays before return");
    bank[3]=15;Apply(bank);VanillaGrant();Check(gSaveContext.inventory.dungeonKeys[3]==0 && gSaveContext.ship.keyConsumption.owed[3]==0,"actual vanilla grant repays before return");
    Fresh(2,1);Open(1);Check(gSaveContext.inventory.dungeonKeys[3]==1,"preactor legacy baseline excludes current new flag");
    Check(gSaveContext.ship.keyConsumption.accounted[3]==3 && gSaveContext.ship.keyConsumption.pendingPublish[3]==2,"legacy unlock not published");
    Fresh();publishOkay=false;Open(0);interactor.frame();Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==1,"refused publish retained");
    Check(manager.snapshots.back().ship.keyConsumption.pendingPublish[3]==1 && manager.snapshots.back().inventory.dungeonKeys[3]==1,"pending and stock same save snapshot");
    const auto snapshot=manager.snapshots.back();Open(1);manager.encoder(snapshot,0,true);
    Check(manager.arrays["accounted"][3]==1,"codec uses const snapshot rather than global later bank");
    manager.completions.front()(false);size_t saves=manager.snapshots.size();interactor.frame();Check(manager.snapshots.size()==saves,"failed write not retried every frame");
    interactor.frame();Check(manager.snapshots.size()==saves,"failed write retry remains blocked");
    interactor.scene();interactor.frame();Check(manager.snapshots.size()==saves+1,"safe scene event retries dirty save");
    RoundTrip();Reset();owner.active=false;interactor.frame();Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==3,"park preserves pending");
    owner.active=true;publishOkay=true;owner.sharing=false;interactor.frame();Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==0,"OFF local costs durable publish");
    Check(publications.back().scene==3 && publications.back().domain==3,"collision-free scene domain");
    Fresh();owner.admitted=owner.ready=false;Open(0);interactor.frame();Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==1 && publications.empty(),"admission hold retains local spend");
    Check(!Apply(bank) && gSaveContext.inventory.dungeonKeys[3]==1,"held remote cost has no effect");
    owner.admitted=owner.ready=true;interactor.frame();Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==0,"hold release retries pending");
    Fresh();Open(0);std::strcpy(owner.scope,"different-seed");Check(!Apply(bank),"scope replacement refused");
    Check(gSaveContext.inventory.dungeonKeys[3]==1,"replacement has no old effects");
    Fresh();ring=true;Open(0);Apply(bank);Check(gSaveContext.inventory.dungeonKeys[3]==1 && gSaveContext.ship.keyConsumption.pendingPublish[3]==0,"ring mode retains native behavior");
    Fresh(0);bank[3]=1;Apply(bank);skeleton=true;gSaveContext.inventory.dungeonKeys[3]=5;AnchorKeyConsumption_Granted(3);
    Check(gSaveContext.inventory.dungeonKeys[3]==5 && gSaveContext.ship.keyConsumption.owed[3]==1,"skeleton assignment does not repay old debt");
    Fresh();AnchorKeyConsumption::SetBinding(nullptr);Open(0);Check(gSaveContext.inventory.dungeonKeys[3]==1,"standalone ordinary decrement retained");
    Fresh();play.sceneNum=4;AnchorKeyConsumption_Consume(&play,3,0);Check(gSaveContext.inventory.dungeonKeys[3]==1 && gSaveContext.ship.keyConsumption.pendingPublish[3]==0,"scene domain mismatch unshared");
    Fresh();Open(27);Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==0,"non-key switch excluded by actual catalogue");
    Fresh();mq=true;Open(6);Check(gSaveContext.ship.keyConsumption.pendingPublish[3]==64,"actual MQ catalogue selected");
    Fresh();play.sceneNum=SCENE_THIEVES_HIDEOUT;gSaveContext.inventory.dungeonKeys[12]=2;carpenterMode=RO_GF_CARPENTERS_FAST;
    AnchorKeyConsumption_Consume(&play,12,1);Check(gSaveContext.ship.keyConsumption.pendingPublish[12]==2,"actual fortress fast catalogue");
    AnchorKeyConsumption_Consume(&play,12,2);Check(gSaveContext.ship.keyConsumption.pendingPublish[12]==2,"fortress omitted fast lock excluded");
    Fresh();gSaveContext.inventory.dungeonKeys[16]=2;play.sceneNum=16;AnchorKeyConsumption_Consume(&play,16,0);
    Check(gSaveContext.ship.keyConsumption.pendingPublish[16]==0,"repeatable chest game excluded");
    Fresh();publishOkay=false;Open(0);play.gameplayFrames=59;interactor.frame();Check(manager.snapshots.empty(),"initial magic fill unsafe save deferred");
    play.gameplayFrames=60;play.sceneNum=SCENE_CUTSCENE_MAP;interactor.frame();Check(manager.snapshots.empty(),"cutscene map save deferred");
    play.sceneNum=3;paused=true;interactor.frame();Check(manager.snapshots.empty(),"paused save deferred");
    paused=false;ocarina=true;songTime=false;interactor.frame();Check(manager.snapshots.empty(),"ocarina before songtime save deferred");
    songTime=true;saveExists=false;interactor.frame();Check(manager.snapshots.empty(),"deleted file not recreated");
    saveExists=true;interactor.frame();Check(manager.snapshots.size()==1,"native safe prepared save requested");
    Open(1);interactor.frame();Check(manager.snapshots.size()==1,"inflight save coalesces new cost");
    manager.completions.back()(true);interactor.frame();Check(manager.snapshots.size()==2,"dirty during inflight saved afterward");
    for(auto& completion:manager.completions)completion(true);
    std::cout<<"PASS "<<assertions<<" actual native consumption assertions\n";
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    native = (ROOT / "soh/soh/Network/Anchor/KeyConsumption.cpp").read_text()
    native = re.sub(r'^#include "[^"\n]+"\n', "", native, flags=re.MULTILINE)
    saved = block((ROOT / "soh/include/z64save.h").read_text(), "typedef struct AnchorKeyConsumptionSaveData")
    saved += " AnchorKeyConsumptionSaveData;"
    dungeon = (ROOT / "soh/soh/Enhancements/randomizer/dungeon.cpp").read_text()
    catalogue = block(dungeon, "static std::span<const uint8_t> ThievesHideoutDoorFlags")
    catalogue += "\n" + re.search(r'static constexpr std::array<uint8_t, 6> chestGameDoorFlags[^;]+;', dungeon)[0]
    catalogue += "\n" + block(dungeon, "std::span<const uint8_t> DungeonInfo::GetDoorFlagsForQuest")
    catalogue += "\n" + block(dungeon, "std::span<const uint8_t> GetSceneSmallKeyDoorFlags")
    vectors = re.search(r'SCENE_FOREST_TEMPLE, (\{[^}]+\}), (\{[^}]+\}), (\{[^}]+\})', dungeon).groups()
    rando = (ROOT / "soh/soh/Enhancements/randomizer/randomizer.cpp").read_text()
    rando = block(rando, "if ((item >= RG_FOREST_TEMPLE_SMALL_KEY) && (item <= RG_TREASURE_GAME_SMALL_KEY))")
    vanilla = (ROOT / "soh/src/code/z_parameter.c").read_text()
    vanilla = block(vanilla, "if (item == ITEM_KEY_SMALL)")
    policy = block((ROOT / "soh/soh/Enhancements/QoL/Autosave.cpp").read_text(), "int Play_CanPerformAutomaticSave(void)")
    fixture = FIXTURE
    for token, body in {"SAVE_DATA": saved, "NATIVE": native, "CATALOGUE": catalogue,
                        "VANILLA": vectors[0], "RANDO": vectors[1], "MQ": vectors[2],
                        "RANDO_GRANT": rando, "VANILLA_GRANT": vanilla, "SAVE_POLICY": policy}.items():
        fixture = fixture.replace("@" + token + "@", body)
    with tempfile.TemporaryDirectory(prefix="soh-key-consumption-") as directory:
        path = pathlib.Path(directory)
        (path / "fixture.cpp").write_text(fixture)
        command = [args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   "-I" + str(ROOT / "soh/soh/Network/Anchor"), str(path / "fixture.cpp"), "-o", str(path / "fixture")]
        command += ["-I" + str(ROOT / "soh/soh/Enhancements/QoL")]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(command, check=True)
        subprocess.run([str(path / "fixture")], check=True)


if __name__ == "__main__":
    main()
