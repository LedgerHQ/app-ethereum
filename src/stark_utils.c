#ifdef HAVE_STARKWARE

#include "stark_crypto.h"
#include "shared_context.h"
#include "ethUtils.h"
#include "uint256.h"
#include "uint_common.h"

#include "os_io_seproxyhal.h"

#define SIGNATURE_MAX_LEN (72)

static const ECPoint PEDERSEN_SHIFT[] = {{
    0x04,

    0x04, 0x9e, 0xe3, 0xeb, 0xa8, 0xc1, 0x60, 0x07, 0x00, 0xee, 0x1b, 0x87, 0xeb, 0x59, 0x9f, 0x16,
    0x71, 0x6b, 0x0b, 0x10, 0x22, 0x94, 0x77, 0x33, 0x55, 0x1f, 0xde, 0x40, 0x50, 0xca, 0x68, 0x04,

    0x03, 0xca, 0x0c, 0xfe, 0x4b, 0x3b, 0xc6, 0xdd, 0xf3, 0x46, 0xd4, 0x9d, 0x06, 0xea, 0x0e, 0xd3,
    0x4e, 0x62, 0x10, 0x62, 0xc0, 0xe0, 0x56, 0xc1, 0xd0, 0x40, 0x5d, 0x26, 0x6e, 0x10, 0x26, 0x8a,
}};

static const ECPoint PEDERSEN_POINTS[4] = {
    {
        0x04,

        0x02, 0x34, 0x28, 0x7d, 0xcb, 0xaf, 0xfe, 0x7f, 0x96, 0x9c, 0x74,
        0x86, 0x55, 0xfc, 0xa9, 0xe5, 0x8f, 0xa8, 0x12, 0x0b, 0x6d, 0x56,
        0xeb, 0x0c, 0x10, 0x80, 0xd1, 0x79, 0x57, 0xeb, 0xe4, 0x7b,

        0x03, 0xb0, 0x56, 0xf1, 0x00, 0xf9, 0x6f, 0xb2, 0x1e, 0x88, 0x95,
        0x27, 0xd4, 0x1f, 0x4e, 0x39, 0x94, 0x01, 0x35, 0xdd, 0x7a, 0x6c,
        0x94, 0xcc, 0x6e, 0xd0, 0x26, 0x8e, 0xe8, 0x9e, 0x56, 0x15,
    },
    {
        0x04,

        0x04, 0xfa, 0x56, 0xf3, 0x76, 0xc8, 0x3d, 0xb3, 0x3f, 0x9d, 0xab,
        0x26, 0x56, 0x55, 0x8f, 0x33, 0x99, 0x09, 0x9e, 0xc1, 0xde, 0x5e,
        0x30, 0x18, 0xb7, 0xa6, 0x93, 0x2d, 0xba, 0x8a, 0xa3, 0x78,

        0x03, 0xfa, 0x09, 0x84, 0xc9, 0x31, 0xc9, 0xe3, 0x81, 0x13, 0xe0,
        0xc0, 0xe4, 0x7e, 0x44, 0x01, 0x56, 0x27, 0x61, 0xf9, 0x2a, 0x7a,
        0x23, 0xb4, 0x51, 0x68, 0xf4, 0xe8, 0x0f, 0xf5, 0xb5, 0x4d,
    },
    {
        0x04,

        0x04, 0xba, 0x4c, 0xc1, 0x66, 0xbe, 0x8d, 0xec, 0x76, 0x49, 0x10,
        0xf7, 0x5b, 0x45, 0xf7, 0x4b, 0x40, 0xc6, 0x90, 0xc7, 0x47, 0x09,
        0xe9, 0x0f, 0x3a, 0xa3, 0x72, 0xf0, 0xbd, 0x2d, 0x69, 0x97,

        0x00, 0x40, 0x30, 0x1c, 0xf5, 0xc1, 0x75, 0x1f, 0x4b, 0x97, 0x1e,
        0x46, 0xc4, 0xed, 0xe8, 0x5f, 0xca, 0xc5, 0xc5, 0x9a, 0x5c, 0xe5,
        0xae, 0x7c, 0x48, 0x15, 0x1f, 0x27, 0xb2, 0x4b, 0x21, 0x9c,
    },
    {
        0x04,

        0x05, 0x43, 0x02, 0xdc, 0xb0, 0xe6, 0xcc, 0x1c, 0x6e, 0x44, 0xcc,
        0xa8, 0xf6, 0x1a, 0x63, 0xbb, 0x2c, 0xa6, 0x50, 0x48, 0xd5, 0x3f,
        0xb3, 0x25, 0xd3, 0x6f, 0xf1, 0x2c, 0x49, 0xa5, 0x82, 0x02,

        0x01, 0xb7, 0x7b, 0x3e, 0x37, 0xd1, 0x35, 0x04, 0xb3, 0x48, 0x04,
        0x62, 0x68, 0xd8, 0xae, 0x25, 0xce, 0x98, 0xad, 0x78, 0x3c, 0x25,
        0x56, 0x1a, 0x87, 0x9d, 0xcc, 0x77, 0xe9, 0x9c, 0x24, 0x26,
    }};

