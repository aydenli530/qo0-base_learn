#include "legitbot.h"

// used: cheat variables
#include "../core/variables.h"
// used: main window open state
#include "../core/menu.h"
// used: key state
#include "../utilities/inputsystem.h"
// used: globals, cliententitylist interfaces
#include "../core/interfaces.h"
// used: backtracking
#include "lagcompensation.h"
// used: global variables
#include "../global.h"
// used: vector/angle calculations
#include "../utilities/math.h"
// used: result logging
#include "../utilities/logging.h"
// used: firebullet data
#include "autowall.h"

void CLegitBot::Run(CUserCmd* pCmd, CBaseEntity* pLocal, bool& bSendPacket)
{
	if (!pLocal->IsAlive())
		return;

	CBaseCombatWeapon* pWeapon = pLocal->GetWeapon();

	if (pWeapon == nullptr)
		return;

	// disable when in-menu for more legit (lol)
	if (W::bMainOpened)
		return;
	
	for (int i = 1; i <= I::Globals->nMaxClients; i++)
	{
		CBaseEntity* pEntity = I::ClientEntityList->Get<CBaseEntity>(i);

		if (pEntity == nullptr || !pEntity->IsPlayer() || pEntity->IsDormant() || !pEntity->IsAlive() || pEntity->HasImmunity()|| !pLocal->IsEnemy(pEntity))
			continue;

		auto model = pEntity->GetModel();
		if (!model) continue;
		auto hdr = I::ModelInfo->GetStudioModel(model);
		if (!hdr) continue;
		auto hitbox_set = hdr->GetHitboxSet(pEntity->GetHitboxSet());

		Gethitbox();
		if (Hithoxs < 0)
			continue;

		auto hitbox_head = hitbox_set->GetHitbox(Hithoxs);
		auto hitbox_center = (hitbox_head->vecBBMin + hitbox_head->vecBBMax) * 0.5f;

		matrix3x4_t pmatrix[MAXSTUDIOBONES];

		pEntity->SetupBones(pmatrix, MAXSTUDIOBONES, 0x7FF00, I::Globals->flCurrentTime); 

		Vector hitbox_pos = M::VectorTransform(hitbox_center, pmatrix[hitbox_head->iBone]); //Target hixbox position
		Vector local_eye_pos = G::pLocal->GetEyePosition();
		Aim = M::CalcAngle(local_eye_pos,hitbox_pos);  //Calculate the pitch and yaw 

		if (!ScanDamage(pLocal, local_eye_pos, hitbox_pos))
			continue;

		float fov = M::fov_to_player(pCmd->angViewPoint, Aim); // radius = distance from view_angles to angles
		
				if (fov < C::Get<int>(Vars.bAimLock)) { // Run if the distance between viewangle and targert is less than aim_lock
					pCmd->angViewPoint = Aim;

					if (pLocal->CanShoot(static_cast<CWeaponCSBase*>(pWeapon)) && C::Get<bool>(Vars.bAimAutoShot))
					{
						pCmd->iButtons |= IN_ATTACK;
					}
                    #ifdef DEBUG_CONSOLE
					L::PushConsoleColor(FOREGROUND_YELLOW);
					std::string abc = "X " + std::to_string(Aim.x) + "Y " + std::to_string(Aim.y);
					L::Print(abc);
					L::PopConsoleColor();
                    #endif
					//if (C::Get<bool>(Vars.bMiscBacktrack))
					//	pCmd->iTickCount = TIME_TO_TICKS(CLagCompensation::Get().Get_Best_SimulationTime(pCmd) + CLagCompensation::Get().lerp_time); // that is to go back in time
					//else
					//	pCmd->iTickCount = TIME_TO_TICKS(pEntity->GetSimulationTime() + CLagCompensation::Get().lerp_time);
				}
	}
}

void CLegitBot::Gethitbox()
{
	// Head
	if (C::Get<bool>(Vars.bAimHead))
		Hithoxs = HITGROUP_HEAD;
	// chest
	else if (C::Get<bool>(Vars.bAimChest))
		Hithoxs = HITGROUP_CHEST;
	// stomach
	else if (C::Get<bool>(Vars.bAimStomach))
		Hithoxs = HITGROUP_STOMACH;
	// arms
	else if (C::Get<bool>(Vars.bAimArms))
		Hithoxs = HITGROUP_RIGHTARM;
	// legs
	else if (C::Get<bool>(Vars.bAimLegs))
		Hithoxs = HITGROUP_RIGHTLEG;
	else
		Hithoxs = -1;

}

bool CLegitBot::ScanDamage(CBaseEntity* pLocal, Vector vecStart, Vector vecEnd)
{
	Trace_t trace = { };
	if (C::Get<bool>(Vars.bAimAutoWall))
	{
		FireBulletData_t data = { };

		// get autowall damage and data from it
		float flDamage = CAutoWall::Get().GetDamage(pLocal, vecEnd, data);

		// check for minimal damage
		if (flDamage < C::Get<int>(Vars.iAimMinimalDamage))
			return false;

		// copy trace from autowall
		trace = data.enterTrace;
	}
	else
	{
		// otherwise ray new trace
		Ray_t ray(vecStart, vecEnd);
		CTraceFilter filter(pLocal);
		I::EngineTrace->TraceRay(ray, MASK_SHOT, &filter, &trace);
	}

	CBaseEntity* pEntity = trace.pHitEntity;

	if (pEntity == nullptr || !pEntity->IsAlive() || pEntity->IsDormant() || !pEntity->IsPlayer() || pEntity->HasImmunity() || !pLocal->IsEnemy(pEntity))
	{
		return false;
	}

	return true;
}