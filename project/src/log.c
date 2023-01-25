#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include <log.h>

// creazione struttura dati per gestire il file di log
struct Log * crea_struttura_log(){
	
	struct Log *l = malloc(sizeof(struct Log));
	if(l == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(l->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(l);
		return NULL;
	}
	l->fp = fopen(LOG_FILE_NAME, "w");
	if(l->fp == NULL){
		perror("fopen");
		free(l);
		return NULL;
	}
	
	return l;
}

// calcola la differenza in secondi tra strutture timeval
float diff_timeval(struct timeval t0, struct timeval t1){
	
	// converto timeval in microsecondi
	long t0_mic = (t0.tv_sec * 1000000) + t0.tv_usec;
	long t1_mic = (t1.tv_sec * 1000000) + t1.tv_usec;
	
	// calcolo la differenza dei tempi convertita in secondi
	float diff = (t1_mic - t0_mic) / 1000000.0;
	
	return diff;
}
