#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include <config.h>
#include <coda.h>
#include <lista.h>
#include <log.h>
#include <signalhandler.h>
#include <cliente.h>
#include <cassiere.h>
#include <direttore.h>

/// GESTIONE STRUTTURE /////////////////////////////////////////////////////////////////////////

// creazione array di dati aggiornati dai cassieri per il direttore
struct Aggiornamento * crea_array_aggiornamenti(int dim){
	
	struct Aggiornamento *array = malloc(sizeof(struct Aggiornamento) * dim);
	if(array == NULL){
		perror("malloc");
		return NULL;
	}
	// inizializzazione array con lunghezza nulla delle code
	for(int i=0; i<dim; i++){
		if(pthread_mutex_init(&(array[i].mutex), NULL) != 0){
			fprintf(stderr, "pthread_mutex_init : errore fatale\n");
			free(array);
			return NULL;
		}
		// all'inizio le code sono vuote
		array[i].len_coda = 0;
	}
	
	return array;
}

// creazione struttura dati direttore
struct DatiDirettore * raggruppa_dati_direttore(
 struct config *conf, struct join *l, struct DatiCliente *d, struct DatiCassiereCondivisi *dcc,
 struct Chiusura *c, struct Aggiornamento *array_agg
){
	struct DatiDirettore *dati = malloc(sizeof(struct DatiDirettore));
	if(dati == NULL){
		perror("malloc");
		return NULL;
	}
	// copia dati
	dati->K = conf->K;
	dati->C = conf->C;
	dati->E = conf->E;
	dati->S1 = conf->S1;
	dati->S2 = conf->S2;
	dati->CASSE = conf->CASSE;
	dati->array_casse = d->array_casse;
	dati->array_aggiornamenti = array_agg;
	dati->lista_clienti = l;
	dati->lista_permessi = d->lista_permessi;
	dati->dati_cliente = d;
	dati->dati_cassiere_condivisi = dcc;
	dati->tot_clienti = d->tot_clienti;
	dati->chiusura = c;
	dati->log = d->log;
	
	return dati;
}

/// FUNZIONI ///////////////////////////////////////////////////////////////////////////////////

