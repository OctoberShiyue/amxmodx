
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

#endif //_INCLUDE_AMXMODX_LUA_HEADER_H