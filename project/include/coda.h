#if !defined(CODA_H_)
#define CODA_H_

// singolo elemento della coda delle casse (lista doppia)
struct elem_coda {
	int my_p;
	int flag_servito;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct elem_coda *prima;
	struct elem_coda *dopo;
	struct timeval ts0;
};

// struttura della cassa con riferimenti e informazioni sulla coda
struct Cassa {
	int len;
	int flag_chiusa;
	int tot_servizi;
	int tot_chiusure;
	int tot_prodotti;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct elem_coda *inizio;
	struct elem_coda *fine;
	int flag_in_uso;
	int tot_cassieri;
	pthread_mutex_t mutex_flag;
	pthread_cond_t cond_flag;
};

/// FUNZIONI /////////////////////////////////////////////////////////////////////////////

// crea struttura da accodare ad una cassa che rappresenta il singolo cliente
struct elem_coda * crea_elemento_coda(int my_p);

// creazione array di puntatori a code vuote (doppiamente collegate)
// ritorna array se successo, NULL se errore
struct Cassa * crea_array_di_casse(int dim);


// aggiunge elemento in fondo alla coda nella cella dell'array specificata
void aggiungi_elemento_in_coda(struct elem_coda *e, struct Cassa *c);

// estrae elemento in testa alla coda nella cella dell'array specificata
// ritorna puntatore all'elemento se presente, NULL se coda vuota
struct elem_coda * estrai_elemento_da_coda(struct Cassa *c);

// rimuove elemento dalla coda
void lascia_la_coda(struct elem_coda *e, struct Cassa *c);

#endif
