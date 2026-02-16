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
    DATA,
    TOKEN_ID,
    OPERATOR,
    APPROVED,
    NONE,
} erc721_selector_field;
