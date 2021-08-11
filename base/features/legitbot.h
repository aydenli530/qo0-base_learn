#pragma once
// used: winapi includes, singleton
#include "../common.h"
// used: usercmd
#include "../sdk/datatypes/usercmd.h"
// used: baseentity
#include "../sdk/entity.h"

// @note: FYI - https://www.unknowncheats.me/forum/counterstrike-global-offensive/137492-math-behind-hack-1-coding-better-aimbot-stop-using-calcangle.html

class CLegitBot : public CSingleton<CLegitBot>
{
public:
	// Get
	void Run(CUserCmd* pCmd, CBaseEntity* pLocal, bool& bSendPacket);
	void Gethitbox();
	bool ScanDamage(CBaseEntity* pLocal, Vector vecStart, Vector vecEnd);
private:
	// Main
	/* aimbot, smooth, etc */
	QAngle Aim;
	int Hithoxs;
	// Check
	/* is entity valid */
};
