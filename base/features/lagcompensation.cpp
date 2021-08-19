﻿#include "lagcompensation.h"


// used: vector/angle calculations
#include "../utilities/math.h"
// used: result logging
#include "../utilities/logging.h"
// used: cheat variables
#include "../core/variables.h"
// USED: Legit hitbox
#include "legitbot.h"
#include <string>  

// ==========================Bone Mask============================
// 
// cached bone matrix is located at m_nForceBone + 0x1C = Bone Mask 
//
//====================emplace_front vs push_front=================
// 
// 相当于emplace直接把原料拿进家，造了一个。而push是造好了之后，再复制到自己家里，多了复制这一步。
// 所以emplace相对于push，使用第三种方法会更节省内存。
// @credit Korpse - https://blog.csdn.net/Kprogram/article/details/82055673
// 
//====================InvBoneCache================================
// 
// you don't need to invalidate bone cache after setting absorigin, it's as simple as
// setorigin(uninterpolated) -> setupbones() -> setorigin(original)
// either way just remember you will need to fix animations still after using this method
// 
// instead of using InvBoneCache
// overwrite bonecache -> run your traces -> restore bonecache
// 
//============================================================Source-sdk-2013================================================
// @ credit Gladiatorcheatz - v2.1
void CLagCompensation::Run(CUserCmd* pCmd) //To achieve the backtrack by using restored data which allow us shoot the old data
{
	/*
	 * we have much public info for that
	 * now it is your own way gl
	 */

	 // Check Game condition and Local condition
	if (!I::Engine->IsInGame() || !G::pLocal || !G::pLocal->IsAlive()) {
		data.clear();
		return;
	}

	// Check Backtrack tick and lancher of backtrack
	if (!C::Get<bool>(Vars.bMiscBacktrack)) {
		data.clear();
		return;
	}

	// To stop the lag compensation when we haven't gun
	if (!G::pLocal->HaveWeapon())
		return;

	//Find Correct time
	Get_Correct_Time();

	//doing lag_compensation
	if (Get_Best_SimulationTime(pCmd) != -1)
		pCmd->iTickCount = Get_Best_SimulationTime(pCmd);

}

void CLagCompensation::FrameUpdatePostEntityThink() { //Restore the data for lag compensation 

	// Enables player lag compensation
	static CConVar* sv_unlag = I::ConVar->FindVar(XorStr("sv_unlag"));

	if ((I::Globals->nMaxClients <= 1) || !sv_unlag->GetBool() || !C::Get<bool>(Vars.bMiscBacktrack))
	{
		ClearHistory();
		return;
	}

	for (int i = 1; i <= I::Globals->nMaxClients; ++i) {
		CBaseEntity* player = I::ClientEntityList->Get<CBaseEntity>(i);

		if (player == nullptr || !player->IsPlayer() || player->IsDormant() || !player->IsAlive() || player->HasImmunity() || !G::pLocal->IsEnemy(player)){
			if (data.count(i) > 0)
				data.erase(i); continue; // Delete more than one data 	
		}

		auto& cur_data = data[i]; //& = 會修改到data

		if (!cur_data.empty()) {

			auto& back = cur_data.back();

			// remove tail records that are too old
			while (!IsTickValid(TIME_TO_TICKS(back.sim_time))) {

				// Delete the old record
				cur_data.pop_back();

				// Get the new data at the end
				if (!cur_data.empty())
					back = cur_data.back();
			}
		}


		// check if record has same simulation time
		if (cur_data.size() > 0)
		{
			auto& tail = cur_data.back();

			if (tail.sim_time == player->GetSimulationTime())
				continue;
		}

		// update animations


        // create the record
		backtrack_data bd;
		bd.SaveRecord(player);

		// add new record to player track
		data[i].emplace_front(bd);
	}
}

