#if !defined(CASSIERE_H_)
#define CASSIERE_H_

#define SOGLIA_INF_SERV 20
#define SOGLIA_SUP_SERV 80

#define SET_TIMESPEC_FROM_MS(var, ms)		\
	var.tv_sec = ms / 1000;					\
	var.tv_nsec = (ms % 1000) * 1000000;


// header del cassiere ha bisogno della dichiarazione di DatiDirettore
struct DatiDirettore;


struct Aggiornamento {
	int len_coda;
	pthread_mutex_t mutex;
};

struct DatiCassiereCondivisi {
	int INTERV;
	int PROD;
	struct Chiusura *chiusura;
	struct TotClienti *tot_clienti;
	struct Log *log;
};

struct DatiCassiere {
	struct DatiCassiereCondivisi *condivisi;
	int *index;
	struct Cassa *my_cassa;
	struct Aggiornamento *my_aggiornamento;
};


// creazione struttura dati cassiere condivisi
struct DatiCassiereCondivisi * raggruppa_dati_cassiere_condivisi(
 struct config *conf, struct Chiusura *chiusura, struct TotClienti *tot_clienti, struct Log *log
);

// creazione struttura dati cassiere
struct DatiCassiere * crea_dati_cassiere(struct DatiDirettore *dd, int index);

/* PARTE DELL'ENTITA' CASSIERE CHE AGGIORNA IL DIRETTORE SULLA LUNGHEZZA DELLA CODA */
void* th_cassiere_agg(void* arg);

/* PARTE DELL'ENTITA' CASSIERE CHE SERVE I CLIENTI NELLA SUA CODA */
void* th_cassiere(void* arg);

#endif
