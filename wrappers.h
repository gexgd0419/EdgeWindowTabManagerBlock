#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Handle
{
private:
	HANDLE m_handle;
public:
	Handle() : m_handle(NULL) {}
	Handle(HANDLE handle) : m_handle(handle) {}
	~Handle()
	{
		if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE)
			CloseHandle(m_handle);
	}
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;

	operator HANDLE() { return m_handle; }
	HANDLE* operator&() { return &m_handle; }
};