/*****************************************************************************
 * Copyright (C) 2013 x265 project
 *
 * Authors: Steve Borho <steve@borho.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#include "common.h"
#include "primitives.h"

namespace x265 {
// x265 private namespace

extern const uint8_t lumaPartitionMapTable[] =
{
//  4          8          12          16          20  24          28  32          36  40  44  48          52  56  60  64
    LUMA_4x4,  LUMA_4x8,  255,        LUMA_4x16,  255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 4
    LUMA_8x4,  LUMA_8x8,  255,        LUMA_8x16,  255, 255,        255, LUMA_8x32,  255, 255, 255, 255,        255, 255, 255, 255,        // 8
    255,        255,      255,        LUMA_12x16, 255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 12
    LUMA_16x4, LUMA_16x8, LUMA_16x12, LUMA_16x16, 255, 255,        255, LUMA_16x32, 255, 255, 255, 255,        255, 255, 255, LUMA_16x64, // 16
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 20
    255,        255,      255,        255,        255, 255,        255, LUMA_24x32, 255, 255, 255, 255,        255, 255, 255, 255,        // 24
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 28
    255,        LUMA_32x8, 255,       LUMA_32x16, 255, LUMA_32x24, 255, LUMA_32x32, 255, 255, 255, 255,        255, 255, 255, LUMA_32x64, // 32
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 36
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 40
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 44
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, LUMA_48x64, // 48
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 52
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 56
    255,        255,      255,        255,        255, 255,        255, 255,        255, 255, 255, 255,        255, 255, 255, 255,        // 60
    255,        255,      255,        LUMA_64x16, 255, 255,        255, LUMA_64x32, 255, 255, 255, LUMA_64x48, 255, 255, 255, LUMA_64x64  // 64
};

/* the "authoritative" set of encoder primitives */
EncoderPrimitives primitives;

void Setup_C_PixelPrimitives(EncoderPrimitives &p);
void Setup_C_DCTPrimitives(EncoderPrimitives &p);
void Setup_C_IPFilterPrimitives(EncoderPrimitives &p);
void Setup_C_IPredPrimitives(EncoderPrimitives &p);
void Setup_C_LoopFilterPrimitives(EncoderPrimitives &p);

void Setup_C_Primitives(EncoderPrimitives &p)
{
    Setup_C_PixelPrimitives(p);      // pixel.cpp
    Setup_C_DCTPrimitives(p);        // dct.cpp
    Setup_C_IPFilterPrimitives(p);   // ipfilter.cpp
    Setup_C_IPredPrimitives(p);      // intrapred.cpp
    Setup_C_LoopFilterPrimitives(p); // loopfilter.cpp
}

