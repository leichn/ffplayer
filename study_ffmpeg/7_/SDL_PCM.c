#include<SDL.h>
#include<stdio.h>

#define BLOCK_SIZE 4096000

static size_t buffer_len = 0;
static Uint8* audio_buf = NULL;
static Uint8* audio_pos = NULL;

// udata 是 spec.userdata。 stream 是声卡的缓冲区指针。len 是声卡缓冲区的长度。
void read_audio_data(void* udata, Uint8* stream, int len)
{
    if (0 == buffer_len) {
        return;
    }

    SDL_memset(stream, 0, len);

    len = (len < buffer_len) ? len : buffer_len;
    SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);

    audio_pos += len;
    buffer_len -= len;
}

int main(int argc, char* argv[]) 
{
    int ret = -1;
    char* path = "./1.pcm";
    FILE* audio_fd = NULL;

    if (SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("Failed to initial!");
        return ret;
    }

    audio_fd = fopen(path, "r");
    if (!audio_fd) {
        SDL_Log("Failed to open pcm file!");
        goto __FAIL;
    }

    audio_buf = (Uint8*)malloc(BLOCK_SIZE);
    if (!audio_buf) {
        SDL_Log("Failed to alloc memory");
        goto __FAIL;
    }
    
    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.channels = 2;
    spec.format = AUDIO_S16SYS;
    spec.silence = 0;
    spec.callback = read_audio_data;
    spec.userdata = NULL;

    if(SDL_OpenAudio(&spec, NULL)) {
        SDL_Log("Failed to open audio device!");
        goto __FAIL;
    }

    SDL_PauseAudio(0);

    do {
        buffer_len = fread(audio_buf, 1, BLOCK_SIZE, audio_fd);
        audio_pos = audio_buf;
        while (audio_pos < (audio_buf + buffer_len)) {
            SDL_Delay(1);
        }
    } while(buffer_len != 0);

    SDL_CloseAudio();

    ret = 0;

__FAIL:
    if (audio_buf) {
        free(audio_buf);
    }

    if (audio_fd) {
        fclose(audio_fd);
    }

    SDL_Quit();

    return 0;
}