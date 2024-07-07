#include<stdio.h>
#include<libavutil/log.h>
#include<libavutil/avutil.h>
#include<libavformat/avformat.h>
#include<libavcodec/avcodec.h>
#include<libswscale/swscale.h>

#define WORD uint16_t
#define DWORD uint32_t
#define LONG int32_t

static void savePic(unsigned char *buf, int linesize, int width, int height, char *name){
    FILE *f;
    f = fopen(name, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", width, height, 255);
    for( int i=0 ; i<height ; i++ ){
        fwrite(buf + i * linesize, 1, width, f);
    }
    fclose(f);
}

// 定义 BITMAPFILEHEADER 结构体
typedef struct tagBITMAPFILEHEADER {
  WORD  bfType;          // 文件类型标识，通常为 'BM'
  DWORD bfSize;          // 文件大小，以字节为单位
  WORD  bfReserved1;     // 保留字段，通常为 0
  WORD  bfReserved2;     // 保留字段，通常为 0
  DWORD bfOffBits;       // 位图数据的偏移量，即位图数据相对于文件起始位置的偏移量
} BITMAPFILEHEADER;


// 定义 BITMAPINFOHEADER 结构体
typedef struct tagBITMAPINFOHEADER {
    uint32_t biSize;          // 结构体的大小，通常为 40 字节
    int32_t  biWidth;         // 图像的宽度，以像素为单位
    int32_t  biHeight;        // 图像的高度，以像素为单位，如果为正数，图像是底部向上的，为负数则为顶部向下的
    uint16_t biPlanes;        // 目标设备的位平面数，总是为 1
    uint16_t biBitCount;      // 每个像素的位数，表示颜色深度
    uint32_t biCompression;   // 图像的压缩类型，常见的值有 BI_RGB 表示不压缩的 RGB 格式
    uint32_t biSizeImage;     // 图像的大小，以字节为单位，通常可设置为 0
    int32_t  biXPelsPerMeter; // 图像水平分辨率，单位为像素每米，通常可设置为 0
    int32_t  biYPelsPerMeter; // 图像垂直分辨率，单位为像素每米，通常可设置为 0
    uint32_t biClrUsed;       // 实际使用的颜色表中的颜色索引数，通常为 0
    uint32_t biClrImportant;  // 对图像显示有重要影响的颜色索引数，通常为 0
} BITMAPINFOHEADER;

static void saveBMP(struct SwsContext *swsCtx, AVFrame *frame, int width, int height, char *name){
    // 1.先进性转换， 将YUV frame转成BGR24 Frame
    AVFrame *frameBGR = av_frame_alloc();
    frameBGR->width = width;
    frameBGR->height = height;
    frameBGR->format = AV_PIX_FMT_BGR24;

    av_frame_get_buffer(frameBGR, 0);

    sws_scale(swsCtx, (const uint8_t * const *)frame->data, frame->linesize, 0, frame->height, frameBGR->data, frameBGR->linesize);
    
    // 2.BITMAPINFOHEADER
    BITMAPINFOHEADER infoHeader;
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width ;
    infoHeader.biHeight = height * (-1);  // 因为坐标正好与我们正常坐标相反
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = 0;
    infoHeader.biClrImportant = 0;
    infoHeader.biClrUsed = 0;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biPlanes = 0;

    // 3.BITMAPFILEHEADER
    int datasize = width * height * 3;

    BITMAPFILEHEADER fileHeader;
    fileHeader.bfType = 0x4d42 ;   // "BM"
    fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + datasize;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    // 4.将数据写到文件中
    FILE  *f = NULL;
    f = fopen(name, "wb");
    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, f);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, f);
    fwrite(frame->data[0], 1, datasize, f);
    
    // 5.释放资源
    fclose(f);
    av_freep(&frameBGR->data[0]);
    av_free(frameBGR);


}

static int decode(AVCodecContext *ctx, struct SwsContext *swsCtx, AVFrame *frame, AVPacket *pkt, char *out){

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
        /*
        savePic(frame->data[0],
                frame->linesize[0],
                frame->width,
                frame->height,
                buf);
        */
        saveBMP(swsCtx, frame, frame->width, frame->height, buf);
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

    // 6.1获得SWS上下文
    swsCtx = sws_getContext(ctx->width, ctx->height, AV_PIX_FMT_YUV420P,              // src的  
                        640, 360, AV_PIX_FMT_BGR24,          // dst的
                        SWS_BICUBIC, NULL, NULL, NULL);

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
            decode(ctx, swsCtx, frame, pkt, dst);
        }
    }
    decode(ctx, swsCtx, frame, NULL, dst);

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