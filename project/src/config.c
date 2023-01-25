#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>

/// FUNZIONI ///////////////////////////////////////////////////////////////////////////////

struct config * lettura_config(char *file_name){

	FILE *fp;

	if(file_name == NULL){
		// se file config non specificato, apro quello di default
		fp = fopen(DEFAULT_CONFIG, "r");
	}
	else
		fp = fopen(file_name, "r");

	// controllo esito apertura file
	if(fp == NULL){
		perror("fopen");
		return NULL;
	}

	// allocazione struct che memorizzerà i valori di inizializzazione
	struct config *conf = malloc(sizeof(struct config));
	if(conf == NULL){
		perror("malloc");
		fclose(fp);
		return NULL;
	}
	
	// crea e inizializza array a zero
	int flag[TOT_PARAM] = {};
	
	char riga[SIZE_RIGA];
	
	//==========================================================================
	// lettura di una riga dal file
	fgets(riga, SIZE_RIGA*sizeof(char), fp);
	
	while(!feof(fp)){
	
		// estrazione nome parametro
		char *var = strtok(riga, "= \t\n");
		
		if(var != NULL){
			
			int value;
			// lettura valore intero dopo il carattere '='
			if(sscanf(riga+strlen(var)+1, "%d", &value) == 1){
			
				if(strcmp(var, "K")==0 && value>=SOGLIA_INF_K){
					conf->K = value;
					flag[0]++;
				}
				else if(strcmp(var, "C")==0 && value>=SOGLIA_INF_C){
					conf->C = value;
					flag[1]++;
				}
				else if(strcmp(var, "E")==0 && value>=SOGLIA_INF_E){
					conf->E = value;
					flag[2]++;
				}
				else if(strcmp(var, "T")==0 && value>=SOGLIA_INF_T){
					conf->T = value;
					flag[3]++;
				}
				else if(strcmp(var, "P")==0 && value>=SOGLIA_INF_P){
					conf->P = value;
					flag[4]++;
				}
				else if(strcmp(var, "S1")==0 && value>=SOGLIA_INF_S1){
					conf->S1 = value;
					flag[5]++;
				}
				else if(strcmp(var, "S2")==0 && value>=SOGLIA_INF_S2){
					conf->S2 = value;
					flag[6]++;
				}
				else if(strcmp(var, "INTERV")==0 && value>=SOGLIA_INF_INTERV){
					conf->INTERV = value;
					flag[7]++;
				}
				else if(strcmp(var, "PROD")==0 && value>=SOGLIA_INF_PROD){
					conf->PROD = value;
					flag[8]++;
				}
				else if(strcmp(var, "CASSE")==0 && value>=SOGLIA_INF_CASSE){
					conf->CASSE = value;
					flag[9]++;
				}
			}
		}
		
		// lettura della riga sucessiva dal file
		fgets(riga, SIZE_RIGA*sizeof(char), fp);
	}
	//==========================================================================
	
	// chiusura file di config
	fclose(fp);
	
	// verifico specifiche: E<C, CASSE<=K
	if((conf->E >= conf->C) || (conf->CASSE > conf->K)){
		fprintf(stderr, "ERRORE: verificare che il file '%s' sia corretto e completo\n", file_name);
		free(conf);
		return NULL;
	}
	
	// verifico di aver letto tutti i parametri necessari
	for(int i=0; i<TOT_PARAM; i++){
		if(flag[i] == 0){
			free(conf);
			return NULL;
		}	
	}
	
	// ritorna puntatore alla struttura
	return conf;
}
