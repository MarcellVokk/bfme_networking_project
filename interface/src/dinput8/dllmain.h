#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

extern HANDLE g_hDllHandle;

void SetHooks();
void StartRelay();
void ReceiveThread();
bool ReceiveMessage();
void ModifyIp(const sockaddr* addr, const char* new_ip);
std::string GetIp(const sockaddr* sa);