void Setup_Alias_Primitives(EncoderPrimitives &p)
{
    /* copy reusable luma primitives to chroma 4:4:4 */
    for (int i = 0; i < NUM_LUMA_PARTITIONS; i++)
    {
        p.chroma[X265_CSP_I444].copy_pp[i] = p.luma_copy_pp[i];
        p.chroma[X265_CSP_I444].copy_ps[i] = p.luma_copy_ps[i];
        p.chroma[X265_CSP_I444].copy_sp[i] = p.luma_copy_sp[i];
        p.chroma[X265_CSP_I444].copy_ss[i] = p.luma_copy_ss[i];
        p.chroma[X265_CSP_I444].addAvg[i] = p.luma_addAvg[i];
        p.chroma[X265_CSP_I444].satd[i] = p.satd[i];
    }

    for (int i = 0; i < NUM_SQUARE_BLOCKS; i++)
    {
        p.chroma[X265_CSP_I444].add_ps[i]  = p.luma_add_ps[i];
        p.chroma[X265_CSP_I444].sub_ps[i]  = p.luma_sub_ps[i];
    }

    primitives.sa8d[BLOCK_4x4]   = primitives.sa8d_inter[LUMA_4x4];
    primitives.sa8d[BLOCK_8x8]   = primitives.sa8d_inter[LUMA_8x8];
    primitives.sa8d[BLOCK_16x16] = primitives.sa8d_inter[LUMA_16x16];
    primitives.sa8d[BLOCK_32x32] = primitives.sa8d_inter[LUMA_32x32];
    primitives.sa8d[BLOCK_64x64] = primitives.sa8d_inter[LUMA_64x64];

    // SA8D devolves to SATD for blocks not even multiples of 8x8
    primitives.sa8d_inter[LUMA_4x4]   = primitives.satd[LUMA_4x4];
    primitives.sa8d_inter[LUMA_4x8]   = primitives.satd[LUMA_4x8];
    primitives.sa8d_inter[LUMA_4x16]  = primitives.satd[LUMA_4x16];
    primitives.sa8d_inter[LUMA_8x4]   = primitives.satd[LUMA_8x4];
    primitives.sa8d_inter[LUMA_16x4]  = primitives.satd[LUMA_16x4];
    primitives.sa8d_inter[LUMA_16x12] = primitives.satd[LUMA_16x12];
    primitives.sa8d_inter[LUMA_12x16] = primitives.satd[LUMA_12x16];

    // Chroma SATD can often reuse luma primitives
    p.chroma[X265_CSP_I420].satd[CHROMA_4x4]   = primitives.satd[LUMA_4x4];
    p.chroma[X265_CSP_I420].satd[CHROMA_8x8]   = primitives.satd[LUMA_8x8];
    p.chroma[X265_CSP_I420].satd[CHROMA_16x16] = primitives.satd[LUMA_16x16];
    p.chroma[X265_CSP_I420].satd[CHROMA_32x32] = primitives.satd[LUMA_32x32];

    //p.chroma[X265_CSP_I420].satd[CHROMA_4x2] = sad<4, 2>;
    //p.chroma[X265_CSP_I420].satd[CHROMA_2x4] = sad<2, 4>;
    p.chroma[X265_CSP_I420].satd[CHROMA_8x4]   = primitives.satd[LUMA_8x4];
    p.chroma[X265_CSP_I420].satd[CHROMA_4x8]   = primitives.satd[LUMA_4x8];
    p.chroma[X265_CSP_I420].satd[CHROMA_16x8]  = primitives.satd[LUMA_16x8];
    p.chroma[X265_CSP_I420].satd[CHROMA_8x16]  = primitives.satd[LUMA_8x16];
    p.chroma[X265_CSP_I420].satd[CHROMA_32x16] = primitives.satd[LUMA_32x16];
    p.chroma[X265_CSP_I420].satd[CHROMA_16x32] = primitives.satd[LUMA_16x32];

    //p.chroma[X265_CSP_I420].satd[CHROMA_8x6] = sad<8, 6>;
    //p.chroma[X265_CSP_I420].satd[CHROMA_6x8] = sad<6, 8>;
    //p.chroma[X265_CSP_I420].satd[CHROMA_8x2] = sad<8, 2>;
    //p.chroma[X265_CSP_I420].satd[CHROMA_2x8] = sad<2, 8>;
    p.chroma[X265_CSP_I420].satd[CHROMA_16x12] = primitives.satd[LUMA_16x12];
    p.chroma[X265_CSP_I420].satd[CHROMA_12x16] = primitives.satd[LUMA_12x16];
    p.chroma[X265_CSP_I420].satd[CHROMA_16x4]  = primitives.satd[LUMA_16x4];
    p.chroma[X265_CSP_I420].satd[CHROMA_4x16]  = primitives.satd[LUMA_4x16];
    p.chroma[X265_CSP_I420].satd[CHROMA_32x24] = primitives.satd[LUMA_32x24];
    p.chroma[X265_CSP_I420].satd[CHROMA_24x32] = primitives.satd[LUMA_24x32];
    p.chroma[X265_CSP_I420].satd[CHROMA_32x8]  = primitives.satd[LUMA_32x8];
    p.chroma[X265_CSP_I420].satd[CHROMA_8x32]  = primitives.satd[LUMA_8x32];

    p.chroma[X265_CSP_I422].satd[CHROMA422_4x8]   = primitives.satd[LUMA_4x8];
    p.chroma[X265_CSP_I422].satd[CHROMA422_8x16]  = primitives.satd[LUMA_8x16];
    p.chroma[X265_CSP_I422].satd[CHROMA422_16x32] = primitives.satd[LUMA_16x32];
    p.chroma[X265_CSP_I422].satd[CHROMA422_32x64] = primitives.satd[LUMA_32x64];

    p.chroma[X265_CSP_I422].satd[CHROMA422_4x4]   = primitives.satd[LUMA_4x4];
    //p.chroma[X265_CSP_I422].satd[CHROMA422_2x8] = sad<2, 8>;
    p.chroma[X265_CSP_I422].satd[CHROMA422_8x8]   = primitives.satd[LUMA_8x8];
    p.chroma[X265_CSP_I422].satd[CHROMA422_4x16]  = primitives.satd[LUMA_4x16];
    p.chroma[X265_CSP_I422].satd[CHROMA422_16x16] = primitives.satd[LUMA_16x16];
    p.chroma[X265_CSP_I422].satd[CHROMA422_8x32]  = primitives.satd[LUMA_8x32];
    p.chroma[X265_CSP_I422].satd[CHROMA422_32x32] = primitives.satd[LUMA_32x32];
    p.chroma[X265_CSP_I422].satd[CHROMA422_16x64] = primitives.satd[LUMA_16x64];

    //p.chroma[X265_CSP_I422].satd[CHROMA422_8x12]  = satd4<8, 12>;
    //p.chroma[X265_CSP_I422].satd[CHROMA422_6x16]  = sad<6, 16>;
    p.chroma[X265_CSP_I422].satd[CHROMA422_8x4]   = primitives.satd[LUMA_8x4];
    //p.chroma[X265_CSP_I422].satd[CHROMA422_2x16]  = sad<2, 16>;
    //p.chroma[X265_CSP_I422].satd[CHROMA422_16x24] = satd8<16, 24>;
    //p.chroma[X265_CSP_I422].satd[CHROMA422_12x32] = satd4<12, 32>;
    p.chroma[X265_CSP_I422].satd[CHROMA422_16x8]  = primitives.satd[LUMA_16x8];
    //p.chroma[X265_CSP_I422].satd[CHROMA422_4x32]  = satd4<4, 32>;
    //p.chroma[X265_CSP_I422].satd[CHROMA422_32x48] = satd8<32, 48>;
    //p.chroma[X265_CSP_I422].satd[CHROMA422_24x64] = satd8<24, 64>;
    p.chroma[X265_CSP_I422].satd[CHROMA422_32x16] = primitives.satd[LUMA_32x16];
    //p.chroma[X265_CSP_I422].satd[CHROMA422_8x64]  = satd8<8, 64>;
}
}
using namespace x265;

