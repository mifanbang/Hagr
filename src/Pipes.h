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

#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <tuple>

#include <windows.h>

#include "AutoHandle.h"
#include "LightWeightMutex.h"



struct Buffer
{
	uint32_t size { 0 };
	uint8_t* data { nullptr };

	Buffer(uint32_t size)
		: size(size)
		, data(new uint8_t[size])
	{
		ZeroMemory(data, size);
	}

	~Buffer()
	{
		if (data != nullptr)
			delete[] data;
	}

	template <typename T>
	operator T& ()
	{
		assert(sizeof(T) <= size);
		return *reinterpret_cast<T*>(data);
	}
};


// return true if an invocation of func() returned false
template <typename T, typename F>
bool IterateBuffer(const Buffer& buffer, const F& func)
{
	assert(sizeof(T) <= buffer.size);
	const unsigned int numOfT = buffer.size / sizeof(T);
	for (unsigned int i = 0; i < numOfT; ++i)
	{
		const T* instOfT = reinterpret_cast<const T*>(buffer.data) + i;
		if (!func(*instOfT))
			return true;
	}
	return false;
}



class Pipe
{
public:
	enum class OpResultCode
	{
		Success,
		StillExecuting,  // previous operation still executing
		InvalidFile  // the file is or has become invalid
	};

	using SyncResult = OpResultCode;
	using SystemErrorCode = DWORD;
	using OpResult = std::tuple<OpResultCode, SystemErrorCode>;

	static constexpr std::chrono::milliseconds k_syncInfinite { 0 };


	Pipe(HANDLE file, unsigned int bufferSize);
	~Pipe();

	SyncResult Sync(std::chrono::milliseconds timeout);  // return false if file is not valid or timed out
	void CancelOp();

	bool IsOpExecuting() const;
	unsigned int GetBufferSize() const;
	bool IsValid() const;  // must be able to check validity as Pipe supports move operation

	Pipe& operator =(Pipe&& other);


protected:
	class Helper;

	void Close();  // cancel running overlapped operation and relese some resources. the file would still be open.


	HANDLE m_file;
	std::unique_ptr<OVERLAPPED> m_overlapped;
	std::unique_ptr<Buffer> m_buffer;
};



class ReadPipe : public Pipe
{
public:
	using ReadResult = std::tuple<OpResultCode, SystemErrorCode, unsigned int>;

	ReadPipe(HANDLE file, unsigned int bufferSize);

	OpResult Read();
	ReadResult ReadSync(Buffer& outBuffer, std::chrono::milliseconds timeout);

	// for a successful read, only the first call to GetResult() returns a meaningful result.
	// all subsequent calls without issuing another read will succeed but have zero data copied to outBuffer.
	ReadResult GetResult(Buffer& outBuffer);


private:
	bool m_isResultConsumed;
};



class WritePipe : public Pipe
{
public:
	WritePipe(HANDLE file, unsigned int bufferSize);

	OpResult Write(const Buffer& buffer);
	OpResult WriteSync(const Buffer& buffer, std::chrono::milliseconds timeout);
};



class DeviceIoPipes
{
public:
	struct PipeParams
	{
		unsigned int readBufferSize;
		unsigned int writeBufferSize;
	};

	DeviceIoPipes(AutoHandle&& file, const PipeParams& pipeParams);
	~DeviceIoPipes();

	Pipe::OpResult Read();
	ReadPipe::ReadResult ReadSync(Buffer& outBuffer, std::chrono::milliseconds timeout);
	ReadPipe::ReadResult PopReadResult(Buffer& outBuffer);
	Pipe::OpResult Write(const Buffer& buffer);
	Pipe::OpResult WriteSync(const Buffer& buffer, std::chrono::milliseconds timeout);

	Pipe::SyncResult SyncRead(std::chrono::milliseconds timeout);
	Pipe::SyncResult SyncWrite(std::chrono::milliseconds timeout);
	Pipe::SyncResult SyncAll(std::chrono::milliseconds timeout);
	void CancelRead();

	DeviceIoPipes& operator =(DeviceIoPipes&& other);
	void Close();

	unsigned int GetReadBufferSize() const;
	unsigned int GetWriteBufferSize() const;
	bool IsFileValid() const;


private:
	AutoHandle m_file;
	ReadPipe m_pipeRead;
	WritePipe m_pipeWrite;
	LWMutex m_mutexRead;
	LWMutex m_mutexWrite;
};
