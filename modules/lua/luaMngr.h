
#ifndef _INCLUDE_AMXMODX_LUA_HEADER_H
#define _INCLUDE_AMXMODX_LUA_HEADER_H

#define	MAX_PHYSENTS 600 		  // Must have room for all entities in the world.
#define MAX_MOVEENTS 64

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

bool SV_EmitSound2(IRehldsHook_SV_EmitSound2 *chain,
                    edict_t *entity,
                    IGameClient *receiver,
                    int channel,
                    const char *sample,
                    float volume,
                    float attenuation,
                    int flags,
                    int pitch,
                    int emitFlags,
                    const float *pOrigin);



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

typedef struct
{
	vec3_t	normal;
	float	dist;
} pmplane_t;

typedef struct pmtrace_s pmtrace_t;

struct pmtrace_s
{
	qboolean	allsolid;	      // if true, plane is not valid
	qboolean	startsolid;	      // if true, the initial point was in a solid area
	qboolean	inopen, inwater;  // End point is in empty space or in water
	float		fraction;		  // time completed, 1.0 = didn't hit anything
	vec3_t		endpos;			  // final position
	pmplane_t	plane;		      // surface normal at impact
	int			ent;			  // entity at impact
	vec3_t      deltavelocity;    // Change in player's velocity caused by impact.  
								  // Only run on server.
	int         hitgroup;
};


typedef struct physent_s
{
	char			name[32];             // Name of model, or "player" or "world".
	int				player;
	vec3_t			origin;               // Model's origin in world coordinates.
	struct model_s	*model;		          // only for bsp models
	struct model_s	*studiomodel;         // SOLID_BBOX, but studio clip intersections.
	vec3_t			mins, maxs;	          // only for non-bsp models
	int				info;		          // For client or server to use to identify (index into edicts or cl_entities)
	vec3_t			angles;               // rotated entities need this info for hull testing to work.

	int				solid;				  // Triggers and func_door type WATER brushes are SOLID_NOT
	int				skin;                 // BSP Contents for such things like fun_door water brushes.
	int				rendermode;			  // So we can ignore glass
	
	// Complex collision detection.
	float			frame;
	int				sequence;
	byte			controller[4];
	byte			blending[2];

	int				movetype;
	int				takedamage;
	int				blooddecal;
	int				team;
	int				classnumber;

	// For mods
	int				iuser1;
	int				iuser2;
	int				iuser3;
	int				iuser4;
	float			fuser1;
	float			fuser2;
	float			fuser3;
	float			fuser4;
	vec3_t			vuser1;
	vec3_t			vuser2;
	vec3_t			vuser3;
	vec3_t			vuser4;
} physent_t;


struct playermove_s
{
	int				player_index;  // So we don't try to run the PM_CheckStuck nudging too quickly.
	qboolean		server;        // For debugging, are we running physics code on server side?

	qboolean		multiplayer;   // 1 == multiplayer server
	float			time;          // realtime on host, for reckoning duck timing
	float			frametime;	   // Duration of this frame

	vec3_t			forward, right, up; // Vectors for angles
	// player state
	vec3_t			origin;        // Movement origin.
	vec3_t			angles;        // Movement view angles.
	vec3_t			oldangles;     // Angles before movement view angles were looked at.
	vec3_t			velocity;      // Current movement direction.
	vec3_t			movedir;       // For waterjumping, a forced forward velocity so we can fly over lip of ledge.
	vec3_t			basevelocity;  // Velocity of the conveyor we are standing, e.g.
	
	// For ducking/dead
	vec3_t			view_ofs;      // Our eye position.
	float			flDuckTime;    // Time we started duck
	qboolean		bInDuck;       // In process of ducking or ducked already?
	
	// For walking/falling
	int				flTimeStepSound;  // Next time we can play a step sound
	int				iStepLeft;

	float			flFallVelocity;
	vec3_t			punchangle;

	float			flSwimTime;

	float			flNextPrimaryAttack;

	int				effects;		// MUZZLE FLASH, e.g.

