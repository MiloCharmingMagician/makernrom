#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dvd.h"

/* ------------------------------------------------------ */
/* Scramble table / NROM scrambling */
/* ------------------------------------------------------ */
static u8 scramble_table[32768];
static const u16 seed_table[16] =
{
    3,48,32512,28673,
    6,69,32256,24579,
    12,192,31744,16391,
    24,384,30720,15
};

static u8 GetScramblePreset(const u8 *discId8)
{
    u32 sum = 0;
    int i;
    for (i = 0; i < 8; i++)
        sum += discId8[i];
    return (u8)(((sum >> 4) + sum) & 0x0F);
}

static void GenerateScrambleTable(u16 initial_lfsr)
{
    int i;
    u16 lfsr = initial_lfsr;
    u16 bit;
    for (i = 0; i < 32768; i++)
    {
        scramble_table[i] = (u8)lfsr;
        bit = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
        lfsr = (lfsr >> 1) | (bit << 14);
    }
}

static void ScrambleUserdata(u8 *data2048, u32 psn, const u8 *discId8)
{
    u32 extra = 0;
    int i;
    u8 nibble, preset;
    if (psn >= 0x30010UL)
    {
        nibble = (u8)((psn >> 4) & 0x0F);
        preset = GetScramblePreset(discId8);
        extra = 0x3C00UL + ((u32)(nibble ^ preset) * 0x800UL);
    }
    for (i = 6; i < 2048; i++)
        data2048[i] ^= scramble_table[(extra + (u32)(i - 6)) % 32768];
}

/* ------------------------------------------------------ */
/* Control blocks: PFI / DMI */
/* ------------------------------------------------------ */
static void FillPFI(PhysicalFormatInfo *pfi)
{
    memset(pfi, 0, sizeof(*pfi));
    memset(pfi->reversed, 0, sizeof(pfi->reversed));
    pfi->discMagic = 0xFF;
    pfi->discSizeMinTransferRate = 0x12;
    pfi->discStructure = 0x01;
    pfi->recordedDensity = 0x00;
    memset(pfi->m_dataAreaAllocation.reserved1, 0,
           sizeof(pfi->m_dataAreaAllocation.reserved1));
    pfi->m_dataAreaAllocation.startSector = 0x030000;
    pfi->m_dataAreaAllocation.reserved2[0] = 0;
    pfi->m_dataAreaAllocation.endSector = 0x0DE0AF;
    memset(pfi->reversed2, 0, sizeof(pfi->reversed2));
    memset(pfi->reversed3, 0, sizeof(pfi->reversed3));
}

static void FillDMI(DiscManufacturingInfo *dmi)
{
    memset(dmi, 0, sizeof(*dmi));
    strncpy(dmi->mediaId, "Nintendo Game Disk", sizeof(dmi->mediaId) - 1);
    dmi->mediaId[sizeof(dmi->mediaId) - 1] = '\0';
    dmi->bookTypePartVersion = 0x01;
    dmi->discSizeMinReadoutRate = 0x12;
    dmi->discStructure = 0x01;
    dmi->recordedDensity = 0x00;
    dmi->m_dataAreaAllocation.startSector = 0x030000;
    dmi->m_dataAreaAllocation.endSector = 0x0DE0AF;
    dmi->bcaDescriptor = 0x80;
}

