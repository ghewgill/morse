#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#endif

#ifdef _WIN32
#include <windows.h>
#define for if(0);else for
#endif

#ifndef M_PI
double M_PI = 4*atan(1.0);
#endif

int WPM_chars = 18;
int WPM_total = 5;
int Freq = 750;
bool Verbose = false;
bool Echo = false;
const char *OutputFile = NULL;

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
    {'/', "-..-."},
    {'?', "..--.."},

    {';', "-.-.-"},
    {':', "---..."},
    {'-', "-....-"},
    {'\'',".----."},
    {'(', "-.--.-"},
    {')', "-.--.-"},
    {'_', "..--.-"},
    {'Á', ".--.-"},
    {'Ä', ".-.-"},
    {'É', "..-.."},
    {'Ñ', "--.--"},
    {'Ö', "---."},
    {'Ü', "..--"},
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

class PcmOutputWav: public PcmOutput {
public:
    PcmOutputWav(const char *fn);
    virtual ~PcmOutputWav();
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
    Header header;
    int data_size;
    FILE *f;
};

PcmOutputWav::PcmOutputWav(const char *fn)
{
    data_size = 0;

    strncpy(header.tagRIFF, "RIFF", 4);
    header.riffsize = 0;
    strncpy(header.tagWAVE, "WAVE", 4);
    strncpy(header.tagfmt, "fmt ", 4);
    header.fmtsize = 16;
    header.wFormatTag = 1;
    header.nChannels = 1;
    header.nSamplesPerSec = 22050;
    header.nAvgBytesPerSec = 22050*16/8*1;
    header.nBlockAlign = 16/8;
    header.nBitsPerSample = 16;
    strncpy(header.tagdata, "data", 4);
    header.datasize = 0;

    f = fopen(fn, "wb");
    if (f == NULL) {
        perror("fopen");
        exit(1);
    }
    fwrite(&header, 1, sizeof(header), f);
}

PcmOutputWav::~PcmOutputWav()
{
    flush();
    fclose(f);
}

void PcmOutputWav::output(const short *buf, int n)
{
    fwrite(buf, sizeof(short), n, f);
    data_size += n*sizeof(short);
}

void PcmOutputWav::flush()
{
    header.riffsize = 36+data_size;
    header.datasize = data_size;
    fseek(f, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(header), f);
    fseek(f, 0, SEEK_END);
}

#ifdef _WIN32

class PcmOutputWin32: public PcmOutput {
public:
    PcmOutputWin32();
    virtual ~PcmOutputWin32();
    virtual int getSampleRate() { return 22050; }
    virtual void output(const short *buf, int n);
    virtual void flush();
private:
    HWAVEOUT wo;
    struct Buffer {
        bool prepared;
        WAVEHDR hdr;
        enum {SIZE = 16384};
        short data[SIZE];
        int index;
    };
    enum {NBUFS = 4};
    Buffer buffers[NBUFS];
    int bufindex;
    int buftail;
    void wait();
};

PcmOutputWin32::PcmOutputWin32()
{
    WAVEFORMATEX wf;
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = 22050;
    wf.nAvgBytesPerSec = 22050*16/8*1;
    wf.nBlockAlign = 16/8;
    wf.wBitsPerSample = 16;
    wf.cbSize = 0;
    MMRESULT r = waveOutOpen(&wo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) {
        fprintf(stderr, "could not open wave device\n");
        exit(1);
    }
    for (int i = 0; i < NBUFS; i++) {
        buffers[i].prepared = false;
        buffers[i].hdr.lpData = (char *)buffers[i].data;
        buffers[i].hdr.dwBufferLength = Buffer::SIZE*sizeof(short);
        buffers[i].hdr.dwFlags = 0;
        buffers[i].index = 0;
    }
    bufindex = 0;
    buftail = 0;
}

PcmOutputWin32::~PcmOutputWin32()
{
    flush();
    waveOutReset(wo);
    waveOutClose(wo);
}

