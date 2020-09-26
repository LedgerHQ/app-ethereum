#include "shared_context.h"

#define APP_FLAG_DATA_ALLOWED 0x01
#define APP_FLAG_EXTERNAL_TOKEN_NEEDED 0x02
#define APP_FLAG_STARKWARE 0x04

#define CLA 0xE0
#define INS_GET_PUBLIC_KEY 0x02
#define INS_SIGN 0x04
#define INS_GET_APP_CONFIGURATION 0x06
#define INS_SIGN_PERSONAL_MESSAGE 0x08
#define INS_PROVIDE_ERC20_TOKEN_INFORMATION 0x0A
#define INS_SIGN_EIP_712_MESSAGE 0x0C
#define P1_CONFIRM 0x01
#define P1_NON_CONFIRM 0x00
#define P2_NO_CHAINCODE 0x00
#define P2_CHAINCODE 0x01
#define P1_FIRST 0x00
#define P1_MORE 0x80

#define COMMON_CLA 0xB0
#define COMMON_INS_GET_WALLET_ID 0x04

#ifdef HAVE_STARKWARE

#define STARKWARE_CLA 0xF0
#define STARKWARE_INS_GET_PUBLIC_KEY 0x02
#define STARKWARE_INS_SIGN_MESSAGE 0x04
#define STARKWARE_INS_PROVIDE_QUANTUM 0x08

#define P1_STARK_ORDER 0x01
#define P1_STARK_TRANSFER 0x02

#define STARK_ORDER_TYPE 0
#define STARK_TRANSFER_TYPE 1

#endif

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

void handleGetPublicKey(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);
void handleProvideErc20TokenInformation(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);
void handleSign(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);
void handleGetAppConfiguration(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);
void handleSignPersonalMessage(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);

#ifdef HAVE_STARKWARE

void handleStarkwareGetPublicKey(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);
void handleStarkwareSignMessage(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);
void handleStarkwareProvideQuantum(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx);

#endif

