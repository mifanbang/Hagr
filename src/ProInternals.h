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

#include <cstdint>
#include <tuple>



#pragma pack(push, nspro, 1)



// ----------------------------------------------------------------------------
// data types -----------------------------------------------------------------

struct UInt24
{
	uint8_t bytes[3];

	__forceinline operator uint32_t () const
	{
		return (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];  // little endian
	}

	__forceinline std::tuple<uint16_t, uint16_t> Split() const
	{
		return {
			static_cast<uint16_t>(bytes[0] | ((bytes[1] & 0xF) << 8)),
			static_cast<uint16_t>((bytes[2] << 4) | (bytes[1] >> 4))
		};
	}
};


enum class Buttons
{
	Y = 0,
	X,
	B,
	A,
	// [4, 5] unmapped
	R = 6,
	ZR,
	Minus,
	Plus,
	TriggerR,
	TriggerL,
	Home,
	Share,
	// [14, 15] unmapped
	Down = 16,
	Up,
	Right,
	Left,
	// [20, 21] unmapped
	L = 22,
	ZL
};



// ----------------------------------------------------------------------------
// subpackets sent from host --------------------------------------------------

namespace HostSubPacket
{
	enum class SubcommandCode : uint8_t
	{
		SetPlayerLights = 0x30,
		SetIMUSensitivity = 0x41,
	};

	enum class CommandCode : uint8_t
	{
		HandShake = 0x02,
		SetHighSpeed = 0x03,
		ForceUSB = 0x04
	};

	struct RumbleParam
	{
		uint8_t highFreq;
		uint8_t highFreqAmp;
		uint8_t lowFreq;
		uint8_t lowFreqAmp;

		constexpr static RumbleParam Neutral()
		{
			return { 0x00, 0x01, 0x40, 0x40 };
		}
	};


	// 0x01
	struct RumbleAndSubcommand
	{
		uint8_t serialId;
		RumbleParam left;
		RumbleParam right;
		SubcommandCode subcmdCode;
		uint32_t subcmdData;
	};

	// 0x10
	struct Rumble
	{
		RumbleParam left;
		RumbleParam right;
	};

	// 0x80
	struct Command
	{
		CommandCode cmdCode;
	};

}  // namespace HostSubPacket



// ----------------------------------------------------------------------------
// subpackets sent from client ------------------------------------------------

namespace DeviceSubPacket
{
	// shared data structure embedded in multiple types of packet
	struct CommonStates
	{
		uint8_t timestamp;
		uint8_t batteryAndWired;
		UInt24 keys;
		UInt24 leftStick;
		UInt24 rightStick;
		uint8_t vibration;
	};

	// 0x21
	struct SubcommandReply : CommonStates
	{
		uint8_t subcmdAck;  // success if bit index 7 is set(?)
		HostSubPacket::SubcommandCode subcmdCode;  // same as subcommand code sent in RumbleAndSubcommand packet
		uint32_t data;  // unknown
	};

	// 0x30
	struct FullStates : CommonStates
	{
		// no additional fields
	};

	// 0x81
	struct CommandReply
	{
		HostSubPacket::CommandCode cmdCode;
	};
}  // namespace DeviceSubPacket



// ----------------------------------------------------------------------------
// packet ---------------------------------------------------------------------

enum class PacketType : uint8_t
{
	Host_RumbleAndSubcommand = 0x01,
	Host_Rumble = 0x10,
	Host_Command = 0x80,
	Device_SubcommandReply = 0x21,  // reply to Host_RumbleAndSubcommand
	Device_FullStates = 0x30,
	Device_CommandReply = 0x81,  // reply to Host_Command
};

// type traits for all subpackets
template <PacketType Type> struct ToPacketType { };

#define DEFINE_PACKET_TYPE_MAPPING(enumValue, typeStruct)	\
	template <> struct ToPacketType<enumValue>				\
	{ using type = typeStruct; };

DEFINE_PACKET_TYPE_MAPPING(PacketType::Host_RumbleAndSubcommand, HostSubPacket::RumbleAndSubcommand);
DEFINE_PACKET_TYPE_MAPPING(PacketType::Host_Rumble, HostSubPacket::Rumble);
DEFINE_PACKET_TYPE_MAPPING(PacketType::Host_Command, HostSubPacket::Command);
DEFINE_PACKET_TYPE_MAPPING(PacketType::Device_SubcommandReply, DeviceSubPacket::SubcommandReply);
DEFINE_PACKET_TYPE_MAPPING(PacketType::Device_FullStates, DeviceSubPacket::FullStates);
DEFINE_PACKET_TYPE_MAPPING(PacketType::Device_CommandReply, DeviceSubPacket::CommandReply);
#undef DEFINE_PACKET_TYPE_MAPPING


struct Packet
{
	PacketType type;
	union
	{
		HostSubPacket::RumbleAndSubcommand rumbleAndSubcommand;
		HostSubPacket::Rumble rumble;
		HostSubPacket::Command command;

		DeviceSubPacket::SubcommandReply subcommandReply;
		DeviceSubPacket::FullStates fullStates;
		DeviceSubPacket::CommandReply commandReply;

		uint8_t unused[63];
	};

	bool IsTypeValid() const;

	// subpacket getters - deleted by default
	template <PacketType T> typename ToPacketType<T>::type& GetSubPacket() = delete;
	template <PacketType T> const typename ToPacketType<T>::type& GetSubPacket() const = delete;

	// subpacket getter specializations
#define DEFINE_SUBPACKET_GETTER(enumType, memberName)	\
	template<> ToPacketType<enumType>::type& GetSubPacket<enumType>() { return memberName; }	\
	template<> const ToPacketType<enumType>::type& GetSubPacket<enumType>() const { return memberName; }

	DEFINE_SUBPACKET_GETTER(PacketType::Host_RumbleAndSubcommand, rumbleAndSubcommand);
	DEFINE_SUBPACKET_GETTER(PacketType::Host_Rumble, rumble);
	DEFINE_SUBPACKET_GETTER(PacketType::Host_Command, command);
	DEFINE_SUBPACKET_GETTER(PacketType::Device_SubcommandReply, subcommandReply);
	DEFINE_SUBPACKET_GETTER(PacketType::Device_FullStates, fullStates);
	DEFINE_SUBPACKET_GETTER(PacketType::Device_CommandReply, commandReply);
#undef DEFINE_SUBPACKET_GETTER

	constexpr size_t GetSize()
	{
		return sizeof(Packet);
	}
};

static_assert(sizeof(Packet) == 64);



#pragma pack(pop, nspro)