void PcmOutputWin32::output(const short *buf, int n)
{
    while (n > 0) {
        if (!buffers[bufindex].prepared) {
            buffers[bufindex].hdr.dwBufferLength = Buffer::SIZE*sizeof(short);
            buffers[bufindex].hdr.dwFlags = 0;
            MMRESULT r = waveOutPrepareHeader(wo, &buffers[bufindex].hdr, sizeof(WAVEHDR));
            if (r != MMSYSERR_NOERROR) {
                fprintf(stderr, "error preparing header\n");
                return;
            }
            buffers[bufindex].index = 0;
            buffers[bufindex].prepared = true;
        }
        int c = Buffer::SIZE-buffers[bufindex].index;
        if (c > n) {
            c = n;
        }
        memcpy(buffers[bufindex].data+buffers[bufindex].index, buf, c*sizeof(short));
        buffers[bufindex].index += c;
        buf += c;
        n -= c;
        if (buffers[bufindex].index >= Buffer::SIZE) {
            MMRESULT r = waveOutWrite(wo, &buffers[bufindex].hdr, sizeof(WAVEHDR));
            if (r != MMSYSERR_NOERROR) {
                fprintf(stderr, "error on waveOutWrite: %d\n", r);
                return;
            }
            int newbufindex = (bufindex + 1) % NBUFS;
            if (newbufindex == buftail) {
                wait();
            }
            bufindex = newbufindex;
            assert(bufindex != buftail);
        }
    }
}

void PcmOutputWin32::flush()
{
    if (buffers[bufindex].prepared && buffers[bufindex].index > 0) {
        buffers[bufindex].hdr.dwBufferLength = buffers[bufindex].index*sizeof(short);
        MMRESULT r = waveOutWrite(wo, &buffers[bufindex].hdr, sizeof(WAVEHDR));
        if (r != MMSYSERR_NOERROR) {
            fprintf(stderr, "error on waveOutWrite: %d\n", r);
            return;
        }
        int newbufindex = (bufindex + 1) % NBUFS;
        if (newbufindex == buftail) {
            wait();
        }
        bufindex = newbufindex;
        assert(bufindex != buftail);
    }
    while (buftail != bufindex) {
        wait();
    }
}

void PcmOutputWin32::wait()
{
    if (buftail == bufindex) {
        return;
    }
    while ((buffers[buftail].hdr.dwFlags & WHDR_DONE) == 0) {
        Sleep(100);
    }
    MMRESULT r = waveOutUnprepareHeader(wo, &buffers[buftail].hdr, sizeof(WAVEHDR));
    if (r != MMSYSERR_NOERROR) {
        fprintf(stderr, "error on waveOutUnprepareHeader: %d\n", r);
        return;
    }
    buffers[buftail].prepared = false;
    buftail = (buftail + 1) % NBUFS;
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
    const int RAMP = 110;
    short ramp[RAMP];
    memcpy(ramp, buf_signal, sizeof(ramp));
    for (int i = 0; i < RAMP; i++) {
        ramp[i] = ramp[i]*i/RAMP;
    }
    pcm->output(ramp, RAMP);
    pcm->output(buf_signal+RAMP, w*SPC_chars-2*RAMP);
    memcpy(ramp, buf_signal+w*SPC_chars-RAMP, sizeof(ramp));
    for (int i = 0; i < RAMP; i++) {
        ramp[i] = ramp[i]*(RAMP-i-1)/RAMP;
    }
    pcm->output(ramp, RAMP);
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
        case 'o':
            if (argv[a][2]) {
                OutputFile = &argv[a][2];
            } else {
                a++;
                OutputFile = argv[a];
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
    if (WPM_total > WPM_chars) {
        WPM_chars = WPM_total;
    }
    if (WPM_chars == 0 || WPM_total == 0) {
        fprintf(stderr, "%s: Invalid wpm parameter\n", argv[0]);
        exit(1);
    }
    if (Verbose) {
        fprintf(stderr, "%d WPM (%d WPM chars)\n", WPM_total, WPM_chars);
    }
    if (OutputFile) {
        pcm = new PcmOutputWav(OutputFile);
    } else {
#if defined(unix)
        pcm = new PcmOutputUnix("/dev/dsp");
#elif defined(_WIN32)
        pcm = new PcmOutputWin32();
#else
        #error unsupported platform
#endif
    }
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
