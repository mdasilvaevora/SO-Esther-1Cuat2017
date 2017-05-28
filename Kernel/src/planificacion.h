/*
 * planificacion.h

 *
 *  Created on: 27/5/2017
 *      Author: utnso
 */

#ifndef PLANIFICACION_H_
#define PLANIFICACION_H_

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

#include "configuraciones.h"



typedef struct {
	char* codigo;
	int size;
	int pid;
	int socketHiloConsola;
}t_codigoPrograma;

typedef struct CONSOLA{
	int pid;
	int socketHiloPrograma;
}t_consola;

/*----LARGO PLAZO--------*/
void* planificarLargoPlazo();
t_codigoPrograma* recibirCodigoPrograma(int socketConsola);
t_codigoPrograma* buscarCodigoDeProceso(int pid);
void crearProceso(t_pcb* proceso, t_codigoPrograma* codigoPrograma);
int inicializarProcesoEnMemoria(t_pcb* proceso, t_codigoPrograma* codigoPrograma);

t_list* listaCodigosProgramas;
pthread_t planificadorLargoPlazo;
/*----LARGO PLAZO--------*/

/*---PLANIFICACION GENERAL-------*/
int atenderNuevoPrograma(int socketAceptado);
int verificarGradoDeMultiprogramacion();
void encolarProcesoListo(t_pcb *procesoListo);
void cargarConsola(int pid, int idConsola);
//void dispatcher(int socket);
void terminarProceso(int socket);

int contadorPid=0;
t_list* colaNuevos;
t_list* colaListos;
t_list* colaTerminados;
t_list* listaConsolas;
t_list* listaCPU;
/*---PLANIFICACION GENERAL-------*/


/*------------------------LARGO PLAZO-----------------------------------------*/
void* planificarLargoPlazo(int socket){
	t_pcb* proceso;
	while(1){
		sem_wait(&sem_admitirNuevoProceso); // Previamente hay que eliminar el proceso de las colas y liberar todos sus recursos.
			if(verificarGradoDeMultiprogramacion() == 0 && list_size(colaNuevos)>0){
			proceso = list_get(colaNuevos,0);
			t_codigoPrograma* codigoPrograma = buscarCodigoDeProceso(proceso->pid);
			crearProceso(proceso,codigoPrograma);
			list_remove(colaNuevos,0);
			}
	}
}

t_codigoPrograma* recibirCodigoPrograma(int socketHiloConsola){
	log_info(loggerConPantalla,"Recibiendo codigo del nuevo programa ANSISOP\n");
	//char* comandoRecibirCodigo='R';
	t_codigoPrograma* codigoPrograma=malloc(sizeof(t_codigoPrograma));
	//send(socketHiloConsola,&comandoRecibirCodigo,sizeof(char),0);
	recv(socketHiloConsola,&codigoPrograma->size, sizeof(int),0);
	codigoPrograma->codigo = malloc(codigoPrograma->size);
	recv(socketHiloConsola,codigoPrograma->codigo,codigoPrograma->size  ,0);
	codigoPrograma->socketHiloConsola=socketHiloConsola;

	return codigoPrograma;
}

t_codigoPrograma* buscarCodigoDeProceso(int pid){
	_Bool verificarPid(t_codigoPrograma* codigoPrograma){
			return (codigoPrograma->pid == pid);
		}
	return list_find(listaCodigosProgramas, (void*)verificarPid);

}

void crearProceso(t_pcb* proceso,t_codigoPrograma* codigoPrograma){
	if(inicializarProcesoEnMemoria(proceso,codigoPrograma) < 0 ){
				log_error(loggerConPantalla ,"\nNo se pudo reservar recursos para ejecutar el programa");
				interruptHandler(codigoPrograma->socketHiloConsola,'A'); // Informa a consola error por no poder reservar recursos
				free(proceso);
				free(codigoPrograma);
			}
	else{
			encolarProcesoListo(proceso);
			cargarConsola(proceso->pid,codigoPrograma->socketHiloConsola);
			free(codigoPrograma);
			log_info(loggerConPantalla, "PCB encolado en lista de listos ---- PID: %d", proceso->pid);
			sem_post(&sem_colaReady);//Agregado para saber si hay algo en cola Listos, es el Signal
	}
}

