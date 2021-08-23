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

struct LayerRecord
{
	LayerRecord()
	{
		m_nOrder = 0;
		m_nSequence = 0;
		m_flWeight = 0.f;
		m_flCycle = 0.f;
	}

	LayerRecord(const LayerRecord& src)
	{
		m_nOrder = src.m_nOrder;
		m_nSequence = src.m_nSequence;
		m_flWeight = src.m_flWeight;
		m_flCycle = src.m_flCycle;
	}

	uint32_t                 m_nOrder;
	uint32_t                 m_nSequence;
	float_t                  m_flWeight;
	float_t                  m_flCycle;
};

struct LagRecord
{
	LagRecord()
	{
		m_iPriority = -1;

		m_nFlags = 0;
		m_flSimulationTime = -1.f;
		m_vecOrigin;
		m_angAngles;
		m_vecMins;
		m_vecMax;
		m_bMatrixBuilt = false;
	}

	bool operator==(const LagRecord& rec)
	{
		return m_flSimulationTime == rec.m_flSimulationTime;
	}

	void SaveRecord(CBaseEntity* player);

	matrix3x4_t	             matrix[MAXSTUDIOBONES];
	bool                     m_bMatrixBuilt;
	Vector                   m_vecHeadSpot;
	float                    m_iTickCount;

	// For priority/other checks
	int32_t                  m_iPriority;
	Vector                   m_vecVelocity;
	float_t                  m_flPrevLowerBodyYaw;
	std::array<float_t, 24>  m_arrflPrevPoseParameters;
	Vector                   m_vecLocalAimspot;
	bool                     m_bNoGoodSpots;

	// For testing
	Vector                   hitbox_pos;
	CBaseEntity*             Recordplayer;
	mstudiohitboxset_t*      hitboxset;

	// Did player die this frame
	int32_t                  m_nFlags;

	// Player position, orientation and bbox for backtracing
	Vector                   m_vecOrigin;	   // Server data, change to this for accuracy
	Vector                   m_vecAbsOrigin;
	QAngle                   m_angAngles;
	Vector                   m_vecMins;
	Vector                   m_vecMax;

	float_t                  m_flSimulationTime;

	std::array<float_t, 24>  m_arrflPoseParameters;

	// Player animation details, so we can get the legs in the right spot.
	std::array<LayerRecord, 15> m_LayerRecords;
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
	float Get_Lerp_Time();
	void Run(CUserCmd* pCmd);
	int Get_Tick_Count(CUserCmd* pCmd);

	// Source sdk 2013
	void FrameUpdatePostEntityThink();
	bool IsTickValid(int tick);
	
	bool StartLagCompensation(CBaseEntity* player);
	bool FindViableRecord(CBaseEntity* player, LagRecord* record);
	void FinishLagCompensation(CBaseEntity* player);
	void BacktrackPlayer(CBaseEntity* player, float flTargetTime);

	// Main
	void UpdateIncomingSequences(INetChannel* pNetChannel);
	void ClearIncomingSequences();
	void AddLatencyToNetChannel(INetChannel* pNetChannel, float flLatency);

	//Values
	int tick = 0;

	//Backtrack values
	Vector LastTarget;

	// keep a list of lag records for each player
	// using std::map to replace data[64] which can give a key for each player 
	std::map<int, std::deque<LagRecord>> data = {};

	// Scratchpad for determining what needs to be restored
	std::deque<LagRecord> backtrack_records = {};

	// Pair是将2个数据组合成一组数据，当需要这样的需求时就可以使用pair，如stl中的map就是将key和value放在一起来保存。
	std::pair<LagRecord, LagRecord> m_RestoreLagRecord[64]; // Used to restore/change
	
	// Used for various other actions where we need data of the current lag compensated player
	std::map<int, std::deque<LagRecord>> current_record = {};

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
