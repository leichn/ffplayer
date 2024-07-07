#include <stdio.h>
#include <libavutil/log.h>
int main(int argc, char* argv[]){
    printf("\n__())()___\n");
    av_log_set_level(AV_LOG_DEBUG);
    av_log(NULL, AV_LOG_INFO,"hello world!\n");
    return 0;
}