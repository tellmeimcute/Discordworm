#include "hooks.h"
#include <stdint.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <detours/detours.h>

const char ProxyAddress[14]{ "127.0.0.1" };
const uint16_t ProxyPort{ 2080 };

const uint8_t FakeUDPpayload[16]{};
const int ReadWriteTimeout{ 15 };

connect_t Real_connect = connect;
sendto_t Real_sendto = sendto;
WSASendTo_t Real_WSASendTo = WSASendTo;

bool IsUDPSocket(SOCKET s)
{
	int32_t sockOptVal{};
	int32_t sockOptLen = sizeof(sockOptVal);

	if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&sockOptVal, &sockOptLen) != 0)
		return false;

	return sockOptVal == SOCK_DGRAM;
}

bool WaitForWrite(SOCKET s, int timeoutSec)
{
	fd_set writeSet{};
	FD_SET(s, &writeSet);

	timeval timeout = { timeoutSec, 0 };
	int result = select(0, NULL, &writeSet, NULL, &timeout);
	if (result > 0 && FD_ISSET(s, &writeSet)) {
		return true;
	}

	return false;
}

bool WaitForRead(SOCKET s, int timeoutSec)
{
	fd_set readSet{};
	FD_SET(s, &readSet);

	timeval timeout = { timeoutSec, 0 };
	int result = select(0, &readSet, NULL, NULL, &timeout);
	if (result > 0 && FD_ISSET(s, &readSet)) {
		return true;
	}

	return false;
}

int ConnectToProxy(SOCKET s) {
	SOCKADDR_IN proxyAddr{};
	proxyAddr.sin_family = AF_INET;
	inet_pton(AF_INET, ProxyAddress, &proxyAddr.sin_addr);
	proxyAddr.sin_port = htons(ProxyPort);

	int result = Real_connect(s, (struct sockaddr*)&proxyAddr, sizeof(proxyAddr));
	if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		return result;
	}

	if (WaitForWrite(s, ReadWriteTimeout)) {
		return ERROR_SUCCESS;
	}

	WSASetLastError(WSAETIMEDOUT);
	return SOCKET_ERROR;

}

int SendSocks5Handshake(SOCKET s) {
	uint8_t request[3]{ 0x05, 0x01, 0x00 };

	if (send(s, (const char*)request, sizeof(request), 0) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	uint8_t response[2]{};

	WaitForRead(s, ReadWriteTimeout);

	if (recv(s, (char*)response, sizeof(response), 0) <= 0) {
		return SOCKET_ERROR;
	}
	
	if (response[1] != 0x00) {
		return SOCKET_ERROR;
	}

	if (WSAGetLastError() != WSAEWOULDBLOCK) {
		return ERROR_SUCCESS;
	}

	if (WaitForWrite(s, ReadWriteTimeout)) {
		return ERROR_SUCCESS;
	}

	WSASetLastError(WSAETIMEDOUT);
	return SOCKET_ERROR;
}


int SendSocks5Connect(SOCKET s, const struct sockaddr_in* targetAddr) {

	uint8_t connectRequest[10]{ 0x05, 0x01, 0x00, 0x01 };
	memcpy(connectRequest + 4, &targetAddr->sin_addr, 4);
	memcpy(connectRequest + 8, &targetAddr->sin_port, 2);

	WaitForWrite(s, ReadWriteTimeout);

	if (send(s, (const char*)connectRequest, sizeof(connectRequest), 0) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	uint8_t connectResponse[10]{};

	WaitForRead(s, ReadWriteTimeout);

	if (recv(s, (char*)connectResponse, sizeof(connectResponse), 0) <= 0) {
		return SOCKET_ERROR;
	}

	if (connectResponse[1] != 0x00) {
		return SOCKET_ERROR;
	}

	WSASetLastError(WSAEWOULDBLOCK);
	return SOCKET_ERROR;
}

int WINAPI Pudge_connect(SOCKET s, const sockaddr* name, int namelen) {

	if (IsUDPSocket(s)) {
		return Real_connect(s, name, namelen);
	}

	if (name->sa_family == AF_INET6) {
		WSASetLastError(WSAECONNREFUSED);
		return SOCKET_ERROR;
	}

	if (ConnectToProxy(s) != ERROR_SUCCESS) {
		return SOCKET_ERROR;
	}

	if (SendSocks5Handshake(s) != ERROR_SUCCESS) {
		return SOCKET_ERROR;
	}

	const struct sockaddr_in* target = reinterpret_cast<const struct sockaddr_in*>(name);
	return SendSocks5Connect(s, target);
	
}

int WINAPI Pudge_sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
	if (len == 74) {
		Real_sendto(s, (const char*)FakeUDPpayload, sizeof(FakeUDPpayload), flags, to, tolen);
	}

	return Real_sendto(s, buf, len, flags, to, tolen);
}

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
)
{
	if (lpBuffers->len == 74) {
		Real_sendto(s, (const char*)FakeUDPpayload, sizeof(FakeUDPpayload), 0, lpTo, iTolen);
	}

	return Real_WSASendTo(
		s,
		lpBuffers,
		dwBufferCount,
		lpNumberOfBytesSent,
		dwFlags,
		lpTo,
		iTolen,
		lpOverlapped,
		lpCompletionRoutine
	);
}

void HooksInit() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)Real_WSASendTo, Pudge_WSASendTo);
	//DetourAttach(&(PVOID&)Real_sendto, Pudge_sendto);
	DetourAttach(&(PVOID&)Real_connect, Pudge_connect);
	DetourTransactionCommit();
}

void HooksDetach() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)Real_WSASendTo, Pudge_WSASendTo);
	//DetourDetach(&(PVOID&)Real_sendto, Pudge_sendto);
	DetourDetach(&(PVOID&)Real_connect, Pudge_connect);
	DetourTransactionCommit();
}