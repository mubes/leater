
#ifndef LEDFLASH_H_
#define LEDFLASH_H_

typedef enum { LEDFLASH_ERROR, LEDFLASH_NORMAL, LEDFLASH_OFF, LEDFLASH_MAX=LEDFLASH_OFF} LedFlashType;

// ============================================================================================
void ledTimeout(void);

void ledSetState(LedFlashType FlashSet);
void ledInit(void);
// ============================================================================================

#endif /* LEDFLASH_H_ */
