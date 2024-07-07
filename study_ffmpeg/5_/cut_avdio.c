#include<stdio.h>
#include<stdlib.h>
#include<libavutil/log.h>
#include<libavutil/avutil.h>
#include<libavformat/avformat.h>

int main(int argc, char* argv[])
{
    // 1.处理一些参数；
    int ret = -1;
    char* src;
    char* dst;
    double starttime;
    double endtime;

    av_log_set_level(AV_LOG_DEBUG);
    // cut src dst start end
    if( argc < 5 ) {
        av_log(NULL, AV_LOG_INFO, "arguments must be more than 3\n");
        exit(-1);
    }

    src = argv[1];
    dst = argv[2];
    starttime = atof(argv[3]);
    endtime = atof(argv[4]);


    // 2.打开多媒体文件；

    AVFormatContext *pFmtCtx = NULL;
    if( (ret = avformat_open_input(&pFmtCtx, src, NULL, NULL)) < 0 ){
        av_log(NULL, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        exit(-1);
    }

    // 4.打开目的文件的上下文
    // AVFormatContext *oFmtCtx = avformat_alloc_context();
    // if( !oFmtCtx ){
    //     av_log(NULL, AV_LOG_ERROR, "NO Memory!\n");
    //     goto _ERROR;
    // } 
    // const AVOutputFormat *outFmt = av_guess_format(NULL, dst, NULL);
    // oFmtCtx->oformat = outFmt;
    AVFormatContext *oFmtCtx = NULL;
    avformat_alloc_output_context2(&oFmtCtx, NULL, NULL, dst);
    if( !oFmtCtx ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORT!\n");
        goto _ERROR;
    } 
    
    int *stream_map = NULL;
    int stream_idx = 0;
    stream_map = av_calloc(pFmtCtx->nb_streams, sizeof(int));
    if( !stream_map ){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORT!\n");
        goto _ERROR;
    }
    for( int i=0 ; i < pFmtCtx->nb_streams ; i++ ){
        AVStream *inStream = pFmtCtx->streams[i];
        AVStream *outStream = NULL;
        AVCodecParameters *inCodecPar = inStream->codecpar;
        if( inCodecPar->codec_type != AVMEDIA_TYPE_AUDIO && 
            inCodecPar->codec_type != AVMEDIA_TYPE_VIDEO && 
            inCodecPar->codec_type != AVMEDIA_TYPE_SUBTITLE){
            stream_map[i] = -1;
            continue;
        }
        else{
            stream_map[i] = stream_idx++ ;
            outStream = avformat_new_stream(oFmtCtx, NULL);
            if( !outStream ){
                av_log(oFmtCtx, AV_LOG_ERROR, "NO MEMORT!\n");
                goto _ERROR;
            }
            avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
            outStream->codecpar->codec_tag = 0;
        }
    }


    // 绑定
    ret = avio_open2(&oFmtCtx->pb, dst, AVIO_FLAG_WRITE, NULL, NULL);
    if( ret < 0 ){
        av_log(oFmtCtx, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        goto _ERROR;
    }

    // 7.写多媒体文件头到目的文件

    ret = avformat_write_header(oFmtCtx, NULL);
    if( ret < 0 ){
        av_log(oFmtCtx, AV_LOG_ERROR, "%s\n", av_err2str(ret));
        goto _ERROR;
    }


    // cut起作用的地方
    // 很多视频的裁剪不准确，是因为视频有IPB帧，我们很有可能裁剪到了B帧(没有I帧无法解析)，那么就必须向前或者向后来裁剪
    // 这个函数表示，从流的哪个时间点截取文件
    ret = av_seek_frame(pFmtCtx, -1, starttime * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    
    if( ret < 0 ){
        av_log(oFmtCtx, AV_LOG_ERROR, "%s", av_err2str(ret));
        goto _ERROR;
    }


    int64_t *dts_start_time = av_calloc(pFmtCtx->nb_streams, sizeof(int64_t));
    int64_t *pts_start_time = av_calloc(pFmtCtx->nb_streams, sizeof(int64_t));
    for( int i = 0 ; i < pFmtCtx->nb_streams ; i++ ){
        dts_start_time[i] = -1;
        pts_start_time[i] = -1;
    }
    // 8.从源多媒体文件中读到音频/视频/字幕数据到目的文件中

// 0   1   2   3   4   5 
// -1  1  -1   1  -1   1

// 0   1   2   3   4   5
// -1  0  -1   1  -1   2

    AVPacket pkt;
    while( av_read_frame(pFmtCtx, &pkt) >= 0) {
        AVStream *inStream, *outStream;

        if( dts_start_time[pkt.stream_index] == -1 && pkt.dts > 0 ){
            dts_start_time[pkt.stream_index] = pkt.dts;
        }

        if( pts_start_time[pkt.stream_index] == -1 && pkt.pts > 0 ){
            pts_start_time[pkt.stream_index] = pkt.pts;
        }

        inStream = pFmtCtx->streams[pkt.stream_index];
        if( av_q2d(inStream->time_base) * pkt.pts > endtime ){
            av_log(oFmtCtx, AV_LOG_INFO, "success!\n");
            break;
        }

        if( stream_map[pkt.stream_index] < 0 ){
            av_packet_unref(&pkt);
            continue;
        }

        pkt.pts = pkt.pts - pts_start_time[pkt.stream_index];
        pkt.dts = pkt.dts - dts_start_time[pkt.stream_index];
        if( pkt.pts > pkt.dts ){
            pkt.pts = pkt.dts;
        }  

        pkt.stream_index = stream_map[pkt.stream_index];
        
        outStream = oFmtCtx->streams[pkt.stream_index];
        av_packet_rescale_ts(&pkt, inStream->time_base, outStream->time_base);
        // pkt.pts = av_rescale_q_rnd(pkt.pts, inStream->time_base, outStream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        // pkt.dts = av_rescale_q_rnd(pkt.dts, inStream->time_base, outStream->time_base, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        // pkt.duration = av_rescale_q(pkt.duration, inStream->time_base, outStream->time_base);
        // pkt.stream_index = 0;
        pkt.pos = -1;
        av_interleaved_write_frame(oFmtCtx, &pkt);
        av_packet_unref(&pkt);
    }

    // 9.写多媒体文件尾到文件中

    av_write_trailer(oFmtCtx);

    // 10.将申请的资源释放掉

_ERROR:
    if( pFmtCtx ){  
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
    }
    if( oFmtCtx->pb ){
        avio_close(oFmtCtx->pb);
    }
    if( oFmtCtx ){
        avformat_free_context(oFmtCtx);
        oFmtCtx = NULL;
    }
    if( stream_map ){
        av_free(stream_map);
    }
    if( dts_start_time ){
        av_free(dts_start_time);

    }
    if( pts_start_time ){
        av_free(pts_start_time);
    }
    return 0;
}

