#include "testEnv.hpp"

class PlayerManager {
public:
    struct Player {
        SqPlus::ScriptStringVar64 name;
        void printName(void) {
            scprintf(_SC("Player.name = %s\n"), name.s);
        }
    };
    Player playerVar; // Will be accessed directly.
    Player players[2];
    Player *GetPlayer(int player) {
        // Must return pointer: a returned reference will behave the
        // same as return by value.
        return &players[player];
    }
    PlayerManager() {
        players[0].name = _SC("Player1");
        players[1].name = _SC("Player2");
        playerVar.name  = _SC("PlayerVar");
    }
} playerManager;

DECLARE_INSTANCE_TYPE(PlayerManager);
DECLARE_INSTANCE_TYPE(PlayerManager::Player);

PlayerManager *
getPlayerManager(void) {
    // Must return pointer: a returned reference will behave the same
    // as return by value.
    return &playerManager;
}

SQPLUS_TEST(Test_Instance)
{
    SQPLUS_TEST_TRACE();

    using namespace SqPlus;

    // === BEGIN Instance Test ===

    SQClassDef<PlayerManager::Player>(_SC("PlayerManager::Player"))
        .func(&PlayerManager::Player::printName, _SC("printName"))
        .var(&PlayerManager::Player::name, _SC("name"))
        ;
    SQClassDef<PlayerManager>(_SC("PlayerManager"))
        .func(&PlayerManager::GetPlayer,_SC("GetPlayer"))
        .var(&PlayerManager::playerVar,_SC("playerVar"))
        ;
    RegisterGlobal(getPlayerManager, _SC("getPlayerManager"));
    BindVariable(&playerManager, _SC("playerManagerVar"));

    SQPLUS_TEST_TRACE_SUB(testGetInstance);
    RUN_SCRIPT(_SC("\
        local PlayerManager = getPlayerManager(); \n\
        local oPlayer = PlayerManager.GetPlayer(0); \n\
        print(typeof oPlayer); \n\
        assert(typeof(oPlayer) == \"instance\") \n\
        oPlayer.printName(); \n\
        assert(oPlayer.name == \"Player1\") \n\
        PlayerManager.playerVar.printName(); \n\
        print(PlayerManager.playerVar.name); \n\
        assert(PlayerManager.playerVar.name == \"PlayerVar\") \n\
        oPlayer = PlayerManager.playerVar; \n\
        oPlayer.name = \"New_Name1\"; \n\
        playerManagerVar.playerVar.printName(); \n\
        assert(playerManagerVar.playerVar.name == \"New_Name1\") \n\
        oPlayer.name = \"New_Name2\"; \n\
      "));
    scprintf(_SC("playerManager.playerVar.name: %s\n"),
             (const SQChar*)playerManager.playerVar.name);
    
    CHECK_EQUAL((const SQChar*)playerManager.playerVar.name, _SC("New_Name2"));

    // === END Instance Test ===
}
