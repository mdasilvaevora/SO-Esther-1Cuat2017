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
#include "pcb.h"
#include "conexionMemoria.h"

typedef struct CPU {
	int pid;
	int socket;
}t_cpu;

typedef struct CONSOLA{
	int pid;
	int socketHiloPrograma;
}t_consola;

typedef struct {
	char* codigo;
	int size;
	int pid;
	int socketHiloConsola;
}t_codigoPrograma;

/*----LARGO PLAZO--------*/
void* planificarLargoPlazo();
void crearProceso(t_pcb* proceso, t_codigoPrograma* codigoPrograma);
int inicializarProcesoEnMemoria(t_pcb* proceso, t_codigoPrograma* codigoPrograma);
void informarConsola(int socketHiloPrograma,char* mensaje, int size);
t_codigoPrograma* recibirCodigoPrograma(int socketConsola);
t_codigoPrograma* buscarCodigoDeProceso(int pid);
t_list* listaCodigosProgramas;
pthread_t planificadorLargoPlazo;
/*----LARGO PLAZO--------*/

/*----CORTO PLAZO--------*/

void* planificarCortoPlazo();
void agregarA(t_list* lista, void* elemento, pthread_mutex_t mutex);
pthread_t planificadorCortoPlazo;
/*----CORTO PLAZO--------*/

/*---PLANIFICACION GENERAL-------*/
int atenderNuevoPrograma(int socketAceptado);
int verificarGradoDeMultiprogramacion();
void encolarProcesoListo(t_pcb *procesoListo);
void cargarConsola(int pid, int idConsola);
void terminarProceso(int socket);

int contadorPid=0;
t_list* colaNuevos;
t_list* colaListos;
t_list* colaTerminados;
t_list* listaConsolas;
t_list* listaCPU;
t_list* colaEjecucion;
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
	return list_remove_by_condition(listaCodigosProgramas, (void*)verificarPid);

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
			log_info(loggerConPantalla, "PCB encolado en cola de listos ---- PID: %d", proceso->pid);
			sem_post(&sem_colaReady);
	}
}

int inicializarProcesoEnMemoria(t_pcb* proceso, t_codigoPrograma* codigoPrograma){
	log_info(loggerConPantalla, "Inicializando proceso en memoria ---- PID: %d \n", proceso->pid);
	if((pedirMemoria(proceso))< 0){
				log_error(loggerConPantalla ,"\nMemoria no autorizo la solicitud de reserva");
				return -1;
			}
	if((almacenarCodigoEnMemoria(proceso,codigoPrograma->codigo,codigoPrograma->size))< 0){
					log_error(loggerConPantalla ,"\nMemoria no puede almacenar contenido");
					return -2;
				}
	return 0;
}

/*------------------------LARGO PLAZO-----------------------------------------*/

/*------------------------CORTO PLAZO-----------------------------------------*/

void agregarA(t_list* lista, void* elemento, pthread_mutex_t mutex){

	pthread_mutex_lock(&mutex);
	list_add(lista, elemento);
	pthread_mutex_unlock(&mutex);
}

void* planificarCortoPlazo(){
	t_pcb* pcbListo;
	t_cpu* cpuEnEjecucion = malloc(sizeof(t_cpu));

	while(1){
		sem_wait(&sem_CPU);
		sem_wait(&sem_colaReady);

		pthread_mutex_lock(&mutexColaListos);
		pcbListo = list_remove(colaListos,0);
		pthread_mutex_unlock(&mutexColaListos);


		pthread_mutex_lock(&mutexListaCPU);
		cpuEnEjecucion = list_remove(listaCPU,0);
		pthread_mutex_unlock(&mutexListaCPU);

		cpuEnEjecucion->pid = pcbListo->pid;

		pthread_mutex_lock(&mutexColaEjecucion);
		list_add(colaEjecucion, pcbListo);
		pthread_mutex_unlock(&mutexColaEjecucion);

		serializarPcbYEnviar(pcbListo, cpuEnEjecucion->socket);
	}
}


