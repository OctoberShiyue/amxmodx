// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// Fakemeta Module
//

#include "fakemeta_amxx.h"
#include "sh_stack.h"

TraceResult g_tr_2;

KVD_Wrapper g_kvd_glb;
KVD_Wrapper g_kvd_ext;

ke::Vector<KVD_Wrapper *>g_KVDWs;
ke::Vector<KVD_Wrapper *>g_FreeKVDWs;

clientdata_t g_cd_glb;
entity_state_t g_es_glb;
usercmd_t g_uc_glb;

static cell AMX_NATIVE_CALL set_tr2(AMX *amx, cell *params)
{
	TraceResult *tr;
	if (params[1] == 0)
		tr = &g_tr_2;
	else
		tr = reinterpret_cast<TraceResult *>(params[1]);

	if (*params / sizeof(cell) < 3)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "No data passed");
		return 0;
	}

	cell *ptr = MF_GetAmxAddr(amx, params[3]);

	switch (params[2])
	{
	case TR_AllSolid:
		{
			tr->fAllSolid = *ptr;
			return 1;
			break;
		}
	case TR_InOpen:
		{
			tr->fInOpen = *ptr;
			return 1;
			break;
		}
	case TR_StartSolid:
		{
			tr->fStartSolid = *ptr;
			return 1;
			break;
		}
	case TR_InWater:
		{
			tr->fInWater = *ptr;
			return 1;
			break;
		}
	case TR_flFraction:
		{
			tr->flFraction = amx_ctof(*ptr);
			return 1;
			break;
		}
	case TR_vecEndPos:
		{
			tr->vecEndPos.x = amx_ctof(ptr[0]);
			tr->vecEndPos.y = amx_ctof(ptr[1]);
			tr->vecEndPos.z = amx_ctof(ptr[2]);
			return 1;
			break;
		}
	case TR_flPlaneDist:
		{
			tr->flPlaneDist = amx_ctof(*ptr);
			return 1;
			break;
		}
	case TR_vecPlaneNormal:
		{
			tr->vecPlaneNormal.x = amx_ctof(ptr[0]);
			tr->vecPlaneNormal.y = amx_ctof(ptr[1]);
			tr->vecPlaneNormal.z = amx_ctof(ptr[2]);
			return 1;
			break;
		}
	case TR_pHit:
		{
			const auto pEdict = TypeConversion.id_to_edict(*ptr);
			if (pEdict == nullptr)
			{
				return 0;
			}
			tr->pHit = pEdict;
			return 1;
		}
	case TR_iHitgroup:
		{
			tr->iHitgroup = *ptr;
			return 1;
			break;
		}
	default:
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "Unknown TraceResult member %d", params[2]);
			return 0;
		}
	}

	return 0;
}

static cell AMX_NATIVE_CALL get_tr2(AMX *amx, cell *params)
{
	TraceResult *tr;
	if (params[1] == 0)
		tr = &g_tr_2;
	else
		tr = reinterpret_cast<TraceResult *>(params[1]);

	cell *ptr;

	switch (params[2])
	{
	case TR_AllSolid:
		{
			return tr->fAllSolid;
			break;
		}
	case TR_InOpen:
		{
			return tr->fInOpen;
			break;
		}
	case TR_StartSolid:
		{
			return tr->fStartSolid;
			break;
		}
	case TR_InWater:
		{
			return tr->fInWater;
			break;
		}
	case TR_flFraction:
		{
			ptr = MF_GetAmxAddr(amx, params[3]);
			*ptr = amx_ftoc(tr->flFraction);
			return 1;
			break;
		}
	case TR_vecEndPos:
		{
			ptr = MF_GetAmxAddr(amx, params[3]);
			ptr[0] = amx_ftoc(tr->vecEndPos.x);
			ptr[1] = amx_ftoc(tr->vecEndPos.y);
			ptr[2] = amx_ftoc(tr->vecEndPos.z);
			return 1;
			break;
		}
	case TR_flPlaneDist:
		{
			ptr = MF_GetAmxAddr(amx, params[3]);
			*ptr = amx_ftoc(tr->flPlaneDist);
			return 1;
			break;
		}
	case TR_vecPlaneNormal:
		{
			ptr = MF_GetAmxAddr(amx, params[3]);
			ptr[0] = amx_ftoc(tr->vecPlaneNormal.x);
			ptr[1] = amx_ftoc(tr->vecPlaneNormal.y);
			ptr[2] = amx_ftoc(tr->vecPlaneNormal.z);
			return 1;
			break;
		}
	case TR_pHit:
		{
			if (FNullEnt(tr->pHit))
				return -1;
			return ENTINDEX(tr->pHit);
			break;
		}
	case TR_iHitgroup:
		{
			return tr->iHitgroup;
			break;
		}
	default:
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "Unknown TraceResult member %d", params[2]);
			return 0;
		}
	}

	return 0;
}