void accum_ec_mul(ECPoint *hash, uint8_t *buf, int len, int pedersen_idx) {
    ECPoint tmp;
    if (!allzeroes(buf, len)) {
        uint8_t pad[32];
        memcpy(tmp, PEDERSEN_POINTS[pedersen_idx], sizeof(ECPoint));
        io_seproxyhal_io_heartbeat();
        memset(pad, 0, sizeof(pad));
        memmove(pad + 32 - len, buf, len);
        cx_ecfp_scalar_mult(CX_CURVE_Stark256, tmp, sizeof(ECPoint), pad, sizeof(pad));
        io_seproxyhal_io_heartbeat();
        cx_ecfp_add_point(CX_CURVE_Stark256, *hash, *hash, tmp, sizeof(ECPoint));
    }
}

void pedersen(FieldElement res, /* out */
              FieldElement a,
              FieldElement b) {
    ECPoint hash;

    memcpy(hash, PEDERSEN_SHIFT, sizeof(hash));

    accum_ec_mul(&hash, a, 1, 1);
    accum_ec_mul(&hash, a + 1, FIELD_ELEMENT_SIZE - 1, 0);
    accum_ec_mul(&hash, b, 1, 3);
    accum_ec_mul(&hash, b + 1, FIELD_ELEMENT_SIZE - 1, 2);

    memcpy(res, hash + 1, FIELD_ELEMENT_SIZE);
}

void shift_stark_hash(FieldElement hash) {
    uint256_t hash256, final_hash256;
    readu256BE(hash, &hash256);
    uint32_t bits_count = bits256(&hash256);
    if (bits_count < 248) {
        return;
    } else if (bits_count >= 248 && bits_count % 8 >= 1 && bits_count % 8 <= 4) {
        shiftl256(&hash256, 4, &final_hash256);
        write_u64_be(hash, UPPER(UPPER_P((&final_hash256))));
        write_u64_be(hash + 8, LOWER(UPPER_P((&final_hash256))));
        write_u64_be(hash + 16, UPPER(LOWER_P((&final_hash256))));
        write_u64_be(hash + 24, LOWER(LOWER_P((&final_hash256))));
        return;
    } else {
        THROW(0x6A80);
    }
}

