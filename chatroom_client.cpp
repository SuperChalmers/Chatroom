// chatroom_client.cpp : Defines the entry point for the console application.
//

#define WIN32_LEAN_AND_MEAN

#define HAVE_STRUCT_TIMESPEC

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "5727"
#define SERVER_IP "127.0.0.1"
static char *token = "=token"; //token to let the client say something
static char *pass = "=pass"; //client response saying nothing to say
static char *end = "=end"; //client wants to leave
static char *who = "=who"; //ask who the client was 
static char *disconnect = "=dis"; //message from server to disconnect
SOCKET ConnectSocket = INVALID_SOCKET;
char globalbuf[DEFAULT_BUFLEN] = { 0 };
char clientName[DEFAULT_BUFLEN] = { 0 };

void * InputThread(void *data)
{
	while (true)
	{
		printf("Comment: \n");
		fgets(globalbuf, DEFAULT_BUFLEN, stdin);
	}
	return NULL;
}

int  main(int argc, char **argv)
{
	WSADATA wsaData;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	char sendbuf[DEFAULT_BUFLEN] = { 0 };
	char recvbuf[DEFAULT_BUFLEN] = { 0 };
	int iResult, s, irc;
	pthread_t thread;
	pthread_attr_t attr;

	//make the pthread detached. I don't need to join. It just keeps running until the client shuts down.
	s = pthread_attr_init(&attr);
	if (s != 0)
	{
		printf("Error init pthread attributes");
		return 1;
	}
	s = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (s != 0)
	{
		printf("pthread_attr_setdetachstate error");
		return 1;
	}
	s = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (s != 0)
	{
		printf("pthread_attr_setinheritsched error");
		return 1;
	}

	
	// Validate the parameters. You must send the name of the client.
	if (argc != 2)
	{
		printf("usage: %s client name\n", argv[0]);
		return 1;
	}
	
	strcat_s(sendbuf,argv[1]);
	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(SERVER_IP, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
	{
		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET)
		{
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int) ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET)
	{
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	// Send my login info
	//printf("Sending: %s\n", sendbuf);
	iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
	if (iResult == SOCKET_ERROR)
	{
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}
	irc = pthread_create(&thread, &attr, InputThread, reinterpret_cast<void*>(ConnectSocket));

	iResult = 0; //reset iresult
	while(TRUE)
	{
		memset(recvbuf, 0, DEFAULT_BUFLEN); //clear it befroe getting something new from the server
		iResult = recv(ConnectSocket, recvbuf, DEFAULT_BUFLEN, 0);
		if (iResult > 0)
		{
			if (strcmp(recvbuf, who) == 0)
			{
				//printf("Asked who I am. Responding: %s\n", sendbuf );
				iResult = send(ConnectSocket, sendbuf, (int) strlen(sendbuf), 0);
				if (iResult == SOCKET_ERROR)
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ConnectSocket);
					WSACleanup();
					return 1;
				}
			}
			else if (strcmp(recvbuf, token) == 0)
			{
				//printf("Sent the token.\n");
				if (strlen(globalbuf) == 0)
				{
					//printf("Nothing to say. Sending pass\n");
					iResult = send(ConnectSocket, pass, (int) strlen(pass), 0);
				}
				else
				{
					if (strcmp(globalbuf, "q\n") == 0)
					{
						printf("Quit request. Sending quit\n");
						iResult = send(ConnectSocket, end, (int) strlen(end), 0);
						memset(globalbuf, 0, DEFAULT_BUFLEN);
					}
					else
					{
					//printf("Something to say. Sending: %s\n", globalbuf);
					iResult = send(ConnectSocket, globalbuf, (int) strlen(globalbuf), 0);
					memset(globalbuf, 0, DEFAULT_BUFLEN);
					}
				}
			}
			else if (strcmp(recvbuf, disconnect) == 0)
			{
				printf("Disconnecting. Goodbye.\n");
				break;
			}
			else
			{
				//printf("Bytes received: %d\n", iResult);
				printf("Message recieved: %s\n", recvbuf);
			}
		}

	}


	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}


	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}


