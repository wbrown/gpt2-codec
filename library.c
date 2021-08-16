#include "library.h"
#include "rdtsc.h"
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <wctype.h>
#include <utf8proc/utf8proc.h>

codecTables_t *codecTables;

// ==========================================================================
// Escape print output
// ==========================================================================

void EscapePrint(int ch) {
    // Delete or adjust these 2 arrays per code's goals
    // All simple-escape-sequence C11 6.4.4.4
    static const char *escapev = "\a\b\t\n\v\f\r\"\'\?\\";
    static const char *escapec = "abtnvfr\"\'\?\\";
    char *p = strchr(escapev, ch);
    if (p && *p) {
        printf("\\%c", escapec[p - escapev]);
    } else if (isprint(ch)) {
        fputc(ch, stdout);
    } else {
        // Use octal as hex is problematic reading back
        printf("\\%03o", ch);
    }
}

void EscapePrints(const char *data, int length) {
    if (length == 0) {
        length = strlen(data);
    }
    while (length-- > 0) {
        EscapePrint((unsigned char) *data++);
    }
}

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
            *retval = &lhtab.table[idx].entry;
            *hash = hval;
            return idx;
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
                *retval = &lhtab.table[idx].entry;
                *hash = hval;
                return idx;
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
    //printf("Number of bigrams: %zu\n", *numBigrams);
    // showBigrams(bigrams);
    return highestBigram;
}

void u8_inc(char *s, int *i) {
    (void) (isutf(s[++(*i)]) || isutf(s[++(*i)]) ||
            isutf(s[++(*i)]) || ++(*i));
}

size_t encodeCharBPE(codecTables_t *tables, char **ch, char **dest) {
    uint16_t rune = tables->bytesToUnicode[**ch];
    *ch = *ch + 1;
    if (rune == 0) {
        return 0;
    } else if (rune < 0x80) {
        **dest = (char) rune;
        *dest = *dest + 1;
        return 1;
    } else {
        **dest = (char) ((rune >> 6) | 0xC0);
        *dest = *dest + 1;
        **dest = (char) ((rune & 0x3F) | 0x80);
        *dest = *dest + 1;
        return 2;
    }
}


rankedBigram_t *initBPE(codecTables_t *tables, rankedBigram_t *bigrams,
                        const char *s, const size_t numBytes, char *encoded) {
    char *inputPtr = (char *) s;
    char *endPtr = (char *) s + numBytes;
    rankedBigram_t *head = NULL;
    size_t bigrams_ct = 1;
    bigrams[0].left = encoded;
    bigrams[0].hash = 0;
    bigrams[0].repr = NULL;
    bigrams[0].rank = 0;
    bigrams[0].prev = NULL;
    bigrams[0].next = NULL;
    DL_APPEND(head, &(bigrams[0]));

    size_t encodedLen = encodeCharBPE(tables, &inputPtr, &encoded);
    bigrams[0].left_len = encodedLen;
    if (inputPtr >= endPtr) {
        bigrams[0].right_len = 0;
        bigrams[0].right = NULL;
        return head;
    } else {
        bigrams[0].right = encoded;
        bigrams[0].right_len = encodeCharBPE(tables, &inputPtr, &encoded);
    }
    for (; inputPtr < endPtr;) {
        DL_APPEND(head, &(bigrams[bigrams_ct]));
        bigrams[bigrams_ct].right = encoded;
        encodedLen = encodeCharBPE(tables, &inputPtr, &encoded);
        bigrams[bigrams_ct].right_len = encodedLen;
        bigrams[bigrams_ct].rank = 0;
        bigrams[bigrams_ct].left = bigrams[bigrams_ct - 1].right;
        bigrams[bigrams_ct].left_len = bigrams[bigrams_ct - 1].right_len;
        bigrams[bigrams_ct].repr = NULL;
        bigrams[bigrams_ct].hash = 0;
        bigrams_ct++;
    }
    *encoded = '\0';
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


size_t toBPE(codecTables_t *tables, const char *s, const size_t numBytes,
             rankedBigram_t *bigramsBuffer, char *transcode) {
    TokenCacheEntry *cacheEntry;
    /* HASH_FIND_STR(tables->tokenCache, s, cacheEntry);
    if (cacheEntry != NULL) {
        return cacheEntry->numTokens;
    } */
    size_t numBigrams = 0;
    size_t numDups = 0;
    rankedBigram_t *bigrams = initBPE(tables, bigramsBuffer, s, numBytes,
                                      transcode);
    rankedBigram_t *highestBigram = rankBigrams(tables, bigrams,
                                                &numDups, &numBigrams);
    // showBigrams(bigrams);
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
        highestBigram = rankBigrams(tables, bigrams, &numDups, &numBigrams);
        if (numBigrams <= 1) break;
    }

    rankedBigram_t *bigram;
    // showBigrams(bigrams);
    // printf("TOKENS: ");
    size_t tokens_ct = 0;
    DL_FOREACH(bigrams, bigram) {
        printf("TOKEN: |");
        if (bigram->rank != 65535) {
            /* printf("|%.*s%.*s", (int)bigram->left_len, bigram->left,
                   (int)bigram->right_len, bigram->right); */
            EscapePrints(bigram->left, bigram->left_len);
            EscapePrints(bigram->right, bigram->right_len);
            printf("|");
            tokens_ct += 1;
        } else {
            EscapePrints(bigram->left, bigram->left_len);
            //printf("|%.*s", (int)bigram->left_len, bigram->left);
            tokens_ct += 1;
            printf("|");
            if (bigram->next == NULL && bigram->right_len != 0) {
                EscapePrints(bigram->right, bigram->right_len);
                //printf("|%.*s", (int)bigram->right_len, bigram->right);
                tokens_ct += 1;
                printf("|");
            }
        }
        printf("\n");
    }
    cacheEntry = (TokenCacheEntry *) malloc(sizeof *cacheEntry);
    cacheEntry->numTokens = tokens_ct;
    cacheEntry->id = strdup(s);
    // HASH_ADD_STR(tables->tokenCache, id, cacheEntry);
    return tokens_ct;
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
            *dest++ = (char) ((rune >> 6) | 0xC0);
            *dest++ = (char) ((rune & 0x3F) | 0x80);
        }
    }
    *dest = '\0';
    size_t sz = dest - *unicode;
    return sz;
}

