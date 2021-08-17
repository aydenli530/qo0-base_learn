#pragma once
// used: winapi includes, singleton
#include "../common.h"
// used: usercmd
#include "../sdk/datatypes/usercmd.h"
// used: baseentity
#include "../sdk/entity.h"

// @note: FYI - https://www.unknowncheats.me/forum/counterstrike-global-offensive/137492-math-behind-hack-1-coding-better-aimbot-stop-using-calcangle.html
// @Credit Sensum - https://github.com/K4HVH/Sensum/blob/main/src/features/aimbot.cpp
// @Credit Operators - https://www.cs.uaf.edu/2010/spring/cs202/lecture/02_18_operators.html
// @Credit Vector<int> - https://www.runoob.com/w3cnote/cpp-vector-container-analysis.html
// array<Vector, 4> - using array to store the four elements with vector type
//=============================================================================================
// Lists将元素按顺序储存在链表中. 与 向量(vectors)相比, 它允许快速的插入和删除，但是随机访问却比较慢.
// 適用於进行大量添加或删除元素的操作，而直接访问元素的需求却很少
// @ Credit Lists - http://c.biancheng.net/view/6892.html
//==============================================================================================

struct target_t
{
	int            Tick;                         //Store for Backtrack shot 
	float          Fov;                          //Store for comparing
	float          Distance;                     //Store for comparing
	CBaseEntity*   Targert;                      //Store for identify
	int            Hitbox_id;                    //Store the hitbox Id
	Vector         Hitbox_pos;                   //Store for shot
	bool           Is_visible;                   //Store for shot condition
	bool           Is_Air;                       //Store for shot condition
	std::array<Vector, 4> Hitboxes;              //Store the pre-shot hitboxs
	Vector         Hitbox;                       //Store for final shot
	bool operator<(const target_t& other) const  //The function helps to sort the target_t struct by the Distance & Fov values
	{
		return (Distance < other.Distance) && (Fov < other.Fov);
	}
};

class CLegitBot : public CSingleton<CLegitBot>
{
public:
	// Run
	void Run(CUserCmd* pCmd, CBaseEntity* pLocal, bool& bSendPacket);

	// Get
	void Gethitbox();
	void GetBestTarget();

	// Check 
	bool IsFlashed();
	bool IsBehindSmoke(Vector src, Vector rem);
	void CheckTarget(CBaseEntity* player);
	void Hitchance();
	bool ScanDamage(CBaseEntity* pLocal, Vector vecStart, Vector vecEnd);
	
	// Function
	void BacktrackShot();
	void RecoilControlSystem();

	// Reset
	void Reset();

	//Values
	int Hithoxs;
private:
	// Main
	/* aimbot, smooth, etc */
	QAngle Aim;
	QAngle Smooth;
	QAngle OldPunch;

	// Values
	float          Nearest;
	
	// Check
	/* is entity valid */
	std::vector<target_t> entity = {};
};
