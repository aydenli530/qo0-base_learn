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
	
	for (int i = 1; i <= I::Globals->nMaxClients; i++)
	{
		CBaseEntity* pEntity = I::ClientEntityList->Get<CBaseEntity>(i);

		if (pEntity == nullptr || !pEntity->IsPlayer() || pEntity->IsDormant() || !pEntity->IsAlive() || pEntity->HasImmunity()|| !pLocal->IsEnemy(pEntity))
			continue;

		Gethitbox();
		if (Hithoxs < 0)
			continue;

		Vector hitbox_pos;


		if (C::Get<bool>(Vars.bMiscBacktrack))
			hitbox_pos = BacktrackPos(pCmd);
		else
		{
			if (const auto HitboxPos = pEntity->GetHitboxPosition(Hithoxs); HitboxPos.has_value())
				hitbox_pos = HitboxPos.value();
		}

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
                    #ifdef DEBUG_CONSOLE
					L::PushConsoleColor(FOREGROUND_YELLOW);
					std::string abc = "X " + std::to_string(Aim.x) + "Y " + std::to_string(Aim.y);
					L::Print(abc);
					L::PopConsoleColor();
                    #endif
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

Vector CLegitBot::BacktrackPos(CUserCmd* pCmd)
{
	static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag"));
	QAngle angles;
	Vector Pos;
	float best_fov = 255.f;
	for (auto i = CLagCompensation::Get().data.begin(); i != CLagCompensation::Get().data.end(); i++)
	{
		for (int record = 0; record < i->second.size(); record++) {

			
			Vector local_eye_pos = G::pLocal->GetEyePosition();

			float deltaTime = std::clamp(CLagCompensation::Get().correct_time, 0.f, sv_maxunlag->GetFloat()) - (I::Globals->flCurrentTime - i->second[record].sim_time);
			/*
			clamp(value, low, high);
			如果值小於low，則返回low。
			如果high大於value，則返回high
			*/
			if (std::fabsf(deltaTime) > C::Get<float>(Vars.bMiscBacktrackticks)/1000)  // run if less than 200ms
				continue;
			//處理float型別的取絕對值(非負值)

			angles = M::CalcAngle(local_eye_pos, i->second[record].hitbox_pos);
			//From the differences between the localplayer eye position and the backtrack player hitbox poistion
			//To calculate the pitch and yaw angles 

			float fov = M::fov_to_player(pCmd->angViewPoint, angles); // radius = distance from view_angles to angles
			if (best_fov > fov) { //To update the best_fov until the best_fov less than fov
				best_fov = fov;
				Pos = i->second[record].hitbox_pos;
				
			}
		}

	}
	return Pos;

}