static cell AMX_NATIVE_CALL get_kvd(AMX *amx, cell *params)
{
	KeyValueData *kvd;
	if (params[1] == 0)
		kvd = &(g_kvd_glb.kvd);
	else
		kvd = reinterpret_cast<KeyValueData *>(params[1]);

	switch (params[2])
	{
	case KV_fHandled:
		{
			return kvd->fHandled;
			break;
		}
	case KV_ClassName:
		{
			if (params[0] / sizeof(cell) != 4)
			{
				MF_LogError(amx, AMX_ERR_NATIVE, "Invalid number of parameters passed");
				return 0;
			}
			cell *ptr = MF_GetAmxAddr(amx, params[4]);
			return MF_SetAmxString(amx, params[3], kvd->szClassName, (int)*ptr);
			break;
		}
	case KV_KeyName:
		{
			if (params[0] / sizeof(cell) != 4)
			{
				MF_LogError(amx, AMX_ERR_NATIVE, "Invalid number of parameters passed");
				return 0;
			}
			cell *ptr = MF_GetAmxAddr(amx, params[4]);
			return MF_SetAmxString(amx, params[3], kvd->szKeyName, (int)*ptr);
			break;
		}
	case KV_Value:
		{
			if (params[0] / sizeof(cell) != 4)
			{
				MF_LogError(amx, AMX_ERR_NATIVE, "Invalid number of parameters passed");
				return 0;
			}
			cell *ptr = MF_GetAmxAddr(amx, params[4]);
			return MF_SetAmxString(amx, params[3], kvd->szValue, (int)*ptr);
			break;
		}
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid KeyValueData member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL set_kvd(AMX *amx, cell *params)
{
	KVD_Wrapper *kvdw = nullptr;
	KeyValueData *kvd = nullptr;
	
	KVD_Wrapper *tmpw = reinterpret_cast<KVD_Wrapper *>(params[1]);
	if (params[1] == 0 || tmpw == &g_kvd_glb) {
		kvdw = &g_kvd_glb;
		kvd = &(kvdw->kvd);
	} else {
		for (size_t i = 0; i < g_KVDWs.length(); ++i) {
			if (g_KVDWs[i] == tmpw) {
				kvdw = tmpw;
				kvd = &(kvdw->kvd);
			}
		}

		if (kvdw == nullptr) {
			kvdw = &g_kvd_ext;
			kvd = reinterpret_cast<KeyValueData *>(tmpw);
		}
	}

	if (*params / sizeof(cell) < 3)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "No data passed");
		return 0;
	}

	cell *ptr = MF_GetAmxAddr(amx, params[3]);
	int len;

	switch (params[2])
	{
	case KV_fHandled:
		{
			kvd->fHandled = (int)*ptr;
			return 1;
			break;
		}
	case KV_ClassName:
		{
			kvdw->cls = MF_GetAmxString(amx, params[3], 0, &len);
			kvd->szClassName = const_cast<char *>(kvdw->cls.chars());
			return 1;
			break;
		}
	case KV_KeyName:
		{
			kvdw->key = MF_GetAmxString(amx, params[3], 0, &len);
			kvd->szKeyName = const_cast<char *>(kvdw->key.chars());
			return 1;
			break;
		}
	case KV_Value:
		{
			kvdw->val = MF_GetAmxString(amx, params[3], 0, &len);
			kvd->szValue = const_cast<char *>(kvdw->val.chars());
			return 1;
			break;
		}
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid KeyValueData member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL get_cd(AMX *amx, cell *params)
{
	clientdata_s *cd;
	if (params[1] == 0)
		cd = &g_cd_glb;
	else
		cd = reinterpret_cast<clientdata_t *>(params[1]);

	cell *ptr;

	switch(params[2])
	{
	case CD_Origin:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->origin.x);
		ptr[1] = amx_ftoc(cd->origin.y);
		ptr[2] = amx_ftoc(cd->origin.z);
		return 1;
	case CD_Velocity:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->velocity.x);
		ptr[1] = amx_ftoc(cd->velocity.y);
		ptr[2] = amx_ftoc(cd->velocity.z);
		return 1;
	case CD_ViewModel:
		return cd->viewmodel;
	case CD_PunchAngle:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->punchangle.x);
		ptr[1] = amx_ftoc(cd->punchangle.y);
		ptr[2] = amx_ftoc(cd->punchangle.z);
		return 1;
	case CD_Flags:
		return cd->flags;
	case CD_WaterLevel:
		return cd->waterlevel;
	case CD_WaterType:
		return cd->watertype;
	case CD_ViewOfs:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->view_ofs.x);
		ptr[1] = amx_ftoc(cd->view_ofs.y);
		ptr[2] = amx_ftoc(cd->view_ofs.z);
		return 1;
	case CD_Health:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->health);
		return 1;
	case CD_bInDuck:
		return cd->bInDuck;
	case CD_Weapons:
		return cd->weapons;
	case CD_flTimeStepSound:
		return cd->flTimeStepSound;
	case CD_flDuckTime:
		return cd->flDuckTime;
	case CD_flSwimTime:
		return cd->flSwimTime;
	case CD_WaterJumpTime:
		return cd->waterjumptime;
	case CD_MaxSpeed:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->maxspeed);
		return 1;
	case CD_FOV:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->fov);
		return 1;
	case CD_WeaponAnim:
		return cd->weaponanim;
	case CD_ID:
		return cd->m_iId;
	case CD_AmmoShells:
		return cd->ammo_shells;
	case CD_AmmoNails:
		return cd->ammo_nails;
	case CD_AmmoCells:
		return cd->ammo_cells;
	case CD_AmmoRockets:
		return cd->ammo_rockets;
	case CD_flNextAttack:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->m_flNextAttack);
		return 1;
	case CD_tfState:
		return cd->tfstate;
	case CD_PushMsec:
		return cd->pushmsec;
	case CD_DeadFlag:
		return cd->deadflag;
	case CD_PhysInfo:
		ptr = MF_GetAmxAddr(amx, params[4]);
		return MF_SetAmxString(amx, params[3], cd->physinfo, (int)*ptr);
	case CD_iUser1:
		return cd->iuser1;
	case CD_iUser2:
		return cd->iuser2;
	case CD_iUser3:
		return cd->iuser3;
	case CD_iUser4:
		return cd->iuser4;
	case CD_fUser1:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->fuser1);
		return 1;
	case CD_fUser2:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->fuser2);
		return 1;
	case CD_fUser3:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->fuser3);
		return 1;
	case CD_fUser4:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(cd->fuser4);
		return 1;
	case CD_vUser1:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->vuser1.x);
		ptr[1] = amx_ftoc(cd->vuser1.y);
		ptr[2] = amx_ftoc(cd->vuser1.z);
		return 1;
	case CD_vUser2:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->vuser2.x);
		ptr[1] = amx_ftoc(cd->vuser2.y);
		ptr[2] = amx_ftoc(cd->vuser2.z);
		return 1;
	case CD_vUser3:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->vuser3.x);
		ptr[1] = amx_ftoc(cd->vuser3.y);
		ptr[2] = amx_ftoc(cd->vuser3.z);
		return 1;
	case CD_vUser4:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(cd->vuser4.x);
		ptr[1] = amx_ftoc(cd->vuser4.y);
		ptr[2] = amx_ftoc(cd->vuser4.z);
		return 1;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid ClientData member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL set_cd(AMX *amx, cell *params)
{
	if (*params / sizeof(cell) < 3)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "No data passed");
		return 0;
	}

	clientdata_s *cd;
	if (params[1] == 0)
		cd = &g_cd_glb;
	else
		cd = reinterpret_cast<clientdata_t *>(params[1]);

	cell *ptr = MF_GetAmxAddr(amx, params[3]);
	char *phys;

	switch(params[2])
	{
	case CD_Origin:
		cd->origin.x = amx_ctof(ptr[0]);
		cd->origin.y = amx_ctof(ptr[1]);
		cd->origin.z = amx_ctof(ptr[2]);
		return 1;
	case CD_Velocity:
		cd->velocity.x = amx_ctof(ptr[0]);
		cd->velocity.y = amx_ctof(ptr[1]);
		cd->velocity.z = amx_ctof(ptr[2]);
		return 1;
	case CD_ViewModel:
		cd->viewmodel = *ptr;
		return 1;
	case CD_PunchAngle:
		cd->punchangle.x = amx_ctof(ptr[0]);
		cd->punchangle.y = amx_ctof(ptr[1]);
		cd->punchangle.z = amx_ctof(ptr[2]);
		return 1;
	case CD_Flags:
		cd->flags = *ptr;
		return 1;
	case CD_WaterLevel:
		cd->waterlevel = *ptr;
		return 1;
	case CD_WaterType:
		cd->watertype = *ptr;
		return 1;
	case CD_ViewOfs:
		cd->view_ofs.x = amx_ctof(ptr[0]);
		cd->view_ofs.y = amx_ctof(ptr[1]);
		cd->view_ofs.z = amx_ctof(ptr[2]);
		return 1;
	case CD_Health:
		cd->health = amx_ctof(*ptr);
		return 1;
	case CD_bInDuck:
		cd->bInDuck = *ptr;
		return 1;
	case CD_Weapons:
		cd->weapons = *ptr;
		return 1;
	case CD_flTimeStepSound:
		cd->flTimeStepSound = *ptr;
		return 1;
	case CD_flDuckTime:
		cd->flDuckTime = *ptr;
		return 1;
	case CD_flSwimTime:
		cd->flSwimTime = *ptr;
		return 1;
	case CD_WaterJumpTime:
		cd->waterjumptime = *ptr;
		return 1;
	case CD_MaxSpeed:
		cd->maxspeed = amx_ctof(*ptr);
		return 1;
	case CD_FOV:
		cd->fov = amx_ctof(*ptr);
		return 1;
	case CD_WeaponAnim:
		cd->weaponanim = *ptr;
		return 1;
	case CD_ID:
		cd->m_iId = *ptr;
		return 1;
	case CD_AmmoShells:
		cd->ammo_shells = *ptr;
		return 1;
	case CD_AmmoNails:
		cd->ammo_nails = *ptr;
		return 1;
	case CD_AmmoCells:
		cd->ammo_cells = *ptr;
		return 1;
	case CD_AmmoRockets:
		cd->ammo_rockets = *ptr;
		return 1;
	case CD_flNextAttack:
		cd->m_flNextAttack = amx_ctof(*ptr);
		return 1;
	case CD_tfState:
		cd->tfstate = *ptr;
		return 1;
	case CD_PushMsec:
		cd->pushmsec = *ptr;
		return 1;
	case CD_DeadFlag:
		cd->deadflag = *ptr;
		return 1;
	case CD_PhysInfo:
		int len;
		phys = MF_GetAmxString(amx, params[3], 0, &len);
		strncpy(cd->physinfo, phys, len);
		return 1;
	case CD_iUser1:
		cd->iuser1 = *ptr;
		return 1;
	case CD_iUser2:
		cd->iuser2 = *ptr;
		return 1;
	case CD_iUser3:
		cd->iuser3 = *ptr;
		return 1;
	case CD_iUser4:
		cd->iuser4 = *ptr;
		return 1;
	case CD_fUser1:
		cd->fuser1 = amx_ctof(*ptr);
		return 1;
	case CD_fUser2:
		cd->fuser2 = amx_ctof(*ptr);
		return 1;
	case CD_fUser3:
		cd->fuser3 = amx_ctof(*ptr);
		return 1;
	case CD_fUser4:
		cd->fuser4 = amx_ctof(*ptr);
		return 1;
	case CD_vUser1:
		cd->vuser1.x = amx_ctof(ptr[0]);
		cd->vuser1.y = amx_ctof(ptr[1]);
		cd->vuser1.z = amx_ctof(ptr[2]);
		return 1;
	case CD_vUser2:
		cd->vuser2.x = amx_ctof(ptr[0]);
		cd->vuser2.y = amx_ctof(ptr[1]);
		cd->vuser2.z = amx_ctof(ptr[2]);
		return 1;
	case CD_vUser3:
		cd->vuser3.x = amx_ctof(ptr[0]);
		cd->vuser3.y = amx_ctof(ptr[1]);
		cd->vuser3.z = amx_ctof(ptr[2]);
		return 1;
	case CD_vUser4:
		cd->vuser4.x = amx_ctof(ptr[0]);
		cd->vuser4.y = amx_ctof(ptr[1]);
		cd->vuser4.z = amx_ctof(ptr[2]);
		return 1;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid ClientData member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL get_es(AMX *amx, cell *params)
{
	entity_state_t *es;
	if (params[1] == 0)
		es = &g_es_glb;
	else
		es = reinterpret_cast<entity_state_t *>(params[1]);

	cell *ptr;

	switch(params[2])
	{
	case ES_EntityType:
		return es->entityType;
	case ES_Number:
		return es->number;
	case ES_MsgTime:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->msg_time);
		return 1;
	case ES_MessageNum:
		return es->messagenum;
	case ES_Origin:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->origin.x);
		ptr[1] = amx_ftoc(es->origin.y);
		ptr[2] = amx_ftoc(es->origin.z);
		return 1;
	case ES_Angles:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->angles.x);
		ptr[1] = amx_ftoc(es->angles.y);
		ptr[2] = amx_ftoc(es->angles.z);
		return 1;
	case ES_ModelIndex:
		return es->modelindex;
	case ES_Sequence:
		return es->sequence;
	case ES_Frame:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->frame);
		return 1;
	case ES_ColorMap:
		return es->colormap;
	case ES_Skin:
		return es->skin;
	case ES_Solid:
		return es->solid;
	case ES_Effects:
		return es->effects;
	case ES_Scale:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->scale);
		return 1;
	case ES_eFlags:
		return es->eflags;
	case ES_RenderMode:
		return es->rendermode;
	case ES_RenderAmt:
		return es->renderamt;
	case ES_RenderColor:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = es->rendercolor.r;
		ptr[1] = es->rendercolor.b;
		ptr[2] = es->rendercolor.g;
		return 1;
	case ES_RenderFx:
		return es->renderfx;
	case ES_MoveType:
		return es->movetype;
	case ES_AnimTime:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->animtime);
		return 1;
	case ES_FrameRate:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->framerate);
		return 1;
	case ES_Body:
		return es->body;
	case ES_Controller:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = es->controller[0];
		ptr[1] = es->controller[1];
		ptr[2] = es->controller[2];
		ptr[3] = es->controller[3];
		return 1;
	case ES_Blending:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = es->blending[0];
		ptr[1] = es->blending[1];
		ptr[2] = es->blending[2];
		ptr[3] = es->blending[3];
		return 1;
	case ES_Velocity:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->velocity.x);
		ptr[1] = amx_ftoc(es->velocity.y);
		ptr[2] = amx_ftoc(es->velocity.z);
		return 1;
	case ES_Mins:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->mins.x);
		ptr[1] = amx_ftoc(es->mins.y);
		ptr[2] = amx_ftoc(es->mins.z);
		return 1;
	case ES_Maxs:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->maxs.x);
		ptr[1] = amx_ftoc(es->maxs.y);
		ptr[2] = amx_ftoc(es->maxs.z);
		return 1;
	case ES_AimEnt:
		return es->aiment;
	case ES_Owner:
		return es->owner;
	case ES_Friction:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->friction);
		return 1;
	case ES_Gravity:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->gravity);
		return 1;
	case ES_Team:
		return es->team;
	case ES_PlayerClass:
		return es->playerclass;
	case ES_Health:
		return es->health;
	case ES_Spectator:
		return es->spectator;
	case ES_WeaponModel:
		return es->weaponmodel;
	case ES_GaitSequence:
		return es->gaitsequence;
	case ES_BaseVelocity:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->basevelocity.x);
		ptr[1] = amx_ftoc(es->basevelocity.y);
		ptr[2] = amx_ftoc(es->basevelocity.z);
		return 1;
	case ES_UseHull:
		return es->usehull;
	case ES_OldButtons:
		return es->oldbuttons;
	case ES_OnGround:
		return es->onground;
	case ES_iStepLeft:
		return es->iStepLeft;
	case ES_flFallVelocity:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->flFallVelocity);
		return 1;
	case ES_FOV:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->fov);
		return 1;
	case ES_WeaponAnim:
		return es->weaponanim;
	case ES_StartPos:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->startpos.x);
		ptr[1] = amx_ftoc(es->startpos.y);
		ptr[2] = amx_ftoc(es->startpos.z);
		return 1;
	case ES_EndPos:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->endpos.x);
		ptr[1] = amx_ftoc(es->endpos.y);
		ptr[2] = amx_ftoc(es->endpos.z);
		return 1;
	case ES_ImpactTime:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->impacttime);
		return 1;
	case ES_StartTime:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->starttime);
		return 1;
	case ES_iUser1:
		return es->iuser1;
	case ES_iUser2:
		return es->iuser2;
	case ES_iUser3:
		return es->iuser3;
	case ES_iUser4:
		return es->iuser4;
	case ES_fUser1:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->fuser1);
		return 1;
	case ES_fUser2:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->fuser2);
		return 1;
	case ES_fUser3:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->fuser3);
		return 1;
	case ES_fUser4:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(es->fuser4);
		return 1;
	case ES_vUser1:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->vuser1.x);
		ptr[1] = amx_ftoc(es->vuser1.y);
		ptr[2] = amx_ftoc(es->vuser1.z);
		return 1;
	case ES_vUser2:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->vuser2.x);
		ptr[1] = amx_ftoc(es->vuser2.y);
		ptr[2] = amx_ftoc(es->vuser2.z);
		return 1;
	case ES_vUser3:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->vuser3.x);
		ptr[1] = amx_ftoc(es->vuser3.y);
		ptr[2] = amx_ftoc(es->vuser3.z);
		return 1;
	case ES_vUser4:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(es->vuser4.x);
		ptr[1] = amx_ftoc(es->vuser4.y);
		ptr[2] = amx_ftoc(es->vuser4.z);
		return 1;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid EntityState member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL set_es(AMX *amx, cell *params)
{
	if (*params / sizeof(cell) < 3)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "No data passed");
		return 0;
	}

	entity_state_t *es;
	if (params[1] == 0)
		es = &g_es_glb;
	else
		es = reinterpret_cast<entity_state_t *>(params[1]);

	cell *ptr = MF_GetAmxAddr(amx, params[3]);

	switch(params[2])
	{
	case ES_EntityType:
		es->entityType = *ptr;
		return 1;
	case ES_Number:
		es->number = *ptr;
		return 1;
	case ES_MsgTime:
		es->msg_time = amx_ctof(*ptr);
		return 1;
	case ES_MessageNum:
		es->messagenum = *ptr;
		return 1;
	case ES_Origin:
		es->origin.x = amx_ctof(ptr[0]);
		es->origin.y = amx_ctof(ptr[1]);
		es->origin.z = amx_ctof(ptr[2]);
		return 1;
	case ES_Angles:
		es->angles.x = amx_ctof(ptr[0]);
		es->angles.y = amx_ctof(ptr[1]);
		es->angles.z = amx_ctof(ptr[2]);
		return 1;
	case ES_ModelIndex:
		es->modelindex = *ptr;
		return 1;
	case ES_Sequence:
		es->sequence = *ptr;
		return 1;
	case ES_Frame:
		es->frame = amx_ctof(*ptr);
		return 1;
	case ES_ColorMap:
		es->colormap = *ptr;
		return 1;
	case ES_Skin:
		es->skin = *ptr;
		return 1;
	case ES_Solid:
		es->solid = *ptr;
		return 1;
	case ES_Effects:
		es->effects = *ptr;
		return 1;
	case ES_Scale:
		es->scale = amx_ctof(*ptr);
		return 1;
	case ES_eFlags:
		es->eflags = *ptr;
		return 1;
	case ES_RenderMode:
		es->rendermode = *ptr;
		return 1;
	case ES_RenderAmt:
		es->renderamt = *ptr;
		return 1;
	case ES_RenderColor:
		es->rendercolor.r = ptr[0];
		es->rendercolor.g = ptr[1];
		es->rendercolor.b = ptr[2];
		return 1;
	case ES_RenderFx:
		es->renderfx = *ptr;
		return 1;
	case ES_MoveType:
		es->movetype = *ptr;
		return 1;
	case ES_AnimTime:
		es->animtime = amx_ctof(*ptr);
		return 1;
	case ES_FrameRate:
		es->framerate = amx_ctof(*ptr);
		return 1;
	case ES_Body:
		es->body = *ptr;
		return 1;
	case ES_Controller:
		es->controller[0] = ptr[0];
		es->controller[1] = ptr[1];
		es->controller[2] = ptr[2];
		es->controller[3] = ptr[3];
		return 1;
	case ES_Blending:
		es->blending[0] = ptr[0];
		es->blending[1] = ptr[1];
		es->blending[2] = ptr[2];
		es->blending[3] = ptr[3];
		return 1;
	case ES_Velocity:
		es->velocity.x = amx_ctof(ptr[0]);
		es->velocity.y = amx_ctof(ptr[1]);
		es->velocity.z = amx_ctof(ptr[2]);
		return 1;
	case ES_Mins:
		es->mins.x = amx_ctof(ptr[0]);
		es->mins.y = amx_ctof(ptr[1]);
		es->mins.z = amx_ctof(ptr[2]);
		return 1;
	case ES_Maxs:
		es->maxs.x = amx_ctof(ptr[0]);
		es->maxs.y = amx_ctof(ptr[1]);
		es->maxs.z = amx_ctof(ptr[2]);
		return 1;
	case ES_AimEnt:
		es->aiment = *ptr;
		return 1;
	case ES_Owner:
		es->owner = *ptr;
		return 1;
	case ES_Friction:
		es->friction = amx_ctof(*ptr);
		return 1;
	case ES_Gravity:
		es->gravity = amx_ctof(*ptr);
		return 1;
	case ES_Team:
		es->team = *ptr;
		return 1;
	case ES_PlayerClass:
		es->playerclass = *ptr;
		return 1;
	case ES_Health:
		es->health = *ptr;
		return 1;
	case ES_Spectator:
		es->spectator = *ptr;
		return 1;
	case ES_WeaponModel:
		es->weaponmodel = *ptr;
		return 1;
	case ES_GaitSequence:
		es->gaitsequence = *ptr;
		return 1;
	case ES_BaseVelocity:
		es->basevelocity.x = amx_ctof(ptr[0]);
		es->basevelocity.y = amx_ctof(ptr[1]);
		es->basevelocity.z = amx_ctof(ptr[2]);
		return 1;
	case ES_UseHull:
		es->usehull = *ptr;
		return 1;
	case ES_OldButtons:
		es->oldbuttons = *ptr;
		return 1;
	case ES_OnGround:
		es->onground = *ptr;
		return 1;
	case ES_iStepLeft:
		es->iStepLeft = *ptr;
		return 1;
	case ES_flFallVelocity:
		es->flFallVelocity = amx_ctof(*ptr);
		return 1;
	case ES_FOV:
		es->fov = amx_ctof(*ptr);
		return 1;
	case ES_WeaponAnim:
		es->weaponanim = *ptr;
		return 1;
	case ES_StartPos:
		es->startpos.x = amx_ctof(ptr[0]);
		es->startpos.y = amx_ctof(ptr[1]);
		es->startpos.z = amx_ctof(ptr[2]);
		return 1;
	case ES_EndPos:
		es->endpos.x = amx_ctof(ptr[0]);
		es->endpos.y = amx_ctof(ptr[1]);
		es->endpos.z = amx_ctof(ptr[2]);
		return 1;
	case ES_ImpactTime:
		es->impacttime= amx_ctof(*ptr);
		return 1;
	case ES_StartTime:
		es->starttime = amx_ctof(*ptr);
		return 1;
	case ES_iUser1:
		es->iuser1 = *ptr;
		return 1;
	case ES_iUser2:
		es->iuser2 = *ptr;
		return 1;
	case ES_iUser3:
		es->iuser3 = *ptr;
		return 1;
	case ES_iUser4:
		es->iuser4 = *ptr;
		return 1;
	case ES_fUser1:
		es->fuser1 = amx_ctof(*ptr);
		return 1;
	case ES_fUser2:
		es->fuser2 = amx_ctof(*ptr);
		return 1;
	case ES_fUser3:
		es->fuser3 = amx_ctof(*ptr);
		return 1;
	case ES_fUser4:
		es->fuser4 = amx_ctof(*ptr);
		return 1;
	case ES_vUser1:
		es->vuser1.x = amx_ctof(ptr[0]);
		es->vuser1.y = amx_ctof(ptr[1]);
		es->vuser1.z = amx_ctof(ptr[2]);
		return 1;
	case ES_vUser2:
		es->vuser2.x = amx_ctof(ptr[0]);
		es->vuser2.y = amx_ctof(ptr[1]);
		es->vuser2.z = amx_ctof(ptr[2]);
		return 1;
	case ES_vUser3:
		es->vuser3.x = amx_ctof(ptr[0]);
		es->vuser3.y = amx_ctof(ptr[1]);
		es->vuser3.z = amx_ctof(ptr[2]);
		return 1;
	case ES_vUser4:
		es->vuser3.x = amx_ctof(ptr[0]);
		es->vuser3.y = amx_ctof(ptr[1]);
		es->vuser3.z = amx_ctof(ptr[2]);
		return 1;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid EntityState member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL get_uc(AMX *amx, cell *params)
{
	usercmd_t *uc;
	if (params[1] == 0)
		uc = &g_uc_glb;
	else
		uc = reinterpret_cast<usercmd_t *>(params[1]);

	cell *ptr;

	switch(params[2])
	{
	case UC_LerpMsec:
		return uc->lerp_msec;
	case UC_Msec:
		return uc->msec;
	case UC_ViewAngles:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(uc->viewangles.x);
		ptr[1] = amx_ftoc(uc->viewangles.y);
		ptr[2] = amx_ftoc(uc->viewangles.z);
		return 1;
	case UC_ForwardMove:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(uc->forwardmove);
		return 1;
	case UC_SideMove:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(uc->sidemove);
		return 1;
	case UC_UpMove:
		ptr = MF_GetAmxAddr(amx, params[3]);
		*ptr = amx_ftoc(uc->upmove);
		return 1;
	case UC_LightLevel:
		return uc->lightlevel;
	case UC_Buttons:
		return uc->buttons;
	case UC_Impulse:
		return uc->impulse;
	case UC_WeaponSelect:
		return uc->weaponselect;
	case UC_ImpactIndex:
		return uc->impact_index;
	case UC_ImpactPosition:
		ptr = MF_GetAmxAddr(amx, params[3]);
		ptr[0] = amx_ftoc(uc->impact_position.x);
		ptr[1] = amx_ftoc(uc->impact_position.y);
		ptr[2] = amx_ftoc(uc->impact_position.z);
		return 1;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid UserCmd member: %d", params[2]);

	return 0;
}

static cell AMX_NATIVE_CALL set_uc(AMX *amx, cell *params)
{
	if (*params / sizeof(cell) < 3)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "No data passed");
		return 0;
	}

	usercmd_t *uc;
	if (params[1] == 0)
		uc = &g_uc_glb;
	else
		uc = reinterpret_cast<usercmd_t *>(params[1]);

	cell *ptr = MF_GetAmxAddr(amx, params[3]);

	switch(params[2])
	{
	case UC_LerpMsec:
		uc->lerp_msec = *ptr;
		return 1;
	case UC_Msec:
		uc->msec = *ptr;
		return 1;
	case UC_ViewAngles:
		uc->viewangles.x = amx_ctof(ptr[0]);
		uc->viewangles.y = amx_ctof(ptr[1]);
		uc->viewangles.z = amx_ctof(ptr[2]);
		return 1;
	case UC_ForwardMove:
		uc->forwardmove = amx_ctof(*ptr);
		return 1;
	case UC_SideMove:
		uc->sidemove = amx_ctof(*ptr);
		return 1;
	case UC_UpMove:
		uc->upmove = amx_ctof(*ptr);
		return 1;
	case UC_LightLevel:
		uc->lightlevel = *ptr;
		return 1;
	case UC_Buttons:
		uc->buttons = *ptr;
		return 1;
	case UC_Impulse:
		uc->impulse = *ptr;
		return 1;
	case UC_WeaponSelect:
		uc->weaponselect = *ptr;
		return 1;
	case UC_ImpactIndex:
		uc->impact_index = *ptr;
		return 1;
	case UC_ImpactPosition:
		uc->impact_position.x = amx_ctof(ptr[0]);
		uc->impact_position.y = amx_ctof(ptr[1]);
		uc->impact_position.z = amx_ctof(ptr[2]);
		return 1;
	}

	MF_LogError(amx, AMX_ERR_NATIVE, "Invalid UserCmd member: %d", params[2]);

	return 0;
}

