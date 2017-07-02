/*
 * comandosCPU.h
 *
 *  Created on: 17/6/2017
 *      Author: utnso
 */

#ifndef COMANDOSCPU_H_
#define COMANDOSCPU_H_

#include "sincronizacion.h"

t_list* listaCPU;

enum {EJECUCTANDO,OCIOSA};

typedef struct CPU {
	int estado;
	int pid;
	int socket;
	int socketInterrupciones;
}t_cpu;


void cpuEjecucionAOciosa(int socketCPU);




void cpuEjecucionAOciosa(int socketCPU){

	t_cpu *cpu = malloc(sizeof(t_cpu));
	int cpuFinalizada;

	_Bool verificaSocket(t_cpu* unaCpu){
		return (unaCpu->socket == socketCPU);
	}

	//recv(socketCPU,&cpuFinalizada, sizeof(int),0);

	pthread_mutex_lock(&mutexListaCPU);
	cpu = list_remove_by_condition(listaCPU, (void*)verificaSocket);
	cpu->estado = OCIOSA;
	list_add(listaCPU,cpu);
	pthread_mutex_unlock(&mutexListaCPU);

	sem_post(&sem_CPU);
}

#endif /* COMANDOSCPU_H_ */
