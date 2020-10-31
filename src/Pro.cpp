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

#include "Pro.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>

#include <windows.h>
#include <initguid.h>
#include <hidclass.h>
#include <setupapi.h>

#include "DebugUtils.h"
#include "Pipes.h"
#include "ProInternals.h"
#include "SteadyTimer.h"


#pragma comment(lib, "setupapi.lib")



namespace
{


constexpr DeviceIoPipes::PipeParams k_pipeParams = { 128, 64 };  // read = 128 B; write = 64 B
constexpr unsigned int k_pullInterval = 15;  // ms; ~60 ticks per second seem like Pro's spec
constexpr uint64_t k_packetTimeout = 100;  // ms; for how long the cached states are considered invalid and controller disconnected
constexpr std::chrono::milliseconds k_cmdReplyTimeout(400);  // for how long we wait for device to reply to a certain command
constexpr uint8_t k_batteryType = BATTERY_TYPE_NIMH;  // doesn't really matter so we hard-code this


template <typename F>
class Tearoff
{
public:
	__forceinline Tearoff(const F& func, unsigned int count)
		: m_func(func)
		, m_count(count)
	{ }

	__forceinline bool IsAvailable() const
	{
		return m_count > 0;
	}

	template <typename... Args>
	typename std::invoke_result<F, Args...>::type operator () (Args&&... args)
	{
		assert(m_count > 0);
		const auto result = m_func(std::forward<Args>(args)...);
		const auto lastCount = m_count;
		--m_count;
		return result;
	}

	template <typename... Args>
	std::optional<typename std::invoke_result<F, Args...>::type> RunSafe(Args&&... args)
	{
		return IsAvailable() ? std::make_optional(operator()(std::forward<Args>(args)...)) : std::nullopt;
	}

private:
	const F& m_func;
	unsigned int m_count;
};


std::wstring FindDevicePath()
{
	constexpr wchar_t k_devicePathSigPro[] = L"hid#vid_057e&pid_2009";

	std::wstring foundPath;

	HDEVINFO hDevInfoList = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_HID, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hDevInfoList == INVALID_HANDLE_VALUE)
		return foundPath;

	SP_DEVINFO_DATA devInfoData { sizeof(devInfoData) };
	for (unsigned int i = 0; SetupDiEnumDeviceInfo(hDevInfoList, i, &devInfoData) != FALSE; ++i)
	{
		SP_DEVICE_INTERFACE_DATA devIntfData { sizeof(devIntfData) };
		if (SetupDiEnumDeviceInterfaces(hDevInfoList, &devInfoData, &GUID_DEVINTERFACE_HID, 0, &devIntfData))
		{
			constexpr unsigned int buffSize = 1024;
			uint8_t buffer[buffSize];
			auto* devIntfDetail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer);
			devIntfDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

			if (SetupDiGetDeviceInterfaceDetailW(hDevInfoList, &devIntfData, devIntfDetail, buffSize, nullptr, nullptr) &&
				wcsstr(devIntfDetail->DevicePath, k_devicePathSigPro) != nullptr)
			{
				foundPath = devIntfDetail->DevicePath;
				break;
			}
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfoList);
	return foundPath;
}


HANDLE OpenDevice(const std::wstring& path)
{
	constexpr LPSECURITY_ATTRIBUTES k_noSecurityAttr = nullptr;
	constexpr HANDLE k_noTemplateFile = nullptr;

	return CreateFileW(
		path.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,  // have to let others read and write
		k_noSecurityAttr,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		k_noTemplateFile
	);
}


const Packet* GetLastPacket(const Buffer& buffer)
{
	const Packet* lastGoodPacket = nullptr;
	const auto& funcMatch = [&lastGoodPacket](const Packet& packet) {
		if (packet.type == PacketType::Device_FullStates)
			lastGoodPacket = &packet;
		return true;
	};

	IterateBuffer<Packet>(buffer, funcMatch);
	return lastGoodPacket;
}