CStack<TraceResult *> g_FreeTRs;

static cell AMX_NATIVE_CALL create_tr2(AMX *amx, cell *params)
{
	TraceResult *tr;
	if (g_FreeTRs.empty())
	{
		tr = new TraceResult;
	} else {
		tr = g_FreeTRs.front();
		g_FreeTRs.pop();
	}
	memset(static_cast<void *>(tr), 0, sizeof(TraceResult));
	return reinterpret_cast<cell>(tr);
}

static cell AMX_NATIVE_CALL free_tr2(AMX *amx, cell *params)
{
	TraceResult *tr = reinterpret_cast<TraceResult *>(params[1]);
	if (!tr)
	{
		return 0;
	}

	g_FreeTRs.push(tr);

	return 1;
}

static cell AMX_NATIVE_CALL create_kvd(AMX *amx, cell *params)
{
	KVD_Wrapper *kvdw;
	if (g_FreeKVDWs.empty()) {
		kvdw = new KVD_Wrapper;
	} else {
		kvdw = g_FreeKVDWs.popCopy();
	}

	kvdw->cls = "";
	kvdw->kvd.szClassName = const_cast<char*>(kvdw->cls.chars());
	kvdw->key = "";
	kvdw->kvd.szKeyName = const_cast<char*>(kvdw->key.chars());
	kvdw->val = "";
	kvdw->kvd.szValue = const_cast<char*>(kvdw->val.chars());
	kvdw->kvd.fHandled = 0;

	g_KVDWs.append(kvdw);

	return reinterpret_cast<cell>(kvdw);
}