int inicializarProcesoEnMemoria(t_pcb* proceso, t_codigoPrograma* codigoPrograma){
	log_info(loggerConPantalla, "Inicializando proceso en memoria ---- PID: %d \n", proceso->pid);
	if((pedirMemoria(proceso))< 0){
				log_error(loggerConPantalla ,"\nMemoria no autorizo la solicitud de reserva");
				return -1;
			}
	log_info(loggerConPantalla ,"Existe espacio en memoria para el nuevo programa\n");

	if((almacenarEnMemoria(proceso,codigoPrograma->codigo,codigoPrograma->size))< 0){
					log_error(loggerConPantalla ,"\nMemoria no puede almacenar contenido");
					return -2;
				}
	log_info(loggerConPantalla ,"El nuevo programa se almaceno correctamente en memoria\n");

	return 0;
}






/*-------------------PLANIFICACION GENERAL------------------------------------*/

int atenderNuevoPrograma(int socketAceptado){
		log_info(loggerConPantalla,"Atendiendo nuevo programa\n");

		contadorPid++; // VAR GLOBAL

		t_codigoPrograma* codigoPrograma = recibirCodigoPrograma(socketAceptado);
		codigoPrograma->pid=contadorPid;

		t_pcb* proceso=crearPcb(codigoPrograma->codigo,codigoPrograma->size);
		log_info(loggerConPantalla,"Program Size: %d \n", codigoPrograma->size);
		log_info(loggerConPantalla ,"Program Code: \" %s \" \n", codigoPrograma->codigo);

		send(socketAceptado,&contadorPid,sizeof(int),0);

		if(verificarGradoDeMultiprogramacion() <0 ){
					list_add(colaNuevos,proceso);
					list_add(listaCodigosProgramas,codigoPrograma);
					interruptHandler(socketAceptado,'M'); // Informa a consola error por grado de multiprogramacion
					return -1;
				}
		gradoMultiProgramacion++;//VAR GLOBAL
		crearProceso(proceso, codigoPrograma);
		return 0;
}


int verificarGradoDeMultiprogramacion(){
	log_info(loggerConPantalla, "Verificando grado de multiprogramacion");
	pthread_mutex_lock(&mutexGradoMultiProgramacion);
	if(gradoMultiProgramacion >= config_gradoMultiProgramacion) {
		pthread_mutex_unlock(&mutexGradoMultiProgramacion);
		log_error(loggerConPantalla, "Capacidad limite de procesos en sistema\n");
		return -1;
	}
	pthread_mutex_unlock(&mutexGradoMultiProgramacion);
	log_info(loggerConPantalla, "Grado de multiprogramacion suficiente\n");

	return 0;
}

void cargarConsola(int pid, int socketHiloPrograma) {
	t_consola *infoConsola = malloc(sizeof(t_consola));
	infoConsola->socketHiloPrograma=socketHiloPrograma;
	infoConsola->pid=pid;

	pthread_mutex_lock(&mutexListaConsolas);
	list_add(listaConsolas,infoConsola);
	pthread_mutex_unlock(&mutexListaConsolas);
	//free(infoConsola);
}

void encolarProcesoListo(t_pcb *procesoListo){
//	colaListos
	log_info(loggerConPantalla, "Encolando proceso a cola de listos---- PID: %d \n", procesoListo->pid);

	pthread_mutex_lock(&mutexColaListos);
	list_add(colaListos,procesoListo);
	pthread_mutex_unlock(&mutexColaListos);
}


void terminarProceso(int socketCPU){
	t_pcb* pcbProcesoTerminado/* = malloc(sizeof(t_pcb))*/;
	t_consola* consolaAInformar = malloc(sizeof(t_consola));

	char *mensaje = malloc(30);

	pcbProcesoTerminado = recibirYDeserializarPcb(socketCPU);

	pthread_mutex_lock(&mutexColaTerminados);
	list_add(colaTerminados, pcbProcesoTerminado);
	pthread_mutex_unlock(&mutexColaTerminados);

	_Bool verificarPid(t_consola* pidNuevo){
						return (pidNuevo->pid == pcbProcesoTerminado->pid);
					}

	consolaAInformar = list_find(listaConsolas, verificarPid);


	strcpy(mensaje,"termine\0");

	int tamanioMensaje = strlen(mensaje) + 1;

	send(consolaAInformar->socketHiloPrograma,&tamanioMensaje,sizeof(int),0);
	send(consolaAInformar->socketHiloPrograma,mensaje,tamanioMensaje,0);


	/* TODO: Buscar Consola por PID e informar */

	/* TODO:Liberar recursos */

}

#endif /* PLANIFICACION_H_ */