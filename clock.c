#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <ncurses.h>
#include <ao/ao.h>
#include <sndfile.h>

#define BUFFER_SIZE 4096
#define TIME_ZONE 0
#define MAX_ALARMS 10
#define MAX_DESC_LENGTH 100
#define ALARM_FILE "alarms.dat"
#define MAX_PATH_LENGTH 256
#define MAX_COMMAND_LENGTH 256

const char *weekdays[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
const char *months[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};

struct Alarm {
    int hour;
    int minute;
    char description[MAX_DESC_LENGTH];
    int repeat;
    int custom_sound;
    char sound_path[MAX_PATH_LENGTH];
    int execute_payload;
    char payload_command[MAX_COMMAND_LENGTH];
};

struct Alarm alarms[MAX_ALARMS];
int alarm_count = 0;

int alarm_ringing = 0;
int stop_alarm = 0;
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

void play_sound(const char *filename) {
    ao_device *device;
    ao_sample_format format;
    int default_driver;
    SNDFILE *sndfile;
    SF_INFO sfinfo;
    short buffer[BUFFER_SIZE];
    int num_read;

    // Initialize libao
    ao_initialize();

    // Open the audio file
    sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (!sndfile) {
        fprintf(stderr, "Could not open audio file %s\n", filename);
        return;
    }

    // Set the output format
    format.bits = 16;
    format.channels = sfinfo.channels;
    format.rate = sfinfo.samplerate;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;

    // Open the default driver
    default_driver = ao_default_driver_id();
    device = ao_open_live(default_driver, &format, NULL);
    if (!device) {
        fprintf(stderr, "Could not open audio device\n");
        sf_close(sndfile);
        return;
    }

    // Play the audio file
    while ((num_read = sf_read_short(sndfile, buffer, BUFFER_SIZE)) > 0) {
        ao_play(device, (char *)buffer, num_read * sizeof(short));

        // Check if we need to stop the alarm
        pthread_mutex_lock(&alarm_mutex);
        if (stop_alarm) {
            pthread_mutex_unlock(&alarm_mutex);
            break;
        }
        pthread_mutex_unlock(&alarm_mutex);
    }

    // Close and cleanup
    ao_close(device);
    sf_close(sndfile);
    ao_shutdown();
}

void clear_screen() {
    printf("\x1b[0m");
    printf("\033[H\033[J");
}

void tocode(int value, int pos, int realClock[7][7]) {
    int result[7];

    switch (value) {
        case 0: result[0] = 1; result[1] = 1; result[2] = 0; result[3] = 1; result[4] = 1; result[5] = 1; result[6] = 1; break;
        case 1: result[0] = 0; result[1] = 0; result[2] = 0; result[3] = 1; result[4] = 0; result[5] = 0; result[6] = 1; break;
        case 2: result[0] = 1; result[1] = 0; result[2] = 1; result[3] = 1; result[4] = 1; result[5] = 1; result[6] = 0; break;
        case 3: result[0] = 1; result[1] = 0; result[2] = 1; result[3] = 1; result[4] = 0; result[5] = 1; result[6] = 1; break;
        case 4: result[0] = 0; result[1] = 1; result[2] = 1; result[3] = 1; result[4] = 0; result[5] = 0; result[6] = 1; break;
        case 5: result[0] = 1; result[1] = 1; result[2] = 1; result[3] = 0; result[4] = 0; result[5] = 1; result[6] = 1; break;
        case 6: result[0] = 1; result[1] = 1; result[2] = 1; result[3] = 0; result[4] = 1; result[5] = 1; result[6] = 1; break;
        case 7: result[0] = 1; result[1] = 0; result[2] = 0; result[3] = 1; result[4] = 0; result[5] = 0; result[6] = 1; break;
        case 8: result[0] = 1; result[1] = 1; result[2] = 1; result[3] = 1; result[4] = 1; result[5] = 1; result[6] = 1; break;
        case 9: result[0] = 1; result[1] = 1; result[2] = 1; result[3] = 1; result[4] = 0; result[5] = 1; result[6] = 1; break;
        default: printf("Invalid value\n"); return;
    }

    for (int i = 0; i < 7; i++) {
        realClock[pos][i] = result[i];
    }
}

void spaceCol(int norCol) {
    for(int i = 0; i < norCol; i++) {
        printf(" ");
    }
}

void ptClock(int value[7][7], int arr, int norCol) {
    int y, x;

    // First
    spaceCol(norCol-2);
    for (x = 0; x < arr; x++) {
        printf(value[x][0] ? " _ " : "   ");
        if (x % 2 == 1 && x < 5) printf("   ");
    }
    printf("\n");

    // Second
    spaceCol(norCol-2);
    for (x = 0; x < arr; x++) {
        for (y = 1; y < 4; y++) {
            if (y == 1) printf(value[x][y] ? "|" : " ");
            else if (y == 2) printf(value[x][y] ? "_" : " ");
            else if (y == 3) printf(value[x][y] ? "|" : " ");
        }
        if (x % 2 == 1 && x < 5)
            printf(" ○ ");
    }
    printf("\n");

    // Third
    spaceCol(norCol-2);
    for (x = 0; x < arr; x++) {
        for (y = 4; y < 7; y++) {
            if (y == 4) printf(value[x][y] ? "|" : " ");
            else if (y == 5) printf(value[x][y] ? "_" : " ");
            else if (y == 6) printf(value[x][y] ? "|" : " ");
        }
        if (x % 2 == 1 && x < 5) printf(" ○ ");
    }
    printf("\n");
}

void decimizer(int num, int futurePos, int realClock[7][7]) {
    int ten = num / 10;
    int one = num % 10;
    tocode(ten, futurePos, realClock);
    tocode(one, futurePos + 1, realClock);
}

void requestClock(int norCol) {
    time_t currentTime;
    time(&currentTime);

    struct tm *localTime = localtime(&currentTime);
    int hour = (localTime->tm_hour + TIME_ZONE) % 24;
    int minute = localTime->tm_min;
    int second = localTime->tm_sec;

    int realClock[7][7] = {0};
    decimizer(hour, 0, realClock);
    decimizer(minute, 2, realClock);
    decimizer(second, 4, realClock);

    ptClock(realClock, 6, norCol);

    int week = localTime->tm_wday;
    int day = localTime->tm_mday;
    int month = localTime->tm_mon;
    int year = localTime->tm_year + 1900;

    printf("\x1b[35m\n");
    spaceCol(norCol+3);
    printf("%s %02d %s %d\n", weekdays[week], day, months[month], year);
}

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

void show_alarms(int norRow) {
    clear_screen();
    printf("Sveglie impostate:\n");
    for (int i = 0; i < alarm_count; i++) {
        printf("%d. %02d:%02d - %s (Ripetizione: %s, Suono personalizzato: %s, Esegui comando: %s)\n", i + 1, alarms[i].hour, alarms[i].minute, alarms[i].description, alarms[i].repeat ? "Sì" : "No", alarms[i].custom_sound ? "Sì" : "No", alarms[i].execute_payload ? "Sì" : "No");
    }
    for (int i = 0; i + alarm_count < 10; i++) {
        printf("%d. \n", i + 1 + alarm_count);
    }

    //printf("%d - %d",norRow, alarm_count);
    for(int i = 0; i < (norRow*2-13); i++) printf("\n");
    printf("'a' Aggiungi Sveglia     'x' Elimina sveglia     'm' Modifica sveglia     'q' Torna all'orologio");
}

void add_alarm() {
    if (alarm_count >= MAX_ALARMS) {
        printf("Numero massimo di sveglie raggiunto.\n");
        return;
    }

    struct Alarm new_alarm;
    char input[MAX_PATH_LENGTH];

    printf("Inserisci l'ora della sveglia (HH:MM): ");
    fgets(input, sizeof(input), stdin);
    sscanf(input, "%d:%d", &new_alarm.hour, &new_alarm.minute);

    printf("Inserisci la descrizione: ");
    fgets(new_alarm.description, MAX_DESC_LENGTH, stdin);
    new_alarm.description[strcspn(new_alarm.description, "\n")] = 0;

    printf("Si ripete ogni giorno? (s/n): ");
    fgets(input, sizeof(input), stdin);
    new_alarm.repeat = (tolower(input[0]) == 's');

    printf("Vuoi usare un suono personalizzato? (s/n): ");
    fgets(input, sizeof(input), stdin);
    new_alarm.custom_sound = (tolower(input[0]) == 's');

    if (new_alarm.custom_sound) {
        printf("Inserisci il percorso del file audio: ");
        fgets(new_alarm.sound_path, MAX_PATH_LENGTH, stdin);
        new_alarm.sound_path[strcspn(new_alarm.sound_path, "\n")] = 0;
    } else {
        strcpy(new_alarm.sound_path, "cat.mp3");  // Default sound
    }

    printf("Vuoi eseguire un comando al suono della sveglia? (s/n): ");
    fgets(input, sizeof(input), stdin);
    new_alarm.execute_payload = (tolower(input[0]) == 's');

    if (new_alarm.execute_payload) {
        printf("Inserisci il comando da eseguire: ");
        fgets(new_alarm.payload_command, MAX_COMMAND_LENGTH, stdin);
        new_alarm.payload_command[strcspn(new_alarm.payload_command, "\n")] = 0;
    } else {
        new_alarm.payload_command[0] = '\0';
    }

    alarms[alarm_count++] = new_alarm;
    printf("Sveglia aggiunta con successo.\n");
}

void delete_alarm() {
    int index;
    char input[10];

    printf("Inserisci il numero della sveglia da eliminare: ");
    fgets(input, sizeof(input), stdin);
    sscanf(input, "%d", &index);

    if (index < 1 || index > alarm_count) {
        printf("Numero di sveglia non valido.\n");
        return;
    }

    for (int i = index - 1; i < alarm_count - 1; i++) alarms[i] = alarms[i + 1];
    alarm_count--;
    printf("Sveglia eliminata con successo.\n");
}

void modify_alarm() {
    int index;
    char input[10];

    printf("Inserisci il numero della sveglia da modificare: ");
    fgets(input, sizeof(input), stdin);
    sscanf(input, "%d", &index);

    if (index < 1 || index > alarm_count) {
        printf("Numero di sveglia non valido.\n");
        return;
    }

    struct Alarm *alarm = &alarms[index - 1];

    printf("Inserisci la nuova ora della sveglia (HH:MM): ");
    fgets(input, sizeof(input), stdin);
    sscanf(input, "%d:%d", &alarm->hour, &alarm->minute);

    printf("Inserisci la nuova descrizione: ");
    fgets(alarm->description, MAX_DESC_LENGTH, stdin);
    alarm->description[strcspn(alarm->description, "\n")] = 0;

    printf("Si ripete ogni giorno? (s/n): ");
    fgets(input, sizeof(input), stdin);
    alarm->repeat = (tolower(input[0]) == 's');

    printf("Sveglia modificata con successo.\n");
}

void alarm_menu(int norRow) {
    char choice;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    do {
        show_alarms(norRow);
        choice = getchar();

        switch (choice) {
            case 'a': add_alarm(); break;
            case 'x': delete_alarm(); break;
            case 'm': modify_alarm(); break;
            case 'q': break;
            default: printf("Scelta non valida.\n");
        }
    } while (choice != 'q');

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void *alarm_sound(void *arg) {
    struct Alarm *current_alarm = (struct Alarm *)arg;
    int command_executed = 0;  // Flag to track if the command has been executed

    pthread_mutex_lock(&alarm_mutex);
    stop_alarm = 0;
    pthread_mutex_unlock(&alarm_mutex);

    while (1) {
        pthread_mutex_lock(&alarm_mutex);
        if (alarm_ringing && !stop_alarm) {
            pthread_mutex_unlock(&alarm_mutex);

            play_sound(current_alarm->sound_path);

            // Execute the command only once
            if (current_alarm->execute_payload && !command_executed) {
                system(current_alarm->payload_command);
                command_executed = 1;  // Set the flag to indicate the command has been executed
            }

            usleep(500000); // Sleep for 0.5 seconds
        } else {
            pthread_mutex_unlock(&alarm_mutex);
            break;
        }
    }
    return NULL;
}

void check_alarms(struct tm *localTime) {
    for (int i = 0; i < alarm_count; i++) {
        if (alarms[i].hour == localTime->tm_hour && alarms[i].minute == localTime->tm_min && localTime->tm_sec == 0) {
            pthread_mutex_lock(&alarm_mutex);
            alarm_ringing = 1;
            stop_alarm = 0;
            pthread_mutex_unlock(&alarm_mutex);

            pthread_t alarm_thread;
            pthread_create(&alarm_thread, NULL, alarm_sound, &alarms[i]);

            clear_screen();
            printf("\n\n\n\n\n\n\n");
            printf("########################################################\n");
            printf("#                                                      #\n");
            printf("#                       SVEGLIA!                       #\n");
            printf("#                                                      #\n");
            printf("########################################################\n");
            printf("\n%s\n\n", alarms[i].description);

            if (alarms[i].execute_payload) printf("Esecuzione del comando: %s\n\n", alarms[i].payload_command);

            printf("Premi un tasto qualsiasi per fermare la sveglia...\n");

            while (1) {
                if (kbhit()) {
                    getchar(); // Consume the character
                    pthread_mutex_lock(&alarm_mutex);
                    alarm_ringing = 0;
                    stop_alarm = 1;
                    pthread_mutex_unlock(&alarm_mutex);
                    pthread_join(alarm_thread, NULL);
                    break;
                }
                usleep(100000); // Sleep for 0.1 seconds to reduce CPU usage
            }

            if (!alarms[i].repeat) {
                for (int j = i; j < alarm_count - 1; j++) alarms[j] = alarms[j + 1];
                alarm_count--;
                i--;
            }
        }
    }
}

void *clock_loop(void *arg) {
    struct winsize w;
    int norRow, norCol;
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (1) {
        ioctl(0, TIOCGWINSZ, &w);
        clear_screen();

        norRow = w.ws_row / 2;
        if(w.ws_row % 2 == 1) norRow = (w.ws_row - 1) / 2;
        for(int i = 0; i < norRow - 3; i++) printf("\n");

        norCol = w.ws_col / 2;
        if(w.ws_col % 2 == 1) norCol = (w.ws_col - 1) / 2;

        time_t currentTime;
        time(&currentTime);
        struct tm *localTime = localtime(&currentTime);

        // Call the functions to print the clock and check alarms here
        requestClock(norCol - 12);
        for(int i = 0; i < norRow - 3; i++) printf("\n");
        printf("'q' To Allarm Menu");
        check_alarms(localTime);

        if (kbhit()) {
            getchar(); // Consume the character
            alarm_menu(norRow);
        }

        usleep(1000000); // Sleep for 1 second to reduce CPU usage
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return NULL;
}

void save_alarms() {
    FILE *file = fopen(ALARM_FILE, "wb");
    if (file == NULL) {
        perror("Errore nell'apertura del file per salvare le sveglie");
        return;
    }
    fwrite(&alarm_count, sizeof(int), 1, file);
    fwrite(alarms, sizeof(struct Alarm), alarm_count, file);
    fclose(file);
}

void load_alarms() {
    FILE *file = fopen(ALARM_FILE, "rb");
    if (file == NULL) {
        // Se il file non esiste, non è un errore, inizializziamo semplicemente con zero allarmi
        alarm_count = 0;
        return;
    }
    fread(&alarm_count, sizeof(int), 1, file);
    fread(alarms, sizeof(struct Alarm), alarm_count, file);
    fclose(file);
}

int main() {
    load_alarms();

    pthread_t clock_thread;
    pthread_create(&clock_thread, NULL, clock_loop, NULL);
    pthread_join(clock_thread, NULL);

    save_alarms();
    return 0;
}

//1