static cell AMX_NATIVE_CALL free_kvd(AMX *amx, cell *params) {
	if (params[1] == 0) {
		return 0;
	}

	KVD_Wrapper *kvdw = reinterpret_cast<KVD_Wrapper *>(params[1]);

	for (size_t i = 0; i < g_KVDWs.length(); ++i) {
		if (g_KVDWs[i] == kvdw) {
			g_KVDWs.remove(i);
			g_FreeKVDWs.append(kvdw);

			return 1;
		}
	}

	return 0;
}

AMX_NATIVE_INFO ext2_natives[] = 
{
	{"create_tr2L",		create_tr2},
	{"free_tr2L",		free_tr2},
	{"get_tr2L",			get_tr2},
	{"set_tr2L",			set_tr2},
	{"create_kvdL",		create_kvd},
	{"free_kvdL",		free_kvd},
	{"get_kvdL",			get_kvd},
	{"set_kvdL",			set_kvd},
	{"get_cdL",			get_cd},
	{"set_cdL",			set_cd},
	{"get_esL",			get_es},
	{"set_esL",			set_es},
	{"get_ucL",			get_uc},
	{"set_ucL",			set_uc},
	{"Lfakemetal_func_init_tr2",	amx_fakemetal_func_init_tr2},
	{NULL,				NULL},
};

