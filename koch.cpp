#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef _WIN32
#include <windows.h>
#define snprintf _snprintf
#define sleep(x) Sleep((x)*1000)
#define for if(0);else for
#endif

#ifdef _WIN32
#define BIN_MORSE "morse.exe"
#else
#define BIN_MORSE "./morse"
#endif

const char Letters[] = "KMRSUAPTLOWI.NJEF0Y,VG5/Q9ZH38B?427C1D6X<BT><SK><AR>";
const int WMAX = 10;

int WPM = 10;
int Level = 2;

double urand()
{
    return (double)rand() / RAND_MAX;
}

class Morse {
public:
    Morse(const char *text);
    ~Morse();
    void stop();
private:
#ifdef _WIN32
    HANDLE process;
#else
    pid_t process;
#endif
};

Morse::Morse(const char *text)
{
#ifdef _WIN32
    char cmd[1000];
    snprintf(cmd, sizeof(cmd), BIN_MORSE" -c20 -w%d %d", WPM, text);
    printf("TODO: exec %s\n", cmd);
    //CreateProcess
#else
    process = fork();
    if (process == 0) {
        char wpm[10];
        snprintf(wpm, sizeof(wpm), "-w%d\n", WPM);
        execl(BIN_MORSE, BIN_MORSE, "-c20", wpm, text, NULL);
        exit(127);
    }
#endif
}

Morse::~Morse()
{
    stop();
#ifdef _WIN32
    CloseHandle(process);
#else
    int status;
    waitpid(process, &status, 0);
#endif
}

void Morse::stop()
{
#ifdef _WIN32
    TerminateProcess(process, 0);
#else
    kill(process, SIGTERM);
#endif
}

int match(const char *good, const char *test)
{
    int n = 0;
    int t = 0;
    while (*good != 0) {
        if (isspace(*good)) {
            while (*test != 0 && !isspace(*test)) {
                test++;
            }
            if (isspace(*test)) {
                test++;
            }
        } else {
            if (toupper(*good) == toupper(*test)) {
                t++;
            }
            if (*test != 0 && !isspace(*test)) {
                test++;
            }
            n++;
        }
        good++;
    }
    return 100*t/n;
}

int main(int argc, char *argv[])
{
    if (argc >= 2) {
        Level = atoi(argv[1]);
    }
    srand(time(0));
    for (;;) {
        printf("Letters: %.*s\n", Level, Letters);
        printf("(press Enter to start)\n");
        if (getchar() == EOF) {
            break;
        }
        char words[1000];
        words[0] = 0;
        for (int i = 0; i < WPM*5; i++) {
            int len = static_cast<int>(urand()*5+2); //5*(1/-log(urand()));
            char word[WMAX];
            for (int j = 0; j < len; j++) {
                word[j] = Letters[static_cast<int>(urand()*Level)];
            }
            word[len] = 0;
            strcat(words, word);
            strcat(words, " ");
        }
        //printf("words: %s\n", words);
        sleep(1);
        Morse morse(words);
        char user[1000];
        if (fgets(user, sizeof(user), stdin) == NULL) {
            break;
        }
        int score = match(words, user);
        printf("%d%%\n", score);
        if (score >= 90) {
            Level++;
        }
    }
    return 0;
}
