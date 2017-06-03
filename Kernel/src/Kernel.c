/*
 ============================================================================
 Name        : Kernel.c
 Author      : Servomotor
 Version     :
 Copyright   : Your copyright notice
 Description : Kernel Module
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
#include <commons/collections/list.h>
#include <commons/log.h>
#include <parser/metadata_program.h>
#include <parser/parser.h>
#include "pcb.h"
#include "conexiones.h"
#include "interfazHandler.h"
#include "sincronizacion.h"
#include "planificacion.h"
#include "configuraciones.h"
#include "conexionMemoria.h"

char** tablaGlobalArchivos;
typedef struct FS{//Para poder guardar en la lista
	char** tablaArchivoPorProceso;
}t_tablaArchivoPorProceso;
t_list* listaTablasArchivosPorProceso;
void handshakeFS();

//--------ConnectionHandler--------//
void connectionHandler(int socketAceptado, char orden);
//---------ConnectionHandler-------//

//------InterruptHandler-----//
void interruptHandler(int socket,char orden);
int buscarSocketHiloPrograma(int pid);
void buscarProcesoYTerminarlo(int pid);
void eliminarSocket(int socket);

//------InterruptHandler-----//


//---------Conexiones---------------//
void *get_in_addr(struct sockaddr *sa);
void nuevaOrdenDeAccion(int puertoCliente, char nuevaOrden);
void selectorConexiones();
void eliminarSockets(int socketConsolaGlobal,int* procesosAFinales);
fd_set master;
int flagFinalizar;
//---------Conexiones-------------//

//------------Sockets unicos globales--------------------//
void inicializarSockets();
int socketFyleSys;
int socketServidor; // Para CPUs y Consolas
//------------Sockets unicos globales-------------------//


void inicializarListas();

int main(void) {
	leerConfiguracion("/home/utnso/workspace/tp-2017-1c-servomotor/Kernel/config_Kernel");
	imprimirConfiguraciones();
	imprimirInterfazUsuario();
	inicializarSockets();
	gradoMultiProgramacion=0;
	inicializarSemaforos();

	inicializarLog("/home/utnso/Log/logKernel.txt");
	inicializarListas();
	handshakeMemoria();
	handshakeFS();
	pthread_create(&planificadorCortoPlazo, NULL,planificarCortoPlazo,NULL);
	pthread_create(&planificadorLargoPlazo, NULL,(void*)planificarLargoPlazo,NULL);

	selectorConexiones(socketServidor);

	return 0;
}


void handshakeFS(){
	char comandoTamanioPagina = 'V';
	char* archivoAVerificar="alumnoosdad.bin";
	int tamano=strlen(archivoAVerificar);
	int validado;
	send(socketFyleSys,&comandoTamanioPagina,sizeof(char),0);
	send(socketFyleSys,&tamano,sizeof(int),0);
	send(socketFyleSys,archivoAVerificar,tamano,0);
	recv(socketFyleSys,&validado,sizeof(int),0);

}


void connectionHandler(int socketAceptado, char orden) {

	char comandoEnviarPcb='S';
	if(orden == '\0')nuevaOrdenDeAccion(socketAceptado, orden);
	_Bool verificarPid(t_consola* pidNuevo){
		return (pidNuevo->socketHiloPrograma == socketAceptado);
	}
	_Bool verificaSocket(t_cpu* cpu){
			return (cpu->socket == socketAceptado);
		}

	switch (orden) {
		case 'I':
					atenderNuevoPrograma(socketAceptado);
					break;
		case 'N':
					pthread_mutex_lock(&mutexListaCPU);
					t_cpu* cpu = malloc(sizeof(t_cpu));
					cpu->socket = socketAceptado;
					list_add(listaCPU,cpu);
					pthread_mutex_unlock(&mutexListaCPU);
					send(socketAceptado,&comandoEnviarPcb,sizeof(char),0);
					sem_post(&sem_CPU);
					break;
		case 'T':
					log_info(loggerConPantalla,"\nProceso finalizado exitosamente desde CPU con socket : %d asignado",socketAceptado);
					terminarProceso(socketAceptado);
					break;
		case 'P':
				send(socketAceptado,&config_paginaSize,sizeof(int),0);
				send(socketAceptado,&stackSize,sizeof(int),0);
				break;
		case 'X':
				recv(socketAceptado,&orden,sizeof(char),0);
				interruptHandler(socketAceptado,orden);
			break;
		default:
				if(orden == '\0') break;
				log_warning(loggerConPantalla,"\nOrden %c no definida\n", orden);
				break;
		} // END switch de la consola

	pthread_mutex_unlock(&mutexConexion);
	orden = '\0';
	return;

}

void interruptHandler(int socketAceptado,char orden){
	log_info(loggerConPantalla,"Ejecutando interrupt handler\n");
	int size;
	int pid;
	int socketHiloPrograma;
	int i=0;
	char* mensaje; /*TODO: Ver el tema de pedir memoria y liberar esta variable siempre que se pueda*/

	switch(orden){
	case 'A':
		log_info(loggerConPantalla,"Informando a Consola excepcion por problemas al reservar recursos\n");
		mensaje="El programa ANSISOP no puede iniciar actualmente debido a que no se pudo reservar recursos para ejecutar el programa, intente mas tarde";
		size=strlen(mensaje);
		informarConsola(socketAceptado,mensaje,size);
		log_info(loggerConPantalla,"El programa ANSISOP enviado por socket: %d ha sido expulsado del sistema e se ha informado satifactoriamente",socketAceptado);
		break;
	case 'P':
		log_info(loggerConPantalla,"Iniciando rutina para imprimir por consola\n");
		recv(socketAceptado,&size,sizeof(int),0);
		mensaje=malloc(size);
		recv(socketAceptado,&mensaje,size,0);
		recv(socketAceptado,&pid,sizeof(int),0);

		socketHiloPrograma = buscarSocketHiloPrograma(pid);
		informarConsola(socketHiloPrograma,mensaje,size);
		log_info(loggerConPantalla,"Rutina para imprimir finalizo ----- PID: %d\n", pid);
			break;
	case 'M':
		log_info(loggerConPantalla,"Informando a Consola excepcion por grado de multiprogramacion\n");
		mensaje = "El programa ANSISOP no puede iniciar actualmente debido a grado de multiprogramacion, se mantendra en espera hasta poder iniciar";
		size=strlen(mensaje);
		informarConsola(socketAceptado,mensaje,size);
		break;
	case 'C':
		log_info(loggerConPantalla,"La CPU de socket %d se ha cerrado por  signal\n",socketAceptado);
		/*TODO: Eliminar la CPU que se desconecto. De la lista de CPUS y de la lista de SOCKETS*/
		break;
	case 'E':
		log_info(loggerConPantalla,"La Consola con socket %d asignado se ha cerrado por signal \n",socketAceptado);
		recv(socketAceptado,&size,sizeof(int),0);
		mensaje = malloc(size);
		recv(socketAceptado,mensaje,size,0);
		strcpy(mensaje+size,"\0");
		int* procesosAFinalizar = malloc(size);
		for(i=0;i<size;i++){
			memcpy(procesosAFinalizar,mensaje,sizeof(int));
			mensaje += sizeof(int);
		}
		//printf("%d\n", procesosAFinalizar[0]);
		/*TODO:Hay que buscar a todos los procesosAFinalizar en la cola de los estados, y llevarlos a la cola de terminados*/

		eliminarSockets(socketAceptado,procesosAFinalizar);
		break;
	case 'F':
		log_info(loggerConPantalla,"La consola con socket: %d asignado ha solicitado finalizar un proceso ",socketAceptado);
		recv(socketAceptado,&pid,sizeof(int),0);
		buscarProcesoYTerminarlo(pid);
		mensaje = "El proceso ha sido finalizado";
		size=strlen(mensaje);
		informarConsola(socketAceptado,mensaje,size);
		log_info(loggerConPantalla,"Proceso finalizado-----PID: %d",pid);
		break;
	default:
			break;
	}
}