static int L_fakemeta_create_tr2(lua_State* L)
{
	TraceResult *tr;
	if (g_FreeTRs.empty())
	{
		tr = new TraceResult;
	}
	else
	{
		tr = g_FreeTRs.front();
		g_FreeTRs.pop();
	}
	memset(static_cast<void*>(tr), 0, sizeof(TraceResult));
	lua_pushlightuserdata(L, tr);
	return 1;
}

static int L_fakemeta_free_tr2(lua_State* L)
{
	TraceResult *tr = static_cast<TraceResult*>(lua_touserdata(L, 1));
	if (!tr)
		return 0;
	g_FreeTRs.push(tr);
	return 1;
}

static int L_fakemeta_get_tr2(lua_State* L)
{
	TraceResult *tr;
	if (lua_isnil(L, 1))
		tr = &g_tr_2;
	else
		tr = static_cast<TraceResult*>(lua_touserdata(L, 1));

	if (!tr)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case TR_AllSolid:
		lua_pushinteger(L, tr->fAllSolid);
		break;
	case TR_StartSolid:
		lua_pushinteger(L, tr->fStartSolid);
		break;
	case TR_InOpen:
		lua_pushinteger(L, tr->fInOpen);
		break;
	case TR_InWater:
		lua_pushinteger(L, tr->fInWater);
		break;
	case TR_flFraction:
		lua_pushnumber(L, tr->flFraction);
		break;
	case TR_vecEndPos:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, tr->vecEndPos.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, tr->vecEndPos.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, tr->vecEndPos.z);
		lua_rawseti(L, -2, 3);
		break;
	case TR_flPlaneDist:
		lua_pushnumber(L, tr->flPlaneDist);
		break;
	case TR_vecPlaneNormal:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, tr->vecPlaneNormal.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, tr->vecPlaneNormal.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, tr->vecPlaneNormal.z);
		lua_rawseti(L, -2, 3);
		break;
	case TR_pHit:
		if (FNullEnt(tr->pHit))
			lua_pushinteger(L, -1);
		else
			lua_pushinteger(L, ENTINDEX(tr->pHit));
		break;
	case TR_iHitgroup:
		lua_pushinteger(L, tr->iHitgroup);
		break;
	default:
		return luaL_error(L, "Unknown TraceResult member: %d", member);
	}
	return 1;
}

static int L_fakemeta_set_tr2(lua_State* L)
{
	TraceResult *tr;
	if (lua_isnil(L, 1))
		tr = &g_tr_2;
	else
		tr = static_cast<TraceResult*>(lua_touserdata(L, 1));

	if (!tr)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case TR_AllSolid:
		tr->fAllSolid = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case TR_StartSolid:
		tr->fStartSolid = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case TR_InOpen:
		tr->fInOpen = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case TR_InWater:
		tr->fInWater = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case TR_flFraction:
		tr->flFraction = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case TR_vecEndPos:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		tr->vecEndPos.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		tr->vecEndPos.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		tr->vecEndPos.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case TR_flPlaneDist:
		tr->flPlaneDist = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case TR_vecPlaneNormal:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		tr->vecPlaneNormal.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		tr->vecPlaneNormal.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		tr->vecPlaneNormal.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case TR_pHit:
		{
			int entindex = static_cast<int>(luaL_checkinteger(L, 3));
			if (entindex <= 0)
				tr->pHit = NULL;
			else
				tr->pHit = INDEXENT(entindex);
		}
		break;
	case TR_iHitgroup:
		tr->iHitgroup = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	default:
		return luaL_error(L, "Unknown TraceResult member: %d", member);
	}
	return 1;
}

static int L_fakemeta_create_kvd(lua_State* L)
{
	KVD_Wrapper *kvdw;
	if (g_FreeKVDWs.empty())
	{
		kvdw = new KVD_Wrapper;
	}
	else
	{
		kvdw = g_FreeKVDWs.popCopy();
	}

	kvdw->cls = "";
	kvdw->kvd.szClassName = const_cast<char*>(kvdw->cls.chars());
	kvdw->key = "";
	kvdw->kvd.szKeyName = const_cast<char*>(kvdw->key.chars());
	kvdw->val = "";
	kvdw->kvd.szValue = const_cast<char*>(kvdw->val.chars());
	kvdw->kvd.fHandled = 0;

	g_KVDWs.append(kvdw);

	lua_pushlightuserdata(L, kvdw);
	return 1;
}

static int L_fakemeta_free_kvd(lua_State* L)
{
	KVD_Wrapper *kvdw = static_cast<KVD_Wrapper*>(lua_touserdata(L, 1));
	if (!kvdw)
		return 0;

	for (size_t i = 0; i < g_KVDWs.length(); ++i)
	{
		if (g_KVDWs[i] == kvdw)
		{
			g_KVDWs.remove(i);
			g_FreeKVDWs.append(kvdw);
			return 1;
		}
	}
	return 0;
}

