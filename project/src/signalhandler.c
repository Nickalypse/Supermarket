#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include <coda.h>
#include <signalhandler.h>

/// GESTIONE STRUTTURE /////////////////////////////////////////////////////////////////////////

struct Chiusura * crea_chiusura(){
	
	struct Chiusura *c = malloc(sizeof(struct Chiusura));
	if(c == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(c->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(c);
		return NULL;
	}
	if(pthread_cond_init(&(c->cond), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(c);
		return NULL;
	}
	
	// il supermercato è inizialmente aperto
	c->codice = 0;
	
	return c;
}

// creazione struttura dati signal handler
struct DatiSignalHandler * raggruppa_dati_signal_handler(
 int K, struct Cassa *array_casse, struct Chiusura *chiusura
){
	
	struct DatiSignalHandler *dati = malloc(sizeof(struct DatiSignalHandler));
	if(dati == NULL){
		perror("malloc");
		return NULL;
	}
	// copia dati
	dati->K = K;
	dati->array_casse = array_casse;
	dati->chiusura = chiusura;
	
	return dati;
}

////////////////////////////////////////////////////////////////////////////////////////////////

/* ENTITA' CHE CHIUDE IL NEGOZIO SEGNALANDOLO ALLE ALTRE */
void* th_signal_handler(void* arg){
	
	sigset_t mask;
	// inizializza maschera ad 1
	if(sigfillset(&mask) == -1){
		fprintf(stderr, "sigfillset : errore fatale\n");
		goto termina;
	}
	// applica nuova maschera al thread signal handler ==> intercetto tutti i segnali
	if(pthread_sigmask(SIG_SETMASK, &mask, NULL) != 0){
		fprintf(stderr, "pthread_sigmask : errore fatale\n");
		goto termina;
	}
	
	struct DatiSignalHandler *dati = (struct DatiSignalHandler *)arg;
	
	while(1){
		int segnale;
		// attesa di uno dei segnali specificati nella maschera
		if(sigwait(&mask, &segnale) != 0){
			fprintf(stderr, "sigwait : errore fatale\n");
			segnale = SIGQUIT;
		}
		// === GESTIONE CHIUSURA #1 ===========================================================
		if(segnale == SIGHUP){
			#ifdef DEBUG
			printf("<<< SIGHUP >>>\n");
			#endif
			pthread_mutex_lock(&(dati->chiusura->mutex));
			dati->chiusura->codice = 1;
			pthread_mutex_unlock(&(dati->chiusura->mutex));
			break;
		}
		// === GESTIONE CHIUSURA #2 ===========================================================
		else if(segnale == SIGQUIT){
			
		termina:
			
			#ifdef DEBUG
			printf("<<< SIGQUIT >>>\n");
			#endif
			pthread_mutex_lock(&(dati->chiusura->mutex));
			dati->chiusura->codice = 2;
			pthread_mutex_unlock(&(dati->chiusura->mutex));
			
			// avverto tutti i cassieri di essere in chiusura
			for(int i=0; i<(dati->K); i++){
				pthread_mutex_lock(&(dati->array_casse[i].mutex));
				// comunico alle casse
				dati->array_casse[i].flag_chiusa = 1;
				pthread_cond_signal(&(dati->array_casse[i].cond));
				pthread_mutex_unlock(&(dati->array_casse[i].mutex));
			}
			break;
		}
	}
	
	// termina esecuzione signal_handler
	return NULL;
}
