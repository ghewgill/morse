#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef M_PI
double M_PI = 4*atan(1);
#endif

int WPM_chars = 5;
int WPM_total = 5;
int Freq = 750;
bool Verbose = false;
bool Echo = false;

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

class PcmOutput {
public:
    virtual ~PcmOutput() {}
    virtual int getSampleRate() = 0;
    virtual void output(const short *buf, int n) = 0;
    virtual void flush() {}
};

#ifdef unix

class PcmOutputUnix: public PcmOutput {
public:
    PcmOutputUnix(const char *dev);
    virtual ~PcmOutputUnix();
    virtual int getSampleRate();
    virtual void output(const short *buf, int n);
private:
    int fd;
    int sample_rate;
};

PcmOutputUnix::PcmOutputUnix(const char *dev)
{
    fd = open(dev, O_WRONLY);
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
    sample_rate = 22050;
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
}

PcmOutputUnix::~PcmOutputUnix()
{
    close(fd);
}

int PcmOutputUnix::getSampleRate()
{
    return sample_rate;
}

void PcmOutputUnix::output(const short *buf, int n)
{
    write(fd, buf, n*sizeof(short));
}

#endif // unix

#ifdef _WIN32

class PcmOutputWin32: public PcmOutput {
public:
    PcmOutputWin32();
    virtual ~PcmOutputWin32();
    virtual int getSampleRate() { return 22050; }
    virtual void output(const short *buf, int n);
    virtual void flush();
private:
    struct Header {
        char tagRIFF[4];
        unsigned long riffsize;
        char tagWAVE[4];
        char tagfmt[4];
        unsigned long fmtsize;
        unsigned short wFormatTag;
        unsigned short nChannels;
        unsigned long nSamplesPerSec;
        unsigned long nAvgBytesPerSec;
        unsigned short nBlockAlign;
        unsigned short nBitsPerSample;
        char tagdata[4];
        unsigned long datasize;
    };
    Header *wav;
    int data_size;
    int alloc_size;
};

PcmOutputWin32::PcmOutputWin32()
{
    data_size = 0;
    alloc_size = 65536;
    wav = (Header *)malloc(sizeof(Header)+alloc_size);

    strncpy(wav->tagRIFF, "RIFF", 4);
    wav->riffsize = 0;
    strncpy(wav->tagWAVE, "WAVE", 4);
    strncpy(wav->tagfmt, "fmt ", 4);
    wav->fmtsize = 16;
    wav->wFormatTag = 1;
    wav->nChannels = 1;
    wav->nSamplesPerSec = 22050;
    wav->nAvgBytesPerSec = 22050*16/8*1;
    wav->nBlockAlign = 16/8;
    wav->nBitsPerSample = 16;
    strncpy(wav->tagdata, "data", 4);
    wav->datasize = 0;
}

PcmOutputWin32::~PcmOutputWin32()
{
    flush();
}

void PcmOutputWin32::output(const short *buf, int n)
{
    while (data_size+n*sizeof(short) > alloc_size) {
        alloc_size *= 2;
        wav = (Header *)realloc(wav, sizeof(Header)+alloc_size);
    }
    memcpy((BYTE *)wav+sizeof(Header)+data_size, buf, n*sizeof(short));
    data_size += n*sizeof(short);
}

void PcmOutputWin32::flush()
{
    if (data_size == 0) {
        return;
    }
    wav->riffsize = 36+data_size;
    wav->datasize = data_size;
    PlaySound((const char *)wav, NULL, SND_MEMORY|SND_SYNC);
    data_size = 0;
}

#endif // _WIN32

PcmOutput *pcm;

void pause(int w)
{
    while (w--) {
        pcm->output(buf_silent, SPC_total);
    }
}

void tone(int w)
{
    pcm->output(buf_signal, w*SPC_chars);
    pcm->output(buf_silent, SPC_chars);
}

void morse(const char *word)
{
    for (const char *p = word; *p != 0; p++) {
        if (*p == ' ') {
            pause(7);
        } else {
            const char *cw = getcode(toupper(*p));
            if (cw != NULL) {
                for (const char *c = cw; *c != 0; c++) {
                    if (*c == '.') {
                        tone(1);
                    } else {
                        tone(3);
                    }
                }
                pause(3);
            }
        }
    }
    pause(7);
    pcm->flush();
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
        case 'e':
            Echo = true;
            break;
        case 'f':
            if (argv[a][2]) {
                Freq = atoi(argv[a]+2);
            } else {
                a++;
                Freq = atoi(argv[a]);
            }
            break;
        case 'v':
            Verbose = true;
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
    if (WPM_chars == 0 || WPM_total == 0 || WPM_total > WPM_chars) {
        fprintf(stderr, "%s: Invalid wpm parameter\n", argv[0]);
        exit(1);
    }
    if (Verbose) {
        fprintf(stderr, "%d WPM (%d WPM chars)\n", WPM_total, WPM_chars);
    }
#if defined(unix)
    pcm = new PcmOutputUnix("/dev/dsp");
#elif defined(_WIN32)
    pcm = new PcmOutputWin32();
#else
    #error unsupported platform
#endif
    int sample_rate = pcm->getSampleRate();
    SPC_chars = (sample_rate*60)/(WPM_chars*50);
    SPC_total = ((sample_rate*60)/WPM_total - SPC_chars*31) / 19;
    buf_silent = new short[SPC_total];
    memset(buf_silent, 0, SPC_total*sizeof(short));
    for (int i = 0; i < sizeof(buf_signal)/sizeof(short); i++) {
        buf_signal[i] = static_cast<short>(16000*sin(Freq*2*M_PI*i/sample_rate));
    }
    if (a < argc) {
        while (a < argc) {
            morse(argv[a]);
            if (Echo) {
                printf("%s\n", argv[a]);
            }
            a++;
        }
    } else {
        char buf[1024];
        while (fgets(buf, sizeof(buf), stdin)) {
            morse(buf);
            if (Echo) {
                fputs(buf, stdout);
            }
        }
    }
    delete pcm;
    return 0;
}
