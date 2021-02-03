#pragma once

#include "IOCPServer.h"
#include "Packet.h"

#include <deque>


class EchoServer : public IOCPServer
{
private:
	bool mIsRunProcessThread = false;

	std::thread mProcessThread;

	std::mutex mLock;

	std::deque<PacketData> mPacketDataQueue;

public:
	EchoServer() = default;
	virtual ~EchoServer() = default;

	virtual void OnConnect(const UINT32 _clientIndex) override
	{
		printf("[OnConnect] Index(%d)\n", _clientIndex);
	}

	virtual void OnClose(const UINT32 _clientIndex) override
	{
		printf("[OnClose] Index(%d)\n", _clientIndex);
	}

	virtual void OnReceive(const UINT32 _clientIndex, const UINT32 _size, char* _pData) override
	{
		printf("[OnReceive] Index(%d), dataSize(%d)\n", _clientIndex, _size);

		PacketData packet;
		packet.Set(_clientIndex, _size, _pData);

		std::lock_guard<std::mutex> guard(mLock);
		mPacketDataQueue.push_back(packet);
	}

	void Run(const UINT32 _maxClient)
	{
		mIsRunProcessThread = true;
		mProcessThread = std::thread([this]() {ProcessPacket(); });

		StartServer(_maxClient);
	}

	void End()
	{
		mIsRunProcessThread = false;

		if (mProcessThread.joinable())
		{
			mProcessThread.join();
		}

		DestroyThread();
	}

private:
	void ProcessPacket()
	{
		while (mIsRunProcessThread)
		{
			auto packetData = DequePacketData();
			if (packetData.DataSize != 0)
			{
				SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	PacketData DequePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty())
		{
			return PacketData();
		}

		packetData.Set(mPacketDataQueue.front());

		mPacketDataQueue.front().Release();
		mPacketDataQueue.pop_front();

		return packetData;
	}
};