	int				flags;         // FL_ONGROUND, FL_DUCKING, etc.
	int				usehull;       // 0 = regular player hull, 1 = ducked player hull, 2 = point hull
	float			gravity;       // Our current gravity and friction.
	float			friction;
	int				oldbuttons;    // Buttons last usercmd
	float			waterjumptime; // Amount of time left in jumping out of water cycle.
	qboolean		dead;          // Are we a dead player?
	int				deadflag;
	int				spectator;     // Should we use spectator physics model?
	int				movetype;      // Our movement type, NOCLIP, WALK, FLY

	int				onground;
	int				waterlevel;
	int				watertype;
	int				oldwaterlevel;

	char			sztexturename[256];
	char			chtexturetype;

	float			maxspeed;
	float			clientmaxspeed; // Player specific maxspeed

	// For mods
	int				iuser1;
	int				iuser2;
	int				iuser3;
	int				iuser4;
	float			fuser1;
	float			fuser2;
	float			fuser3;
	float			fuser4;
	vec3_t			vuser1;
	vec3_t			vuser2;
	vec3_t			vuser3;
	vec3_t			vuser4;
	// world state
	// Number of entities to clip against.
	int				numphysent;    
	physent_t		physents[MAX_PHYSENTS];
	// Number of momvement entities (ladders)
	int				nummoveent;
	// just a list of ladders
	physent_t		moveents[MAX_MOVEENTS];	

	// All things being rendered, for tracing against things you don't actually collide with
	int				numvisent;
	physent_t		visents[ MAX_PHYSENTS ];

	// input to run through physics.
	usercmd_t		cmd;

	// Trace results for objects we collided with.
	int				numtouch;
	pmtrace_t		touchindex[MAX_PHYSENTS];

	char			physinfo[ MAX_PHYSINFO_STRING ]; // Physics info string

	struct movevars_s *movevars;
	vec3_t player_mins[4];
	vec3_t player_maxs[4];
	
	// Common functions
	const char		*(*PM_Info_ValueForKey) ( const char *s, const char *key );
	void			(*PM_Particle)( vec3_t origin, int color, float life, int zpos, int zvel);
	int				(*PM_TestPlayerPosition) (vec3_t pos, pmtrace_t *ptrace );
	void			(*Con_NPrintf)( int idx, char *fmt, ... );
	void			(*Con_DPrintf)( char *fmt, ... );
	void			(*Con_Printf)( char *fmt, ... );
	double			(*Sys_FloatTime)( void );
	void			(*PM_StuckTouch)( int hitent, pmtrace_t *ptraceresult );
	int				(*PM_PointContents) (vec3_t p, int *truecontents /*filled in if this is non-null*/ );
	int				(*PM_TruePointContents) (vec3_t p);
	int				(*PM_HullPointContents) ( struct hull_s *hull, int num, vec3_t p);   
	pmtrace_t		(*PM_PlayerTrace) (vec3_t start, vec3_t end, int traceFlags, int ignore_pe );
	struct pmtrace_s *(*PM_TraceLine)( float *start, float *end, int flags, int usehulll, int ignore_pe );
	int32			(*RandomLong)( int32 lLow, int32 lHigh );
	float			(*RandomFloat)( float flLow, float flHigh );
	int				(*PM_GetModelType)( struct model_s *mod );
	void			(*PM_GetModelBounds)( struct model_s *mod, vec3_t mins, vec3_t maxs );
	void			*(*PM_HullForBsp)( physent_t *pe, vec3_t offset );
	float			(*PM_TraceModel)( physent_t *pEnt, vec3_t start, vec3_t end, trace_t *trace );
	int				(*COM_FileSize)(char *filename);
	byte			*(*COM_LoadFile) (char *path, int usehunk, int *pLength);
	void			(*COM_FreeFile) ( void *buffer );
	char			*(*memfgets)( byte *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize );

	// Functions
	// Run functions for this frame?
	qboolean		runfuncs;      
	void			(*PM_PlaySound) ( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch );
	const char		*(*PM_TraceTexture) ( int ground, vec3_t vstart, vec3_t vend );
	void			(*PM_PlaybackEventFull) ( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
};

#endif //_INCLUDE_AMXMODX_LUA_HEADER_H