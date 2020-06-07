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

#include <cstdio>

#include <windows.h>
#include <xinput.h>

#pragma comment(lib, "xinput.lib")



void EnableConsoleColoring(HANDLE console)
{
	DWORD consoleMode;
	if (GetConsoleMode(console, &consoleMode))
		SetConsoleMode(console, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}


int main()
{
	constexpr DWORD k_playerID = 0;
	constexpr DWORD k_sleepInterval = 16;
	constexpr COORD k_cursorOrigin = { 0, 0 };

	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	EnableConsoleColoring(console);

	XINPUT_STATE state;
	XINPUT_BATTERY_INFORMATION batteryInfo;
	while (true)
	{
		SetConsoleCursorPosition(console, k_cursorOrigin);

		ZeroMemory(&state, sizeof(state));
		ZeroMemory(&batteryInfo, sizeof(batteryInfo));
		const DWORD getStateResult = XInputGetState(k_playerID, &state);
		XInputGetBatteryInformation(k_playerID, BATTERY_DEVTYPE_GAMEPAD, &batteryInfo);

		printf("%sResult code: %08X\033[0m\n",
			getStateResult == 0 ? "\033[0;32m" : "\033[0;31m",  // color
			getStateResult
		);
		printf("System tick: %lld\n"
			"Input states:\n"
			"    Timestamp = %02X\n"
			"    Buttons = %04X\n"
			"    Left trigger = %3d\n"
			"    Right trigger = %3d\n"
			"    Left thumbstick = (%+6d, %+6d)\n"
			"    Right thumbstick = (%+6d, %+6d)\n"
			"Battery info:\n"
			"    Type = %02X\n"
			"    Level = %02X\n",
			GetTickCount64(),
			state.dwPacketNumber,
			state.Gamepad.wButtons,
			state.Gamepad.bLeftTrigger,
			state.Gamepad.bRightTrigger,
			state.Gamepad.sThumbLX, state.Gamepad.sThumbLY,
			state.Gamepad.sThumbRX, state.Gamepad.sThumbRY,
			batteryInfo.BatteryType,
			batteryInfo.BatteryLevel);

		Sleep(k_sleepInterval);
	}

	return NO_ERROR;
}
