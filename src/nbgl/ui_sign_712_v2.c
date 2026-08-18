#include <string.h>
#include "common_ui.h"
#include "ui_nbgl.h"
#include "ui_sign_712.h"
#include "common_712.h"
#include "ui_utils.h"
#include "app_mem_utils.h"
#include "shared_context.h"
#include "typed_data.h"
#include "typed_data_format.h"
#include "cmd_get_gating.h"
#include "get_public_key.h"  // get_displayable_address

// The pairs on screen must not share a slot. A touch screen shows up to 5 review pairs at once, a
// Nano 1: the spare slots cover a pair fetched while the previous ones are still on screen.
#ifdef SCREEN_SIZE_WALLET
#define REVIEW_SLOTS 8
#else
#define REVIEW_SLOTS 2
#endif

// The review starts with the signer, the account of the derivation path the values came with
#define FROM_ROWS     1
#define FROM_STR_SIZE HEX_0X_STRING_SIZE(ADDRESS_LENGTH)

// The review pairs are not built up front: nbgl asks for a pair when it draws the page holding
// it, and the value of the pair is looked up in the trees and formatted into the slot its index
// maps to. Nothing is kept per value, so the review takes the same memory whatever the size of the
// message. The pairs hold the slots, then the row of the signer, then the rows of the hashes when
// they are shown. All of them are one allocation: the pairs handed to nbgl, then the text blocks. A
// text block holds a key and its value back to back, item pointing at its start and value just
// past the key's NUL. The blocks of the slots come first, then the address of the signer, then the
// strings of the hashes.
static struct {
    nbgl_contentTagValue_t *pairs;
    uint16_t count;                   // values
    uint8_t slots;                    // pairs that format values
    uint8_t hash_rows;                // pairs that show the hashes, after the signer
    bool failed;                      // whether a value could not be displayed
    size_t offset[REVIEW_SLOTS + 1];  // start of each slot's text block, then the total
} s_review;

typedef struct {
    uint16_t count;             // values seen so far
    size_t room[REVIEW_SLOTS];  // largest text block each slot will have to hold
} s_measure_ctx;

typedef struct {
    uint16_t index;   // value being met
    uint16_t target;  // value to format
    bool found;       // whether it was formatted
} s_fill_ctx;

/**
 * Count the values, size the text block of every one, and check each can be displayed
 *
 * to be used as a \ref td_f_value_visitor
 */
static bool measure_leaf(const s_struct_712_value *node, void *context) {
    s_measure_ctx *ctx = context;
    size_t value_size;
    size_t block;
    uint8_t slot;

    if (node->kind != VAL_ATOMIC) {
        return true;
    }
    if ((node->field == NULL) || (node->field->key_name == NULL)) {
        return false;
    }
    // a type with no text representation cannot be shown, and must not be signed unseen
    if ((value_size = td_format_value_size(node->field, node->length)) == 0) {
        return false;
    }
    block = strlen(node->field->key_name) + 1 + value_size;
    slot = ctx->count % REVIEW_SLOTS;
    if (block > ctx->room[slot]) {
        ctx->room[slot] = block;
    }
    ctx->count += 1;
    return true;
}

static char *text_area(void) {
    return (char *) &s_review.pairs[s_review.slots + FROM_ROWS + s_review.hash_rows];
}

/**
 * Format a value into the slot of the pair that stands for it
 *
 * @param[in] node the value
 * @param[in] index index of the pair, among the ones that stand for a value
 * @return whether the value could be formatted
 */
static bool format_leaf(const s_struct_712_value *node, uint16_t index) {
    const s_struct_712_field *field = node->field;
    uint8_t slot = index % s_review.slots;
    char *block = text_area() + s_review.offset[slot];
    size_t room = s_review.offset[slot + 1] - s_review.offset[slot];
    size_t key_size;
    size_t value_size;

    if ((field == NULL) || (field->key_name == NULL)) {
        return false;
    }
    key_size = strlen(field->key_name) + 1;
    value_size = td_format_value_size(field, node->length);
    if ((value_size == 0) || ((key_size + value_size) > room)) {
        return false;
    }
    memcpy(block, field->key_name, key_size);
    if (!td_format_value(field, node->data, node->length, block + key_size, value_size)) {
        return false;
    }
    s_review.pairs[slot].item = block;
    s_review.pairs[slot].value = block + key_size;
    return true;
}

/**
 * Format the value a review pair stands for, once the walk reaches it
 *
 * The walk ends on the value asked for, whether it could be formatted or not.
 *
 * to be used as a \ref td_f_value_visitor
 */
static bool fill_pair(const s_struct_712_value *node, void *context) {
    s_fill_ctx *ctx = context;

    if (node->kind != VAL_ATOMIC) {
        return true;
    }
    if (ctx->index != ctx->target) {
        ctx->index += 1;
        return true;
    }
    ctx->found = format_leaf(node, ctx->target);
    return false;
}

/**
 * Format every value in turn, each into the slot of its pair
 *
 * to be used as a \ref td_f_value_visitor
 */
static bool fill_every_pair(const s_struct_712_value *node, void *context) {
    uint16_t *index = context;

    if (node->kind != VAL_ATOMIC) {
        return true;
    }
    if (!format_leaf(node, *index)) {
        return false;
    }
    *index += 1;
    return true;
}

/**
 * Format a review pair into its slot, looking its value up in both trees
 *
 * @param[in] index index of the pair, among the ones that stand for a value
 * @return whether the pair could be formatted
 */
