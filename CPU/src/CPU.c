/*
 ============================================================================
 Name        : CPU.c
 Author      : Servomotor
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>


int main(void)
{
	struct sockaddr_in direcciónServidor;

	direcciónServidor.sin_family = AF_INET;
	direcciónServidor.sin_addr.s_addr = inet_addr("127.0.0.1");
	direcciónServidor.sin_port = htons(8080);
	int cliente = socket(AF_INET, SOCK_STREAM, 0);

	if (connect(cliente, (void*) &direcciónServidor, sizeof(direcciónServidor)) != 0)
	{
		perror("No se pudo conectar");
		return 1;
	}
	return EXIT_SUCCESS;
}
