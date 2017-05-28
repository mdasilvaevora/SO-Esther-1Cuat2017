/*
 * interfazHandler.h
 *
 *  Created on: 27/5/2017
 *      Author: utnso
 */
#include "sincronizacion.h"
#include "planificacion.h"
#include "configuraciones.h"

void interfazHandler();
void imprimirInterfazUsuario();
void modificarGradoMultiprogramacion();
void obtenerListadoProcesos();
void mostrarProcesos(char orden);
void imprimirListadoDeProcesos(t_list* listaPid);
void filtrarPorPidYMostrar(t_list* cola);

/*-------------LOG-----------------*/
void inicializarLog(char *rutaDeLog);
t_log *loggerSinPantalla;
t_log *loggerConPantalla;


void interfazHandler(){
	log_info(loggerConPantalla,"Iniciando Interfaz Handler\n");
	char orden;
	//int pid;
	char* mensajeRecibido;

	scanf("%c",&orden);

	switch(orden){
			case 'O':
				obtenerListadoProcesos();
				break;
			case 'P':
				/*obtenerProcesoDato(int pid); TODO HAY QUE IMPLEMENTAR*/
				break;
			case 'G':
				/*mostrarTablaGlobalArch(); TODO HAY QUE IMPLEMENTAR*/
				break;
			case 'M':
				modificarGradoMultiprogramacion();
				break;
			case 'K':
				/*finalizarProceso(int pid) TODO HAY QUE IMPLEMENTAR*/
				break;
			case 'D':
				/*pausarPlanificacion() TODO HAY QUE IMPLEMENTAR*/
				break;
			case 'S':
				if((solicitarContenidoAMemoria(&mensajeRecibido))<0){
					printf("No se pudo solicitar el contenido\n");
					break;
				}
				else{
					printf("El mensaje recibido de la Memoria es : %s\n" , mensajeRecibido);
					}
				break;
			case 'I':
					imprimirInterfazUsuario();
				break;
			default:
				if(orden == '\0') break;
				log_warning(loggerConPantalla ,"\nOrden no reconocida\n");
				break;
	}
	orden = '\0';
	log_info(loggerConPantalla,"Finalizando atencion de Interfaz Handler\n");
	return;

}

void obtenerListadoProcesos(){
	char orden;
	log_info(loggerConPantalla,"T: Mostrar todos los procesos\nC: Mostrar procesos de un estado\n");
	scanf("%c",&orden);
	switch(orden){
	case 'T':
		orden = 'T';
		mostrarProcesos(orden);
		break;
	case 'C':
		log_info(loggerConPantalla,"N:New\nR:Ready\nE:Exec\nF:Finished\nB:Blocked\n");
		scanf("%c",&orden);
		mostrarProcesos(orden);
		break;
	default:
		log_error(loggerConPantalla,"Orden no reconocida\n");
		break;
	}
}

void mostrarProcesos(char orden){

	switch(orden){
	case 'N':
		pthread_mutex_lock(&mutexColaNuevos);
		filtrarPorPidYMostrar(colaNuevos);
		pthread_mutex_unlock(&mutexColaNuevos);
		break;
	case 'R':
		pthread_mutex_lock(&mutexColaListos);
		filtrarPorPidYMostrar(colaListos);
		pthread_mutex_unlock(&mutexColaListos);
		break;
	case 'E':
		pthread_mutex_lock(&mutexColaNuevos);
		pthread_mutex_unlock(&mutexColaNuevos);
		break;
	case 'F':
		pthread_mutex_lock(&mutexColaNuevos);
		pthread_mutex_unlock(&mutexColaNuevos);
		break;
	case 'B':
		pthread_mutex_lock(&mutexColaNuevos);
		pthread_mutex_unlock(&mutexColaNuevos);
		break;
	case 'T':
		break;
	default:
		break;
	}
}

void filtrarPorPidYMostrar(t_list* cola){
	int transformarPid(t_pcb* pcb){
			return pcb->pid;
		}
		void liberar(int* pid){
			free(pid);
		}
		imprimirListadoDeProcesos(list_filter(cola,(void*)transformarPid));
			//list_destroy_and_destroy_elements(listaPid, (void*)liberar);
			/*TODO:PREGUNTAR QUE PASA CON ESTA NUEVA LISTA CREADA*/
}

void imprimirListadoDeProcesos(t_list* listaPid){
	printf("PID\n");
	int pid;
	int i;
	for(i=0 ; i<listaPid->elements_count ; i++){
		pid = *(int*)list_get(listaPid,i);
		printf("%d\n",pid);
	}
}

void modificarGradoMultiprogramacion(){
	int nuevoGrado;
	log_info(loggerConPantalla,"Ingresar nuevo grado de multiprogramacion");
	scanf("%d",&nuevoGrado);
	pthread_mutex_lock(&mutexGradoMultiProgramacion);
	config_gradoMultiProgramacion= nuevoGrado;
	pthread_mutex_unlock(&mutexGradoMultiProgramacion);
}

void inicializarLog(char *rutaDeLog){

		mkdir("/home/utnso/Log",0755);

		loggerSinPantalla = log_create(rutaDeLog,"Kernel", false, LOG_LEVEL_INFO);
		loggerConPantalla = log_create(rutaDeLog,"Kernel", true, LOG_LEVEL_INFO);
}

void imprimirInterfazUsuario(){

	/**************************************Printea interfaz Usuario Kernel*******************************************************/
	printf("\n-----------------------------------------------------------------------------------------------------\n");
	printf("Para realizar acciones permitidas en la consola Kernel, seleccionar una de las siguientes opciones\n");
	printf("\nIngresar orden de accion:\nO - Obtener listado programas\nP - Obtener datos proceso\nG - Mostrar tabla global de archivos\nM - Modif grado multiprogramacion\nK - Finalizar proceso\nD - Pausar planificacion\n");
	printf("\n-----------------------------------------------------------------------------------------------------\n");
	/****************************************************************************************************************************/
}