static bool format_pair(uint8_t index) {
    s_fill_ctx ctx = {.target = index};

    // the visitor ends the walk once it has met the value
    if (td_traverse_domain(fill_pair, &ctx)) {
        td_traverse_message(fill_pair, &ctx);
    }
    return ctx.found;
}

/**
 * Retrieve a review pair, to be used as a nbgl_contentTagValueCallback_t
 *
 * Every pair was formatted once when the review was prepared, and the trees do not change
 * meanwhile, so failing here is not expected. If it happens, the pair says so and the review can
 * no longer be approved, see review_choice().
 */
static nbgl_contentTagValue_t *get_pair(uint8_t index) {
    if (index < FROM_ROWS) {
        return &s_review.pairs[s_review.slots];
    }
    index -= FROM_ROWS;
    if (index < s_review.count) {
        if (format_pair(index)) {
            return &s_review.pairs[index % s_review.slots];
        }
    } else if ((index - s_review.count) < s_review.hash_rows) {
        return &s_review.pairs[s_review.slots + FROM_ROWS + (index - s_review.count)];
    }
    s_review.failed = true;
    s_review.pairs[index % s_review.slots].item = "Error";
    s_review.pairs[index % s_review.slots].value = "Value cannot be displayed";
    return &s_review.pairs[index % s_review.slots];
}

/**
 * A review that could not show everything is refused, whatever the user chose
 */
static void review_choice(bool confirm) {
    ui_712_review_choice(confirm && !s_review.failed);
}

void ui_712_v2_cleanup(void) {
    // nbgl may still draw before the list is freed, and must then find it empty. Only the list of
    // this review is emptied: another review frees its pairs through their count.
    if ((g_pairsList != NULL) && (g_pairsList->callback == get_pair)) {
        g_pairsList->nbPairs = 0;
    }
    APP_MEM_FREE_AND_NULL((void **) &s_review.pairs);
    explicit_bzero(&s_review, sizeof(s_review));
}

/**
 * Set the row of the signer
 *
 * @param[out] pair the row
 * @param[out] from_str receives the address of the signer, of FROM_STR_SIZE
 * @return whether the address could be derived and formatted
 */
static bool set_from_pair(nbgl_contentTagValue_t *pair, char *from_str) {
    if (!get_displayable_address(&tmpCtx.messageSigningContext712.bip32,
                                 from_str,
                                 FROM_STR_SIZE,
                                 g_chain_config->chain_id)) {
        return false;
    }
    pair->item = "From";
    pair->value = from_str;
    return true;
}

/**
 * Check every value can be displayed, and set up the slots and the pair list for the review
 *
 * @return whether the review is ready to be shown
 */
static bool build_pairs(void) {
    s_measure_ctx measure = {0};
    uint16_t filled = 0;
    uint8_t hash_rows = N_storage.displayHash ? EIP712_HASH_ROWS : 0;
    uint16_t count;
    uint8_t slots;

    ui_712_v2_cleanup();
    if (!td_traverse_domain(measure_leaf, &measure) ||
        !td_traverse_message(measure_leaf, &measure)) {
        return false;
    }
    count = measure.count;
    if ((count == 0) || ((FROM_ROWS + count + hash_rows) > MAX_REVIEW_ROWS)) {
        return false;
    }
    slots = (count < REVIEW_SLOTS) ? (uint8_t) count : REVIEW_SLOTS;
    s_review.count = count;
    s_review.slots = slots;
    s_review.hash_rows = hash_rows;
    for (uint8_t i = 0; i < slots; i++) {
        s_review.offset[i + 1] = s_review.offset[i] + measure.room[i];
    }

    ui_pairs_cleanup();
    if (!APP_MEM_CALLOC((void **) &s_review.pairs,
                        ((slots + FROM_ROWS + hash_rows) * sizeof(nbgl_contentTagValue_t)) +
                            s_review.offset[slots] + FROM_STR_SIZE +
                            (hash_rows * EIP712_HASH_STR_SIZE))) {
        return false;
    }
    if (!APP_MEM_CALLOC((void **) &g_pairsList, sizeof(*g_pairsList))) {
        return false;
    }
    g_pairsList->callback = get_pair;
    g_pairsList->nbPairs = (uint8_t) (FROM_ROWS + count + hash_rows);
    g_pairsList->wrapping = true;

    if (!set_from_pair(&s_review.pairs[slots], text_area() + s_review.offset[slots])) {
        return false;
    }
    if (hash_rows != 0) {
        char *domain_str = text_area() + s_review.offset[slots] + FROM_STR_SIZE;

        eip712_set_hash_pairs(&s_review.pairs[slots + FROM_ROWS],
                              domain_str,
                              domain_str + EIP712_HASH_STR_SIZE);
    }

    // formatting is deterministic, so what succeeds here succeeds each time nbgl asks again
    if (!td_traverse_domain(fill_every_pair, &filled) ||
        !td_traverse_message(fill_every_pair, &filled)) {
        return false;
    }
    return filled == count;
}

bool ui_712_v2_review(void) {
    if (!build_pairs()) {
        return false;
    }

    // V2 has no field descriptors yet, so every value is shown raw, which is blind signing. The
    // schema opened the flow, so the app is already out of idle and nothing is reset.
    if (!ui_712_prepare_blind_review()) {
        return false;
    }
    if (!set_gating_warning()) {
        return false;
    }
    return ui_712_start_review(TYPE_MESSAGE, review_choice);
}
