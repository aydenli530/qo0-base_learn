@credit: https://github.com/spirthack/CSGOSimple/blob/master/CSGOSimple/hooks.cpp

bool H::Setup()
{
  	if (!DTR::CreateMove_Proxy.Create(MEM::GetVFunc(I::Client, VTABLE::CREATEMOVE_PROXY), &hkCreateMove_Proxy))
		return false;
}

__declspec(naked) void FASTCALL H::hkCreateMove_Proxy(void* _this, int, int sequence_number, float input_sample_frametime, bool active)
{
	__asm
	{
		push ebp
		mov  ebp, esp
		push ebx; not sure if we need this
		push esp
		push dword ptr[active]
		push dword ptr[input_sample_frametime]
		push dword ptr[sequence_number]
		call H::hkCreateMove
		pop  ebx
		pop  ebp
		retn 0Ch
	}
}

void __stdcall H::hkCreateMove(int32_t iSequence, float_t flFrametime, bool bIsActive, bool& bSendPacket)
{
	static auto oCreateMove = DTR::CreateMove_Proxy.GetOriginal<decltype(&hkCreateMove_Proxy)>();
	oCreateMove(I::Client,0,iSequence, flFrametime, bIsActive);

	CBaseEntity* pLocal = G::pLocal = CBaseEntity::GetLocalPlayer();

	const auto pCmd = I::Input->GetUserCmd(iSequence);
	auto verified = I::Input->GetVerifiedCmd(iSequence);

	if (!pCmd || !verified || !pCmd->iCommandNumber)
		return;

  	/*
	 * check is called from CInput::CreateMove
	 * and SetLocalViewAngles for engine/prediction at the same time
	 * cuz SetViewAngles isn't called if return false and can cause frame stuttering
	 */
	//I::Prediction->SetLocalViewAngles(pCmd->angViewPoint);
  
  
	// save global cmd pointer
	G::pCmd = pCmd;

	if (I::ClientState == nullptr || I::Engine->IsPlayingDemo())
		return;

	// netchannel pointer
	INetChannel* pNetChannel = I::ClientState->pNetChannel;

	// save previous view angles for movement correction
	QAngle angOldViewPoint = pCmd->angViewPoint;

	SEH_START

	// @note: need do bunnyhop and other movements before prediction
	CMiscellaneous::Get().Run(pCmd, pLocal, bSendPacket);
	
	/*
	 * CL_RunPrediction
	 * correct prediction when framerate is lower than tickrate
	 * https://github.com/VSES/SourceEngine2007/blob/master/se2007/engine/cl_pred.cpp#L41
	 */
	if (I::ClientState->iDeltaTick > 0)
		I::Prediction->Update(I::ClientState->iDeltaTick, I::ClientState->iDeltaTick > 0, I::ClientState->iLastCommandAck, I::ClientState->iLastOutgoingCommand + I::ClientState->nChokedCommands);

	CPrediction::Get().Start(pCmd, pLocal);
	{   
		if (C::Get<bool>(Vars.bMiscAutoPistol))      
			CMiscellaneous::Get().AutoPistol(pCmd, pLocal);

		if (C::Get<bool>(Vars.bMiscFakeLag) || C::Get<bool>(Vars.bAntiAim))
			CMiscellaneous::Get().FakeLag(pLocal, bSendPacket);

		if (C::Get<bool>(Vars.bRage))
			CRageBot::Get().Run(pCmd, pLocal, bSendPacket);

		if (C::Get<bool>(Vars.bLegit))
			CLegitBot::Get().Run(pCmd, pLocal, bSendPacket);

		if (C::Get<bool>(Vars.bTrigger))
			CTriggerBot::Get().Run(pCmd, pLocal);

		if (C::Get<bool>(Vars.bAntiAim))
		{
			CAntiAim::Get().UpdateServerAnimations(G::pCmd, pLocal);
			CAntiAim::Get().UpdateFakeAnimations(G::pCmd, pLocal);
		}

		if (C::Get<bool>(Vars.bAntiAim))
			CAntiAim::Get().Run(pCmd, pLocal, bSendPacket);
	}
	CPrediction::Get().End(pCmd, pLocal);

	if (pLocal->IsAlive())
		CMiscellaneous::Get().MovementCorrection(pCmd, angOldViewPoint);

	// clamp & normalize view angles
	if (C::Get<bool>(Vars.bMiscAntiUntrusted))
	{
		pCmd->angViewPoint.Normalize();
		pCmd->angViewPoint.Clamp();
	}

	if (C::Get<bool>(Vars.bMiscPingSpike))
		CLagCompensation::Get().UpdateIncomingSequences(pNetChannel);
	else
		CLagCompensation::Get().ClearIncomingSequences();

	// @note: doesnt need rehook cuz detours here
	if (pNetChannel != nullptr)
	{
		if (!DTR::SendNetMsg.IsHooked())
			DTR::SendNetMsg.Create(MEM::GetVFunc(pNetChannel, VTABLE::SENDNETMSG), &hkSendNetMsg);

		if (!DTR::SendDatagram.IsHooked())
			DTR::SendDatagram.Create(MEM::GetVFunc(pNetChannel, VTABLE::SENDDATAGRAM), &hkSendDatagram);
	}

	// save next view angles state
	G::angRealView = pCmd->angViewPoint;

	// store next tick view angles state
	G::bSendPacket = bSendPacket;

	// @note: i seen many times this mistake and please do not set/clamp angles here cuz u get confused with psilent aimbot later!

	verified->userCmd = *pCmd;
	verified->uHashCRC = pCmd->GetChecksum();

	SEH_END

}
