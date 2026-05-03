#include <windows.h>
#include <wmmintrin.h>
#include <smmintrin.h>
#include "Aes128Ctr.h"

//EXTERN_C int _fltused = 0;

// AES Definitions 
#define AES_BLOCK_SIZE      16
#define AES_ROUNDS          10
#define AES_KEY_SCHEDULE    (AES_ROUNDS + 1)
#define AES_SHIFT_VAL       4   
#define BYTES_TO_AES_BLOCKS(qwBytes) ((qwBytes) >> 4)

// ==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==-==

static void Aes128KeyExpansion(const unsigned char* pAesKey, __m128i* pKeySchedule)
{
    __m128i xmmTemp1, xmmTemp2;

    // Load master key
    xmmTemp1 = _mm_loadu_si128((const __m128i*)pAesKey);
    pKeySchedule[0] = xmmTemp1;

    // Helper macro to reduce repetition in expansion
    // RCON: Round Constant used in AES key schedule
#define EXPAND_ROUND(round, rcon) \
        xmmTemp2 = _mm_aeskeygenassist_si128(pKeySchedule[round-1], rcon); \
        xmmTemp2 = _mm_shuffle_epi32(xmmTemp2, _MM_SHUFFLE(3, 3, 3, 3)); \
        xmmTemp1 = _mm_xor_si128(pKeySchedule[round-1], _mm_slli_si128(pKeySchedule[round-1], AES_SHIFT_VAL)); \
        xmmTemp1 = _mm_xor_si128(xmmTemp1, _mm_slli_si128(xmmTemp1, AES_SHIFT_VAL)); \
        xmmTemp1 = _mm_xor_si128(xmmTemp1, _mm_slli_si128(xmmTemp1, AES_SHIFT_VAL)); \
        pKeySchedule[round] = _mm_xor_si128(xmmTemp1, xmmTemp2);

    EXPAND_ROUND(1, 0x01);
    EXPAND_ROUND(2, 0x02);
    EXPAND_ROUND(3, 0x04);
    EXPAND_ROUND(4, 0x08);
    EXPAND_ROUND(5, 0x10);
    EXPAND_ROUND(6, 0x20);
    EXPAND_ROUND(7, 0x40);
    EXPAND_ROUND(8, 0x80);
    EXPAND_ROUND(9, 0x1B);
    EXPAND_ROUND(10, 0x36);

#undef EXPAND_ROUND
}

void Aes128CtrCrypt(IN OUT unsigned char* pBuffer, IN unsigned __int64 qwBufferSize, IN unsigned char* pAesKey, IN unsigned char* pAesIv, IN unsigned __int64 qwFileOffset)
{
    __m128i xmmKeySchedule[AES_KEY_SCHEDULE];
    Aes128KeyExpansion(pAesKey, xmmKeySchedule);

    __m128i xmmBaseCtr = _mm_loadu_si128((const __m128i*)pAesIv);
    unsigned __int64 uBlockOffset = BYTES_TO_AES_BLOCKS(qwFileOffset);
    unsigned __int64 uIndex = 0;

    // Full block loop — no branch inside, runs the hot path
    for (; uIndex + AES_BLOCK_SIZE <= qwBufferSize; uIndex += AES_BLOCK_SIZE)
    {
        unsigned __int64 uCurrentBlock = uBlockOffset + BYTES_TO_AES_BLOCKS(uIndex);

        __m128i xmmCtr = _mm_insert_epi64(xmmBaseCtr, _mm_extract_epi64(xmmBaseCtr, 0) + uCurrentBlock, 0);

        __m128i xmmKeystream = _mm_xor_si128(xmmCtr, xmmKeySchedule[0]);

        for (int iRound = 1; iRound < AES_ROUNDS; ++iRound)
            xmmKeystream = _mm_aesenc_si128(xmmKeystream, xmmKeySchedule[iRound]);

        xmmKeystream = _mm_aesenclast_si128(xmmKeystream, xmmKeySchedule[AES_ROUNDS]);

        __m128i xmmData = _mm_loadu_si128((const __m128i*)(pBuffer + uIndex));
        _mm_storeu_si128((__m128i*)(pBuffer + uIndex), _mm_xor_si128(xmmData, xmmKeystream));
    }

    // Tail block (partial block at end of buffer if any)
    if (uIndex < qwBufferSize)
    {
        unsigned __int64 uCurrentBlock = uBlockOffset + BYTES_TO_AES_BLOCKS(uIndex);
        unsigned int     uBytesLeft = (unsigned int)(qwBufferSize - uIndex);
        unsigned char    u8KeystreamBuf[AES_BLOCK_SIZE];

        __m128i xmmCtr = _mm_insert_epi64(xmmBaseCtr, _mm_extract_epi64(xmmBaseCtr, 0) + uCurrentBlock, 0);

        __m128i xmmKeystream = _mm_xor_si128(xmmCtr, xmmKeySchedule[0]);

        for (int iRound = 1; iRound < AES_ROUNDS; ++iRound)
            xmmKeystream = _mm_aesenc_si128(xmmKeystream, xmmKeySchedule[iRound]);

        xmmKeystream = _mm_aesenclast_si128(xmmKeystream, xmmKeySchedule[AES_ROUNDS]);
        _mm_storeu_si128((__m128i*)u8KeystreamBuf, xmmKeystream);

        for (unsigned int j = 0; j < uBytesLeft; ++j)
        {
            pBuffer[uIndex + j] ^= u8KeystreamBuf[j];
        }
    }
}