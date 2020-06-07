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

#include "ProInternals.h"


bool Packet::IsTypeValid() const
{
	switch (type)
	{
		case PacketType::Host_RumbleAndSubcommand:
		case PacketType::Host_Rumble:
		case PacketType::Host_Command:
		case PacketType::Device_SubcommandReply:
		case PacketType::Device_FullStates:
		case PacketType::Device_CommandReply:
			return true;
		default:
			return false;
	}
}

