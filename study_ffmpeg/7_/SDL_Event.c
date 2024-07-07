#include<SDL.h>
#include<stdio.h>

int main(int argc, char* argv[]) 
{
    int quit = 1;





    SDL_Init(SDL_INIT_VIDEO);


    SDL_Window* window = NULL;
    window = SDL_CreateWindow("SDL2 Window", 
                    200, // 窗口初始位置x 
                    200, // 窗口初始位置y
                    640, // 窗口宽度
                    480, // 窗口高度
                    SDL_WINDOW_SHOWN
                    );
    if (!window) {
        printf("Failed to create window.");
        goto __EXIT;
    }


    SDL_Renderer* render = NULL;
    render = SDL_CreateRenderer(window, -1, 0);
    if (!render) {
        SDL_Log("Failed to create render."); 
        goto __DWINDOW;
    }

/*
    SDL_SetRenderDrawColor(render, 255, 0, 0, 255);

    SDL_RenderClear(render);

    SDL_RenderPresent(render);
    */

    SDL_Texture* texture = NULL;
    texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 600, 480);
    if (!texture) {
        SDL_Log("Failed to create texture!");
        goto __RENDER;
    }



    SDL_Event event;
    SDL_Rect rect;
    rect.w = 30;
    rect.h = 30;
    do {
        // SDL_WaitEvent(&event); 没有事件的时候，代码会阻塞在这里，等待事件。
        SDL_PollEvent(&event);
        switch(event.type) {
        case SDL_QUIT:
            quit = 0;
            break;
        default:
            SDL_Log("event type is: %d", event.type);
        }

        
        rect.x = rand() % 600;
        rect.y = rand() % 480;

        SDL_SetRenderTarget(render, texture);
        SDL_SetRenderDrawColor(render, 0, 0, 0, 0);
        SDL_RenderClear(render);

        SDL_RenderDrawRect(render, &rect);
        SDL_SetRenderDrawColor(render, 255, 0, 0, 0);
        SDL_RenderFillRect(render, &rect);          // 方块是特殊的，不是完整的，不能用SDL_RenderClear

        SDL_SetRenderTarget(render, NULL);          // 恢复成默认的渲染目标
        SDL_RenderCopy(render, texture, NULL, NULL);

        SDL_RenderPresent(render);
    } while(quit);

    SDL_DestroyTexture(texture);

__RENDER:
    SDL_DestroyRenderer(render);

__DWINDOW:
    SDL_DestroyWindow(window);

__EXIT:
    SDL_Quit();
    return 0;
}