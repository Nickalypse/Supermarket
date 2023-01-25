#if !defined(LISTA_H_)
#define LISTA_H_

/// STRUTTURE ////////////////////////////////////////////////////////////////////////////

/*** JOIN *******************************************************************************/

// singolo elemento di una lista di thread che attendono la pthread_join
struct elem_join {
	pthread_t id;
	int flag_ultimo;
	struct elem_join *next;
};

// riferimenti al primo e all'ultimo thread in lista di attesa per pthread_join
struct join {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct elem_join *inizio;
	struct elem_join *fine;
};

/*** PERMESSI ***************************************************************************/

// struttura per la richiesta di uscita dei clienti senza prodotti
struct elem_permessi {
	int confermato;
	int flag_ultimo;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct elem_permessi *next;
};

// riferimenti alla prima e all'ultima richiesta di uscita dei clienti
struct permessi {
	int len;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct elem_permessi *inizio;
	struct elem_permessi *fine;
};

/// FUNZIONI /////////////////////////////////////////////////////////////////////////////

/*** JOIN *******************************************************************************/

// creazione struttura con riferimenti a lista di attesa thread (vuota) per pthread_join
struct join * crea_lista_join();

// crea elemento lista che identifica un thread
struct elem_join * crea_elem_join();

// aggiunge elemento in fondo alla lista di attesa pthread_join
void aggiungi_thread_in_lista_join(struct elem_join *e, struct join *l);

// estrae elemento in testa alla lista di attesa pthread_join
struct elem_join * estrai_thread_da_lista_join(struct join *l);

/*** PERMESSI ***************************************************************************/

// creazione struttura con riferimenti a lista di permessi di uscita clienti
struct permessi * crea_lista_permessi();

// crea elemento lista che identifica una richiesta di uscita senza prodotti
struct elem_permessi * crea_elem_permessi();

// aggiunge elemento in fondo alla lista di permessi
void aggiungi_elemento_in_lista_permessi(struct elem_permessi *e, struct permessi *l);

// estrae elemento in testa alla lista di permessi
struct elem_permessi * estrai_elemento_da_lista_permessi(struct permessi *l);

#endif
