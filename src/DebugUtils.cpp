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

#include "DebugUtils.h"

#include <iomanip>
#include <sstream>

#include "Pipes.h"
#include "ProInternals.h"



void DebugOutputString([[maybe_unused]] const wchar_t* str)
{
#ifdef _DEBUG
	OutputDebugStringW(str);
#endif  // _DEBUG
}


void DebugOutputPacket([[maybe_unused]] const Buffer& buffer)
{
#ifdef _DEBUG
	unsigned int packetIdx = 0;
	const auto& funcDumpPacket = [&packetIdx](const Packet& packet) {
		std::ostringstream oss;

		oss << packetIdx << ": ";
		for (unsigned int i = 0; i < sizeof(packet); ++i)
			oss << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << static_cast<unsigned int>(reinterpret_cast<const uint8_t*>(&packet)[i]) << " ";
		oss << std::endl;
		OutputDebugStringA(oss.str().c_str());

		++packetIdx;
		return true;
	};

	IterateBuffer<Packet>(buffer, funcDumpPacket);
#endif  // _DEBUG
}
