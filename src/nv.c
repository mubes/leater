/*
 * Manage non-volatile storage.  This is the 'spare' flash from the top of program to the end
 * of the flash space.  The current configuration is stored right at the very top and the rest
 * of the spare memory is given over to log storage.
 */

#include "config.h"
#include "dutils.h"
#include "nv.h"
#include "printf.h"
#include "bod.h"

#define NV_END           0x00004000                         // End of NV store

#define NV_BLOCK_LEN     64
#define NV_SECTOR_LEN    1024

extern uint32_t _edata;                 // Symbol from linker representing end of initialised data
extern uint32_t _data;                  // Symbol from linker representing start of initialised data
extern uint32_t _etext;                 // Symbol from linker representing end of program

// Make sure we've got a storage location defined -- this can be overridden in config
// by default we put it after the initialisation data in flash
#ifndef NV_STORE_START
#define NV_STORE_START (&_etext+(&_edata-&_data))
#endif

#define IAP_LOCATION 0x1FFF1FF1
typedef void (*IAP)(unsigned long[], unsigned long[]);
const IAP iap_entry=(IAP)IAP_LOCATION;

static uint32_t *nv_wp=0;               // Current position in NV store for writing
uint32_t first_free_page;               // First free location
uint32_t config_store_page;             // Last free location before config
// ============================================================================================
BOOL _write_sector(uint32_t page_start, uint32_t *data_to_store)

// Write sector to flash using rom based routines. New data is written to first free location
// and assumption is made that existing data does not change.

{
    unsigned long command[5], result[4];

    if (bodIsActive()) return FALSE;

    // Prepare sector for write
    command[0]=50;
    command[1]=(uint32_t)page_start/NV_SECTOR_LEN;
    command[2]=(uint32_t)page_start/NV_SECTOR_LEN;
    command[3]=SystemCoreClock;
    denter_critical();
    iap_entry(command,result);
    if (result[0]!=0)
        {
            dleave_critical();
            return FALSE;
        }

    // Perform the write
    command[0]=51;
    command[1]=(uint32_t)page_start&0xFFFFFFC0;
    command[2]=(uint32_t)data_to_store;
    command[3]=NV_BLOCK_LEN;
    command[4]=SystemCoreClock/1000;
    iap_entry(command,result);
    dleave_critical();
    return result[0]==0;
}
// ============================================================================================
BOOL _flush_sector(uint32_t page_start)

// Scrub a sector back to 0xFFFFFFFF

{
    unsigned long command[5], result[4];

    if (bodIsActive()) return FALSE;

    // Prepare sector for write
    command[0] = 50;
    command[1] = page_start / NV_SECTOR_LEN;
    command[2] = page_start / NV_SECTOR_LEN;
    command[3] = SystemCoreClock / 1000;

    denter_critical();
    iap_entry(command, result);
    if (result[0] != 0)
        {
            dleave_critical();
            return FALSE;
        }

    // Now perform the scrub
    command[0] = 59;
    command[1] = page_start / NV_BLOCK_LEN;
    command[2] = page_start / NV_BLOCK_LEN;
    iap_entry(command, result);
    dleave_critical();
    return (result[0]==0);
}
// ============================================================================================
BOOL _realWrite_entry(uint32_t val_to_write)

// ACtually write the sector, assuming we're not in a brownout condition

