#include "library.h"
#include "rdtsc.h"
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>

codecTables_t *codecTables;

// ==========================================================================
// Hashtable implementation specifically for bigrams
// ==========================================================================

unsigned int genHash(const char *s, const unsigned int len,
                     unsigned int hval) {
    unsigned int count;

    if (hval == 0) hval = len;
    count = len;
    while (count-- > 0) {
        hval <<= 4;
        hval += s[count];
    }
    if (hval == 0) ++hval;

    return hval;
}

uint64_t hashBigram(rankedBigram_t *item) {
    const char space = ' ';

    uint64_t hval;
    if (item->hash) {
        return item->hash;
    }
    hval = genHash(item->left, item->left_len, 0);
    hval = genHash(&space, 1, hval);
    hval = genHash(item->right, item->right_len, hval);
    item->hash = hval;
    return hval;
}

uint64_t cmpBigramKey(const rankedBigram_t *item, const char *key) {
    if (strncmp(item->left, key, item->left_len) == 0
        && strncmp(item->right, &(key[item->left_len + 1]),
                   item->right_len) == 0) {
        return 0;
    } else {
        return 1;
    }
}

unsigned int hashLookup(rankedBigram_t *item, ENTRY **retval,
                        unsigned int *hash, const struct hsearch_data *htab) {
    struct hsearch_data lhtab = *htab;
    unsigned int hval;
    unsigned int idx;

    hval = hashBigram(item);
    idx = hval % lhtab.size + 1;

    if (lhtab.table[idx].used) {
        /* Further action might be required according to the action value. */
        if (lhtab.table[idx].used == hval
            && cmpBigramKey(item, lhtab.table[idx].entry.key) == 0) {
            if (retval == NULL) {
                /* Set errno to EINVAL, because 'retval' is a NULL pointer
				(invalid pointer for returning a hash table ENTRY). */
                errno = EINVAL;
                *hash = hval;
                return 0;
            } else {
                *retval = &lhtab.table[idx].entry;
                *hash = hval;
                return idx;
            }
        }

        /* Second hash function, as suggested in [Knuth] */
        unsigned int hval2 = 1 + hval % (lhtab.size - 2);
        unsigned int first_idx = idx;

        do {
            /* Because SIZE is prime this guarantees to step through all
                   available indeces.  */
            if (idx <= hval2)
                idx = lhtab.size + idx - hval2;
            else
                idx -= hval2;

            /* If we visited all entries leave the loop unsuccessfully.  */
            if (idx == first_idx)
                break;

            /* If entry is found use it. */
            if (lhtab.table[idx].used == hval
                && cmpBigramKey(item, lhtab.table[idx].entry.key) == 0) {
                if (retval == NULL) {
                    /* Set errno to EINVAL, because 'retval' is a NULL pointer
                    (invalid pointer for returning a hash table ENTRY). */
                    errno = EINVAL;
                    *hash = hval;
                    return 0;
                } else {
                    *retval = &lhtab.table[idx].entry;
                    *hash = hval;
                    return idx;
                }
            }
        } while (lhtab.table[idx].used);
    }
    errno = ESRCH;
    /* Prevent the dereferencing of a NULL pointer. */
    if (retval != NULL) {
        *retval = NULL;
    }
    *hash = hval;
    return idx;
}


int hashInsert(rankedBigram_t *item, ENTRY **retval,
               struct hsearch_data *htab) {
    unsigned int hval = 0;
    unsigned int idx = hashLookup(item, retval, &hval, htab);

    /* If table is full and another entry should be entered return
       with error.  */
    if (htab->filled == htab->size) {
        errno = ENOMEM;
        /* Prevent the dereferencing of a NULL pointer. */
        if (retval != NULL) {
            *retval = NULL;
        }
        return 0;
    }

    htab->table[idx].used = hval;
    htab->table[idx].entry.key = item->repr;
    htab->table[idx].entry.data = item;
    ++htab->filled;

    /* Ignore 'retval' if 'action' is 'ENTER' and 'retval' is a
            NULL pointer. */
    if (retval != NULL) {
        /* Prevent the dereferencing of a NULL pointer. */
        *retval = &htab->table[idx].entry;
    }
    return 1;
}

// ==========================================================================
// Initialization of various data structures for the codec
// ==========================================================================

