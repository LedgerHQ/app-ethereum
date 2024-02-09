#include "shared_context.h"
#include "apdu_constants.h"
#include "feature_favoriteAccounts.h"

/*********************
 *      DEFINES
 *********************/
/**********************
 *      TYPEDEFS
 **********************/
/**********************
 *  GLOBAL VARIABLES
 **********************/
const AccountStorage_t N_accounts_real;
AccountUpdate_t accountsUpdate;

/**********************
 *  STATIC VARIABLES
 **********************/
/**********************
 *  STATIC FUNCTIONS
 **********************/

/**
 * @brief Used to send the Status Word
 *
 * @param sw Status Word value
 * @param tx Status Word offset in the result buffer
 *
 * @note TODO: Put such function in a global file to be widely used, and change the exception
 * handling
 */
static void returnResult(uint16_t sw, uint8_t tx) {
    G_io_apdu_buffer[tx] = (sw >> 8) & 0xFF;
    G_io_apdu_buffer[tx + 1] = sw & 0xFF;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx + 2);
}

/**
 * @brief Used to send back the APDU buffer
 *
 * @param buffer Received APDU buffer
 * @param length APDU buffer length
 * @param sw Status Word value
 *
 * @note TODO: Put such function in a global file to be widely used, and change the exception
 * handling
 */
static void returnBuffer(const uint8_t *buffer, uint8_t length, uint16_t sw) {
    memmove(G_io_apdu_buffer, buffer, length);

    returnResult(sw, length);
}

/**
 * @brief Cleanup the temporary update context and throw an error
 *
 * @param sw Status Word value
 */
static void throwError(uint16_t sw) {
    // Reset the update tmp storage
    memset(&accountsUpdate, 0, sizeof(AccountUpdate_t));
    THROW(sw);
}

/**
 * @brief Convert an Hex digit to a number
 *
 * @param in Value to convert
 *
 * @return Converted value
 */
static uint8_t fromHexDigit(uint8_t in) {
    if (in >= '0' && (in <= '9')) {
        return in - '0';
    }
    if (in >= 'A' && (in <= 'F')) {
        return in - 'A' + 10;
    }
    if (in >= 'a' && (in <= 'f')) {
        return in - 'a' + 10;
    }
    return 0;
}

/**
 * @brief Convert an Account address string to Hex
 *
 * @param out Converted address
 * @param in Input address string
 */
static void convertAddressHex(uint8_t *out, const uint8_t *address) {
    uint8_t i;
    uint8_t hi, lo;

    for (i = 0; i < ADDRESS_LENGTH; i++) {
        hi = fromHexDigit(address[(i * 2)]);
        lo = fromHexDigit(address[(i * 2) + 1]);
        out[i] = (hi << 4) | lo;
    }
}

/**
 * @brief Convert an Account chain_id string to Hex
 *
 * @param out Converted chain_id
 * @param in Input chain_id string
 */
static void convertChainIdHex(uint8_t *out, uint64_t in) {
    int i = 0;

    for (i = (int) ACCOUNT_CHAIN_ID_LENGTH; i > 0; i--) {
        out[i - 1] = in & 0xFF;
        in >>= 8;
    }
}

/**
 * @brief Get the account index for a given name
 *
 * @param name Current account name
 *
 * @note The check is based on the same address and chain_id
 */
static uint8_t getAccountIdByName(char *name) {
    uint8_t i = 0;

    for (i = 0; i < getNbFavoriteAccounts(); i++) {
        if (memcmp((const char *) N_accounts.accounts[i].name, name, ACCOUNT_NAME_MAX_LENGTH) ==
            0) {
            // Found account name
            return i;
        }
    }
    // Not found
    PRINTF("[ACCOUNT] - Name: '%s' not found!\n", name);
    throwError(APDU_RESPONSE_REF_DATA_NOT_FOUND);
    // Will never arrive, but makes the compiler happy
    return 0xFF;
}

/**
 * @brief Get the existing account index
 *
 * @param account New account data
 *
 * @note The check is based on the same address and chain_id
 *
 * @return The found index, or INVALID_ACCOUNT_INDEX
 */
