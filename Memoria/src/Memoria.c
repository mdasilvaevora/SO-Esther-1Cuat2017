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
#include <malloc.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <commons/conexiones.h>

char* puertoMemoria;//4000
char* ipMemoria;
int marcos;
int marco_size;
int entradas_cache;
int cache_x_proc;
int retardo_memoria;
pthread_t thread_id;
t_config* configuracion_memoria;

typedef struct
{
	int frame;
	int pid;
	int num_pag;
}struct_adm_memoria;

char* bitMap;
void* frame_Memoria;

//the thread function
void *connection_handler(void *);

//--------------------Funciones Conexiones----------------------------//
int recibirConexion(int socket_servidor);
//----------------------Funciones Conexiones----------------------------//

char nuevaOrdenDeAccion(int puertoCliente);

void imprimirConfiguraciones();
void leerConfiguracion(char* ruta);
void inicializarMemoriaAdm();

int main_inicializarPrograma();//Falta codificar
int main_solicitarBytesPagina();//Falta codificar
int main_almacenarBytesPagina();//Falta codificar
int main_asignarPaginasAProceso();
int main_finalizarPrograma();//Falta codificar

//-----------------------FUNCIONES MEMORIA--------------------------//
int inicializarPrograma(int pid, int cantPaginas);//Falta codificar
char* solicitarBytesPagina(int pid,int pagina, int offset, int size);//Falta codificar
int almacenarBytesPagina(int pid,int pagina, int offset,int size, char* buffer);//Falta codificar
int asignarPaginasAProceso(int pid, int cantPaginas, int frame);//Falta codificar
int finalizarPrograma(int pid);//Falta codificar
//------------------------------------------------------------------//

void liberarBitMap(int pos, int size);
void ocuparBitMap(int pos, int size);
int verificarEspacio(int size);
void escribirEstructuraAdmAMemoria(int pid, int frame, int cantPaginas);
void borrarProgramDeStructAdms(int pid);
int buscarFrameDePaginaDeProceso(int pid, int pagina);

int main(void)
{
	leerConfiguracion("/home/utnso/workspace/tp-2017-1c-servomotor/Memoria/config_Memoria");
	imprimirConfiguraciones();

	bitMap = string_repeat('0',marcos);

	frame_Memoria= malloc(marco_size*marcos);

	inicializarMemoriaAdm();

	int socket_servidor = crear_socket_servidor(ipMemoria,puertoMemoria);
	recibirConexion(socket_servidor);

	return EXIT_SUCCESS;
}

void leerConfiguracion(char* ruta)
{
	configuracion_memoria = config_create(ruta);
	puertoMemoria = config_get_string_value(configuracion_memoria,"PUERTO");
	ipMemoria = config_get_string_value(configuracion_memoria,"IP_MEMORIA");
	marcos = config_get_int_value(configuracion_memoria,"MARCOS");
	marco_size = config_get_int_value(configuracion_memoria,"MARCO_SIZE");
	entradas_cache = config_get_int_value(configuracion_memoria,"ENTRADAS_CACHE");
	cache_x_proc = config_get_int_value(configuracion_memoria,"CACHE_X_PROC");
	retardo_memoria = config_get_int_value(configuracion_memoria,"RETARDO_MEMORIA");
}

void inicializarMemoriaAdm()
{
	int sizeMemoriaAdm = ((sizeof(int)*3*marcos)+marco_size-1)/marco_size;
	printf("Las estructuras administrativas ocupan %i paginas\n",sizeMemoriaAdm);
	printf("---------------------------------------------------\n");
	ocuparBitMap(0,sizeMemoriaAdm);
	struct_adm_memoria aux;
	int i = 0;
	int desplazamiento = sizeof(struct_adm_memoria);
	aux.pid=-1;
	aux.num_pag=-1;
	aux.frame = i;
	while(i < marcos)
	{
		memcpy(frame_Memoria, &aux, sizeof(struct_adm_memoria));
		i++;
		aux.frame = i;
		frame_Memoria = frame_Memoria + desplazamiento;
	}
	frame_Memoria = (frame_Memoria - desplazamiento*marcos);
}

int inicializarPrograma(int pid, int cantPaginas)
{
	printf("Inicializar Programa %d\n",pid);
	int posicionFrame = verificarEspacio(cantPaginas);
	if(posicionFrame >= 0)
	{
		ocuparBitMap(posicionFrame,cantPaginas);
		asignarPaginasAProceso(pid,cantPaginas,posicionFrame);
	}

	return posicionFrame;
}

char* solicitarBytesPagina(int pid,int pagina, int offset, int size)
{
	printf("Solicitar Bytes Pagina %d del proceso %d\n",pagina,pid);
	int frame = buscarFrameDePaginaDeProceso(pid,pagina);
	char* buffer;
	frame_Memoria = frame_Memoria + frame*sizeof(marco_size)+offset;
	memcpy(&buffer,frame_Memoria,size);

	return buffer;
}