enum CODEC_STATUS readJson(const char *filename, cJSON **json) {
    char *buffer;
    long length;
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return ERR_JSON_FOPEN;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length == 0) {
        fclose(f);
        return ERR_JSON_EMPTY;
    }
    buffer = malloc(length);
    if (!buffer) {
        fclose(f);
        return ERR_JSON_MALLOC;
    }
    fread(buffer, 1, length, f);
    fclose(f);
    *json = cJSON_ParseWithLength(buffer, length);
    free(buffer);
    if (*json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "JSON error in `%s` before: %s\n", filename,
                    error_ptr);
        }
        return ERR_JSON_FAILED;
    }
    return CODEC_SUCCESS;
}

void buildTableRange(codecTables_t **table, const uint8_t begin,
                     const uint8_t end) {
    for (uint8_t b = begin; b <= end; b++) {
        (*table)->bytesToUnicode[b] = (uint16_t) b;
        (*table)->unicodeToBytes[(uint16_t) b] = b;
        if (b == 255) break;
    }
}

void fillUnicodePoints(uint16_t *uct, codecTables_t **table,
                       const uint8_t begin, const uint8_t end) {
    for (uint8_t b = begin; b <= end; b++) {
        (*table)->bytesToUnicode[b] = (uint16_t) 256 + *uct;
        (*table)->unicodeToBytes[(uint16_t) 256 + *uct] = b;
        *uct = *uct + 1;
        if (b == 255) break;
    }
}

void buildUnicodeByteTable(codecTables_t **table) {
    buildTableRange(table, 33, 126);
    buildTableRange(table, 161, 172);
    buildTableRange(table, 174, 255);
    uint16_t uct = 0;
    fillUnicodePoints(&uct, table, 0, 32);
    fillUnicodePoints(&uct, table, 127, 160);
    fillUnicodePoints(&uct, table, 173, 173);
}

enum CODEC_STATUS readBpeVocabulary(const char *filename,
                                    codecTables_t **table) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return ERR_BPE_FOPEN;
    }
    char *line = NULL;
    size_t len = 0;
    size_t numLines = 0;
    long skip = 0;

    while (getline(&line, &len, f) != -1) {
        if (!numLines) {
            skip = ftell(f);
        }
        numLines += 1;
    }
    fseek(f, skip, SEEK_SET);
    hcreate_r(numLines, &(*table)->bpeRanks);

    size_t bpeRank = 1;
    while (getline(&line, &len, f) != -1) {
        rankedBigram_t *entry = malloc(sizeof(rankedBigram_t));
        size_t repr_len = strlen(line) - 1;
        entry->repr = strndup(line, repr_len);
        entry->rank = bpeRank;
        char *divisor = strchr(entry->repr, ' ');
        entry->left_len = divisor - entry->repr;
        entry->left = strndup(line, entry->left_len);
        entry->right = divisor + 1;
        entry->right_len = strlen(entry->right);
        ENTRY *ep;
        hashInsert(entry, &ep, &(*table)->bpeRanks);
        bpeRank++;
    }
    return CODEC_SUCCESS;
}

enum CODEC_STATUS readEncoderDefinitions(const char *filename,
                                         codecTables_t **tables) {
    *tables = malloc(sizeof(codecTables_t));

    cJSON *encoderJson = malloc(sizeof(cJSON));
    if (readJson(filename, &encoderJson) != CODEC_SUCCESS) {
        return ERR_JSON_FAILED;
    }
    int numEntries = cJSON_GetArraySize(encoderJson);
    hcreate_r(numEntries, &(*tables)->toToken);
    const cJSON *entry;
    cJSON_ArrayForEach(entry, encoderJson) {
        ENTRY e;
        ENTRY *ep;
        e.key = strdup(entry->string);
        e.data = (void *) (uint64_t) entry->valueint;
        hsearch_r(e, ENTER, &ep, &(*tables)->toToken);
        (*tables)->fromToken[entry->valueint] = e.key;
    }
    cJSON_free(encoderJson);
    return CODEC_SUCCESS;
}

// ==========================================================================
// Bigram functions
// ==========================================================================


void printBigramRepr(const rankedBigram_t *bigram) {
    if (bigram->left != NULL) {
        printf("Rank: %d Left[%zu]: %.*s Right[%zdu]: %.*s\n", bigram->rank,
               bigram->left_len, (int) bigram->left_len, bigram->left,
               bigram->right_len, (int) bigram->right_len, bigram->right);
    }
}

