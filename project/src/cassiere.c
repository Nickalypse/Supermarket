#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include <config.h>
#include <coda.h>
#include <log.h>
#include <signalhandler.h>
#include <cliente.h>
#include <direttore.h>

#include <cassiere.h>

#define CHECK_GETTIMEOFDAY(var)																	\
	if(gettimeofday(&(var), NULL) == -1){														\
		perror("gettimeofday");																	\
		fprintf(stderr,																			\
		"{GESTIONE ERRORI} : supermercato prosegue ma 'log.txt' contiene dati inconsistenti\n"	\
		);																						\
	}

/// GESTIONE STRUTTURE /////////////////////////////////////////////////////////////////////////

// creazione struttura dati cassiere condivisi
struct DatiCassiereCondivisi * raggruppa_dati_cassiere_condivisi(
 struct config *conf, struct Chiusura *chiusura, struct TotClienti *tot_clienti, struct Log *log
){
	
	struct DatiCassiereCondivisi *dati = malloc(sizeof(struct DatiCassiereCondivisi));
	if(dati == NULL){
		perror("malloc");
		return NULL;
	}
	// copia dati
	dati->INTERV = conf->INTERV;
	dati->PROD = conf->PROD;
	dati->chiusura = chiusura;
	dati->tot_clienti = tot_clienti;
	dati->log = log;
	
	return dati;
}

// creazione struttura dati cassiere
struct DatiCassiere * crea_dati_cassiere(struct DatiDirettore *dd, int index){
	
	struct DatiCassiere *dati = malloc(sizeof(struct DatiCassiere));
	if(dati == NULL){
		perror("malloc");
		return NULL;
	}
	dati->condivisi = dd->dati_cassiere_condivisi;
	dati->my_cassa = &(dd->array_casse[index]);
	dati->my_aggiornamento = &(dd->array_aggiornamenti[index]);
	
	// creo indice
	int *i = malloc(sizeof(int));
	if(i == NULL){
		perror("malloc");
		free(dati);
		return NULL;
	}
	*i = index;
	dati->index = i;
	
	return dati;
}

////////////////////////////////////////////////////////////////////////////////////////////////

