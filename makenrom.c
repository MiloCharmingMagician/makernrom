#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dvd.h"

/* ------------------------------------------------------ */
/* Scramble table / NROM scrambling                       */
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
    u8 nibble;
    u8 preset;

    if (psn >= 0x30010UL)
    {
        nibble = (u8)((psn >> 4) & 0x0F);
        preset = GetScramblePreset(discId8);
        extra = 0x3C00UL + ((u32)(nibble ^ preset) * 0x800UL);
    }

    for (i = 6; i < 2048; i++)
    {
        data2048[i] ^= scramble_table[(extra + (u32)(i - 6)) % 32768];
    }
}

/* ------------------------------------------------------ */
/* Control blocks: PFI / DMI                               */
/* ------------------------------------------------------ */
static void FillPFI(PhysicalFormatInfo *pfi)
{
    int i;
    memset(pfi, 0, sizeof(*pfi));
    for (i = 0; i < sizeof(pfi->reversed); i++) pfi->reversed[i] = 0;

    pfi->discMagic = 0xFF;
    pfi->discSizeMinTransferRate = 0x12;
    pfi->discStructure = 0x01;
    pfi->recordedDensity = 0x00;

    for (i = 0; i < sizeof(pfi->m_dataAreaAllocation.reserved1); i++)
        pfi->m_dataAreaAllocation.reserved1[i] = 0;

    pfi->m_dataAreaAllocation.startSector = 0x030000;
    pfi->m_dataAreaAllocation.reserved2[0] = 0;
    pfi->m_dataAreaAllocation.endSector = 0x0DE0AF;

    for (i = 0; i < sizeof(pfi->reversed2); i++) pfi->reversed2[i] = 0;
    for (i = 0; i < sizeof(pfi->reversed3); i++) pfi->reversed3[i] = 0;
}

static void FillDMI(DiscManufacturingInfo *dmi)
{
    int i;
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

    for (i = 0; i < sizeof(dmi->randomNumber2); i++)
        dmi->randomNumber2[i] = 0; /* just example fill */
}

/* ------------------------------------------------------ */
/* Generate Datel / Action Replay style BCA (scrambled)  */
/* ------------------------------------------------------ */
static void GenerateBCA(const char *filename)
{
    DiscBca bca;
    FILE *f;
    int i, j;
    u8 *markBytes;

    /* --- Initialize BCA --- */
    memset(&bca, 0, sizeof(bca));

    /* ---- manufacturing metadata ---- */
    bca.manufacturer[0] = 'D';
    bca.manufacturer[1] = 'T';
    bca.recorderDevice[0] = 'D';
    bca.recorderDevice[1] = 'T';
    bca.bcaSerial = 1;

    /* simple example timestamp */
    bca.discDate[0] = 0x20;
    bca.discDate[1] = 0x24;
    bca.discTime[0] = 0x12;
    bca.discTime[1] = 0x00;
    bca.discNumber[0] = 0;
    bca.discNumber[1] = 0;
    bca.discNumber[2] = 1;

    /* ---- authentication fields ---- */
    memcpy(bca.key, "DATELKEY", 8);
    memcpy(bca.id,  "DTL0", 4);

    /* ---- define radial marks ---- */
    bca.marks[0].psn = 0x30008; bca.marks[0].bitOffset = 0x100; bca.marks[0].type = 0xFF;
    bca.marks[1].psn = 0x30024; bca.marks[1].bitOffset = 0x120; bca.marks[1].type = 0xFF;
    bca.marks[2].psn = 0x30048; bca.marks[2].bitOffset = 0x140; bca.marks[2].type = 0xFF;
    bca.marks[3].psn = 0x3006A; bca.marks[3].bitOffset = 0x160; bca.marks[3].type = 0xFF;
    bca.marks[4].psn = 0x3008E; bca.marks[4].bitOffset = 0x180; bca.marks[4].type = 0xFF;
    bca.marks[5].psn = 0x300B2; bca.marks[5].bitOffset = 0x1A0; bca.marks[5].type = 0xFF;

    /* -------------------------------------------------- */
    /* Scramble the radial marks using the 8-byte key   */
    /* Each mark struct has 8 bytes (PSN + offset + type) */
    /* -------------------------------------------------- */
    for (i = 0; i < 6; i++)
    {
        /* Treat each mark as bytes for scrambling */
        markBytes = (u8*)&bca.marks[i];

        for (j = 0; j < 8; j++)
        {
            /* XOR each byte with the key (simple scrambling) */
            markBytes[j] ^= bca.key[j % 8];
        }
    }

    /* Write BCA file */
    f = fopen(filename, "wb");
    if (!f)
    {
        printf("Error: cannot create %s\n", filename);
        return;
    }

    fwrite(&bca, 1, sizeof(bca), f);
    fclose(f);

    /* Print results */
    printf("Datel-style BCA generated (scrambled): %s\n", filename);
    printf("Size: %u bytes\n", (unsigned)sizeof(bca));
    printf("Radial marks (scrambled):\n");

    for (i = 0; i < 6; i++)
    {
        printf("Mark %d : PSN %08lX Offset %04X\n",
               i,
               (unsigned long)bca.marks[i].psn,
               bca.marks[i].bitOffset);
    }
}

