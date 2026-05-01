#ifndef PCM_TO_WAV_H
#define PCM_TO_WAV_H

// WAV 文件头结构体 (44 字节)
#include <stddef.h>
#include <stdint.h>
typedef struct __attribute__((packed)) {
    // RIFF 块
    char     chunkID[4];        // "RIFF"
    uint32_t chunkSize;         // 文件总大小 - 8
    char     format[4];         // "WAVE"
    
    // FMT 子块
    char     subchunk1ID[4];    // "fmt "
    uint32_t subchunk1Size;     // 16 for PCM
    uint16_t audioFormat;       // 1 = PCM
    uint16_t numChannels;       // 声道数
    uint32_t sampleRate;        // 采样率
    uint32_t byteRate;          // 字节率
    uint16_t blockAlign;        // 块对齐
    uint16_t bitsPerSample;     // 位深
    
    // DATA 子块
    char     subchunk2ID[4];    // "data"
    uint32_t subchunk2Size;     // PCM 数据大小
} wav_header_t;


void generate_wav_header(wav_header_t *header, 
                        int sample_rate, 
                        int channels, 
                        int bits_per_sample, 
                        size_t pcm_data_size);

#endif