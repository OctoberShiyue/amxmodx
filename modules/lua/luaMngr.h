
#ifndef _INCLUDE_AMXMODX_LUA_HEADER_H
#define _INCLUDE_AMXMODX_LUA_HEADER_H

#include <main.h>

extern "C" {
#include <lualib/lua.c>
#include <lualib/lualib.h>
#include <lualib/lauxlib.h>
}
typedef struct tagAMX_HEADER {
  int32_t size          PACKED; /* size of the "file" */
  uint16_t magic        PACKED; /* signature */
  char    file_version; 		/* file format version */
  char    amx_version; 			/* required version of the AMX */
  int16_t flags         PACKED;
  int16_t defsize       PACKED; /* size of a definition record */
  int32_t cod           PACKED; /* initial value of COD - code block */
  int32_t dat           PACKED; /* initial value of DAT - data block */
  int32_t hea           PACKED; /* initial value of HEA - start of the heap */
  int32_t stp           PACKED; /* initial value of STP - stack top */
  int32_t cip           PACKED; /* initial value of CIP - the instruction pointer */
  int32_t publics       PACKED; /* offset to the "public functions" table */
  int32_t natives       PACKED; /* offset to the "native functions" table */
  int32_t libraries     PACKED; /* offset to the table of libraries */
  int32_t pubvars       PACKED; /* the "public variables" table */
  int32_t tags          PACKED; /* the "public tagnames" table */
  int32_t nametable     PACKED; /* name table */
} PACKED AMX_HEADER;

typedef struct tagFUNCSTUBNT {
  ucell address         PACKED;
  ucell nameofs      PACKED;	//we need this for amxx to be backwards comaptible
} PACKED AMX_FUNCSTUBNT;

cell UTIL_ExecNative(AMX *amx, const char *Nativename, cell *params);