void CLagCompensation::Get_Correct_Time()
{
	//cvars

	//static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag"));
	//This console command sets the maximum lag compensation in seconds.

	static CConVar* sv_minupdaterate = I::ConVar->FindVar("sv_minupdaterate");
	// This command sets the minimum update rate for the server.
	//This can be used in combation with the sv_mincmdrate command to change the server's tick rate (e.g. to 128 tick).

	static CConVar* sv_maxupdaterate = I::ConVar->FindVar("sv_maxupdaterate");
	// This command sets the maximum update rate for the server

	static CConVar* sv_client_min_interp_ratio = I::ConVar->FindVar("sv_client_min_interp_ratio");
	//

	static CConVar* sv_client_max_interp_ratio = I::ConVar->FindVar("sv_client_max_interp_ratio");
	//

	static CConVar* cl_interp_ratio = I::ConVar->FindVar("cl_interp_ratio");
	//This command sets the interpolation amount. 
	//The final value is determined from this command's value divided by the value of cl_updaterate.

	static CConVar* cl_interp = I::ConVar->FindVar("cl_interp");
	//Changes and sets your interpolation amount. 
	//This command can't be altered while playing, meaning you must be on the main menu or spectating a match.

	static CConVar* cl_updaterate = I::ConVar->FindVar("cl_updaterate");
	//This command is used to set the number of packets per second of updates you request from the server.
	
	
	float updaterate = cl_updaterate->GetFloat();
	float minupdaterate = sv_minupdaterate->GetFloat();
	float maxupdaterate = sv_maxupdaterate->GetFloat();
	float min_interp = sv_client_min_interp_ratio->GetFloat();
	float max_interp = sv_client_max_interp_ratio->GetFloat();
	float lerp_amount = cl_interp->GetFloat();
	float lerp_ratio = cl_interp_ratio->GetFloat();

	lerp_ratio = std::clamp(lerp_ratio, min_interp, max_interp);
	if (lerp_ratio == 0.0f) lerp_ratio = 1.0f;
	float update_rate = std::clamp(updaterate, minupdaterate, maxupdaterate);
	INetChannelInfo* pNetChannelInfo = I::Engine->GetNetChannelInfo();
	lerp_time = std::fmaxf(lerp_amount, lerp_ratio / update_rate); // Retuen the maximum value between two values with float type
	latency = pNetChannelInfo->GetLatency(FLOW_OUTGOING) + pNetChannelInfo->GetLatency(FLOW_INCOMING);
	correct_time = latency + lerp_time;
}

int CLagCompensation::Get_Best_SimulationTime(CUserCmd* pCmd)
{
	Vector local_eye_pos = G::pLocal->GetEyePosition();
	QAngle angles;
	int tick_count = -1;
	float best_fov = 255.0f;
	for (auto& node : data) { //the old data
		auto& cur_data = node.second;

		/*Map總共有兩個值

		x.first : 第一個稱為關鍵字(key)，在Map裡面，絕對不會重複.

		x.second: 第二個稱為關鍵字的值(value).
		*/

		if (cur_data.empty())
			continue;

		int i = 0;

		//Loop the vaild data of each player
		for (auto& bd : cur_data) { 

			//if (!Get_Correct_Tick(bd))
			//	continue;

			if (!IsTickValid(TIME_TO_TICKS(bd.sim_time)))
				continue;

			i++;

			//From the differences between the localplayer eye position and the backtrack player hitbox poistion
			//To calculate the pitch and yaw angles 
			angles = M::CalcAngle(local_eye_pos, bd.hitbox_pos);

			float fov = M::fov_to_player(pCmd->angViewPoint, angles); // radius = distance from view_angles to angles
			if (best_fov > fov -10) { //To update the best_fov until the best_fov less than fov, issue- the 10 values should be calculated to provide a more accuracy hithox_pos
				LastTarget = bd.hitbox_pos;

				// Add cl_interp on to the tickcount is to bypass the interpolation created ticks for ultimate accuracy
				// @credit SenatorII - https://www.unknowncheats.me/forum/counterstrike-global-offensive/289641-aimbot-interpolation.html
			    // un-interpolated tick
				tick_count = TIME_TO_TICKS(bd.sim_time + lerp_time);
				best_fov = fov;
			}
			
		}
		tick = i;
		 //#ifdef DEBUG_CONSOLE
		 //L::PushConsoleColor(FOREGROUND_YELLOW);
		 //std::string abc = "Tick: " + std::to_string(tick);
		 //L::Print(abc);
		 //L::PopConsoleColor();
   //      #endif
	}
	return tick_count;
}

