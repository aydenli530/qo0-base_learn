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

	short nDefinitionIndex = pWeapon->GetItemDefinitionIndex();
	CCSWeaponData* pWeaponData = I::WeaponSystem->GetWeaponData(nDefinitionIndex);

	if (!pWeaponData || !pWeaponData->IsGun()) {

		return;
	}

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

		// If the enemy is not visible, do the scan damage 
		if (!pLocal->IsVisible(pEntity, hitbox_pos))
		{
			if (!ScanDamage(pLocal, local_eye_pos, hitbox_pos))
				continue;
		}

		// Calculate the pitch and yaw 
		Aim = M::CalcAngle(local_eye_pos, hitbox_pos);  

		// get view and add punch
		QAngle angView = pCmd->angViewPoint;
		angView += pLocal->GetPunch() * weapon_recoil_scale->GetFloat();

		// radius = distance from view_angles to angles
		float fov = M::fov_to_player(angView, Aim); 
		
		// Check the fov
		if (fov < C::Get<int>(Vars.bAimLock)) { 
			
			// Set the target angle to the view angle for shot
			pCmd->angViewPoint = Aim;

			// Aim Lock
			if (!C::Get<bool>(Vars.bAimSilentShot))
			{
				I::Engine->SetViewAngles(Aim);
			}

			// Check the condition to shot and auto shot
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

	//Traced target 
	CBaseEntity* pEntity = trace.pHitEntity;

	//Check the traced target is vailed
	if (pEntity == nullptr || !pEntity->IsAlive() || pEntity->IsDormant() || !pEntity->IsPlayer() || pEntity->HasImmunity() || !pLocal->IsEnemy(pEntity))
	{
		return false;
	}

	//We can shot the target
	return true;
}