/* cpuid >= 0 - force CPU type
 * cpuid < 0  - auto-detect if uninitialized */
extern "C"
void x265_setup_primitives(x265_param *param, int cpuid)
{
    if (cpuid < 0)
        cpuid = x265::cpu_detect();

    // initialize global variables
    if (!primitives.sad[0])
    {
        Setup_C_Primitives(primitives);

#if ENABLE_ASSEMBLY
        Setup_Instrinsic_Primitives(primitives, cpuid);
        Setup_Assembly_Primitives(primitives, cpuid);
#else
        x265_log(param, X265_LOG_WARNING, "Assembly not supported in this binary\n");
#endif

        Setup_Alias_Primitives(primitives);
    }

    if (param->logLevel >= X265_LOG_INFO)
    {
        char buf[1000];
        char *p = buf + sprintf(buf, "using cpu capabilities:");
        char *none = p;
        for (int i = 0; x265::cpu_names[i].flags; i++)
        {
            if (!strcmp(x265::cpu_names[i].name, "SSE")
                && (cpuid & X265_CPU_SSE2))
                continue;
            if (!strcmp(x265::cpu_names[i].name, "SSE2")
                && (cpuid & (X265_CPU_SSE2_IS_FAST | X265_CPU_SSE2_IS_SLOW)))
                continue;
            if (!strcmp(x265::cpu_names[i].name, "SSE3")
                && (cpuid & X265_CPU_SSSE3 || !(cpuid & X265_CPU_CACHELINE_64)))
                continue;
            if (!strcmp(x265::cpu_names[i].name, "SSE4.1")
                && (cpuid & X265_CPU_SSE42))
                continue;
            if (!strcmp(x265::cpu_names[i].name, "BMI1")
                && (cpuid & X265_CPU_BMI2))
                continue;
            if ((cpuid & x265::cpu_names[i].flags) == x265::cpu_names[i].flags
                && (!i || x265::cpu_names[i].flags != x265::cpu_names[i - 1].flags))
                p += sprintf(p, " %s", x265::cpu_names[i].name);
        }

        if (p == none)
            sprintf(p, " none!");
        x265_log(param, X265_LOG_INFO, "%s\n", buf);
    }
}

#if ENABLE_ASSEMBLY
/* these functions are implemented in assembly. When assembly is not being
 * compiled, they are unnecessary and can be NOPs */
#else
extern "C" {
int x265_cpu_cpuid_test(void) { return 0; }
void x265_cpu_emms(void) {}
void x265_cpu_cpuid(uint32_t, uint32_t *, uint32_t *, uint32_t *, uint32_t *) {}
void x265_cpu_xgetbv(uint32_t, uint32_t *, uint32_t *) {}
}
#endif
