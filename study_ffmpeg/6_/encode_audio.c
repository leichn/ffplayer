#include <libavutil/log.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt){
    const enum AVSampleFormat *p = codec->sample_fmts;
    while( *p != AV_SAMPLE_FMT_NONE ){
        if( *p == sample_fmt ){
            return 1;
        } 
        p++;
    }
    return 0;
}


static int select_best_sample_rate(const AVCodec *codec){
    const int *p;
    int best_samplerate = 0;

    if( !codec->supported_samplerates){
        return 44100;
    }
    p = codec->supported_samplerates;
    while( *p ){
        if( !best_samplerate || abs(44100 - *p )<abs(44100 - best_samplerate) ){
            best_samplerate = *p;
        }
        p++;
    }
    return best_samplerate;
}

static int encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *out){
    int ret = -1;


    ret = avcodec_send_frame(ctx, frame);
    if( ret < 0 )
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to send frame to encoder!\n");
        goto _END;
    }

    while( ret >= 0 ){
        ret = avcodec_receive_packet(ctx, pkt);
        if( ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ){
            return 0;
        }
        else if( ret < 0 ){
            return -1;
        } 
        fwrite(pkt->data, 1, pkt->size, out);
        av_packet_unref(pkt);
    }

_END:
    return 0;
}


int main(int argc, char *argv[])
{

    av_log_set_level(AV_LOG_DEBUG);
    // 1.输入参数
    if( argc<3 ){
        av_log(NULL, AV_LOG_ERROR, "arguments must be more than 2\n");
        goto _ERROR;

    }
    char *dst  = NULL;
    char *codecName = NULL;
    dst = argv[1];


    // 2.查找编码器
    const AVCodec *codec = NULL;
    // codec = avcodec_find_encoder_by_name(codecName);
    // avcodec_find_encoder_by_name("libfdk_acc");      // 第三方的
    codec = avcodec_find_encoder(AV_CODEC_ID_AAC);       // ffmpeg内部的
    if( !codec ){
        av_log(NULL, AV_LOG_ERROR, "don't find Codec: %s\n", codecName);
        goto _ERROR;
    }
    
    // 3.创建编码器上下文
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if( !ctx ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMRORY\n");
        goto _ERROR;
    }

    // 4.设置编码器参数

    ctx->bit_rate = 64000; // 一般在64k\128k\之间
    ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    if( !check_sample_fmt(codec, ctx->sample_fmt) )
    {
        av_log(NULL, AV_LOG_ERROR, "Encoder does not support sample format!\n");
        goto _ERROR;
    }

    ctx->sample_rate = select_best_sample_rate(codec);
    av_channel_layout_copy(&ctx->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO);

    // 5.编码器与编码器上下文绑定到一起

    int ret;
    ret = avcodec_open2(ctx, codec, NULL);
    if( ret < 0 ){
        av_log(ctx, AV_LOG_ERROR, "Don't open codec: %s\n", av_err2str(ret));
        goto _ERROR;
    }

    // 6.创建输出文件
    FILE *f = fopen(dst, "wb");
    if( !f ){
        av_log(NULL, AV_LOG_ERROR, "Don't open file: %s\n", dst);
        goto _ERROR;   
    }
    
    // 7.创建AVFrame  （保存原始音频帧
    
    AVFrame *frame = av_frame_alloc();
    if( !frame ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto _ERROR;   
    }

    frame->nb_samples = ctx->frame_size;
    frame->format  = ctx->sample_fmt;
    av_channel_layout_copy(&frame->ch_layout, &ctx->ch_layout);
    ret = av_frame_get_buffer(frame, 0);
    if( ret < 0 ){
        av_log(NULL, AV_LOG_ERROR, "Could not allocate the video frame. \n");
        goto _ERROR;
    }

    // 8.创建AVPacket （保存编码后的音频帧 / 一个packet中可能含有多个

    AVPacket *pkt = av_packet_alloc();
    if( !pkt ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto _ERROR;   
    }

    
    // 9.生成音频内容
    uint16_t *samples = NULL;
    float t = 0 ;
    float tincr = 2 * M_PI * 440 / ctx->sample_rate;
    for( int i=0 ; i<200 ; i++ ){
        ret = av_frame_make_writable(frame);
        if( ret < 0 ){
            av_log(NULL, AV_LOG_ERROR, "Could not allocate space!\n");
            goto _ERROR;
        }
        samples = (uint16_t*)frame->data[0];
        for(int j=0 ; j<ctx->frame_size ; j++ )
        {
            samples[2*j] = (int)(sin(t) * 10000 );   // 因为每个采样大小是两字节
            for( int k=1 ; k<ctx->ch_layout.nb_channels ; k++ ){
                samples[2*j+ k] = samples[2*j];
            }
            t += tincr; 
        }
        encode(ctx, NULL, pkt, f);
    }

    // 10.编码
    encode(ctx, NULL, pkt, f);

_ERROR:
    // ctx 
    if( ctx ){
        avcodec_free_context(&ctx);
    }
    // // avframe
    // if( frame ){
    //     av_frame_free(&frame);
    // }
    // // avpacket
    // if( pkt ){
    //     av_packet_free(&pkt);
    // }
    // // dst
    // if( f ){
    //     fclose(f);
    // } 

    return 0;
}

