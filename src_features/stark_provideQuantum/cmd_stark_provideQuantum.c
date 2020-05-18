#ifdef HAVE_STARKWARE
  
#include "shared_context.h"
#include "apdu_constants.h"
#ifdef TARGET_BLUE
#include "ui_blue.h"
#endif
#ifdef HAVE_UX_FLOW
#include "ui_flow.h"
#endif

void handleStarkwareProvideQuantum(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, unsigned int *flags, unsigned int *tx) {
  size_t i = 0;
  tokenDefinition_t *currentToken = NULL;
  if (appState != APP_STATE_IDLE) {
    reset_app_context();
  }
  if (dataLength != 20 + 32) {
    THROW(0x6700);
  }
  if (!allzeroes(dataBuffer, 20)) {
    for(i=0; i<MAX_TOKEN; i++){
      currentToken = &tmpCtx.transactionContext.tokens[i];
      if (tmpCtx.transactionContext.tokenSet[i] && (os_memcmp(currentToken->address, dataBuffer, 20) == 0)) {
        break;
      }
    }
    if (i == MAX_TOKEN) {
      PRINTF("Associated token not found\n");
      THROW(0x6A80);
    }
  }
  else {
    i = MAX_TOKEN;
  }
  os_memmove(dataContext.tokenContext.quantum, dataBuffer + 20, 32);
  dataContext.tokenContext.quantumIndex = i;
  quantumSet = true;
  THROW(0x9000);
}

#endif