{
    uint32_t ram_block[16];  // 64 Bytes read from sector
    uint32_t readPos=0;
    uint32_t *rp=(uint32_t *)((uint32_t)nv_wp&0xFFFFFFC0);  // Go to start of this sector

    if (bodIsActive()) return FALSE;

    // If nv isn't available, or full, return false
    if ((!nv_wp) || ((uint32_t)nv_wp>=(config_store_page-1))) return FALSE;

    while (readPos<16) ram_block[readPos++]=*rp++; // Read the data that's there

    // Now put the new value in...
    ram_block[((uint32_t)nv_wp&0x3F)>>2]=val_to_write;
    if (_write_sector((uint32_t)nv_wp&0xFFFFFFC0, ram_block))
        nv_wp+=1;  // Need to move on by four bytes, but these are uint32_t, so thats +1

    return TRUE;
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
// ============================================================================================
uint32_t nvRead_partID(void)

// Read the part identifier

{
    unsigned long command[5], result[4];
    command[0]=54;
    iap_entry(command,result);
    return result[1];
}
// ============================================================================================
uint32_t nvRead_partVersion(void)

// Read the ROM version identifier

{
    unsigned long command[5], result[4];
    command[0]=55;
    iap_entry(command,result);
    return result[1];
}
// ============================================================================================
BOOL nvWrite_entry(uint32_t val_to_write)

// Write value to store, but check it's not NV_EMPTY (0xFFFFFFFF) first.

{
    // Not allowed to write empty values - assert check
    ASSERT(val_to_write!=NV_EMPTY);
    return _realWrite_entry(val_to_write);
}
// ============================================================================================
BOOL nvFlush(void)

// Flush the whole of the log memory

{
    uint32_t flushmem = first_free_page;

    do
        {
            if (!_flush_sector(flushmem))
                return FALSE;
            flushmem += NV_BLOCK_LEN;
        }
    while (flushmem < config_store_page);

    return (nvInit());
}
// ============================================================================================
uint32_t nvGetSpace(void)

// Return the amount of free space left

{
    return config_store_page-(uint32_t)nv_wp;
}
// ============================================================================================
BOOL nvWriteConfig(const ConfigStoreType *c)

// Scrub and re-write the config to nv memory

{
    uint32_t flushmem;                                  // Scratchpad for write location
    uint32_t ram_block[NV_BLOCK_LEN/sizeof(uint32_t)];  // Scratchpad for transfer mem
    uint32_t *rp=(uint32_t *)c;                         // Pointer to location in config
    uint32_t writeRemaining=sizeof(ConfigStoreType);    // Number of bytes to write
    uint32_t i;

    // First erase the old version of the config
    flushmem = config_store_page;
    do
        {
            if (!_flush_sector(flushmem))
                return FALSE;
            flushmem += NV_BLOCK_LEN;
        }
    while (flushmem < NV_END);

    // Now write the new values back
    flushmem = config_store_page;
    while (writeRemaining)
        {
            i=0;
            // Copy across the config to write
            while ((writeRemaining) && (i<(NV_BLOCK_LEN/sizeof(uint32_t))))
                {
                    writeRemaining-=sizeof(uint32_t);
                    ram_block[i++]=*rp++;
                }

            // If there's any left-over then scrub the ram
            while (i<(NV_BLOCK_LEN/sizeof(uint32_t)))
                ram_block[i++]=0;

            if (!_write_sector((uint32_t)flushmem, ram_block))
                return FALSE;
            flushmem+=NV_BLOCK_LEN;
        }
    while (writeRemaining);
    return TRUE;
}
// ============================================================================================
void nvReadConfig(ConfigStoreType *c)

// Get configuration from nv ram into config buffer

{
    uint32_t readRemaining=sizeof(ConfigStoreType);                     // Number of bytes to read
    uint32_t *readmem = (uint32_t *)(config_store_page);                // Were to get it from
    uint32_t *writemem = (uint32_t *)c;                                 // Where to write it to

    while (readRemaining)
        {
            *writemem++=*readmem++;
            readRemaining-=sizeof(uint32_t);
        }
}
// ============================================================================================
uint32_t nvTotalSpace(void)

// Return total space in NV memory

{
    return (config_store_page-first_free_page);
}
// ============================================================================================
BOOL nvInit(void)

// Initialisation function for non-volatile memory

{
    uint32_t *rp;

    // Round the first page up to be on a boundary
    first_free_page=(((uint32_t)NV_STORE_START)+NV_BLOCK_LEN-1)&0xFFFFFFC0;

    // Round down the last free page to make sure there's room for the config
    config_store_page=((NV_END-sizeof(ConfigStoreType))&0xFFFFFFC0);

    rp=(uint32_t *)first_free_page;

    // Loop through NV ram looking for first unused location
    while ((*rp!=NV_EMPTY) && (rp<(uint32_t *)(config_store_page))) rp++;

    nv_wp=rp;
    return ((BOOL)nv_wp<config_store_page);
}
// ============================================================================================
void nvInitIterator(nvIterator *n)

// Initialise read iterator

{
    n->rp=(uint32_t *)first_free_page;
    n->state=NV_OK;
}
// ============================================================================================
nvState nvIteratorState(nvIterator *n)

// Return current state of iterator

{
    return n->state;
}
// ============================================================================================
uint32_t nvIteratorNext(nvIterator *n)

// Get next entry from nv store

{
    if ((*n->rp==NV_EMPTY) || ((uint32_t)n->rp>=config_store_page))
        {
            n->state=NV_ENDSTATE;
            return NV_EMPTY;
        }

    if (n->state==NV_OK)
        return *n->rp++;
    else
        return NV_EMPTY;
}
// ============================================================================================

