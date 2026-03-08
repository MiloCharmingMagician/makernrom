#ifndef DVD_H
#define DVD_H

#include "types.h"

#define BCA_SIZE 188

extern const u32 edc_table[256];

#pragma pack(push,1)

typedef struct {
    u8  reserved1[3];      /* padding */
    u32 startSector;       /* 0x00030000 */
    u8  reserved2[1];      /* padding */
    u32 endSector;         /* 0x00030000 + totalSectors - 1 */
} DataAreaAllocation;      /* exactly 12 bytes - FIXED for correct PSN */

typedef struct {
    u32 start;
    u32 end;
} PsnRegion;

typedef struct {
    u8 optInfo[52];
    u8 manufacturer[2];
    u8 recorderDevice[2];
    u8 bcaSerial;
    u8 discDate[2];
    u8 discTime[2];
    u8 discNumber[3];

    u8 key[8];
    u8 id[4];

    PsnRegion psn[6];
} DiscBca; /* 188 */

typedef struct {
    u32 id;
    u16 ied;
    u8  userdata[2048];
    u8  cpr_mai[6];
    u32 edc;
} DataFrame; /* 2064 */

typedef struct {
    u8 reversed[6];
    u8 secret1[6];
    u8 randomNumber2[6];
    u8 secret2[6];
    u8 randomNumber3[6];
    char mediaId[19];

    u8 randomNumber4[6];

    u8 bookTypePartVersion;
    u8 discSizeMinReadoutRate;
    u8 discStructure;
    u8 recordedDensity;

    DataAreaAllocation m_dataAreaAllocation;

    u8 bcaDescriptor;

    u8 reversed2[1967];
    u8 reversed3[6];
} DiscManufacturingInfo; /* 2048 */

typedef struct {
    u8 reversed[6];
    u8 discMagic;
    u8 discSizeMinTransferRate;
    u8 discStructure;
    u8 recordedDensity;

    DataAreaAllocation m_dataAreaAllocation;

    u8 reversed2[2000];
    u8 reversed3[6];
} PhysicalFormatInfo; /* 2048 */

typedef struct {
    PhysicalFormatInfo    m_pfi;
    DiscManufacturingInfo m_dmi;
} ControlDataZone; /* 4096 */

#pragma pack(pop)

#endif // DVD_H