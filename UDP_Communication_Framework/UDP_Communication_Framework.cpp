// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//

#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"
#include <cstdint>  // Includes ::std::uint32_t SONRADAN EKLENDI
#include <openssl/sha.h>
#include <zlib.h>
#include <zconf.h>
#include <string.h>
#include <chrono>
#include <thread>

#define TARGET_IP	"127.0.0.1"

#define BUFFERS_LEN 1024
#define CRC_LEN sizeof(uLong)
#define POSITION_LEN sizeof(long)
#define ACK_LEN 4

#define SENDER
//#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 5000
#define LOCAL_PORT 6001
#endif // SENDER

#ifdef RECEIVER
#define TARGET_PORT 8888
#define LOCAL_PORT 5555
#endif // RECEIVER

typedef struct packet {
	char data[BUFFERS_LEN];
}Packet;

typedef struct frame {
	int frame_kind; //ACK:0, SEQ:1 FIN:2
	int sq_no;
	int ack;
	Packet packet;
}Frame;


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
	char buffer_rx[BUFFERS_LEN + POSITION_LEN + CRC_LEN];
	char buffer_tx[BUFFERS_LEN + POSITION_LEN + CRC_LEN];

#ifdef SENDER

	Frame frame_recv;
	Frame frame_send;

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	char* sendedFileName = "Image.jpg";
	size_t bytesRead;
		
	FILE* sender_file = fopen(sendedFileName, "rb");
	//Error message if we fail to open a file
	if (sender_file == NULL)
	{
		perror("The file could not be accessed.");
		return 1;
	}

	long position = 0;

	printf("Sending packet.\n");
	while (true) {
		// Veri oku
		size_t bytesRead = fread(buffer_tx , 1, BUFFERS_LEN, sender_file);
		if (bytesRead <= 0) {
			break; // Dosya sonuna ulaşıldı
		}

		// Hash hesapla (SHA256)
		//unsigned char hash[SHA256_DIGEST_LENGTH];
		//SHA256(reinterpret_cast<const unsigned char*>(buffer_tx),BUFFERS_LEN, hash);
		
		// CRC hesapla (CRC32)
		uLong crc = crc32(0L, Z_NULL, 0);
		crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer_tx), bytesRead);
		// Hash ve CRC'yi paketin sonuna ekle

		memcpy(buffer_tx + bytesRead, &position, POSITION_LEN);
		memcpy(buffer_tx + bytesRead + POSITION_LEN, &crc, CRC_LEN);

		// Onay bekleyin
		char ackBuffer[ACK_LEN + POSITION_LEN];
		bool ackReceived = false; // ACK alınıp alınmadığını takip etmek için bir bayrak

		// Wait for acknowledgment
		while (!ackReceived) {
			// Send the packet
			sendto(socketS, buffer_tx, bytesRead + POSITION_LEN + CRC_LEN, 0, (struct sockaddr*)&addrDest, sizeof(addrDest));

			printf("Waiting for acknowledgment...\n");

			// Set a timeout for acknowledgment reception
			struct timeval timeout;
			timeout.tv_sec = 5; // Timeout duration in seconds
			timeout.tv_usec = 0;
			setsockopt(socketS, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

			// Receive acknowledgment
			int bytes_received = recvfrom(socketS, ackBuffer, sizeof(ackBuffer), 0, (sockaddr*)&from, &fromlen);

			long ack_position = 0;
			memcpy(&ack_position, ackBuffer + ACK_LEN, POSITION_LEN);
			printf("\nACK POSITION: %ld, POSITION: %ld\n", ack_position, position);
			if (bytes_received == SOCKET_ERROR) {
				printf("ACK reception error!\n");
			}
			// Check acknowledgment
			else if ((memcmp(ackBuffer, "ACK", 3) == 0) && (ack_position == position + BUFFERS_LEN)) {
				printf("\nACK received with correct package position...\n");
				position += bytesRead;
				memcpy(buffer_tx, &position, POSITION_LEN); // Pozisyon bilgisini paketin başına ekle
				ackReceived = true; // Set the flag to true upon receiving ACK
			}
			else if ((memcmp(ackBuffer, "NACK", 4) == 0)) {
				printf("\nNACK received OR wrong packet position. Resending packet...\n");
				// Resend the packet if NACK received or wrong packet position
			}
			memset(ackBuffer, 0, ACK_LEN + POSITION_LEN);
		}
		//Clear buffer
		memset(buffer_tx, 0, BUFFERS_LEN + POSITION_LEN + CRC_LEN);
	}
	sendto(socketS, "EOF", 3, 0, (struct sockaddr*)&addrDest, sizeof(addrDest));

	fclose(sender_file);
	closesocket(socketS);
	WSACleanup();

	printf("\nFile successfully sent!\n");


