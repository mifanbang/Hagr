/*
 *  hagr - bridging Nintendo Switch Pro controller and XInput
 *  Copyright (C) 2020 Mifan Bang <https://debug.tw>.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef _DEBUG
	#include <cstdio>
	#define dbgPrint	printf
#else
	#define dbgPrint
#endif  // _DEBUG

#include <windows.h>
#include <hidusage.h>
#include <xinput.h>

#include "Pro.h"



namespace
{


// Unity may be pulling data from raw input interface provided by User32.dll.
// it may thus interfere with Hagr so we must disable it.
class RawInputDisabler
{
public:
	RawInputDisabler()
	{
		// unregister raw input for joystick devices (HID code of Pro is a joystick rather than a gamepad)
		RAWINPUTDEVICE inputDev { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_JOYSTICK, RIDEV_REMOVE, nullptr };
		RegisterRawInputDevices(&inputDev, 1, sizeof(inputDev));
	}
};


ProAgent& GetProAgent()
{
	static RawInputDisabler s_rawInputDisabler;
	static ProAgent s_proAgent;
	return s_proAgent;
}


}  // unnames namespace


extern "C"
{


DWORD __stdcall _XInputGetState(
	DWORD dwUserIndex,
	__out XINPUT_STATE* pState)
{
	ProAgent& proAgent = GetProAgent();

	if (!proAgent.IsDeviceValid() || dwUserIndex > 0)
	{
		dbgPrint("XInputGetState disconnected %d\n", dwUserIndex);
		return ERROR_DEVICE_NOT_CONNECTED;
	}

	proAgent.WaitForFirstCachedState();

	const bool result = proAgent.GetCachedState(*pState);
	dbgPrint("XInputGetState %d %04X %08X\n", result, pState->dwPacketNumber, pState->Gamepad.wButtons);

	// some games stop pulling states once an non-zero value is returned.
	// thus for the cases of no available cached states while device is connected,
	// we'd want to still report a success but return a neutral gamepad state.
	if (!result)
		ZeroMemory(pState, sizeof(*pState));
	return NO_ERROR;
}


DWORD __stdcall _XInputSetState(
	DWORD dwUserIndex,
	[[maybe_unused]] XINPUT_VIBRATION* pVibration)
{
	ProAgent& proAgent = GetProAgent();

	dbgPrint("XInputSetState %d\n", dwUserIndex);

	if (!proAgent.IsDeviceValid() || dwUserIndex > 0)
		return ERROR_DEVICE_NOT_CONNECTED;

	return NO_ERROR;
}


DWORD __stdcall _XInputGetCapabilities(
	DWORD dwUserIndex,
	[[maybe_unused]] DWORD dwFlags,
	__out XINPUT_CAPABILITIES* pCapabilities)
{
	ProAgent& proAgent = GetProAgent();

	dbgPrint("XInputGetCapabilities\n");

	if (!proAgent.IsDeviceValid() || dwUserIndex > 0)
		return ERROR_DEVICE_NOT_CONNECTED;

	// values read from a real Xbox One controller connected with USB cable
	pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
	pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
	pCapabilities->Flags = 0;
	pCapabilities->Gamepad.wButtons = 0xF3FF;
	pCapabilities->Gamepad.bLeftTrigger = 0xFF;
	pCapabilities->Gamepad.bRightTrigger = 0xFF;
	pCapabilities->Gamepad.sThumbLX = static_cast<SHORT>(0xFFC0);
	pCapabilities->Gamepad.sThumbLY = static_cast<SHORT>(0xFFC0);
	pCapabilities->Gamepad.sThumbRX = static_cast<SHORT>(0xFFC0);
	pCapabilities->Gamepad.sThumbRY = static_cast<SHORT>(0xFFC0);
	pCapabilities->Vibration.wLeftMotorSpeed = 0xFF;
	pCapabilities->Vibration.wRightMotorSpeed = 0xFF;

	return NO_ERROR;
}


void __stdcall _XInputEnable(BOOL enable)
{
	dbgPrint("XInputEnable %d\n", enable);
}


DWORD __stdcall _XInputGetAudioDeviceIds(
	DWORD dwUserIndex,
	[[maybe_unused]] __out_ecount_opt(*pRenderCount) LPWSTR pRenderDeviceId,
	[[maybe_unused]] __inout_opt UINT* pRenderCount,
	[[maybe_unused]] __out_ecount_opt(*pCaptureCount) LPWSTR pCaptureDeviceId,
	[[maybe_unused]] __inout_opt UINT* pCaptureCount)
{
	ProAgent& proAgent = GetProAgent();

	dbgPrint("XInputGetAudioDeviceIds\n");

	if (!proAgent.IsDeviceValid() || dwUserIndex > 0)
		return ERROR_DEVICE_NOT_CONNECTED;

	return ERROR_DEVICE_NOT_CONNECTED;
}


DWORD __stdcall _XInputGetBatteryInformation(
	DWORD dwUserIndex,
	BYTE devType,
	__out XINPUT_BATTERY_INFORMATION* pBatteryInformation)
{
	ProAgent& proAgent = GetProAgent();

	if (!proAgent.IsDeviceValid() || dwUserIndex > 0 || devType != BATTERY_DEVTYPE_GAMEPAD)
	{
		dbgPrint("XInputGetBatteryInformation disconnected %d\n", dwUserIndex);
		return ERROR_DEVICE_NOT_CONNECTED;
	}

	proAgent.WaitForFirstCachedState();

	const bool result = proAgent.GetBatteryInfo(*pBatteryInformation);
	dbgPrint("XInputGetBatteryInformation %d %02X %02X\n", result, pBatteryInformation->BatteryType, pBatteryInformation->BatteryLevel);

	// for the same reason as in XInputGetState(), we fake the battery state
	if (!result)
	{
		pBatteryInformation->BatteryType = BATTERY_TYPE_NIMH;
		pBatteryInformation->BatteryLevel = BATTERY_LEVEL_MEDIUM;
	}
	return NO_ERROR;
}


DWORD __stdcall _XInputGetKeystroke(
	DWORD dwUserIndex,
	[[maybe_unused]] __reserved DWORD dwReserved,
	[[maybe_unused]] __out XINPUT_KEYSTROKE* pKeystroke)
{
	ProAgent& proAgent = GetProAgent();

	dbgPrint("XInputGetKeystroke\n");

	if (!proAgent.IsDeviceValid() || dwUserIndex > 0)
		return ERROR_DEVICE_NOT_CONNECTED;

	return ERROR_EMPTY;  // we basically don't support this function
}


DWORD __stdcall _XInputGetDSoundAudioDeviceGuids(
	[[maybe_unused]] DWORD dwUserIndex,
	[[maybe_unused]] __out GUID* pDSoundRenderGuid,
	[[maybe_unused]] __out GUID* pDSoundCaptureGuid)
{
	dbgPrint("XInputGetDSoundAudioDeviceGuids\n");

	return ERROR_DEVICE_NOT_CONNECTED;
}


}  // extern "C"


__declspec(dllexport) BOOL __stdcall DllMain(
	[[maybe_unused]] HINSTANCE hinstDLL,
	DWORD fdwReason,
	[[maybe_unused]] void* lpReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
#if _DEBUG
		FILE* fp;
		AllocConsole();
		freopen_s(&fp, "CONIN$", "r+t", stdin);
		freopen_s(&fp, "CONOUT$", "w+t", stdout);
		freopen_s(&fp, "CONOUT$", "w+t", stderr);
#endif
	}

	return TRUE;
}



// export configuration sets
// x64 uses undecorated names while x86 uses decorated ones
#if defined _WIN64
	#pragma comment(linker, "/export:DllMain,@1")
	#pragma comment(linker, "/export:XInputGetState=_XInputGetState,@2")
	#pragma comment(linker, "/export:XInputSetState=_XInputSetState,@3")
	#pragma comment(linker, "/export:XInputGetCapabilities=_XInputGetCapabilities,@4")
	#pragma comment(linker, "/export:XInputEnable=_XInputEnable,@5")
	#pragma comment(linker, "/export:XInputGetAudioDeviceIds=_XInputGetAudioDeviceIds,@6")
	#pragma comment(linker, "/export:XInputGetBatteryInformation=_XInputGetBatteryInformation,@7")
	#pragma comment(linker, "/export:XInputGetKeystroke=_XInputGetKeystroke,@8")
	#pragma comment(linker, "/export:XInputGetDSoundAudioDeviceGuids=_XInputGetDSoundAudioDeviceGuids,@9")
#else
	#pragma comment(linker, "/export:DllMain=_DllMain@12,@1")
	#pragma comment(linker, "/export:XInputGetState=__XInputGetState@8,@2")
	#pragma comment(linker, "/export:XInputSetState=__XInputSetState@8,@3")
	#pragma comment(linker, "/export:XInputGetCapabilities=__XInputGetCapabilities@12,@4")
	#pragma comment(linker, "/export:XInputEnable=__XInputEnable@4,@5")
	#pragma comment(linker, "/export:XInputGetAudioDeviceIds=__XInputGetAudioDeviceIds@20,@6")
	#pragma comment(linker, "/export:XInputGetBatteryInformation=__XInputGetBatteryInformation@12,@7")
	#pragma comment(linker, "/export:XInputGetKeystroke=__XInputGetKeystroke@12,@8")
	#pragma comment(linker, "/export:XInputGetDSoundAudioDeviceGuids=__XInputGetDSoundAudioDeviceGuids@12,@9")
#endif
