#pragma comment(lib, "detours.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")

#include "dllmain.h"
#include "Detours.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <shlwapi.h>
#include <string>
#include <thread>
#include <mutex>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

FARPROC	realDirectInput8Create;
extern "C" __declspec(dllexport, naked) int DirectInput8Create()
{
	__asm
	{
		jmp far realDirectInput8Create;
	}
}

SOCKET cs_send = INVALID_SOCKET;
SOCKET cs_recv = INVALID_SOCKET;
char* buffer = new char[10000];
int buffer_size = 0;
std::mutex buffer_lock;

typedef int (WINAPI* SENDTO)(SOCKET, const char*, int, int, const sockaddr*, int);
SENDTO origSendTo = NULL;

typedef int (WINAPI* RECVFROM)(SOCKET, char*, int, int, const sockaddr*, int*);
RECVFROM origRecvFrom = NULL;

int WINAPI detrSendTo(SOCKET s, const char* buf, int len, int flags, const sockaddr* to, int tolen)
{
	int messageLength = len;
	if (send(cs_send, (char*)&messageLength, sizeof(int), 0) != SOCKET_ERROR)
	{
		if (send(cs_send, buf, len, 0) != SOCKET_ERROR)
		{
			return len;
		}
	}

	return SOCKET_ERROR;
}

int WINAPI detrRecvFrom(SOCKET s, char* buf, int len, int flags, const sockaddr* from, int* fromlen)
{
	if (buffer_size > 0)
	{
		buffer_lock.lock();
		int messageLength = 0;
		memcpy((char*)&messageLength, buffer, sizeof(int));
		memcpy(buf, buffer + sizeof(int), messageLength);
		buffer_size -= messageLength + sizeof(int);
		memmove(buffer, buffer + messageLength + sizeof(int), buffer_size);
		buffer_lock.unlock();

		return messageLength;
	}

	WSASetLastError(WSAEWOULDBLOCK);
	return SOCKET_ERROR;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID)
{
	if (DetourIsHelperProcess()) {
		return TRUE;
	}

	HMODULE hDinput = NULL;
	WCHAR wbuf[MAX_PATH + 1];

	switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			GetSystemDirectoryW(wbuf, MAX_PATH);
			lstrcatW(wbuf, L"\\dinput8.dll");
			hDinput = LoadLibraryW(wbuf);
			if (hDinput && hDinput != NULL) realDirectInput8Create = GetProcAddress(hDinput, "DirectInput8Create");
			SetHooks();
			StartRelay();
			break;

		case DLL_PROCESS_DETACH:
			if (hDinput && hDinput != NULL) FreeLibrary(hDinput);
			break;
	}

	return TRUE;
}

void SetHooks()
{
	origSendTo = (SENDTO)sendto;
	origRecvFrom = (RECVFROM)recvfrom;

	DetourRestoreAfterWith();
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourAttach(&(PVOID&)origSendTo, detrSendTo);
	DetourAttach(&(PVOID&)origRecvFrom, detrRecvFrom);

	DetourTransactionCommit();
}

void StartRelay()
{
	WSADATA wsaData;
	struct sockaddr_in serverAddr1, serverAddr2;

	// Init winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
		return;
	}

	// Create recv client socket
	{
		cs_recv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (cs_recv == INVALID_SOCKET)
		{
			std::cerr << "Socket creation failed. Error: " << WSAGetLastError() << std::endl;
			WSACleanup();
			return;
		}

		serverAddr1.sin_family = AF_INET;
		serverAddr1.sin_port = htons(443);
		inet_pton(AF_INET, "70.34.247.79", &serverAddr1.sin_addr);

		if (connect(cs_recv, (struct sockaddr*)&serverAddr1, sizeof(serverAddr1)) == SOCKET_ERROR)
		{
			std::cerr << "Connection to receiving socket failed. Error: " << WSAGetLastError() << std::endl;
			closesocket(cs_recv);
			WSACleanup();
			return;
		}
	}

	// Create send client socket
	{
		cs_send = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (cs_send == INVALID_SOCKET)
		{
			std::cerr << "Socket creation for sending failed. Error: " << WSAGetLastError() << std::endl;
			closesocket(cs_send);
			WSACleanup();
			return;
		}

		serverAddr2.sin_family = AF_INET;
		serverAddr2.sin_port = htons(80);
		inet_pton(AF_INET, "70.34.247.79", &serverAddr2.sin_addr);

		if (connect(cs_send, (struct sockaddr*)&serverAddr2, sizeof(serverAddr2)) == SOCKET_ERROR)
		{
			std::cerr << "Connection to sending socket failed. Error: " << WSAGetLastError() << std::endl;
			closesocket(cs_recv);
			closesocket(cs_send);
			WSACleanup();
			return;
		}
	}

	char hostname[256];
	if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
	{
		closesocket(cs_recv);
		closesocket(cs_send);
		WSACleanup();
		return;
	}

	struct addrinfo hints = { 0 };
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* addrResult = nullptr;
	int result = getaddrinfo(hostname, nullptr, &hints, &addrResult);
	if (result != 0)
	{
		closesocket(cs_recv);
		closesocket(cs_send);
		WSACleanup();
		return;
	}

	std::string address = GetIp(addrResult->ai_addr);
	int messageLength = address.length();
	send(cs_send, (char*)&messageLength, sizeof(int), 0);
	send(cs_send, address.c_str(), address.length(), 0);

	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, 0, 0, 0);
}

void ReceiveThread()
{
	while (true)
	{
		if (!ReceiveMessage())
		{
			break;
		}
	}

	closesocket(cs_recv);
	closesocket(cs_send);
	WSACleanup();
}

bool ReceiveMessage()
{
	int messageLength = 0;
	int bytesReceived = recv(cs_recv, (char*)&messageLength, sizeof(int), 0);

	if (bytesReceived <= 0)
		return false;

	char* messageBuffer = new char[messageLength];
	int totalBytesReceived = 0;
	while (totalBytesReceived < messageLength)
	{
		int remainingBytes = messageLength - totalBytesReceived;
		bytesReceived = recv(cs_recv, messageBuffer + totalBytesReceived, remainingBytes, 0);
		if (bytesReceived <= 0)
		{
			delete[] messageBuffer;
			return false;
		}

		totalBytesReceived += bytesReceived;
	}

	if (buffer_size + messageLength <= 9000)
	{
		// Process message
		buffer_lock.lock();
		memcpy(buffer + buffer_size, (char*)&messageLength, sizeof(int));
		buffer_size += sizeof(int);
		memcpy(buffer + buffer_size, messageBuffer, messageLength);
		buffer_size += messageLength;
		buffer_lock.unlock();
	}

	delete[] messageBuffer;
	return true;
}

void ModifyIp(const sockaddr* addr, const char* new_ip)
{
	sockaddr_in* addr_in = const_cast<sockaddr_in*>(reinterpret_cast<const sockaddr_in*>(addr));
	addr_in->sin_addr.s_addr = inet_addr(new_ip);
}

std::string GetIp(const sockaddr* sa)
{
	if (sa->sa_family == AF_INET)
	{
		const sockaddr_in* sa_in = reinterpret_cast<const sockaddr_in*>(sa);
		char ip_str[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &(sa_in->sin_addr), ip_str, INET_ADDRSTRLEN) != nullptr)
		{
			return std::string(ip_str);
		}
		else
		{
			return "";
		}
	}
	else
	{
		return "";
	}
}