template <typename F>
bool ReadUntil(DeviceIoPipes& pipes, const F& func)
{
	bool shouldContinuePulling = true;
	Buffer buffer(pipes.GetReadBufferSize());
	const SteadyTimer timer;
	while (shouldContinuePulling)
	{
		const auto elaspedTime = timer.GetElapsed();
		if (elaspedTime > k_cmdReplyTimeout)
			return false;

		const auto readResult = std::get<Pipe::OpResultCode>(pipes.ReadSync(buffer, k_cmdReplyTimeout - elaspedTime));
		if (readResult != Pipe::OpResultCode::Success)
			return false;  // either an erorr occurred or operation timed out

		DebugOutputPacket(buffer);

		shouldContinuePulling = !IterateBuffer<Packet>(buffer, func);
	}
	return true;
}


bool WaitForDeviceFullStatesPacket(DeviceIoPipes& pipes)
{
	return ReadUntil(pipes, [](const Packet& packet) {
		const bool found = packet.type == PacketType::Device_FullStates;
		return !found;  // return true to continue reading
	} );
}


bool WaitForDeviceCommandReply(DeviceIoPipes& pipes, HostSubPacket::CommandCode cmd)
{
	return ReadUntil(pipes, [cmd](const Packet& packet) {
		const bool found = packet.type == PacketType::Device_CommandReply &&
			packet.GetSubPacket<PacketType::Device_CommandReply>().cmdCode == cmd;
		return !found;  // return true to continue reading
	} );
}


bool WaitForDeviceSubcommandReply(DeviceIoPipes& pipes, HostSubPacket::SubcommandCode subcmd)
{
	return ReadUntil(pipes, [subcmd](const Packet& packet) {
		const bool found = packet.type == PacketType::Device_SubcommandReply &&
			packet.GetSubPacket<PacketType::Device_SubcommandReply>().subcmdCode == subcmd;
		return !found;  // return true to continue reading
	} );
}


bool SendHostCommand(DeviceIoPipes& devPipes, HostSubPacket::CommandCode cmdCode, bool readReply)
{
	constexpr PacketType k_packetType = PacketType::Host_Command;

	Buffer writeBuffer(k_pipeParams.writeBufferSize);
	Packet& packet = writeBuffer;

	ZeroMemory(&packet, sizeof(packet));
	packet.type = k_packetType;
	packet.GetSubPacket<k_packetType>().cmdCode = cmdCode;
	const auto writeResult = devPipes.WriteSync(writeBuffer, Pipe::k_syncInfinite);
	if (std::get<Pipe::OpResultCode>(writeResult) != Pipe::OpResultCode::Success)
		return false;

	if (readReply && !WaitForDeviceCommandReply(devPipes, cmdCode))
		return false;

	return true;
}


bool SendHostSubcommand(DeviceIoPipes& devPipes, HostSubPacket::SubcommandCode subcmdCode, uint8_t serialId, uint32_t subcmdData, bool readReply)
{
	constexpr PacketType k_packetType = PacketType::Host_RumbleAndSubcommand;

	Buffer writeBuffer(k_pipeParams.writeBufferSize);
	Packet& packet = writeBuffer;

	ZeroMemory(&packet, sizeof(packet));
	packet.type = k_packetType;
	auto& rumbleAndSubcmd = packet.GetSubPacket<k_packetType>();
	rumbleAndSubcmd.serialId = serialId;
	rumbleAndSubcmd.left = HostSubPacket::RumbleParam::Neutral();
	rumbleAndSubcmd.right = HostSubPacket::RumbleParam::Neutral();
	rumbleAndSubcmd.subcmdCode = subcmdCode;
	rumbleAndSubcmd.subcmdData = subcmdData;

	const auto writeResult = devPipes.WriteSync(writeBuffer, Pipe::k_syncInfinite);
	if (std::get<Pipe::OpResultCode>(writeResult) != Pipe::OpResultCode::Success)
		return false;

	if (readReply && !WaitForDeviceSubcommandReply(devPipes, subcmdCode))
		return false;

	return true;
}


