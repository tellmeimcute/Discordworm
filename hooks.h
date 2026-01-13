#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
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

using WSARecvFrom_t = int (WINAPI*)(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	sockaddr* lpFrom,
	LPINT lpFromlen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

using connect_t = int (WINAPI*)(SOCKET s, const sockaddr* name, int namelen);
using bint_t = int (WINAPI*)(SOCKET s, const sockaddr* addr, int namelen);
using closesocket_t = int (WINAPI*)(SOCKET s);

using udp_association_t = struct {
	SOCKET controlSocket;
	sockaddr_in proxyAddr;
	bool is_initialized;
};


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

void HooksAttach();
void HooksDetach();