int stark_sign(uint8_t *signature, /* out */
               uint8_t *privateKeyData,
               FieldElement token1,
               FieldElement token2,
               FieldElement msg,
               FieldElement condition) {
    unsigned int info = 0;
    FieldElement hash;
    cx_ecfp_private_key_t privateKey;
    PRINTF("Stark sign msg w1 %.*H\n", 32, token1);
    PRINTF("Stark sign msg w2 %.*H\n", 32, token2);
    PRINTF("Stark sign w3 %.*H\n", 32, msg);
    if (condition != NULL) {
        PRINTF("Stark sign w4 %.*H\n", 32, condition);
    }
    pedersen(hash, token1, token2);
    PRINTF("Pedersen hash 1 %.*H\n", 32, hash);
    if (condition != NULL) {
        pedersen(hash, hash, condition);
        PRINTF("Pedersen hash condition %.*H\n", 32, hash);
    }
    pedersen(hash, hash, msg);
    PRINTF("Pedersen hash 2 %.*H\n", 32, hash);
    shift_stark_hash(hash);
    cx_ecfp_init_private_key(CX_CURVE_Stark256, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    int signatureLength = cx_ecdsa_sign(&privateKey,
                                        CX_RND_RFC6979 | CX_LAST,
                                        CX_SHA256,
                                        hash,
                                        sizeof(hash),
                                        signature,
                                        SIGNATURE_MAX_LEN,
                                        &info);
    PRINTF("Stark signature %.*H\n", signatureLength, signature);
    return signatureLength;
}

// ERC20Token(address)
static const uint8_t ERC20_SELECTOR[] = {0xf4, 0x72, 0x61, 0xb0};
// ETH()
static const uint8_t ETH_SELECTOR[] = {0x83, 0x22, 0xff, 0xf2};
// ERC721Token(address, uint256)
static const uint8_t ERC721_SELECTOR[] = {0x02, 0x57, 0x17, 0x92};
// MintableERC20Token(address)
static const uint8_t MINTABLE_ERC20_SELECTOR[] = {0x68, 0x64, 0x6e, 0x2d};
// MintableERC721Token(address,uint256)
static const uint8_t MINTABLE_ERC721_SELECTOR[] = {0xb8, 0xb8, 0x66, 0x72};
static const char NFT_ASSET_ID_PREFIX[] = {'N', 'F', 'T', ':', 0};
static const char MINTABLE_ASSET_ID_PREFIX[] = {'M', 'I', 'N', 'T', 'A', 'B', 'L', 'E', ':', 0};

void compute_token_id(cx_sha3_t *sha3,
                      uint8_t *contractAddress,
                      uint8_t quantumType,
                      uint8_t *quantum,
                      uint8_t *mintingBlob,
                      bool assetTypeOnly,
                      uint8_t *output) {
    uint8_t tmp[36];
    cx_keccak_init(sha3, 256);
    if ((contractAddress != NULL) && (!allzeroes(contractAddress, 20))) {
        const uint8_t *selector = NULL;
        switch (quantumType) {
            case STARK_QUANTUM_ERC20:
            case STARK_QUANTUM_LEGACY:
                selector = ERC20_SELECTOR;
                break;
            case STARK_QUANTUM_ERC721:
                selector = ERC721_SELECTOR;
                break;
            case STARK_QUANTUM_MINTABLE_ERC20:
                selector = MINTABLE_ERC20_SELECTOR;
                break;
            case STARK_QUANTUM_MINTABLE_ERC721:
                selector = MINTABLE_ERC721_SELECTOR;
                break;
            default:
                PRINTF("Unsupported quantum type %d\n", quantumType);
                return;
        }
        PRINTF("compute_token_id for %.*H\n", 20, contractAddress);
        memset(tmp, 0, sizeof(tmp));
        memmove(tmp, selector, 4);
        memmove(tmp + 16, contractAddress, 20);
        cx_hash((cx_hash_t *) sha3, 0, tmp, sizeof(tmp), NULL, 0);
    } else {
        PRINTF("compute_token_id for ETH\n");
        cx_hash((cx_hash_t *) sha3, 0, ETH_SELECTOR, sizeof(ETH_SELECTOR), NULL, 0);
    }
    if ((quantumType == STARK_QUANTUM_ERC721) || (quantumType == STARK_QUANTUM_MINTABLE_ERC721)) {
        memset(tmp, 0, 32);
        tmp[31] = 1;
        PRINTF("compute_token_id quantum %.*H\n", 32, tmp);
        cx_hash((cx_hash_t *) sha3, CX_LAST, tmp, 32, output, 32);
    } else {
        PRINTF("compute_token_id quantum %.*H\n", 32, quantum);
        cx_hash((cx_hash_t *) sha3, CX_LAST, quantum, 32, output, 32);
    }
    if (!assetTypeOnly &&
        ((quantumType != STARK_QUANTUM_LEGACY) && (quantumType != STARK_QUANTUM_ETH) &&
         (quantumType != STARK_QUANTUM_ERC20))) {
        const char *prefix = NULL;
        output[0] &= 0x03;
        cx_keccak_init(sha3, 256);
        switch (quantumType) {
            case STARK_QUANTUM_ERC721:
                prefix = NFT_ASSET_ID_PREFIX;
                break;
            case STARK_QUANTUM_MINTABLE_ERC20:
            case STARK_QUANTUM_MINTABLE_ERC721:
                prefix = MINTABLE_ASSET_ID_PREFIX;
                break;
            default:
                PRINTF("Unsupported non default quantum type %d\n", quantumType);
                return;
        }
        cx_hash((cx_hash_t *) sha3, 0, (const uint8_t *) prefix, strlen(prefix), NULL, 0);
        cx_hash((cx_hash_t *) sha3, 0, output, 32, NULL, 0);
        cx_hash((cx_hash_t *) sha3, CX_LAST, mintingBlob, 32, output, 32);
    }
    if (!assetTypeOnly && ((quantumType == STARK_QUANTUM_MINTABLE_ERC20) ||
                           (quantumType == STARK_QUANTUM_MINTABLE_ERC721))) {
        output[0] = 0x04;
        output[1] = 0x00;
    } else {
        output[0] &= 0x03;
    }
    PRINTF("compute_token_id computed token %.*H\n", 32, output);
}

#endif  // HAVE_STARK
