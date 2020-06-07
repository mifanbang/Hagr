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

#include <windows.h>
#include <xinput.h>



// declaration for the function removed or deprecated in newer SDKs
#if _WIN32_WINNT >= _WIN32_WINNT_WIN8

	extern "C" DWORD __stdcall XInputGetDSoundAudioDeviceGuids
	(
		DWORD dwUserIndex,
		__out GUID* pDSoundRenderGuid,
		__out GUID* pDSoundCaptureGuid
	);

	extern "C" void __stdcall XInputEnable_NoDeprecation
	(
		_In_ BOOL enable
	);

#endif  // _WIN32_WINNT >= _WIN32_WINNT_WIN8



namespace
{

	decltype(XInputGetState)* g_fpGetState = nullptr;
	decltype(XInputSetState)* g_fpSetState = nullptr;
	decltype(XInputGetCapabilities)* g_fpGetCapabilities = nullptr;
	decltype(XInputEnable_NoDeprecation)* g_fpEnable = nullptr;
	decltype(XInputGetAudioDeviceIds)* g_fpGetAudioDeviceIds = nullptr;
	decltype(XInputGetBatteryInformation)* g_fpGetBatteryInformation = nullptr;
	decltype(XInputGetKeystroke)* g_fpGetKeystroke = nullptr;
	decltype(XInputGetDSoundAudioDeviceGuids)* g_fpGetDSoundAudioDeviceGuids = nullptr;

	template <typename T>
	void AssignFuncPtr(T& funcPtr, FARPROC farproc)
	{
		funcPtr = reinterpret_cast<T>(farproc);
	}

}  // unnamed namespace



extern "C"
{


DWORD __stdcall _XInputGetState(DWORD dwUserIndex, __out XINPUT_STATE* pState)
{
	return g_fpGetState ? g_fpGetState(dwUserIndex, pState) : ERROR_DEVICE_NOT_CONNECTED;
}


DWORD __stdcall _XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
	return g_fpSetState ? g_fpSetState(dwUserIndex, pVibration) : ERROR_DEVICE_NOT_CONNECTED;
}


DWORD __stdcall _XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, __out XINPUT_CAPABILITIES* pCapabilities)
{
	return g_fpGetCapabilities ? g_fpGetCapabilities(dwUserIndex, dwFlags, pCapabilities) : ERROR_DEVICE_NOT_CONNECTED;
}


void __stdcall _XInputEnable(BOOL enable)
{
	if (g_fpEnable)
		g_fpEnable(enable);
}


DWORD __stdcall _XInputGetAudioDeviceIds(
	DWORD dwUserIndex,
	__out_ecount_opt(*pRenderCount) LPWSTR pRenderDeviceId,
	__inout_opt UINT* pRenderCount,
	__out_ecount_opt(*pCaptureCount) LPWSTR pCaptureDeviceId,
	__inout_opt UINT* pCaptureCount)
{
	return g_fpGetAudioDeviceIds ? g_fpGetAudioDeviceIds(dwUserIndex, pRenderDeviceId, pRenderCount, pCaptureDeviceId, pCaptureCount) : ERROR_DEVICE_NOT_CONNECTED;
}


DWORD __stdcall _XInputGetBatteryInformation(
	DWORD dwUserIndex,
	BYTE devType,
	__out XINPUT_BATTERY_INFORMATION* pBatteryInformation)
{
	return g_fpGetBatteryInformation ? g_fpGetBatteryInformation(dwUserIndex, devType, pBatteryInformation) : ERROR_DEVICE_NOT_CONNECTED;
}


DWORD __stdcall _XInputGetKeystroke(
	DWORD dwUserIndex,
	__reserved DWORD dwReserved,
	__out XINPUT_KEYSTROKE* pKeystroke)
{
	return g_fpGetKeystroke ? g_fpGetKeystroke(dwUserIndex, dwReserved, pKeystroke) : ERROR_DEVICE_NOT_CONNECTED;
}


DWORD __stdcall _XInputGetDSoundAudioDeviceGuids(
	DWORD dwUserIndex,
	__out GUID* pDSoundRenderGuid,
	__out GUID* pDSoundCaptureGuid)
{
	return g_fpGetDSoundAudioDeviceGuids ? g_fpGetDSoundAudioDeviceGuids(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid) : ERROR_DEVICE_NOT_CONNECTED;
}


}  // extern "C"


__declspec(dllexport) BOOL __stdcall DllMain([[maybe_unused]] HINSTANCE hinstDLL, DWORD fdwReason, [[maybe_unused]] void* lpvReserved)
{
	constexpr wchar_t pathHagrImplDll[] = L"xinput1_4.dll";  // the DLL which owns Hagr implementation

	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (HMODULE hMod = LoadLibraryW(pathHagrImplDll))
		{
			AssignFuncPtr(g_fpGetState, GetProcAddress(hMod, "XInputGetState"));
			AssignFuncPtr(g_fpSetState, GetProcAddress(hMod, "XInputSetState"));
			AssignFuncPtr(g_fpGetCapabilities, GetProcAddress(hMod, "XInputGetCapabilities"));
			AssignFuncPtr(g_fpEnable, GetProcAddress(hMod, "XInputEnable"));
			AssignFuncPtr(g_fpGetAudioDeviceIds, GetProcAddress(hMod, "XInputGetAudioDeviceIds"));
			AssignFuncPtr(g_fpGetBatteryInformation, GetProcAddress(hMod, "XInputGetBatteryInformation"));
			AssignFuncPtr(g_fpGetKeystroke, GetProcAddress(hMod, "XInputGetKeystroke"));
			AssignFuncPtr(g_fpGetDSoundAudioDeviceGuids, GetProcAddress(hMod, "XInputGetDSoundAudioDeviceGuids"));
		}
	}

	return TRUE;
}



