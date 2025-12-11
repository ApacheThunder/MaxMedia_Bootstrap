#include <nds/arm9/dldi.h>

typedef signed int addr_t;
typedef unsigned char data_t;

extern const DLDI_INTERFACE* io_dldi_data2;

void dldiLoadFromBin (const u8 dldiAddr[]);
void dldiLoadFromBin2 (const u8 dldiAddr2[]);
void dldiRelocateBinary (data_t *binData, size_t dldiFileSize);
void dldiRelocateBinary2 (data_t *binData, size_t dldiFileSize);

const DISC_INTERFACE *dldiGet(void);
const DISC_INTERFACE *dldiGet2(void);

