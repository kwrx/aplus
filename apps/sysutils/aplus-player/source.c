/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>

#include "player.h"


#define DECODER_PREROLL_MS 250


struct player_decoder {

    char* path;

    AVFormatContext* format;
    AVCodecContext* codec;
    SwrContext* swr;
    AVPacket* packet;
    AVFrame* frame;

    int stream;
    int64_t start;

    int16_t (*pending)[2];
    size_t pending_capacity;
    size_t pending_count;
    size_t pending_at;

    int64_t next;
    int64_t skip;
    int64_t offset;

    bool calibrating;
    bool seeking;
    bool flushing;
    bool finished;
    bool touched;
};


static const struct {

    const char* name;
    const char* label;

} decoder_labels[] = {

    {"vorbis",  "Vorbis" },
    {"opus",    "Opus"   },
    {"wavpack", "WavPack"},
    {"ape",     "Monkey's Audio"},
    {"wmav1",   "WMA"    },
    {"wmav2",   "WMA"    },
    {"wmapro",  "WMA Pro"},
};


static void decoder_label(player_source_t* source, enum AVCodecID id) {

    const AVCodecDescriptor* descriptor = avcodec_descriptor_get(id);

    const char* name = descriptor ? descriptor->name : "audio";

    if (strncmp(name, "pcm_", 4) == 0) {

        const char* container = source->decoder->format->iformat->name;

        if (strcmp(container, "wav") == 0) {
            snprintf(source->codec, sizeof(source->codec), "WAV");
        } else if (strcmp(container, "aiff") == 0) {
            snprintf(source->codec, sizeof(source->codec), "AIFF");
        } else if (strcmp(container, "s16le") == 0) {
            snprintf(source->codec, sizeof(source->codec), "Raw PCM");
        } else {
            snprintf(source->codec, sizeof(source->codec), "PCM");
        }

        return;
    }

    for (size_t i = 0; i < sizeof(decoder_labels) / sizeof(decoder_labels[0]); i++) {

        if (strcmp(name, decoder_labels[i].name) == 0) {
            snprintf(source->codec, sizeof(source->codec), "%s", decoder_labels[i].label);
            return;
        }
    }

    size_t i = 0;

    for (; name[i] && i < sizeof(source->codec) - 1; i++) {
        source->codec[i] = (char)toupper((unsigned char)name[i]);
    }

    source->codec[i] = '\0';
}


static const char* decoder_tag(const player_decoder_t* decoder, const char* key) {

    const AVDictionaryEntry* entry = av_dict_get(decoder->format->metadata, key, NULL, 0);

    if (!entry || !entry->value[0]) {
        entry = av_dict_get(decoder->format->streams[decoder->stream]->metadata, key, NULL, 0);
    }

    return entry && entry->value[0] ? entry->value : NULL;
}


static void decoder_describe(player_source_t* source) {

    const player_decoder_t* decoder = source->decoder;

    const AVStream* stream = decoder->format->streams[decoder->stream];


    decoder_label(source, decoder->codec->codec_id);


    source->bits = (uint16_t)decoder->codec->bits_per_raw_sample;

    const AVCodecDescriptor* descriptor = avcodec_descriptor_get(decoder->codec->codec_id);

    if (source->bits == 0 && descriptor && strncmp(descriptor->name, "pcm_", 4) == 0) {
        source->bits = (uint16_t)av_get_bits_per_sample(decoder->codec->codec_id);
    }


    int64_t bitrate = stream->codecpar->bit_rate;

    if (bitrate <= 0) {
        bitrate = decoder->format->bit_rate;
    }

    source->bitrate = bitrate > 0 ? (uint32_t)(bitrate / 1000) : 0;


    const char* title  = decoder_tag(decoder, "title");
    const char* artist = decoder_tag(decoder, "artist");

    if (title && artist) {
        snprintf(source->title, sizeof(source->title), "%s \xE2\x80\x94 %s", artist, title);
    } else if (title) {
        snprintf(source->title, sizeof(source->title), "%s", title);
    }


    if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
        source->frames = (uint64_t)av_rescale_q(stream->duration, stream->time_base, (AVRational){1, (int)source->rate});
    } else if (decoder->format->duration != AV_NOPTS_VALUE && decoder->format->duration > 0) {
        source->frames = (uint64_t)av_rescale(decoder->format->duration, source->rate, AV_TIME_BASE);
    } else {
        source->frames = 0;
    }
}


static int decoder_resampler(player_decoder_t* decoder) {

    const AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;

    if (swr_alloc_set_opts2(&decoder->swr, &stereo, AV_SAMPLE_FMT_S16, decoder->codec->sample_rate, &decoder->codec->ch_layout, decoder->codec->sample_fmt, decoder->codec->sample_rate, 0, NULL) < 0) {
        return -1;
    }

    if (decoder->codec->ch_layout.nb_channels == 1) {

        const double matrix[2] = {1.0, 1.0};

        if (swr_set_matrix(decoder->swr, matrix, 1) < 0) {
            return -1;
        }
    }

    return swr_init(decoder->swr);
}