/* ------------------------------------------------------ */
/* Generate Datel-style BCA (fixed pattern from real dumps) */
/* ------------------------------------------------------ */
static void GenerateBCA(const char *filename)
{
    unsigned char bca[BCA_SIZE];
    FILE *f = NULL;
    int i;

    memset(bca, 0, BCA_SIZE);

    /* Real Datel/Action Replay pattern: mostly zeros + "DATEL..." at ~0x30 */
    bca[0x30] = 0x44;  /* D */
    bca[0x31] = 0x41;  /* A */
    bca[0x32] = 0x54;  /* T */
    bca[0x33] = 0x45;  /* E */
    bca[0x34] = 0x4C;  /* L */
    bca[0x35] = 0x02;
    bca[0x36] = 0x99;
    bca[0x37] = 0x03;
    bca[0x38] = 0x21;

    f = fopen(filename, "wb");
    if (!f)
    {
        printf("Error: cannot create %s\n", filename);
        return;
    }
    fwrite(bca, 1, BCA_SIZE, f);
    fclose(f);

    printf("Datel-style BCA generated: %s (188 bytes)\n", filename);
    printf("First 64 bytes preview:\n");
    for (i = 0; i < 64; i++)
    {
        printf("%02X ", bca[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

/* ------------------------------------------------------ */
/* MAIN */
/* ------------------------------------------------------ */
int main(int argc, char *argv[])
{
    FILE *in = NULL;
    FILE *out = NULL;
    u32 fileSize = 0;
    u32 numSectors = 0;
    u8 discId[8];
    u32 i = 0;
    DataFrame frame;
    u8 buffer[2048];
    u32 psnBase = 0x30000UL;
    u32 psn = 0;

    argv[1] = "test.gcm";
    argv[2] = "test.img";

#ifdef NDEBUG
    if (argc < 3)
    {
        printf("Usage: makenrom input.gcm output.img\n");
        return 1;
    }
#endif

    in = fopen(argv[1], "rb");
    if (!in) { perror("input"); return 1; }

    out = fopen(argv[2], "wb");
    if (!out) { perror("output"); fclose(in); return 1; }

    fseek(in, 0, SEEK_END);
    fileSize = (u32)ftell(in);
    numSectors = fileSize / 2048;
    fseek(in, 0, SEEK_SET);

    fread(discId, 1, 8, in);

    GenerateBCA("bca.bin");
    GenerateScrambleTable(seed_table[GetScramblePreset(discId)]);

    printf("Creating premaster image...\n");
    printf("User data sectors: %lu\n", (unsigned long)numSectors);

    /* Lead-in (2 frames) */
    for (i = 0; i < 2; i++)
    {
        memset(&frame, 0, sizeof(frame));
        frame.id = i;
        frame.ied = CalculateIED(i);
        if (i == 0)
        {
            PhysicalFormatInfo pfi;
            FillPFI(&pfi);
            memcpy(frame.userdata, &pfi, sizeof(pfi));
        }
        else if (i == 1)
        {
            DiscManufacturingInfo dmi;
            FillDMI(&dmi);
            memcpy(frame.userdata, &dmi, sizeof(dmi));
        }
        memset(frame.cpr_mai, 0, sizeof(frame.cpr_mai));
        frame.edc = CalculateEDC((u8*)&frame, sizeof(frame) - sizeof(frame.edc));
        fwrite(&frame, 1, sizeof(frame), out);
    }

    /* User data (scrambled + Datel-style zero corruption) */
    fseek(in, 0, SEEK_SET);  /* rewind for second read */
    fread(discId, 1, 8, in);  /* skip disc ID again */

    for (i = 0; i < numSectors; i++)
    {
        fread(buffer, 1, 2048, in);
        ScrambleUserdata(buffer, psnBase + i, discId);

        /* Datel radial mark emulation: long zero runs in early sectors */
        psn = psnBase + i;
        if (psn >= 0x30000UL && psn < 0x30100UL)
        {
            /* only safe zero-run inside 2048 bytes */
            memset(buffer + 0x200, 0x00, 0x400);   /* 1024 zeros starting at 512 */

            printf("Datel zero-run applied at PSN 0x%lX (sector %lu)\n",
                   (unsigned long)psn, (unsigned long)i);
        }

        frame.id = psn;
        frame.ied = CalculateIED(frame.id);
        memcpy(frame.userdata, buffer, 2048);
        memset(frame.cpr_mai, 0, sizeof(frame.cpr_mai));
        frame.edc = CalculateEDC((u8*)&frame, sizeof(frame) - sizeof(frame.edc));
        fwrite(&frame, 1, sizeof(frame), out);
    }

    fclose(in);
    fclose(out);

    printf("Done!\n");
    printf("Factory image: %s\n", argv[2]);
    printf("Total size : %lu sectors\n",
           (unsigned long)(0x30000UL + numSectors));
    printf("BCA generated as bca.bin (Datel-style).\n");
    printf("\nPress ENTER to exit...");
    fflush(stdout);
    getchar();

    return 0;
}