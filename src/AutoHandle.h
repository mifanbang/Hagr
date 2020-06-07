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


using Handle = void*;


bool IsHandleValid(Handle handle);


class AutoHandle
{
public:
	AutoHandle() noexcept;
	AutoHandle(Handle handle) noexcept;
	AutoHandle(AutoHandle&&) noexcept;
	~AutoHandle() noexcept;

	AutoHandle& operator = (Handle other) noexcept;
	AutoHandle& operator = (AutoHandle&& other) noexcept;

	operator bool () const noexcept;
	operator Handle () const noexcept;

	void Close() noexcept;

	AutoHandle(const AutoHandle&) = delete;
	AutoHandle& operator = (const AutoHandle&) = delete;

private:
	Handle m_handle;
};