#include "lagcompensation.h"


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

	//doing lag_compensation
	if (Get_Tick_Count(pCmd) != -1)
		pCmd->iTickCount = Get_Tick_Count(pCmd);

}

//=======================================Backtrack - Store Record=================================

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
		
		//if (player == nullptr || !player->IsPlayer() || player->IsDormant() || !player->IsAlive() || player->HasImmunity() || !G::pLocal->IsEnemy(player)){
		//	if (data.count(i) > 0)
		//		data.erase(i); continue; // Delete more than one data 	
		//}

		if (!player->IsPlayerValid())
		{
			if (data.count(i) > 0)
				data.erase(i); continue; // Delete more than one data 	
		}

		auto& cur_data = data[i]; //& = 會修改到data

		if (!cur_data.empty()) {

			auto& back = cur_data.back();

			// remove tail records that are too old
			while (!IsTickValid(TIME_TO_TICKS(back.m_flSimulationTime))) {

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

			if (tail.m_flSimulationTime == player->GetSimulationTime())
				continue;
		}

		// update animations


        // create the record
		LagRecord bd;
		bd.SaveRecord(player);

		// add new record to player track
		data[i].emplace_front(bd);
	}
}

float CLagCompensation::Get_Lerp_Time()
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

	// Calculate
	lerp_ratio = std::clamp(lerp_ratio, min_interp, max_interp);
	if (lerp_ratio == 0.0f) lerp_ratio = 1.0f;
	float update_rate = std::clamp(updaterate, minupdaterate, maxupdaterate);

	// Retuen the maximum value between two values with float type
	return std::fmaxf(lerp_amount, lerp_ratio / update_rate); 

}

bool CLagCompensation::IsTickValid(int tick) {
	
	//Maximum lag compensation in seconds
	static CConVar* sv_maxunlag = I::ConVar->FindVar(XorStr("sv_maxunlag"));

	INetChannelInfo* pNetChannelInfo = I::Engine->GetNetChannelInfo();

	// Due to extended backtrack, the correct time need to add FLOW_INCOMING
	float latency = pNetChannelInfo->GetLatency(FLOW_OUTGOING) + pNetChannelInfo->GetLatency(FLOW_INCOMING);

	// Get correct time
	float correct_time = latency + Get_Lerp_Time();

	//delete the data at the end
	float deltaTime = std::clamp(correct_time, 0.f, sv_maxunlag->GetFloat()) - (I::Globals->flCurrentTime - TICKS_TO_TIME(tick));

	// return false if large than 0.2f
	return std::fabsf(deltaTime) < 0.2f;
	
}

void LagRecord::SaveRecord(CBaseEntity* player)
{
	m_vecOrigin = player->GetOrigin();
	m_vecAbsOrigin = player->GetAbsOrigin();
	m_angAngles = player->GetEyeAngles();
	m_flSimulationTime = player->GetSimulationTime();
	m_vecMax = player->GetCollideable()->OBBMaxs();
	m_vecMins = player->GetCollideable()->OBBMins();

	m_nFlags = player->GetFlags();
	Recordplayer = player->GetBaseEntity();

	int layerCount = player->GetAnimationOverlaysCount();
	for (int i = 0; i < layerCount; i++)
	{
		CAnimationLayer* currentLayer = player->GetAnimationLayer(i);
		m_LayerRecords[i].m_nOrder = currentLayer->iOrder;
		m_LayerRecords[i].m_nSequence = currentLayer->nSequence;
		m_LayerRecords[i].m_flWeight = currentLayer->flWeight;
		m_LayerRecords[i].m_flCycle = currentLayer->flCycle;
	}
	// To replace GetSequence() & GetCycle() ?
	m_arrflPoseParameters = player->GetPoseParameter(); // 姿勢參數 Didn't see in source

	auto model = player->GetModel();
	if (!model) return;
	auto hdr = I::ModelInfo->GetStudioModel(model);
	if (!hdr) return;
	auto hitbox_set = hdr->GetHitboxSet(player->GetHitboxSet());
	auto hitbox = hitbox_set->GetHitbox(CLegitBot::Get().Hithoxs);
	auto hitbox_center = (hitbox->vecBBMin + hitbox->vecBBMax) * 0.5f;

	hitboxset = hitbox_set;

	//Before we save bones, we must set up player animations, bones, etc to use non-interpolated data here
	player->SetAbsOrigin(player->GetOrigin());

	// Calling invalidatebonecache allows the entity to properly be “scanned”
	// Resets the entity's bone cache values in order to prepare for a model change.
	player->Inv_bone_cache();

	// Set up the bone of the backtrack player
	player->SetupBones(matrix, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, I::Globals->flCurrentTime);

	//output to bd.hitbox_pos
	hitbox_pos = M::VectorTransform(hitbox_center, matrix[hitbox->iBone]);

}