template <typename T1, typename T2>
bool IsSet(T1 val, T2 index)
{
	return ((static_cast<uint64_t>(val) >> static_cast<uint8_t>(index)) & 1) == 1;
}


class PacketAdaptor
{
public:
	static void Translate(const Packet& packet, __out XINPUT_STATE& outputStates, __out XINPUT_BATTERY_INFORMATION& outputBattery)
	{
		assert(packet.type == PacketType::Device_FullStates);
		const auto& gameStates = packet.GetSubPacket<PacketType::Device_FullStates>();

		outputStates.dwPacketNumber = gameStates.timestamp;

		const auto [leftX, leftY] = gameStates.leftStick.Split();
		const auto [rightX, rightY] = gameStates.rightStick.Split();
		outputStates.Gamepad.sThumbLX = RemapAxis<RamapConfigLeftX>(leftX);
		outputStates.Gamepad.sThumbLY = RemapAxis<RamapConfigLeftY>(leftY);
		outputStates.Gamepad.sThumbRX = RemapAxis<RamapConfigRightX>(rightX);
		outputStates.Gamepad.sThumbRY = RemapAxis<RamapConfigRightY>(rightY);

		const uint32_t buttons = gameStates.keys;
		outputStates.Gamepad.bLeftTrigger = IsSet(buttons, Buttons::ZL) ? 0xFF : 0;  // Unlike XBO, Pro's triggers are binary
		outputStates.Gamepad.bRightTrigger = IsSet(buttons, Buttons::ZR) ? 0xFF : 0;
		outputStates.Gamepad.wButtons = 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Y) ? XINPUT_GAMEPAD_X : 0;  // Pro's X is at the physical position of Xbox's Y
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::X) ? XINPUT_GAMEPAD_Y : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::B) ? XINPUT_GAMEPAD_A : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::A) ? XINPUT_GAMEPAD_B : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::R) ? XINPUT_GAMEPAD_RIGHT_SHOULDER : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Minus) ? XINPUT_GAMEPAD_BACK : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Plus) ? XINPUT_GAMEPAD_START : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::TriggerR) ? XINPUT_GAMEPAD_RIGHT_THUMB : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::TriggerL) ? XINPUT_GAMEPAD_LEFT_THUMB : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Down) ? XINPUT_GAMEPAD_DPAD_DOWN : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Up) ? XINPUT_GAMEPAD_DPAD_UP : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Right) ? XINPUT_GAMEPAD_DPAD_RIGHT : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::Left) ? XINPUT_GAMEPAD_DPAD_LEFT : 0;
		outputStates.Gamepad.wButtons |= IsSet(buttons, Buttons::L) ? XINPUT_GAMEPAD_LEFT_SHOULDER : 0;

		outputBattery.BatteryType = k_batteryType;
		outputBattery.BatteryLevel = DecodeBatteryLevel(gameStates.batteryAndWired);
	}

private:

#define DEFINE_REMAP_CONFIG(name, M, m, neu)	\
	enum class name : int16_t {	\
		max = M,				\
		min = m,				\
		neutral = neu			\
	};
	DEFINE_REMAP_CONFIG(RamapConfigLeftX, 0xE20, 0x220, 0x7E0);
	DEFINE_REMAP_CONFIG(RamapConfigLeftY, 0xE20, 0x1B0, 0x7A0);
	DEFINE_REMAP_CONFIG(RamapConfigRightX, 0xE00, 0x230, 0x800);
	DEFINE_REMAP_CONFIG(RamapConfigRightY, 0xE20, 0x150, 0x770);
