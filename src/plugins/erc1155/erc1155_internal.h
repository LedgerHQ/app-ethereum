#pragma once

typedef enum {
    SET_APPROVAL_FOR_ALL,
    SAFE_TRANSFER,
    SAFE_BATCH_TRANSFER,
} erc1155_selector_t;

typedef enum {
    FROM,
    TO,
    TOKEN_IDS_OFFSET,
    TOKEN_IDS_LENGTH,
    TOKEN_ID,
    VALUE_OFFSET,
    VALUE_LENGTH,
    VALUE,
    // receiver payload of both transfers: its dynamic offset, then its length
    DATA_OFFSET,
    DATA_LENGTH,
    OPERATOR,
    APPROVED,
    NONE,
} erc1155_selector_field;
