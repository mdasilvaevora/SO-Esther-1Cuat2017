/*
 ============================================================================
 Name        : Consola.c
 Author      : Servomotor
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "Consola.h"
#include "hiloPrograma.h"


void signalHandler(int signum);

int main(void) {

	leerConfiguracion("/home/utnso/workspace/tp-2017-1c-servomotor/Consola/config_Consola");
	imprimirConfiguraciones();
	inicializarLog("/home/utnso/Log/logConsola.txt");
	inicializarListas();
	inicializarSemaforos();
	flagCerrarConsola = 1;

	socketKernel = crear_socket_cliente(ipKernel, puertoKernel);
	log_info(loggerConPantalla,"LCreando hilo interfaz usuario");
	int err = pthread_create(&hiloInterfazUsuario, NULL, (void*)connectionHandler,NULL);
	if (err != 0) log_error(loggerConPantalla,"\nError al crear el hilo :[%s]", strerror(err));

	signal(SIGINT, signalHandler);

	pthread_join(hiloInterfazUsuario, NULL);
	log_warning(loggerConPantalla,"La Consola ha finalizado");
	return 0;

}

void signalHandler(int signum)
{
    if (signum == SIGINT)
    {
    	log_warning(loggerConPantalla,"Finalizando consola");
    	cerrarTodo();
    	pthread_kill(hiloInterfazUsuario,SIGUSR1);
    }
    if(signum==SIGUSR1){
    	log_warning(loggerConPantalla,"Finalizando hilo de Interfaz de Usuario");
    	int exitCode;
    	pthread_exit(&exitCode);
    }
}

void connectionHandler() {

	signal(SIGUSR1,signalHandler);
	char orden;
	int cont=0;
	imprimirInterfaz();

	while (flagCerrarConsola) {
		pthread_mutex_lock(&mutex_crearHilo);
		scanf("%c", &orden);
		log_info(loggerConPantalla,"Orden definida %d",orden);
		cont++;
		imprimirInterfaz();

		switch (orden) {
			case 'I':
				crearHiloPrograma();
				break;
			case 'F':
				finalizarPrograma();
				break;
			case 'C':
				limpiarPantalla();
				break;
			case 'Q':
				cerrarTodo();
				break;
			default:
				if(cont!=2)log_error(loggerConPantalla,"Orden %c no definida", orden);
				else cont=0;
				pthread_mutex_unlock(&mutex_crearHilo);
				break;
			}

	}
}

void limpiarPantalla(){
	system("clear");
	log_info(loggerConPantalla,"Limpiando pantalla");
	pthread_mutex_unlock(&mutex_crearHilo);
}



void cerrarTodo(){
	char comandoInterruptHandler='X';
	char comandoCierreConsola = 'E';
	int i;
	int desplazamiento = 0;

	pthread_mutex_lock(&mutexListaHilos);
	int cantidad= listaHilosProgramas->elements_count;
	flagCerrarConsola = 0;

	log_info(loggerConPantalla,"Informando Kernel el cierre de la Consola");
	if(cantidad == 0) {
		char comandoCerrarSocket= 'Z';
		send(socketKernel,&comandoCerrarSocket,sizeof(char),0);
		close(socketKernel);
		return;
	}

	int bufferSize = sizeof(char)*2 +sizeof(int)*2 + sizeof(int)* cantidad;
	int bufferProcesosSize = sizeof(int) + sizeof(int)*cantidad;

	char* mensaje= malloc(bufferSize);

	t_hiloPrograma* programaAbortar = malloc(sizeof(t_hiloPrograma));

	memcpy(mensaje + desplazamiento,&comandoInterruptHandler,sizeof(char));
	desplazamiento += sizeof(char);

	memcpy(mensaje + desplazamiento,&comandoCierreConsola,sizeof(char));
	desplazamiento+=sizeof(char);

	memcpy(mensaje + desplazamiento,&bufferProcesosSize,sizeof(int));
	desplazamiento +=sizeof(int);

	memcpy(mensaje+desplazamiento,&cantidad,sizeof(int));
	desplazamiento += sizeof(int);

	for(i=0;i<cantidad;i++){
		programaAbortar = (t_hiloPrograma*) list_get(listaHilosProgramas,i);
		memcpy(mensaje + desplazamiento,&programaAbortar->pid,sizeof(int));
		desplazamiento += sizeof(int);
		informarEstadisticas(programaAbortar);
	}
	pthread_mutex_unlock(&mutexListaHilos);

	send(socketKernel,mensaje,bufferSize,0);

	list_destroy_and_destroy_elements(listaHilosProgramas,free);
	free(mensaje);
}
void recibirDatosDelKernel(int socketHiloKernel){
	int pid;
	int size;
	int flagCerrarHilo=1;
	char* mensaje;

	recv(socketHiloKernel, &pid, sizeof(int), 0);
	log_info(loggerConPantalla,"Al Programa ANSISOP en socket: %d se le ha asignado el PID: %d", socketHiloKernel,pid);

	cargarHiloPrograma(pid,socketHiloKernel);
	pthread_mutex_unlock(&mutex_crearHilo);

	while(flagCerrarHilo){
		recv(socketHiloKernel,&size,sizeof(int),0);

		pthread_mutex_lock(&mutexRecibirDatos);

		log_warning(loggerConPantalla,"Hilo:%d--->PID:%d",socketHiloKernel,pid);

		mensaje = malloc(size * sizeof(char) + sizeof(char));

		recv(socketHiloKernel,mensaje,size,0);

		strcpy(mensaje+size,"\0");

		if(strcmp(mensaje,"Finalizar")==0) {
			flagCerrarHilo = 0;
			free(mensaje);
			break;
		}
		printf("\n%s\n",mensaje);
		actualizarCantidadImpresiones(pid);
		free(mensaje);
		imprimirInterfaz();
		pthread_mutex_unlock(&mutexRecibirDatos);
	}

	gestionarCierrePrograma(pid);
	log_warning(loggerConPantalla,"Hilo Programa ANSISOP--->PID:%d--->Socket:%d ha finalizado",pid,socketHiloKernel);

	imprimirInterfaz();
	pthread_mutex_unlock(&mutexRecibirDatos);

}

void actualizarCantidadImpresiones(int pid){
	bool verificaPid(t_hiloPrograma* proceso){
			return (proceso->pid == pid);
		}
	pthread_mutex_lock(&mutexListaHilos);
	t_hiloPrograma* programa = list_remove_by_condition(listaHilosProgramas,(void*) verificaPid);
	programa->cantImpresiones += 1;
	list_add(listaHilosProgramas,programa);
	pthread_mutex_unlock(&mutexListaHilos);
}

void leerConfiguracion(char* ruta) {
	configuracion_Consola = config_create(ruta);
	ipKernel = config_get_string_value(configuracion_Consola, "IP_KERNEL");
	puertoKernel = config_get_string_value(configuracion_Consola,"PUERTO_KERNEL");
}

void imprimirConfiguraciones() {

	printf("---------------------------------------------------\n");
	printf("CONFIGURACIONES\nIP KERNEL:%s\nPUERTO KERNEL:%s\n", ipKernel,
			puertoKernel);
	printf("---------------------------------------------------\n");
}

void imprimirInterfaz(){
	printf("----------------------------------------------------------------------\n");
	printf("Ingresar orden:\n 'I' para iniciar un programa AnSISOP\n 'F' para finalizar un programa AnSISOP\n 'C' para limpiar la pantalla\n 'Q' para desconectar esta Consola\n");
	printf("----------------------------------------------------------------------\n");
}

void inicializarLog(char *rutaDeLog){
		mkdir("/home/utnso/Log",0755);
		loggerSinPantalla = log_create(rutaDeLog,"Consola", false, LOG_LEVEL_INFO);
		loggerConPantalla = log_create(rutaDeLog,"Consola", true, LOG_LEVEL_INFO);
}

void inicializarListas(){
	listaHilosProgramas= list_create();
}

void inicializarSemaforos(){
	pthread_mutex_init(&mutex_crearHilo,NULL);
	pthread_mutex_init(&mutexListaHilos,NULL);
	pthread_mutex_init(&mutexRecibirDatos,NULL);
	sem_init(&sem_crearHilo,0,1);
}
