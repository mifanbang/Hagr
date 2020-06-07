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

#include "AutoHandle.h"

#include <windows.h>



bool IsHandleValid(Handle handle)
{
	return handle != INVALID_HANDLE_VALUE && handle != nullptr;
}



AutoHandle::AutoHandle() noexcept
	: m_handle(nullptr)
{
}

AutoHandle::AutoHandle(Handle handle) noexcept
	: m_handle(handle)
{
}

AutoHandle::AutoHandle(AutoHandle&& handle) noexcept
	: m_handle(handle.m_handle)
{
	handle.m_handle = nullptr;
}

AutoHandle::~AutoHandle() noexcept
{
	Close();
}

AutoHandle& AutoHandle::operator = (Handle other) noexcept
{
	Close();
	m_handle = other;
	return *this;
}

AutoHandle& AutoHandle::operator = (AutoHandle&& other) noexcept
{
	*this = other.m_handle;  // call "operator = (Handle)" version
	other.m_handle = nullptr;
	return *this;
}

AutoHandle::operator Handle () const noexcept
{
	return m_handle;
}

AutoHandle::operator bool () const noexcept
{
	return IsHandleValid(m_handle);
}

void AutoHandle::Close() noexcept
{
	if (static_cast<bool>(*this))
		CloseHandle(m_handle);
	m_handle = nullptr;
}
