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

#define NOMINMAX

#include "Pipes.h"
#include "SteadyTimer.h"

#include <algorithm>
#include <mutex>


class Pipe::Helper
{
public:
	template <typename Func>
	static OpResult ExecuteOp(Pipe& pipe, const Func& func)
	{
		if (!pipe.IsValid())
			return { OpResultCode::InvalidFile, NO_ERROR };
		else if (pipe.IsOpExecuting())
			return { OpResultCode::StillExecuting, NO_ERROR };

		func();

		const DWORD lastError = GetLastError();
		if (lastError == ERROR_IO_PENDING || lastError == NO_ERROR)  // although NO_ERROR isn't expected but we still treat it as a success
			return { OpResultCode::Success, NO_ERROR };
		else
		{
			pipe.m_overlapped->Internal = 0;  // m_overlapped might be set to a bad state
			return { OpResultCode::InvalidFile, lastError };
		}
	}
};



Pipe::Pipe(HANDLE file, unsigned int bufferSize)
	: m_file(file)
	, m_overlapped()
	, m_buffer(new Buffer(bufferSize))
{
	assert(m_buffer != nullptr);

	if (IsHandleValid(m_file))
	{
		m_overlapped.reset(new OVERLAPPED);

		constexpr BOOL k_autoReset = FALSE;  // we want auto-reset
		constexpr BOOL k_initallyCleared = FALSE;
		constexpr wchar_t* k_noEventName = nullptr;
		HANDLE event = CreateEventW(nullptr, k_autoReset, k_initallyCleared, k_noEventName);
		assert(event != NULL);

		ZeroMemory(m_overlapped.get(), sizeof(*m_overlapped));
		m_overlapped->hEvent = event;
	}
}

Pipe::~Pipe()
{
	Close();
}

Pipe::SyncResult Pipe::Sync(std::chrono::milliseconds timeout)
{
	if (!IsValid())
		return SyncResult::InvalidFile;
	else if (IsOpExecuting())
	{
		const auto waitResult = WaitForSingleObject(
			m_overlapped->hEvent,
			timeout == k_syncInfinite ? INFINITE : static_cast<DWORD>(timeout.count())  // translate infinite case
		);

		if (waitResult == WAIT_OBJECT_0)
			return SyncResult::Success;
		else if (waitResult == WAIT_TIMEOUT)
			return SyncResult::StillExecuting;
		else
			return SyncResult::InvalidFile;
	}
	else
		return SyncResult::Success;
}

bool Pipe::IsOpExecuting() const
{
	return m_overlapped && !HasOverlappedIoCompleted(m_overlapped.get());
}

unsigned int Pipe::GetBufferSize() const
{
	return m_buffer->size;
}

bool Pipe::IsValid() const
{
	const auto isOverlappedValid = static_cast<bool>(m_overlapped);
	[[maybe_unused]] const auto isBufferValid = static_cast<bool>(m_buffer);
	assert(isOverlappedValid == isBufferValid);
	return isOverlappedValid;
}

Pipe& Pipe::operator =(Pipe&& other)
{
	Close();

	m_file = other.m_file;
	other.m_file = INVALID_HANDLE_VALUE;
	m_overlapped = std::move(other.m_overlapped);
	m_buffer = std::move(other.m_buffer);

	return *this;
}

void Pipe::CancelOp()
{
	if (IsOpExecuting())
		CancelIoEx(m_file, m_overlapped.get());
}

void Pipe::Close()
{
	if (m_overlapped)
	{
		CancelOp();
		CloseHandle(m_overlapped->hEvent);
	}
}


ReadPipe::ReadPipe(HANDLE file, unsigned int bufferSize)
	: Pipe(file, bufferSize)
	, m_isResultConsumed(false)
{
}

Pipe::OpResult ReadPipe::Read()
{
	const auto result = Helper::ExecuteOp(
		*this,
		[this] () {
			ReadFile(m_file, m_buffer->data, m_buffer->size, nullptr, m_overlapped.get());
		}
	);

	// because GetResult() returns StillExecuting if an operation is running, there's no worries
	// about result from the last successful being pulled.
	if (std::get<OpResultCode>(result) == OpResultCode::Success)
		m_isResultConsumed = false;

	return result;
}

ReadPipe::ReadResult ReadPipe::ReadSync(Buffer& outBuffer, std::chrono::milliseconds timeout)
{
	assert(outBuffer.size <= m_buffer->size);

	const auto result = Read();
	assert(std::get<0>(result) != OpResultCode::StillExecuting);
	if (std::get<0>(result) == OpResultCode::Success)
	{
		const auto syncResult = Sync(timeout);
		if (syncResult == SyncResult::Success)
			return GetResult(outBuffer);
		else if (syncResult == SyncResult::StillExecuting)
			return { OpResultCode::StillExecuting, NO_ERROR, 0 };
		else
			return { OpResultCode::InvalidFile, GetLastError(), 0 };
	}

	return { std::get<0>(result), std::get<1>(result), 0 };
}

