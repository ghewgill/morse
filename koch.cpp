#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
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

int WPM_chars = 20;
int WPM_total = 15;
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
    snprintf(cmd, sizeof(cmd), BIN_MORSE" -c%d -w%d %s", WPM_chars, WPM_total, text);
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "CreateProcess failed: %d\n", GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    process = pi.hProcess;
#else
    process = fork();
    if (process == 0) {
        char wpm_chars[10];
        snprintf(wpm_chars, sizeof(wpm_chars), "-w%d\n", WPM_chars);
        char wpm_total[10];
        snprintf(wpm_total, sizeof(wpm_total), "-w%d\n", WPM_total);
        execl(BIN_MORSE, BIN_MORSE, wpm_chars, wpm_total, text, NULL);
        fprintf(stderr, "execl failed: %d\n", errno);
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
    int a = 1;
    while (a < argc && argv[a][0] == '-') {
        switch (argv[a][1]) {
        case 'c':
            if (argv[a][2]) {
                WPM_chars = atoi(argv[a]+2);
            } else {
                a++;
                WPM_chars = atoi(argv[a]);
            }
            break;
        case 'w':
            if (argv[a][2]) {
                WPM_total = atoi(argv[a]+2);
            } else {
                a++;
                WPM_total = atoi(argv[a]);
            }
            break;
        default:
            fprintf(stderr, "%s: invalid option %s\n", argv[0], argv[a]);
            exit(1);
        }
        a++;
    }
    if (WPM_total > WPM_chars) {
        WPM_chars = WPM_total;
    }
    if (a < argc) {
        Level = atoi(argv[a]);
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
        for (int i = 0; i < WPM_total*5; i++) {
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
        time_t start = time(0);
        char user[1000];
        if (fgets(user, sizeof(user), stdin) == NULL) {
            break;
        }
        time_t end = time(0);
        printf("%s\n", words);
        printf("%d seconds\n", end-start);
        int score = match(words, user);
        printf("%d%%\n", score);
        if (score >= 90) {
            Level++;
        }
    }
    return 0;
}
