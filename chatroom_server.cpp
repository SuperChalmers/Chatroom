// chatroom_server.cpp : Defines the entry point for the console application.
//

#pragma comment(lib, "Ws2_32.lib")
#define HAVE_STRUCT_TIMESPEC
#include <pthread.h>
#include <stdio.h>  
#include <stdlib.h>
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>


#define PORT "5727"
#define DEFAULT_BUFLEN 1024
#define MAX_CLIENTS 5
int threadCount = 0; //how many thread there are
pthread_mutex_t lock; //mutex on the token
SOCKET ClientSocket[MAX_CLIENTS];
SOCKET listenSocket = INVALID_SOCKET;

void broadcastMessage(int me, char * buffer)
{
	int iSendResult;
	for (int j = 0; j < threadCount; j++)
	{
		if (j != me) // don't send the same message to yourself.
		{
			iSendResult = send(ClientSocket[j], buffer, (int) strlen(buffer), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				printf("send  to client %d failed: %d\n", j, WSAGetLastError());
				continue;
			}
		}
	}

}
void * AcceptThread(void *data)
{

	static char *waiting = "[Waiting...]";
	char buffer[DEFAULT_BUFLEN] = { 0 };
	static char *joined = " joined.";
	int iResult = 0, iSendResult = 0;

	// Accept a client socket
	printf("%s\n", waiting);
	while (ClientSocket[threadCount] = accept(listenSocket, NULL, NULL))
	{
		if (ClientSocket[threadCount] == INVALID_SOCKET)
		{
			printf("accept failed: %d\n", WSAGetLastError());
			continue;
		}

		iResult = recv(ClientSocket[threadCount], buffer, DEFAULT_BUFLEN, 0);
		if (iResult > 0)
		{
			printf("Bytes received: %d\n", iResult);
			strcat_s(buffer, joined);
			printf("%s\n", buffer);

			//tell all other clients this client joined
			for (int i = 0; i < (threadCount - 1); i++) //-1 since I don't need to tell the new client they joined
			{
				iSendResult = send(ClientSocket[i], buffer, sizeof(buffer), 0);
				if (iSendResult == SOCKET_ERROR)
				{
					printf("send failed: %d\n", WSAGetLastError());
					continue;
				}
			}
		}
		else
		{
			printf("recv failed: %d\n", WSAGetLastError());
			continue;
		}
		pthread_mutex_lock(&lock); //lock the mutex to be safe
		threadCount++; //increment the count
		pthread_mutex_unlock(&lock); //done changing. Free the mutex. 
		printf("%s\n", waiting);
		memset(buffer, 0, DEFAULT_BUFLEN);
	}

	return NULL;
}
int main(int argc, char const *argv[])
{
	
	int iResult, iSendResult, irc, s;
	pthread_t thread;
	pthread_attr_t attr;
	static char *token = "=token"; //token to let the client say something
	static char *pass = "=pass"; //client response saying nothing to say
	static char *end = "=end"; //client wants to leave
	static char *who = "=who"; //ask who the client was 
	static char *left = " left."; //Text to append for leaving
	static char *disconnect = "=dis"; //message from server to disconnect

	char buffer[DEFAULT_BUFLEN] = { 0 };

	//make the pthread detached. I don't need to join. It just keeps running until the server dies.
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


	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	if (pthread_mutex_init(&lock, NULL) != 0) //define the mutex
	{
		printf("\n Mutex init has failed\n");
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.                                  */
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, PORT, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}
	iResult = bind(listenSocket, result->ai_addr, (int) result->ai_addrlen);
	if(iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}
	freeaddrinfo(result); //don't need this structure anymore.
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	//create the thread that just listens. It goes for forever. this is the thread with detached set.
	irc = pthread_create(&thread, &attr, AcceptThread, reinterpret_cast<void*>(listenSocket));
	while (TRUE) //keep looping through this until ? Should I put in a server kill option?
	{
		for (int i = 0; i < threadCount; i++)
		{

			iSendResult = send(ClientSocket[i], token, (int)strlen(token), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				printf("send failed: %d\n", WSAGetLastError());
				continue;
			}
			memset(buffer, 0, DEFAULT_BUFLEN);
			iResult = recv(ClientSocket[i], buffer, DEFAULT_BUFLEN, 0);
			if (strcmp(buffer, pass) == 0)
			{
				//printf("client # %d has nothing to say\n", i);
			}
			else if (strcmp(buffer, end) == 0) //client asked to leave
			{
				iSendResult = send(ClientSocket[i], who, (int)strlen(who), 0); //ask the client who it is 
				memset(buffer, 0, DEFAULT_BUFLEN);
				iResult = recv(ClientSocket[i], buffer, DEFAULT_BUFLEN, 0);
				strcat_s(buffer, left);
				printf("%s\n", buffer);
				broadcastMessage(i, buffer);
				printf("Sending disconnect message to client: %s\n", disconnect);
				printf("[Waiting...]\n"); //because it was said in the earlier loop and won't get said again.
				iSendResult = send(ClientSocket[i], disconnect, (int) strlen(disconnect), 0);
				for (int j = (i + 1); j < threadCount; j++) //move things down if the client that went away is in the middle
				{
					ClientSocket[i++] = ClientSocket[j];
				}
				pthread_mutex_lock(&lock); //lock the mutex to be safe
				threadCount--; //increment the count
				pthread_mutex_unlock(&lock); //done changing. Free the mutex. 
			}
			else
			{
				//printf("Recievd %d bytes", iResult);
				broadcastMessage(i, buffer);
			}
		}
	}

	pthread_mutex_destroy(&lock);	
   return 0;
}