void showBigrams(rankedBigram_t *bigrams) {
    printf("====\n");
    rankedBigram_t *bigram;
    LL_FOREACH(bigrams, bigram) {
        printBigramRepr(bigram);
    }
    printf("====\n");
}

rankedBigram_t *rankBigrams(const codecTables_t *tables,
                            rankedBigram_t *bigrams, size_t *numDups,
                            size_t *numBigrams) {
    rankedBigram_t *highestBigram = NULL;
    rankedBigram_t *currBigram = NULL;
    *numBigrams = 0;
    *numDups = 0;
    DL_FOREACH(bigrams, currBigram) {
        if (currBigram->rank != 0) {
            if (highestBigram == NULL ||
                currBigram->rank < highestBigram->rank) {
                highestBigram = currBigram;
            } else if (currBigram->hash == highestBigram->hash) {
                (*numDups)++;
            }
            *numBigrams += 1;
            continue;
        }
        *numBigrams += 1;
        ENTRY *ep;
        unsigned int hash = 0;
        hashLookup(currBigram,
                   &ep, &hash, &(tables->bpeRanks));
        if (ep != NULL) {
            rankedBigram_t *matchBigram = (rankedBigram_t *) &(*ep->data);
            currBigram->rank = matchBigram->rank;
            currBigram->repr = matchBigram->repr;
        } else {
            currBigram->rank = 65535;
        }
        if (highestBigram == NULL ||
            currBigram->rank < highestBigram->rank) {
            highestBigram = currBigram;
        } else if (currBigram->hash == highestBigram->hash) {
            (*numDups)++;
        }
    }
    // printf("Number of bigrams: %zu\n", *numBigrams);
    // showBigrams(bigrams);
    return highestBigram;
}

void u8_inc(char *s, int *i) {
    (void) (isutf(s[++(*i)]) || isutf(s[++(*i)]) ||
            isutf(s[++(*i)]) || ++(*i));
}

rankedBigram_t *initBPE(rankedBigram_t *bigrams,
                        const char *s, const size_t numBytes) {
    rankedBigram_t *head = NULL;
    size_t bigrams_ct = 1;
    int c_idx = 1;
    bigrams[0].left = s;
    bigrams[0].hash = 0;
    bigrams[0].repr = NULL;
    bigrams[0].rank = 0;
    bigrams[0].prev = NULL;
    bigrams[0].next = NULL;
    if ((uint8_t) s[0] > 192) {
        c_idx += 2;
        bigrams[0].right = s + 2;
        bigrams[0].left_len = 2;
    } else {
        c_idx += 1;
        bigrams[0].right = s + 1;
        bigrams[0].left_len = 1;
    }
    if ((uint8_t) s[1] > 192) {
        bigrams[0].right_len = 2;
    } else {
        bigrams[0].right_len = 1;
    }
    DL_APPEND(head, &(bigrams[0]));
    for (; c_idx < numBytes; c_idx += 1) {
        DL_APPEND(head, &(bigrams[bigrams_ct]));
        bigrams[bigrams_ct].rank = 0;
        bigrams[bigrams_ct].left = bigrams[bigrams_ct - 1].right;
        bigrams[bigrams_ct].left_len = bigrams[bigrams_ct - 1].right_len;
        bigrams[bigrams_ct].right = &(s[c_idx]);
        bigrams[bigrams_ct].repr = NULL;
        bigrams[bigrams_ct].hash = 0;
        if ((uint8_t) s[c_idx] > 192) {
            bigrams[bigrams_ct].right_len = 2;
            c_idx += 1;
        } else {
            bigrams[bigrams_ct].right_len = 1;
        }
        bigrams_ct++;
    }
    return head;
}

bool mergeNeighboringBigrams(rankedBigram_t *head, rankedBigram_t *bigram) {
    rankedBigram_t *prev = bigram->prev;
    rankedBigram_t *next = bigram->next;
    if (prev != NULL && bigram != head) {
        prev->right_len += bigram->right_len;
        prev->repr = NULL;
        prev->rank = 0;
        prev->hash = 0;
    }
    if (next != NULL) {
        next->left -= bigram->left_len;
        next->left_len += bigram->left_len;
        next->repr = NULL;
        next->rank = 0;
        next->hash = 0;
    }
    return true;
}


