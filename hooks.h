#pragma once

#include <WinSock2.h>
#include <detours/detours.h>

using sendto_t = int (WINAPI*)(
	SOCKET s,
	const char* buf,
	int len,
	int flags,
	const struct sockaddr* to,
	int tolen
);

using WSASendTo_t = int (WINAPI*)(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	const sockaddr* lpTo,
	int iTolen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

using connect_t = int (WINAPI*)(SOCKET s, const sockaddr* name, int namelen);

int WINAPI Pudge_WSASendTo(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesSent,
	DWORD dwFlags,
	const sockaddr* lpTo,
	int iTolen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

int WINAPI Pudge_sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen);
int WINAPI Pudge_connect(SOCKET s, const sockaddr* name, int namelen);

void HooksInit();
void HooksDetach();