static int L_fakemeta_get_kvd(lua_State* L)
{
	KeyValueData *kvd;
	if (lua_isnil(L, 1))
		kvd = &(g_kvd_glb.kvd);
	else
		kvd = &(static_cast<KVD_Wrapper*>(lua_touserdata(L, 1))->kvd);

	if (!kvd)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case KV_ClassName:
		lua_pushstring(L, kvd->szClassName ? kvd->szClassName : "");
		break;
	case KV_KeyName:
		lua_pushstring(L, kvd->szKeyName ? kvd->szKeyName : "");
		break;
	case KV_Value:
		lua_pushstring(L, kvd->szValue ? kvd->szValue : "");
		break;
	case KV_fHandled:
		lua_pushinteger(L, kvd->fHandled);
		break;
	default:
		return luaL_error(L, "Unknown KeyValueData member: %d", member);
	}
	return 1;
}

static int L_fakemeta_set_kvd(lua_State* L)
{
	KVD_Wrapper *kvdw = nullptr;
	KeyValueData *kvd = nullptr;

	if (lua_isnil(L, 1))
	{
		kvdw = &g_kvd_glb;
		kvd = &(kvdw->kvd);
	}
	else
	{
		kvdw = static_cast<KVD_Wrapper*>(lua_touserdata(L, 1));
		kvd = &(kvdw->kvd);
	}

	if (!kvd)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case KV_ClassName:
		kvdw->cls = luaL_checkstring(L, 3);
		kvd->szClassName = const_cast<char*>(kvdw->cls.chars());
		break;
	case KV_KeyName:
		kvdw->key = luaL_checkstring(L, 3);
		kvd->szKeyName = const_cast<char*>(kvdw->key.chars());
		break;
	case KV_Value:
		kvdw->val = luaL_checkstring(L, 3);
		kvd->szValue = const_cast<char*>(kvdw->val.chars());
		break;
	case KV_fHandled:
		kvd->fHandled = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	default:
		return luaL_error(L, "Unknown KeyValueData member: %d", member);
	}
	return 1;
}

static int L_fakemeta_get_cd(lua_State* L)
{
	clientdata_t *cd;
	if (lua_isnil(L, 1))
		cd = &g_cd_glb;
	else
		cd = reinterpret_cast<clientdata_t*>(lua_touserdata(L, 1));

	if (!cd)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case CD_Origin:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->origin.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->origin.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->origin.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_Velocity:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->velocity.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->velocity.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->velocity.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_ViewModel:
		lua_pushinteger(L, cd->viewmodel);
		break;
	case CD_PunchAngle:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->punchangle.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->punchangle.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->punchangle.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_Flags:
		lua_pushinteger(L, cd->flags);
		break;
	case CD_WaterLevel:
		lua_pushinteger(L, cd->waterlevel);
		break;
	case CD_WaterType:
		lua_pushinteger(L, cd->watertype);
		break;
	case CD_ViewOfs:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->view_ofs.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->view_ofs.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->view_ofs.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_Health:
		lua_pushnumber(L, cd->health);
		break;
	case CD_bInDuck:
		lua_pushinteger(L, cd->bInDuck);
		break;
	case CD_Weapons:
		lua_pushinteger(L, cd->weapons);
		break;
	case CD_flTimeStepSound:
		lua_pushinteger(L, cd->flTimeStepSound);
		break;
	case CD_flDuckTime:
		lua_pushinteger(L, cd->flDuckTime);
		break;
	case CD_flSwimTime:
		lua_pushinteger(L, cd->flSwimTime);
		break;
	case CD_WaterJumpTime:
		lua_pushnumber(L, cd->waterjumptime);
		break;
	case CD_MaxSpeed:
		lua_pushnumber(L, cd->maxspeed);
		break;
	case CD_FOV:
		lua_pushnumber(L, cd->fov);
		break;
	case CD_WeaponAnim:
		lua_pushinteger(L, cd->weaponanim);
		break;
	case CD_ID:
		lua_pushinteger(L, cd->m_iId);
		break;
	case CD_AmmoShells:
		lua_pushinteger(L, cd->ammo_shells);
		break;
	case CD_AmmoNails:
		lua_pushinteger(L, cd->ammo_nails);
		break;
	case CD_AmmoCells:
		lua_pushinteger(L, cd->ammo_cells);
		break;
	case CD_AmmoRockets:
		lua_pushinteger(L, cd->ammo_rockets);
		break;
	case CD_flNextAttack:
		lua_pushnumber(L, cd->m_flNextAttack);
		break;
	case CD_tfState:
		lua_pushinteger(L, cd->tfstate);
		break;
	case CD_PushMsec:
		lua_pushinteger(L, cd->pushmsec);
		break;
	case CD_DeadFlag:
		lua_pushinteger(L, cd->deadflag);
		break;
	case CD_PhysInfo:
		lua_pushstring(L, cd->physinfo);
		break;
	case CD_iUser1:
		lua_pushinteger(L, cd->iuser1);
		break;
	case CD_iUser2:
		lua_pushinteger(L, cd->iuser2);
		break;
	case CD_iUser3:
		lua_pushinteger(L, cd->iuser3);
		break;
	case CD_iUser4:
		lua_pushinteger(L, cd->iuser4);
		break;
	case CD_fUser1:
		lua_pushnumber(L, cd->fuser1);
		break;
	case CD_fUser2:
		lua_pushnumber(L, cd->fuser2);
		break;
	case CD_fUser3:
		lua_pushnumber(L, cd->fuser3);
		break;
	case CD_fUser4:
		lua_pushnumber(L, cd->fuser4);
		break;
	case CD_vUser1:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->vuser1.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->vuser1.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->vuser1.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_vUser2:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->vuser2.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->vuser2.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->vuser2.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_vUser3:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->vuser3.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->vuser3.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->vuser3.z);
		lua_rawseti(L, -2, 3);
		break;
	case CD_vUser4:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, cd->vuser4.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cd->vuser4.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cd->vuser4.z);
		lua_rawseti(L, -2, 3);
		break;
	default:
		return luaL_error(L, "Unknown ClientData member: %d", member);
	}
	return 1;
}

static int L_fakemeta_set_cd(lua_State* L)
{
	clientdata_t *cd;
	if (lua_isnil(L, 1))
		cd = &g_cd_glb;
	else
		cd = reinterpret_cast<clientdata_t*>(lua_touserdata(L, 1));

	if (!cd)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case CD_Origin:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->origin.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->origin.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->origin.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_Velocity:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->velocity.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->velocity.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->velocity.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_ViewModel:
		cd->viewmodel = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_PunchAngle:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->punchangle.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->punchangle.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->punchangle.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_Flags:
		cd->flags = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_WaterLevel:
		cd->waterlevel = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_WaterType:
		cd->watertype = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_ViewOfs:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->view_ofs.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->view_ofs.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->view_ofs.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_Health:
		cd->health = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_bInDuck:
		cd->bInDuck = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_Weapons:
		cd->weapons = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_flTimeStepSound:
		cd->flTimeStepSound = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_flDuckTime:
		cd->flDuckTime = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_flSwimTime:
		cd->flSwimTime = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_WaterJumpTime:
		cd->waterjumptime = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_MaxSpeed:
		cd->maxspeed = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_FOV:
		cd->fov = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_WeaponAnim:
		cd->weaponanim = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_ID:
		cd->m_iId = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_AmmoShells:
		cd->ammo_shells = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_AmmoNails:
		cd->ammo_nails = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_AmmoCells:
		cd->ammo_cells = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_AmmoRockets:
		cd->ammo_rockets = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_flNextAttack:
		cd->m_flNextAttack = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_tfState:
		cd->tfstate = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_PushMsec:
		cd->pushmsec = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_DeadFlag:
		cd->deadflag = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_PhysInfo:
		strncpy(cd->physinfo, luaL_checkstring(L, 3), sizeof(cd->physinfo) - 1);
		cd->physinfo[sizeof(cd->physinfo) - 1] = 0;
		break;
	case CD_iUser1:
		cd->iuser1 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_iUser2:
		cd->iuser2 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_iUser3:
		cd->iuser3 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_iUser4:
		cd->iuser4 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case CD_fUser1:
		cd->fuser1 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_fUser2:
		cd->fuser2 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_fUser3:
		cd->fuser3 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_fUser4:
		cd->fuser4 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case CD_vUser1:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->vuser1.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->vuser1.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->vuser1.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_vUser2:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->vuser2.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->vuser2.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->vuser2.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_vUser3:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->vuser3.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->vuser3.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->vuser3.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	case CD_vUser4:
		if (!lua_istable(L, 3))
			return luaL_error(L, "Argument 3 must be a table {x, y, z}");
		lua_rawgeti(L, 3, 1);
		cd->vuser4.x = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 2);
		cd->vuser4.y = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		lua_rawgeti(L, 3, 3);
		cd->vuser4.z = static_cast<float>(lua_tonumber(L, -1));
		lua_pop(L, 1);
		break;
	default:
		return luaL_error(L, "Unknown ClientData member: %d", member);
	}
	return 1;
}

