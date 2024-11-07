# Sveglia e Orologio in C

Questo progetto è un'applicazione in C che implementa un orologio digitale con funzioni di sveglia personalizzabili. Il programma consente di aggiungere, modificare ed eliminare sveglie, oltre a riprodurre suoni personalizzati o eseguire comandi al momento della sveglia.

## Funzionalità Principali

- **Orologio Digitale**: Visualizzazione dell'ora corrente in formato digitale.
- **Sveglie Multiple**: Possibilità di impostare fino a 10 sveglie con orari specifici.
- **Ripetizione Sveglia**: Opzione per impostare sveglie ripetitive.
- **Suoni Personalizzati**: Possibilità di selezionare un file audio personalizzato per ciascuna sveglia.
- **Comandi Personalizzati**: Esecuzione di comandi al momento della sveglia, utile per azioni automatizzate.
- **Salvataggio**: Salvataggio automatico delle sveglie impostate in un file.

## Dipendenze

Per eseguire il programma, assicurati di avere installate le seguenti librerie:
- **ncurses**: Per la gestione dell'interfaccia a schermo.
- **libao** e **libsndfile**: Per la riproduzione dei suoni.
- **pthread**: Per la gestione dei thread necessari per eseguire le sveglie in background.

Installa queste librerie su sistemi basati su Linux con il seguente comando:

```bash
sudo apt-get install libao-dev libsndfile1-dev libncurses5-dev
