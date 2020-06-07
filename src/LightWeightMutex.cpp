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

#include "LightWeightMutex.h"

#include <windows.h>


LWMutex::LWMutex()
	: m_cs(new CRITICAL_SECTION)
{
	InitializeCriticalSection(m_cs.get());
}

LWMutex::~LWMutex()
{
	DeleteCriticalSection(m_cs.get());
}

void LWMutex::lock()
{
	EnterCriticalSection(m_cs.get());
}

bool LWMutex::try_lock()
{
	return TryEnterCriticalSection(m_cs.get()) != FALSE;
}

void LWMutex::unlock()
{
	LeaveCriticalSection(m_cs.get());
}
