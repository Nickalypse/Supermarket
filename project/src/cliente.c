#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include <config.h>
#include <coda.h>
#include <lista.h>
#include <log.h>
#include <signalhandler.h>
#include <cliente.h>

#define CHECK_GETTIMEOFDAY(var)																	\
	if(gettimeofday(&var, NULL) == -1){															\
		perror("gettimeofday");																	\
		fprintf(stderr,																			\
		"{GESTIONE ERRORI} : cliente prosegue ma 'log.txt' contiene dati inconsistenti\n"		\
		);																						\
	}

/// GESTIONE STRUTTURE /////////////////////////////////////////////////////////////////////////

// creazione struttura condivisa con totale di clienti
struct TotClienti * crea_totale_clienti(){

	struct TotClienti *t = malloc(sizeof(struct TotClienti));
	if(t == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(t->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(t);
		return NULL;
	}
	if(pthread_cond_init(&(t->cond), NULL) != 0){
		fprintf(stderr, "pthread_cond_init : errore fatale\n");
		free(t);
		return NULL;
	}
	// inizialmente non ci sono clienti
	t->n = 0;
	
	return t;
}

// creazione struttura con id del prossimo cliente
struct IdCliente * crea_id_cliente(){
	
	struct IdCliente *t = malloc(sizeof(struct IdCliente));
	if(t == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(t->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(t);
		return NULL;
	}
	// id disponibile per il primo cliente
	t->id = 1;
	
	return t;
}

// creazione struttura dati cliente
struct DatiCliente * raggruppa_dati_cliente(
 struct config *conf, struct Cassa *array_casse, struct TotClienti *tot_clienti,
 struct IdCliente *id_cliente, struct permessi *lista_permessi,
 struct Chiusura *chiusura, struct Log *log
){
	struct DatiCliente *dati = malloc(sizeof(struct DatiCliente));
	if(dati == NULL){
		perror("malloc");
		return NULL;
	}
	// copia dati
	dati->K = conf->K;
	dati->T = conf->T;
	dati->P = conf->P;
	dati->array_casse = array_casse;
	dati->tot_clienti = tot_clienti;
	dati->id_cliente = id_cliente;
	dati->lista_permessi = lista_permessi;
	dati->chiusura = chiusura;
	dati->log = log;
	
	return dati;
}

////////////////////////////////////////////////////////////////////////////////////////////////

// cerca casualmente l'indice di una cassa aperta
int trova_cassa_aperta(struct DatiCliente *dati, unsigned int seed){
	
	int index;
	int flag_trovata = 0;
	
	do{
		// verifico se il supermercato è in chiusura immediata (SIGQUIT)
		pthread_mutex_lock(&(dati->chiusura->mutex));
		int codice = dati->chiusura->codice;
		pthread_mutex_unlock(&(dati->chiusura->mutex));
		if(codice == 2) return -1;

		// scelgo casualmente una cassa
		index = rand_r(&seed) % (dati->K);
		
		pthread_mutex_lock(&(dati->array_casse[index].mutex));
		// verifico se la cassa è aperta
		if(dati->array_casse[index].flag_chiusa == 0){ flag_trovata = 1; }
		pthread_mutex_unlock(&(dati->array_casse[index].mutex));
	}while(flag_trovata == 0);
	
	return index;
}

// ---------------------------------------------------------------------------------------------

/* ENTITA' CLIENTE CHE SI METTE IN CODA / ATTENDE PERMESSO DI USCIRE / ESCE PER EMERGENZA */
void* th_cliente(void* arg){
	
	// == LOG.TXT ===========================
	// dati da memorizzare nel file di log
	int tot_code_visitate = 0;
	int flag_servito = 0;
	struct timeval t0, t1, tc0, tc1;
	// ======================================
	
	struct DatiCliente *dati = (struct DatiCliente *)arg;
	
	pthread_mutex_lock(&(dati->id_cliente->mutex));
	int my_id = dati->id_cliente->id;
	(dati->id_cliente->id)++;
	pthread_mutex_unlock(&(dati->id_cliente->mutex));
	
	// generazione seed diverso per ogni thread cliente
	unsigned int seed = clock() * (my_id);
	
	int my_t = rand_r(&seed) % (dati->T - SOGLIA_INF_T + 1) + SOGLIA_INF_T;
	int my_p = rand_r(&seed) % (dati->P + 1);
	
	if(my_p == 0){
		// incrementa il totale di permessi che il direttore deve gestire
		pthread_mutex_lock(&(dati->lista_permessi->mutex));
		(dati->lista_permessi->len)++;
		pthread_mutex_unlock(&(dati->lista_permessi->mutex));
	}
	
	// crea struttura per la gestione dei nanosecondi
	SET_TIMESPEC(aspetta, my_t);
	// [LOG] acquisizione tempo iniziale
	CHECK_GETTIMEOFDAY(t0);
	// simulo tempo necessario all'acquisto dei my_p prodotti
	if(nanosleep(&aspetta, NULL) == -1){
		perror("nanosleep");
		fprintf(
			stderr,
			"{GESTIONE ERRORI} : il cliente non attende tempo per la spesa\n"
		);
	}
	
	// [LOG] acquisizione tempo in coda iniziale
	CHECK_GETTIMEOFDAY(tc0);
	
	/** SE non ho preso alcun prodotto aspetto permesso dal direttore per uscire *************/
	if(my_p == 0){
		
		// alloca struttura per attendere il consenso del direttore
		struct elem_permessi *e = crea_elem_permessi();
		if(e == NULL){
			fprintf(stderr, "{GESTIONE ERRORI} : il cliente esce subito\n");
			goto fine;
		}
		
		// [LOG] acquisizione tempo in coda iniziale
		CHECK_GETTIMEOFDAY(tc0);
		
		// elemento aggiunto alla lista in mutua esclusione
		pthread_mutex_lock(&(dati->lista_permessi->mutex));
		aggiungi_elemento_in_lista_permessi(e, dati->lista_permessi);
		// sveglia il direttore per l'accettazione del nuovo permesso
		pthread_cond_signal(&(dati->lista_permessi->cond));
		pthread_mutex_unlock(&(dati->lista_permessi->mutex));
		
		// attesa del consenso del direttore
		pthread_mutex_lock(&(e->mutex));
		while(e->confermato == 0){
			pthread_cond_wait(&(e->cond), &(e->mutex));
		}
		pthread_mutex_unlock(&(e->mutex));
		
		// [LOG] acquisizione tempo in coda finale
		CHECK_GETTIMEOFDAY(tc1);
		
		free(e);
	}
	
	/** SE ho preso prodotti mi accodo ad una cassa ******************************************/
	else{
	
		// creo struttura che identifica il cliente in coda dal cassiere
		struct elem_coda *e = crea_elemento_coda(my_p);
		if(e == NULL){
			fprintf(stderr, "{GESTIONE ERRORI} : il cliente esce subito\n");
			goto fine;
		}
		
		// ciclo ripetuto ogni volta che il cliente è in coda e la cassa viene chiusa
		while(1){
			
			// trovo indice casuale di una cassa aperta
			int index = trova_cassa_aperta(dati, seed);
			
			// uscita immediata per SIGQUIT
			if(index == -1) break;
			
			pthread_mutex_lock(&(dati->array_casse[index].mutex));
				// mi accodo alla cassa scelta
				aggiungi_elemento_in_coda(e, &(dati->array_casse[index]));
				if(tot_code_visitate == 0){
					// [LOG] acquisizione tempo in coda iniziale
					CHECK_GETTIMEOFDAY(tc0);
				}
				// incremento il totale di code visitate
				tot_code_visitate++;
				
				// sveglio cassiere se la coda era vuota prima del mio arrivo
				if(dati->array_casse[index].len == 1){
					pthread_cond_signal(&(dati->array_casse[index].cond));
				}
			pthread_mutex_unlock(&(dati->array_casse[index].mutex));
			
			pthread_mutex_lock(&(e->mutex));
				// attendo inizio servizio
				while(e->flag_servito == 0){
					pthread_cond_wait(&(e->cond), &(e->mutex));
				}
				if(e->flag_servito == 1){
					pthread_mutex_unlock(&(e->mutex));
					flag_servito = 1;
					break;
				}
			pthread_mutex_unlock(&(e->mutex));
			
			// resetto lo stato
			e->flag_servito = 0;
		}
		
		// copio tempo inizio servizio in tempo fine coda
		tc1.tv_sec = (e->ts0).tv_sec;
		tc1.tv_usec = (e->ts0).tv_usec;
		
		// dealloco struttura che identifica cliente in coda
		free(e);
	}
	
	
fine:

	// [LOG] acquisizione tempo finale
	CHECK_GETTIMEOFDAY(t1);
	
	// decrementa totale di clienti nel supermercato [mutua escluzione]
	pthread_mutex_lock(&(dati->tot_clienti->mutex));
		(dati->tot_clienti->n)--;
		// sveglia il direttore che deciderà se far entrare altri clienti
		pthread_cond_signal(&(dati->tot_clienti->cond));
	pthread_mutex_unlock(&(dati->tot_clienti->mutex));
	
	// == LOG ===========================================================
	float diff_coda;
	if(flag_servito == 1)
		diff_coda = diff_timeval(tc0, tc1);
	else
		diff_coda = diff_timeval(tc0, t1);
	
	// [LOG] scrittura dati su file di log
	pthread_mutex_lock(&(dati->log->mutex));
	fprintf(dati->log->fp,
		"CLIENTE;%d;%f;%f;%d;%d\n",
		my_id, diff_timeval(t0, t1), diff_coda, my_p, tot_code_visitate
	);
	pthread_mutex_unlock(&(dati->log->mutex));
	// ==================================================================
	
	// termina thread attendendo pthread_join
	return NULL;
}
