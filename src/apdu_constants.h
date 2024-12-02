#ifndef _APDU_CONSTANTS_H_
#define _APDU_CONSTANTS_H_

#include "offsets.h"
#include "shared_context.h"

#define APP_FLAG_DATA_ALLOWED          0x01
#define APP_FLAG_EXTERNAL_TOKEN_NEEDED 0x02

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
#define INS_PROVIDE_ENUM_VALUE              0x24
#define INS_GTP_TRANSACTION_INFO            0x26
#define INS_GTP_FIELD                       0x28
#define INS_PROVIDE_NETWORK_CONFIGURATION   0x30
#define P1_CONFIRM                          0x01
#define P1_NON_CONFIRM                      0x00
#define P2_NO_CHAINCODE                     0x00
#define P2_CHAINCODE                        0x01
#define P1_FIRST                            0x00
#define P1_MORE                             0x80
#define P1_FIRST_CHUNK                      0x01
#define P1_FOLLOWING_CHUNK                  0x00
#define P2_EIP712_LEGACY_IMPLEM             0x00
#define P2_EIP712_FULL_IMPLEM               0x01

#define APDU_NO_RESPONSE                      0x0000
#define APDU_RESPONSE_MODE_CHECK_FAILED       0x6001
#define APDU_RESPONSE_TX_TYPE_NOT_SUPPORTED   0x6501
#define APDU_RESPONSE_CHAINID_OUT_BUF_SMALL   0x6502
#define APDU_RESPONSE_INTERNAL_ERROR          0x6800
#define APDU_RESPONSE_SECURITY_NOT_SATISFIED  0x6982
#define APDU_RESPONSE_WRONG_DATA_LENGTH       0x6983
#define APDU_RESPONSE_PLUGIN_NOT_INSTALLED    0x6984
#define APDU_RESPONSE_CONDITION_NOT_SATISFIED 0x6985
#define APDU_RESPONSE_ERROR_NO_INFO           0x6a00
#define APDU_RESPONSE_INVALID_DATA            0x6a80
#define APDU_RESPONSE_INSUFFICIENT_MEMORY     0x6a84
#define APDU_RESPONSE_REF_DATA_NOT_FOUND      0x6a88
#define APDU_RESPONSE_INVALID_P1_P2           0x6b00
#define APDU_RESPONSE_INVALID_INS             0x6d00
#define APDU_RESPONSE_INVALID_CLA             0x6e00
#define APDU_RESPONSE_UNKNOWN                 0x6f00
#define APDU_RESPONSE_OK                      0x9000
#define APDU_RESPONSE_CMD_CODE_NOT_SUPPORTED  0x911c

uint16_t handleGetPublicKey(uint8_t p1,
                            uint8_t p2,
                            const uint8_t *dataBuffer,
                            uint8_t dataLength,
                            unsigned int *flags,
                            unsigned int *tx);
uint16_t handleProvideErc20TokenInformation(const uint8_t *workBuffer,
                                            uint8_t dataLength,
                                            unsigned int *tx);
uint16_t handleProvideNFTInformation(const uint8_t *dataBuffer,
                                     uint8_t dataLength,
                                     unsigned int *tx);
uint16_t handleSign(uint8_t p1,
                    uint8_t p2,
                    const uint8_t *dataBuffer,
                    uint8_t dataLength,
                    unsigned int *flags);
uint16_t handleGetAppConfiguration(unsigned int *tx);
uint16_t handleSignPersonalMessage(uint8_t p1,
                                   const uint8_t *const payload,
                                   uint8_t length,
                                   unsigned int *flags);
uint16_t handleSignEIP712Message_v0(uint8_t p1,
                                    const uint8_t *dataBuffer,
                                    uint8_t dataLength,
                                    unsigned int *flags);

uint16_t handleSetExternalPlugin(const uint8_t *workBuffer, uint8_t dataLength);

uint16_t handleSetPlugin(const uint8_t *workBuffer, uint8_t dataLength);

uint16_t handlePerformPrivacyOperation(uint8_t p1,
                                       uint8_t p2,
                                       const uint8_t *workBuffer,
                                       uint8_t dataLength,
                                       unsigned int *flags,
                                       unsigned int *tx);

#ifdef HAVE_ETH2

uint16_t handleGetEth2PublicKey(uint8_t p1,
                                uint8_t p2,
                                const uint8_t *dataBuffer,
                                uint8_t dataLength,
                                unsigned int *flags,
                                unsigned int *tx);

#endif

extern uint16_t apdu_response_code;

#endif  // _APDU_CONSTANTS_H_
