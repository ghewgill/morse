#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <sys/soundcard.h>

const int WPM_chars = 10;
const int WPM_total = 5;
const int FREQ = 500;

int SPC_chars;
int SPC_total;
short *buf_silent;
short buf_signal[20000];

struct {
    char c;
    char code[8];
} CW[] = {
    {'A', ".-"},
    {'B', "-..."},
    {'C', "-.-."},
    {'D', "-.."},
    {'E', "."},
    {'F', "..-."},
    {'G', "--."},
    {'H', "...."},
    {'I', ".."},
    {'J', ".---"},
    {'K', "-.-"},
    {'L', ".-.."},
    {'M', "--"},
    {'N', "-."},
    {'O', "---"},
    {'P', ".--."},
    {'Q', "--.-"},
    {'R', ".-."},
    {'S', "..."},
    {'T', "-"},
    {'U', "..-"},
    {'V', "...-"},
    {'W', ".--"},
    {'X', "-..-"},
    {'Y', "-.--"},
    {'Z', "--.."},
    {'Á', ".--.-"},
    {'Ä', ".-.-"},
    {'É', "..-.."},
    {'Ñ', "--.--"},
    {'Ö', "---."},
    {'Ü', "..--"},
    {'1', ".----"},
    {'2', "..---"},
    {'3', "...--"},
    {'4', "....-"},
    {'5', "....."},
    {'6', "-...."},
    {'7', "--..."},
    {'8', "---.."},
    {'9', "----."},
    {'0', "-----"},
    {',', "--..--"},
    {'.', ".-.-.-"},
    {'?', "..--.."},
    {';', "-.-.-"},
    {':', "---..."},
    {'/', "-..-."},
    {'-', "-....-"},
    {'\'',".----."},
    {'(', "-.--.-"},
    {')', "-.--.-"},
    {'_', "..--.-"},
};

const char *getcode(char c)
{
    for (int i = 0; i < sizeof(CW)/sizeof(CW[0]); i++) {
        if (c == CW[i].c) {
            return CW[i].code;
        }
    }
    return NULL;
}

void pause(int fd, int w)
{
    while (w--) {
        write(fd, buf_silent, SPC_total*sizeof(short));
    }
}

void tone(int fd, int w)
{
    write(fd, buf_signal, w*SPC_chars*sizeof(short));
    write(fd, buf_silent, SPC_chars*sizeof(short));
}

int main(int argc, char *argv[])
{
    int fd = open("/dev/dsp", O_WRONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    int sndparam = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &sndparam) == -1) { 
        perror("ioctl: SNDCTL_DSP_SETFMT");
        exit(1);
    }
    if (sndparam != AFMT_S16_LE) {
        perror("ioctl: SNDCTL_DSP_SETFMT");
        exit(1);
    }
    sndparam = 0;
    if (ioctl(fd, SNDCTL_DSP_STEREO, &sndparam) == -1) {
        perror("ioctl: SNDCTL_DSP_STEREO");
        exit(1);
    }
    if (sndparam != 0) {
        fprintf(stderr, "gen: Error, cannot set the channel number to 0\n");
        exit(1);
    }
    int sample_rate = 22050;
    sndparam = sample_rate;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &sndparam) == -1) {
        perror("ioctl: SNDCTL_DSP_SPEED");
        exit(1);
    }
    if ((10*abs(sndparam-sample_rate)) > sample_rate) {
        perror("ioctl: SNDCTL_DSP_SPEED");
        exit(1);
    }
    if (sndparam != sample_rate) {
        fprintf(stderr, "Warning: Sampling rate is %u, requested %u\n", sndparam, sample_rate);
    }
    SPC_chars = (sample_rate*60)/(WPM_chars*50);
    SPC_total = ((sample_rate*60)/WPM_total - SPC_chars*31) / 19;
    buf_silent = new short[SPC_total];
    memset(buf_silent, 0, SPC_total*sizeof(short));
    for (int i = 0; i < sizeof(buf_signal)/sizeof(short); i++) {
        buf_signal[i] = static_cast<short>(12000*sin(FREQ*2*M_PI*i/sample_rate));
    }
    for (int i = 1; i < argc; i++) {
        for (const char *p = argv[i]; *p != 0; p++) {
            if (*p == ' ') {
                pause(fd, 7);
            } else {
                const char *cw = getcode(toupper(*p));
                if (cw != NULL) {
                    for (const char *c = cw; *c != 0; c++) {
                        if (*c == '.') {
                            tone(fd, 1);
                        } else {
                            tone(fd, 3);
                        }
                    }
                    pause(fd, 3);
                }
            }
        }
        pause(fd, 7);
        printf("%s\n", argv[i]);
    }
    close(fd);
    return 0;
}
