// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//

#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"

#define TARGET_IP	"10.0.55.53"

#define BUFFERS_LEN 2048


//#define SENDER
#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 5555
#define LOCAL_PORT 8888
#endif // SENDER

#ifdef RECEIVER
#define TARGET_PORT 8888
#define LOCAL_PORT 5555
#endif // RECEIVER


void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//**********************************************************************
int main()
{
	SOCKET socketS;

	InitWinsock();

	struct sockaddr_in local;
	struct sockaddr_in from;

	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;

	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding error!\n");
		getchar(); //wait for press Enter
		return 1;
	}

	//**********************************************************************
	char buffer_rx[BUFFERS_LEN];
	char buffer_tx[BUFFERS_LEN];

#ifdef SENDER

	
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);
	
	char* sendedFileName = "Deneme.mp4";
	size_t bytesRead;

	FILE* sender_file = fopen(sendedFileName, "rb");
	//Error message if we fail to open a file
	if (sender_file == NULL)
	{
		perror("The file could not be accessed.");
		return 1;
	}

	printf("Sending packet.\n");
	while ((bytesRead = fread(buffer_tx, 1, BUFFERS_LEN, sender_file)) > 0)
	{	
		//For Debug
		/*
		printf("%zu byte has been read:", bytesRead);
		for (size_t i = 0; i < bytesRead; i++)
		{
			printf("%c", buffer_tx[i]);
		}
		printf("\n");
		*/
		sendto(socketS, buffer_tx, bytesRead, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		memset(buffer_tx, 0, BUFFERS_LEN);
	}	
	printf("\nFile successfully send!");
	fclose(sender_file);
	closesocket(socketS);

#endif // SENDER

#ifdef RECEIVER

	int bytes_received;
	printf("Waiting for datagrams...\n");

	FILE* received_file = fopen("received_file", "ab");
	if (!received_file) {
		printf("Unable to open file.\n");
		return 1;
	}

	while (true) {
		bytes_received = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		if (bytes_received == SOCKET_ERROR) {
			printf("Socket error: %d\n", WSAGetLastError());
			break;
		}

		if (fwrite(buffer_rx, 1, bytes_received, received_file) != (size_t)bytes_received) {
			printf("Failed to write to file.\n");
			break;
		}
	}
	printf("File transfer completed.\n");

	closesocket(socketS);
	if (received_file) {
		fclose(received_file);
	}
#endif
	//**********************************************************************

	getchar(); //wait for press Enter
	return 0;
}
