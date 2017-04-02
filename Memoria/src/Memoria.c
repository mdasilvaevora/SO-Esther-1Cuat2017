/*
 ============================================================================
 Name        : Memoria.c
 Author      : Servomotor
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <sys/epoll.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <commons/string.h>
#include <commons/config.h>

//#define RUTA_LOG "/home/utnso/memoria.log"

t_config* configuracion_memoria;
int puertoMemoria;
int marcos;
int marco_size;
int entradas_cache;
int cache_x_proc;
int retardo_memoria;

//Revisar y discutir estructuras

/*
typedef struct
{
	char datosMarco[marco_size];
	//Revisar
}t_Memoria;
*/

typedef struct
{
	int marco;
	int pid;
	int num_pag;
}struct_adm_memoria;

int crear_socket_servidor(char *ip, char *puerto);
int recibirConexion(int socket_servidor);
char* recibir_string_generico(int socket_aceptado);
void enviar_string(int socket, char * mensaje);
void* recibir(int socket);
void enviar(int socket, void* cosaAEnviar, int tamanio);


void leerConfiguracion(char* ruta);//Revisar junto a estructura
void inicializarMemoria(char* ruta);//Falta codificar

//-----------------------FUNCIONES MEMORIA--------------------------//
int inicializarPrograma(int pid, int cantPaginas);//Falta codificar
int solicitarBytesPagina(int pid,int pagina, int offset, int size);//Falta codificar
int almacenarBytesPagina(int pid,int pagina, int offset,int size, char* buffer);//Falta codificar
int asignarPaginaAProceso(int pid, int cantPaginas);//Falta codificar
int finalizarPrograma(int pid);//Falta codificar
//------------------------------------------------------------------//

int main(void)
{
	int socket_servidor = crear_socket_servidor("127.0.0.1","4040");
	int cliente = recibirConexion(socket_servidor);
	return EXIT_SUCCESS;
}

void leerConfiguracion(char* ruta)
{
	configuracion_memoria = config_create(ruta);
	puertoMemoria = config_get_int_value(configuracion_memoria,"PUERTO");
	marcos = config_get_int_value(configuracion_memoria,"MARCOS");
	marco_size = config_get_int_value(configuracion_memoria,"MARCO_SIZE");
	entradas_cache = config_get_int_value(configuracion_memoria,"ENTRADA_CACHE");
	cache_x_proc = config_get_int_value(configuracion_memoria,"CACHE_X_PROC");
	retardo_memoria = config_get_int_value(configuracion_memoria,"RETARDO_MEMORIA");
}

void inicializarMemoria(char* ruta)
{

}

int inicializarPrograma(int pid, int cantPaginas)
{
	return EXIT_SUCCESS;
}

int solicitarBytesPagina(int pid,int pagina, int offset, int size)
{
	return EXIT_SUCCESS;
}

int almacenarBytesPagina(int pid,int pagina, int offset,int size, char* buffer)
{
	return EXIT_SUCCESS;
}

int asignarPaginaAProceso(int pid, int cantPaginas)
{
	return EXIT_SUCCESS;
}

int finalizarPrograma(int pid)
{
	return EXIT_SUCCESS;
}

int crear_socket_servidor(char *ip, char *puerto){
    int descriptorArchivo, estado;
    struct addrinfo hints, *infoServer, *n;

    memset(&hints,0,sizeof (struct addrinfo));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((estado = getaddrinfo(ip, puerto, &hints, &infoServer)) != 0){
        fprintf(stderr, "Error en getaddrinfo: %s", gai_strerror(estado));

        return -1;
    }

    for(n = infoServer; n != NULL; n = n->ai_next){
        descriptorArchivo = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
        if(descriptorArchivo != -1) break;
    }

    if(descriptorArchivo == -1){
        perror("Error al crear el socket");
        freeaddrinfo(infoServer);

        return -1;
    }

    int si = 1;

    if(setsockopt(descriptorArchivo,SOL_SOCKET,SO_REUSEADDR,&si,sizeof(int)) == -1){
    	perror("Error en setsockopt");
        close(descriptorArchivo);
        freeaddrinfo(infoServer);

        return -1;
    }

    if (bind(descriptorArchivo, n->ai_addr, n->ai_addrlen) == -1){
    	perror("Error bindeando el socket");
        close(descriptorArchivo);
        freeaddrinfo(infoServer);

        return -1;
    }

    freeaddrinfo(infoServer);

    return descriptorArchivo;
}

int recibirConexion(int socket_servidor){
	struct sockaddr_storage their_addr;

	socklen_t addr_size;

	int estado = listen(socket_servidor, 5);

	if(estado == -1){
		printf("Error al poner el servidor en listen\n");
		close(socket_servidor);
		return 1;
	}

	if(estado == 0){
		printf("Se puso el socket en listen\n");
	}

	addr_size = sizeof(their_addr);
	int socket_aceptado = accept(socket_servidor, (struct sockaddr *)&their_addr, &addr_size);

	if (socket_aceptado == -1){
		close(socket_servidor);
		printf("Error al aceptar conexion\n");
		return 1;
	}

	return socket_aceptado;
}

char* recibir_string_generico(int socket_aceptado){
	return (char*) recibir(socket_aceptado);
}

void enviar_string(int socket, char* mensaje){
	int tamanio = string_length(mensaje) + 1;

	enviar(socket, (void*) mensaje, tamanio);
}

void enviar(int socket, void* cosaAEnviar, int tamanio){
	void* mensaje = malloc(sizeof(int) + tamanio);
	void* aux = mensaje;
	*((int*)aux) = tamanio;
	aux += sizeof(int);
	memcpy(aux, cosaAEnviar, tamanio);

	send(socket, mensaje, sizeof(int) + tamanio, 0);
	free(mensaje);
}

void* recibir(int socket){
	int checkSocket = -1;

	void* recibido = malloc(sizeof(int));

	checkSocket = read(socket, recibido, sizeof(int));

	int tamanioDelMensaje = *((int*)recibido);

	free(recibido);

	if(!checkSocket) return NULL;

	recibido = malloc(tamanioDelMensaje);

	int bytesRecibidos = 0;

	while(bytesRecibidos < tamanioDelMensaje && checkSocket){
		checkSocket = read(socket, (recibido + bytesRecibidos), (tamanioDelMensaje - bytesRecibidos));
		bytesRecibidos += checkSocket;
	}

	return !checkSocket ? NULL:recibido;
}
