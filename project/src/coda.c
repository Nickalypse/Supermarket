#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#include <coda.h>

/// FUNZIONI /////////////////////////////////////////////////////////////////////////////

struct elem_coda * crea_elemento_coda(int my_p){
	
	struct elem_coda *e = malloc(sizeof(struct elem_coda));
	if(e == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(e->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(e);
		return NULL;
	}
	if(pthread_cond_init(&(e->cond), NULL) != 0){
		fprintf(stderr, "pthread_cond_init : errore fatale\n");
		free(e);
		return NULL;
	}
	
	e->my_p = my_p;
	e->flag_servito = 0;
	e->prima = NULL;
	e->dopo = NULL;
	
	return e;
}

// creazione array di puntatori a code vuote (doppiamente collegate)
// ritorna array se successo, NULL se errore
struct Cassa * crea_array_di_casse(int dim){
	
	// allocazione di un array di puntatori a casse
	struct Cassa *array = malloc(sizeof(struct Cassa) * dim);
	// controllo esito malloc
	if(array == NULL){
		perror("malloc");
		return NULL;
	}
	
	// inizializzazione delle casse nell'array come vuote
	for(int i=0; i<dim; i++){	
		if(pthread_mutex_init(&(array[i].mutex), NULL) != 0){
			fprintf(stderr, "pthread_mutex_init : errore fatale\n");
			free(array);
			return NULL;
		}
		if(pthread_cond_init(&(array[i].cond), NULL) != 0){
			fprintf(stderr, "pthread_cond_init : errore fatale\n");
			free(array);
			return NULL;
		}
		if(pthread_mutex_init(&(array[i].mutex_flag), NULL) != 0){
			fprintf(stderr, "pthread_mutex_init : errore fatale\n");
			free(array);
			return NULL;
		}
		if(pthread_cond_init(&(array[i].cond_flag), NULL) != 0){
			fprintf(stderr, "pthread_cond_init : errore fatale\n");
			free(array);
			return NULL;
		}
		array[i].len = 0;
		array[i].flag_chiusa = 1;
		array[i].inizio = NULL;
		array[i].fine = NULL;
		array[i].tot_servizi = 0;
		array[i].tot_chiusure = 0;
		array[i].tot_prodotti = 0;
		array[i].flag_in_uso = 0;
		array[i].tot_cassieri = 0;
	}
	
	return array;
}


// aggiunge elemento in fondo alla coda nella cella dell'array specificata
void aggiungi_elemento_in_coda(struct elem_coda *e, struct Cassa *c){
	
	// nuovo elemento non è seguito da nessuno
	e->dopo = NULL;
	
	if(c->inizio == NULL){
		// nuovo elemento non è precedeuto da nessuno
		e->prima = NULL;
		// nuovo elemento diventa primo ed ultimo della coda
		c->inizio = e;
		c->fine = e;
	}
	else{
		// nuovo elemento è preceduto dall'attuale chiudifila
		e->prima = c->fine;
		// chiudifila è seguito dal nuovo elemento
		c->fine->dopo = e;
		// nuovo elemento diventa l'ultimo in coda
		c->fine = e;
	}
	
	// incremento lunghezza della coda
	(c->len)++;
}

// estrae elemento in testa alla coda nella cella dell'array specificata
// ritorna puntatore all'elemento se presente, NULL se coda vuota
struct elem_coda * estrai_elemento_da_coda(struct Cassa *c){
	
	// se coda vuota ritorno NULL
	if(c->len == 0) return NULL;
	
	// memorizzo puntatore all'attuale primo elemento in coda
	struct elem_coda *ris = c->inizio;
	
	// il secondo elemento in coda acquisisce la prima posizione
	c->inizio = c->inizio->dopo;
	
	// se la coda rimane vuota
	if(c->inizio == NULL){
		// metto a NULL il puntatore all'ultimo elemento
		c->fine = NULL;
	}
	else{
		// il secondo elemento in coda non ha più l'elemento davanti
		c->inizio->prima = NULL;
	}
	
	// decremento la lunghezza della coda
	(c->len)--;
	
	ris->dopo = NULL;
	return ris;
}

// rimuove elemento dalla coda
void lascia_la_coda(struct elem_coda *e, struct Cassa *c){
	
	// se è l'unico elemento in coda
	if(c->len == 1){
		c->inizio = NULL;
		c->fine = NULL;
	}
	// se è l'ultimo elemento in coda
	else if(e->dopo == NULL){
		c->fine = e->prima;
		c->fine->dopo = NULL;
		e->prima = NULL;
	}
	// se è il primo elemento in coda
	else if(e->prima == NULL){
		c->inizio = e->dopo;
		c->inizio->prima = NULL;
		e->dopo = NULL;
	}
	// se è un elemento centrale
	else{
		e->prima->dopo = e->dopo;
		e->dopo->prima = e->prima;
		e->prima = NULL;
		e->dopo = NULL;
	}
	
	// decremento la lunghezza della coda
	(c->len)--;
}

