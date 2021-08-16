#pragma once
// used: winapi includes, singleton
#include "../common.h"
// used: usercmd
#include "../sdk/datatypes/usercmd.h"
// used: baseentity
#include "../sdk/entity.h"

// @note: FYI - https://www.unknowncheats.me/forum/counterstrike-global-offensive/137492-math-behind-hack-1-coding-better-aimbot-stop-using-calcangle.html

struct target_t
{
	CBaseEntity*   targert;
	Vector         hitbox_pos;
	bool           Is_Smoked;
	bool           Is_Flashed;
	bool           Is_Air;
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
	float Fov;
	float Nearest;
	float Distance;
	
	// Check
	/* is entity valid */
	std::vector<target_t> entity = {};
};