static uint8_t getAccountIndex(AccountData_t *account) {
    bool sameChain = false;
    bool sameName = false;
    bool sameAddress = false;
    uint8_t nbAccounts = INVALID_ACCOUNT_INDEX;
    uint8_t i = 0;

    for (i = 0; i < getNbFavoriteAccounts(); i++) {
        sameChain = N_accounts.accounts[i].chain_id == account->chain_id;
        sameAddress = memcmp((const char *) N_accounts.accounts[i].address,
                             account->address,
                             ACCOUNT_ADDR_MAX_LENGTH) == 0;
        sameName = memcmp((const char *) N_accounts.accounts[i].name,
                          account->name,
                          ACCOUNT_NAME_MAX_LENGTH) == 0;

        if (sameName == true) {
            if (sameAddress == true && sameChain == true) {
                // Account already exists in the Address book!
                PRINTF("[ACCOUNT] - Index: Account already exist!\n");
                throwError(APDU_RESPONSE_CONDITION_NOT_SATISFIED);
            }
            // Name exists for another account!
            PRINTF("[ACCOUNT] - Index: Name already used!\n");
            throwError(APDU_RESPONSE_INVALID_DATA);
        }
        if (sameAddress == true && sameChain == true) {
            nbAccounts = i;
            // Just need to rename of the account
            break;
        }
    }
    // New account
    return nbAccounts;
}

/**
 * @brief Called when receiving an APDU to Update an account
 *
 * @param buffer Received APDU buffer
 * @param length APDU buffer length
 * @param last 'true' if it is the last buffer, to trig the NVRAM write
 * @param address 'true' if the buffer contain an address; 'false' if it is a derivation path
 */
static void favoriteAccountUpdate(const uint8_t *buffer, uint8_t length, bool last, bool address) {
    uint8_t *data = (uint8_t *) buffer;
    uint8_t nbAccounts = 0;
    uint16_t sw = APDU_RESPONSE_INVALID_DATA;
    uint8_t remaining = 0;
    bip32_path_t bip32;

    // Check APDU parameters (size of the different parameters)
    if ((length < ACCOUNTS_MIN_APDU_LENGTH) || (length > ACCOUNTS_MAX_APDU_LENGTH)) {
        PRINTF("[ACCOUNT] - Update: Invalid length: %d (%d - %d)!\n",
               length,
               ACCOUNTS_MIN_APDU_LENGTH,
               ACCOUNTS_MAX_APDU_LENGTH);
        throwError(ERR_APDU_SIZE_MISMATCH);
    }

    // Check already available accounts
    nbAccounts = getNbFavoriteAccounts();

    // Check Available slots
    if ((nbAccounts + accountsUpdate.nbAccounts) == ACCOUNTS_MAX) {
        PRINTF("[ACCOUNT] - Update: No available slot: Current=%d, requested=%d - Max=%d)!\n",
               nbAccounts,
               accountsUpdate.nbAccounts,
               ACCOUNTS_MAX);
        throwError(APDU_RESPONSE_INSUFFICIENT_MEMORY);
    }

    // Save the Account data: chain_id
    accountsUpdate.accounts[accountsUpdate.nbAccounts].chain_id =
        u64_from_BE(data, ACCOUNT_CHAIN_ID_LENGTH);
    data += ACCOUNT_CHAIN_ID_LENGTH;
    length -= ACCOUNT_CHAIN_ID_LENGTH;
    if (address) {
        // Get the address
        snprintf(accountsUpdate.accounts[accountsUpdate.nbAccounts].address,
                 ACCOUNT_ADDR_MAX_LENGTH,
                 "0x%.*H",
                 ADDRESS_LENGTH,
                 data);
        data += ADDRESS_LENGTH;
        length -= ADDRESS_LENGTH;
    } else {
        // Get the derivation path
        data = (uint8_t *) parseBip32(data, &length, &bip32);
        getEthPublicKey(bip32.path, bip32.length);

        // Save the Account data: address
        snprintf(accountsUpdate.accounts[accountsUpdate.nbAccounts].address,
                 ACCOUNT_ADDR_MAX_LENGTH,
                 "0x%.*s",
                 (ADDRESS_LENGTH * 2),
                 tmpCtx.publicKeyContext.address);
    }

    // Save the Account data: name
    memmove(accountsUpdate.accounts[accountsUpdate.nbAccounts].name, data, length);

    // Store Account update type (rename or add)
    accountsUpdate.existIndex[accountsUpdate.nbAccounts] =
        getAccountIndex(&accountsUpdate.accounts[accountsUpdate.nbAccounts]);
    // Next index
    accountsUpdate.nbAccounts++;

    if (last) {
        // Last account: confirm and store
        ui_account_sync();
        sw = APDU_RESPONSE_OK;
    } else {
        // Not the last one, returns remaining places
        remaining = ACCOUNTS_MAX - nbAccounts - accountsUpdate.nbAccounts;
        if (remaining == 0) {
            PRINTF("[ACCOUNT] - Update: Memory: no more slots!\n");
            throwError(APDU_RESPONSE_INSUFFICIENT_MEMORY);
        }
        sw = APDU_RESPONSE_AVAILABLE | remaining;
    }
    returnResult(sw, 0);
}

