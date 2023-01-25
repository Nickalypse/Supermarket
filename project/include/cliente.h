#if !defined(CLIENTE_H_)
#define CLIENTE_H_

#define SET_TIMESPEC(var, ms)				\
	struct timespec var;					\
	var.tv_sec = ms / 1000;					\
	var.tv_nsec = (ms % 1000) * 1000000;


// struttura con totale di clienti in ogni istante
struct TotClienti {
	int n;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

// struttura con id del prossimo cliente
struct IdCliente {
	int id;
	pthread_mutex_t mutex;
};

// struttura con i soli dati interessanti per il cliente
struct DatiCliente {
	int K;
	int T;
	int P;
	struct Cassa *array_casse;
	struct TotClienti *tot_clienti;
	struct IdCliente *id_cliente;
	struct permessi *lista_permessi;
	struct Chiusura *chiusura;
	struct Log *log;
};

// creazione struttura condivisa con totale di clienti
struct TotClienti * crea_totale_clienti();

// creazione struttura con id del prossimo cliente
struct IdCliente * crea_id_cliente();

// creazione struttura dati cliente
struct DatiCliente * raggruppa_dati_cliente(
 struct config *conf, struct Cassa *array_casse, struct TotClienti *tot_clienti,
 struct IdCliente *id_cliente, struct permessi *lista_permessi,
 struct Chiusura *chiusura, struct Log *log
);

// cerca una cassa aperta e inserisce in coda una struttura che identifica il cliente
int trova_cassa_aperta(struct DatiCliente *dati, unsigned int seed);

/* ENTITA' CLIENTE CHE SI METTE IN CODA | ATTENDE PERMESSO DI USCIRE | ESCE PER EMERGENZA */
void* th_cliente(void* arg);

#endif