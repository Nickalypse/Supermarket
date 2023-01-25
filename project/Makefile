CC		= gcc -g -std=c99 -D_POSIX_C_SOURCE=200112L
FLAG	= -Wall

INCDIR  = ./include
LIBDIR  = ./lib
SRCDIR  = ./src
BINDIR  = ./bin
TESTDIR = ./test

LIBLIST = -lentita -lutilita -lpthread
TARGET  = $(BINDIR)/supermercato

####################################################################################

.PHONY: all run debug test clear

all: $(TARGET)

run: $(TARGET)
	clear
	$(TARGET) ./config.txt

debug: $(TARGET)
	clear
	valgrind --leak-check=full --show-reachable=yes -v $(TARGET) config.txt

test: $(TARGET)
	clear
	@echo "[AVVIO SUPERMERCATO]"
	@($(TARGET) ./test/config.txt > ./bin/output.txt 2>./bin/errori.txt & \
		echo $$! > ./bin/supermercato.PID) &
	@echo "./bin/supermercato ./test/config.txt"
	@echo "[ATTENDO 25s]"
	@sleep 25
	@echo "[INVIO SEGNALE SIGHUP]"
	@kill -1 $$(cat ./bin/supermercato.PID)
	@printf "[AVVIO SCRIPT DI ANALISI] "
	@chmod +x ./analisi.sh
	@./analisi.sh

clear:
	@echo "{RIMOZIONE FILE OGGETTO}"
	-rm -f $(SRCDIR)/*.o core *~
	@echo "{RIMOZIONE LIBRERIE}"
	-rm -f $(LIBDIR)/*.so core *~
	@echo "{RIMOZIONE ESEGUIBILI}"
	-rm -f $(BINDIR)/supermercato
	@echo "{RIMOZIONE FILE OUTPUT}"
	-rm -f $(BINDIR)/*.txt core *~
	clear
	
####################################################################################

# CREAZIONE ESEGUIBILE
$(TARGET): $(LIBDIR)/libentita.so $(LIBDIR)/libutilita.so $(SRCDIR)/supermercato.c
	$(CC) $(SRCDIR)/supermercato.c -o $@ -I $(INCDIR) -L $(LIBDIR) $(LIBLIST) -Wl,-rpath,$(LIBDIR)

# CREAZIONE LIBRERIE
$(LIBDIR)/libentita.so: $(SRCDIR)/direttore.o $(SRCDIR)/cassiere.o $(SRCDIR)/cliente.o $(SRCDIR)/signalhandler.o
	$(CC) -shared -o $@ $^
$(LIBDIR)/libutilita.so: $(SRCDIR)/config.o $(SRCDIR)/log.o $(SRCDIR)/lista.o $(SRCDIR)/coda.o
	$(CC) -shared -o $@ $^

# CREAZIONE FILE OGGETTO
$(SRCDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(FLAG) $< -c -fPIC -o $@ -I $(INCDIR)
