#include "lagcompensation.h"

// used: globals interface
#include "../core/interfaces.h"
// used: global variables
#include "../global.h"
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
//====================Inv bone cache==============================
//
// @Credit https://www.unknowncheats.me/wiki/Counter-Strike:_Global_Offensive:Fix_for_inaccurate_SetupBones()_when_target_player_is_behind_you
//================================================================
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
	if (!C::Get<bool>(Vars.bMiscBacktrack) ||( C::Get<float>(Vars.bMiscBacktrackticks) <= 0)) {
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

void CLagCompensation::on_fsn() { //Restore the data for lag compensation 

	if (!C::Get<bool>(Vars.bMiscBacktrack) || (C::Get<float>(Vars.bMiscBacktrackticks) <= 0)) {
		data.clear();
		return;
	}

	//Maximum lag compensation in seconds
	static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag")); 
	Sv_maxunlag = sv_maxunlag->GetFloat();

	for (int i = 1; i <= I::Globals->nMaxClients; ++i) {
		CBaseEntity* player = I::ClientEntityList->Get<CBaseEntity>(i);


		if (player == nullptr || !player->IsPlayer() || player->IsDormant() || !player->IsAlive() || player->HasImmunity() || !G::pLocal->IsEnemy(player)){
			if (data.count(i) > 0)
				data.erase(i); continue; // Delete more than one data 	
		}

		auto& cur_data = data[i]; //& = 會修改到data
		if (!cur_data.empty()) {

			// Get the data at the begin
			auto& front = cur_data.front(); 

			//No difference between the record and the player
			if (front.sim_time == player->GetSimulationTime()) 
				continue;

			while (!cur_data.empty()) {

				//Get the data at the end
				auto& back = cur_data.back(); 
				float deltaTime = std::clamp(correct_time, 0.f, Sv_maxunlag) - (I::Globals->flCurrentTime - back.sim_time);
				
				//run if large than 200ms
				if (std::fabsf(deltaTime) <= 0.2f) 
					break;

				//delete the data at the end
				cur_data.pop_back(); 
			}
		}

		//now all the data is large than 200ms
		auto model = player->GetModel();
		if (!model) continue;
		auto hdr = I::ModelInfo->GetStudioModel(model);
		if (!hdr) continue;
		auto hitbox_set = hdr->GetHitboxSet(player->GetHitboxSet());
		auto hitbox_pos = hitbox_set->GetHitbox(CLegitBot::Get().Hithoxs);
		auto hitbox_center = (hitbox_pos->vecBBMin + hitbox_pos->vecBBMax) * 0.5f;


		backtrack_data bd;
		bd.hitboxset = hitbox_set;
		bd.sim_time = player->GetSimulationTime();
		bd.player = player->GetBaseEntity();

		//Origin
		//*(Vector*)((uintptr_t)player + 0xA0) = player->GetOrigin();
		// the m_vecOrigin netvar is the uninterpolated origin, AbsOrigin is their interpolated origin
		player->SetAbsOrigin(player->GetOrigin());

		//Last_animation_framecount  
		*(int*)((uintptr_t)player + 0xA68) = 0; 
		*(int*)((uintptr_t)player + 0xA30) = 0;  

		// Fix PVS on networked players
		player->Inv_bone_cache();

		//save the bone of the backtrack player
		player->SetupBones(bd.bone_matrix, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::Globals->flCurrentTime); 
		
		//output to bd.hitbox_pos
		bd.hitbox_pos = M::VectorTransform(hitbox_center, bd.bone_matrix[hitbox_pos->iBone]); 

		//insert the old data at the begin 
		data[i].emplace_front(bd); 

		//example
		/*
		  Input:list list{1, 2, 3, 4, 5};
		  list.emplace_front(6);
		  Output:6, 1, 2, 3, 4, 5
		*/
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

bool CLagCompensation::Get_Correct_Tick(backtrack_data &bd)
{
	float deltaTime = std::clamp(correct_time, 0.f, Sv_maxunlag) - (I::Globals->flCurrentTime - bd.sim_time); //deltaTime = clamp(value, low, high);
	
	// Get the absolue values with float type
	if (std::fabsf(deltaTime) > (C::Get<float>(Vars.bMiscBacktrackticks) / 1000))  // run if less than the backtrack tick
		return false;

		return true;
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

			if (!Get_Correct_Tick(bd))
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
	}
	return tick_count;
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
