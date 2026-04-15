#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Encode a number according to RLP - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] number buffer containing the number to encode
 * @param [in] numberLength size of the number to encode in bytes
 * @param [out] output buffer to store the RLP encoded number
 * @param [in] output_size size of the output buffer in bytes
 * @return size of the RLP encoded number in bytes, or 0
 */
uint8_t rlpEncodeNumber(const uint8_t *number,
                        uint8_t numberLength,
                        uint8_t *output,
                        size_t output_size);

/**
 * @brief Get the length of a number encoded according to RLP - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] number buffer containing the number to encode, encoded as smallest big endian
 * representation
 * @param [in] numberLength size of the number to encode in bytes
 * @return size of the RLP encoded number in bytes
 */
uint8_t rlpGetEncodedNumberLength(const uint8_t *number, uint8_t numberLength);

/**
 * @brief Get the length of an unsigned 64 bits number encoded according to RLP - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] number number to encode
 * @return size of the RLP encoded number in bytes
 */
uint8_t rlpGetEncodedNumber64Length(uint64_t number);

/**
 * @brief Get smallest encoding size of a 64 bits number according to RLP - see
 * https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] number number to encode
 * @return smallest encoding size in bytes
 */
uint8_t rlpGetSmallestNumber64EncodingSize(uint64_t number);

/**
 * @brief Get the length of the header of an RLP list whose elements have a size between 0 and 255
 * bytes - see https://github.com/ethereum/wiki/wiki/RLP
 * @param [in] size size of the elements of the list in bytes
 * @param [out] output buffer to store the header of the RLP encoded list
 * @param [in] output_size size of the output buffer in bytes
 * @return size of the RLP encoded list header, or 0
 */
uint8_t rlpEncodeListHeader8(uint8_t size, uint8_t *output, size_t output_size);
