#pragma once


#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct PacketData
{
	UINT32 SessionIndex = 0;
	UINT32 DataSize = 0;
	char* pPacketData = nullptr;

	void Set(PacketData& value)
	{
		SessionIndex = value.SessionIndex;
		DataSize = value.DataSize;

		pPacketData = new char[DataSize];
		CopyMemory(pPacketData, value.pPacketData, DataSize);
	}

	void Set(UINT32 _sessionIndex, UINT32 _dataSize, char* _pData)
	{
		SessionIndex = _sessionIndex;
		DataSize = _dataSize;

		pPacketData = new char[DataSize];
		CopyMemory(pPacketData, _pData, DataSize);
	}

	void Release()
	{
		delete pPacketData;
	}
};