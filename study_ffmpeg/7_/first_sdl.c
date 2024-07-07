#include<SDL.h>
#include<stdio.h>

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;

    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow("SDL2 Window", 200, 200, 640, 480, SDL_WINDOW_SHOWN);

    if( !window ){
        printf("Failed to Create window!\n");
        goto __EXIT;
    }

    SDL_Renderer *render = NULL;
    render = SDL_CreateRenderer(window, -1, 0); 

    if( !render ){
        SDL_Log("Failed ot Create Render!");
        goto __DWINDOWN;
    }

    SDL_SetRenderDrawColor(render, 255, 0, 0, 255);

    SDL_RenderClear(render);

    SDL_RenderPresent(render);

    SDL_Delay(30000);

__DWINDOWN:
    SDL_DestroyWindow(window);

__EXIT:
    SDL_Quit();
    return 0;
}
