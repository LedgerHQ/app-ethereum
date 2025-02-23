#include "shared_context.h"
#include "apdu_constants.h"
#include "common_ui.h"
#include "network.h"
#include "crypto_helpers.h"
#include "commands_7702.h"
#include "shared_7702.h"
#include "rlp_encode.h"
#include "whitelist_7702.h"

#define MAGIC_7702 5

/**
 * @brief Read an input parameter passed as a length (1 byte) followed by the parameter
 * @param [in] dataBuffer buffer containing the length + parameter to read
 * @param [in] dataLength size of the input buffer
 * @param [out] output buffer to store the parameter
 * @param [in,out] output_size size of the output buffer in bytes (input), size of the copied parameter (output)
 * @return APDU_NO_RESPONSE if the parameter could be copied, or an error code
 */
uint16_t readDataLVParameter(const uint8_t *dataBuffer, uint8_t dataLength, uint8_t *output, uint8_t *outputLength) {
	uint8_t valueLength;
	if (dataLength < 1) {
		return APDU_RESPONSE_INVALID_DATA;
	}
	valueLength = *dataBuffer;
	dataBuffer++;
	dataLength--;
	if ((dataLength < valueLength) || (valueLength > *outputLength)) {
		return APDU_RESPONSE_INVALID_DATA;
	}
	memmove(output, dataBuffer, valueLength);
	*outputLength = valueLength;
	return APDU_NO_RESPONSE;
}

/**
 * @brief Encode a parameter as RLP and hash it using Keccak
 * @param [in] data parameter to hash
 * @param [in] dataLength size of the parameter to hash
 * @param [out] rlpTmp temporary buffer to store the RLP encoded parameter
 * @param [in] rlpTmpLength size of the temporary buffer to store the RLP encoded parameter
 * @return APDU_NO_RESPONSE if the parameter could be hashed, or an error code
 */
uint16_t hashRLP(const uint8_t *data, uint8_t dataLength, uint8_t *rlpTmp, uint8_t rlpTmpLength) {
	cx_err_t error = CX_INTERNAL_ERROR;
	uint8_t hashSize;

	hashSize = rlpEncodeNumber(data, dataLength, rlpTmp, rlpTmpLength);
    if (hashSize == 0) {
    	return APDU_RESPONSE_UNKNOWN;
    }
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              0,
                              rlpTmp,
                              hashSize,
                              NULL,
                              0));
    return APDU_NO_RESPONSE;
end:
	return error;
}

uint16_t handleSignEIP7702Authorization(uint8_t p1,
                                uint8_t p2,
                                const uint8_t *dataBuffer,
                                uint8_t dataLength,
                                unsigned int *flags,
                                unsigned int *tx) {
	UNUSED(p1);
	UNUSED(p2);
	UNUSED(tx);
	uint8_t delegate[20];
	uint8_t chainId[32];
	uint8_t chainIdLength = sizeof(chainId);
	uint8_t nonce[8];
	uint8_t nonceLength = sizeof(nonce);
	uint8_t rlpDataSize = 0;
	uint8_t rlpTmp[40];
	uint8_t hashSize;
	uint16_t sw;
	uint64_t chain_id64;
	cx_err_t error = CX_INTERNAL_ERROR;
	cx_ecfp_public_key_t publicKey;
	uint64_t nonce64;
	const char *networkName;
#ifdef HAVE_EIP7702_WHITELIST
	const char* delegateName;
#endif

	// Parse all input parameters
	// * BIP 32 Path
	dataBuffer = parseBip32(dataBuffer, &dataLength, &tmpCtx.authSigningContext7702.bip32);
	if (dataBuffer == NULL) {
		return APDU_RESPONSE_INVALID_DATA;
	}
	// * Delegate address
	if (dataLength < sizeof(delegate)) {
		return APDU_RESPONSE_INVALID_DATA;
	}
	memmove(delegate, dataBuffer, sizeof(delegate));
	dataBuffer += sizeof(delegate);
	dataLength -= sizeof(delegate);
	// * ChainId
	sw = readDataLVParameter(dataBuffer, dataLength, chainId, &chainIdLength);
	if (sw != APDU_NO_RESPONSE) {
		return sw;
	}
	dataBuffer += chainIdLength + 1;
	dataLength -= chainIdLength + 1;
	// * Nonce
	sw = readDataLVParameter(dataBuffer, dataLength, nonce, &nonceLength);
	if (sw != APDU_NO_RESPONSE) {
		return sw;
	}
	dataLength -= nonceLength + 1;
	if (dataLength != 0) {
		return APDU_RESPONSE_INVALID_DATA;
	}
	// Compute the authorization hash
 	// keccak(MAGIC || rlp([chain_id, address, nonce]))
 	// * Compute the size of the RLP list data
	rlpDataSize += rlpGetEncodedNumberLength(chainId, chainIdLength);
	rlpDataSize += rlpGetEncodedNumberLength(delegate, sizeof(delegate)); // 20 bytes RLP string is encoded as a number 
	rlpDataSize += rlpGetEncodedNumberLength(nonce, nonceLength);
	// * Hash the components
	rlpTmp[0] = MAGIC_7702;
	hashSize = rlpEncodeListHeader8(rlpDataSize, rlpTmp + 1, sizeof(rlpTmp) - 1);
	if (hashSize == 0) {
		return APDU_RESPONSE_UNKNOWN;
	}
	CX_CHECK(cx_keccak_init_no_throw(&global_sha3, 256));
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              0,
                              rlpTmp,
                              hashSize + 1,
                              NULL,
                              0));
    sw = hashRLP(chainId, chainIdLength, rlpTmp, sizeof(rlpTmp));
    if (sw != APDU_NO_RESPONSE) {
    	return sw;
    }
    sw = hashRLP(delegate, sizeof(delegate), rlpTmp, sizeof(rlpTmp));
    if (sw != APDU_NO_RESPONSE) {
    	return sw;
    }
    sw = hashRLP(nonce, nonceLength, rlpTmp, sizeof(rlpTmp));
    if (sw != APDU_NO_RESPONSE) {
    	return sw;
    }
    CX_CHECK(cx_hash_no_throw((cx_hash_t *) &global_sha3,
                              CX_LAST,
                              NULL,
                              0,
                              tmpCtx.authSigningContext7702.authHash,
                              sizeof(tmpCtx.authSigningContext7702.authHash)));
    // Prepare information to be displayed
    // * Address to be delegated
    // we will be losing precision here in case of a looong chain Id (unlikely, Forge also limits to uint64)
    // also will not be correct for EIP 1191 if using wildcard chainId 0 (doesn't really matter ...)
    chain_id64 = u64_from_BE(chainId, chainIdLength);  
    CX_CHECK(bip32_derive_get_pubkey_256(
        CX_CURVE_256K1,
        tmpCtx.authSigningContext7702.bip32.path,
        tmpCtx.authSigningContext7702.bip32.length,
        publicKey.W,
        NULL,
        CX_SHA512));
    strings.common.fromAddress[0] = '0';
    strings.common.fromAddress[1] = 'x';
    getEthAddressStringFromRawKey(publicKey.W,
                                  strings.common.fromAddress + 2,
                                  chain_id64);    
    // * Delegate
