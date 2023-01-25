#if !defined(CONFIG_H_)
#define CONFIG_H_

#define TOT_PARAM 10
#define SIZE_RIGA 128

#define DEFAULT_CONFIG "./test/config2.txt"

// VALORI SOGLIA INFERIORI:
#define SOGLIA_INF_K		1
#define SOGLIA_INF_C		2
#define SOGLIA_INF_E		1
#define SOGLIA_INF_T		10
#define SOGLIA_INF_P		0
#define SOGLIA_INF_S1		1
#define SOGLIA_INF_S2		2
#define SOGLIA_INF_INTERV	10
#define SOGLIA_INF_PROD		0
#define SOGLIA_INF_CASSE	1

struct config{
	int K;
	int C;
	int E;
	int T;
	int P;
	int S1;
	int S2;
	int INTERV;
	int PROD;
	int CASSE;
};

// ritorna struttura con valori di inizializzazione acquisiti
struct config * lettura_config(char *file_name);

#endif