/*------------------------CORTO PLAZO-----------------------------------------*/

/*-------------------PLANIFICACION GENERAL------------------------------------*/

int atenderNuevoPrograma(int socketAceptado){
		log_info(loggerConPantalla,"Atendiendo nuevo programa\n");

		contadorPid++; // VAR GLOBAL

		t_codigoPrograma* codigoPrograma = recibirCodigoPrograma(socketAceptado);
		codigoPrograma->pid=contadorPid;

		t_pcb* proceso=crearPcb(codigoPrograma->codigo,codigoPrograma->size);
		//log_info(loggerConPantalla,"Program Size: %d \n", codigoPrograma->size);
		//log_info(loggerConPantalla ,"Program Code: \" %s \" \n", codigoPrograma->codigo);

		send(socketAceptado,&contadorPid,sizeof(int),0);

		if(verificarGradoDeMultiprogramacion() <0 ){
					list_add(colaNuevos,proceso);
					list_add(listaCodigosProgramas,codigoPrograma);
					interruptHandler(socketAceptado,'M'); // Informa a consola error por grado de multiprogramacion
					return -1;
				}

		pthread_mutex_lock(&mutexGradoMultiProgramacion);
		gradoMultiProgramacion++;
		pthread_mutex_unlock(&mutexGradoMultiProgramacion);
		crearProceso(proceso, codigoPrograma);
		return 0;
}


int verificarGradoDeMultiprogramacion(){
	//log_info(loggerConPantalla, "Verificando grado de multiprogramacion");
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
	pthread_mutex_lock(&mutexColaListos);
	list_add(colaListos,procesoListo);
	pthread_mutex_unlock(&mutexColaListos);
}


void terminarProceso(int socketCPU){
	t_pcb* pcbProcesoTerminado;
	t_consola* consolaAInformar;

	_Bool verificarPidConsola(t_consola* consola){
						return (consola->pid == pcbProcesoTerminado->pid);
					}

	_Bool verificarPid(t_pcb* pcb){
		return (pcb->pid == pcbProcesoTerminado->pid);
	}

	_Bool verificarCPU(t_cpu* cpu){
			return (cpu->socket == socketCPU);
	}

	pcbProcesoTerminado = recibirYDeserializarPcb(socketCPU);
	log_info(loggerConPantalla, "Terminando proceso---- PID: %d ", pcbProcesoTerminado->pid);

	pthread_mutex_lock(&mutexColaEjecucion);
	list_remove_by_condition(colaEjecucion, (void*)verificarPid);//Remueve pcb de la colaEjecucion
	pthread_mutex_unlock(&mutexColaEjecucion);


	pthread_mutex_lock(&mutexGradoMultiProgramacion);
	gradoMultiProgramacion--;
	pthread_mutex_unlock(&mutexGradoMultiProgramacion);

	pthread_mutex_lock(&mutexListaCPU);
	list_remove_by_condition(listaCPU,(void*)verificarCPU);
	pthread_mutex_unlock(&mutexListaCPU);


	pthread_mutex_lock(&mutexColaTerminados);
	list_add(colaTerminados, pcbProcesoTerminado);
	pthread_mutex_unlock(&mutexColaTerminados);

	consolaAInformar = list_remove_by_condition(listaConsolas,(void*) verificarPidConsola);

	char* mensaje= "El proceso ha finalizado correctamente";
	int size= strlen(mensaje);
	informarConsola(consolaAInformar->socketHiloPrograma,mensaje,size);

	/* TODO:Liberar recursos */
	//free(consolaAInformar); //Ojo aca, si metes este free aca nomas, vas a ver que borras la consolaAInformar antes de que la Consola reciba el mensaje. Hay que hacer algun tipo de espera aca
}

void informarConsola(int socketHiloPrograma,char* mensaje, int size){
	send(socketHiloPrograma,&size,sizeof(int),0);
	send(socketHiloPrograma,mensaje,size,0);
}
#endif /* PLANIFICACION_H_ */