/**
 * @brief Called when receiving an APDU to Rename an account
 *
 * @param buffer Received APDU buffer
 * @param length APDU buffer length
 */
static void favoriteAccountRename(const uint8_t *buffer, uint8_t length, uint8_t curNameSize) {
    char curName[ACCOUNT_NAME_MAX_LENGTH] = {0};
    uint8_t *data = (uint8_t *) buffer;
    uint8_t nbAccount = 0;

    // Check APDU parameters (size of the different parameters)
    if ((length < (2 * ACCOUNT_NAME_MIN_LENGTH)) || (length > (2 * ACCOUNT_NAME_MAX_LENGTH))) {
        PRINTF("[ACCOUNT] - Rename: Invalid length: %d (%d - %d)!\n",
               length,
               (2 * ACCOUNT_NAME_MIN_LENGTH),
               (2 * ACCOUNT_NAME_MAX_LENGTH));
        throwError(ERR_APDU_SIZE_MISMATCH);
    }
    if (((length - curNameSize) < ACCOUNT_NAME_MIN_LENGTH) ||
        ((length - curNameSize) > ACCOUNT_NAME_MAX_LENGTH)) {
        PRINTF("[ACCOUNT] - Rename: Invalid new length: %d (%d - %d)!\n",
               length,
               ACCOUNT_NAME_MIN_LENGTH,
               ACCOUNT_NAME_MAX_LENGTH);
        throwError(ERR_APDU_SIZE_MISMATCH);
    }

    // Save the Account data: Current (old) name
    memmove(curName, data, curNameSize);
    data += curNameSize;
    length -= curNameSize;

    // Get current account index
    nbAccount = getAccountIdByName(curName);
    // Retrieve current account
    memcpy((void *) &accountsUpdate.accounts[0],
           (const void *) &N_accounts.accounts[nbAccount],
           sizeof(AccountData_t));
    // Save the Account data: New name
    memmove(accountsUpdate.accounts[0].name, data, length);
    accountsUpdate.existIndex[0] = nbAccount;
    accountsUpdate.nbAccounts = 1;

    ui_account_sync();
    returnResult(APDU_RESPONSE_OK, 0);
}

/**
 * @brief Called when receiving an APDU to Get a specific account
 *
 * @param account Account numlber to retrieve
 */
static void favoriteAccountGet(uint8_t account) {
    uint8_t buffer[ACCOUNTS_MAX_APDU_LENGTH * 2] = {0};
    uint8_t nbAccounts = 0;
    uint8_t length = 0;
    uint8_t nameLen = 0;

    // Check already available accounts
    nbAccounts = getNbFavoriteAccounts();

    // Check Available slots
    if ((nbAccounts == 0) || (account >= nbAccounts)) {
        PRINTF("[ACCOUNT] - Get: Account %d not found!\n", account);
        THROW(APDU_RESPONSE_REF_DATA_NOT_FOUND);
    }

    // Generate APDU Response
    length = 0;
    memset(buffer, 0, ACCOUNTS_MAX_APDU_LENGTH);
    // Convert chain_id
    convertChainIdHex(buffer, N_accounts.accounts[account].chain_id);
    length += ACCOUNT_CHAIN_ID_LENGTH;
    // Convert address
    convertAddressHex(buffer + length,
                      (const uint8_t *) (N_accounts.accounts[account].address + 2));
    length += ADDRESS_LENGTH;
    // Convert name
    nameLen = strlen((const void *) N_accounts.accounts[account].name);
    memmove(buffer + length, (const void *) N_accounts.accounts[account].name, nameLen);
    length += nameLen;

    returnBuffer((const uint8_t *) buffer, length, APDU_RESPONSE_OK);
}

/**
 * @brief Called when receiving an APDU to delete all accounts
 */
