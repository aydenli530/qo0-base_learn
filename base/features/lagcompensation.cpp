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
#include <string>  

bool not_target(CBaseEntity* player) {
		if (!player || player == G::pLocal)
			return true;

		if (player->GetHealth() <= 0)
			return true;

		int entIndex = player->GetIndex();
		return entIndex > I::Globals->nMaxClients;
}

void inv_bone_cache(CBaseEntity* player)
{
	static DWORD addReady = (DWORD)(MEM::FindPattern(CLIENT_DLL, XorStr("80 3D ? ? ? ? ? 74 16 A1 ? ? ? ? 48 C7 81"))); // @xref: "deffered"

	*(int*)((uintptr_t)player + 0xA30) = I::Globals->flFrameTime; //we'll skip occlusion checks now
	*(int*)((uintptr_t)player + 0xA28) = 0;//clear occlusion flags

	unsigned long model_bone_counter = **(unsigned long**)(addReady + 10);
	*(unsigned int*)((DWORD)player + 0x2924) = 0xFF7FFFFF; // m_flLastBoneSetupTime = -FLT_MAX;
	*(unsigned int*)((DWORD)player + 0x2690) = (model_bone_counter - 1); // m_iMostRecentModelBoneCounter = g_iModelBoneCounter - 1;
}

void CLagCompensation::on_fsn() { //data replacement can restore the old data which allow us only shoot the old data

	if (!C::Get<bool>(Vars.bMiscBacktrack))
		return;

	CBaseEntity* player;
	static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag")); //Maximum lag compensation in seconds
	for (int i = 1; i <= I::Globals->nMaxClients; ++i) {
		player = I::ClientEntityList->Get<CBaseEntity>(i);

		if (not_target(player)) {
			if (data.count(i) > 0)
				data.erase(i); continue; //刪除 vector 中一個或多個元素。
		}

		if (player->GetTeam() == G::pLocal->GetTeam()) {
			if (data.count(i) > 0)
				data.erase(i); continue; //刪除 vector 中一個或多個元素。
		}

		auto& cur_data = data[i]; //& = 會修改到data
		if (!cur_data.empty()) {
			auto& front = cur_data.front(); //回傳 vector 第一個元素的值

			if (front.sim_time == player->GetSimulationTime()) //No difference between the record and the player
				continue;

			while (!cur_data.empty()) {
				auto& back = cur_data.back(); //回傳 vector 最尾元素的值
				float deltaTime = std::clamp(correct_time, 0.f, sv_maxunlag->GetFloat()) - (I::Globals->flCurrentTime - back.sim_time);

				if (std::fabsf(deltaTime) <= 0.2f) //run if large than 200ms
					break;
				cur_data.pop_back(); //delete the new data and store the old data
				//刪除 vector 最尾端的元素。 
			}
		}

		//now all the data is large than 200ms
		auto model = player->GetModel();
		if (!model) continue;
		auto hdr = I::ModelInfo->GetStudioModel(model);
		if (!hdr) continue;
		auto hitbox_set = hdr->GetHitboxSet(player->GetHitboxSet());
		auto hitbox_head = hitbox_set->GetHitbox(HITBOX_HEAD);
		auto hitbox_center = (hitbox_head->vecBBMin + hitbox_head->vecBBMax) * 0.5f;


		backtrack_data bd;
		bd.hitboxset = hitbox_set;
		bd.sim_time = player->GetSimulationTime();
		*(Vector*)((uintptr_t)player + 0xA0) = player->GetOrigin();
		*(int*)((uintptr_t)player + 0xA68) = 0;
		*(int*)((uintptr_t)player + 0xA30) = 0;
		inv_bone_cache(player);
		player->SetupBones(bd.bone_matrix, MAXSTUDIOBONES, 0x7FF00, I::Globals->flCurrentTime); //save the bone of the backtrack player
		bd.hitbox_pos = M::VectorTransform(hitbox_center, bd.bone_matrix[hitbox_head->iBone]); //output to bd.hitbox_pos
		data[i].push_front(bd); //insert the old data to the begin 

		//將元素從前面推入列表。在當前第一個元素和容器大小增加1之前，將新值插入到列表的開頭。
		//example
		/*
		  Input:list list{1, 2, 3, 4, 5};
		  list.push_front(6);
		  Output:6, 1, 2, 3, 4, 5
		*/
	}
}

void CLagCompensation::Run(CUserCmd* pCmd)
{
	/*
	 * we have much public info for that
	 * now it is your own way gl
	 */
	
	if (!I::Engine->IsInGame() || !G::pLocal || !G::pLocal->IsAlive()) {
		data.clear();
		return;
	}

	CBaseCombatWeapon* pWeapon = G::pLocal->GetWeapon();

	if (pWeapon == nullptr)
		return;

	short nDefinitionIndex = pWeapon->GetItemDefinitionIndex();
	CCSWeaponData* pWeaponData = I::WeaponSystem->GetWeaponData(nDefinitionIndex);

	if (!pWeaponData || !pWeaponData->IsGun()) {
		data.clear();
		return;
	}
	if (!C::Get<bool>(Vars.bMiscBacktrack)) {
		data.clear();
		return;
	}

	if (Get_Best_SimulationTime(pCmd) != -1)
		pCmd->iTickCount = Get_Best_SimulationTime(pCmd); //doing lag_compensation

}

float CLagCompensation::Get_Best_SimulationTime(CUserCmd* pCmd)
{

	//cvars

	static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag"));
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
	lerp_time = std::fmaxf(lerp_amount, lerp_ratio / update_rate); //返回两个浮点参数中较大的一个,将NaN作为缺失数据处理(在NaN和数值之间,选择数值)。
	latency = pNetChannelInfo->GetLatency(FLOW_OUTGOING) + pNetChannelInfo->GetLatency(FLOW_INCOMING);
	correct_time = latency + lerp_time;

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

		for (auto& bd : cur_data) { //the data is vaild
			float deltaTime = std::clamp(correct_time, 0.f, sv_maxunlag->GetFloat()) - (I::Globals->flCurrentTime - bd.sim_time);
			/*
			clamp(value, low, high);
			如果值小於low，則返回low。
			如果high大於value，則返回high
			*/
			if (std::fabsf(deltaTime) > C::Get<float>(Vars.bMiscBacktrackticks)/1000)  // run if less than 200ms
				continue;
			//處理float型別的取絕對值(非負值)

			angles = M::CalcAngle(local_eye_pos, bd.hitbox_pos);
			//From the differences between the localplayer eye position and the backtrack player hitbox poistion
			//To calculate the pitch and yaw angles 

			float fov = M::fov_to_player(pCmd->angViewPoint, angles); // radius = distance from view_angles to angles
			if (best_fov > fov) { //To update the best_fov until the best_fov less than fov
				best_fov = fov;
				tick_count = TIME_TO_TICKS(bd.sim_time + lerp_time);
			}
		}
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
