#if !defined(LOG_H_)
#define LOG_H_

#define LOG_FILE_NAME "./bin/log.txt"

// struttura per la scrittura dei dati in file di log
struct Log {
	FILE *fp;
	pthread_mutex_t mutex;
};

// creazione struttura dati per gestire il file di log
struct Log * crea_struttura_log();

// calcola la differenza in secondi tra strutture timeval
float diff_timeval(struct timeval t0, struct timeval t1);

#endif
