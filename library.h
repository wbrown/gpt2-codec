#ifndef GPT2_CODEC_LIBRARY_H
#define GPT2_CODEC_LIBRARY_H

#include <cJSON/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <search_hsearch_r.h>
#include <regex.h>

#define isutf(c) (((c)&0xC0)!=0x80)

typedef struct _ENTRY {
    unsigned int used;
    ENTRY entry;
} _ENTRY;

enum CODEC_STATUS {
    CODEC_SUCCESS,
    ERR_JSON_FOPEN,
    ERR_JSON_MALLOC,
    ERR_JSON_EMPTY,
    ERR_JSON_FAILED,
    ERR_BPE_FOPEN,
    ERR_BPE_MALLOC,
    ERR_BPE_EMPTY,
    ERR_BPE_FAILED,
    ERR_CORPUS_FOPEN,
    ERR_CORPUS_MALLOC,
    ERR_CORPUS_EMPTY
};

struct codecTablesStruct {
    struct hsearch_data toToken;
    struct hsearch_data bpeRanks;
    char *fromToken[65535];
    uint8_t unicodeToBytes[324];
    uint16_t bytesToUnicode[256];
    regex_t pattern;
};

struct BPERankedPair {
    char *repr;
    uint16_t rank;
    uint64_t hash;
    char *left;
    size_t left_len;
    char *right;
    size_t right_len;
};

typedef struct BPERankedPair rankedBigram_t;

typedef struct codecTablesStruct codecTables_t;

enum CODEC_STATUS readJson(const char *filename, cJSON **json);

enum CODEC_STATUS readEncoderDefinitions(const char *filename,
                                         codecTables_t **tables);

enum CODEC_STATUS readBpeVocabulary(const char *filename,
                                    codecTables_t **tables);

void buildUnicodeByteTable(codecTables_t **tables);

enum CODEC_STATUS InitializeGPT2Codec();

enum CODEC_STATUS EncodeTextFile(const char *path);

#endif //GPT2_CODEC_LIBRARY_H
