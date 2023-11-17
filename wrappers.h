#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

template <typename THandle, typename TCloseFunc, TCloseFunc CloseFunc, THandle ZeroValue = THandle()>
class HandleWrapper
{
private:
	THandle m_handle;
public:
	HandleWrapper() : m_handle(ZeroValue) {}
	HandleWrapper(THandle handle) : m_handle(handle) {}
	~HandleWrapper()
	{
		if (m_handle != ZeroValue)
			CloseFunc(m_handle);
	}
	HandleWrapper(const HandleWrapper&) = delete;
	HandleWrapper& operator=(const HandleWrapper&) = delete;

	operator THandle() { return m_handle; }
	THandle* operator&() { return &m_handle; }
};

typedef HandleWrapper<HANDLE, decltype(CloseHandle), CloseHandle> Handle;
typedef HandleWrapper<HANDLE, decltype(CloseHandle), CloseHandle, INVALID_HANDLE_VALUE> HFile;
typedef HandleWrapper<HKEY, decltype(RegCloseKey), RegCloseKey> HKey;