void eliminarSocket(int socket){
	close(socket);
	pthread_mutex_lock(&mutex_FDSET);
	FD_CLR(socket,&master);
	pthread_mutex_unlock(&mutex_FDSET);
}

void buscarProcesoYTerminarlo(int pid){
	t_pcb* procesoAEliminar;
	_Bool verificarPid(t_pcb* pcb){
			return (pcb->pid==pid);
		}
	_Bool verificarPidCPU(t_cpu* cpu){
				return (cpu->pid==pid);
			}

	pthread_mutex_lock(&mutexColaNuevos);
	if(list_any_satisfy(colaNuevos,(void*)verificarPid)){
		procesoAEliminar=list_remove_by_condition(colaNuevos,(void*)verificarPid);
	}
	pthread_mutex_unlock(&mutexColaNuevos);
	pthread_mutex_lock(&mutexColaListos);
	if(list_any_satisfy(colaListos,(void*)verificarPid)){
			procesoAEliminar=list_remove_by_condition(colaListos,(void*)verificarPid);
		}
	pthread_mutex_unlock(&mutexColaListos);
	sem_wait(&sem_colaReady);
	/*pthread_mutex_lock(&mutexColaEjecucion); TODO: Hay que ver como hacer este temita
	if(list_any_satisfy(colaEjecucion,(void*)verificarPid)){
		procesoAEliminar=list_remove_by_condition(colaEjecucion,(void*)verificarPid);
		t_cpu*cpu = list_remove_by_condition(listaCPU,(void*)verificarPidCPU);
		list_add(listaCPU,cpu);
		terminarProceso(cpu->socket);
		}
	pthread_mutex_unlock(&mutexColaEjecucion);*/

	pthread_mutex_lock(&mutexColaTerminados);
	list_add(colaTerminados,procesoAEliminar);
	pthread_mutex_unlock(&mutexColaTerminados);
}

