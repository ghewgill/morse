#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define snprintf _snprintf
#define sleep(x) Sleep((x)*1000)
#define for if(0);else for
#endif

const char Letters[] = "KMRSUAPTLOWI.NJEF0Y,VG5/Q9ZH38B?427C1D6X<BT><SK><AR>";
const int WMAX = 10;

int Level = 2;

struct {
    int total;
    int correct;
} History[5];
int HistoryCount = 0;

double urand()
{
    return (double)rand() / RAND_MAX;
}

int main(int argc, char *argv[])
{
    if (argc >= 2) {
        Level = atoi(argv[1]);
    }
    int lastminute = 0;
    int total = 0;
    int correct = 0;
    for (;;) {
        sleep(1);
        int len = urand()*5+2; //5*(1/-log(urand()));
        char buf[WMAX];
        for (int i = 0; i < len; i++) {
            buf[i] = Letters[static_cast<int>(urand()*Level)];
        }
        buf[len] = 0;
        char cmd[200];
        snprintf(cmd, sizeof(cmd), "morse -c20 -w15 %s", buf);
        system(cmd);
        char user[WMAX];
        memset(user, 0, sizeof(user));
        if (fgets(user, sizeof(user), stdin) == NULL) {
            break;
        }
        if (user[0] == '*') {
            printf("(reset)\n");
            total = 0;
            correct = 0;
            continue;
        } else if (user[0] == '+') {
            Level++;
            printf("Level %d\n", Level);
            continue;
        }
        //printf("%s\n", buf);
        for (int i = 0; i < len; i++) {
            if (toupper(user[i]) == buf[i]) {
                correct++;
            }
            total++;
        }
        int minute = time(0) / 60;
        if (minute != lastminute) {
            memcpy(History+1, History, sizeof(History)-sizeof(History[0]));
            History[0].total = total;
            History[0].correct = correct;
            if (HistoryCount < sizeof(History)/sizeof(History[0])) {
                HistoryCount++;
            }
            int t = 0;
            int c = 0;
            for (int i = 0; i < HistoryCount; i++) {
                t += History[i].total;
                c += History[i].correct;
            }
            printf("%d%%\n", 100*c/t);
            lastminute = minute;
        }
    }
    return 0;
}