size_t toBPE(const codecTables_t *tables, const char *s, const size_t numBytes,
             rankedBigram_t *bigramsBuffer) {
    size_t numBigrams = 0;
    size_t numDups = 0;
    rankedBigram_t *bigrams = initBPE(bigramsBuffer, s, numBytes);
    rankedBigram_t *highestBigram = rankBigrams(tables, bigrams,
                                                &numDups, &numBigrams);
    while (highestBigram->rank != 65535 && highestBigram->rank != 0 &&
           numBigrams > 1) {
        rankedBigram_t *bigram;
        rankedBigram_t *tmpBigram;
        DL_FOREACH_SAFE(highestBigram, bigram, tmpBigram) {
            if (bigram->hash != 0 && highestBigram->hash == bigram->hash) {
                mergeNeighboringBigrams(bigrams, bigram);
                DL_DELETE(bigrams, bigram);
                numBigrams--;
                if (numDups > 0) {
                    numDups--;
                } else {
                    break;
                }
            }
        }
        if (numBigrams <= 1) break;
        highestBigram = rankBigrams(tables, bigrams, &numDups, &numBigrams);
    }
    //showBigrams(bigrams);
    return numBigrams;
}

// ==========================================================================
// Higher level functions
// ==========================================================================

size_t toUnicode(const codecTables_t *tables, const char *s, char **unicode,
                 long long numchars) {
    *unicode = malloc(numchars * 2);
    char *dest = *unicode;
    for (int idx = 0; idx < numchars; idx++) {
        uint16_t rune = tables->bytesToUnicode[s[idx]];
        if (rune < 0x80) {
            *dest++ = (char) rune;
        } else {
            *dest++ = (rune >> 6) | 0xC0;
            *dest++ = (rune & 0x3F) | 0x80;
        }
    }
    *dest = '\0';
    size_t sz = dest - *unicode;
    return sz;
}

void SplitWords(const codecTables_t *tables, const char *s) {
    CalibrateRdtscTicks();
    uint64_t start_rdtsc, end_rdtsc;
    uint64_t host_cpu_ticks;
    double host_cpu_ns;
    double host_cpu_us;
    double host_cpu_s;
    double tokens_per_us;
    start_rdtsc = RDTSC();
    regmatch_t match;
    int regex_status = 0;
    size_t token_ct = 0;
    const char *s_ptr = s;
    rankedBigram_t bigrams[256];
    while (regex_status == 0) {
        regex_status = regexec(&tables->pattern, s_ptr, 1, &match, 0);
        char *unicode = NULL;
        size_t unicode_sz = toUnicode(tables, s_ptr, &unicode, match.rm_eo);
        token_ct += toBPE(tables, unicode, unicode_sz,
                          (rankedBigram_t *) &bigrams);
        free(unicode);
        //printf("%.*s\n", match.rm_eo, s_ptr);
        s_ptr += match.rm_eo;
    }
    end_rdtsc = RDTSC();
    // Calculate rates
    host_cpu_ticks = end_rdtsc - start_rdtsc;
    host_cpu_ns = host_cpu_ticks / g_TicksPerNanoSec;
    host_cpu_us = host_cpu_ns / 1000;
    host_cpu_s = host_cpu_ns / 1000000000;
    tokens_per_us = token_ct / host_cpu_us;
    printf("%.2lf token/µs, %zu tokens, %.4f seconds, %llu ticks\n",
           tokens_per_us,
           token_ct,
           host_cpu_s,
           host_cpu_ticks);
}

enum CODEC_STATUS EncodeTextFile(const char *path) {
    if (codecTables == NULL) {
        InitializeGPT2Codec();
    }
    char *buffer = 0;
    long length;
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ERR_CORPUS_FOPEN;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length == 0) {
        fclose(f);
        return ERR_CORPUS_EMPTY;
    }
    buffer = malloc(length);
    if (!buffer) {
        fclose(f);
        return ERR_CORPUS_MALLOC;
    }
    fread(buffer, 1, length, f);
    SplitWords(codecTables, buffer);
    return CODEC_SUCCESS;
}

enum CODEC_STATUS InitializeGPT2Codec() {
    readEncoderDefinitions("resources/encoder.json", &codecTables);
    readBpeVocabulary("resources/vocab.bpe", &codecTables);
    buildUnicodeByteTable(&codecTables);
    int result = regcomp(
            &(codecTables->pattern),
            "'s|'t|'re|'ve|'m|'ll|'d| ?[[:alpha:]]+| ?[[:digit:]]+| ?[^[:space:][:alpha:][:digit:]]+|[[:space:]]+",
            REG_EXTENDED);
    printf("regex result: %d\n", result);
}