ReadPipe::ReadResult ReadPipe::GetResult(Buffer& outBuffer)
{
	assert(outBuffer.size <= m_buffer->size);

	if (!IsValid())
		return { OpResultCode::InvalidFile, NO_ERROR, 0 };
	else if (IsOpExecuting())
		return { OpResultCode::StillExecuting, NO_ERROR, 0 };

	DWORD bytesRead;
	if (m_isResultConsumed)
	{
		return { OpResultCode::Success, NO_ERROR, 0 };
	}
	else if (GetOverlappedResult(m_file, m_overlapped.get(), &bytesRead, FALSE) != 0)
	{
		m_isResultConsumed = true;
		memcpy(
			outBuffer.data,
			m_buffer->data,
			std::min(static_cast<uint32_t>(bytesRead), outBuffer.size)
		);
		return { OpResultCode::Success, NO_ERROR, bytesRead };  // return the valid length from last device read
	}
	else
		return { OpResultCode::InvalidFile, GetLastError(), 0 };
}


WritePipe::WritePipe(HANDLE file, unsigned int bufferSize)
	: Pipe(file, bufferSize)
{
}

Pipe::OpResult WritePipe::Write(const Buffer& buffer)
{
	assert(buffer.size <= m_buffer->size);

	if (IsOpExecuting())
		return { OpResultCode::StillExecuting, NO_ERROR };

	ZeroMemory(m_buffer->data, m_buffer->size);
	memcpy(m_buffer->data, buffer.data, buffer.size);

	return Helper::ExecuteOp(
		*this,
		[this] () {
			WriteFile(m_file, m_buffer->data, m_buffer->size, nullptr, m_overlapped.get());
		}
	);
}

Pipe::OpResult WritePipe::WriteSync(const Buffer& buffer, std::chrono::milliseconds timeout)
{
	const auto result = Write(buffer);
	if (std::get<OpResultCode>(result) == OpResultCode::Success)
		return { Sync(timeout), GetLastError() };

	return result;
}


DeviceIoPipes::DeviceIoPipes(AutoHandle&& file, const PipeParams& pipeParams)
	: m_file(std::move(file))
	, m_pipeRead(m_file, pipeParams.readBufferSize)
	, m_pipeWrite(m_file, pipeParams.writeBufferSize)
	, m_mutexRead()
	, m_mutexWrite()
{
}

DeviceIoPipes::~DeviceIoPipes()
{
}

Pipe::OpResult DeviceIoPipes::Read()
{
	std::scoped_lock lock(m_mutexRead);
	return m_pipeRead.Read();
}

ReadPipe::ReadResult DeviceIoPipes::ReadSync(Buffer& outBuffer, std::chrono::milliseconds timeout)
{
	std::scoped_lock lock(m_mutexRead);
	return m_pipeRead.ReadSync(outBuffer, timeout);
}

ReadPipe::ReadResult DeviceIoPipes::PopReadResult(Buffer& outBuffer)
{
	std::scoped_lock lock(m_mutexRead);
	return m_file ?
		m_pipeRead.GetResult(outBuffer) :
		ReadPipe::ReadResult { Pipe::OpResultCode::InvalidFile, NO_ERROR, 0 };
}

Pipe::OpResult DeviceIoPipes::Write(const Buffer& buffer)
{
	std::scoped_lock lock(m_mutexWrite);
	return m_pipeWrite.Write(buffer);
}

Pipe::OpResult DeviceIoPipes::WriteSync(const Buffer& buffer, std::chrono::milliseconds timeout)
{
	std::scoped_lock lock(m_mutexWrite);
	return m_pipeWrite.WriteSync(buffer, timeout);
}

Pipe::SyncResult DeviceIoPipes::SyncRead(std::chrono::milliseconds timeout)
{
	std::scoped_lock lock(m_mutexRead);
	return m_pipeRead.Sync(timeout);
}

Pipe::SyncResult DeviceIoPipes::SyncWrite(std::chrono::milliseconds timeout)
{
	std::scoped_lock lock(m_mutexWrite);
	return m_pipeWrite.Sync(timeout);
}

Pipe::SyncResult DeviceIoPipes::SyncAll(std::chrono::milliseconds timeout)
{
	const SteadyTimer timer;
	const auto syncReadResult = SyncRead(timeout);

	// only continue if the read operation finished
	if (syncReadResult == Pipe::SyncResult::Success)
	{
		const auto elaspedTime = timer.GetElapsed();

		// system overhead may make usedTimeout greater than timeout in which case we consider operation timed out
		if (timeout > elaspedTime)
			return SyncWrite(timeout - elaspedTime);
		else
			return Pipe::SyncResult::StillExecuting;
	}

	return syncReadResult;
}

void DeviceIoPipes::CancelRead()
{
	std::scoped_lock lock(m_mutexRead);
	m_pipeRead.CancelOp();
}

DeviceIoPipes& DeviceIoPipes::operator =(DeviceIoPipes&& other)
{
	std::scoped_lock lock(m_mutexRead, m_mutexWrite, other.m_mutexRead, other.m_mutexWrite);

	m_pipeRead = std::move(other.m_pipeRead);
	m_pipeWrite = std::move(other.m_pipeWrite);
	m_file = std::move(other.m_file);
	other.m_file = INVALID_HANDLE_VALUE;

	return *this;
}

void DeviceIoPipes::Close()
{
	// closing m_file automatically makes future operations of m_pipeRead and m_pipeWrite return InvalidFile
	std::scoped_lock lock(m_mutexRead, m_mutexWrite);
	m_file.Close();
}

unsigned int DeviceIoPipes::GetReadBufferSize() const
{
	return m_pipeRead.GetBufferSize();
}

unsigned int DeviceIoPipes::GetWriteBufferSize() const
{
	return m_pipeWrite.GetBufferSize();
}

bool DeviceIoPipes::IsFileValid() const
{
	return m_file;
}