static int L_fakemeta_get_es(lua_State* L)
{
	entity_state_t *es;
	if (lua_isnil(L, 1))
		es = &g_es_glb;
	else
		es = reinterpret_cast<entity_state_t*>(lua_touserdata(L, 1));

	if (!es)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case ES_EntityType:
		lua_pushinteger(L, es->entityType);
		break;
	case ES_Number:
		lua_pushinteger(L, es->number);
		break;
	case ES_MsgTime:
		lua_pushnumber(L, es->msg_time);
		break;
	case ES_MessageNum:
		lua_pushinteger(L, es->messagenum);
		break;
	case ES_Origin:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->origin.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->origin.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->origin.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_Angles:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->angles.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->angles.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->angles.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_ModelIndex:
		lua_pushinteger(L, es->modelindex);
		break;
	case ES_Sequence:
		lua_pushinteger(L, es->sequence);
		break;
	case ES_Frame:
		lua_pushnumber(L, es->frame);
		break;
	case ES_ColorMap:
		lua_pushinteger(L, es->colormap);
		break;
	case ES_Skin:
		lua_pushinteger(L, es->skin);
		break;
	case ES_Solid:
		lua_pushinteger(L, es->solid);
		break;
	case ES_Effects:
		lua_pushinteger(L, es->effects);
		break;
	case ES_Scale:
		lua_pushnumber(L, es->scale);
		break;
	case ES_eFlags:
		lua_pushinteger(L, es->eflags);
		break;
	case ES_RenderMode:
		lua_pushinteger(L, es->rendermode);
		break;
	case ES_RenderAmt:
		lua_pushinteger(L, es->renderamt);
		break;
	case ES_RenderColor:
		lua_createtable(L, 3, 0);
		lua_pushinteger(L, es->rendercolor.r);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, es->rendercolor.g);
		lua_rawseti(L, -2, 2);
		lua_pushinteger(L, es->rendercolor.b);
		lua_rawseti(L, -2, 3);
		break;
	case ES_RenderFx:
		lua_pushinteger(L, es->renderfx);
		break;
	case ES_MoveType:
		lua_pushinteger(L, es->movetype);
		break;
	case ES_AnimTime:
		lua_pushnumber(L, es->animtime);
		break;
	case ES_FrameRate:
		lua_pushnumber(L, es->framerate);
		break;
	case ES_Body:
		lua_pushinteger(L, es->body);
		break;
	case ES_Controller:
		lua_createtable(L, 4, 0);
		for (int i = 0; i < 4; i++)
		{
			lua_pushinteger(L, es->controller[i]);
			lua_rawseti(L, -2, i + 1);
		}
		break;
	case ES_Blending:
		lua_createtable(L, 2, 0);
		lua_pushinteger(L, es->blending[0]);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, es->blending[1]);
		lua_rawseti(L, -2, 2);
		break;
	case ES_Velocity:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->velocity.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->velocity.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->velocity.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_Mins:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->mins.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->mins.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->mins.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_Maxs:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->maxs.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->maxs.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->maxs.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_AimEnt:
		lua_pushinteger(L, es->aiment);
		break;
	case ES_Owner:
		lua_pushinteger(L, es->owner);
		break;
	case ES_Friction:
		lua_pushnumber(L, es->friction);
		break;
	case ES_Gravity:
		lua_pushnumber(L, es->gravity);
		break;
	case ES_Team:
		lua_pushinteger(L, es->team);
		break;
	case ES_PlayerClass:
		lua_pushinteger(L, es->playerclass);
		break;
	case ES_Health:
		lua_pushinteger(L, es->health);
		break;
	case ES_Spectator:
		lua_pushinteger(L, es->spectator);
		break;
	case ES_WeaponModel:
		lua_pushinteger(L, es->weaponmodel);
		break;
	case ES_GaitSequence:
		lua_pushinteger(L, es->gaitsequence);
		break;
	case ES_BaseVelocity:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->basevelocity.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->basevelocity.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->basevelocity.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_UseHull:
		lua_pushinteger(L, es->usehull);
		break;
	case ES_OldButtons:
		lua_pushinteger(L, es->oldbuttons);
		break;
	case ES_OnGround:
		lua_pushinteger(L, es->onground);
		break;
	case ES_iStepLeft:
		lua_pushinteger(L, es->iStepLeft);
		break;
	case ES_flFallVelocity:
		lua_pushnumber(L, es->flFallVelocity);
		break;
	case ES_FOV:
		lua_pushnumber(L, es->fov);
		break;
	case ES_WeaponAnim:
		lua_pushinteger(L, es->weaponanim);
		break;
	case ES_StartPos:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->startpos.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->startpos.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->startpos.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_EndPos:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->endpos.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->endpos.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->endpos.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_ImpactTime:
		lua_pushnumber(L, es->impacttime);
		break;
	case ES_StartTime:
		lua_pushnumber(L, es->starttime);
		break;
	case ES_iUser1:
		lua_pushinteger(L, es->iuser1);
		break;
	case ES_iUser2:
		lua_pushinteger(L, es->iuser2);
		break;
	case ES_iUser3:
		lua_pushinteger(L, es->iuser3);
		break;
	case ES_iUser4:
		lua_pushinteger(L, es->iuser4);
		break;
	case ES_fUser1:
		lua_pushnumber(L, es->fuser1);
		break;
	case ES_fUser2:
		lua_pushnumber(L, es->fuser2);
		break;
	case ES_fUser3:
		lua_pushnumber(L, es->fuser3);
		break;
	case ES_fUser4:
		lua_pushnumber(L, es->fuser4);
		break;
	case ES_vUser1:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->vuser1.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->vuser1.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->vuser1.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_vUser2:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->vuser2.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->vuser2.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->vuser2.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_vUser3:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->vuser3.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->vuser3.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->vuser3.z);
		lua_rawseti(L, -2, 3);
		break;
	case ES_vUser4:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, es->vuser4.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, es->vuser4.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, es->vuser4.z);
		lua_rawseti(L, -2, 3);
		break;
	default:
		return luaL_error(L, "Unknown EntityState member: %d", member);
	}
	return 1;
}

