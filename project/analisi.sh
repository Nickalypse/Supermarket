#!/bin/bash
# imposta il '.' come separatore della parte decimale nei float
LC_NUMERIC=C

# definizioni non necessarie ma mantenute per chiarezza
somma_t_servizio=()
cassa_t_tot=()
cassa_tot_servizi=()
cassa_tot_chiusure=()
cassa_tot_prodotti=()

file_pid=./bin/supermercato.PID
file_log=./bin/log.txt

# attendi termine processo supermercato
while [ -e $file_pid ]; do
	sleep 0.5
	printf "."
done

if [ ! -f $file_log ]; then
    echo "ERRORE: $file_log non esiste."
	exit 1
fi

# apertura file di log
exec 4<$file_log

printf "\n\n|CLIENTE|PRODOTTI|T_TOTALE|T_TOT_CODA|CODE_VISITATE|\n"

# lettura del file log una riga per volta
while read -u 4 riga; do
	
	# riga suddivisa in sottostringhe memorizzate nell'array v
	IFS=';' read -r -a v <<< "$riga"
	
	# controllo parola iniziale della riga
	case "${v[0]}" in	#----------------------------------------------------------------------------------
		("CASSA")
			# se non inizializzato, mette elemento array a 0
			if [[ -z ${somma_t_servizio[${v[1]}]} ]]; then
				somma_t_servizio[${v[1]}]=0
			fi
			
			# aggiorno la somma dei tempi di servizio per la specifica cassa
			somma_t_servizio[${v[1]}]=$(bc <<< "${somma_t_servizio[${v[1]}]} + ${v[2]}")
		;;	#----------------------------------------------------------------------------------
		("CASSA_CHIUSA")
			# se non inizializzato, mette elemento array a 0
			if [[ -z ${cassa_t_tot[${v[1]}]} ]]; then
				cassa_t_tot[${v[1]}]=0
			fi
			
			# aggiorno la somma dei tempi di apertura per la specifica cassa
			cassa_t_tot[${v[1]}]=$(bc <<< "${cassa_t_tot[${v[1]}]} + ${v[2]}")
		;;	#----------------------------------------------------------------------------------
		("CLIENTE")
			printf "|%7d|%8d|%8.3f|%10.3f|%13d|\n" ${v[1]} ${v[4]} ${v[2]} \
				${v[3]} ${v[5]} >> not_sorted.txt
		;;	#----------------------------------------------------------------------------------
		("FINE_CASSA")
			cassa_tot_servizi[${v[1]}]=${v[2]}
			cassa_tot_chiusure[${v[1]}]=${v[3]}
			cassa_tot_prodotti[${v[1]}]=${v[4]}
		;;	#----------------------------------------------------------------------------------
		("SUPERMERCATO")
			tot_servizi=${v[1]}
			tot_prodotti=${v[2]}
		;;
	esac
done


# stampa clienti ordinati per ID
sort -n -k2 not_sorted.txt
# elimina file temporaneo con clienti disordinati
rm not_sorted.txt


# calcolo totale di casse
K=${#cassa_tot_servizi[@]}

printf "\n|CASSA|PRODOTTI|CLIENTI|T_APERTURA|T_MEDIO_SERV|CHIUSURE|\n"

# stampa dati delle casse
for (( i=0; i<K; i++ )) do
	
	# se il totale di servizio è nullo, non calcolo la media
	if [[ ${cassa_tot_servizi[$i]} -ne 0 ]]; then
		# calcolo tempo medio di servizio di ogni cassiere
		somma_t_servizio[$i]=$(bc -l <<< "${somma_t_servizio[$i]} / ${cassa_tot_servizi[$i]}")
	fi
	
	printf "|%5d|%8d|%7d|%10.3f|%12.3f|%8d|\n" $((i+1)) ${cassa_tot_prodotti[$i]}	\
		${cassa_tot_servizi[$i]} ${cassa_t_tot[$i]} ${somma_t_servizio[$i]}			\
		${cassa_tot_chiusure[$i]}
done

# non richiesto:
printf "\n|SUPERMERCATO|TOT_SERVIZI_IN_CASSA|TOT_PRODOTTI|\n"
printf "|            |%20d|%12d|\n\n" $tot_servizi $tot_prodotti