/* ------------------------------------------------------ */
/* Apply Datel zero-run for early sectors               */
/* ------------------------------------------------------ */
static void ApplyDatelZeroRun(u8 *sector, u32 psn)
{
    u32 i;
    if (psn >= 0x30000UL && psn < 0x30100UL)
    {
        for (i = 0; i < 0x400; i++)
            sector[0x200 + i] = 0x00;
    }
}

/* ------------------------------------------------------ */
/* MAIN                                                 */
/* ------------------------------------------------------ */
int main(int argc, char *argv[])
{
    FILE *in;
    FILE *out;
    u32 fileSize;
    u32 numSectors;
    u32 i;
    u32 psn;
    u32 psnBase = 0x30000UL;
    u8 discId[8];
    u8 buffer[2048];
    DataFrame frame;
    PhysicalFormatInfo pfi;
    DiscManufacturingInfo dmi;

#ifdef NDEBUG
    if (argc < 3) {
        printf("Usage: %s input.gcm output.img\n", argv[0]);
        return 1;
    }
#else
    argv[1] = "test.gcm";
	argv[2] = "test.img";
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

    /* Lead-in frames */
    for (i = 0; i < 2; i++)
    {
        memset(&frame, 0, sizeof(frame));
        frame.id = i;
        frame.ied = CalculateIED(i);

        if (i == 0)
        {
            FillPFI(&pfi);
            memcpy(frame.userdata, &pfi, sizeof(pfi));
        }
        else
        {
            FillDMI(&dmi);
            memcpy(frame.userdata, &dmi, sizeof(dmi));
        }

        memset(frame.cpr_mai, 0, sizeof(frame.cpr_mai));
        frame.edc = CalculateEDC((u8*)&frame, sizeof(frame) - sizeof(frame.edc));
        fwrite(&frame, 1, sizeof(frame), out);
    }

    /* User data sectors */
    fseek(in, 0, SEEK_SET);
    fread(discId, 1, 8, in); /* skip disc ID again */

    for (i = 0; i < numSectors; i++)
    {
        fread(buffer, 1, 2048, in);
        ScrambleUserdata(buffer, psnBase + i, discId);

        psn = psnBase + i;
        ApplyDatelZeroRun(buffer, psn);

        frame.id = psn;
        frame.ied = CalculateIED(frame.id);
        memcpy(frame.userdata, buffer, 2048);
        memset(frame.cpr_mai, 0, sizeof(frame.cpr_mai));
        frame.edc = CalculateEDC((u8*)&frame, sizeof(frame) - sizeof(frame.edc));
        fwrite(&frame, 1, sizeof(frame), out);

        if (psn >= 0x30000UL && psn < 0x30100UL)
        {
            printf("Datel zero-run applied at PSN 0x%lX (sector %lu)\n",
                   (unsigned long)psn, (unsigned long)i);
        }
    }

    fclose(in);
    fclose(out);

    printf("Done!\n");
    printf("Factory image: %s\n", "test.img");
    printf("Total size : %lu sectors\n", (unsigned long)(psnBase + numSectors));
    printf("BCA generated as bca.bin (Datel-style).\n");
    printf("\nPress ENTER to exit...");
    fflush(stdout);
    getchar();

    return 0;
}