static int L_fakemeta_set_es(lua_State* L)
{
	entity_state_t *es;
	if (lua_isnil(L, 1))
		es = &g_es_glb;
	else
		es = reinterpret_cast<entity_state_t*>(lua_touserdata(L, 1));

	if (!es)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case ES_EntityType:
		es->entityType = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Number:
		es->number = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_MsgTime:
		es->msg_time = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_MessageNum:
		es->messagenum = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Origin:
	case ES_Angles:
	case ES_Velocity:
	case ES_Mins:
	case ES_Maxs:
	case ES_BaseVelocity:
	case ES_StartPos:
	case ES_EndPos:
	case ES_vUser1:
	case ES_vUser2:
	case ES_vUser3:
	case ES_vUser4:
		{
			if (!lua_istable(L, 3))
				return luaL_error(L, "Argument 3 must be a table {x, y, z}");
			float *vec = nullptr;
			switch (member)
			{
			case ES_Origin: vec = &es->origin.x; break;
			case ES_Angles: vec = &es->angles.x; break;
			case ES_Velocity: vec = &es->velocity.x; break;
			case ES_Mins: vec = &es->mins.x; break;
			case ES_Maxs: vec = &es->maxs.x; break;
			case ES_BaseVelocity: vec = &es->basevelocity.x; break;
			case ES_StartPos: vec = &es->startpos.x; break;
			case ES_EndPos: vec = &es->endpos.x; break;
			case ES_vUser1: vec = &es->vuser1.x; break;
			case ES_vUser2: vec = &es->vuser2.x; break;
			case ES_vUser3: vec = &es->vuser3.x; break;
			case ES_vUser4: vec = &es->vuser4.x; break;
			}
			lua_rawgeti(L, 3, 1);
			vec[0] = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 2);
			vec[1] = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 3);
			vec[2] = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
		break;
	case ES_ModelIndex:
		es->modelindex = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Sequence:
		es->sequence = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Frame:
		es->frame = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_ColorMap:
		es->colormap = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Skin:
		es->skin = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Solid:
		es->solid = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Effects:
		es->effects = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Scale:
		es->scale = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_eFlags:
		es->eflags = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_RenderMode:
		es->rendermode = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_RenderAmt:
		es->renderamt = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_RenderColor:
		{
			if (!lua_istable(L, 3))
				return luaL_error(L, "Argument 3 must be a table {r, g, b}");
			lua_rawgeti(L, 3, 1);
			es->rendercolor.r = static_cast<byte>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 2);
			es->rendercolor.g = static_cast<byte>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 3);
			es->rendercolor.b = static_cast<byte>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
		break;
	case ES_RenderFx:
		es->renderfx = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_MoveType:
		es->movetype = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_AnimTime:
		es->animtime = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_FrameRate:
		es->framerate = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_Body:
		es->body = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Controller:
		{
			if (!lua_istable(L, 3))
				return luaL_error(L, "Argument 3 must be a table {0, 1, 2, 3}");
			for (int i = 0; i < 4; i++)
			{
				lua_rawgeti(L, 3, i + 1);
				es->controller[i] = static_cast<byte>(lua_tonumber(L, -1));
				lua_pop(L, 1);
			}
		}
		break;
	case ES_Blending:
		{
			if (!lua_istable(L, 3))
				return luaL_error(L, "Argument 3 must be a table {0, 1}");
			for (int i = 0; i < 2; i++)
			{
				lua_rawgeti(L, 3, i + 1);
				es->blending[i] = static_cast<byte>(lua_tonumber(L, -1));
				lua_pop(L, 1);
			}
		}
		break;
	case ES_AimEnt:
		es->aiment = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Owner:
		es->owner = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Friction:
		es->friction = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_Gravity:
		es->gravity = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_Team:
		es->team = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_PlayerClass:
		es->playerclass = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Health:
		es->health = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_Spectator:
		es->spectator = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_WeaponModel:
		es->weaponmodel = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_GaitSequence:
		es->gaitsequence = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_UseHull:
		es->usehull = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_OldButtons:
		es->oldbuttons = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_OnGround:
		es->onground = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_iStepLeft:
		es->iStepLeft = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_flFallVelocity:
		es->flFallVelocity = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_FOV:
		es->fov = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_WeaponAnim:
		es->weaponanim = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_ImpactTime:
		es->impacttime = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_StartTime:
		es->starttime = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_iUser1:
		es->iuser1 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_iUser2:
		es->iuser2 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_iUser3:
		es->iuser3 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_iUser4:
		es->iuser4 = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case ES_fUser1:
		es->fuser1 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_fUser2:
		es->fuser2 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_fUser3:
		es->fuser3 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case ES_fUser4:
		es->fuser4 = static_cast<float>(luaL_checknumber(L, 3));
		break;
	default:
		return luaL_error(L, "Unknown EntityState member: %d", member);
	}
	return 1;
}

static int L_fakemeta_get_uc(lua_State* L)
{
	usercmd_t *uc;
	if (lua_isnil(L, 1))
		uc = &g_uc_glb;
	else
		uc = reinterpret_cast<usercmd_t*>(lua_touserdata(L, 1));

	if (!uc)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case UC_LerpMsec:
		lua_pushinteger(L, uc->lerp_msec);
		break;
	case UC_Msec:
		lua_pushinteger(L, uc->msec);
		break;
	case UC_ViewAngles:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, uc->viewangles.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, uc->viewangles.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, uc->viewangles.z);
		lua_rawseti(L, -2, 3);
		break;
	case UC_ForwardMove:
		lua_pushnumber(L, uc->forwardmove);
		break;
	case UC_SideMove:
		lua_pushnumber(L, uc->sidemove);
		break;
	case UC_UpMove:
		lua_pushnumber(L, uc->upmove);
		break;
	case UC_LightLevel:
		lua_pushinteger(L, uc->lightlevel);
		break;
	case UC_Buttons:
		lua_pushinteger(L, uc->buttons);
		break;
	case UC_Impulse:
		lua_pushinteger(L, uc->impulse);
		break;
	case UC_WeaponSelect:
		lua_pushinteger(L, uc->weaponselect);
		break;
	case UC_ImpactIndex:
		lua_pushinteger(L, uc->impact_index);
		break;
	case UC_ImpactPosition:
		lua_createtable(L, 3, 0);
		lua_pushnumber(L, uc->impact_position.x);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, uc->impact_position.y);
		lua_rawseti(L, -2, 2);
		lua_pushnumber(L, uc->impact_position.z);
		lua_rawseti(L, -2, 3);
		break;
	default:
		return luaL_error(L, "Unknown UserCmd member: %d", member);
	}
	return 1;
}

static int L_fakemeta_set_uc(lua_State* L)
{
	usercmd_t *uc;
	if (lua_isnil(L, 1))
		uc = &g_uc_glb;
	else
		uc = reinterpret_cast<usercmd_t*>(lua_touserdata(L, 1));

	if (!uc)
		return 0;

	int member = static_cast<int>(luaL_checkinteger(L, 2));

	switch (member)
	{
	case UC_LerpMsec:
		uc->lerp_msec = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_Msec:
		uc->msec = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_ViewAngles:
		{
			if (!lua_istable(L, 3))
				return luaL_error(L, "Argument 3 must be a table {x, y, z}");
			lua_rawgeti(L, 3, 1);
			uc->viewangles.x = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 2);
			uc->viewangles.y = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 3);
			uc->viewangles.z = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
		break;
	case UC_ForwardMove:
		uc->forwardmove = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case UC_SideMove:
		uc->sidemove = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case UC_UpMove:
		uc->upmove = static_cast<float>(luaL_checknumber(L, 3));
		break;
	case UC_LightLevel:
		uc->lightlevel = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_Buttons:
		uc->buttons = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_Impulse:
		uc->impulse = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_WeaponSelect:
		uc->weaponselect = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_ImpactIndex:
		uc->impact_index = static_cast<int>(luaL_checkinteger(L, 3));
		break;
	case UC_ImpactPosition:
		{
			if (!lua_istable(L, 3))
				return luaL_error(L, "Argument 3 must be a table {x, y, z}");
			lua_rawgeti(L, 3, 1);
			uc->impact_position.x = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 2);
			uc->impact_position.y = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			lua_rawgeti(L, 3, 3);
			uc->impact_position.z = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
		break;
	default:
		return luaL_error(L, "Unknown UserCmd member: %d", member);
	}
	return 1;
}

cell AMX_NATIVE_CALL amx_fakemetal_func_init_tr2(AMX* amx, cell* params)
{
	lua_State* L = (lua_State*)params[1];
	g_L = L;
	lua_register(L, "fakemeta_create_tr2", L_fakemeta_create_tr2);
	lua_register(L, "fakemeta_free_tr2", L_fakemeta_free_tr2);
	lua_register(L, "fakemeta_get_tr2", L_fakemeta_get_tr2);
	lua_register(L, "fakemeta_set_tr2", L_fakemeta_set_tr2);
	lua_register(L, "fakemeta_create_kvd", L_fakemeta_create_kvd);
	lua_register(L, "fakemeta_free_kvd", L_fakemeta_free_kvd);
	lua_register(L, "fakemeta_get_kvd", L_fakemeta_get_kvd);
	lua_register(L, "fakemeta_set_kvd", L_fakemeta_set_kvd);
	lua_register(L, "fakemeta_get_cd", L_fakemeta_get_cd);
	lua_register(L, "fakemeta_set_cd", L_fakemeta_set_cd);
	lua_register(L, "fakemeta_get_es", L_fakemeta_get_es);
	lua_register(L, "fakemeta_set_es", L_fakemeta_set_es);
	lua_register(L, "fakemeta_get_uc", L_fakemeta_get_uc);
	lua_register(L, "fakemeta_set_uc", L_fakemeta_set_uc);
	return TRUE;
}