static int decoder_convert(player_source_t* source) {

    player_decoder_t* decoder = source->decoder;

    AVFrame* frame = decoder->frame;

    const int room = swr_get_out_samples(decoder->swr, frame->nb_samples);

    if (room <= 0) {
        return 0;
    }

    if ((size_t)room > decoder->pending_capacity) {

        int16_t (*pending)[2] = realloc(decoder->pending, (size_t)room * sizeof(pending[0]));

        if (!pending) {
            return -1;
        }

        decoder->pending          = pending;
        decoder->pending_capacity = (size_t)room;
    }


    uint8_t* out = (uint8_t*)decoder->pending;

    const int got = swr_convert(decoder->swr, &out, room, (const uint8_t**)frame->extended_data, frame->nb_samples);

    if (got < 0) {
        return -1;
    }


    const AVStream* stream = decoder->format->streams[decoder->stream];

    int64_t start = decoder->next;

    if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {

        start = av_rescale_q(frame->best_effort_timestamp - decoder->start, stream->time_base, (AVRational){1, (int)source->rate});

        const int64_t nominal = frame->duration > 0 ? av_rescale_q(frame->duration, stream->time_base, (AVRational){1, (int)source->rate}) : 0;

        if (start < 0 && nominal > got) {
            start += nominal - got;
        }

        if (decoder->calibrating) {
            decoder->offset = start;
        }

        start -= decoder->offset;
    }

    decoder->calibrating = false;

    decoder->next = start + got;


    size_t drop = 0;

    if (decoder->seeking && decoder->skip > start) {
        drop = decoder->skip - start < got ? (size_t)(decoder->skip - start) : (size_t)got;
    }

    if (drop < (size_t)got) {
        decoder->seeking = false;
    }

    decoder->pending_at    = drop;
    decoder->pending_count = (size_t)got;

    return 0;
}


static int decoder_step(player_source_t* source) {

    player_decoder_t* decoder = source->decoder;

    for (;;) {

        const int r = avcodec_receive_frame(decoder->codec, decoder->frame);

        if (r == 0) {

            const int e = decoder_convert(source);

            av_frame_unref(decoder->frame);

            return e;
        }

        if (r == AVERROR_EOF) {
            decoder->finished = true;
            return 0;
        }

        if (r != AVERROR(EAGAIN)) {
            decoder->finished = true;
            return -1;
        }

        if (decoder->flushing) {
            decoder->finished = true;
            return 0;
        }


        const int e = av_read_frame(decoder->format, decoder->packet);

        if (e < 0) {
            decoder->flushing = true;
            avcodec_send_packet(decoder->codec, NULL);
            continue;
        }

        if (decoder->packet->stream_index == decoder->stream) {
            avcodec_send_packet(decoder->codec, decoder->packet);
        }

        av_packet_unref(decoder->packet);
    }
}


static void decoder_stop(player_decoder_t* decoder) {

    swr_free(&decoder->swr);
    av_frame_free(&decoder->frame);
    av_packet_free(&decoder->packet);
    avcodec_free_context(&decoder->codec);
    avformat_close_input(&decoder->format);
}


static int decoder_start(player_decoder_t* decoder, const char** error) {

    const AVInputFormat* input = NULL;

    AVDictionary* options = NULL;

    const char* dot = strrchr(decoder->path, '.');

    if (dot && strcasecmp(dot, ".pcm") == 0) {

        input = av_find_input_format("s16le");

        av_dict_set_int(&options, "sample_rate", PLAYER_PCM_RATE, 0);
        av_dict_set(&options, "ch_layout", PLAYER_PCM_LAYOUT, 0);
    }


    const int e = avformat_open_input(&decoder->format, decoder->path, input, &options);

    av_dict_free(&options);

    if (e < 0) {
        *error = e == AVERROR(ENOENT) || e == AVERROR(EACCES) ? strerror(AVUNERROR(e)) : "Unsupported file format";
        return -1;
    }

    if (avformat_find_stream_info(decoder->format, NULL) < 0) {
        *error = "The file is damaged";
        return -1;
    }


    const AVCodec* codec = NULL;

    if ((decoder->stream = av_find_best_stream(decoder->format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0)) < 0) {
        *error = decoder->stream == AVERROR_DECODER_NOT_FOUND ? "Unsupported audio codec" : "The file has no audio";
        return -1;
    }

    for (unsigned i = 0; i < decoder->format->nb_streams; i++) {

        if ((int)i != decoder->stream) {
            decoder->format->streams[i]->discard = AVDISCARD_ALL;
        }
    }


    const AVStream* stream = decoder->format->streams[decoder->stream];

    if ((decoder->codec = avcodec_alloc_context3(codec)) == NULL || avcodec_parameters_to_context(decoder->codec, stream->codecpar) < 0) {
        *error = strerror(ENOMEM);
        return -1;
    }

    decoder->codec->thread_count = 1;

    if (avcodec_open2(decoder->codec, codec, NULL) < 0) {
        *error = "Unsupported audio codec";
        return -1;
    }

    if (decoder->codec->sample_rate <= 0 || decoder->codec->ch_layout.nb_channels <= 0) {
        *error = "The file is damaged";
        return -1;
    }


    if (decoder_resampler(decoder) < 0) {
        *error = "Unsupported channel layout";
        return -1;
    }

    if ((decoder->packet = av_packet_alloc()) == NULL || (decoder->frame = av_frame_alloc()) == NULL) {
        *error = strerror(ENOMEM);
        return -1;
    }

    decoder->start = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;

    return 0;
}


