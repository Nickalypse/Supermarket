#if !defined(SIGNALHANDLER_H_)
#define SIGNALHANDLER_H_

/// STRUTTURE ////////////////////////////////////////////////////////////////////////////

// struttura per la comunicazione della chiusura del supermercato
struct Chiusura {
	int codice;
	pthread_mutex_t mutex;
	pthread_cond_t cond;	// FORSE NON SERVE!
};

// struttura con i soli dati interessanti per il cliente
struct DatiSignalHandler {
	int K;
	struct Cassa *array_casse;
	struct Chiusura *chiusura;
};

/// GESTIONE STRUTTURE /////////////////////////////////////////////////////////////////////////

struct Chiusura * crea_chiusura();

// creazione struttura dati signal handler
struct DatiSignalHandler * raggruppa_dati_signal_handler(
 int K, struct Cassa *array_casse, struct Chiusura *chiusura
);

////////////////////////////////////////////////////////////////////////////////////////////////

/* ENTITA' CHE CHIUDE IL NEGOZIO SEGNALANDOLO ALLE ALTRE */
void* th_signal_handler(void* arg);

#endif
