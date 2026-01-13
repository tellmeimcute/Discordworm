#include "hooks.h"
#include "framework.h"
#include <stdint.h>
#include <shared_mutex>
#include <set>
#include <map>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <detours/detours.h>

connect_t Real_connect = connect;
sendto_t Real_sendto = sendto;
WSASendTo_t Real_WSASendTo = WSASendTo;
WSARecvFrom_t Real_WSARecvFrom = WSARecvFrom;
bint_t Real_bind = bind;
closesocket_t Real_closesocket = closesocket;

std::shared_mutex SockMtx{};
std::map<SOCKET, udp_association_t> activeAssociations{};

bool IsUDPSocket(SOCKET s)
{
	int32_t sockOptVal{};
	int32_t sockOptLen = sizeof(sockOptVal);

	if (getsockopt(s, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&sockOptVal), &sockOptLen) != 0) {
		return false;
	}

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
	sockaddr_in proxyAddr{};
	proxyAddr.sin_family = AF_INET;
	proxyAddr.sin_addr = config.ProxyAddress;
	proxyAddr.sin_port = config.ProxyPort;

	int result = Real_connect(s, reinterpret_cast<sockaddr*>(&proxyAddr), sizeof(proxyAddr));

	if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		return result;
	}

	if (WaitForWrite(s, config.ReadWriteTimeout)) {
		return ERROR_SUCCESS;
	}

	WSASetLastError(WSAETIMEDOUT);
	return SOCKET_ERROR;

}

