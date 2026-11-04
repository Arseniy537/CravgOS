#include <stdio.h>
#include <string.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32;
#include "../crypto.h"
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while(0)
static int unhex(const char *h, u8 *o) { int n = 0; for (; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); o[n++] = v; } return n; }
static int eqhex(const u8 *a, const char *h) { u8 b[256]; int n = unhex(h, b); return memcmp(a, b, n) == 0; }
int main(void) {
    u8 d[32]; struct sha256 c;
    sha256_init(&c); sha256_update(&c, (const u8 *)"abc", 3); sha256_final(&c, d);
    CHECK(eqhex(d, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    u8 big[1000]; memset(big, 'a', sizeof big);
    sha256_init(&c); for (int i = 0; i < 1000; i++) sha256_update(&c, big, 1000); sha256_final(&c, d);       /* миллион 'a' */
    CHECK(eqhex(d, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    sha256_init(&c); sha256_update(&c, (const u8 *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56); sha256_final(&c, d);
    CHECK(eqhex(d, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    /* HMAC RFC 4231, тест 2 */
    hmac_sha256((const u8 *)"Jefe", 4, (const u8 *)"what do ya want for nothing?", 28, d);
    CHECK(eqhex(d, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    u8 key[131]; memset(key, 0xaa, sizeof key);          /* тест 6: ключ длиннее блока */
    hmac_sha256(key, 131, (const u8 *)"Test Using Larger Than Block-Size Key - Hash Key First", 54, d);
    CHECK(eqhex(d, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
    /* AES-128, FIPS-197 */
    struct aes a; u8 k[16], pt[16], ct[16];
    unhex("000102030405060708090a0b0c0d0e0f", k); unhex("00112233445566778899aabbccddeeff", pt);
    aes_set_key(&a, k); aes_enc(&a, pt, ct);
    CHECK(eqhex(ct, "69c4e0d86a7b0430d8cdb78070b4c55a"));
    /* AES-GCM, тест-кейс 4 из спецификации GCM */
    u8 gk[16], iv[12], aad[20], p[60], out[60], tag[16];
    unhex("feffe9928665731c6d6a8f9467308308", gk); unhex("cafebabefacedbaddecaf888", iv);
    unhex("feedfacedeadbeeffeedfacedeadbeefabaddad2", aad);
    unhex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39", p);
    aes_set_key(&a, gk); gcm_crypt(&a, iv, aad, 20, p, 60, out, tag, 0);
    CHECK(eqhex(out, "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091"));
    CHECK(eqhex(tag, "5bc94fbc3221a5db94fae95ae7121a47"));
    u8 back[60], tag2[16];
    gcm_crypt(&a, iv, aad, 20, out, 60, back, tag2, 1);
    CHECK(memcmp(back, p, 60) == 0 && memcmp(tag2, tag, 16) == 0);
    gcm_crypt(&a, iv, aad, 20, out, 60, out, tag2, 1);                 /* на месте */
    CHECK(memcmp(out, p, 60) == 0);
    /* GCM: пустое сообщение и нулевой ключ (тест-кейс 1) */
    u8 z16[16] = {0}, z12[12] = {0};
    aes_set_key(&a, z16); gcm_crypt(&a, z12, 0, 0, 0, 0, 0, tag, 0);
    CHECK(eqhex(tag, "58e2fccefa7e3061367f1d57a4e7455a"));
    /* X25519, RFC 7748 */
    u8 s[32], u[32], r[32];
    unhex("a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4", s);
    unhex("e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c", u);
    x25519(r, s, u);
    CHECK(eqhex(r, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552"));
    u8 as[32], bs[32], base[32] = {9}, apub[32], bpub[32], k1[32], k2[32];       /* Диффи-Хеллман RFC 7748 6.1 */
    unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", as);
    unhex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb", bs);
    x25519(apub, as, base); x25519(bpub, bs, base);
    CHECK(eqhex(apub, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"));
    CHECK(eqhex(bpub, "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f"));
    x25519(k1, as, bpub); x25519(k2, bs, apub);
    CHECK(memcmp(k1, k2, 32) == 0 && eqhex(k1, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742"));
    /* PRF: самосогласованность с Python (проверяется в test_tls.py), здесь - детерминированность и длина */
    u8 o1[100], o2[100];
    tls_prf((const u8 *)"secret", 6, "test label", (const u8 *)"seed", 4, o1, 100);
    tls_prf((const u8 *)"secret", 6, "test label", (const u8 *)"seed", 4, o2, 40);
    CHECK(memcmp(o1, o2, 40) == 0);
    printf("crypto tests OK (sha256, hmac, aes, gcm, x25519)\n");
    return 0;
}
