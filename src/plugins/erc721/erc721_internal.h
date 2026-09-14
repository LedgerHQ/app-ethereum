#pragma once

typedef enum {
    APPROVE,
    SET_APPROVAL_FOR_ALL,
    TRANSFER,
    SAFE_TRANSFER,
    SAFE_TRANSFER_DATA,
} erc721_selector_t;

typedef enum {
    FROM,
    TO,
    // safeTransferFrom(address,address,uint256,bytes) only: the dynamic offset
    // of the receiver payload, then its length. Replaces a DATA state that was
    // declared but never reached, leaving the payload unparsed.
    DATA_OFFSET,
    DATA_LENGTH,
    TOKEN_ID,
    OPERATOR,
    APPROVED,
    NONE,
} erc721_selector_field;
