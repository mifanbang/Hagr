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

#pragma once

#include <shared_mutex>
#include <thread>

#include <windows.h>
#include <xinput.h>

#include "Pipes.h"



class ProAgent
{
public:
	ProAgent();
	~ProAgent();

	bool GetCachedState(__out XINPUT_STATE& result) const;
	bool GetBatteryInfo(__out XINPUT_BATTERY_INFORMATION& result) const;
	bool IsDeviceValid() const;


private:
	void InitWorkerThread();
	bool ReattachToDevice();
	bool InitDevice();  // NS Pro controller needs to be initialized via a private protocol
	bool TryUpdate();
	void WorkerThreadProc();


	struct CachedStates
	{
		// book-keeping
		uint64_t timestamp;
		mutable std::shared_mutex mutex;  // VS' implementation is simple enough so use STL here

		// actual data
		XINPUT_STATE gamepad;
		XINPUT_BATTERY_INFORMATION battery;

		CachedStates();
	};


	DeviceIoPipes m_devPipes;
	CachedStates m_cachedStates;

	std::unique_ptr<std::thread> m_workerThread;
	volatile bool m_workerStopSignal;
};

