#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pcm_to_wav.h"

/**
 * 生成 WAV 文件头
 * 
 * @param header      输出缓冲区 (至少 44 字节)
 * @param sample_rate 采样率 (Hz)
 * @param channels    声道数 (1=单声道, 2=立体声)
 * @param bits_per_sample 位深 (8, 16, 24, 32)
 * @param pcm_data_size    PCM 数据大小 (字节)
 * @return             返回 header 指针
 */
void generate_wav_header(wav_header_t *header, 
                                   int sample_rate, 
                                   int channels, 
                                   int bits_per_sample, 
                                   size_t pcm_data_size) 
{
    // RIFF 块
    memcpy(header->chunkID, "RIFF", 4);
    memcpy(header->format, "WAVE", 4);
    header->chunkSize = 36 + pcm_data_size;  // 总文件大小 - 8
    
    // FMT 块
    memcpy(header->subchunk1ID, "fmt ", 4);
    header->subchunk1Size = 16;              // PCM 固定为 16
    header->audioFormat = 1;                 // PCM
    header->numChannels = channels;
    header->sampleRate = sample_rate;
    header->bitsPerSample = bits_per_sample;
    header->byteRate = sample_rate * channels * (bits_per_sample / 8);
    header->blockAlign = channels * (bits_per_sample / 8);
    
    // DATA 块
    memcpy(header->subchunk2ID, "data", 4);
    header->subchunk2Size = pcm_data_size;
}

/**
 * 便捷函数：直接将头和 PCM 数据合并到目标缓冲区
 * 
 * @param dest_buff    目标缓冲区 (需要足够空间: 44 + pcm_data_size)
 * @param pcm_buff     PCM 数据缓冲区
 * @param pcm_data_size PCM 数据大小
 * @param sample_rate  采样率
 * @param channels     声道数
 * @param bits_per_sample 位深
 * @return             返回总大小 (44 + pcm_data_size)
 */
size_t combine_wav_with_pcm(uint8_t *dest_buff, 
                             const uint8_t *pcm_buff, 
                             size_t pcm_data_size,
                             int sample_rate, 
                             int channels, 
                             int bits_per_sample) {
    wav_header_t header;
    
    // 生成头
    generate_wav_header(&header, sample_rate, channels, bits_per_sample, pcm_data_size);
    
    // 复制头到目标缓冲区
    memcpy(dest_buff, &header, sizeof(wav_header_t));
    
    // 复制 PCM 数据到目标缓冲区
    memcpy(dest_buff + sizeof(wav_header_t), pcm_buff, pcm_data_size);
    
    return sizeof(wav_header_t) + pcm_data_size;
}