int SendSocks5Handshake(SOCKET s) {
	const uint8_t request[3]{ 0x05, 0x01, 0x00 };

	if (send(s, reinterpret_cast<const char*>(request), sizeof(request), 0) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	uint8_t response[2]{};

	WaitForRead(s, config.ReadWriteTimeout);

	if (recv(s, reinterpret_cast<char*>(response), sizeof(response), 0) <= 0) {
		return SOCKET_ERROR;
	}
	
	if (response[1] != 0x00) {
		return SOCKET_ERROR;
	}

	if (WSAGetLastError() != WSAEWOULDBLOCK) {
		return ERROR_SUCCESS;
	}

	if (WaitForWrite(s, config.ReadWriteTimeout)) {
		return ERROR_SUCCESS;
	}

	WSASetLastError(WSAETIMEDOUT);
	return SOCKET_ERROR;
}

int SendSocks5Connect(SOCKET s, const struct sockaddr_in* targetAddr) {

	uint8_t connectRequest[10]{ 0x05, 0x01, 0x00, 0x01 };
	memcpy(connectRequest + 4, &targetAddr->sin_addr, 4);
	memcpy(connectRequest + 8, &targetAddr->sin_port, 2);

	WaitForWrite(s, config.ReadWriteTimeout);

	if (send(s, reinterpret_cast<const char*>(connectRequest), sizeof(connectRequest), 0) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	uint8_t connectResponse[10]{};

	WaitForRead(s, config.ReadWriteTimeout);

	if (recv(s, reinterpret_cast<char*>(connectResponse), sizeof(connectResponse), 0) <= 0) {
		return SOCKET_ERROR;
	}

	if (connectResponse[1] != 0x00) {
		return SOCKET_ERROR;
	}

	WSASetLastError(WSAEWOULDBLOCK);
	return SOCKET_ERROR;
}

int SendSocks5Associate(udp_association_t &assoc) {
	uint8_t connectRequest[10]{ 0x05, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	WaitForWrite(assoc.controlSocket, config.ReadWriteTimeout);

	if (send(assoc.controlSocket, reinterpret_cast<const char*>(connectRequest), sizeof(connectRequest), 0) == SOCKET_ERROR) {
		return SOCKET_ERROR;
	}

	uint8_t connectResponse[10]{};
	WaitForRead(assoc.controlSocket, config.ReadWriteTimeout);

	if (recv(assoc.controlSocket, reinterpret_cast<char*>(connectResponse), sizeof(connectResponse), 0) <= 0) {
		return SOCKET_ERROR;
	}

	if (connectResponse[1] != 0x00) {
		return SOCKET_ERROR;
	}

	assoc.proxyAddr.sin_family = AF_INET;
	memcpy(&assoc.proxyAddr.sin_addr, connectResponse + 4, 4);
	memcpy(&assoc.proxyAddr.sin_port, connectResponse + 8, 2);

	assoc.is_initialized = true;

	return ERROR_SUCCESS;
}

int InitSocksAssociation(udp_association_t& assoc) {
	assoc.controlSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (assoc.controlSocket == INVALID_SOCKET) {
		return SOCKET_ERROR;
	}

	// SET NON BLOCKING MODE
	u_long mode = true;
	ioctlsocket(assoc.controlSocket, FIONBIO, &mode);

	if (ConnectToProxy(assoc.controlSocket) != ERROR_SUCCESS) {
		return SOCKET_ERROR;
	}

	if (SendSocks5Handshake(assoc.controlSocket) != ERROR_SUCCESS) {
		return SOCKET_ERROR;
	}

	if (SendSocks5Associate(assoc) != ERROR_SUCCESS) {
		return SOCKET_ERROR;
	}

	return 0;
}

void EncapsulateSocks5Datagram(WSABUF* target, char* buf, int len, const sockaddr* lpTo)
{
	target->len = len + 10;
	//target->buf = (char*)malloc(target->len);
	target->buf = new char[target->len];

	target->buf[0] = 0;
	target->buf[1] = 0;
	target->buf[2] = 0;
	target->buf[3] = 1;

	const struct sockaddr_in* addr = reinterpret_cast<const struct sockaddr_in*>(lpTo);
	memcpy(&target->buf[4], &addr->sin_addr.s_addr, sizeof(addr->sin_addr.s_addr)); //ip addr == 4 byte
	memcpy(&target->buf[8], &addr->sin_port, sizeof(addr->sin_port)); // port == 2 byte
	memcpy(&target->buf[10], buf, len);
}

void ExtractSockAddr(char* buf, sockaddr* target)
{
	struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(target);
	memcpy((void*)&addr->sin_addr.s_addr, &buf[4], sizeof(addr->sin_addr.s_addr)); //ip addr
	memcpy((void*)&addr->sin_port, &buf[8], sizeof(addr->sin_port)); // port
}

int WINAPI Pudge_connect(SOCKET s, const sockaddr* name, int namelen) {
	const struct sockaddr_in* target = reinterpret_cast<const struct sockaddr_in*>(name);

	if (IsUDPSocket(s) || target->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
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

	
	return SendSocks5Connect(s, target);
	
}

int WINAPI Pudge_bind(SOCKET s, const sockaddr* addr, int namelen) {
	return Real_bind(s, addr, namelen);
}

int WINAPI Pudge_closesocket(SOCKET s) {

	{
		std::unique_lock<std::shared_mutex> write_lock(SockMtx);
		if (activeAssociations.contains(s)) {
			Real_closesocket(activeAssociations[s].controlSocket);
			activeAssociations.erase(s);
		}
	}

	return Real_closesocket(s);
}

int WINAPI Pudge_sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
	if (len == 74) {
		Real_sendto(s, reinterpret_cast<const char*>(config.FakeUDPpayload), sizeof(config.FakeUDPpayload), flags, to, tolen);
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
	if (!config.ProxyMedia) {
		// 74 байта размер пейлода DISCORD IP DISCOVERY
		if (lpBuffers->len == 74) {
			Real_sendto(s, reinterpret_cast<const char*>(config.FakeUDPpayload), sizeof(config.FakeUDPpayload), 0, lpTo, iTolen);
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

	//const struct sockaddr_in* addr = reinterpret_cast<const struct sockaddr_in*>(lpTo);
	udp_association_t currAssoc{};
	{
		std::unique_lock<std::shared_mutex> write_lock(SockMtx);
		if (IsUDPSocket(s) && !activeAssociations.contains(s)) {
			InitSocksAssociation(currAssoc);
			{
				activeAssociations.insert(std::pair<SOCKET, udp_association_t>(s, currAssoc));
			}
		}
		else {
			currAssoc = activeAssociations.at(s);
		}
	}

	WSABUF destBuff;
	EncapsulateSocks5Datagram(&destBuff, lpBuffers->buf, lpBuffers->len, lpTo);

	auto status = Real_WSASendTo(
		s,
		&destBuff,
		1,
		lpNumberOfBytesSent,
		0,
		reinterpret_cast<const sockaddr*>(&currAssoc.proxyAddr),
		sizeof(currAssoc.proxyAddr),
		lpOverlapped,
		lpCompletionRoutine
	);

	//free(destBuff.buf);
	delete[] destBuff.buf;

	return status;
}

int WINAPI Pudge_WSARecvFrom(
	SOCKET s,
	LPWSABUF lpBuffers,
	DWORD dwBufferCount,
	LPDWORD lpNumberOfBytesRecvd,
	LPDWORD lpFlags,
	sockaddr* lpFrom,
	LPINT lpFromlen,
	LPWSAOVERLAPPED lpOverlapped,
	LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
	if (!config.ProxyMedia) {
		return Real_WSARecvFrom(
			s,
			lpBuffers,
			dwBufferCount,
			lpNumberOfBytesRecvd,
			lpFlags,
			lpFrom,
			lpFromlen,
			lpOverlapped,
			lpCompletionRoutine
		);
	}

	DWORD received = 0;
	if (lpNumberOfBytesRecvd == NULL) {
		lpNumberOfBytesRecvd = &received;
	}
		
	auto status = Real_WSARecvFrom(
		s,
		lpBuffers,
		dwBufferCount,
		lpNumberOfBytesRecvd,
		lpFlags,
		lpFrom,
		lpFromlen,
		lpOverlapped,
		lpCompletionRoutine
	);

	std::shared_lock<std::shared_mutex> read_lock(SockMtx);
	if (status == ERROR_SUCCESS && activeAssociations.contains(s)) {
		//Encapsulated header is 10 bytes
		if (*lpNumberOfBytesRecvd < 10) {
			return SOCKET_ERROR;
		}

		ExtractSockAddr(lpBuffers->buf, lpFrom);
		memmove(lpBuffers->buf, &lpBuffers->buf[10], *lpNumberOfBytesRecvd -= 10);
	}

	return status;
}

void HooksAttach() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)Real_WSASendTo, Pudge_WSASendTo);
	DetourAttach(&(PVOID&)Real_WSARecvFrom, Pudge_WSARecvFrom);
	//DetourAttach(&(PVOID&)Real_sendto, Pudge_sendto);
	DetourAttach(&(PVOID&)Real_bind, Pudge_bind);
	DetourAttach(&(PVOID&)Real_closesocket, Pudge_closesocket);
	DetourAttach(&(PVOID&)Real_connect, Pudge_connect);
	DetourTransactionCommit();
}

void HooksDetach() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)Real_WSASendTo, Pudge_WSASendTo);
	DetourDetach(&(PVOID&)Real_WSARecvFrom, Pudge_WSARecvFrom);
	//DetourDetach(&(PVOID&)Real_sendto, Pudge_sendto);
	DetourDetach(&(PVOID&)Real_bind, Pudge_bind);
	DetourDetach(&(PVOID&)Real_closesocket, Pudge_closesocket);
	DetourDetach(&(PVOID&)Real_connect, Pudge_connect);
	DetourTransactionCommit();
}