static void favoriteAccountDelete(void) {
    // Delete all accounts
    deleteFavoriteAccounts();
    returnResult(APDU_RESPONSE_OK, 0);
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief Check for already registered accounts
 *
 * @return the number of found accounts
 */
uint8_t getNbFavoriteAccounts(void) {
    uint8_t nbAccounts = 0;
    while (N_accounts.accounts[nbAccounts].address[0] != 0) {
        nbAccounts++;
    }
    return nbAccounts;
}

/**
 * @brief Called when receiving an APDU to delete all accounts
 */
void deleteFavoriteAccounts(void) {
    AccountStorage_t empty = {0};
    // Delete all accounts
    nvm_write((void *) &N_accounts, &empty, sizeof(AccountStorage_t));
}

/**
 * @brief Called when receiving an APDU to handle favorite accounts
 *
 * @param p1 APDU P1 parameter
 * @param p2 APDU P2 parameter
 * @param buffer APDU buffer
 * @param length APDU buffer length
 */
void handleFavoriteAccounts(uint8_t p1, uint8_t p2, const uint8_t *buffer, uint8_t length) {
    if ((p2 != 0) && ((p1 == P1_FAVORITE_DELETE) || (p1 == P1_FAVORITE_UPDATE))) {
        PRINTF("[ACCOUNT] - Favorite: Invalid P1 0x%02x / P2 0x%02x!\n", p1, p2);
        throwError(APDU_RESPONSE_INVALID_P1_P2);
    }
    if ((p2 > 1) && ((p1 == P1_FAVORITE_UPDATE_MULTI) || (p1 == P1_FAVORITE_UPDATE_PATH_MULTI))) {
        PRINTF("[ACCOUNT] - Favorite: Invalid P2 0x%02x!\n", p2);
        throwError(APDU_RESPONSE_INVALID_P1_P2);
    }

    if ((p1 != P1_FAVORITE_GET) && (p1 != P1_FAVORITE_DELETE) && (p1 != P1_FAVORITE_UPDATE_MULTI) &&
        (p1 != P1_FAVORITE_UPDATE_PATH_MULTI)) {
        // Reset the update tmp storage
        PRINTF("[ACCOUNT] - Favorite: Command not following update group... Resetting\n");
        memset(&accountsUpdate, 0, sizeof(AccountUpdate_t));
    }

    switch (p1) {
        case P1_FAVORITE_GET:
            favoriteAccountGet(p2);
            break;
        case P1_FAVORITE_DELETE:
            favoriteAccountDelete();
            break;
        case P1_FAVORITE_UPDATE_MULTI:
            favoriteAccountUpdate(buffer, length, (p2 == 0x01), true);
            break;
        case P1_FAVORITE_UPDATE:
            favoriteAccountUpdate(buffer, length, true, true);
            break;
        case P1_FAVORITE_UPDATE_PATH_MULTI:
            favoriteAccountUpdate(buffer, length, (p2 == 0x01), false);
            break;
        case P1_FAVORITE_UPDATE_PATH:
            favoriteAccountUpdate(buffer, length, true, false);
            break;
        case P1_FAVORITE_RENAME:
            favoriteAccountRename(buffer, length, p2);
            break;
        default:
            PRINTF("[ACCOUNT] - Favorite: Invalid P1 0x%02x!\n", p1);
            throwError(APDU_RESPONSE_INVALID_P1_P2);
            break;
    }
}

// ********************************************************
//  TEST functions - to be removed
// ********************************************************

void dumpAllAccounts(void) {
    uint8_t i;
    char chain_str[20];

    PRINTF("[ACCOUNT] - dump ALL Accounts\n");
    for (i = 0; i < getNbFavoriteAccounts(); i++) {
        u64_to_string(N_accounts.accounts[i].chain_id, chain_str, sizeof(chain_str));
        PRINTF("[ACCOUNT] -   [%d]: address: %s - chain_id=%-3s (name: %s)\n",
               i,
               N_accounts.accounts[i].address,
               chain_str,
               N_accounts.accounts[i].name);
    }
}

void dumpAccount(AccountData_t *account) {
    char chain_str[20];

    PRINTF("[ACCOUNT] - dump Account\n");
    u64_to_string(account->chain_id, chain_str, sizeof(chain_str));
    PRINTF("[ACCOUNT] -   address: %s - chain_id=%-3s (name: %s)\n",
           account->address,
           chain_str,
           account->name);
}
#if 0
void accounts_init(void) {
    AccountStorage_t storage = {0};

    // TESTS - Initialize the NVM data if required
    if (getNbFavoriteAccounts() == 0) {
        storage.accounts[0].chain_id = 1;
        strlcpy(storage.accounts[0].name, "My main Ether", ACCOUNT_NAME_MAX_LENGTH);
        strlcpy(storage.accounts[0].address,
                "0x6a9f04c6C38B91ed6D6dA00F51CD37B9AaFa2648",
                ACCOUNT_ADDR_MAX_LENGTH);
        storage.accounts[1].chain_id = 5;
        strlcpy(storage.accounts[1].name, "ETH savings", ACCOUNT_NAME_MAX_LENGTH);
        strlcpy(storage.accounts[1].address,
                "0x524aBfF9D799c5f5219E9354B54551fB1Ed41Ceb",
                ACCOUNT_ADDR_MAX_LENGTH);
        nvm_write((void *) &N_accounts, &storage, sizeof(AccountStorage_t));
    }
    dumpAllAccounts();
}
#endif
