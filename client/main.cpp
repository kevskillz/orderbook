#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    SOCKET ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientService.sin_port = htons(54000);

    if (connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect." << std::endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    } else {
        std::cout << "Connected to server." << std::endl;
    }

    std::string order = "buy 100.5 10";
    int sent = send(ConnectSocket, order.c_str(), (int)order.size(), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "Failed to send order." << std::endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    } else {
        std::cout << "Sent order: '" << order << "'" << std::endl;
    }

    char recvbuf[512];
    int bytesReceived = recv(ConnectSocket, recvbuf, 512, 0);
    if (bytesReceived > 0) {
        recvbuf[bytesReceived] = '\0';
        std::cout << "Server: " << recvbuf << std::endl;
    } else if (bytesReceived == 0) {
        std::cerr << "Connection closed by server." << std::endl;
    } else {
        std::cerr << "Failed to receive response." << std::endl;
    }

    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
} 