#undef DEFINE_REMAP_CONFIG


	template <typename Config>
	static int16_t RemapAxis(uint16_t value)
	{
		constexpr int16_t k_max = static_cast<int16_t>(Config::max);
		constexpr int16_t k_min = static_cast<int16_t>(Config::min);
		constexpr int16_t k_neutral = static_cast<int16_t>(Config::neutral);

		const float signedVal = static_cast<float>(std::clamp(static_cast<int16_t>(value), k_min, k_max) - k_neutral);
		if (signedVal > 0.f)
		{
			constexpr float rangeOrig = k_max - k_neutral;
			constexpr float invRangeOrig = 1.f / rangeOrig;
			return static_cast<int16_t>(signedVal * invRangeOrig * 0x7FFF);
		}
		else if (signedVal < 0.f)
		{
			constexpr float rangeOrig = k_neutral - k_min;
			constexpr float invRangeOrig = 1.f / rangeOrig;
			return static_cast<int16_t>(signedVal * invRangeOrig * 0x8000);
		}
		else
			return 0;
	}

	static uint8_t DecodeBatteryLevel(uint8_t batteryAndWired)
	{
		// 0 is EMPTY; remap the rest [1-8] to [1-3], where BATTERY_LEVEL_LOW=1, MEDIUM=2, FULL=3
		const uint8_t battery = batteryAndWired >> 4;
		if (battery >= 7)
			return BATTERY_LEVEL_FULL;
		else if (battery >= 4)
			return BATTERY_LEVEL_MEDIUM;
		else if (battery >= 1)
			return BATTERY_LEVEL_LOW;
		else
			return BATTERY_LEVEL_EMPTY;
	}
};


}  // unnamed namespace



// ----------------------------------------------------------------------------
// ProAgent definitions -------------------------------------------------------

ProAgent::CachedStates::CachedStates()
	: timestamp(0)
	, mutex()
	, gamepad()
	, battery()
{
	std::unique_lock lock(mutex);

	ZeroMemory(&gamepad, sizeof(gamepad));
	ZeroMemory(&battery, sizeof(battery));
}


ProAgent::ProAgent()
	: m_devPipes(OpenDevice(FindDevicePath()), k_pipeParams)
	, m_cachedStates()
	, m_workerThread()
	, m_workerStopSignal(false)
	, m_deviceTriedFirstPull(false)
{
	InitWorkerThread();
}

ProAgent::~ProAgent()
{
	if (m_workerThread)
	{
		m_workerStopSignal = true;
		m_workerThread->join();
	}
}

// return true if result is cached or being read from device; return false otherwise
bool ProAgent::TryUpdate()
{
	// only try to reattach once in each tick
	Tearoff reattachRequest(std::mem_fn(&ProAgent::ReattachToDevice), 1);

	if (!m_devPipes.IsFileValid() && !reattachRequest(this))
		return false;

	Buffer buffer(k_pipeParams.readBufferSize);
	const auto popResultCode = std::get<Pipe::OpResultCode>(m_devPipes.PopReadResult(buffer));
	if (popResultCode == Pipe::OpResultCode::InvalidFile)
	{
		m_devPipes.Close();
		reattachRequest.RunSafe(this);
		return false;  // we don't have results ready in this tick
	}
	else if (popResultCode == Pipe::OpResultCode::StillExecuting)
	{
		// if PopReadResult() keeps returning StillExecuting, it could mean another process, e.g. Steam, is
		// communicating with the device and somehow forces it into sleep mode.
		if (GetTickCount64() - m_cachedStates.timestamp > k_packetTimeout)
		{
			m_devPipes.Close();
			reattachRequest.RunSafe(this);
			return false;
		}
	}
	else if (popResultCode == Pipe::OpResultCode::Success)
	{
		// issue next read before processing packets to maximize throughput
		const auto readResultCode = std::get<Pipe::OpResultCode>(m_devPipes.Read());

		// now process packets
		if (const auto* packet = GetLastPacket(buffer))
		{
			std::unique_lock lock(m_cachedStates.mutex);
			m_cachedStates.timestamp = GetTickCount64();
			PacketAdaptor::Translate(*packet, m_cachedStates.gamepad, m_cachedStates.battery);
			m_deviceTriedFirstPull = true;
		}

		// handle failed read operation only after caching states
		if (readResultCode == Pipe::OpResultCode::InvalidFile)
		{
			// still return true as this tick successfully updated cache
			m_devPipes.Close();
			reattachRequest.RunSafe(this);
		}
	}
	return true;
}

