#ifndef _GET_FAVORITE_ACCOUNTS_H_
#define _GET_FAVORITE_ACCOUNTS_H_

#include "shared_context.h"
#include "common_utils.h"

/*********************
 *      DEFINES
 *********************/
/**
 *  @brief Length of the Account chain_id
 */
#define ACCOUNT_CHAIN_ID_LENGTH (sizeof(uint64_t))

/**
 *  @brief Min length of the Account name
 */
#define ACCOUNT_NAME_MIN_LENGTH (5)

/**
 *  @brief Max length of the Account name (excluding '\0')
 */
#define ACCOUNT_NAME_MAX_LENGTH (17)

/**
 *  @brief Max length of the Account address (including '0x' and '\0')
 */
#define ACCOUNT_ADDR_MAX_LENGTH ((ADDRESS_LENGTH + 3) * 2)  // Adding '0x' + '\0'

/**
 *  @brief Max number of managed Accounts
 */
#define ACCOUNTS_MAX (5)

/**
 *  @brief Min length of a Add/Get buffer
 */
#define ACCOUNTS_MIN_APDU_LENGTH \
    (ACCOUNT_CHAIN_ID_LENGTH + ADDRESS_LENGTH + ACCOUNT_NAME_MIN_LENGTH)
/**
 *  @brief Max length of a Add/Get buffer
 */
#define ACCOUNTS_MAX_APDU_LENGTH \
    (ACCOUNT_CHAIN_ID_LENGTH + ADDRESS_LENGTH + ACCOUNT_NAME_MAX_LENGTH)

/**
 *  @brief Invalid Account index in Address Book, or No Accounts
 */
#define INVALID_ACCOUNT_INDEX 0xFF
/**********************
 *      TYPEDEFS
 **********************/

/**
 * @brief This structure contains the individual Account data.
 *
 */
typedef struct AccountData_s {
    uint64_t chain_id;                       ///< Value representing the targetted network
    char name[ACCOUNT_NAME_MAX_LENGTH + 1];  ///< Account user-friendly name (@ref
                                             ///< ACCOUNT_NAME_MIN_LENGTH to @ref
                                             ///< ACCOUNT_NAME_MAX_LENGTH characters)
    char address[ACCOUNT_ADDR_MAX_LENGTH];   ///< Account address (@ref ACCOUNT_ADDR_MAX_LENGTH max
                                             ///< characters)
} AccountData_t;

/**
 * @brief This structure contains the whole favorite accounts data.
 *
 */
typedef struct AccountStorage_s {
    AccountData_t accounts[ACCOUNTS_MAX];  ///< Accounts individual data
} AccountStorage_t;

/**
 * @brief This structure contains the temporary favorite accounts data update.
 *
 */
typedef struct AccountUpdate_s {
    AccountData_t accounts[ACCOUNTS_MAX];  ///< Accounts individual data
    uint8_t existIndex[ACCOUNTS_MAX];      ///< Indicate if account already exist
    uint8_t nbAccounts;                    ///< Indicate current account index
} AccountUpdate_t;

/**********************
 *  GLOBAL VARIABLES
 **********************/
extern const AccountStorage_t N_accounts_real;
extern AccountUpdate_t accountsUpdate;

/**********************
 *      MACROS
 **********************/
#define N_accounts (*(volatile AccountStorage_t *) PIC(&N_accounts_real))

/**********************
 * GLOBAL PROTOTYPES
 **********************/

extern void dumpAllAccounts(void);
extern void dumpAccount(AccountData_t *account);

extern uint8_t getNbFavoriteAccounts(void);
extern void deleteFavoriteAccounts(void);
extern void handleFavoriteAccounts(uint8_t p1, uint8_t p2, const uint8_t *buffer, uint8_t length);

#endif  // _GET_FAVORITE_ACCOUNTS_H_