// export configuration sets
// x64 uses undecorated names while x86 uses decorated ones

#if (TARGET_XINPUT_VER_1_3 + TARGET_XINPUT_VER_9_1_0 + TARGET_XINPUT_VER_UAP) != 1
	#error Must define exactly one of the followings: TARGET_XINPUT_VER_1_3=1, TARGET_XINPUT_VER_9_1=1, or TARGET_XINPUT_VER_UAP=1.
#endif

#if TARGET_XINPUT_VER_1_3 == 1
	#if defined _WIN64
		#pragma comment(linker, "/export:DllMain,@1")
		#pragma comment(linker, "/export:XInputGetState=_XInputGetState,@2")
		#pragma comment(linker, "/export:XInputSetState=_XInputSetState,@3")
		#pragma comment(linker, "/export:XInputGetCapabilities=_XInputGetCapabilities,@4")
		#pragma comment(linker, "/export:XInputEnable=_XInputEnable,@5")
		#pragma comment(linker, "/export:XInputGetDSoundAudioDeviceGuids=_XInputGetDSoundAudioDeviceGuids,@6")
		#pragma comment(linker, "/export:XInputGetBatteryInformation=_XInputGetBatteryInformation,@7")
		#pragma comment(linker, "/export:XInputGetKeystroke=_XInputGetKeystroke,@8")
	#else
		#pragma comment(linker, "/export:_DllMain@12,@1")
		#pragma comment(linker, "/export:XInputGetState=__XInputGetState@8,@2")
		#pragma comment(linker, "/export:XInputSetState=__XInputSetState@8,@3")
		#pragma comment(linker, "/export:XInputGetCapabilities=__XInputGetCapabilities@12,@4")
		#pragma comment(linker, "/export:XInputEnable=__XInputEnable@4,@5")
		#pragma comment(linker, "/export:XInputGetDSoundAudioDeviceGuids=__XInputGetDSoundAudioDeviceGuids@12,@6")
		#pragma comment(linker, "/export:XInputGetBatteryInformation=__XInputGetBatteryInformation@12,@7")
		#pragma comment(linker, "/export:XInputGetKeystroke=__XInputGetKeystroke@12,@8")
	#endif

#elif TARGET_XINPUT_VER_9_1_0 == 1
	#if defined _WIN64
		#pragma comment(linker, "/export:DllMain,@1")
		#pragma comment(linker, "/export:XInputGetCapabilities=_XInputGetCapabilities,@2")
		#pragma comment(linker, "/export:XInputGetDSoundAudioDeviceGuids=_XInputGetDSoundAudioDeviceGuids,@3")
		#pragma comment(linker, "/export:XInputGetState=_XInputGetState,@4")
		#pragma comment(linker, "/export:XInputSetState=_XInputSetState,@5")
	#else
		#pragma comment(linker, "/export:_DllMain@12,@1")
		#pragma comment(linker, "/export:XInputGetCapabilities=__XInputGetCapabilities@12,@2")
		#pragma comment(linker, "/export:XInputGetDSoundAudioDeviceGuids=__XInputGetDSoundAudioDeviceGuids@12,@3")
		#pragma comment(linker, "/export:XInputGetState=__XInputGetState@8,@4")
		#pragma comment(linker, "/export:XInputSetState=__XInputSetState@8,@5")
	#endif

#elif TARGET_XINPUT_VER_UAP == 1
	#if defined _WIN64
		#pragma comment(linker, "/export:DllMain,@1")
		#pragma comment(linker, "/export:XInputEnable=_XInputEnable,@2")
		#pragma comment(linker, "/export:XInputGetAudioDeviceIds=_XInputGetAudioDeviceIds,@3")
		#pragma comment(linker, "/export:XInputGetBatteryInformation=_XInputGetBatteryInformation,@4")
		#pragma comment(linker, "/export:XInputGetCapabilities=_XInputGetCapabilities,@5")
		#pragma comment(linker, "/export:XInputGetKeystroke=_XInputGetKeystroke,@6")
		#pragma comment(linker, "/export:XInputGetState=_XInputGetState,@7")
		#pragma comment(linker, "/export:XInputSetState=_XInputSetState,@8")
	#else
		#pragma comment(linker, "/export:_DllMain@12,@1")
		#pragma comment(linker, "/export:XInputEnable=__XInputEnable@4,@2")
		#pragma comment(linker, "/export:XInputGetAudioDeviceIds=__XInputGetAudioDeviceIds@20,@3")
		#pragma comment(linker, "/export:XInputGetBatteryInformation=__XInputGetBatteryInformation@12,@4")
		#pragma comment(linker, "/export:XInputGetCapabilities=__XInputGetCapabilities@12,@5")
		#pragma comment(linker, "/export:XInputGetKeystroke=__XInputGetKeystroke@12,@6")
		#pragma comment(linker, "/export:XInputGetState=__XInputGetState@8,@7")
		#pragma comment(linker, "/export:XInputSetState=__XInputSetState@8,@8")
	#endif

#endif
