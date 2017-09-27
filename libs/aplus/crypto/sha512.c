#include <aplus/base.h>
#include <aplus/utils/rotate-bits.h>
#include <aplus/crypto/sha512.h>
#include <string.h>
#include <stdio.h>

#define SHR64(a, n) ((a) >> (n))
#define ROR64(a, n) (((a) >> (n)) | ((a) << (64 - (n))))
#define CH(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define SIGMA1(x) (ROR64(x, 28) ^ ROR64(x, 34) ^ ROR64(x, 39))
#define SIGMA2(x) (ROR64(x, 14) ^ ROR64(x, 18) ^ ROR64(x, 41))
#define SIGMA3(x) (ROR64(x, 1) ^ ROR64(x, 8) ^ SHR64(x, 7))
#define SIGMA4(x) (ROR64(x, 19) ^ ROR64(x, 61) ^ SHR64(x, 6))
#define MIN(x, y) ((x) < (y) ? x : y)


static const uint8_t padding[128] =
{
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
 
static const uint64_t k[80] =
{
    0x428A2F98D728AE22LL, 0x7137449123EF65CDLL, 0xB5C0FBCFEC4D3B2FLL, 0xE9B5DBA58189DBBCLL,
    0x3956C25BF348B538LL, 0x59F111F1B605D019LL, 0x923F82A4AF194F9BLL, 0xAB1C5ED5DA6D8118LL,
    0xD807AA98A3030242LL, 0x12835B0145706FBELL, 0x243185BE4EE4B28CLL, 0x550C7DC3D5FFB4E2LL,
    0x72BE5D74F27B896FLL, 0x80DEB1FE3B1696B1LL, 0x9BDC06A725C71235LL, 0xC19BF174CF692694LL,
    0xE49B69C19EF14AD2LL, 0xEFBE4786384F25E3LL, 0x0FC19DC68B8CD5B5LL, 0x240CA1CC77AC9C65LL,
    0x2DE92C6F592B0275LL, 0x4A7484AA6EA6E483LL, 0x5CB0A9DCBD41FBD4LL, 0x76F988DA831153B5LL,
    0x983E5152EE66DFABLL, 0xA831C66D2DB43210LL, 0xB00327C898FB213FLL, 0xBF597FC7BEEF0EE4LL,
    0xC6E00BF33DA88FC2LL, 0xD5A79147930AA725LL, 0x06CA6351E003826FLL, 0x142929670A0E6E70LL,
    0x27B70A8546D22FFCLL, 0x2E1B21385C26C926LL, 0x4D2C6DFC5AC42AEDLL, 0x53380D139D95B3DFLL,
    0x650A73548BAF63DELL, 0x766A0ABB3C77B2A8LL, 0x81C2C92E47EDAEE6LL, 0x92722C851482353BLL,
    0xA2BFE8A14CF10364LL, 0xA81A664BBC423001LL, 0xC24B8B70D0F89791LL, 0xC76C51A30654BE30LL,
    0xD192E819D6EF5218LL, 0xD69906245565A910LL, 0xF40E35855771202ALL, 0x106AA07032BBD1B8LL,
    0x19A4C116B8D2D0C8LL, 0x1E376C085141AB53LL, 0x2748774CDF8EEB99LL, 0x34B0BCB5E19B48A8LL,
    0x391C0CB3C5C95A63LL, 0x4ED8AA4AE3418ACBLL, 0x5B9CCA4F7763E373LL, 0x682E6FF3D6B2B8A3LL,
    0x748F82EE5DEFB2FCLL, 0x78A5636F43172F60LL, 0x84C87814A1F0AB72LL, 0x8CC702081A6439ECLL,
    0x90BEFFFA23631E28LL, 0xA4506CEBDE82BDE9LL, 0xBEF9A3F7B2C67915LL, 0xC67178F2E372532BLL,
    0xCA273ECEEA26619CLL, 0xD186B8C721C0C207LL, 0xEADA7DD6CDE0EB1ELL, 0xF57D4F7FEE6ED178LL,
    0x06F067AA72176FBALL, 0x0A637DC5A2C898A6LL, 0x113F9804BEF90DAELL, 0x1B710B35131C471BLL,
    0x28DB77F523047D84LL, 0x32CAAB7B40C72493LL, 0x3C9EBE0A15C9BEBCLL, 0x431D67C49C100D4CLL,
    0x4CC5D4BECB3E42B6LL, 0x597F299CFC657E2ALL, 0x5FCB6FAB3AD6FAECLL, 0x6C44198C4A475817LL
};

void
sha512_init(sha512_t *ctx)
{
    ctx->h_dig.h[0] = 0x6A09E667F3BCC908LL;
    ctx->h_dig.h[1] = 0xBB67AE8584CAA73BLL;
    ctx->h_dig.h[2] = 0x3C6EF372FE94F82BLL;
    ctx->h_dig.h[3] = 0xA54FF53A5F1D36F1LL;
    ctx->h_dig.h[4] = 0x510E527FADE682D1LL;
    ctx->h_dig.h[5] = 0x9B05688C2B3E6C1FLL;
    ctx->h_dig.h[6] = 0x1F83D9ABFB41BD6BLL;
    ctx->h_dig.h[7] = 0x5BE0CD19137E2179LL;

    ctx->size =
    ctx->totalSize = 0;
}

char*
sha512(const char* data)
{
  char* out = (char*) __libaplus_malloc(SHA512_DIGEST_SIZE * 2);

  sha512_t hash;
  char buf[SHA512_DIGEST_SIZE];
  sha512_init(&hash);
  sha512_update(&hash, data, strlen(data));
  sha512_final(&hash, buf);


  int i;
  for(i = 0; i < SHA512_DIGEST_SIZE; i++)
      sprintf(&out[i * 2], "%02x", buf[i] & 0xFF);


  return out;
}

static void sha512_process(sha512_t* ctx) {
    int t;
    uint64_t temp1;
    uint64_t temp2;
 
    // Initialize the 8 working registers
    uint64_t a = ctx->h_dig.h[0];
    uint64_t b = ctx->h_dig.h[1];
    uint64_t c = ctx->h_dig.h[2];
    uint64_t d = ctx->h_dig.h[3];
    uint64_t e = ctx->h_dig.h[4];
    uint64_t f = ctx->h_dig.h[5];
    uint64_t g = ctx->h_dig.h[6];
    uint64_t h = ctx->h_dig.h[7];
 
    // Process message in 16-word blocks
    uint64_t *w = ctx->w_buf.w;
 
    // Convert from big-endian byte order to host byte order
    for (t = 0; t < 16; t++)
       w[t] = __builtin_bswap64(w[t]);

    // Prepare the message schedule
    for (t = 16; t < 80; t++)
       w[t] = SIGMA4(w[t - 2]) + w[t - 7] + SIGMA3(w[t - 15]) + w[t - 16];
 
    // SHA-512 hash computation
    for (t = 0; t < 80; t++)
    {
       // Calculate T1 and T2
       temp1 = h + SIGMA2(e) + CH(e, f, g) + k[t] + w[t];
       temp2 = SIGMA1(a) + MAJ(a, b, c);
 
       // Update the working registers
       h = g;
       g = f;
       f = e;
       e = d + temp1;
       d = c;
       c = b;
       b = a;
       a = temp1 + temp2;
    }
 
    // Update the hash value
    ctx->h_dig.h[0] += a;
    ctx->h_dig.h[1] += b;
    ctx->h_dig.h[2] += c;
    ctx->h_dig.h[3] += d;
    ctx->h_dig.h[4] += e;
    ctx->h_dig.h[5] += f;
    ctx->h_dig.h[6] += g;
    ctx->h_dig.h[7] += h;
}


void
sha512_update(sha512_t *ctx, const unsigned char *msg, size_t len)
{
    while (len > 0)
    {
        // The buffer can hold at most 128 bytes
        size_t n = MIN(len, 128 - ctx->size);
 
        // Copy the data to the buffer
        memcpy(ctx->w_buf.buffer + ctx->size, msg, n);
 
        // Update the SHA-512 ctx
        ctx->size += n;
        ctx->totalSize += n;
        // Advance the data pointer
        msg = (uint8_t *) msg + n;
        // Remaining bytes to process
        len -= n;
 
        // Process message in 16-word blocks
        if (ctx->size == 128)
        {
            // Transform the 16-word block
            sha512_process(ctx);
            // Empty the buffer
            ctx->size = 0;
        }
    }
}


void
sha512_final(sha512_t *ctx, unsigned char digest[SHA512_DIGEST_SIZE])
{
    int i;
    size_t paddingSize;
    uint64_t totalSize;
 
    // Length of the original message (before padding)
    totalSize = ctx->totalSize * 8;
 
    // Pad the message so that its length is congruent to 112 modulo 128
    paddingSize = (ctx->size < 112) ? (112 - ctx->size) : 
                                        (128 + 112 - ctx->size);
    // Append padding
    sha512_update(ctx, padding, paddingSize);
 
    // Append the length of the original message
    ctx->w_buf.w[14] = 0;
    ctx->w_buf.w[15] = __builtin_bswap64(totalSize);
 
    // Calculate the message digest
    sha512_process(ctx);
 
    // Convert from host byte order to big-endian byte order
    for (i = 0; i < 8; i++)
       ctx->h_dig.h[i] = __builtin_bswap64(ctx->h_dig.h[i]);
 
 
    memcpy(digest, ctx->h_dig.digest, SHA512_DIGEST_SIZE);
}