/* PARTE DELL'ENTITA' CASSIERE CHE AGGIORNA IL DIRETTORE SULLA LUNGHEZZA DELLA CODA */
void* th_cassiere_agg(void* arg){
	
	struct DatiCassiere *dati = (struct DatiCassiere *) arg;
	
	// crea struttura per la gestione dei nanosecondi
	struct timespec intervallo;	
	SET_TIMESPEC_FROM_MS(intervallo, dati->condivisi->INTERV);
	
	while(1){
		
		// attendo intervallo di tempo prima di comunicare
		if(nanosleep(&intervallo, NULL) == -1){
			perror("nanosleep");
			fprintf(
				stderr,
				"{GESTIONE ERRORI} : il cassiere [%d] non aggiorna il direttore\n",
				*(dati->index)
			);
		}
		
		pthread_mutex_lock(&(dati->my_cassa->mutex));
			pthread_mutex_lock(&(dati->my_aggiornamento->mutex));
			// aggiorno lunghezza coda alla cassa
			dati->my_aggiornamento->len_coda = dati->my_cassa->len;
			pthread_mutex_unlock(&(dati->my_aggiornamento->mutex));
			// termino se la cassa viene chiusa
			if(dati->my_cassa->flag_chiusa == 1){
				pthread_mutex_unlock(&(dati->my_cassa->mutex));
				break;
			}
		pthread_mutex_unlock(&(dati->my_cassa->mutex));
		
		// verifico se c'è un cassiere che deve sostituirmi
		pthread_mutex_lock(&(dati->my_cassa->mutex_flag));
		int tot_cassieri = dati->my_cassa->tot_cassieri;
		pthread_mutex_unlock(&(dati->my_cassa->mutex_flag));
		if(tot_cassieri > 1) break;
		
		// verifico se il supermercato è in chiusura
		pthread_mutex_lock(&(dati->condivisi->chiusura->mutex));
		int codice = dati->condivisi->chiusura->codice;
		pthread_mutex_unlock(&(dati->condivisi->chiusura->mutex));
		if(codice > 0){
			// verifico se ci sono clienti nel supermercato
			pthread_mutex_lock(&(dati->condivisi->tot_clienti->mutex));
			int tot = dati->condivisi->tot_clienti->n;
			pthread_mutex_unlock(&(dati->condivisi->tot_clienti->mutex));
			// termino se il supermercato è in chiusura e non ci sono clienti
			if(tot == 0) break;
		}
	}
	
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////

/* PARTE DELL'ENTITA' CASSIERE CHE SERVE I CLIENTI NELLA SUA CODA */
void* th_cassiere(void* arg){
	
	struct DatiCassiere *dati = (struct DatiCassiere *) arg;
	
	pthread_mutex_lock(&(dati->my_cassa->mutex_flag));
		// incremento il totale di cassieri che devono utilizzare la cassa
		(dati->my_cassa->tot_cassieri)++;
		// attendo che la cassa sia liberata dal cassiere precedente
		while(dati->my_cassa->flag_in_uso == 1){
			pthread_cond_wait(&(dati->my_cassa->cond_flag), &(dati->my_cassa->mutex_flag));
		}
		// segnalo cassa come occupata
		dati->my_cassa->flag_in_uso = 1;
	pthread_mutex_unlock(&(dati->my_cassa->mutex_flag));
	
	// inizializzazione del thread che comunica al direttore
	pthread_t id_cassiere_agg;
	if(pthread_create(&id_cassiere_agg, NULL, th_cassiere_agg, arg) != 0){
		fprintf(stderr, "pthread_create : errore fatale\n");
		fprintf(
			stderr,
			"{GESTIONE ERRORI} : il cassiere [%d] termina e la cassa viene chiusa\n",
			*(dati->index)
		);
		// chiudo la cassa
		pthread_mutex_lock(&(dati->my_cassa->mutex));
		dati->my_cassa->flag_chiusa = 1;
		// avverto tutti i clienti in coda
		while(dati->my_cassa->len > 0){
			struct elem_coda *e = dati->my_cassa->inizio;
			pthread_mutex_lock(&(e->mutex));
			e->flag_servito = -1;
			// rimuovo il cliente dalla coda
			lascia_la_coda(e, dati->my_cassa);
			pthread_cond_signal(&(e->cond));
			pthread_mutex_unlock(&(e->mutex));
		}
		pthread_mutex_unlock(&(dati->my_cassa->mutex));
		return NULL;
	}
	
	// == LOG.TXT ===========================
	// dati da memorizzare nel file di log
	struct timeval t0, t1, ts0, ts1;
	// ======================================
	
	// generazione seed diverso per ogni thread cassiere
	unsigned int seed = clock();
	// generazione tempo costante per ogni servizio cliente
	int t_const = rand_r(&seed) % (SOGLIA_SUP_SERV - SOGLIA_INF_SERV + 1) + SOGLIA_INF_SERV;
	
	// crea struttura per la gestione dei nanosecondi
	struct timespec time_const;
	SET_TIMESPEC_FROM_MS(time_const, t_const);
	
	/*************************************************************************************/
	int flag_termina = 0;
	
	// [LOG] acquisizione tempo iniziale
	CHECK_GETTIMEOFDAY(t0);
	
	while(1){
		
		pthread_mutex_lock(&(dati->my_cassa->mutex));
			// attendo che arrivi almeno un cliente o che la cassa chiuda
			while((dati->my_cassa->len == 0) && (dati->my_cassa->flag_chiusa == 0)){
				
				// verifico se c'è un cassiere che deve sostituirmi
				pthread_mutex_lock(&(dati->my_cassa->mutex_flag));
				int tot_cassieri = dati->my_cassa->tot_cassieri;
				pthread_mutex_unlock(&(dati->my_cassa->mutex_flag));
				if(tot_cassieri > 1){
					flag_termina = 1;
					break;
				}
				
				// verifico se il supermercato è in chiusura
				pthread_mutex_lock(&(dati->condivisi->chiusura->mutex));
				int codice = dati->condivisi->chiusura->codice;
				pthread_mutex_unlock(&(dati->condivisi->chiusura->mutex));
				
				if(codice > 0){
					// verifico se ci sono clienti nel supermercato
					pthread_mutex_lock(&(dati->condivisi->tot_clienti->mutex));
					int tot = dati->condivisi->tot_clienti->n;
					pthread_mutex_unlock(&(dati->condivisi->tot_clienti->mutex));

					// termino se il supermercato è in chiusura e non ci sono clienti
					if(tot == 0){
						flag_termina = 1;
						break;	
					}
				}
				
				pthread_cond_wait(&(dati->my_cassa->cond), &(dati->my_cassa->mutex));
			}
			
			// [LOG] acquisizione tempo finale
			CHECK_GETTIMEOFDAY(t1);
			
			if(flag_termina == 1) break;
			
			// se la cassa è chiusa
			if(dati->my_cassa->flag_chiusa == 1){
				while(dati->my_cassa->len > 0){
					struct elem_coda *e = dati->my_cassa->inizio;
					pthread_mutex_lock(&(e->mutex));
					e->flag_servito = -1;
					// rimuovo il cliente dalla coda
					lascia_la_coda(e, dati->my_cassa);
					pthread_cond_signal(&(e->cond));
					pthread_mutex_unlock(&(e->mutex));
				}
				// termino
				break;
			}
			
			// estraggo cliente da servire
			struct elem_coda *e = estrai_elemento_da_coda(dati->my_cassa);
			
			// [LOG] incremento il totale di clienti serviti
			(dati->my_cassa->tot_servizi)++;
			// [LOG] incremento il totale di prodotti
			(dati->my_cassa->tot_prodotti) += (e->my_p);
			
		pthread_mutex_unlock(&(dati->my_cassa->mutex));
		
		//== SERVO UN SINGOLO CLIENTE =====================================================
		pthread_mutex_lock(&(e->mutex));
			struct timespec time_linear;
			SET_TIMESPEC_FROM_MS(time_linear, (dati->condivisi->PROD)*(e->my_p));
			
			// [LOG] acquisizione tempo di servizio iniziale
			CHECK_GETTIMEOFDAY(ts0);
			(e->ts0).tv_sec = ts0.tv_sec;
			(e->ts0).tv_usec = ts0.tv_usec;
			
			// simulo tempo costante di servizio
			if(nanosleep(&time_const, NULL) == -1){
				perror("nanosleep");
				fprintf(
					stderr,
					"{GESTIONE ERRORI} : il cassiere [%d] non attende tempo costante nel servizio\n",
					*(dati->index)
				);
			}
			// simulto tempo lineare di servizio per ogni prodotto
			if(nanosleep(&time_linear, NULL) == -1){
				perror("nanosleep");
				fprintf(
					stderr,
					"{GESTIONE ERRORI} : il cassiere [%d] non attende tempo lineare nel servizio\n",
					*(dati->index)
				);
			}
			
			// [LOG] acquisizione tempo di servizio finale
			CHECK_GETTIMEOFDAY(ts1);
			
			e->flag_servito = 1;
			// sveglio cliente al termine del servizio
			pthread_cond_signal(&(e->cond));
		pthread_mutex_unlock(&(e->mutex));
		
		// [LOG] scrittura durata servizio su file di log
		pthread_mutex_lock(&(dati->condivisi->log->mutex));
		fprintf(dati->condivisi->log->fp,
			"CASSA;%d;%f\n",
			*(dati->index), diff_timeval(ts0, ts1)
		);
		pthread_mutex_unlock(&(dati->condivisi->log->mutex));
	}
	pthread_mutex_unlock(&(dati->my_cassa->mutex));
	/**************************************************************************************/
	
	// [LOG] scrittura durata apertura cassa su file di log
	pthread_mutex_lock(&(dati->condivisi->log->mutex));
	fprintf(dati->condivisi->log->fp,
		"CASSA_CHIUSA;%d;%f\n",
		*(dati->index), diff_timeval(t0, t1)
	);
	pthread_mutex_unlock(&(dati->condivisi->log->mutex));
	
	pthread_mutex_lock(&(dati->my_cassa->mutex));
	// [LOG] incremento il totale di chiusure della cassa
	(dati->my_cassa->tot_chiusure)++;
	pthread_mutex_unlock(&(dati->my_cassa->mutex));
	
	pthread_mutex_lock(&(dati->my_cassa->mutex_flag));
		// decremento il totale di cassieri che devono utilizzare la cassa
		(dati->my_cassa->tot_cassieri)--;
		// segnalo che la cassa è libera per il cassiere successivo
		dati->my_cassa->flag_in_uso = 0;
		pthread_cond_signal(&(dati->my_cassa->cond_flag));
	pthread_mutex_unlock(&(dati->my_cassa->mutex_flag));
	
	// attendo termine thread che comunica al direttore
	pthread_join(id_cassiere_agg, NULL);
	
	#ifdef DEBUG
	printf("<del> CASSIERE [%d] <del>\n", *(dati->index));
	#endif
	
	free(dati->index);
	free(dati);
	
	return NULL;
}