/* PARTE DELL'ENTITA' DIRETTORE CHE GESTISCE LE RICHIESTE DI USCITA DEI CLIENTI SENZA PRODOTTI */
void* th_direttore_p(void* arg){
	
	struct DatiDirettore *dati = (struct DatiDirettore *)arg;
	
	// accetta permessi di uscita dei clienti senza prodotti in lista [mutua esclusione]
	while(1){
		
		pthread_mutex_lock(&(dati->lista_permessi->mutex));
		while(dati->lista_permessi->inizio == NULL){
			pthread_cond_wait(&(dati->lista_permessi->cond), &(dati->lista_permessi->mutex));
		}
		struct elem_permessi *e = estrai_elemento_da_lista_permessi(dati->lista_permessi);
		pthread_mutex_unlock(&(dati->lista_permessi->mutex));
		
		pthread_mutex_lock(&(e->mutex));
		// se il supermercato è in chiusura ==> interrompo
		if(e->flag_ultimo == 1){
			pthread_mutex_unlock(&(e->mutex));
			free(e);
			break;
		}
		// viede dato il consenso di uscire al cliente senza prodotti
		e->confermato = 1;
		// sveglia cliente che potrà uscire
		pthread_cond_signal(&(e->cond));
		pthread_mutex_unlock(&(e->mutex));
		
		// decremento il totale di permessi ancora da gestire
		pthread_mutex_lock(&(dati->lista_permessi->mutex));
		(dati->lista_permessi->len)--;
		pthread_cond_signal(&(dati->lista_permessi->cond));
		pthread_mutex_unlock(&(dati->lista_permessi->mutex));
	}
	
	#ifdef DEBUG
	printf("[FINE DIRETTORE_PERMESSI]\n");
	#endif
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////

int crea_nuovo_cassiere(struct DatiDirettore *dati, struct join *lista_cassieri, int index){
	
	struct DatiCassiere *dati_cassiere = crea_dati_cassiere(dati, index);
	if(dati_cassiere == NULL){
		/** GESTIONE ERRORE **/
		return -1;
	}
	
	// imposta cassa come aperta
	pthread_mutex_lock(&(dati->array_casse[index].mutex));
	dati->array_casse[index].flag_chiusa = 0;
	pthread_mutex_unlock(&(dati->array_casse[index].mutex));
	
	// creazione elemento con TID del cassiere per successiva join
	struct elem_join *e = crea_elem_join();
	if(e == NULL){
		/** GESTIONE ERRORE **/
		free(dati_cassiere);
		return -1;
	}
	// inizializzazione thread cassiere
	pthread_create(&(e->id), NULL, th_cassiere, (void*)dati_cassiere);
	
	// inserimento thread in lista cassieri per la join
	pthread_mutex_lock(&(lista_cassieri->mutex));
	aggiungi_thread_in_lista_join(e, lista_cassieri);
	pthread_mutex_unlock(&(lista_cassieri->mutex));
	
	return 0;
}

// apertura e chiusura di casse in base alla lunghezza delle code
void gestisci_casse(struct DatiDirettore *dati, struct join *lista_cassieri, unsigned int *seed){
	
	int *array_casse_chiuse = malloc(sizeof(int) * (dati->K));
	int *array_code_corte   = malloc(sizeof(int) * (dati->K));
	
	if(array_casse_chiuse == NULL || array_code_corte == NULL){
		free(array_casse_chiuse);
		free(array_code_corte);
		perror("malloc");
		fprintf(stderr, "{GESTIONE ERRORE} il direttore evita la gestione delle casse\n");
		return;
	}
	
	int tot_casse_chiuse  = 0;
	int tot_code_corte    = 0;
	int max_lunghezza     = -1;
	
	// scorro tutte le casse
	for(int i=0; i<(dati->K); i++){
		pthread_mutex_lock(&(dati->array_casse[i].mutex));
		// controllo se la cassa è aperta
		if(dati->array_casse[i].flag_chiusa == 0){
			pthread_mutex_lock(&(dati->array_aggiornamenti[i].mutex));
			// verifico se la cassa i-esima ha coda corta (S1)
			if(dati->array_aggiornamenti[i].len_coda <= 1){
				array_code_corte[tot_code_corte] = i;
				tot_code_corte++;
			}
			// memorizzo lunghezza massima delle code (S2)
			if(dati->array_aggiornamenti[i].len_coda > max_lunghezza){
				max_lunghezza = dati->array_aggiornamenti[i].len_coda;
			}
			#ifdef DEBUG
			printf("|%d| ", dati->array_aggiornamenti[i].len_coda);
			#endif
			pthread_mutex_unlock(&(dati->array_aggiornamenti[i].mutex));
		}
		else{
			// memorizzo indice della cassa chiusa
			array_casse_chiuse[tot_casse_chiuse] = i;
			tot_casse_chiuse++;
		}
		pthread_mutex_unlock(&(dati->array_casse[i].mutex));
	}
	#ifdef DEBUG
	printf("\n");
	#endif
	
	// GESTIONE S1
	if((tot_casse_chiuse != (dati->K - 1)) && (tot_code_corte >= (dati->S1))){

		// selezione casuale di una tra le casse con coda corta
		int index_coda_corta = array_code_corte[rand_r(seed) % tot_code_corte];

		pthread_mutex_lock(&(dati->array_casse[index_coda_corta].mutex));
		// chiudo cassa che ha coda troppo corta
		dati->array_casse[index_coda_corta].flag_chiusa = 1;
		// informo cassiere sulla chiusura della sua cassa
		pthread_cond_signal(&(dati->array_casse[index_coda_corta].cond));
		pthread_mutex_unlock(&(dati->array_casse[index_coda_corta].mutex));
		#ifdef DEBUG
		printf("=== HO CHIUSO UNA CASSA =====================================\n");
		#endif
	}
	// GESTIONE S2
	else if((tot_casse_chiuse > 0) && (max_lunghezza >= (dati->S2))){
		// verifico se il supermercato è in chiusura
		pthread_mutex_lock(&(dati->chiusura->mutex));
		int codice = dati->chiusura->codice;
		pthread_mutex_unlock(&(dati->chiusura->mutex));
		// se il negozio non è in chiusura
		if(codice == 0){

			// selezione casuale di una tra le casse chiuse
			int index_cassa_chiusa = array_casse_chiuse[rand_r(seed) % tot_casse_chiuse];

			// apro una nuova cassa e inizializzo il rispettivo thread cassiere
			if(crea_nuovo_cassiere(dati, lista_cassieri, index_cassa_chiusa) == -1){
				fprintf(
					stderr,
					"{GESTIONE ERRORE} : la cassa [%d] non viene aperta\n",
					index_cassa_chiusa
				);
			}
			#ifdef DEBUG
			printf("=== HO APERTO UNA CASSA =====================================\n");
			#endif
		}
	}
	free(array_casse_chiuse);
	free(array_code_corte);
}

// --------------------------------------------------------------------------------------------

/* PARTE DELL'ENTITA' DIRETTORE CHE GESTISCE APERTURA E CHIUSURA DELLE CASSE */
void* th_direttore_c(void* arg){
	
	struct DatiDirettore *dati = (struct DatiDirettore *)arg;
	
	// creazione lista per il join dei thread cassieri
	struct join *lista_cassieri = crea_lista_join();
	if(lista_cassieri == NULL){
		fprintf(stderr, "{GESTIONE ERRORE} : generato segnale SIGQUIT\n");
		kill(getpid(), SIGQUIT);
		return NULL;
	}
	
	// apertura delle casse iniziali
	for(int i=0; i < (dati->CASSE); i++){
		if(crea_nuovo_cassiere(dati, lista_cassieri, i) == -1){
			fprintf(stderr, "{GESTIONE ERRORE} : generato segnale SIGQUIT\n");
			kill(getpid(), SIGQUIT);
		}
	}
	
	// creazione struttura per attesa
	struct timespec intervallo;
	SET_TIMESPEC_FROM_MS(intervallo, dati->dati_cassiere_condivisi->INTERV);
	
	// generazione seed per sequenza di numeri casuali 
	unsigned int seed = time(NULL);
	
	while(1){
		// attendi intervallo prima di leggere gli aggiornamenti dei cassieri
		if(nanosleep(&intervallo, NULL) == -1){
			perror("nanosleep");
			fprintf(
				stderr,
				"{GESTIONE ERRORI} : il direttore non attende intervallo per il controllo aggiornamenti\n"
			);
		}
		
		// decido se chiudere o aprire nuove casse in base ad S1 e S2
		gestisci_casse(dati, lista_cassieri, &seed);
		
		// verifico se il supermercato è in chiusura
		pthread_mutex_lock(&(dati->chiusura->mutex));
		int codice = dati->chiusura->codice;
		pthread_mutex_unlock(&(dati->chiusura->mutex));
		if(codice > 0){
			// verifico se ci sono clienti nel supermercato
			pthread_mutex_lock(&(dati->tot_clienti->mutex));
			int tot = dati->tot_clienti->n;
			pthread_mutex_unlock(&(dati->tot_clienti->mutex));
			// termino se il supermercato è in chiusura e non ci sono clienti
			if(tot == 0) break;
		}
	}
	
	// esecuzione join per ogni thread cassiere che è stato creato
	while(lista_cassieri->inizio != NULL){
		struct elem_join *e = estrai_thread_da_lista_join(lista_cassieri);
		pthread_join(e->id, NULL);
		free(e);
	}
	
	// deallocazione lista per il join dei thread cassieri
	free(lista_cassieri);
	#ifdef DEBUG
	printf("[FINE DIRETTORE_CASSE]\n");
	#endif
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////

int crea_nuovo_cliente(struct DatiDirettore *dati){
	
	// crea elemento lista per singolo thread
	struct elem_join *c = crea_elem_join();
	if(c == NULL) return -1;
	
	// inizializza un nuovo thread cliente
	pthread_create(&(c->id), NULL, th_cliente, (void*)(dati->dati_cliente));
	
	// aggiunge thread clienti in lista di attesa per la pthread_join [mutua esclusione]
	pthread_mutex_lock(&(dati->lista_clienti->mutex));
	aggiungi_thread_in_lista_join(c, dati->lista_clienti);
	pthread_cond_signal(&(dati->lista_clienti->cond));
	pthread_mutex_unlock(&(dati->lista_clienti->mutex));
	
	// incrementa totale di clienti nel supermercato [mutua escluzione]
	pthread_mutex_lock(&(dati->tot_clienti->mutex));
	(dati->tot_clienti->n)++;
	pthread_mutex_unlock(&(dati->tot_clienti->mutex));
	
	return 0;
}

// ---------------------------------------------------------------------------------------------

/* PARTE DELL'ENTITA' DIRETTORE CHE GESTISCE IL TOTALE DI CLIENTI NEL SUPERMERCATO */
void* th_direttore(void* arg){
	
	struct DatiDirettore *dati = (struct DatiDirettore *) arg;
	#ifdef DEBUG
	printf("\n[DIRETTORE]\n");
	#endif
	
	// creazione thread che gestisce le casse
	pthread_t id_direttore_c;
	if(pthread_create(&id_direttore_c, NULL, th_direttore_c, (void*)dati) != 0){
		fprintf(stderr, "{GESTIONE ERRORE} : direttore_c non creato, generato segnale SIGQUIT\n");
		kill(getpid(), SIGQUIT);
		return NULL;
	}
	
	// creazione thread che gestisce i clienti senza prodotti
	pthread_t id_direttore_p;
	if(pthread_create(&id_direttore_p, NULL, th_direttore_p, (void*)dati) != 0){
		fprintf(stderr, "{GESTIONE ERRORE} : direttore_p non creato, generato segnale SIGQUIT\n");
		kill(getpid(), SIGQUIT);
		return NULL;
	}
	
	// creazione dei thread clienti iniziali
	for(int i=0; i<(dati->C); i++){
		if(crea_nuovo_cliente(dati) == -1){
			fprintf(stderr, "{GESTIONE ERRORE} : cliente non creato, generato segnale SIGQUIT\n");
			kill(getpid(), SIGQUIT);
		}
	}
	
	while(1){
	
		pthread_mutex_lock(&(dati->tot_clienti->mutex));
		// attende uscita di altri clienti
		while((dati->tot_clienti->n) > (dati->C - dati->E)){
			pthread_cond_wait(&(dati->tot_clienti->cond), &(dati->tot_clienti->mutex));
		}
		pthread_mutex_unlock(&(dati->tot_clienti->mutex));
		
		// creazione thread clienti quando il totale scende sotto la soglia C-E
		for(int i=0; i<dati->E; i++){

			// verifico se il supermercato è in chiusura
			pthread_mutex_lock(&(dati->chiusura->mutex));
			int codice = dati->chiusura->codice;
			pthread_mutex_unlock(&(dati->chiusura->mutex));
			
			// IN CHIUSURA ==> non entrano nuovi clienti
			if(codice > 0){
				
				#ifdef DEBUG
				printf("{METTI ULTIMI}\n");
				#endif
				
				// ULTIMO: inserisce elemento di terminazione per il ciclo di pthread_join clienti
				struct elem_join *c = crea_elem_join();
				if(c == NULL){
					fprintf(stderr, "{GESTIONE ERRORE} : effettuato un nuovo tentativo\n");
					i--;
					continue;
				}
				c->flag_ultimo = 1;
				pthread_mutex_lock(&(dati->lista_clienti->mutex));
				aggiungi_thread_in_lista_join(c, dati->lista_clienti);
				pthread_cond_signal(&(dati->lista_clienti->cond));
				pthread_mutex_unlock(&(dati->lista_clienti->mutex));
				
				// interrompo gestione permessi dopo che tutti i clienti hanno il consenso
				pthread_mutex_lock(&(dati->lista_permessi->mutex));
				while((dati->lista_permessi->len) != 0){
					pthread_cond_wait(&(dati->lista_permessi->cond), &(dati->lista_permessi->mutex));
				}
				pthread_mutex_unlock(&(dati->lista_permessi->mutex));
				
				// ULTIMO: inserisce elemento di terminazione per lista permessi
				struct elem_permessi *p = crea_elem_permessi();
				if(p == NULL){
					fprintf(stderr, "{GESTIONE ERRORE} : effettuato un nuovo tentativo\n");
					i--;
					continue;
				}
				p->flag_ultimo = 1;
				pthread_mutex_lock(&(dati->lista_permessi->mutex));
				aggiungi_elemento_in_lista_permessi(p, dati->lista_permessi);
				pthread_cond_signal(&(dati->lista_permessi->cond));
				pthread_mutex_unlock(&(dati->lista_permessi->mutex));
				
				// attesa termine gestione clienti senza prodotti
				pthread_join(id_direttore_p, NULL);
				// attesa termine gestione casse
				pthread_join(id_direttore_c, NULL);
				
				// termina esecuzione thread (attende join)
				return NULL;
			}
			
			if(crea_nuovo_cliente(dati) == -1){
				fprintf(stderr, "{GESTIONE ERRORE} : cliente non creato\n");
			}
		}
	}
}