#endif // SENDER

#ifdef RECEIVER

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	memset(buffer_tx, 0, BUFFERS_LEN);
	char* fileName = "receivedFile";

	printf("\nWaiting for datagrams...\n");

	FILE* received_file = fopen("received_file", "ab");
	if (!received_file) {
		printf("\nUnable to open file.\n");
		return 1;
	}

	long current_networkPosition = 0;
	while (true) {
		memset(buffer_rx, 0, BUFFERS_LEN + POSITION_LEN + CRC_LEN);

		int bytes_received = recvfrom(socketS, buffer_rx, BUFFERS_LEN + POSITION_LEN + CRC_LEN, 0, (sockaddr*)&from, &fromlen);
		if (bytes_received == SOCKET_ERROR) {
			printf("\nSocket error!\n");
			break;
		}
		long received_networkPosition;
		//		unsigned char received_hash[SHA256_DIGEST_LENGTH];*
		uLong received_crc;
		char received_data[BUFFERS_LEN];

		memcpy(received_data, buffer_rx, bytes_received - POSITION_LEN - CRC_LEN);
		memcpy(&received_networkPosition, buffer_rx + bytes_received - POSITION_LEN - CRC_LEN, POSITION_LEN);
		memcpy(&received_crc, buffer_rx + bytes_received - CRC_LEN, CRC_LEN);

		//		printf("\nReceived data: %s\n", received_data);
//		printf("\nReceived network position: %ld\n", received_networkPosition);
//		printf("\nReceived CRC: %lu\n", received_crc);

		//		unsigned char calculated_hash[SHA256_DIGEST_LENGTH];
		//		SHA256(reinterpret_cast<const unsigned char*>(buffer_rx), bytes_received, calculated_hash);

		uLong crc = crc32(0L, Z_NULL, 0);
		crc = crc32(crc, reinterpret_cast<const Bytef*>(received_data), bytes_received - CRC_LEN - POSITION_LEN);

//		printf("\nCalculated CRC: %lu\n", crc);
		char ack[ACK_LEN];
		char ackBuffer[ACK_LEN + POSITION_LEN];

		// Check CRC and process the packet
		printf("CURRENT POSITION: %ld, RECEIVED POSITION: %ld", current_networkPosition, received_networkPosition);
		if (received_networkPosition == current_networkPosition){
			if (received_crc == crc) {
				current_networkPosition += BUFFERS_LEN;
				// Correct packet received
				printf("Received packet with correct CRC.\n");
				// Write received data to the 
				fseek(received_file, 0, SEEK_END);
				fwrite(received_data, 1, bytes_received - POSITION_LEN - CRC_LEN, received_file);
				// Send ACK
				strcpy(ack, "ACK");
				memcpy(ackBuffer, ack, 3);
				memcpy(ackBuffer + ACK_LEN, &current_networkPosition, POSITION_LEN);

				sendto(socketS, ackBuffer, ACK_LEN + POSITION_LEN, 0, (sockaddr*)&addrDest, fromlen);
			}
			else {
				// Incorrect CRC, send NACK
				printf("\nReceived packet with incorrect CRC. Retransmitting...\n");
				printf("\nSending acknowledgment: NACK\n");
				strcpy(ack, "NACK");
				memcpy(ackBuffer, ack, 4);
				memcpy(ackBuffer + ACK_LEN, &current_networkPosition, POSITION_LEN);
				sendto(socketS, ackBuffer, ACK_LEN + POSITION_LEN, 0, (sockaddr*)&addrDest, fromlen);
			}
		}
		else{
			//Received previous data again
			// Send ACK
			printf("\nReceived previous data again. Sending ACK\n");
			strcpy(ack, "ACK");
			memcpy(ackBuffer, ack, 3);
			memcpy(ackBuffer + ACK_LEN, &current_networkPosition, POSITION_LEN);
			sendto(socketS, ackBuffer, ACK_LEN + POSITION_LEN, 0, (sockaddr*)&addrDest, fromlen);
		}
	}
	fclose(received_file);
	closesocket(socketS);
	WSACleanup();
#endif // RECEIVER
	//**********************************************************************

	getchar(); //wait for press Enter
	return 0;
}
