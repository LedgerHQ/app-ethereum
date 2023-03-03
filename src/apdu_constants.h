#ifndef _APDU_CONSTANTS_H_
#define _APDU_CONSTANTS_H_

#include "shared_context.h"

#define APP_FLAG_DATA_ALLOWED          0x01
#define APP_FLAG_EXTERNAL_TOKEN_NEEDED 0x02
#define APP_FLAG_STARKWARE             0x04
#define APP_FLAG_STARKWARE_V2          0x08

#define CLA                                 0xE0
#define INS_GET_PUBLIC_KEY                  0x02
#define INS_SIGN                            0x04
#define INS_GET_APP_CONFIGURATION           0x06
#define INS_SIGN_PERSONAL_MESSAGE           0x08
#define INS_PROVIDE_ERC20_TOKEN_INFORMATION 0x0A
#define INS_SIGN_EIP_712_MESSAGE            0x0C
#define INS_GET_ETH2_PUBLIC_KEY             0x0E
#define INS_SET_ETH2_WITHDRAWAL_INDEX       0x10
#define INS_SET_EXTERNAL_PLUGIN             0x12
#define INS_PROVIDE_NFT_INFORMATION         0x14
#define INS_SET_PLUGIN                      0x16
#define INS_PERFORM_PRIVACY_OPERATION       0x18
#define INS_EIP712_STRUCT_DEF               0x1A
#define INS_EIP712_STRUCT_IMPL              0x1C
#define INS_EIP712_FILTERING                0x1E
#define INS_ENS_GET_CHALLENGE               0x20
#define INS_ENS_PROVIDE_INFO                0x22
#define P1_CONFIRM                          0x01
#define P1_NON_CONFIRM                      0x00
#define P2_NO_CHAINCODE                     0x00
#define P2_CHAINCODE                        0x01
#define P1_FIRST                            0x00
#define P1_MORE                             0x80
#define P2_EIP712_LEGACY_IMPLEM             0x00
#define P2_EIP712_FULL_IMPLEM               0x01

#define COMMON_CLA               0xB0
#define COMMON_INS_GET_WALLET_ID 0x04

#define APDU_RESPONSE_OK                      0x9000
#define APDU_RESPONSE_ERROR_NO_INFO           0x6a00
#define APDU_RESPONSE_INVALID_DATA            0x6a80
#define APDU_RESPONSE_INSUFFICIENT_MEMORY     0x6a84
#define APDU_RESPONSE_INVALID_INS             0x6d00
#define APDU_RESPONSE_INVALID_P1_P2           0x6b00
#define APDU_RESPONSE_CONDITION_NOT_SATISFIED 0x6985
#define APDU_RESPONSE_REF_DATA_NOT_FOUND      0x6a88

#ifdef HAVE_STARKWARE

#define STARKWARE_CLA                 0xF0
#define STARKWARE_INS_GET_PUBLIC_KEY  0x02
#define STARKWARE_INS_SIGN_MESSAGE    0x04
#define STARKWARE_INS_PROVIDE_QUANTUM 0x08
#define STARKWARE_INS_UNSAFE_SIGN     0x0A

#define P1_STARK_ORDER                0x01
#define P1_STARK_TRANSFER             0x02
#define P1_STARK_ORDER_V2             0x03
#define P1_STARK_TRANSFER_V2          0x04
#define P1_STARK_CONDITIONAL_TRANSFER 0x05

#define STARK_ORDER_TYPE                0
#define STARK_TRANSFER_TYPE             1
#define STARK_CONDITIONAL_TRANSFER_TYPE 2

#endif

enum { OFFSET_CLA = 0, OFFSET_INS, OFFSET_P1, OFFSET_P2, OFFSET_LC, OFFSET_CDATA };

#define ERR_APDU_EMPTY         0x6982
#define ERR_APDU_SIZE_MISMATCH 0x6983

void handleGetPublicKey(uint8_t p1,
                        uint8_t p2,
                        const uint8_t *dataBuffer,
                        uint8_t dataLength,
                        unsigned int *flags,
                        unsigned int *tx);
void handleProvideErc20TokenInformation(uint8_t p1,
                                        uint8_t p2,
                                        const uint8_t *workBuffer,
                                        uint8_t dataLength,
                                        unsigned int *flags,
                                        unsigned int *tx);
void handleProvideNFTInformation(uint8_t p1,
                                 uint8_t p2,
                                 const uint8_t *dataBuffer,
                                 uint8_t dataLength,
                                 unsigned int *flags,
                                 unsigned int *tx);
void handleSign(uint8_t p1,
                uint8_t p2,
                const uint8_t *dataBuffer,
                uint8_t dataLength,
                unsigned int *flags,
                unsigned int *tx);
void handleGetAppConfiguration(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *dataBuffer,
                               uint8_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx);
bool handleSignPersonalMessage(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *const payload,
                               uint8_t length);
void handleSignEIP712Message_v0(uint8_t p1,
                                uint8_t p2,
                                const uint8_t *dataBuffer,
                                uint8_t dataLength,
                                unsigned int *flags,
                                unsigned int *tx);

void handleSetExternalPlugin(uint8_t p1,
                             uint8_t p2,
                             const uint8_t *workBuffer,
                             uint8_t dataLength,
                             unsigned int *flags,
                             unsigned int *tx);

void handleSetPlugin(uint8_t p1,
                     uint8_t p2,
                     const uint8_t *workBuffer,
                     uint8_t dataLength,
                     unsigned int *flags,
                     unsigned int *tx);

void handlePerformPrivacyOperation(uint8_t p1,
                                   uint8_t p2,
                                   const uint8_t *workBuffer,
                                   uint8_t dataLength,
                                   unsigned int *flags,
                                   unsigned int *tx);

#ifdef HAVE_ETH2

void handleGetEth2PublicKey(uint8_t p1,
                            uint8_t p2,
                            const uint8_t *dataBuffer,
                            uint8_t dataLength,
                            unsigned int *flags,
                            unsigned int *tx);
void handleSetEth2WinthdrawalIndex(uint8_t p1,
                                   uint8_t p2,
                                   uint8_t *dataBuffer,
                                   uint8_t dataLength,
                                   unsigned int *flags,
                                   unsigned int *tx);

#endif

#ifdef HAVE_STARKWARE

void handleStarkwareGetPublicKey(uint8_t p1,
                                 uint8_t p2,
                                 const uint8_t *dataBuffer,
                                 uint8_t dataLength,
                                 unsigned int *flags,
                                 unsigned int *tx);
void handleStarkwareSignMessage(uint8_t p1,
                                uint8_t p2,
                                uint8_t *dataBuffer,
                                uint8_t dataLength,
                                unsigned int *flags,
                                unsigned int *tx);
void handleStarkwareProvideQuantum(uint8_t p1,
                                   uint8_t p2,
                                   const uint8_t *dataBuffer,
                                   uint8_t dataLength,
                                   unsigned int *flags,
                                   unsigned int *tx);
void handleStarkwareUnsafeSign(uint8_t p1,
                               uint8_t p2,
                               const uint8_t *dataBuffer,
                               uint8_t dataLength,
                               unsigned int *flags,
                               unsigned int *tx);

#endif

extern uint16_t apdu_response_code;

#endif  // _APDU_CONSTANTS_H_
