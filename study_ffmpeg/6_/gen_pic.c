#include<stdio.h>
#include<libavutil/log.h>
#include<libavutil/avutil.h>
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>

static void savePic(unsigned char *buf, int linesize, int width, int height, char *name){
    FILE *f;
    f = fopen(name, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", width, height, 255);
    for( int i=0 ; i<height ; i++ ){
        fwrite(buf + i * linesize, 1, width, f);
    }
    fclose(f);
}

static int decode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, char *out){
    int ret = -1;


    ret = avcodec_send_packet(ctx, pkt);
    if( ret < 0 )
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to send frame to decoder!\n");
        goto _END;
    }

    char buf[1024];
    while( ret >= 0 ){
        ret = avcodec_receive_frame(ctx, frame);
        if( ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ){
            return 0;
        }
        else if( ret < 0 ){
            return -1;
        } 
        snprintf(buf, sizeof(buf), "%s-%ld", out, ctx->frame_num);
        savePic(frame->data[0],
                frame->linesize[0],
                frame->width,
                frame->height,
                buf
        );
        if( pkt ){
           av_packet_unref(pkt);
        }
    }

_END:
    return 0;
}

int main(int argc, char* argv[])
{
    // 1.处理一些参数；
    int ret = -1;
    char* src;
    char* dst;

    struct SwsContext *swsCtx = NULL;

    av_log_set_level(AV_LOG_DEBUG);
    if( argc < 3 ) {
        av_log(NULL, AV_LOG_INFO, "arguments must be more than 3\n");
        exit(-1);
    }

    src = argv[1];
    dst = argv[2];

    // 2.打开多媒体文件；

    AVFormatContext *pFmtCtx = NULL;
    if( (ret = avformat_open_input(&pFmtCtx, src, NULL, NULL)) < 0 ){
        av_log(NULL, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        exit(-1);
    }

    // 3.从多媒体文件中找到视频流
    
    int idx = -1;
    idx = av_find_best_stream(pFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if( idx < 0 ){
        av_log(pFmtCtx, AV_LOG_ERROR, "Dose not include audio stream!\n");
        goto _ERROR;
    }

    AVStream *inStream = NULL;
    inStream = pFmtCtx->streams[idx];

    // 4.查找解码器
    const AVCodec *codec = NULL;
    codec = avcodec_find_decoder(inStream->codecpar->codec_id);
    if( !codec ){
        av_log(NULL, AV_LOG_ERROR, "Could not find libx264 Codec\n");
        goto _ERROR;
    }
    
    // 5.创建解码器上下文
    AVCodecContext *ctx = NULL;
    ctx = avcodec_alloc_context3(codec);
    if( !ctx ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMRORY\n");
        goto _ERROR;
    }
    avcodec_parameters_to_context(ctx, inStream->codecpar);


    // 6.解码器与解码器上下文绑定到一起
    ret = avcodec_open2(ctx, codec, NULL);
    if( ret < 0 ){
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s\n", av_err2str(ret));
        goto _ERROR;
    }

    // 7.创建AVFrame  （保存原始音频帧
    
    AVFrame *frame = NULL;
    frame = av_frame_alloc();
    if( !frame ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto _ERROR;   
    }

    // 8.创建AVPacket （保存编码后的音频帧 / 一个packet中可能含有多个

    AVPacket *pkt = NULL;
    pkt = av_packet_alloc();
    if( !pkt ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto _ERROR;   
    }

    // 9.从源多媒体文件中读到视频数据到目的文件中

    while( av_read_frame(pFmtCtx, pkt) >= 0) {
        if(  pkt->stream_index == idx ){
            decode(ctx, frame, pkt, dst);
        }
    }
    decode(ctx, frame, NULL, dst);

    // 10.将申请的资源释放掉
_ERROR:
    if( pFmtCtx ){  
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
    }
    if( ctx ){
        avcodec_free_context(&ctx);
        ctx = NULL;
    }
    if( frame ){
        av_frame_free(&frame);
        frame = NULL;
    }
    if( pkt ){
        av_packet_free(&pkt);
        pkt = NULL;
    }
    return 0;
}