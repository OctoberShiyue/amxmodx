
#ifndef _INCLUDE_AMXMODX_LUA_HEADER_H
#define _INCLUDE_AMXMODX_LUA_HEADER_H

#include <main.h>

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

edict_t *ED_Alloc(IRehldsHook_ED_Alloc *chain);
void ED_Free(IRehldsHook_ED_Free *chain, edict_t *ed);



enum EntityState
{
	// Fields which are filled in by routines outside of delta compression
	ES_EntityType,
	// Index into cl_entities array for this entity
	ES_Number,
	ES_MsgTime,

	// Message number last time the player/entity state was updated
	ES_MessageNum,

	// Fields which can be transitted and reconstructed over the network stream
	ES_Origin,
	ES_Angles,

	ES_ModelIndex,
	ES_Sequence,
	ES_Frame,
	ES_ColorMap,
	ES_Skin,
	ES_Solid,
	ES_Effects,
	ES_Scale,
	ES_eFlags,

	// Render information
	ES_RenderMode,
	ES_RenderAmt,
	ES_RenderColor,
	ES_RenderFx,

	ES_MoveType,
	ES_AnimTime,
	ES_FrameRate,
	ES_Body,
	ES_Controller,
	ES_Blending,
	ES_Velocity,

	// Send bbox down to client for use during prediction
	ES_Mins,
	ES_Maxs,

	ES_AimEnt,
	// If owned by a player, the index of that player (for projectiles)
	ES_Owner,

	// Friction, for prediction
	ES_Friction,
	// Gravity multiplier
	ES_Gravity,

	// PLAYER SPECIFIC
	ES_Team,
	ES_PlayerClass,
	ES_Health,
	ES_Spectator,
	ES_WeaponModel,
	ES_GaitSequence,
	// If standing on conveyor, e.g.
	ES_BaseVelocity,
	// Use the crouched hull, or the regular player hull
	ES_UseHull,
	// Latched buttons last time state updated
	ES_OldButtons,
	// -1 = in air, else pmove entity number
	ES_OnGround,
	ES_iStepLeft,
	// How fast we are falling
	ES_flFallVelocity,

	ES_FOV,
	ES_WeaponAnim,

	// Parametric movement overrides
	ES_StartPos,
	ES_EndPos,
	ES_ImpactTime,
	ES_StartTime,

	// For mods
	ES_iUser1,
	ES_iUser2,
	ES_iUser3,
	ES_iUser4,
	ES_fUser1,
	ES_fUser2,
	ES_fUser3,
	ES_fUser4,
	ES_vUser1,
	ES_vUser2,
	ES_vUser3,
	ES_vUser4
};

#endif //_INCLUDE_AMXMODX_LUA_HEADER_H