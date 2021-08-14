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

	static CConVar* weapon_recoil_scale = I::ConVar->FindVar(XorStr("weapon_recoil_scale"));

	if (weapon_recoil_scale == nullptr)
		return;

	CBaseCombatWeapon* pWeapon = pLocal->GetWeapon();

	if (pWeapon == nullptr)
		return;

	// disable when in-menu for more legit (lol)
	if (W::bMainOpened)
		return;
	
	Gethitbox();
	if (Hithoxs < 0)
		return;

	for (int i = 1; i <= I::Globals->nMaxClients; i++)
	{
		CBaseEntity* pEntity = I::ClientEntityList->Get<CBaseEntity>(i);

		if (pEntity == nullptr || !pEntity->IsPlayer() || pEntity->IsDormant() || !pEntity->IsAlive() || pEntity->HasImmunity()|| !pLocal->IsEnemy(pEntity))
			continue;

		Vector hitbox_pos = C::Get<bool>(Vars.bAimRecord) ? CLagCompensation::Get().LastTarget : pEntity->GetHitboxPosition(Hithoxs).value();

		Vector local_eye_pos = G::pLocal->GetEyePosition();

		if (!pLocal->IsVisible(pEntity, hitbox_pos))
		{
			if (!ScanDamage(pLocal, local_eye_pos, hitbox_pos))
				continue;
		}

		Aim = M::CalcAngle(local_eye_pos, hitbox_pos);  //Calculate the pitch and yaw 

		// get view and add punch
		QAngle angView = pCmd->angViewPoint;
		angView += pLocal->GetPunch() * weapon_recoil_scale->GetFloat();

		float fov = M::fov_to_player(angView, Aim); // radius = distance from view_angles to angles
		
		if (fov < C::Get<int>(Vars.bAimLock)) { // Run if the distance between viewangle and targert is less than aim_lock
					
			pCmd->angViewPoint = Aim;
			if (!C::Get<bool>(Vars.bAimSilentShot))
			{
				I::Engine->SetViewAngles(Aim);
			}
			if (pLocal->CanShoot(static_cast<CWeaponCSBase*>(pWeapon)) && C::Get<bool>(Vars.bAimAutoShot))
			{
				pCmd->iButtons |= IN_ATTACK;
			}
   //         #ifdef DEBUG_CONSOLE
			//L::PushConsoleColor(FOREGROUND_YELLOW);
			//std::string abc = "X " + std::to_string(Aim.x) + "Y " + std::to_string(Aim.y);
			//L::Print(abc);
			//L::PopConsoleColor();
   //         #endif
		}
	}
}

void CLegitBot::Gethitbox()
{
	// Head
	if (C::Get<bool>(Vars.bAimHead))
		Hithoxs = HITBOX_HEAD;
	// chest
	else if (C::Get<bool>(Vars.bAimChest))
		Hithoxs = HITBOX_CHEST;
	// stomach
	else if (C::Get<bool>(Vars.bAimStomach))
		Hithoxs = HITBOX_STOMACH;
	// arms
	else if (C::Get<bool>(Vars.bAimArms))
		Hithoxs = HITBOX_RIGHT_FOREARM;
	// legs
	else if (C::Get<bool>(Vars.bAimLegs))
		Hithoxs = HITBOX_RIGHT_FOOT;
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

	CBaseEntity* pEntity = trace.pHitEntity;

	if (pEntity == nullptr || !pEntity->IsAlive() || pEntity->IsDormant() || !pEntity->IsPlayer() || pEntity->HasImmunity() || !pLocal->IsEnemy(pEntity))
	{
		return false;
	}

	return true;
}