#ifdef HAVE_EIP7702_WHITELIST
    // Check if the delegate is on the whitelist for this chainId
    delegateName = get_delegate_name(&chain_id64, delegate);
    if (delegateName == NULL) {
    	// If it's not in the whitelist, display the contract address if the whitelist is disabled
    	if (N_storage.eip7702_whitelist_disabled) {
		    if (!getEthDisplayableAddress(delegate, strings.common.toAddress, sizeof(strings.common.toAddress), chain_id64)) {
    			return APDU_RESPONSE_UNKNOWN;    	
    		}
    	}
    	else {
    		// Or reject the command
    		ui_error_no_7702_whitelist();
    		return APDU_RESPONSE_CONDITION_NOT_SATISFIED;
    	}
    }
    else {
    	strlcpy(strings.common.toAddress, delegateName, sizeof(strings.common.toAddress));
    }
#else    
    if (!getEthDisplayableAddress(delegate, strings.common.toAddress, sizeof(strings.common.toAddress), chain_id64)) {
    	return APDU_RESPONSE_UNKNOWN;    	
    }
#endif // HAVE_EIP7702_WHITELIST
    // * Nonce
    nonce64 = u64_from_BE(nonce, nonceLength);
    if (!u64_to_string(nonce64, strings.common.nonce, sizeof(strings.common.nonce))) {
    	//return APDU_RESPONSE_UNKNOWN;
    	// Do not crash if the nonce is too long
    	// TBD : shared_context.nonce should be 21 bytes long to support BCD 2**64
    	strings.common.nonce[0] = '?';
    	strings.common.nonce[1] = '\0';
    }
    // * ChainId
    if (chain_id64 == CHAIN_ID_ALL) {
    	// handle special wildcard case
    	strlcpy(strings.common.network_name, "All", sizeof(strings.common.network_name));
    }
    else {
    	networkName = get_network_name_from_chain_id(&chain_id64);
    	if (networkName == NULL) {
    		// Display the numeric chainId if no name was found
    		if (!u64_to_string(chain_id64, strings.common.network_name, sizeof(strings.common.network_name))) {
    			//return APDU_RESPONSE_UNKNOWN;
	 	   		// Do not crash if the chain id is too long
		    	strings.common.network_name[0] = '?';
    			strings.common.network_name[1] = '\0';
    		}
    	} else {
    		strlcpy(strings.common.network_name, networkName, sizeof(strings.common.network_name));
    	}
    }

    ui_sign_7702_auth();
    *flags |= IO_ASYNCH_REPLY;
    error = 0;

end:
	return error;
}
