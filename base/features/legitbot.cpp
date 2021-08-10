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
	QAngle Aim;
	float best_fov = 255;
	for (int i = 1; i <= I::Globals->nMaxClients; i++)
	{
		CBaseEntity* pEntity = I::ClientEntityList->Get<CBaseEntity>(i);

		if (pEntity == nullptr || !pEntity->IsPlayer() || pEntity->IsDormant() || !pEntity->IsAlive() || pEntity->HasImmunity())
			continue;

		if (pEntity->GetTeam() == G::pLocal->GetTeam())
			continue;

		auto model = pEntity->GetModel();
		if (!model) continue;
		auto hdr = I::ModelInfo->GetStudioModel(model);
		if (!hdr) continue;
		auto hitbox_set = hdr->GetHitboxSet(pEntity->GetHitboxSet());
		auto hitbox_head = hitbox_set->GetHitbox(HITBOX_HEAD);
		auto hitbox_center = (hitbox_head->vecBBMin + hitbox_head->vecBBMax) * 0.5f;

		matrix3x4_t pmatrix[MAXSTUDIOBONES];

		pEntity->SetupBones(pmatrix, MAXSTUDIOBONES, 0x7FF00, I::Globals->flCurrentTime); 
		Vector hitbox_pos = M::VectorTransform(hitbox_center, pmatrix[hitbox_head->iBone]);
		Vector local_eye_pos = G::pLocal->GetEyePosition();
		Aim = M::CalcAngle(local_eye_pos,hitbox_pos);
		float fov = M::fov_to_player(pCmd->angViewPoint, Aim); // radius = distance from view_angles to angles
		if (best_fov > fov) { //To update the best_fov until the 
			best_fov = fov;
          #ifdef DEBUG_CONSOLE
				L::PushConsoleColor(FOREGROUND_YELLOW);
				std::string abc = "X " + std::to_string(Aim.x) + "Y " + std::to_string(Aim.y);
				L::Print(abc);
				L::PopConsoleColor();
         #endif
		}
	}
	if (best_fov < 29) // Aim lock
		pCmd->angViewPoint = Aim;
}