bool ProAgent::GetCachedState(__out XINPUT_STATE& result) const
{
	std::shared_lock lock(m_cachedStates.mutex);
	result = m_cachedStates.gamepad;
	return (GetTickCount64() - m_cachedStates.timestamp < k_packetTimeout);
}

bool ProAgent::GetBatteryInfo(__out XINPUT_BATTERY_INFORMATION& result) const
{
	std::shared_lock lock(m_cachedStates.mutex);
	result = m_cachedStates.battery;
	return (GetTickCount64() - m_cachedStates.timestamp < k_packetTimeout);
}

bool ProAgent::IsDeviceValid() const
{
	return m_devPipes.IsFileValid();
}

// return true if cached state is available in the end.
// cannot be called on a worker thread.
bool ProAgent::WaitForFirstCachedState() const
{
	// the spinning loop relies on TryUpdate() properly checking packet time-out interval
	// or it could block forever.
	while (!m_deviceTriedFirstPull && m_devPipes.IsFileValid())
		;  // spin

	return m_deviceTriedFirstPull;
}

void ProAgent::InitWorkerThread()
{
	if (m_workerThread)
		return;

	auto* thread = new std::thread(std::mem_fn(&ProAgent::WorkerThreadProc), this);
	m_workerThread.reset(thread);
}

bool ProAgent::ReattachToDevice()
{
	m_deviceTriedFirstPull = false;

	if (AutoHandle newDeviceFile = OpenDevice(FindDevicePath()))
	{
		DeviceIoPipes newDevicePipes(std::move(newDeviceFile), k_pipeParams);
		m_devPipes = std::move(newDevicePipes);
		if (!WaitForDeviceFullStatesPacket(m_devPipes))
		{
			// controller is not in an initialized state. have to reinitialize.
			m_devPipes.CancelRead();  // cancel previous async read op
			return InitDevice();
		}

		return true;
	}
	return false;
}

bool ProAgent::InitDevice()
{
	using HostSubPacket::CommandCode;
	using HostSubPacket::SubcommandCode;

	// raw data: 0x80 0x02
	DebugOutputString(L"HostCommand=HandShake\n");
	if (!SendHostCommand(m_devPipes, CommandCode::HandShake, true))
		return false;

	// raw data: 0x80 0x03
	DebugOutputString(L"HostCommand=SetHighSpeed\n");
	if (!SendHostCommand(m_devPipes, CommandCode::SetHighSpeed, true))
		return false;

	// raw data: 0x80 0x02
	DebugOutputString(L"HostCommand=HandShake\n");
	if (!SendHostCommand(m_devPipes, CommandCode::HandShake, true))
		return false;

	// raw data: 0x08 0x04
	// device doesn't generate reply to this command code
	DebugOutputString(L"HostCommand=ForceUSB\n");
	if (!SendHostCommand(m_devPipes, CommandCode::ForceUSB, false))
		return false;

	// turn on player 0 light
	constexpr uint32_t k_playerLEDIndex = 1;
	DebugOutputString(L"HostSubcommand=Host_RumbleAndSubcommand\n");
	if (!SendHostSubcommand(m_devPipes, SubcommandCode::SetPlayerLights, 1, k_playerLEDIndex, true))
		return false;

	return true;
}

void ProAgent::WorkerThreadProc()
{
	if (m_devPipes.IsFileValid())
		ReattachToDevice();

	while (!m_workerStopSignal)
	{
		TryUpdate();
		Sleep(k_pullInterval);
	}
}