void eliminarSockets(int socketConsolaGlobal,int* procesosAFinalizar){
			eliminarSocket(socketConsolaGlobal);

	/*TODO: Buscar por pid y borrar los socket de los hilos programas*/
}

int buscarSocketHiloPrograma(int pid){

	_Bool verificarPid(t_consola* consolathread,int pid){
		return (consolathread->pid == pid);
	}
	int socketHiloConsola;
	t_consola* consolathread = list_find(listaConsolas,(void*)verificarPid);
	socketHiloConsola =consolathread->socketHiloPrograma;
	free(consolathread);
	return socketHiloConsola;
}

void enviarAImprimirALaConsola(int socketConsola, void* buffer, int size){
	void* mensajeAConsola = malloc(sizeof(int)*2 + sizeof(char));

	memcpy(mensajeAConsola,&size, sizeof(char));
	memcpy(mensajeAConsola + sizeof(int)+ sizeof(char), buffer,size);
	send(socketConsola,mensajeAConsola,sizeof(char)+ sizeof(int),0);
}


void inicializarSockets(){
		socketServidor = crear_socket_servidor(ipServidor, puertoServidor);
		socketMemoria = crear_socket_cliente(ipMemoria, puertoMemoria);
		socketFyleSys = crear_socket_cliente(ipFileSys, puertoFileSys);
}

void inicializarListas(){
	colaNuevos= list_create();
	colaListos= list_create();
	colaTerminados= list_create();
	listaConsolas = list_create();
	listaCPU = list_create();
	listaCodigosProgramas=list_create();
	listaTablasArchivosPorProceso=list_create();
	colaEjecucion = list_create();
}


void nuevaOrdenDeAccion(int socketCliente, char nuevaOrden) {
		log_info(loggerConPantalla,"\n--Esperando una orden del cliente %d-- \n", socketCliente);
		recv(socketCliente, &nuevaOrden, sizeof nuevaOrden, 0);
		log_info(loggerConPantalla,"El cliente %d ha enviado la orden: %c\n", socketCliente, nuevaOrden);
}
void selectorConexiones() {
	log_info(loggerConPantalla,"Iniciando selector de conexiones");
	int fdMax; // es para un contador de los sockets que hay
	int newfd; // newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr; // client address
	int i; // Contador para las iteracion dentro del FDSET
	int nbytes; // El tamanio de los datos que se recibe por recv
	char orden;
	char remoteIP[INET6_ADDRSTRLEN];
	socklen_t addrlen;
	fd_set readFds; // temp file descriptor list for select()
// listen
	if (listen(socketServidor, 15) == -1) {
	perror("listen");
	exit(1);
	}
	FD_SET(socketServidor, &master); // add the listener to the master set
	FD_SET(0, &master); // Agrega el fd del teclado.
	fdMax = socketServidor; // keep track of the biggest file descriptor so far, it's this one
	for (;;) {
					readFds = master;
					if (select(fdMax + 1, &readFds, NULL, NULL, NULL) == -1) {
					perror("select");
					log_error(loggerSinPantalla,"Error en select\n");
					exit(2);
					}
					for (i = 0; i <= fdMax; i++) {
							if (FD_ISSET(i, &readFds)) { // we got one!!
							/**********************************************************/
										if(i == 0){
										interfazHandler();//Recibe a si mismo
										}
									/************************************************************/
									if (i == socketServidor) {
									addrlen = sizeof remoteaddr;
									newfd = accept(socketServidor, (struct sockaddr *) &remoteaddr,&addrlen);
											if (newfd == -1) {
											perror("accept");
											} else {
											FD_SET(newfd, &master);
											if (newfd > fdMax) {
											fdMax = newfd;
											}
									log_info(loggerConPantalla,"\nSelectserver: nueva conexion desde %s en " "socket %d\n\n",inet_ntop(remoteaddr.ss_family,get_in_addr((struct sockaddr*) &remoteaddr),remoteIP, INET6_ADDRSTRLEN), newfd);
												}
									}
									else if(i!=0) {
											if ((nbytes = recv(i, &orden, sizeof orden, 0) <= 0)) { // Aca se carga el buffer con el mensaje. Actualmente no lo uso
											//if (nbytes == 0) {
											//	log_error(loggerConPantalla,"selectserver: socket %d hung up\n", i);
											//} else {
											//	perror("recv");
											//}
											//close(i);
											//FD_CLR(i, &master);
												}
									else {
									/*
									for(j = 0; j <= fdMax; j++) {//Rota entre las conexiones
									if (FD_ISSET(j, &master)) {
									if (j != socket && j != i) {*/
									pthread_mutex_lock(&mutexConexion);
									connectionHandler(i, orden);
								}
									}
							}
					}
			}
} // END handle data from client
//} // END got new incoming connection
//} // END looping through file descriptors
//}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}
