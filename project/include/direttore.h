#if !defined(DIRETTORE_H_)
#define DIRETTORE_H_

#define SET_TIMESPEC_FROM_MS(var, ms)		\
	var.tv_sec = ms / 1000;					\
	var.tv_nsec = (ms % 1000) * 1000000;

/// STRUTTURE ///////////////////////////////////////////////////////////////////////////////////

// struttura con i soli dati indirizzati al thread direttore
struct DatiDirettore {
	int K;
	int C;
	int E;
	int S1;
	int S2;
	int CASSE;
	struct Cassa *array_casse;
	struct Aggiornamento *array_aggiornamenti;
	struct join *lista_clienti;
	struct permessi *lista_permessi;
	struct DatiCliente *dati_cliente;
	struct DatiCassiereCondivisi *dati_cassiere_condivisi;
	struct TotClienti *tot_clienti;
	struct Chiusura *chiusura;
	struct Log *log;
};

// creazione array di dati aggiornati dai cassieri per il direttore
struct Aggiornamento * crea_array_aggiornamenti(int dim);

/// GESTIONE STRUTTURE /////////////////////////////////////////////////////////////////////////

// creazione struttura dati direttore
struct DatiDirettore * raggruppa_dati_direttore(
 struct config *conf, struct join *l, struct DatiCliente *d, struct DatiCassiereCondivisi *dcc,
 struct Chiusura *c, struct Aggiornamento *array_agg
);

/// FUNZIONI ///////////////////////////////////////////////////////////////////////////////////

/* PARTE DELL'ENTITA' DIRETTORE CHE GESTISCE LE RICHIESTE DI USCITA DEI CLIENTI SENZA PRODOTTI */
void* th_direttore_p(void* arg);

int crea_nuovo_cassiere(struct DatiDirettore *dati, struct join *lista_cassieri, int index);

// apertura e chiusura di casse in base alla lunghezza delle code
void gestisci_casse(struct DatiDirettore *dati, struct join *lista_cassieri, unsigned int *seed);

/* PARTE DELL'ENTITA' DIRETTORE CHE GESTISCE APERTURA E CHIUSURA DELLE CASSE */
void* th_direttore_c(void* arg);

int crea_nuovo_cliente(struct DatiDirettore *dati);

/* PARTE DELL'ENTITA' DIRETTORE CHE GESTISCE IL TOTALE DI CLIENTI NEL SUPERMERCATO */
void* th_direttore(void* arg);

#endif