int almacenarBytesPagina(int pid,int pagina, int offset,int size, char* buffer)
{
	printf("Almacenar Bytes A Pagina:%d del proceso:%d\n",pagina,pid);
	int frame = buscarFrameDePaginaDeProceso(pid,pagina);
	frame_Memoria = frame_Memoria + frame*sizeof(marco_size)+offset;
	memcpy(frame_Memoria,&buffer,size);
	return EXIT_SUCCESS;
}

int asignarPaginasAProceso(int pid, int cantPaginas, int posicionFrame)
{
	printf("Asignar Paginas A Proceso\n");
	escribirEstructuraAdmAMemoria(pid,posicionFrame,cantPaginas);
	return EXIT_SUCCESS;
}

int finalizarPrograma(int pid)
{
	printf("Finalizar Programa:%d\n",pid);
	borrarProgramDeStructAdms(pid);

	return EXIT_SUCCESS;
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
		printf("---------------------------------------------------\n");
	}

	addr_size = sizeof(their_addr);

	int socket_aceptado;
    while( (socket_aceptado = accept(socket_servidor, (struct sockaddr *)&their_addr, &addr_size)) )
    {
        puts("Connection accepted");

        if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &socket_aceptado) < 0)
        {
            perror("could not create thread");
            return 1;
        }
        puts("Handler asignado\n");
    }


	if (socket_aceptado == -1){
		close(socket_servidor);
		printf("Error al aceptar conexion\n");
		return 1;
	}

	return socket_aceptado;
}

char nuevaOrdenDeAccion(int puertoCliente)
{
	char *buffer;
	printf("Esperando Orden del Cliente\n");
	buffer = recibir(puertoCliente);
	//int size_mensaje = sizeof(buffer);
    if(buffer == NULL)
    {
        return 'Q';
    	//puts("Client disconnected");
        //fflush(stdout);
    }
    else if(buffer == NULL)
    {
        return 'X';
    	//perror("recv failed");
    }
    printf("El cliente %d envio el comando:",puertoCliente);
	printf("%c\n",*buffer);
	return *buffer;
}

int main_inicializarPrograma(int sock)
{
	return main_asignarPaginasAProceso(sock);
}
int main_solicitarBytesPagina(int sock)
{
	int pid;
	int pagina;
	pid=atoi((char*)recibir(sock));
	printf("PID:%d\n",pid);
	pagina=atoi((char*)recibir(sock));
	printf("Pagina:%d\n",pagina);
	int offset;
	int size;
	offset=atoi((char*)recibir(sock));
	printf("Offset:%d\n",offset);
	size=atoi((char*)recibir(sock));
	printf("Size:%d\n",size);
	char *mensaje = solicitarBytesPagina(pid,pagina,offset,size);
	enviar_string(sock, mensaje);
	return 0;
}
int main_almacenarBytesPagina(int sock)
{
	int pid;
	int pagina;
	pid=atoi((char*)recibir(sock));
	printf("PID:%d\n",pid);
	pagina=atoi((char*)recibir(sock));
	printf("Pagina:%d\n",pagina);
	int offset;
	int size;
	offset=atoi((char*)recibir(sock));
	printf("Offset:%d\n",offset);
	size=atoi((char*)recibir(sock));
	printf("Size:%d\n",size);
	char* bytes = recibir_string(sock);
	almacenarBytesPagina(pid,pagina,offset,size,bytes);
	return 0;
}
int main_asignarPaginasAProceso(int sock)
{
	int pid;
	int cantPaginas;
	pid=atoi((char*)recibir(sock));
	printf("PID:%d\n",pid);
	cantPaginas=atoi((char*)recibir(sock));
	printf("CantPaginas:%d\n",cantPaginas);
	int posicionFrame = verificarEspacio(cantPaginas);
	printf("Posicion Frame: %d\n",posicionFrame);
	printf("Bitmap:%s\n",bitMap);
	if(posicionFrame >= 0)
	{
		ocuparBitMap(posicionFrame,cantPaginas);
		asignarPaginasAProceso(pid,cantPaginas,posicionFrame);
		return 0;
	}
	else
	{
		printf("No hay espacio suficiente en la memoria\n");
		return -1;
	}
}
int main_finalizarPrograma(int sock)
{
	int pid;
	pid=atoi((char*)recibir(sock));
	finalizarPrograma(pid);
	return 0;
}

void liberarBitMap(int pos, int size)
{
	int i;
	for (i=0; i< size; i++)
	{
		bitMap[pos+i] = (char) '0';
	}
}

void ocuparBitMap(int pos, int size)
{
	int i;
	for (i=0; i< size; i++)
	{
		bitMap[pos+i] = (char) '1';
	}
}