int player_source_open(player_source_t* source, const char* path, const char** error) {

    av_log_set_level(AV_LOG_QUIET);


    player_decoder_t* decoder = calloc(1, sizeof(*decoder));

    if (!decoder) {
        *error = strerror(ENOMEM);
        return -1;
    }

    memset(source, 0, sizeof(*source));

    source->decoder = decoder;

    if ((decoder->path = strdup(path)) == NULL) {
        *error = strerror(ENOMEM);
        player_source_close(source);
        return -1;
    }

    if (decoder_start(decoder, error) < 0) {
        player_source_close(source);
        return -1;
    }

    source->rate     = (uint32_t)decoder->codec->sample_rate;
    source->channels = (uint16_t)decoder->codec->ch_layout.nb_channels;
    source->position = 0;

    decoder_describe(source);


    decoder->calibrating = true;

    while (decoder->pending_count == 0 && !decoder->finished) {

        if (decoder_step(source) < 0) {
            *error = "The file is damaged";
            player_source_close(source);
            return -1;
        }
    }

    return 0;
}


void player_source_close(player_source_t* source) {

    player_decoder_t* decoder = source->decoder;

    if (!decoder) {
        return;
    }

    decoder_stop(decoder);

    if (decoder->pending) {
        free(decoder->pending);
    }

    if (decoder->path) {
        free(decoder->path);
    }

    free(decoder);

    source->decoder = NULL;
}


static int decoder_rewind(player_decoder_t* decoder) {

    const char* error = NULL;

    decoder_stop(decoder);

    if (decoder_start(decoder, &error) < 0) {
        decoder_stop(decoder);
        errno = EIO;
        return -1;
    }

    return 0;
}


int player_source_seek(player_source_t* source, uint64_t frame) {

    player_decoder_t* decoder = source->decoder;

    if (frame == 0 && !decoder->touched) {
        source->position = 0;
        return 0;
    }

    if (!decoder->format) {
        errno = EIO;
        return -1;
    }


    const AVStream* stream = decoder->format->streams[decoder->stream];

    int64_t preroll = stream->codecpar->seek_preroll;

    if (preroll < (int64_t)source->rate * DECODER_PREROLL_MS / 1000) {
        preroll = (int64_t)source->rate * DECODER_PREROLL_MS / 1000;
    }


    if ((int64_t)frame <= preroll) {

        if (decoder_rewind(decoder) < 0) {
            decoder->finished = true;
            return -1;
        }

        decoder->next = 0;

    } else {

        const int64_t target = decoder->start + av_rescale_q((int64_t)frame - preroll, (AVRational){1, (int)source->rate}, stream->time_base);

        if (av_seek_frame(decoder->format, decoder->stream, target, AVSEEK_FLAG_BACKWARD) < 0) {
            errno = ESPIPE;
            return -1;
        }

        avcodec_flush_buffers(decoder->codec);

        swr_free(&decoder->swr);

        if (decoder_resampler(decoder) < 0) {
            errno = ENOMEM;
            return -1;
        }

        decoder->next = (int64_t)frame;
    }

    decoder->pending_count = 0;
    decoder->pending_at    = 0;
    decoder->skip          = (int64_t)frame;
    decoder->seeking       = frame > 0;
    decoder->flushing      = false;
    decoder->finished      = false;
    decoder->touched       = true;

    source->position = frame;

    return 0;
}


size_t player_source_read(player_source_t* source, int16_t (*out)[2], size_t frames) {

    player_decoder_t* decoder = source->decoder;

    decoder->touched = true;


    size_t total = 0;

    while (total < frames) {

        if (decoder->pending_at < decoder->pending_count) {

            size_t take = decoder->pending_count - decoder->pending_at;

            if (take > frames - total) {
                take = frames - total;
            }

            memcpy(out + total, decoder->pending + decoder->pending_at, take * sizeof(out[0]));

            decoder->pending_at += take;
            total += take;

            continue;
        }

        if (decoder->finished || decoder_step(source) < 0) {
            break;
        }
    }


    source->position += total;

    if (decoder->finished && decoder->pending_at >= decoder->pending_count && total < frames) {
        source->frames = source->position;
    } else if (source->position > source->frames) {
        source->frames = source->position;
    }

    return total;
}