bool CLagCompensation::IsTickValid(int tick) {
	
	//Maximum lag compensation in seconds
	static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag"));

	//delete the data at the end
	float deltaTime = std::clamp(correct_time, 0.f, sv_maxunlag->GetFloat()) - (I::Globals->flCurrentTime - TICKS_TO_TIME(tick));

	// return false if large than 0.2f
	return std::fabsf(deltaTime) < 0.2f;
	
}

void backtrack_data::SaveRecord(CBaseEntity* player)
{
	auto model = player->GetModel();
	if (!model) return;
	auto hdr = I::ModelInfo->GetStudioModel(model);
	if (!hdr) return;
	auto hitbox_set = hdr->GetHitboxSet(player->GetHitboxSet());
	auto hitbox = hitbox_set->GetHitbox(CLegitBot::Get().Hithoxs);
	auto hitbox_center = (hitbox->vecBBMin + hitbox->vecBBMax) * 0.5f;

	hitboxset = hitbox_set;
	sim_time = player->GetSimulationTime();
	Recordplayer = player->GetBaseEntity();
	m_angAngles = player->GetEyeAngles();
	m_nFlags = player->GetFlags();
	m_vecAbsAngle = player->GetAbsAngles();
	m_vecAbsOrigin = player->GetAbsOrigin();
	m_vecOrigin = player->GetOrigin();
	m_vecMax = player->GetCollideable()->OBBMaxs();
	m_vecMins = player->GetCollideable()->OBBMins();

	//Before we save bones, we must set up player animations, bones, etc to use non-interpolated data here
	player->SetAbsOrigin(player->GetOrigin());

	// Calling invalidatebonecache allows the entity to properly be “scanned”
	// Resets the entity's bone cache values in order to prepare for a model change.
	player->Inv_bone_cache();

	// Set up the bone of the backtrack player
	player->SetupBones(bone_matrix, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::Globals->flCurrentTime);

	//output to bd.hitbox_pos
	hitbox_pos = M::VectorTransform(hitbox_center, bone_matrix[hitbox->iBone]);

}

void CLagCompensation::FinishLagCompensation(CBaseEntity* player) {

	int idx = player->GetIndex();
	
	// Restore the record
	player->GetFlags() = m_RestoreLagRecord[idx].second.m_nFlags;
	player->SetAbsOrigin(m_RestoreLagRecord[idx].second.m_vecOrigin);
	player->SetAbsAngles(QAngle(0, m_RestoreLagRecord[idx].second.m_angAngles.y, 0));
	player->GetCollideable()->OBBMaxs() = m_RestoreLagRecord[idx].second.m_vecMax;
	player->GetCollideable()->OBBMins() = m_RestoreLagRecord[idx].second.m_vecMins;

}




void CLagCompensation::UpdateIncomingSequences(INetChannel* pNetChannel)
{
	if (pNetChannel == nullptr)
		return;

	// set to real sequence to update, otherwise needs time to get it work again
	if (nLastIncomingSequence == 0)
		nLastIncomingSequence = pNetChannel->iInSequenceNr;

	// check how much sequences we can spike
	if (pNetChannel->iInSequenceNr > nLastIncomingSequence)
	{
		nLastIncomingSequence = pNetChannel->iInSequenceNr;
		vecSequences.emplace_front(SequenceObject_t(pNetChannel->iInReliableState, pNetChannel->iOutReliableState, pNetChannel->iInSequenceNr, I::Globals->flRealTime));
	}

	// is cached too much sequences
	if (vecSequences.size() > 2048U)
		vecSequences.pop_back();
}

void CLagCompensation::ClearIncomingSequences()
{
	if (!vecSequences.empty())
	{
		nLastIncomingSequence = 0;
		vecSequences.clear();
	}
}

void CLagCompensation::AddLatencyToNetChannel(INetChannel* pNetChannel, float flLatency)
{
	for (const auto& sequence : vecSequences)
	{
		if (I::Globals->flRealTime - sequence.flCurrentTime >= flLatency)
		{
			pNetChannel->iInReliableState = sequence.iInReliableState;
			pNetChannel->iInSequenceNr = sequence.iSequenceNr;
			break;
		}
	}
}
