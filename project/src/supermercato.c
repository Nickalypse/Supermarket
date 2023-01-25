#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include <config.h>
#include <lista.h>
#include <coda.h>
#include <log.h>
#include <signalhandler.h>
#include <cliente.h>
#include <cassiere.h>
#include <direttore.h>

#define FILE_PID "./bin/supermercato.PID"

#define CHECK_NULL(var, fun)	\
	if((var=fun) == NULL){		\
		goto dealloca;			\
	}

#define CHECK_PTHREAD(fun, err)									\
	if(fun != 0){												\
		fprintf(stderr, "pthread_%s : errore fatale\n", err);	\
		goto dealloca;											\
	}

////////////////////////////////////////////////////////////////////////////////////////////////

int blocca_tutti_i_segnali(){
	sigset_t mask;
	// inizializza ad 1 la maschera ==> blocca tutti i segnali consentiti
	if(sigfillset(&mask) == -1){
		fprintf(stderr, "sigfillset : errore fatale\n");
		return -1;
	}
	// applica nuova maschera al thread che la esegue
	if(pthread_sigmask(SIG_SETMASK, &mask, NULL) != 0){
		fprintf(stderr, "pthread_sigmask : errore fatale\n");
		return -1;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]){
	
	// controllo totale argomenti
	if(argc > 2){
		fprintf(stderr, "ERRORE: atteso al piu' 1 parametro [config.txt]\n");
		exit(EXIT_FAILURE);
	}
	
	// thread attuale ed i suoi thread figli hanno segnali bloccati (tranne signal handler)
	if(blocca_tutti_i_segnali() == -1) exit(EXIT_FAILURE);
	
	/*** ALLOCAZIONE FILE DI LOG ***************************************************************/
	// apertura in scrittura del file di log
	struct Log *log;
	CHECK_NULL(log, crea_struttura_log());

	/*** ALLOCAZIONE CONFIG ********************************************************************/
	// estrazione dati per l'inizializzazione dal file di configurazione
	struct config *conf;
	CHECK_NULL(conf, lettura_config(argv[1]));
	
	/*** ALLOCAZIONE CHIUSURA ******************************************************************/	
	// creazione struttura dati per comunicaere la chiusura del supermercato
	struct Chiusura *chiusura;
	CHECK_NULL(chiusura, crea_chiusura());
	
	/*** ALLOCAZIONE ARRAY CASSE ***************************************************************/
	// creazione array di casse
	struct Cassa *array_casse;
	CHECK_NULL(array_casse, crea_array_di_casse(conf->K));
	
	/*** ALLOCAZIONE ARRAY AGGIORNAMENTI *******************************************************/
	// creazione array di aggiornamenti per il direttore sulla lunghezza delle code
	struct Aggiornamento *array_aggiornamenti;
	CHECK_NULL(array_aggiornamenti, crea_array_aggiornamenti(conf->K));
	
	/*** ALLOCAZIONE DATI SIGNAL HANDLER *******************************************************/
	// creazione struttura dati per il signal handler
	struct DatiSignalHandler *dati_signal_handler;
	CHECK_NULL(
		dati_signal_handler,
		raggruppa_dati_signal_handler(conf->K, array_casse, chiusura)
	);
	
	/*** ALLOCAZIONE LISTA JOIN DEI CLIENTI ***************************************************/
	// creazione lista di thread clienti in attesa di pthread_join
	struct join *lista_clienti;
	CHECK_NULL(lista_clienti, crea_lista_join());
	
	/*** ALLOCAZIONE LISTA RICHIESTE DI USCITA CLIENTI ****************************************/
	// creazione lista di richieste di uscita dei clienti senza prodotti
	struct permessi *lista_permessi;
	CHECK_NULL(lista_permessi, crea_lista_permessi());
	
	/*** ALLOCAZIONE TOTALE DI CLIENTI ********************************************************/
	// variabile condivisa che memorizza totale di clienti nel supermercato in ogni istante
	struct TotClienti *tot_clienti;
	CHECK_NULL(tot_clienti, crea_totale_clienti());

	/*** ALLOCAZIONE ID CLIENTE ***************************************************************/
	// variabile condivisa che memorizza totale di clienti nel supermercato in ogni istante
	struct IdCliente *id_cliente;
	CHECK_NULL(id_cliente, crea_id_cliente());
	
	/*** ALLOCAZIONE DATI CASSIERE ************************************************************/
	// creazione struttura dati per il cliente
	struct DatiCassiereCondivisi *dati_cassiere;
	CHECK_NULL(
		dati_cassiere,
		raggruppa_dati_cassiere_condivisi(conf, chiusura, tot_clienti, log)
	);
	
	/*** ALLOCAZIONE DATI CLIENTE *************************************************************/
	// creazione struttura dati per il cliente
	struct DatiCliente *dati_cliente;
	CHECK_NULL(
		dati_cliente,
	raggruppa_dati_cliente(conf, array_casse, tot_clienti, id_cliente, lista_permessi, chiusura, log)
	);
	
	/*** ALLOCAZIONE DATI DIRETTORE ***********************************************************/
	// creazione struttura dati per il direttore
	struct DatiDirettore *dati_direttore;
	CHECK_NULL(
		dati_direttore,
		raggruppa_dati_direttore(
			conf, lista_clienti, dati_cliente, dati_cassiere, chiusura, array_aggiornamenti
		)
	);
		
	/*** INIZIALIZZAZIONE SIGNAL HANDLER *******************************************************/
	// inizializzazione thread signal handler
	pthread_t id_signal_handler;
	CHECK_PTHREAD(
	 pthread_create(&id_signal_handler, NULL, th_signal_handler, (void*)dati_signal_handler),
	 "create"
	);
	
	/*** INIZIALIZZAZIONE DIRETTORE ***********************************************************/
	// inizializzazione thread direttore
	pthread_t id_direttore;
	if(pthread_create(&id_direttore, NULL, th_direttore, (void*)dati_direttore) != 0){
		fprintf(stderr, "pthread_create : errore fatale\n");
		// termina signal handler tramite segnale SIGQUIT
		kill(getpid(), SIGQUIT);
		// attesa termine thread signal handler
		pthread_join(id_signal_handler, NULL);
		goto dealloca;
	}
	
	/*** JOIN DEI THREAD CLIENTI ***************************************************************/
	// rimuove thread clienti in lista di attesa con pthread_join [mutua esclusione]
	while(1){
		pthread_mutex_lock(&(lista_clienti->mutex));
		while(lista_clienti->inizio == NULL){
			pthread_cond_wait(&(lista_clienti->cond), &(lista_clienti->mutex));
		}
		struct elem_join *e = estrai_thread_da_lista_join(lista_clienti);
		pthread_mutex_unlock(&(lista_clienti->mutex));
		// se tutti i clienti sono usciti ed il supermercato è chiuso non faccio più join
		if(e->flag_ultimo == 1){
			free(e);
			break;
		}
		// attesa termine esecuzione del thread cliente
		if(pthread_join(e->id, NULL) != 0) fprintf(stderr, "pthread_join : errore fatale\n");
		free(e);
	}
	#ifdef DEBUG
	printf("[FINE JOIN CLIENTI]\n");
	#endif
	
	/*** TERMINA CASSIERI IN ATTESA SU CODE VUOTE **********************************************/
	// sveglia i cassieri che non hanno clienti
	for(int i=0; i<(conf->K); i++){
		pthread_mutex_lock(&(array_casse[i].mutex));
		if(array_casse[i].flag_chiusa == 0)
			pthread_cond_signal(&(array_casse[i].cond));
		pthread_mutex_unlock(&(array_casse[i].mutex));
	}
	
	/*** ATTESA DEI THREAD *********************************************************************/
	// attesa termine thread signal handler
	CHECK_PTHREAD(pthread_join(id_signal_handler, NULL), "join");
	#ifdef DEBUG
	printf("[FINE SIGNAL HANDLER]\n");
	#endif
	
	// attesa termine thread direttore
	CHECK_PTHREAD(pthread_join(id_direttore, NULL), "join");
	#ifdef DEBUG
	printf("[FINE DIRETTORE]\n");
	#endif
	
	/*** AGGIORNAMENTO FILE DI LOG *************************************************************/
	int tot_servizi = 0;
	int tot_prodotti = 0;
	pthread_mutex_lock(&(log->mutex));
	// [LOG] scrittura dati casse su file di log
	for(int i=0; i<(conf->K); i++){
		pthread_mutex_lock(&(array_casse[i].mutex));
		fprintf(log->fp,
			"FINE_CASSA;%d;%d;%d;%d\n",
			i, array_casse[i].tot_servizi, array_casse[i].tot_chiusure,
			array_casse[i].tot_prodotti
		);
		tot_servizi += array_casse[i].tot_servizi;
		tot_prodotti += array_casse[i].tot_prodotti;
		pthread_mutex_unlock(&(array_casse[i].mutex));
	}
	// [LOG] scrittura dati supermercato su file di log
	fprintf(log->fp,
		"SUPERMERCATO;%d;%d\n",
		tot_servizi, tot_prodotti
	);
	pthread_mutex_unlock(&(log->mutex));
	
	// chiusura del file di log
	fclose(log->fp);
	
	
dealloca:
	
	/*** DEALLOCAZIONI *************************************************************************/
	// deallocazione struttura per file di log	
	free(log);
	// deallocazione struttura per chiusura supermercato
	free(chiusura);
	// deallocazione struttura con totale di clienti nel supermercato
	free(tot_clienti);
	// deallocazione struttura con id del nuovo cliente
	free(id_cliente);
	// deallocazione struttura dati signal handler
	free(dati_signal_handler);
	// deallocazione struttura dati cassiere
	free(dati_cassiere);
	// deallocazione struttura dati cliente
	free(dati_cliente);
	// deallocazione struttura dati direttore
	free(dati_direttore);
	// deallocazione struttura con riferimenti alla lista thread clienti in attesa di join
	free(lista_clienti);
	// deallocazione struttura con riferimenti alla lista di richieste di uscita dei clienti
	free(lista_permessi);
	// deallocazione array aggiornamenti
	free(array_aggiornamenti);
	// deallocazione array di casse
	free(array_casse);
	// deallocazione struttura con dati di configurazione
	free(conf);
	
	// elimina file con pid del processo
	unlink(FILE_PID);
	
	#ifdef DEBUG
	printf("\n[FINE PROCESSO]\n\n");
	#endif
	return 0;
}
