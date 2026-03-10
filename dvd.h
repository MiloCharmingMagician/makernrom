#ifndef DVD_H
#define DVD_H

#include "types.h"

#define BCA_SIZE 188

extern const u32 edc_table[256];
extern u32 CalculateEDC(const u8 *data, u32 len);
extern u16 CalculateIED(u32 psn);

#pragma pack(push,1)

typedef struct {
    u8  reserved1[3];      /* padding */
    u32 startSector;       /* 0x00030000 */
    u8  reserved2[1];      /* padding */
    u32 endSector;         /* 0x00030000 + totalSectors - 1 */
} DataAreaAllocation;      /* exactly 12 bytes - FIXED for correct PSN */

/* Each radial mark entry */
typedef struct {
    u32 psn;        /* Physical sector number */
    u16 bitOffset;  /* Bit offset within the sector where the mark starts */
    u8  type;       /* Mark type (Datel style = 0xFF) */
    u8  reserved;   /* Padding to make struct size 8 bytes */
} BcaMarkEntry;

/* Complete BCA block */
typedef struct {
    /* --- Manufacturing metadata (plain) --- */
    u8  optInfo[52];       /* mastering options */
    u8  manufacturer[2];   /* pressing plant code */
    u8  recorderDevice[2]; /* LBR device code */
    u8  bcaSerial;         /* disc batch serial */
    u8  discDate[2];       /* year / month etc. */
    u8  discTime[2];       /* hour / minute etc. */
    u8  discNumber[3];     /* disc number in batch */

    /* --- Authentication key and ID --- */
    u8 key[8];             /* 8-byte key used by hardware check */
    u8 id[4];              /* 4-byte disc identifier */

    /* --- Radial mark table --- */
    BcaMarkEntry marks[6]; /* 6 radial marks used for validation */

    /* --- Padding to fill 188 bytes --- */
    u8  reserved[64];
} DiscBca;

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