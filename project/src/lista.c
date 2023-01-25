#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <lista.h>

/*** JOIN *******************************************************************************/

// creazione struttura con riferimenti a lista di attesa thread (vuota) per pthread_join
struct join * crea_lista_join(){
	
	struct join *l = malloc(sizeof(struct join));
	if(l == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(l->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(l);
		return NULL;
	}
	if(pthread_cond_init(&(l->cond), NULL) != 0){
		fprintf(stderr, "pthread_cond_init : errore fatale\n");
		free(l);
		return NULL;
	}
	l->inizio = NULL;
	l->fine = NULL;
	
	return l;
}

// crea elemento lista che identifica un thread
struct elem_join * crea_elem_join(){
	
	struct elem_join *e = malloc(sizeof(struct elem_join));
	if(e == NULL){
		perror("malloc");
		return NULL;
	}
	// se 1 ==>  termina il ciclo di pthread_join quando esce l'ultimo cliente
	e->flag_ultimo = 0;
	
	return e;
}

// aggiunge elemento in fondo alla lista di attesa pthread_join
void aggiungi_thread_in_lista_join(struct elem_join *e, struct join *l){
	
	// elemento inserito in fondo non ha altri dopo di lui
	e->next = NULL;
	
	if(l->inizio == NULL){		// se lista vuota
		l->inizio = e;
		l->fine = e;
	}
	else{						// se lista ha gia' elementi
		l->fine->next = e;
		l->fine = e;
	}
}

// estrae elemento in testa alla lista di attesa pthread_join
struct elem_join * estrai_thread_da_lista_join(struct join *l){
	
	struct elem_join *e = l->inizio;
	l->inizio = l->inizio->next;
	e->next = NULL;
	
	if(l->inizio == NULL)		// se lista rimane vuota
		l->fine = NULL;
	
	return e;
}


/*** PERMESSI ***************************************************************************/

// creazione struttura con riferimenti a lista di permessi di uscita clienti
struct permessi * crea_lista_permessi(){
	
	struct permessi *l = malloc(sizeof(struct permessi));
	if(l == NULL){
		perror("malloc");
		return NULL;
	}
	if(pthread_mutex_init(&(l->mutex), NULL) != 0){
		fprintf(stderr, "pthread_mutex_init : errore fatale\n");
		free(l);
		return NULL;
	}
	if(pthread_cond_init(&(l->cond), NULL) != 0){
		fprintf(stderr, "pthread_cond_init : errore fatale\n");
		free(l);
		return NULL;
	}
	l->len = 0;
	l->inizio = NULL;
	l->fine = NULL;
	
	return l;
}

// crea elemento lista che identifica una richiesta di uscita senza prodotti
struct elem_permessi * crea_elem_permessi(){
	
	struct elem_permessi *e = malloc(sizeof(struct elem_permessi));
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
	// se 1 ==>  termina il ciclo di pthread_join quando esce l'ultimo cliente
	e->flag_ultimo = 0;
	// se 1 ==> richiesta accolta, il cliente può terminare
	e->confermato = 0;
	
	return e;
}

// aggiunge elemento in fondo alla lista di permessi
void aggiungi_elemento_in_lista_permessi(struct elem_permessi *e, struct permessi *l){
	
	// elemento inserito in fondo non ha altri dopo di lui
	e->next = NULL;
	
	if(l->inizio == NULL){		// se lista vuota
		l->inizio = e;
		l->fine = e;
	}
	else{						// se lista ha gia' elementi
		l->fine->next = e;
		l->fine = e;
	}
}

// estrae elemento in testa alla lista di permessi
struct elem_permessi * estrai_elemento_da_lista_permessi(struct permessi *l){
	
	struct elem_permessi *e = l->inizio;
	l->inizio = l->inizio->next;
	e->next = NULL;
	
	if(l->inizio == NULL)		// se lista rimane vuota
		l->fine = NULL;
	
	return e;
}