int CLagCompensation::Get_Tick_Count(CUserCmd* pCmd)
{
	Vector local_eye_pos = G::pLocal->GetEyePosition();
	QAngle angles;
	int tick_count = -1;
	float best_fov = 255.0f;

	for (auto& node : backtrack_records) { //the old data
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

			if (!IsTickValid(TIME_TO_TICKS(bd.m_flSimulationTime)))
				continue;

			i++;

			//From the differences between the localplayer eye position and the backtrack player hitbox poistion
			//To calculate the pitch and yaw angles 
			angles = M::CalcAngle(local_eye_pos, bd.hitbox_pos);

			float fov = M::fov_to_player(pCmd->angViewPoint, angles); // radius = distance from view_angles to angles
			if (best_fov > fov - 10) { //To update the best_fov until the best_fov less than fov, issue- the 10 values should be calculated to provide a more accuracy hithox_pos
				LastTarget = bd.hitbox_pos;

				// Add cl_interp on to the tickcount is to bypass the interpolation created ticks for ultimate accuracy
				// @credit SenatorII - https://www.unknowncheats.me/forum/counterstrike-global-offensive/289641-aimbot-interpolation.html
				// un-interpolated tick
				tick_count = TIME_TO_TICKS(bd.m_flSimulationTime + Get_Lerp_Time());
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

//=======================================Backtrack - Process ======================================

bool CLagCompensation::StartLagCompensation(CBaseEntity* player) {
	
	// Assume the Backtrack Records are empty
	backtrack_records.clear();

	// Enables player lag compensation
	static CConVar* sv_unlag = I::ConVar->FindVar(XorStr("sv_unlag"));

	if ((I::Globals->nMaxClients <= 1) || !sv_unlag->GetBool() || !C::Get<bool>(Vars.bMiscBacktrack))
	{
		return false;
	}

	enum
	{
		// Only try to store the "best" records, otherwise fail.
		TYPE_BEST_RECORDS,
		// Only try to store the newest and the absolute best record.
		TYPE_BEST_AND_NEWEST,
		// Store everything (fps killer)
		TYPE_ALL_RECORDS,
	};

	// All Records
	auto& m_LagRecords = data[player->GetIndex()]; 

	// Backtrack Records
	auto& Records = backtrack_records[player->GetIndex()];

	// Store the Records and Save into the second of the m_RestoreLagRecord
	m_RestoreLagRecord[player->GetIndex()].second.SaveRecord(player);

	switch (C::Get<int>(Vars.bAim_Rage_Lagcompensation_Type))
	{
	case TYPE_BEST_RECORDS:
	{
		for (auto it : m_LagRecords)
		{
			if (it.m_iPriority >= 1 || (it.m_vecVelocity.Length2D() > 10.f)) // let's account for those moving fags aswell -> it's experimental and not supposed what this lagcomp mode should do
				Records.emplace_back(it);
		}
		break;
	}
	case TYPE_BEST_AND_NEWEST:
	{
		// Set up an Empty Record
		LagRecord newest_record = LagRecord();
		for (auto it : m_LagRecords)
		{
			if (it.m_flSimulationTime > newest_record.m_flSimulationTime)
				newest_record = it;
			// To store more accuracy records if the m_iPriority of the records >= 1
			if (it.m_iPriority >= 1)
				Records.emplace_back(it);
		}
		Records.emplace_back(newest_record);
		break;
	}
	case TYPE_ALL_RECORDS:
		// Backtrack records equal to all the records that we store in the FRAME_NET_UPDATE_END, and this will drop huge fps
		Records = m_LagRecords;
		break;
	}
	// Sort the backtrack records by comparing the m_iPriority
	// Higher m_iPriority will sort at the begin
	//std::sort(backtrack_records.begin(), backtrack_records.end(), [](const LagRecord &a,const LagRecord& b)
	//{
	//	return a.m_iPriority > b.m_iPriority;
	//});

	return backtrack_records.size() > 0;

}

void CLagCompensation::BacktrackPlayer(CBaseEntity* player, float flTargetTime)
{
	// get track history of this player
	auto& cur_data = data[player->GetIndex()]; //& = 會修改到data

    // check if we have at leat one entry
	if (cur_data.size() <= 0)
		return;

	auto& begin = cur_data.front();

	LagRecord* prevRecord = NULL;
	LagRecord* record = NULL;

	Vector prevOrg = player->GetOrigin();
}

bool CLagCompensation::FindViableRecord(CBaseEntity* player, LagRecord* record)
{
	return true;
}

void CLagCompensation::FinishLagCompensation(CBaseEntity* player) {

	int idx = player->GetIndex();
	
	// Restore the record
	player->GetFlags() = m_RestoreLagRecord[idx].second.m_nFlags;
	player->SetAbsOrigin(m_RestoreLagRecord[idx].second.m_vecOrigin);
	player->SetAbsAngles(QAngle(0, m_RestoreLagRecord[idx].second.m_angAngles.y, 0));
	player->GetCollideable()->OBBMaxs() = m_RestoreLagRecord[idx].second.m_vecMax;
	player->GetCollideable()->OBBMins() = m_RestoreLagRecord[idx].second.m_vecMins;

	int layerCount = player->GetAnimationOverlaysCount();
	for (int i = 0; i < layerCount; ++i)
	{
		CAnimationLayer* currentLayer = player->GetAnimationLayer(i);
		currentLayer->iOrder = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_nOrder;
		currentLayer->nSequence = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_nSequence;
		currentLayer->flWeight = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_flWeight;
		currentLayer->flCycle = m_RestoreLagRecord[idx].second.m_LayerRecords[i].m_flCycle;
	}

	player->GetPoseParameter() = m_RestoreLagRecord[idx].second.m_arrflPoseParameters;

}

//=======================================Extended Backtrack==========================================

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