void SplitWords(codecTables_t *tables, const char *s) {
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
    char unicode[256];
    while (regex_status == 0) {
        regex_status = regexec(&tables->pattern, s_ptr, 1, &match, 0);
        token_ct += toBPE(tables, s_ptr, match.rm_eo,
                          (rankedBigram_t *) &bigrams, unicode);
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

// UTF8PROC_CATEGORY_CN  = 0, /**< Other, not assigned */
// UTF8PROC_CATEGORY_LU  = 1, /**< Letter, uppercase */
// UTF8PROC_CATEGORY_LL  = 2, /**< Letter, lowercase */
// UTF8PROC_CATEGORY_LT  = 3, /**< Letter, titlecase */
// UTF8PROC_CATEGORY_LM  = 4, /**< Letter, modifier */
// UTF8PROC_CATEGORY_LO  = 5, /**< Letter, other */
// UTF8PROC_CATEGORY_MN  = 6, /**< Mark, nonspacing */
// UTF8PROC_CATEGORY_MC  = 7, /**< Mark, spacing combining */
// UTF8PROC_CATEGORY_ME  = 8, /**< Mark, enclosing */
// UTF8PROC_CATEGORY_ND  = 9, /**< Number, decimal digit */
// UTF8PROC_CATEGORY_NL = 10, /**< Number, letter */
// UTF8PROC_CATEGORY_NO = 11, /**< Number, other */
// UTF8PROC_CATEGORY_PC = 12, /**< Punctuation, connector */
// UTF8PROC_CATEGORY_PD = 13, /**< Punctuation, dash */
// UTF8PROC_CATEGORY_PS = 14, /**< Punctuation, open */
// UTF8PROC_CATEGORY_PE = 15, /**< Punctuation, close */
// UTF8PROC_CATEGORY_PI = 16, /**< Punctuation, initial quote */
// UTF8PROC_CATEGORY_PF = 17, /**< Punctuation, final quote */
// UTF8PROC_CATEGORY_PO = 18, /**< Punctuation, other */
// UTF8PROC_CATEGORY_SM = 19, /**< Symbol, math */
// UTF8PROC_CATEGORY_SC = 20, /**< Symbol, currency */
// UTF8PROC_CATEGORY_SK = 21, /**< Symbol, modifier */
// UTF8PROC_CATEGORY_SO = 22, /**< Symbol, other */
// UTF8PROC_CATEGORY_ZS = 23, /**< Separator, space */
// UTF8PROC_CATEGORY_ZL = 24, /**< Separator, line */
// UTF8PROC_CATEGORY_ZP = 25, /**< Separator, paragraph */
// UTF8PROC_CATEGORY_CC = 26, /**< Other, control */
// UTF8PROC_CATEGORY_CF = 27, /**< Other, format */
// UTF8PROC_CATEGORY_CS = 28, /**< Other, surrogate */
// UTF8PROC_CATEGORY_CO = 29, /**< Other, private use */

typedef struct {
    bool apostrophe;
    bool priorIsLiteralSpace;
    bool priorIsUnicodeSpace;
    bool priorIsNumber;
    bool priorIsLetter;
    bool priorIsNewline;
    bool priorIsOther;
    size_t numLiteralSpaces;
    size_t numSpaces;
    size_t buffIdx;
    size_t numTokens;
    size_t inputSize;
    size_t bytesScanned;
    codecTables_t *codec;
    char buffer[256];
    char unicode[256];
    rankedBigram_t bigrams[256];
} SplitterState;


void flushState(SplitterState *state) {
    state->buffer[state->buffIdx] = '\0';
    /* printf("SPLIT: |");
    EscapePrints(state->buffer, 0);
    printf("|\n"); */
    state->numTokens += toBPE(state->codec, state->buffer, state->buffIdx,
                              (rankedBigram_t *) &state->bigrams,
                              state->unicode);
    // fflush(stdout);

    state->apostrophe = false;
    state->priorIsLiteralSpace = false;
    state->priorIsUnicodeSpace = false;
    state->priorIsNumber = false;
    state->priorIsLetter = false;
    state->priorIsOther = false;
    state->priorIsNewline = false;
    state->numSpaces = 0;
    state->numLiteralSpaces = 0;
    state->buffIdx = 0;
}

int codePoint(int rune, void *inputState) {
    SplitterState *state = (SplitterState *) inputState;
    bool isSpace = false;
    bool isLiteralSpace = false;
    bool isNewline = false;
    //printf("RUNE: %d TYPE: %s\n", rune, utf8proc_category_string(rune));
    if (rune < 127) {
        char charRune = (char) rune;
        state->buffer[state->buffIdx] = charRune;
        if (state->apostrophe) {
            switch (charRune) {
                case 's':
                case 't':
                case 'm':
                case 'd':
                    if (state->buffIdx == 1) {
                        flushState(state);
                        return rune;
                    }
                    state->apostrophe = false;
                    break;
                case 'l':
                    if (state->buffIdx == 2 &&
                        state->buffer[state->buffIdx - 1] == 'l') {
                        flushState(state);
                        return rune;
                    }
                    state->apostrophe = false;
                    break;
                case 'r':
                case 'v':
                    if (state->buffIdx != 1)
                        state->apostrophe = false;
                    break;
                case 'e':
                    if (state->buffIdx == 2) {
                        flushState(state);
                        return rune;
                    }
                default:
                    state->apostrophe = false;
            }
        }
        switch (charRune) {
            case '\'':
                if (state->buffIdx == 0) state->apostrophe = true;
                return rune;
            case ' ':
                if (state->buffIdx != 0 && state->numLiteralSpaces == 0) {
                    flushState(state);
                    state->buffer[0] = charRune;
                    state->buffIdx++;
                    return rune;
                } else if (state->buffIdx == state->numLiteralSpaces) {
                    state->numLiteralSpaces++;
                    isLiteralSpace = true;
                }
            case '\f':
            case '\r':
            case '\v':
            case '\t':
            case '\n':
                isSpace = true;
                state->numSpaces++;
            default:
                break;
        }
    }
    bool isLetter = false, isNumber = false, isOther = false;
    utf8proc_int32_t unicodeCategory = utf8proc_category(rune);
    // printf("%s\n", utf8proc_category_string(rune));
    switch (unicodeCategory) {
        case UTF8PROC_CATEGORY_LU:
        case UTF8PROC_CATEGORY_LL:
        case UTF8PROC_CATEGORY_LT:
        case UTF8PROC_CATEGORY_LM:
        case UTF8PROC_CATEGORY_LO:
            isLetter = true;
            break;
        case UTF8PROC_CATEGORY_ND:
        case UTF8PROC_CATEGORY_NL:
        case UTF8PROC_CATEGORY_NO:
            isNumber = true;
            break;
        case UTF8PROC_CATEGORY_ZS:
        case UTF8PROC_CATEGORY_ZL:
        case UTF8PROC_CATEGORY_ZP:
            isSpace = true;
            break;
        case UTF8PROC_CATEGORY_CC:
            if (isSpace) break;
        default:
            isOther = true;
    }
    if ((isLetter && state->priorIsNumber) ||   // letters -> numbers
        (isNumber && state->priorIsLetter) ||   // numbers -> letters
        ((isLetter || isNumber) &&              // litSpace -> numbers/letters
         state->priorIsLiteralSpace &&
         state->buffIdx > 1) ||
        ((isLetter || isNumber || isOther) &&   // unicodeSpace ->
         state->priorIsUnicodeSpace) ||         //   non-unicodeSpace
        ((isLetter || isNumber) &&             // other -> letter/number
         state->priorIsOther) ||
        (isSpace && state->priorIsOther) ||
        ((isSpace || isOther) &&                // letter/number ->
         (state->priorIsLetter ||               //   space / other
          state->priorIsNumber)) ||
        (isSpace &&
         (state->priorIsLetter ||
          state->priorIsNumber ||
          (state->priorIsUnicodeSpace &&
           state->numSpaces == 3)))) // newlines
    {
        flushState(state);
    }
    size_t numBytes = utf8proc_encode_char(
            rune, (utf8proc_uint8_t *)
                    &state->buffer[state->buffIdx]);
    state->buffIdx += numBytes;
    state->priorIsLiteralSpace = isLiteralSpace;
    state->priorIsUnicodeSpace = isSpace;
    state->priorIsLetter = isLetter;
    state->priorIsNumber = isNumber;
    state->priorIsOther = isOther;
    state->priorIsNewline = isNewline;
    state->bytesScanned += numBytes;
    return 0;
}

int scanWords(const unsigned char *s, codecTables_t *tables) {
    // printf("\nscanWords called\n");
    // printf("%s\n", s);
    unsigned char *dst;
    CalibrateRdtscTicks();
    uint64_t start_rdtsc, end_rdtsc;
    uint64_t host_cpu_ticks;
    double host_cpu_ns;
    double host_cpu_us;
    double host_cpu_s;
    double tokens_per_us;
    start_rdtsc = RDTSC();
    size_t numBytes = strlen((const char *) s);
    SplitterState state = {false,
                           false,
                           false,
                           false,
                           false,
                           false,
                           false,
                           0,
                           0,
                           0,
                           0,
                           numBytes,
                           0,
                           tables,
                           {},
                           {}};
    utf8proc_decompose_custom(s,
                              (long) numBytes,
                              NULL,
                              0,
                              0,
                              codePoint,
                              &state);
    flushState(&state);
    end_rdtsc = RDTSC();
    // Calculate rates
    host_cpu_ticks = end_rdtsc - start_rdtsc;
    host_cpu_ns = host_cpu_ticks / g_TicksPerNanoSec;
    host_cpu_us = host_cpu_ns / 1000;
    host_cpu_s = host_cpu_ns / 1000000000;
    tokens_per_us = state.numTokens / host_cpu_us;
    printf("\n%.2lf token/µs, %zu tokens, %.4f seconds, %llu ticks\n",
           tokens_per_us,
           state.numTokens,
           host_cpu_s,
           host_cpu_ticks);
    return 0;
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
    scanWords(buffer, codecTables);
    // SplitWords(codecTables, buffer);
    return CODEC_SUCCESS;
}


/*

int nextWord(char *s) {
    utf8proc_custom_func
    if (s == NULL || *s == '\0') return -1;
    switch (*s) {
        case '\'':
            s++;
            switch (*s) {
                case 's':
                case 't':
                case 'm':
                case 'd':
                    return 0;
                case 'r':
                case 'v':
                    s++;
                    if (*s == 'e') return 0;
            }
        case ' ':


    }
} */

enum CODEC_STATUS InitializeGPT2Codec() {
    readEncoderDefinitions("resources/encoder.json", &codecTables);
    readBpeVocabulary("resources/vocab.bpe", &codecTables);
    buildUnicodeByteTable(&codecTables);
    codecTables->tokenCache = NULL;
    int result = regcomp(
            &(codecTables->pattern),
            "'s|'t|'re|'ve|'m|'ll|'d| ?[[:alpha:]]+| ?[[:digit:]]+| ?[^[:space:][:alpha:][:digit:]]+|[[:space:]]+",
            REG_EXTENDED);
    printf("regex result: %d\n", result);
}