void CBasePlayer_Spawn(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *pthis);
void CBasePlayer_Precache(IReGameHook_CBasePlayer_Precache *chain, CBasePlayer *pthis);
int CBasePlayer_ObjectCaps(IReGameHook_CBasePlayer_ObjectCaps *chain, CBasePlayer *pthis);
int CBasePlayer_Classify(IReGameHook_CBasePlayer_Classify *chain, CBasePlayer *pthis);
void CBasePlayer_TraceAttack(IReGameHook_CBasePlayer_TraceAttack *chain, CBasePlayer *pthis, entvars_t *pevAttacker, float flDamage, Vector &vecDir, TraceResult *ptr, int bitsDamageType);
BOOL CBasePlayer_TakeDamage(IReGameHook_CBasePlayer_TakeDamage *chain, CBasePlayer *pthis, entvars_t *pevInflictor, entvars_t *pevAttacker, float &flDamage, int bitsDamageType);
BOOL CBasePlayer_TakeHealth(IReGameHook_CBasePlayer_TakeHealth *chain, CBasePlayer *pthis, float flHealth, int bitsDamageType);
void CBasePlayer_Killed(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *pthis, entvars_t *pevAttacker, int iGib);
void CBasePlayer_AddPoints(IReGameHook_CBasePlayer_AddPoints *chain, CBasePlayer *pthis, int score, BOOL bAllowNegative);
void CBasePlayer_AddPointsToTeam(IReGameHook_CBasePlayer_AddPointsToTeam *chain, CBasePlayer *pthis, int score, BOOL bAllowNegative);
void CBasePlayer_RoundRespawn(IReGameHook_CBasePlayer_RoundRespawn *chain, CBasePlayer *pthis);
void CBasePlayer_Blind(IReGameHook_CBasePlayer_Blind *chain, CBasePlayer *pthis, float view_fade_time, float view_fade_hold, float view_fade_alpha, int view_fade_flags);
CBasePlayer *CBasePlayer_Observer_IsValidTarget(IReGameHook_CBasePlayer_Observer_IsValidTarget *chain, CBasePlayer *pthis, int iTargetIndex, bool bSameTeam);
void CBasePlayer_SetAnimation(IReGameHook_CBasePlayer_SetAnimation *chain, CBasePlayer *pthis, PLAYER_ANIM playerAnim);
void CBasePlayer_GiveDefaultItems(IReGameHook_CBasePlayer_GiveDefaultItems *chain, CBasePlayer *pthis);
CBaseEntity *CBasePlayer_GiveNamedItem(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *pthis, const char *pszName);
void CBasePlayer_AddAccount(IReGameHook_CBasePlayer_AddAccount *chain, CBasePlayer *pthis, int amount, RewardType type, bool bTrackChange);
void CBasePlayer_GiveShield(IReGameHook_CBasePlayer_GiveShield *chain, CBasePlayer *pthis, bool bDeploy);
void CBasePlayer_SetClientUserInfoModel(IReGameHook_CBasePlayer_SetClientUserInfoModel *chain, CBasePlayer *pthis, char *infobuffer, char *newValue);
bool CBasePlayer_SetClientUserInfoName(IReGameHook_CBasePlayer_SetClientUserInfoName *chain, CBasePlayer *pthis, char *infobuffer, char *newValue);
bool CBasePlayer_HasRestrictItem(IReGameHook_CBasePlayer_HasRestrictItem *chain, CBasePlayer *pthis, ItemID item, ItemRestType type);
CBaseEntity *CBasePlayer_DropPlayerItem(IReGameHook_CBasePlayer_DropPlayerItem *chain, CBasePlayer *pthis, const char *pszItemName);
CBaseEntity *CBasePlayer_DropShield(IReGameHook_CBasePlayer_DropShield *chain, CBasePlayer *pthis, bool bDeploy);
void CBasePlayer_OnSpawnEquip(IReGameHook_CBasePlayer_OnSpawnEquip *chain, CBasePlayer *pthis, bool bAddDefault, bool bEquipGame);
void CBasePlayer_Radio(IReGameHook_CBasePlayer_Radio *chain, CBasePlayer *pthis, const char *pszRadioName, const char *pszRadioMessage, short iPitch, bool bShowIcon);
void CBasePlayer_Disappear(IReGameHook_CBasePlayer_Disappear *chain, CBasePlayer *pthis);
void CBasePlayer_MakeVIP(IReGameHook_CBasePlayer_MakeVIP *chain, CBasePlayer *pthis);
bool CBasePlayer_MakeBomber(IReGameHook_CBasePlayer_MakeBomber *chain, CBasePlayer *pthis);
void CBasePlayer_StartObserver(IReGameHook_CBasePlayer_StartObserver *chain, CBasePlayer *pthis, Vector &vecPosition, Vector &vecViewAngle);
bool CBasePlayer_GetIntoGame(IReGameHook_CBasePlayer_GetIntoGame *chain, CBasePlayer *pthis);
void CBaseAnimating_ResetSequenceInfo(IReGameHook_CBaseAnimating_ResetSequenceInfo *chain, CBaseAnimating *pthis);
int GetForceCamera(IReGameHook_GetForceCamera *chain, CBasePlayer *pPlayer);
void PlayerBlind(IReGameHook_PlayerBlind *chain, CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, float fadeTime, float fadeHold, int alpha, Vector &color);
void RadiusFlash_TraceLine(IReGameHook_RadiusFlash_TraceLine *chain, CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, Vector &vecSrc, Vector &vecSpot, TraceResult *ptr);
bool RoundEnd(IReGameHook_RoundEnd *chain, int winStatus, ScenarioEventEndRound event, float tmDelay);
CGameRules *InstallGameRules(IReGameHook_InstallGameRules *chain);
void PM_Init(IReGameHook_PM_Init *chain, struct playermove_s *ppmove);
void PM_Move(IReGameHook_PM_Move *chain, struct playermove_s *ppmove, int server);
void PM_AirMove(IReGameHook_PM_AirMove *chain, int server);
void HandleMenu_ChooseAppearance(IReGameHook_HandleMenu_ChooseAppearance *chain, CBasePlayer *pPlayer, int slot);
BOOL HandleMenu_ChooseTeam(IReGameHook_HandleMenu_ChooseTeam *chain, CBasePlayer *pPlayer, int slot);
void ShowMenu(IReGameHook_ShowMenu *chain, CBasePlayer *pPlayer, int slots, int displaytime, BOOL needmore, char *pszText);
void ShowVGUIMenu(IReGameHook_ShowVGUIMenu *chain, CBasePlayer *pPlayer, int menuType, int slots, char *pszOldMenu);
bool BuyGunAmmo(IReGameHook_BuyGunAmmo *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem, bool bIsPrimary);
CBaseEntity *BuyWeaponByWeaponID(IReGameHook_BuyWeaponByWeaponID *chain, CBasePlayer *pPlayer, WeaponIdType weaponID);
void InternalCommand(IReGameHook_InternalCommand *chain, edict_t *pEdict, const char *szCmd, const char *szVal);
BOOL CSGameRules_FShouldSwitchWeapon(IReGameHook_CSGameRules_FShouldSwitchWeapon *chain, CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
BOOL CSGameRules_GetNextBestWeapon(IReGameHook_CSGameRules_GetNextBestWeapon *chain, CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
float CSGameRules_FlPlayerFallDamage(IReGameHook_CSGameRules_FlPlayerFallDamage *chain, CBasePlayer *pPlayer);
BOOL CSGameRules_FPlayerCanTakeDamage(IReGameHook_CSGameRules_FPlayerCanTakeDamage *chain, CBasePlayer *pPlayer, CBaseEntity *pAttacker);
void CSGameRules_PlayerSpawn(IReGameHook_CSGameRules_PlayerSpawn *chain, CBasePlayer *pPlayer);
BOOL CSGameRules_FPlayerCanRespawn(IReGameHook_CSGameRules_FPlayerCanRespawn *chain, CBasePlayer *pPlayer);
edict_t *CSGameRules_GetPlayerSpawnSpot(IReGameHook_CSGameRules_GetPlayerSpawnSpot *chain, CBasePlayer *pPlayer);
void CSGameRules_ClientUserInfoChanged(IReGameHook_CSGameRules_ClientUserInfoChanged *chain, CBasePlayer *pPlayer, char *infobuffer);
void CSGameRules_PlayerKilled(IReGameHook_CSGameRules_PlayerKilled *chain, CBasePlayer *pPlayer, entvars_t *pevKiller, entvars_t *pevInflictor);
void CSGameRules_DeathNotice(IReGameHook_CSGameRules_DeathNotice *chain, CBasePlayer *pPlayer, entvars_t *pevKiller, entvars_t *pevInflictor);
BOOL CSGameRules_CanHavePlayerItem(IReGameHook_CSGameRules_CanHavePlayerItem *chain, CBasePlayer *pPlayer, CBasePlayerItem *pItem);
int CSGameRules_DeadPlayerWeapons(IReGameHook_CSGameRules_DeadPlayerWeapons *chain, CBasePlayer *pPlayer);
void CSGameRules_ServerDeactivate(IReGameHook_CSGameRules_ServerDeactivate *chain);
void CSGameRules_CheckMapConditions(IReGameHook_CSGameRules_CheckMapConditions *chain);
void CSGameRules_CleanUpMap(IReGameHook_CSGameRules_CleanUpMap *chain);
void CSGameRules_RestartRound(IReGameHook_CSGameRules_RestartRound *chain);
void CSGameRules_CheckWinConditions(IReGameHook_CSGameRules_CheckWinConditions *chain);
void CSGameRules_RemoveGuns(IReGameHook_CSGameRules_RemoveGuns *chain);
void CSGameRules_GiveC4(IReGameHook_CSGameRules_GiveC4 *chain);
void CSGameRules_ChangeLevel(IReGameHook_CSGameRules_ChangeLevel *chain);
void CSGameRules_GoToIntermission(IReGameHook_CSGameRules_GoToIntermission *chain);
void CSGameRules_BalanceTeams(IReGameHook_CSGameRules_BalanceTeams *chain);
void CSGameRules_OnRoundFreezeEnd(IReGameHook_CSGameRules_OnRoundFreezeEnd *chain);
void PM_UpdateStepSound(IReGameHook_PM_UpdateStepSound *chain);
void CBasePlayer_StartDeathCam(IReGameHook_CBasePlayer_StartDeathCam *chain, CBasePlayer *pthis);
void CBasePlayer_SwitchTeam(IReGameHook_CBasePlayer_SwitchTeam *chain, CBasePlayer *pthis);
bool CBasePlayer_CanSwitchTeam(IReGameHook_CBasePlayer_CanSwitchTeam *chain, CBasePlayer *pthis, TeamName team);
CGrenade *CBasePlayer_ThrowGrenade(IReGameHook_CBasePlayer_ThrowGrenade *chain, CBasePlayer *pthis, CBasePlayerWeapon *pWeapon, Vector &vecOrigin, Vector &vecVelocity, float time, unsigned short type);
bool CSGameRules_CanPlayerHearPlayer(IReGameHook_CSGameRules_CanPlayerHearPlayer *chain, CBasePlayer *pListener, CBasePlayer *pSender);
void CWeaponBox_SetModel(IReGameHook_CWeaponBox_SetModel *chain, CWeaponBox *pthis, const char *pszModelName);
void CGrenade_DefuseBombStart(IReGameHook_CGrenade_DefuseBombStart *chain, CGrenade *pthis, CBasePlayer *pPlayer);
void CGrenade_DefuseBombEnd(IReGameHook_CGrenade_DefuseBombEnd *chain, CGrenade *pthis, CBasePlayer *pPlayer, bool bDefused);
void CGrenade_ExplodeHeGrenade(IReGameHook_CGrenade_ExplodeHeGrenade *chain, CGrenade *pthis, TraceResult *pTrace, int bitsDamageType);
void CGrenade_ExplodeFlashbang(IReGameHook_CGrenade_ExplodeFlashbang *chain, CGrenade *pthis, TraceResult *pTrace, int bitsDamageType);
void CGrenade_ExplodeSmokeGrenade(IReGameHook_CGrenade_ExplodeSmokeGrenade *chain, CGrenade *pthis);
void CGrenade_ExplodeBomb(IReGameHook_CGrenade_ExplodeBomb *chain, CGrenade *pthis, TraceResult *pTrace, int bitsDamageType);
CGrenade *ThrowHeGrenade(IReGameHook_ThrowHeGrenade *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity, float time, int iDamage, unsigned short type);
CGrenade *ThrowFlashbang(IReGameHook_ThrowFlashbang *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity, float time);
CGrenade *ThrowSmokeGrenade(IReGameHook_ThrowSmokeGrenade *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity, float time, unsigned short type);
CGrenade *PlantBomb(IReGameHook_PlantBomb *chain, entvars_t *pevOwner, Vector &vecOrigin, Vector &vecVelocity);
void CBasePlayer_RemoveSpawnProtection(IReGameHook_CBasePlayer_RemoveSpawnProtection *chain, CBasePlayer *pthis);
void CBasePlayer_SetSpawnProtection(IReGameHook_CBasePlayer_SetSpawnProtection *chain, CBasePlayer *pthis, float time);
bool IsPenetrableEntity(IReGameHook_IsPenetrableEntity *chain, Vector &vecSrc, Vector &vecDest, entvars_t *pevInflictor, edict_t *pEnt);


qboolean Steam_NotifyClientConnect(IRehldsHook_Steam_NotifyClientConnect *chain, IGameClient *cl, const void *pvSteam2Key, unsigned int ucbSteam2Key);
void SV_ConnectClient(IRehldsHook_SV_ConnectClient *chain);
char* SV_GetIDString(IRehldsHook_SV_GetIDString *chain, USERID_t *id);
void SV_SendServerinfo(IRehldsHook_SV_SendServerinfo *chain, sizebuf_t *msg, IGameClient *cl);
int SV_CheckProtocol(IRehldsHook_SV_CheckProtocol *chain, netadr_t *adr, int protocol);
void SVC_GetChallenge_mod(IRehldsHook_SVC_GetChallenge_mod *chain, char *buffer, int clientChallenge);
int SV_CheckKeyInfo(IRehldsHook_SV_CheckKeyInfo *chain, netadr_t *adr, char *protinfo, uint16 *port, int *player_idx, char *authinfo, char *server_cdkey);
int SV_CheckIPRestrictions(IRehldsHook_SV_CheckIPRestrictions *chain, netadr_t *adr, int protocol);
int SV_FinishCertificateCheck(IRehldsHook_SV_FinishCertificateCheck *chain, netadr_t *adr, int protocol, char *authinfo, char *server_cdkey);
qboolean Steam_NotifyBotConnect(IRehldsHook_Steam_NotifyBotConnect *chain, IGameClient *cl);
void SerializeSteamId(IRehldsHook_SerializeSteamId *chain, USERID_t *id1, USERID_t *id2);
qboolean SV_CompareUserID(IRehldsHook_SV_CompareUserID *chain, USERID_t *id1, USERID_t *id2);
void Steam_NotifyClientDisconnect(IRehldsHook_Steam_NotifyClientDisconnect *chain, IGameClient *cl);
bool PreProcessPacket(IRehldsHook_PreprocessPacket *chain, uint8 *data, unsigned int len, const netadr_t &adr);
bool ValidateCommand(IRehldsHook_ValidateCommand *chain, const char *cmd, cmd_source_t source, IGameClient *cl);
void ExecuteServerStringCmd(IRehldsHook_ExecuteServerStringCmd *chain, const char *cmd, cmd_source_t source, IGameClient *cl);
void ClientConnected(IRehldsHook_ClientConnected *chain, IGameClient *cl);
void HandleNetCommand(IRehldsHook_HandleNetCommand *chain, IGameClient *cl, int8 cmd);
void Mod_LoadBrushModel(IRehldsHook_Mod_LoadBrushModel *chain, model_t *mod, void *buffer);
void Mod_LoadStudioModel(IRehldsHook_Mod_LoadStudioModel *chain, model_t *mod, void *buffer);
void SV_EmitEvents(IRehldsHook_SV_EmitEvents *chain, IGameClient *cl, struct packet_entities_s *pack, sizebuf_t *msg);
void EV_PlayReliableEvent(IRehldsHook_EV_PlayReliableEvent *chain, IGameClient *cl, int entindex, unsigned short eventindex, float delay, struct event_args_s *pargs);
void SV_StartSound(IRehldsHook_SV_StartSound *chain, int recipients, edict_t *entity, int channel, const char *sample, int volume, float attenuation, int flags, int pitch);
void PF_Remove_I(IRehldsHook_PF_Remove_I *chain, edict_t *edict);
void PF_BuildSoundMsg_I(IRehldsHook_PF_BuildSoundMsg_I *chain, edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed);
void SV_WriteFullClientUpdate(IRehldsHook_SV_WriteFullClientUpdate *chain, IGameClient *cl, char *buf, size_t len, sizebuf_t *sb, IGameClient *receiver);
bool SV_CheckConsistencyResponse(IRehldsHook_SV_CheckConsistencyResponse *chain, IGameClient *cl, resource_t *res, uint32 check);
void SV_DropClient(IRehldsHook_SV_DropClient *chain, IGameClient *cl, bool crash, const char *fmt);
void SV_ActivateServer(IRehldsHook_SV_ActivateServer *chain, int maxclients);
void SV_WriteVoiceCodec(IRehldsHook_SV_WriteVoiceCodec *chain, sizebuf_t *sb);
uint64 Steam_GSGetSteamID(IRehldsHook_Steam_GSGetSteamID *chain);
int SV_TransferConsistencyInfo(IRehldsHook_SV_TransferConsistencyInfo *chain);
bool Steam_GSBUpdateUserData(IRehldsHook_Steam_GSBUpdateUserData *chain, uint64 steamID, const char *pchPlayerName, uint32 uScore);
void Cvar_DirectSet(IRehldsHook_Cvar_DirectSet *chain, struct cvar_s *var, const char *value);
void SV_EstablishTimeBase(IRehldsHook_SV_EstablishTimeBase *chain, IGameClient *cl, struct usercmd_s *cmd, int funcs, int movevar, int vec);
void SV_Spawn_f(IRehldsHook_SV_Spawn_f *chain);
int SV_CreatePacketEntities(IRehldsHook_SV_CreatePacketEntities *chain, enum sv_delta_s delta, IGameClient *cl, struct packet_entities_s *pack, struct sizebuf_s *msg);
bool SV_EmitSound2(IRehldsHook_SV_EmitSound2 *chain, edict_t *entity, IGameClient *receiver, int channel, const char *sample, float volume, float attenuation, int flags, int pitch, int emitFlags, const float *pOrigin);
edict_t *CreateFakeClient(IRehldsHook_CreateFakeClient *chain, const char *netname);
bool SV_CheckConnectionLessRateLimits(IRehldsHook_SV_CheckConnectionLessRateLimits *chain, netadr_t &adr, const uint8_t *data, int len);
void SV_Frame(IRehldsHook_SV_Frame *chain);
bool SV_ShouldSendConsistencyList(IRehldsHook_SV_ShouldSendConsistencyList *chain, IGameClient *cl, bool bSend);
ENTITYINIT GetEntityInit(IRehldsHook_GetEntityInit *chain, char *pszClassName);
void SV_EmitPings(IRehldsHook_SV_EmitPings *chain, IGameClient *cl, sizebuf_t *msg);
edict_t *ED_Alloc(IRehldsHook_ED_Alloc *chain);
void ED_Free(IRehldsHook_ED_Free *chain, edict_t *ed);
void Con_Printf(IRehldsHook_Con_Printf *chain, const char *fmt);
int SV_CheckUserInfo(IRehldsHook_SV_CheckUserInfo *chain, netadr_t *adr, char *infobuffer, qboolean nodedup, int duplicateIndex, char *szSteamID);
int PF_precache_generic_I(IRehldsHook_PF_precache_generic_I *chain, const char *s);
int PF_precache_model_I(IRehldsHook_PF_precache_model_I *chain, const char *s);
int PF_precache_sound_I(IRehldsHook_PF_precache_sound_I *chain, const char *s);
unsigned short EV_Precache(IRehldsHook_EV_Precache *chain, int type, const char *psz);
void SV_AddResource(IRehldsHook_SV_AddResource *chain, resourcetype_t type, const char *name, int size, unsigned char flags, int index);
void SV_ClientPrintf(IRehldsHook_SV_ClientPrintf *chain, const char *fmt);
bool SV_AllowPhysent(IRehldsHook_SV_AllowPhysent *chain, edict_t *pEdict, edict_t *pPhysent);
// void SV_SendResources(IRehldsHook_SV_SendResources *chain, sizebuf_t *msg);

#endif //_INCLUDE_AMXMODX_LUA_HEADER_H