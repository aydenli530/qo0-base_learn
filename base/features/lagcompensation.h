#pragma once
// used: std::deque
#include <deque>
// used: std::map
#include <map>
// used: winapi definitions
#include "../common.h"
// used: usercmd
#include "../sdk/datatypes/usercmd.h"
// used: netchannel
#include "../sdk/interfaces/inetchannel.h"
// used: baseentity
#include "../sdk/entity.h"
// used: bone masks, studio classes
#include "../sdk/studio.h"
// used: cheat variables
#include "../core/variables.h"
// used: global variables
#include "../global.h"
// used: globals interface
#include "../core/interfaces.h"

#pragma region lagcompensation_definitions
#define LAG_COMPENSATION_TELEPORTED_DISTANCE_SQR ( 64.0f * 64.0f )
#define LAG_COMPENSATION_EPS_SQR ( 0.1f * 0.1f )
#define LAG_COMPENSATION_ERROR_EPS_SQR ( 4.0f * 4.0f )
#pragma endregion

#pragma region lagcompensation_enumerations
enum ELagCompensationState
{
	LC_NO =					0,
	LC_ALIVE =				(1 << 0),
	LC_ORIGIN_CHANGED =		(1 << 8),
	LC_ANGLES_CHANGED =		(1 << 9),
	LC_SIZE_CHANGED =		(1 << 10),
	LC_ANIMATION_CHANGED =	(1 << 11)
};
#pragma endregion

struct SequenceObject_t
{
	SequenceObject_t(int iInReliableState, int iOutReliableState, int iSequenceNr, float flCurrentTime)
		: iInReliableState(iInReliableState), iOutReliableState(iOutReliableState), iSequenceNr(iSequenceNr), flCurrentTime(flCurrentTime) { }

	int iInReliableState;
	int iOutReliableState;
	int iSequenceNr;
	float flCurrentTime;
};

struct backtrack_data 
{
	void                SaveRecord(CBaseEntity* player);
	CBaseEntity*        Recordplayer;
	float               sim_time;
	mstudiohitboxset_t* hitboxset;
	Vector              hitbox_pos;
	Vector              m_vecOrigin;
	QAngle              m_vecAbsAngle;
	Vector              m_vecAbsOrigin;
	QAngle              m_angAngles;
	Vector              m_vecMins;
	Vector              m_vecMax;
	int                 m_nFlags;
	Vector              m_vecVelocity;
	matrix3x4_t         bone_matrix[MAXSTUDIOBONES];
};

//
// @note: FYI - https://www.unknowncheats.me/forum/counterstrike-global-offensive/280912-road-perfect-aimbot-1-interpolation.html
// @note deque -  容器也擅长在序列尾部添加或删除元素（时间复杂度为O(1)），而不擅长在序列中间添加或删除元素，还擅长在序列头部添加或删除元素
// @credit biancheng - http://c.biancheng.net/view/6860.html
//
class CLagCompensation : public CSingleton<CLagCompensation>
{
public:
	// Get
	void Get_Correct_Time();
	void Run(CUserCmd* pCmd);
	int Get_Best_SimulationTime(CUserCmd* pCmd);

	// Source sdk 2013
	void FrameUpdatePostEntityThink();
	bool IsTickValid(int tick);
	
	bool StartLagCompensation(CBaseEntity* player);
	bool FindViableRecord(CBaseEntity* player, backtrack_data* record);
	void FinishLagCompensation(CBaseEntity* player);

	// Main
	void UpdateIncomingSequences(INetChannel* pNetChannel);
	void ClearIncomingSequences();
	void AddLatencyToNetChannel(INetChannel* pNetChannel, float flLatency);

	//Values
	float correct_time = 0.0f;
	float latency = 0.0f;
	float lerp_time = 0.0f;
	int tick = 0;

	//Backtrack values
	Vector LastTarget;
	std::map<int, std::deque<backtrack_data>> data = { };
	//pair是将2个数据组合成一组数据，当需要这样的需求时就可以使用pair，如stl中的map就是将key和value放在一起来保存。
	std::pair<backtrack_data, backtrack_data> m_RestoreLagRecord[64]; // Used to restore/change
	inline void ClearHistory()
	{
		for (int i = 0; i < I::Globals->nMaxClients; i++)
			data[i].clear();
	}

private:
	// Values
	std::deque<SequenceObject_t> vecSequences = { };
	/* our real incoming sequences count */
	int nRealIncomingSequence = 0;
	/* count of incoming sequences what we can spike */
	int nLastIncomingSequence = 0;


};