int verificarEspacio(int size)
{
	int pos = 0, i = 0, espacioLibre = 0;
	while((espacioLibre < size) && (i < marcos))
	{
		if(bitMap[i] == '0')
		{
			if(pos != i)
			{
				espacioLibre ++;
			}
			else
			{
				pos =i;
				espacioLibre ++;
			}
		}
		else
		{
			espacioLibre = 0;
			pos = i +1;
		}
		i++;
	}
	if(espacioLibre == size)
	{
		return pos;
	}
	else
	{
		return -1;
	}
}


/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    char orden = 'F';
    int resultadoDeEjecucion;
    char *buffer;
	while(orden != 'Q')
	{
		orden = nuevaOrdenDeAccion(sock);
		switch(orden)
		{
		case 'C':
			printf("Esperando mensaje\n");
			buffer = recibir_string(sock);
			printf("\nEl mensaje es: \"%s\"\n", buffer);

			resultadoDeEjecucion = main_inicializarPrograma(sock);
			break;
		case 'S':
			resultadoDeEjecucion = main_solicitarBytesPagina(sock);
			break;
		case 'A':
			resultadoDeEjecucion = main_almacenarBytesPagina(sock);
			break;
		case 'G':
			resultadoDeEjecucion = main_asignarPaginasAProceso(sock);
			break;
		case 'F':
			resultadoDeEjecucion = main_finalizarPrograma(sock);
			break;
		case 'Q':
			printf("Cliente %d desconectado",sock);
			fflush(stdout);
			break;
		case 'X':
			perror("recv failed");
			break;
		default:
			printf("Error: Orden %c no definida\n",orden);
			break;
		}
		printf("Resultado de ejecucion:%d\n",resultadoDeEjecucion);
	}
    return 0;
}

void escribirEstructuraAdmAMemoria(int pid, int frame, int cantPaginas)
{
	struct_adm_memoria aux;
	int i = 0;
	int desplazamiento = sizeof(struct_adm_memoria);
	aux.pid=pid;
	aux.num_pag=0;
	aux.frame = frame;
	frame_Memoria = frame_Memoria + desplazamiento*frame;
	while(i < cantPaginas)
	{
		memcpy(frame_Memoria, &aux, sizeof(struct_adm_memoria));
		aux.num_pag++;
		aux.frame++;
		frame_Memoria = frame_Memoria + desplazamiento;
		i++;
	}
	frame_Memoria = frame_Memoria - desplazamiento*frame;
	frame_Memoria = frame_Memoria - desplazamiento*cantPaginas;
}

void borrarProgramDeStructAdms(int pid)
{
	int i = 0;
	int desplazamiento = sizeof(struct_adm_memoria);
	struct_adm_memoria aux;
	while(i<marcos)
	{
		memcpy(&aux,frame_Memoria,sizeof(struct_adm_memoria)); //Revisar si esto funciona
		if(aux.pid == pid) //Si el PID del programa en mi estructura Aadministrativa es igual al del programa que quiero borrar
		{
			liberarBitMap(aux.frame,1); //Marco la posición del frame que me ocupa esa página en particular de ese programa como vacía
			aux.num_pag = -1;
			aux.pid = -1;
			memcpy(frame_Memoria,&aux,sizeof(struct_adm_memoria)); //Marco que en ese frame no tengo un programa asignado
		}
		i++;
		frame_Memoria = frame_Memoria + desplazamiento; //Muevo el puntero de frame_Memoria
	}
	frame_Memoria = frame_Memoria - desplazamiento*marcos; //Coloco el puntero de mi frame_Memoria al inicio
}

int buscarFrameDePaginaDeProceso(int pid, int pagina)
{
	int i = 0;
	int desplazamiento = sizeof(struct_adm_memoria);
	struct_adm_memoria aux;
	int frame = -1;
	while(i<marcos)
	{
		memcpy(&aux,frame_Memoria,sizeof(struct_adm_memoria)); //Revisar si esto funciona
		if(aux.pid == pid && aux.num_pag == pagina) //Si el PID del programa en mi estructura Aadministrativa es igual al del programa que quiero borrar
		{
			frame = aux.frame;
		}
		i++;
		frame_Memoria = frame_Memoria + desplazamiento; //Muevo el puntero de frame_Memoria
	}
	frame_Memoria = frame_Memoria - desplazamiento*marcos; //Coloco el puntero de mi frame_Memoria al inicio
	return frame;
}

void imprimirConfiguraciones(){
		printf("---------------------------------------------------\n");
		printf("CONFIGURACIONES\nIP:%s\nPUERTO:%s\nMARCOS:%d\nTAMAÑO MARCO:%d\nENTRADAS CACHE:%d\nCACHE POR PROCESOS:%d\nRETARDO MEMORIA:%d\n",ipMemoria,puertoMemoria,marcos,marco_size,entradas_cache,cache_x_proc,retardo_memoria);
		printf("---------------------------------------------------\n");
}
