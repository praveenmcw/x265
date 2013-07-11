/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2013, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TEncSearch.cpp
 \brief    encoder search class
 */

#include "TLibCommon/TypeDef.h"
#include "TLibCommon/TComRom.h"
#include "TLibCommon/TComMotionInfo.h"
#include "TEncSearch.h"
#include "primitives.h"
#include "common.h"
#include "PPA/ppa.h"
#include <math.h>

using namespace x265;

#if CU_STAT_LOGFILE
extern FILE *fp1;
extern bool mergeFlag;
UInt64      meCost;
#endif
DECLARE_CYCLE_COUNTER(ME);

//! \ingroup TLibEncoder
//! \{

static const MV s_mvRefineHpel[9] =
{
    MV(0,  0),  // 0
    MV(0, -1),  // 1
    MV(0,  1),  // 2
    MV(-1,  0), // 3
    MV(1,  0),  // 4
    MV(-1, -1), // 5
    MV(1, -1),  // 6
    MV(-1,  1), // 7
    MV(1,  1)   // 8
};

static const MV s_mvRefineQPel[9] =
{
    MV(0,  0),  // 0
    MV(0, -1),  // 1
    MV(0,  1),  // 2
    MV(-1, -1), // 5
    MV(1, -1),  // 6
    MV(-1,  0), // 3
    MV(1,  0),  // 4
    MV(-1,  1), // 7
    MV(1,  1)   // 8
};

static const UInt s_dFilter[9] =
{
    0, 1, 0,
    2, 3, 2,
    0, 1, 0
};

TEncSearch::TEncSearch()
{
    m_ppcQTTempCoeffY  = NULL;
    m_ppcQTTempCoeffCb = NULL;
    m_ppcQTTempCoeffCr = NULL;
    m_ppcQTTempArlCoeffY  = NULL;
    m_ppcQTTempArlCoeffCb = NULL;
    m_ppcQTTempArlCoeffCr = NULL;
    m_puhQTTempTrIdx   = NULL;
    m_puhQTTempCbf[0] = m_puhQTTempCbf[1] = m_puhQTTempCbf[2] = NULL;
    m_pcQTTempTComYuv  = NULL;
    m_pcEncCfg = NULL;
    m_pcEntropyCoder = NULL;
    m_pTempPel = NULL;
    m_pSharedPredTransformSkip[0] = m_pSharedPredTransformSkip[1] = m_pSharedPredTransformSkip[2] = NULL;
    m_pcQTTempTUCoeffY   = NULL;
    m_pcQTTempTUCoeffCb  = NULL;
    m_pcQTTempTUCoeffCr  = NULL;
    m_ppcQTTempTUArlCoeffY  = NULL;
    m_ppcQTTempTUArlCoeffCb = NULL;
    m_ppcQTTempTUArlCoeffCr = NULL;
    m_puhQTTempTransformSkipFlag[0] = NULL;
    m_puhQTTempTransformSkipFlag[1] = NULL;
    m_puhQTTempTransformSkipFlag[2] = NULL;
    setWpScalingDistParam(NULL, -1, REF_PIC_LIST_X);
}

TEncSearch::~TEncSearch()
{
    if (m_pTempPel)
    {
        delete [] m_pTempPel;
        m_pTempPel = NULL;
    }

    if (m_pcEncCfg)
    {
        const UInt uiNumLayersAllocated = m_pcEncCfg->getQuadtreeTULog2MaxSize() - m_pcEncCfg->getQuadtreeTULog2MinSize() + 1;
        for (UInt ui = 0; ui < uiNumLayersAllocated; ++ui)
        {
            delete[] m_ppcQTTempCoeffY[ui];
            delete[] m_ppcQTTempCoeffCb[ui];
            delete[] m_ppcQTTempCoeffCr[ui];
            delete[] m_ppcQTTempArlCoeffY[ui];
            delete[] m_ppcQTTempArlCoeffCb[ui];
            delete[] m_ppcQTTempArlCoeffCr[ui];
            m_pcQTTempTComYuv[ui].destroy();
        }
    }
    delete[] m_ppcQTTempCoeffY;
    delete[] m_ppcQTTempCoeffCb;
    delete[] m_ppcQTTempCoeffCr;
    delete[] m_ppcQTTempArlCoeffY;
    delete[] m_ppcQTTempArlCoeffCb;
    delete[] m_ppcQTTempArlCoeffCr;
    delete[] m_puhQTTempTrIdx;
    delete[] m_puhQTTempCbf[0];
    delete[] m_puhQTTempCbf[1];
    delete[] m_puhQTTempCbf[2];
    delete[] m_pcQTTempTComYuv;
    delete[] m_pSharedPredTransformSkip[0];
    delete[] m_pSharedPredTransformSkip[1];
    delete[] m_pSharedPredTransformSkip[2];
    delete[] m_pcQTTempTUCoeffY;
    delete[] m_pcQTTempTUCoeffCb;
    delete[] m_pcQTTempTUCoeffCr;
    delete[] m_ppcQTTempTUArlCoeffY;
    delete[] m_ppcQTTempTUArlCoeffCb;
    delete[] m_ppcQTTempTUArlCoeffCr;
    delete[] m_puhQTTempTransformSkipFlag[0];
    delete[] m_puhQTTempTransformSkipFlag[1];
    delete[] m_puhQTTempTransformSkipFlag[2];
    m_pcQTTempTransformSkipTComYuv.destroy();
    m_tmpYuvPred.destroy();
}

Void TEncSearch::init(TEncCfg* pcEncCfg, TComRdCost* pcRdCost, TComTrQuant* pcTrQuant)
{
    m_pcEncCfg          = pcEncCfg;
    m_pcTrQuant         = pcTrQuant;
    m_pcRdCost          = pcRdCost;
    m_iSearchRange      = pcEncCfg->getSearchRange();
    m_bipredSearchRange = pcEncCfg->getBipredSearchRange();
    m_iSearchMethod     = pcEncCfg->getSearchMethod();
    m_pcEntropyCoder    = NULL;
    m_pppcRDSbacCoder   = NULL;
    m_pcRDGoOnSbacCoder = NULL;
    m_me.setSearchMethod(m_iSearchMethod);

    for (Int dir = 0; dir < 2; dir++)
    {
        for (Int ref = 0; ref < 33; ref++)
        {
            m_adaptiveRange[dir][ref] = m_iSearchRange;
        }
    }

    m_puiDFilter = s_dFilter + 4;

    // initialize motion cost
    for (Int iNum = 0; iNum < AMVP_MAX_NUM_CANDS + 1; iNum++)
    {
        for (Int iIdx = 0; iIdx < AMVP_MAX_NUM_CANDS; iIdx++)
        {
            if (iIdx < iNum)
                m_mvpIdxCost[iIdx][iNum] = xGetMvpIdxBits(iIdx, iNum);
            else
                m_mvpIdxCost[iIdx][iNum] = MAX_INT;
        }
    }

    initTempBuff();

    m_pTempPel = new Pel[g_maxCUWidth * g_maxCUHeight];

    const UInt uiNumLayersToAllocate = pcEncCfg->getQuadtreeTULog2MaxSize() - pcEncCfg->getQuadtreeTULog2MinSize() + 1;
    m_ppcQTTempCoeffY  = new TCoeff*[uiNumLayersToAllocate];
    m_ppcQTTempCoeffCb = new TCoeff*[uiNumLayersToAllocate];
    m_ppcQTTempCoeffCr = new TCoeff*[uiNumLayersToAllocate];
    m_ppcQTTempArlCoeffY  = new Int*[uiNumLayersToAllocate];
    m_ppcQTTempArlCoeffCb = new Int*[uiNumLayersToAllocate];
    m_ppcQTTempArlCoeffCr = new Int*[uiNumLayersToAllocate];

    const UInt uiNumPartitions = 1 << (g_maxCUDepth << 1);
    m_puhQTTempTrIdx   = new UChar[uiNumPartitions];
    m_puhQTTempCbf[0]  = new UChar[uiNumPartitions];
    m_puhQTTempCbf[1]  = new UChar[uiNumPartitions];
    m_puhQTTempCbf[2]  = new UChar[uiNumPartitions];
    m_pcQTTempTComYuv  = new TShortYUV[uiNumLayersToAllocate];
    for (UInt ui = 0; ui < uiNumLayersToAllocate; ++ui)
    {
        m_ppcQTTempCoeffY[ui]  = new TCoeff[g_maxCUWidth * g_maxCUHeight];
        m_ppcQTTempCoeffCb[ui] = new TCoeff[g_maxCUWidth * g_maxCUHeight >> 2];
        m_ppcQTTempCoeffCr[ui] = new TCoeff[g_maxCUWidth * g_maxCUHeight >> 2];
        m_ppcQTTempArlCoeffY[ui]  = new Int[g_maxCUWidth * g_maxCUHeight];
        m_ppcQTTempArlCoeffCb[ui] = new Int[g_maxCUWidth * g_maxCUHeight >> 2];
        m_ppcQTTempArlCoeffCr[ui] = new Int[g_maxCUWidth * g_maxCUHeight >> 2];
        m_pcQTTempTComYuv[ui].create(g_maxCUWidth, g_maxCUHeight);
    }

    m_pSharedPredTransformSkip[0] = new Pel[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_pSharedPredTransformSkip[1] = new Pel[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_pSharedPredTransformSkip[2] = new Pel[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_pcQTTempTUCoeffY  = new TCoeff[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_pcQTTempTUCoeffCb = new TCoeff[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_pcQTTempTUCoeffCr = new TCoeff[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_ppcQTTempTUArlCoeffY  = new Int[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_ppcQTTempTUArlCoeffCb = new Int[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_ppcQTTempTUArlCoeffCr = new Int[MAX_TS_WIDTH * MAX_TS_HEIGHT];
    m_pcQTTempTransformSkipTComYuv.create(g_maxCUWidth, g_maxCUHeight);

    m_puhQTTempTransformSkipFlag[0] = new UChar[uiNumPartitions];
    m_puhQTTempTransformSkipFlag[1] = new UChar[uiNumPartitions];
    m_puhQTTempTransformSkipFlag[2] = new UChar[uiNumPartitions];
    m_tmpYuvPred.create(MAX_CU_SIZE, MAX_CU_SIZE);
}

Void TEncSearch::setQPLambda(Int QP, Double lambdaLuma, Double lambdaChroma)
{
    m_me.setQP(QP, lambdaLuma);
    m_bc.setQP(QP, lambdaLuma);
    m_pcRdCost->setLambda(lambdaLuma);
    m_pcTrQuant->setLambda(lambdaLuma, lambdaChroma);
}

inline Void TEncSearch::xTZSearchHelp(TComPattern* patternKey, IntTZSearchStruct& data, Int searchX, Int searchY, UChar pointDir, UInt distance)
{
    Pel*  fref = data.fref + searchY * data.lumaStride + searchX;
    m_pcRdCost->setDistParam(patternKey, fref, data.lumaStride, m_cDistParam);

    if (m_cDistParam.iRows > 12)
    {
        // fast encoder decision: use subsampled SAD when rows > 12 for integer ME
        m_cDistParam.iSubShift = 1;
    }

    // distortion
    m_cDistParam.bitDepth = g_bitDepthY;
    UInt sad = m_cDistParam.DistFunc(&m_cDistParam);

    // motion cost
    sad += m_bc.mvcost(MV(searchX, searchY) << m_pcRdCost->m_iCostScale);

    if (sad < data.bcost)
    {
        data.bcost        = sad;
        data.bestx        = searchX;
        data.besty        = searchY;
        data.bestDistance = distance;
        data.bestRound    = 0;
        data.bestPointDir = pointDir;
    }
}

inline Void TEncSearch::xTZ2PointSearch(TComPattern* patternKey, IntTZSearchStruct& data, MV* mvmin, MV* mvmax)
{
    Int srchRngHorLeft   = mvmin->x;
    Int srchRngHorRight  = mvmax->x;
    Int srchRngVerTop    = mvmin->y;
    Int srchRngVerBottom = mvmax->y;

    // 2 point search,                   //   1 2 3
    // check only the 2 untested points  //   4 0 5
    // around the start point            //   6 7 8
    Int startX = data.bestx;
    Int startY = data.besty;

    switch (data.bestPointDir)
    {
    case 1:
    {
        if ((startX - 1) >= srchRngHorLeft)
        {
            xTZSearchHelp(patternKey, data, startX - 1, startY, 0, 2);
        }
        if ((startY - 1) >= srchRngVerTop)
        {
            xTZSearchHelp(patternKey, data, startX, startY - 1, 0, 2);
        }
    }
    break;
    case 2:
    {
        if ((startY - 1) >= srchRngVerTop)
        {
            if ((startX - 1) >= srchRngHorLeft)
            {
                xTZSearchHelp(patternKey, data, startX - 1, startY - 1, 0, 2);
            }
            if ((startX + 1) <= srchRngHorRight)
            {
                xTZSearchHelp(patternKey, data, startX + 1, startY - 1, 0, 2);
            }
        }
    }
    break;
    case 3:
    {
        if ((startY - 1) >= srchRngVerTop)
        {
            xTZSearchHelp(patternKey, data, startX, startY - 1, 0, 2);
        }
        if ((startX + 1) <= srchRngHorRight)
        {
            xTZSearchHelp(patternKey, data, startX + 1, startY, 0, 2);
        }
    }
    break;
    case 4:
    {
        if ((startX - 1) >= srchRngHorLeft)
        {
            if ((startY + 1) <= srchRngVerBottom)
            {
                xTZSearchHelp(patternKey, data, startX - 1, startY + 1, 0, 2);
            }
            if ((startY - 1) >= srchRngVerTop)
            {
                xTZSearchHelp(patternKey, data, startX - 1, startY - 1, 0, 2);
            }
        }
    }
    break;
    case 5:
    {
        if ((startX + 1) <= srchRngHorRight)
        {
            if ((startY - 1) >= srchRngVerTop)
            {
                xTZSearchHelp(patternKey, data, startX + 1, startY - 1, 0, 2);
            }
            if ((startY + 1) <= srchRngVerBottom)
            {
                xTZSearchHelp(patternKey, data, startX + 1, startY + 1, 0, 2);
            }
        }
    }
    break;
    case 6:
    {
        if ((startX - 1) >= srchRngHorLeft)
        {
            xTZSearchHelp(patternKey, data, startX - 1, startY, 0, 2);
        }
        if ((startY + 1) <= srchRngVerBottom)
        {
            xTZSearchHelp(patternKey, data, startX, startY + 1, 0, 2);
        }
    }
    break;
    case 7:
    {
        if ((startY + 1) <= srchRngVerBottom)
        {
            if ((startX - 1) >= srchRngHorLeft)
            {
                xTZSearchHelp(patternKey, data, startX - 1, startY + 1, 0, 2);
            }
            if ((startX + 1) <= srchRngHorRight)
            {
                xTZSearchHelp(patternKey, data, startX + 1, startY + 1, 0, 2);
            }
        }
    }
    break;
    case 8:
    {
        if ((startX + 1) <= srchRngHorRight)
        {
            xTZSearchHelp(patternKey, data, startX + 1, startY, 0, 2);
        }
        if ((startY + 1) <= srchRngVerBottom)
        {
            xTZSearchHelp(patternKey, data, startX, startY + 1, 0, 2);
        }
    }
    break;
    default:
    {
        assert(false);
    }
    break;
    } // switch( rcStruct.ucPointNr )
}

__inline Void TEncSearch::xTZ8PointDiamondSearch(TComPattern* patternKey, IntTZSearchStruct& data, MV* mvmin, MV* mvmax, Int startX, Int startY, Int distance)
{
    assert(distance != 0);
    Int srchRngHorLeft   = mvmin->x;
    Int srchRngHorRight  = mvmax->x;
    Int srchRngVerTop    = mvmin->y;
    Int srchRngVerBottom = mvmax->y;
    const Int top        = startY - distance;
    const Int bottom     = startY + distance;
    const Int left       = startX - distance;
    const Int right      = startX + distance;
    data.bestRound += 1;

    if (distance == 1) // iDist == 1
    {
        if (top >= srchRngVerTop) // check top
        {
            xTZSearchHelp(patternKey, data, startX, top, 2, distance);
        }
        if (left >= srchRngHorLeft) // check middle left
        {
            xTZSearchHelp(patternKey, data, left, startY, 4, distance);
        }
        if (right <= srchRngHorRight) // check middle right
        {
            xTZSearchHelp(patternKey, data, right, startY, 5, distance);
        }
        if (bottom <= srchRngVerBottom) // check bottom
        {
            xTZSearchHelp(patternKey, data, startX, bottom, 7, distance);
        }
    }
    else if (distance <= 8)
    {
        const Int top2      = startY - (distance >> 1);
        const Int bot2   = startY + (distance >> 1);
        const Int left2     = startX - (distance >> 1);
        const Int right2    = startX + (distance >> 1);

        if (top >= srchRngVerTop && left >= srchRngHorLeft &&
            right <= srchRngHorRight && bottom <= srchRngVerBottom) // check border
        {
            xTZSearchHelp(patternKey, data, startX,   top, 2, distance);
            xTZSearchHelp(patternKey, data, left2,   top2, 1, distance >> 1);
            xTZSearchHelp(patternKey, data, right2,  top2, 3, distance >> 1);
            xTZSearchHelp(patternKey, data, left,  startY, 4, distance);
            xTZSearchHelp(patternKey, data, right, startY, 5, distance);
            xTZSearchHelp(patternKey, data, left2,   bot2, 6, distance >> 1);
            xTZSearchHelp(patternKey, data, right2,  bot2, 8, distance >> 1);
            xTZSearchHelp(patternKey, data, startX, bottom, 7, distance);
        }
        else // check border for each mv
        {
            if (top >= srchRngVerTop) // check top
            {
                xTZSearchHelp(patternKey, data, startX, top, 2, distance);
            }
            if (top2 >= srchRngVerTop) // check half top
            {
                if (left2 >= srchRngHorLeft) // check half left
                {
                    xTZSearchHelp(patternKey, data, left2, top2, 1, (distance >> 1));
                }
                if (right2 <= srchRngHorRight) // check half right
                {
                    xTZSearchHelp(patternKey, data, right2, top2, 3, (distance >> 1));
                }
            } // check half top
            if (left >= srchRngHorLeft) // check left
            {
                xTZSearchHelp(patternKey, data, left, startY, 4, distance);
            }
            if (right <= srchRngHorRight) // check right
            {
                xTZSearchHelp(patternKey, data, right, startY, 5, distance);
            }
            if (bot2 <= srchRngVerBottom) // check half bottom
            {
                if (left2 >= srchRngHorLeft) // check half left
                {
                    xTZSearchHelp(patternKey, data, left2, bot2, 6, (distance >> 1));
                }
                if (right2 <= srchRngHorRight) // check half right
                {
                    xTZSearchHelp(patternKey, data, right2, bot2, 8, (distance >> 1));
                }
            } // check half bottom
            if (bottom <= srchRngVerBottom) // check bottom
            {
                xTZSearchHelp(patternKey, data, startX, bottom, 7, distance);
            }
        } // check border for each mv
    }
    else // iDist > 8
    {
        if (top >= srchRngVerTop && left >= srchRngHorLeft &&
            right <= srchRngHorRight && bottom <= srchRngVerBottom) // check border
        {
            xTZSearchHelp(patternKey, data, startX, top,    0, distance);
            xTZSearchHelp(patternKey, data, left,   startY, 0, distance);
            xTZSearchHelp(patternKey, data, right,  startY, 0, distance);
            xTZSearchHelp(patternKey, data, startX, bottom, 0, distance);
            for (Int index = 1; index < 4; index++)
            {
                Int posYT = top    + ((distance >> 2) * index);
                Int posYB = bottom - ((distance >> 2) * index);
                Int posXL = startX - ((distance >> 2) * index);
                Int PosXR = startX + ((distance >> 2) * index);
                xTZSearchHelp(patternKey, data, posXL, posYT, 0, distance);
                xTZSearchHelp(patternKey, data, PosXR, posYT, 0, distance);
                xTZSearchHelp(patternKey, data, posXL, posYB, 0, distance);
                xTZSearchHelp(patternKey, data, PosXR, posYB, 0, distance);
            }
        }
        else // check border for each mv
        {
            if (top >= srchRngVerTop) // check top
            {
                xTZSearchHelp(patternKey, data, startX, top, 0, distance);
            }
            if (left >= srchRngHorLeft) // check left
            {
                xTZSearchHelp(patternKey, data, left, startY, 0, distance);
            }
            if (right <= srchRngHorRight) // check right
            {
                xTZSearchHelp(patternKey, data, right, startY, 0, distance);
            }
            if (bottom <= srchRngVerBottom) // check bottom
            {
                xTZSearchHelp(patternKey, data, startX, bottom, 0, distance);
            }
            for (Int index = 1; index < 4; index++)
            {
                Int posYT = top    + ((distance >> 2) * index);
                Int posYB = bottom - ((distance >> 2) * index);
                Int posXL = startX - ((distance >> 2) * index);
                Int posXR = startX + ((distance >> 2) * index);

                if (posYT >= srchRngVerTop) // check top
                {
                    if (posXL >= srchRngHorLeft) // check left
                    {
                        xTZSearchHelp(patternKey, data, posXL, posYT, 0, distance);
                    }
                    if (posXR <= srchRngHorRight) // check right
                    {
                        xTZSearchHelp(patternKey, data, posXR, posYT, 0, distance);
                    }
                } // check top
                if (posYB <= srchRngVerBottom) // check bottom
                {
                    if (posXL >= srchRngHorLeft) // check left
                    {
                        xTZSearchHelp(patternKey, data, posXL, posYB, 0, distance);
                    }
                    if (posXR <= srchRngHorRight) // check right
                    {
                        xTZSearchHelp(patternKey, data, posXR, posYB, 0, distance);
                    }
                } // check bottom
            } // for ...
        } // check border for each mv
    } // iDist > 8
}

//<--

UInt TEncSearch::xPatternRefinement(TComPattern* patternKey, MV baseRefMv, Int fracBits, MV& outFracMv, TComPicYuv* refPic, Int offset, TComDataCU* cu, UInt partAddr)
{
    UInt  cost;
    UInt  bcost = MAX_UINT;
    Pel*  fref;
    UInt  bestDir = 0;
    Int   stride = refPic->getStride();

    m_pcRdCost->setDistParam(patternKey, refPic->getLumaFilterBlock(0, 0, cu->getAddr(), cu->getZorderIdxInCU() + partAddr) + offset, stride, 1, m_cDistParam, true);
    m_cDistParam.bitDepth = g_bitDepthY;

    const MV* mvRefine = (fracBits == 2 ? s_mvRefineHpel : s_mvRefineQPel);
    for (int i = 0; i < 9; i++)
    {
        // TODO: this is overly complicated, but it mainly needs to be deleted
        MV cMvTest = baseRefMv + mvRefine[i];

        Int horVal = cMvTest.x * fracBits;
        Int verVal = cMvTest.y * fracBits;
        fref =  refPic->getLumaFilterBlock(verVal & 3, horVal & 3, cu->getAddr(), cu->getZorderIdxInCU() + partAddr) + offset;
        if (horVal < 0)
            fref -= 1;
        if (verVal < 0)
        {
            fref -= stride;
        }
        m_cDistParam.pCur = fref;

        cMvTest = outFracMv + mvRefine[i];
        cost = m_cDistParam.DistFunc(&m_cDistParam) + m_bc.mvcost(cMvTest * fracBits);

        if (cost < bcost)
        {
            bcost  = cost;
            bestDir = i;
        }
    }

    outFracMv = mvRefine[bestDir];
    return bcost;
}

Void TEncSearch::xEncSubdivCbfQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, Bool bLuma, Bool bChroma)
{
    UInt  fullDepth  = cu->getDepth(0) + trDepth;
    UInt  trMode     = cu->getTransformIdx(absPartIdx);
    UInt  subdiv     = (trMode > trDepth ? 1 : 0);
    UInt  trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth()] + 2 - fullDepth;

    if (cu->getPredictionMode(0) == MODE_INTRA && cu->getPartitionSize(0) == SIZE_NxN && trDepth == 0)
    {
        assert(subdiv);
    }
    else if (trSizeLog2 > cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize())
    {
        assert(subdiv);
    }
    else if (trSizeLog2 == cu->getSlice()->getSPS()->getQuadtreeTULog2MinSize())
    {
        assert(!subdiv);
    }
    else if (trSizeLog2 == cu->getQuadtreeTULog2MinSizeInCU(absPartIdx))
    {
        assert(!subdiv);
    }
    else
    {
        assert(trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));
        if (bLuma)
        {
            m_pcEntropyCoder->encodeTransformSubdivFlag(subdiv, 5 - trSizeLog2);
        }
    }

    if (bChroma)
    {
        if (trSizeLog2 > 2)
        {
            if (trDepth == 0 || cu->getCbf(absPartIdx, TEXT_CHROMA_U, trDepth - 1))
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trDepth);
            if (trDepth == 0 || cu->getCbf(absPartIdx, TEXT_CHROMA_V, trDepth - 1))
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trDepth);
        }
    }

    if (subdiv)
    {
        UInt qtPartNum = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
        for (UInt part = 0; part < 4; part++)
        {
            xEncSubdivCbfQT(cu, trDepth + 1, absPartIdx + part * qtPartNum, bLuma, bChroma);
        }

        return;
    }

    //===== Cbfs =====
    if (bLuma)
    {
        m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
    }
}

Void TEncSearch::xEncCoeffQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, TextType ttype)
{
    UInt fullDepth  = cu->getDepth(0) + trDepth;
    UInt trMode     = cu->getTransformIdx(absPartIdx);
    UInt subdiv     = (trMode > trDepth ? 1 : 0);
    UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth()] + 2 - fullDepth;
    UInt chroma     = (ttype != TEXT_LUMA ? 1 : 0);

    if (subdiv)
    {
        UInt qtPartNum = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
        for (UInt part = 0; part < 4; part++)
        {
            xEncCoeffQT(cu, trDepth + 1, absPartIdx + part * qtPartNum, ttype);
        }

        return;
    }

    if (ttype != TEXT_LUMA && trSizeLog2 == 2)
    {
        assert(trDepth > 0);
        trDepth--;
        UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
        Bool bFirstQ = ((absPartIdx % qpdiv) == 0);
        if (!bFirstQ)
        {
            return;
        }
    }

    //===== coefficients =====
    UInt width = cu->getWidth(0) >> (trDepth + chroma);
    UInt height = cu->getHeight(0) >> (trDepth + chroma);
    UInt coeffOffset = (cu->getPic()->getMinCUWidth() * cu->getPic()->getMinCUHeight() * absPartIdx) >> (chroma << 1);
    UInt qtLayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
    TCoeff* coeff = 0;
    switch (ttype)
    {
    case TEXT_LUMA:     coeff = m_ppcQTTempCoeffY[qtLayer];  break;
    case TEXT_CHROMA_U: coeff = m_ppcQTTempCoeffCb[qtLayer]; break;
    case TEXT_CHROMA_V: coeff = m_ppcQTTempCoeffCr[qtLayer]; break;
    default: assert(0);
    }

    coeff += coeffOffset;

    m_pcEntropyCoder->encodeCoeffNxN(cu, coeff, absPartIdx, width, height, fullDepth, ttype);
}

Void TEncSearch::xEncIntraHeader(TComDataCU* cu, UInt trDepth, UInt absPartIdx, Bool bLuma, Bool bChroma)
{
    if (bLuma)
    {
        // CU header
        if (absPartIdx == 0)
        {
            if (!cu->getSlice()->isIntra())
            {
                if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
                {
                    m_pcEntropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
                }
                m_pcEntropyCoder->encodeSkipFlag(cu, 0, true);
                m_pcEntropyCoder->encodePredMode(cu, 0, true);
            }

            m_pcEntropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);

            if (cu->isIntra(0) && cu->getPartitionSize(0) == SIZE_2Nx2N)
            {
                m_pcEntropyCoder->encodeIPCMInfo(cu, 0, true);

                if (cu->getIPCMFlag(0))
                {
                    return;
                }
            }
        }
        // luma prediction mode
        if (cu->getPartitionSize(0) == SIZE_2Nx2N)
        {
            if (absPartIdx == 0)
            {
                m_pcEntropyCoder->encodeIntraDirModeLuma(cu, 0);
            }
        }
        else
        {
            UInt qtNumParts = cu->getTotalNumPart() >> 2;
            if (trDepth == 0)
            {
                assert(absPartIdx == 0);
                for (UInt part = 0; part < 4; part++)
                {
                    m_pcEntropyCoder->encodeIntraDirModeLuma(cu, part * qtNumParts);
                }
            }
            else if ((absPartIdx % qtNumParts) == 0)
            {
                m_pcEntropyCoder->encodeIntraDirModeLuma(cu, absPartIdx);
            }
        }
    }
    if (bChroma)
    {
        // chroma prediction mode
        if (absPartIdx == 0)
        {
            m_pcEntropyCoder->encodeIntraDirModeChroma(cu, 0, true);
        }
    }
}

UInt TEncSearch::xGetIntraBitsQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, Bool bLuma, Bool bChroma)
{
    m_pcEntropyCoder->resetBits();
    xEncIntraHeader(cu, trDepth, absPartIdx, bLuma, bChroma);
    xEncSubdivCbfQT(cu, trDepth, absPartIdx, bLuma, bChroma);

    if (bLuma)
    {
        xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_LUMA);
    }
    if (bChroma)
    {
        xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_U);
        xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_V);
    }
    return m_pcEntropyCoder->getNumberOfWrittenBits();
}

UInt TEncSearch::xGetIntraBitsQTChroma(TComDataCU* cu, UInt trDepth, UInt absPartIdx, UInt chromaId)
{
    m_pcEntropyCoder->resetBits();
    if (chromaId == TEXT_CHROMA_U)
    {
        xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_U);
    }
    else if (chromaId == TEXT_CHROMA_V)
    {
        xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_V);
    }
    return m_pcEntropyCoder->getNumberOfWrittenBits();
}

Void TEncSearch::xIntraCodingLumaBlk(TComDataCU* cu,
                                     UInt        trDepth,
                                     UInt        absPartIdx,
                                     TComYuv*    fencYuv,
                                     TComYuv*    predYuv,
                                     TShortYUV*  resiYuv,
                                     UInt&       outDist,
                                     Int         default0Save1Load2)
{
    UInt    lumaPredMode = cu->getLumaIntraDir(absPartIdx);
    UInt    fullDepth    = cu->getDepth(0)  + trDepth;
    UInt    width        = cu->getWidth(0) >> trDepth;
    UInt    height       = cu->getHeight(0) >> trDepth;
    UInt    stride       = fencYuv->getStride();
    Pel*    fenc         = fencYuv->getLumaAddr(absPartIdx);
    Pel*    pred         = predYuv->getLumaAddr(absPartIdx);
    Short*  residual     = resiYuv->getLumaAddr(absPartIdx);
    Pel*    recon        = predYuv->getLumaAddr(absPartIdx);

    UInt    trSizeLog2     = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
    UInt    qtLayer        = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
    UInt    numCoeffPerInc = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
    TCoeff* coeff          = m_ppcQTTempCoeffY[qtLayer] + numCoeffPerInc * absPartIdx;

    Int*    arlCoeff       = m_ppcQTTempArlCoeffY[qtLayer] + numCoeffPerInc * absPartIdx;
    Short*  reconQt        = m_pcQTTempTComYuv[qtLayer].getLumaAddr(absPartIdx);
    UInt    reconQtStride  = m_pcQTTempTComYuv[qtLayer].width;

    UInt    zorder           = cu->getZorderIdxInCU() + absPartIdx;
    Pel*    reconIPred       = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
    UInt    reconIPredStride = cu->getPic()->getPicYuvRec()->getStride();
    Bool    useTransformSkip = cu->getTransformSkip(absPartIdx, TEXT_LUMA);

    //===== init availability pattern =====

    if (default0Save1Load2 != 2)
    {
        cu->getPattern()->initPattern(cu, trDepth, absPartIdx);
        cu->getPattern()->initAdiPattern(cu, absPartIdx, trDepth, m_piPredBuf, m_iPredBufStride, m_iPredBufHeight, refAbove, refLeft, refAboveFlt, refLeftFlt);
        //===== get prediction signal =====
        predIntraLumaAng(cu->getPattern(), lumaPredMode, pred, stride, width);
        // save prediction
        if (default0Save1Load2 == 1)
        {
            primitives.blockcpy_pp((int)width, height, (pixel*)m_pSharedPredTransformSkip[0], width, (pixel*)pred, stride);
        }
    }
    else
    {
        primitives.blockcpy_pp((int)width, height, (pixel*)pred, stride, (pixel*)m_pSharedPredTransformSkip[0], width);
    }

    //===== get residual signal =====

    primitives.calcresidual[(Int)g_convertToBit[width]]((pixel*)fenc, (pixel*)pred, residual, stride);

    //===== transform and quantization =====
    //--- init rate estimation arrays for RDOQ ---
    if (useTransformSkip ? m_pcEncCfg->getUseRDOQTS() : m_pcEncCfg->getUseRDOQ())
    {
        m_pcEntropyCoder->estimateBit(m_pcTrQuant->m_estBitsSbac, width, width, TEXT_LUMA);
    }

    //--- transform and quantization ---
    UInt absSum = 0;
    cu->setTrIdxSubParts(trDepth, absPartIdx, fullDepth);

    m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

    m_pcTrQuant->selectLambda(TEXT_LUMA);

    absSum = m_pcTrQuant->transformNxN(cu, residual, stride, coeff, arlCoeff, width, height, TEXT_LUMA, absPartIdx, useTransformSkip);

    //--- set coded block flag ---
    cu->setCbfSubParts((absSum ? 1 : 0) << trDepth, TEXT_LUMA, absPartIdx, fullDepth);
    //--- inverse transform ---
    if (absSum)
    {
        Int scalingListType = 0 + g_eTTable[(Int)TEXT_LUMA];
        assert(scalingListType < 6);
        m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_LUMA, cu->getLumaIntraDir(absPartIdx), residual, stride, coeff, width, height, scalingListType, useTransformSkip);
    }
    else
    {
        Short* resiTmp = residual;
        memset(coeff, 0, sizeof(TCoeff) * width * height);
        for (UInt y = 0; y < height; y++)
        {
            memset(resiTmp, 0, sizeof(Short) * width);
            resiTmp += stride;
        }
    }

    //===== reconstruction =====

    primitives.calcrecon[(Int)g_convertToBit[width]]((pixel*)pred, residual, (pixel*)recon, reconQt, (pixel*)reconIPred, stride, reconQtStride, reconIPredStride);

    //===== update distortion =====
    int part = PartitionFromSizes(width, height);
    outDist += primitives.sse_pp[part]((pixel*)fenc, (intptr_t)stride, (pixel*)recon, stride);
}

Void TEncSearch::xIntraCodingChromaBlk(TComDataCU* cu,
                                       UInt        trDepth,
                                       UInt        absPartIdx,
                                       TComYuv*    fencYuv,
                                       TComYuv*    predYuv,
                                       TShortYUV*  resiYuv,
                                       UInt&       outDist,
                                       UInt        chromaId,
                                       Int         default0Save1Load2)
{
    UInt origTrDepth = trDepth;
    UInt fullDepth   = cu->getDepth(0) + trDepth;
    UInt trSizeLog2  = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;

    if (trSizeLog2 == 2)
    {
        assert(trDepth > 0);
        trDepth--;
        UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
        Bool bFirstQ = ((absPartIdx % qpdiv) == 0);
        if (!bFirstQ)
        {
            return;
        }
    }

    TextType ttype          = (chromaId > 0 ? TEXT_CHROMA_V : TEXT_CHROMA_U);
    UInt     chromaPredMode = cu->getChromaIntraDir(absPartIdx);
    UInt     width          = cu->getWidth(0) >> (trDepth + 1);
    UInt     height         = cu->getHeight(0) >> (trDepth + 1);
    UInt     stride         = fencYuv->getCStride();
    Pel*     fenc           = (chromaId > 0 ? fencYuv->getCrAddr(absPartIdx) : fencYuv->getCbAddr(absPartIdx));
    Pel*     pred           = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));
    Short*   residual       = (chromaId > 0 ? resiYuv->getCrAddr(absPartIdx) : resiYuv->getCbAddr(absPartIdx));
    Pel*     recon          = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));

    UInt    qtlayer        = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
    UInt    numCoeffPerInc = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1)) >> 2;
    TCoeff* coeff          = (chromaId > 0 ? m_ppcQTTempCoeffCr[qtlayer] : m_ppcQTTempCoeffCb[qtlayer]) + numCoeffPerInc * absPartIdx;
    Int*    arlCoeff       = (chromaId > 0 ? m_ppcQTTempArlCoeffCr[qtlayer] : m_ppcQTTempArlCoeffCb[qtlayer]) + numCoeffPerInc * absPartIdx;
    Short*  reconQt        = (chromaId > 0 ? m_pcQTTempTComYuv[qtlayer].getCrAddr(absPartIdx) : m_pcQTTempTComYuv[qtlayer].getCbAddr(absPartIdx));
    UInt    reconQtStride  = m_pcQTTempTComYuv[qtlayer].Cwidth;

    UInt    zorder           = cu->getZorderIdxInCU() + absPartIdx;
    Pel*    reconIPred       = (chromaId > 0 ? cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder) : cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder));
    UInt    reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();
    Bool    useTransformSkipChroma = cu->getTransformSkip(absPartIdx, ttype);

    //===== update chroma mode =====
    if (chromaPredMode == DM_CHROMA_IDX)
    {
        chromaPredMode = cu->getLumaIntraDir(0);
    }

    //===== init availability pattern =====
    if (default0Save1Load2 != 2)
    {
        cu->getPattern()->initPattern(cu, trDepth, absPartIdx);

        cu->getPattern()->initAdiPatternChroma(cu, absPartIdx, trDepth, m_piPredBuf, m_iPredBufStride, m_iPredBufHeight);
        Pel* pPatChroma = (chromaId > 0 ? cu->getPattern()->getAdiCrBuf(width, height, m_piPredBuf) : cu->getPattern()->getAdiCbBuf(width, height, m_piPredBuf));

        //===== get prediction signal =====
        predIntraChromaAng(pPatChroma, chromaPredMode, pred, stride, width);

        // save prediction
        if (default0Save1Load2 == 1)
        {
            Pel* predbuf = m_pSharedPredTransformSkip[1 + chromaId];
            primitives.blockcpy_pp((int)width, height, (pixel*)predbuf, width, (pixel*)pred, stride);
        }
    }
    else
    {
        // load prediction
        Pel* predbuf = m_pSharedPredTransformSkip[1 + chromaId];
        primitives.blockcpy_pp((int)width, height, (pixel*)pred, stride, (pixel*)predbuf, width);
    }

    //===== get residual signal =====
    primitives.calcresidual[(Int)g_convertToBit[width]]((pixel*)fenc, (pixel*)pred, residual, stride);

    //===== transform and quantization =====
    {
        //--- init rate estimation arrays for RDOQ ---
        if (useTransformSkipChroma ? m_pcEncCfg->getUseRDOQTS() : m_pcEncCfg->getUseRDOQ())
        {
            m_pcEntropyCoder->estimateBit(m_pcTrQuant->m_estBitsSbac, width, width, ttype);
        }
        //--- transform and quantization ---
        UInt absSum = 0;

        Int curChromaQpOffset;
        if (ttype == TEXT_CHROMA_U)
        {
            curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
        }
        else
        {
            curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
        }
        m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

        m_pcTrQuant->selectLambda(TEXT_CHROMA);

        absSum = m_pcTrQuant->transformNxN(cu, residual, stride, coeff, arlCoeff, width, height, ttype, absPartIdx, useTransformSkipChroma);

        //--- set coded block flag ---
        cu->setCbfSubParts((absSum ? 1 : 0) << origTrDepth, ttype, absPartIdx, cu->getDepth(0) + trDepth);

        //--- inverse transform ---
        if (absSum)
        {
            Int scalingListType = 0 + g_eTTable[(Int)ttype];
            assert(scalingListType < 6);
            m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_CHROMA, REG_DCT, residual, stride, coeff, width, height, scalingListType, useTransformSkipChroma);
        }
        else
        {
            Short* resiTmp = residual;
            memset(coeff, 0, sizeof(TCoeff) * width * height);
            for (UInt y = 0; y < height; y++)
            {
                memset(resiTmp, 0, sizeof(Short) * width);
                resiTmp += stride;
            }
        }
    }

    //===== reconstruction =====
    primitives.calcrecon[(Int)g_convertToBit[width]]((pixel*)pred, residual, (pixel*)recon, reconQt, (pixel*)reconIPred, stride, reconQtStride, reconIPredStride);

    //===== update distortion =====
    int part = x265::PartitionFromSizes(width, height);
    UInt dist = primitives.sse_pp[part]((pixel*)fenc, (intptr_t)stride, (pixel*)recon, stride);
    if (ttype == TEXT_CHROMA_U)
    {
        outDist += m_pcRdCost->scaleChromaDistCb(dist);
    }
    else if (ttype == TEXT_CHROMA_V)
    {
        outDist += m_pcRdCost->scaleChromaDistCr(dist);
    }
    else
    {
        outDist += dist;
    }
}

Void TEncSearch::xRecurIntraCodingQT(TComDataCU* cu,
                                     UInt        trDepth,
                                     UInt        absPartIdx,
                                     Bool        bLumaOnly,
                                     TComYuv*    fencYuv,
                                     TComYuv*    predYuv,
                                     TShortYUV*  resiYuv,
                                     UInt&       outDistY,
                                     UInt&       outDistC,
                                     Bool        bCheckFirst,
                                     UInt64&     rdCost)
{
    UInt    fullDepth   = cu->getDepth(0) +  trDepth;
    UInt    trSizeLog2  = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
    Bool    bCheckFull  = (trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize());
    Bool    bCheckSplit = (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));

    Int maxTuSize = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize();
    Int isIntraSlice = (cu->getSlice()->getSliceType() == I_SLICE);

    // don't check split if TU size is less or equal to max TU size
    Bool noSplitIntraMaxTuSize = bCheckFull;

    if (m_pcEncCfg->getRDpenalty() && !isIntraSlice)
    {
        // in addition don't check split if TU size is less or equal to 16x16 TU size for non-intra slice
        noSplitIntraMaxTuSize = (trSizeLog2  <= min(maxTuSize, 4));

        // if maximum RD-penalty don't check TU size 32x32
        if (m_pcEncCfg->getRDpenalty() == 2)
        {
            bCheckFull = (trSizeLog2  <= min(maxTuSize, 4));
        }
    }
    if (bCheckFirst && noSplitIntraMaxTuSize)
    {
        bCheckSplit = false;
    }

    UInt64 singleCost  = MAX_INT64;
    UInt   singleDistY = 0;
    UInt   singleDistC = 0;
    UInt   singleCbfY  = 0;
    UInt   singleCbfU  = 0;
    UInt   singleCbfV  = 0;
    Bool   checkTransformSkip  = cu->getSlice()->getPPS()->getUseTransformSkip();
    UInt   widthTransformSkip  = cu->getWidth(0) >> trDepth;
    UInt   heightTransformSkip = cu->getHeight(0) >> trDepth;
    Int    bestModeId    = 0;
    Int    bestModeIdUV[2] = { 0, 0 };

    checkTransformSkip &= (widthTransformSkip == 4 && heightTransformSkip == 4);
    checkTransformSkip &= (!cu->getCUTransquantBypass(0));
    checkTransformSkip &= (!((cu->getQP(0) == 0) && (cu->getSlice()->getSPS()->getUseLossless())));
    if (m_pcEncCfg->getUseTransformSkipFast())
    {
        checkTransformSkip &= (cu->getPartitionSize(absPartIdx) == SIZE_NxN);
    }

    if (bCheckFull)
    {
        if (checkTransformSkip == true)
        {
            //----- store original entropy coding status -----
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);

            UInt   singleDistYTmp     = 0;
            UInt   singleDistCTmp     = 0;
            UInt   singleCbfYTmp      = 0;
            UInt   singleCbfUTmp      = 0;
            UInt   singleCbfVTmp      = 0;
            UInt64 singleCostTmp      = 0;
            Int    default0Save1Load2 = 0;
            Int    firstCheckId       = 0;

            UInt   qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + (trDepth - 1)) << 1);
            Bool   bFirstQ = ((absPartIdx % qpdiv) == 0);

            for (Int modeId = firstCheckId; modeId < 2; modeId++)
            {
                singleDistYTmp = 0;
                singleDistCTmp = 0;
                cu->setTransformSkipSubParts(modeId, TEXT_LUMA, absPartIdx, fullDepth);
                if (modeId == firstCheckId)
                {
                    default0Save1Load2 = 1;
                }
                else
                {
                    default0Save1Load2 = 2;
                }
                //----- code luma block with given intra prediction mode and store Cbf-----
                xIntraCodingLumaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistYTmp, default0Save1Load2);
                singleCbfYTmp = cu->getCbf(absPartIdx, TEXT_LUMA, trDepth);
                //----- code chroma blocks with given intra prediction mode and store Cbf-----
                if (!bLumaOnly)
                {
                    if (bFirstQ)
                    {
                        cu->setTransformSkipSubParts(modeId, TEXT_CHROMA_U, absPartIdx, fullDepth);
                        cu->setTransformSkipSubParts(modeId, TEXT_CHROMA_V, absPartIdx, fullDepth);
                    }
                    xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistCTmp, 0, default0Save1Load2);
                    xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistCTmp, 1, default0Save1Load2);
                    singleCbfUTmp = cu->getCbf(absPartIdx, TEXT_CHROMA_U, trDepth);
                    singleCbfVTmp = cu->getCbf(absPartIdx, TEXT_CHROMA_V, trDepth);
                }
                //----- determine rate and r-d cost -----
                if (modeId == 1 && singleCbfYTmp == 0)
                {
                    //In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
                    singleCostTmp = MAX_INT64;
                }
                else
                {
                    UInt singleBits = xGetIntraBitsQT(cu, trDepth, absPartIdx, true, !bLumaOnly);
                    singleCostTmp = m_pcRdCost->calcRdCost(singleDistYTmp + singleDistCTmp, singleBits);
                }

                if (singleCostTmp < singleCost)
                {
                    singleCost   = singleCostTmp;
                    singleDistY = singleDistYTmp;
                    singleDistC = singleDistCTmp;
                    singleCbfY  = singleCbfYTmp;
                    singleCbfU  = singleCbfUTmp;
                    singleCbfV  = singleCbfVTmp;
                    bestModeId    = modeId;
                    if (bestModeId == firstCheckId)
                    {
                        xStoreIntraResultQT(cu, trDepth, absPartIdx, bLumaOnly);
                        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_TEMP_BEST]);
                    }
                }
                if (modeId == firstCheckId)
                {
                    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);
                }
            }

            cu->setTransformSkipSubParts(bestModeId, TEXT_LUMA, absPartIdx, fullDepth);

            if (bestModeId == firstCheckId)
            {
                xLoadIntraResultQT(cu, trDepth, absPartIdx, bLumaOnly);
                cu->setCbfSubParts(singleCbfY << trDepth, TEXT_LUMA, absPartIdx, fullDepth);
                if (!bLumaOnly)
                {
                    if (bFirstQ)
                    {
                        cu->setCbfSubParts(singleCbfU << trDepth, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trDepth - 1);
                        cu->setCbfSubParts(singleCbfV << trDepth, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trDepth - 1);
                    }
                }
                m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_TEMP_BEST]);
            }

            if (!bLumaOnly)
            {
                bestModeIdUV[0] = bestModeIdUV[1] = bestModeId;
                if (bFirstQ && bestModeId == 1)
                {
                    //In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
                    if (singleCbfU == 0)
                    {
                        cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, fullDepth);
                        bestModeIdUV[0] = 0;
                    }
                    if (singleCbfV == 0)
                    {
                        cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, fullDepth);
                        bestModeIdUV[1] = 0;
                    }
                }
            }
        }
        else
        {
            cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, fullDepth);

            //----- store original entropy coding status -----
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);

            //----- code luma block with given intra prediction mode and store Cbf-----
            singleCost = 0;
            xIntraCodingLumaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistY);
            if (bCheckSplit)
            {
                singleCbfY = cu->getCbf(absPartIdx, TEXT_LUMA, trDepth);
            }
            //----- code chroma blocks with given intra prediction mode and store Cbf-----
            if (!bLumaOnly)
            {
                cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, fullDepth);
                cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, fullDepth);
                xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistC, 0);
                xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistC, 1);
                if (bCheckSplit)
                {
                    singleCbfU = cu->getCbf(absPartIdx, TEXT_CHROMA_U, trDepth);
                    singleCbfV = cu->getCbf(absPartIdx, TEXT_CHROMA_V, trDepth);
                }
            }
            //----- determine rate and r-d cost -----
            UInt singleBits = xGetIntraBitsQT(cu, trDepth, absPartIdx, true, !bLumaOnly);
            if (m_pcEncCfg->getRDpenalty() && (trSizeLog2 == 5) && !isIntraSlice)
            {
                singleBits = singleBits * 4;
            }
            singleCost = m_pcRdCost->calcRdCost(singleDistY + singleDistC, singleBits);
        }
    }

    if (bCheckSplit)
    {
        //----- store full entropy coding status, load original entropy coding status -----
        if (bCheckFull)
        {
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_TEST]);
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);
        }
        else
        {
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);
        }

        //----- code splitted block -----
        UInt64 splitCost     = 0;
        UInt   splitDistY    = 0;
        UInt   splitDistC    = 0;
        UInt   qPartsDiv     = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
        UInt   absPartIdxSub = absPartIdx;

        UInt   splitCbfY = 0;
        UInt   splitCbfU = 0;
        UInt   splitCbfV = 0;

        for (UInt part = 0; part < 4; part++, absPartIdxSub += qPartsDiv)
        {
            xRecurIntraCodingQT(cu, trDepth + 1, absPartIdxSub, bLumaOnly, fencYuv, predYuv, resiYuv, splitDistY, splitDistC, bCheckFirst, splitCost);

            splitCbfY |= cu->getCbf(absPartIdxSub, TEXT_LUMA, trDepth + 1);
            if (!bLumaOnly)
            {
                splitCbfU |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_U, trDepth + 1);
                splitCbfV |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_V, trDepth + 1);
            }
        }

        for (UInt offs = 0; offs < 4 * qPartsDiv; offs++)
        {
            cu->getCbf(TEXT_LUMA)[absPartIdx + offs] |= (splitCbfY << trDepth);
        }

        if (!bLumaOnly)
        {
            for (UInt offs = 0; offs < 4 * qPartsDiv; offs++)
            {
                cu->getCbf(TEXT_CHROMA_U)[absPartIdx + offs] |= (splitCbfU << trDepth);
                cu->getCbf(TEXT_CHROMA_V)[absPartIdx + offs] |= (splitCbfV << trDepth);
            }
        }
        //----- restore context states -----
        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);

        //----- determine rate and r-d cost -----
        UInt splitBits = xGetIntraBitsQT(cu, trDepth, absPartIdx, true, !bLumaOnly);
        splitCost = m_pcRdCost->calcRdCost(splitDistY + splitDistC, splitBits);

        //===== compare and set best =====
        if (splitCost < singleCost)
        {
            //--- update cost ---
            outDistY += splitDistY;
            outDistC += splitDistC;
            rdCost   += splitCost;
            return;
        }

        //----- set entropy coding status -----
        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_TEST]);

        //--- set transform index and Cbf values ---
        cu->setTrIdxSubParts(trDepth, absPartIdx, fullDepth);
        cu->setCbfSubParts(singleCbfY << trDepth, TEXT_LUMA, absPartIdx, fullDepth);
        cu->setTransformSkipSubParts(bestModeId, TEXT_LUMA, absPartIdx, fullDepth);
        if (!bLumaOnly)
        {
            cu->setCbfSubParts(singleCbfU << trDepth, TEXT_CHROMA_U, absPartIdx, fullDepth);
            cu->setCbfSubParts(singleCbfV << trDepth, TEXT_CHROMA_V, absPartIdx, fullDepth);
            cu->setTransformSkipSubParts(bestModeIdUV[0], TEXT_CHROMA_U, absPartIdx, fullDepth);
            cu->setTransformSkipSubParts(bestModeIdUV[1], TEXT_CHROMA_V, absPartIdx, fullDepth);
        }

        //--- set reconstruction for next intra prediction blocks ---
        UInt  width     = cu->getWidth(0) >> trDepth;
        UInt  height    = cu->getHeight(0) >> trDepth;
        UInt  qtLayer   = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
        UInt  zorder    = cu->getZorderIdxInCU() + absPartIdx;
        Short* src      = m_pcQTTempTComYuv[qtLayer].getLumaAddr(absPartIdx);
        UInt  srcstride = m_pcQTTempTComYuv[qtLayer].width;
        Pel*  dst       = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
        UInt  dststride = cu->getPic()->getPicYuvRec()->getStride();
        x265::primitives.blockcpy_ps(width, height, dst, dststride, src, srcstride);

        if (!bLumaOnly)
        {
            width >>= 1;
            height >>= 1;
            src       = m_pcQTTempTComYuv[qtLayer].getCbAddr(absPartIdx);
            srcstride = m_pcQTTempTComYuv[qtLayer].Cwidth;
            dst       = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder);
            dststride = cu->getPic()->getPicYuvRec()->getCStride();
            x265::primitives.blockcpy_ps(width, height, dst, dststride, src, srcstride);

            src = m_pcQTTempTComYuv[qtLayer].getCrAddr(absPartIdx);
            dst = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder);
            x265::primitives.blockcpy_ps(width, height, dst, dststride, src, srcstride);
        }
    }

    outDistY += singleDistY;
    outDistC += singleDistC;
    rdCost   += singleCost;
}

Void TEncSearch::xSetIntraResultQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, Bool bLumaOnly, TComYuv* reconYuv)
{
    UInt fullDepth = cu->getDepth(0) + trDepth;
    UInt trMode    = cu->getTransformIdx(absPartIdx);

    if (trMode == trDepth)
    {
        UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
        UInt qtlayer    = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

        Bool bSkipChroma = false;
        Bool bChromaSame = false;
        if (!bLumaOnly && trSizeLog2 == 2)
        {
            assert(trDepth > 0);
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
            bSkipChroma = ((absPartIdx % qpdiv) != 0);
            bChromaSame = true;
        }

        //===== copy transform coefficients =====
        UInt numCoeffY    = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
        UInt numCoeffIncY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
        TCoeff* coeffSrcY = m_ppcQTTempCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
        TCoeff* coeffDestY = cu->getCoeffY()           + (numCoeffIncY * absPartIdx);
        ::memcpy(coeffDestY, coeffSrcY, sizeof(TCoeff) * numCoeffY);
        Int* arlCoeffSrcY = m_ppcQTTempArlCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
        Int* arlCoeffDstY = cu->getArlCoeffY()            + (numCoeffIncY * absPartIdx);
        ::memcpy(arlCoeffDstY, arlCoeffSrcY, sizeof(Int) * numCoeffY);

        if (!bLumaOnly && !bSkipChroma)
        {
            UInt numCoeffC    = (bChromaSame ? numCoeffY    : numCoeffY    >> 2);
            UInt numCoeffIncC = numCoeffIncY >> 2;
            TCoeff* coeffSrcU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
            TCoeff* coeffSrcV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
            TCoeff* coeffDstU = cu->getCoeffCb()            + (numCoeffIncC * absPartIdx);
            TCoeff* coeffDstV = cu->getCoeffCr()            + (numCoeffIncC * absPartIdx);
            ::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff) * numCoeffC);
            ::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff) * numCoeffC);
            Int* arlCoeffSrcU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
            Int* arlCoeffSrcV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
            Int* arlCoeffDstU = cu->getArlCoeffCb()            + (numCoeffIncC * absPartIdx);
            Int* arlCoeffDstV = cu->getArlCoeffCr()            + (numCoeffIncC * absPartIdx);
            ::memcpy(arlCoeffDstU, arlCoeffSrcU, sizeof(Int) * numCoeffC);
            ::memcpy(arlCoeffDstV, arlCoeffSrcV, sizeof(Int) * numCoeffC);
        }

        //===== copy reconstruction =====
        m_pcQTTempTComYuv[qtlayer].copyPartToPartLuma(reconYuv, absPartIdx, 1 << trSizeLog2, 1 << trSizeLog2);
        if (!bLumaOnly && !bSkipChroma)
        {
            UInt trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
            m_pcQTTempTComYuv[qtlayer].copyPartToPartChroma(reconYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
        }
    }
    else
    {
        UInt numQPart = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
        for (UInt part = 0; part < 4; part++)
        {
            xSetIntraResultQT(cu, trDepth + 1, absPartIdx + part * numQPart, bLumaOnly, reconYuv);
        }
    }
}

Void TEncSearch::xStoreIntraResultQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, Bool bLumaOnly)
{
    UInt fullMode = cu->getDepth(0) + trDepth;
    UInt trMode   = cu->getTransformIdx(absPartIdx);

    assert(trMode == trDepth);
    UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullMode] + 2;
    UInt qtlayer    = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

    Bool bSkipChroma  = false;
    Bool bChromaSame  = false;
    if (!bLumaOnly && trSizeLog2 == 2)
    {
        assert(trDepth > 0);
        UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
        bSkipChroma  = ((absPartIdx % qpdiv) != 0);
        bChromaSame  = true;
    }

    //===== copy transform coefficients =====
    UInt numCoeffY    = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullMode << 1);
    UInt numCoeffIncY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
    TCoeff* coeffSrcY = m_ppcQTTempCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
    TCoeff* coeffDstY = m_pcQTTempTUCoeffY;
    ::memcpy(coeffDstY, coeffSrcY, sizeof(TCoeff) * numCoeffY);

    Int* arlCoeffSrcY = m_ppcQTTempArlCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
    Int* arlCoeffDstY = m_ppcQTTempTUArlCoeffY;
    ::memcpy(arlCoeffDstY, arlCoeffSrcY, sizeof(Int) * numCoeffY);

    if (!bLumaOnly && !bSkipChroma)
    {
        UInt numCoeffC    = (bChromaSame ? numCoeffY : numCoeffY >> 2);
        UInt numCoeffIncC = numCoeffIncY >> 2;
        TCoeff* coeffSrcU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
        TCoeff* coeffSrcV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
        TCoeff* coeffDstU = m_pcQTTempTUCoeffCb;
        TCoeff* coeffDstV = m_pcQTTempTUCoeffCr;
        ::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff) * numCoeffC);
        ::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff) * numCoeffC);

        Int* arlCoeffSrcU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
        Int* arlCoeffSrcV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
        Int* arlCoeffDstU = m_ppcQTTempTUArlCoeffCb;
        Int* arlCoeffDstV = m_ppcQTTempTUArlCoeffCr;
        ::memcpy(arlCoeffDstU, arlCoeffSrcU, sizeof(Int) * numCoeffC);
        ::memcpy(arlCoeffDstV, arlCoeffSrcV, sizeof(Int) * numCoeffC);
    }

    //===== copy reconstruction =====
    m_pcQTTempTComYuv[qtlayer].copyPartToPartLuma(&m_pcQTTempTransformSkipTComYuv, absPartIdx, 1 << trSizeLog2, 1 << trSizeLog2);

    if (!bLumaOnly && !bSkipChroma)
    {
        UInt trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
        m_pcQTTempTComYuv[qtlayer].copyPartToPartChroma(&m_pcQTTempTransformSkipTComYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
    }
}

Void TEncSearch::xLoadIntraResultQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, Bool bLumaOnly)
{
    UInt fullDepth = cu->getDepth(0) + trDepth;
    UInt trMode    = cu->getTransformIdx(absPartIdx);

    assert(trMode == trDepth);
    UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
    UInt qtlayer    = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

    Bool bSkipChroma = false;
    Bool bChromaSame = false;
    if (!bLumaOnly && trSizeLog2 == 2)
    {
        assert(trDepth > 0);
        UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
        bSkipChroma = ((absPartIdx % qpdiv) != 0);
        bChromaSame = true;
    }

    //===== copy transform coefficients =====
    UInt numCoeffY    = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
    UInt numCoeffIncY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
    TCoeff* coeffDstY = m_ppcQTTempCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
    TCoeff* coeffSrcY = m_pcQTTempTUCoeffY;
    ::memcpy(coeffDstY, coeffSrcY, sizeof(TCoeff) * numCoeffY);

    Int* arlCoeffDstY = m_ppcQTTempArlCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
    Int* arlCoeffSrcY = m_ppcQTTempTUArlCoeffY;
    ::memcpy(arlCoeffDstY, arlCoeffSrcY, sizeof(Int) * numCoeffY);

    if (!bLumaOnly && !bSkipChroma)
    {
        UInt numCoeffC    = (bChromaSame ? numCoeffY : numCoeffY >> 2);
        UInt numCoeffIncC = numCoeffIncY >> 2;
        TCoeff* coeffDstU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
        TCoeff* coeffDstV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
        TCoeff* coeffSrcU = m_pcQTTempTUCoeffCb;
        TCoeff* coeffSrcV = m_pcQTTempTUCoeffCr;
        ::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff) * numCoeffC);
        ::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff) * numCoeffC);

        Int* arlCoeffDstU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
        Int* arlCoeffDstV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
        Int* arlCoeffSrcU = m_ppcQTTempTUArlCoeffCb;
        Int* arlCoeffSrcV = m_ppcQTTempTUArlCoeffCr;
        ::memcpy(arlCoeffDstU, arlCoeffSrcU, sizeof(Int) * numCoeffC);
        ::memcpy(arlCoeffDstV, arlCoeffSrcV, sizeof(Int) * numCoeffC);
    }

    //===== copy reconstruction =====
    m_pcQTTempTransformSkipTComYuv.copyPartToPartLuma(&m_pcQTTempTComYuv[qtlayer], absPartIdx, 1 << trSizeLog2, 1 << trSizeLog2);

    if (!bLumaOnly && !bSkipChroma)
    {
        UInt trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
        m_pcQTTempTransformSkipTComYuv.copyPartToPartChroma(&m_pcQTTempTComYuv[qtlayer], absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
    }

    UInt   zOrder           = cu->getZorderIdxInCU() + absPartIdx;
    Pel*   reconIPred       = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zOrder);
    UInt   reconIPredStride = cu->getPic()->getPicYuvRec()->getStride();
    Short* reconQt          = m_pcQTTempTComYuv[qtlayer].getLumaAddr(absPartIdx);
    UInt   reconQtStride    = m_pcQTTempTComYuv[qtlayer].width;
    UInt   width            = cu->getWidth(0) >> trDepth;
    UInt   height           = cu->getHeight(0) >> trDepth;
    x265::primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, reconQtStride);

    if (!bLumaOnly && !bSkipChroma)
    {
        width >>= 1;
        height >>= 1;
        reconIPred = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zOrder);
        reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();
        reconQt = m_pcQTTempTComYuv[qtlayer].getCbAddr(absPartIdx);
        reconQtStride = m_pcQTTempTComYuv[qtlayer].Cwidth;
        x265::primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, reconQtStride);

        reconIPred = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zOrder);
        reconQt    = m_pcQTTempTComYuv[qtlayer].getCrAddr(absPartIdx);
        x265::primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, reconQtStride);
    }
}

Void TEncSearch::xStoreIntraResultChromaQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, UInt stateU0V1Both2)
{
    UInt fullDepth = cu->getDepth(0) + trDepth;
    UInt trMode    = cu->getTransformIdx(absPartIdx);

    if (trMode == trDepth)
    {
        UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
        UInt qtlayer    = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

        Bool bChromaSame = false;
        if (trSizeLog2 == 2)
        {
            assert(trDepth > 0);
            trDepth--;
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
            if ((absPartIdx % qpdiv) != 0)
            {
                return;
            }
            bChromaSame = true;
        }

        //===== copy transform coefficients =====
        UInt numCoeffC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
        if (!bChromaSame)
        {
            numCoeffC >>= 2;
        }
        UInt numCoeffIncC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> ((cu->getSlice()->getSPS()->getMaxCUDepth() << 1) + 2);
        if (stateU0V1Both2 == 0 || stateU0V1Both2 == 2)
        {
            TCoeff* coeffSrcU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
            TCoeff* coeffDstU = m_pcQTTempTUCoeffCb;
            ::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff) * numCoeffC);

            Int* arlCoeffSrcU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
            Int* arlCoeffDstU = m_ppcQTTempTUArlCoeffCb;
            ::memcpy(arlCoeffDstU, arlCoeffSrcU, sizeof(Int) * numCoeffC);
        }
        if (stateU0V1Both2 == 1 || stateU0V1Both2 == 2)
        {
            TCoeff* coeffSrcV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
            TCoeff* coeffDstV = m_pcQTTempTUCoeffCr;
            ::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff) * numCoeffC);

            Int* arlCoeffSrcV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
            Int* arlCoeffDstV = m_ppcQTTempTUArlCoeffCr;
            ::memcpy(arlCoeffDstV, arlCoeffSrcV, sizeof(Int) * numCoeffC);
        }

        //===== copy reconstruction =====
        UInt trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
        m_pcQTTempTComYuv[qtlayer].copyPartToPartChroma(&m_pcQTTempTransformSkipTComYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2, stateU0V1Both2);
    }
}

Void TEncSearch::xLoadIntraResultChromaQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, UInt stateU0V1Both2)
{
    UInt fullDepth = cu->getDepth(0) + trDepth;
    UInt trMode    = cu->getTransformIdx(absPartIdx);

    if (trMode == trDepth)
    {
        UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
        UInt qtlayer    = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

        Bool bChromaSame = false;
        if (trSizeLog2 == 2)
        {
            assert(trDepth > 0);
            trDepth--;
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
            if ((absPartIdx % qpdiv) != 0)
            {
                return;
            }
            bChromaSame = true;
        }

        //===== copy transform coefficients =====
        UInt numCoeffC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
        if (!bChromaSame)
        {
            numCoeffC >>= 2;
        }
        UInt numCoeffIncC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> ((cu->getSlice()->getSPS()->getMaxCUDepth() << 1) + 2);

        if (stateU0V1Both2 == 0 || stateU0V1Both2 == 2)
        {
            TCoeff* coeffDstU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
            TCoeff* coeffSrcU = m_pcQTTempTUCoeffCb;
            ::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff) * numCoeffC);

            Int* arlCoeffDstU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
            Int* arlCoeffSrcU = m_ppcQTTempTUArlCoeffCb;
            ::memcpy(arlCoeffDstU, arlCoeffSrcU, sizeof(Int) * numCoeffC);
        }
        if (stateU0V1Both2 == 1 || stateU0V1Both2 == 2)
        {
            TCoeff* coeffDstV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
            TCoeff* coeffSrcV = m_pcQTTempTUCoeffCr;
            ::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff) * numCoeffC);
            
            Int* arlCoeffDstV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
            Int* arlCoeffSrcV = m_ppcQTTempTUArlCoeffCr;
            ::memcpy(arlCoeffDstV, arlCoeffSrcV, sizeof(Int) * numCoeffC);
        }

        //===== copy reconstruction =====
        UInt trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
        m_pcQTTempTransformSkipTComYuv.copyPartToPartChroma(&m_pcQTTempTComYuv[qtlayer], absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2, stateU0V1Both2);

        UInt zorder           = cu->getZorderIdxInCU() + absPartIdx;
        UInt width            = cu->getWidth(0) >> (trDepth + 1);
        UInt height           = cu->getHeight(0) >> (trDepth + 1);
        UInt reconQtStride    = m_pcQTTempTComYuv[qtlayer].Cwidth;
        UInt reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();

        if (stateU0V1Both2 == 0 || stateU0V1Both2 == 2)
        {
            Pel* reconIPred = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder);
            Short* reconQt  = m_pcQTTempTComYuv[qtlayer].getCbAddr(absPartIdx);
            x265::primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, reconQtStride);
        }
        if (stateU0V1Both2 == 1 || stateU0V1Both2 == 2)
        {
            Pel* reconIPred = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder);
            Short* reconQt  = m_pcQTTempTComYuv[qtlayer].getCrAddr(absPartIdx);
            x265::primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, reconQtStride);
        }
    }
}

Void TEncSearch::xRecurIntraChromaCodingQT(TComDataCU* cu,
                                           UInt        trDepth,
                                           UInt        absPartIdx,
                                           TComYuv*    fencYuv,
                                           TComYuv*    predYuv,
                                           TShortYUV*  resiYuv,
                                           UInt&       outDist)
{
    UInt fullDepth = cu->getDepth(0) + trDepth;
    UInt trMode    = cu->getTransformIdx(absPartIdx);

    if (trMode == trDepth)
    {
        Bool checkTransformSkip = cu->getSlice()->getPPS()->getUseTransformSkip();
        UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;

        UInt actualTrDepth = trDepth;
        if (trSizeLog2 == 2)
        {
            assert(trDepth > 0);
            actualTrDepth--;
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + actualTrDepth) << 1);
            Bool bFirstQ = ((absPartIdx % qpdiv) == 0);
            if (!bFirstQ)
            {
                return;
            }
        }

        checkTransformSkip &= (trSizeLog2 <= 3);
        if (m_pcEncCfg->getUseTransformSkipFast())
        {
            checkTransformSkip &= (trSizeLog2 < 3);
            if (checkTransformSkip)
            {
                Int nbLumaSkip = 0;
                for (UInt absPartIdxSub = absPartIdx; absPartIdxSub < absPartIdx + 4; absPartIdxSub++)
                {
                    nbLumaSkip += cu->getTransformSkip(absPartIdxSub, TEXT_LUMA);
                }

                checkTransformSkip &= (nbLumaSkip > 0);
            }
        }

        if (checkTransformSkip)
        {
            // use RDO to decide whether Cr/Cb takes TS
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);

            for (Int chromaId = 0; chromaId < 2; chromaId++)
            {
                UInt64  singleCost     = MAX_INT64;
                Int     bestModeId     = 0;
                UInt    singleDistC    = 0;
                UInt    singleCbfC     = 0;
                UInt    singleDistCTmp = 0;
                UInt64  singleCostTmp  = 0;
                UInt    singleCbfCTmp  = 0;

                Int     default0Save1Load2 = 0;
                Int     firstCheckId       = 0;

                for (Int chromaModeId = firstCheckId; chromaModeId < 2; chromaModeId++)
                {
                    cu->setTransformSkipSubParts(chromaModeId, (TextType)(chromaId + 2), absPartIdx, cu->getDepth(0) + actualTrDepth);
                    if (chromaModeId == firstCheckId)
                    {
                        default0Save1Load2 = 1;
                    }
                    else
                    {
                        default0Save1Load2 = 2;
                    }
                    singleDistCTmp = 0;
                    xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistCTmp, chromaId, default0Save1Load2);
                    singleCbfCTmp = cu->getCbf(absPartIdx, (TextType)(chromaId + 2), trDepth);

                    if (chromaModeId == 1 && singleCbfCTmp == 0)
                    {
                        //In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
                        singleCostTmp = MAX_INT64;
                    }
                    else
                    {
                        UInt bitsTmp = xGetIntraBitsQTChroma(cu, trDepth, absPartIdx, chromaId + 2);
                        singleCostTmp = m_pcRdCost->calcRdCost(singleDistCTmp, bitsTmp);
                    }

                    if (singleCostTmp < singleCost)
                    {
                        singleCost  = singleCostTmp;
                        singleDistC = singleDistCTmp;
                        bestModeId  = chromaModeId;
                        singleCbfC  = singleCbfCTmp;

                        if (bestModeId == firstCheckId)
                        {
                            xStoreIntraResultChromaQT(cu, trDepth, absPartIdx, chromaId);
                            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_TEMP_BEST]);
                        }
                    }
                    if (chromaModeId == firstCheckId)
                    {
                        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);
                    }
                }

                if (bestModeId == firstCheckId)
                {
                    xLoadIntraResultChromaQT(cu, trDepth, absPartIdx, chromaId);
                    cu->setCbfSubParts(singleCbfC << trDepth, (TextType)(chromaId + 2), absPartIdx, cu->getDepth(0) + actualTrDepth);
                    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[fullDepth][CI_TEMP_BEST]);
                }
                cu->setTransformSkipSubParts(bestModeId, (TextType)(chromaId + 2), absPartIdx, cu->getDepth(0) +  actualTrDepth);
                outDist += singleDistC;

                if (chromaId == 0)
                {
                    m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[fullDepth][CI_QT_TRAFO_ROOT]);
                }
            }
        }
        else
        {
            cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) +  actualTrDepth);
            cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) +  actualTrDepth);
            xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, outDist, 0);
            xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, outDist, 1);
        }
    }
    else
    {
        UInt splitCbfU     = 0;
        UInt splitCbfV     = 0;
        UInt qPartsDiv     = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
        UInt absPartIdxSub = absPartIdx;
        for (UInt part = 0; part < 4; part++, absPartIdxSub += qPartsDiv)
        {
            xRecurIntraChromaCodingQT(cu, trDepth + 1, absPartIdxSub, fencYuv, predYuv, resiYuv, outDist);
            splitCbfU |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_U, trDepth + 1);
            splitCbfV |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_V, trDepth + 1);
        }

        for (UInt offs = 0; offs < 4 * qPartsDiv; offs++)
        {
            cu->getCbf(TEXT_CHROMA_U)[absPartIdx + offs] |= (splitCbfU << trDepth);
            cu->getCbf(TEXT_CHROMA_V)[absPartIdx + offs] |= (splitCbfV << trDepth);
        }
    }
}

Void TEncSearch::xSetIntraResultChromaQT(TComDataCU* cu, UInt trDepth, UInt absPartIdx, TComYuv* reconYuv)
{
    UInt fullDepth = cu->getDepth(0) + trDepth;
    UInt trMode    = cu->getTransformIdx(absPartIdx);

    if (trMode == trDepth)
    {
        UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
        UInt qtlayer    = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

        Bool bChromaSame  = false;
        if (trSizeLog2 == 2)
        {
            assert(trDepth > 0);
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
            if ((absPartIdx % qpdiv) != 0)
            {
                return;
            }
            bChromaSame = true;
        }

        //===== copy transform coefficients =====
        UInt numCoeffC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
        if (!bChromaSame)
        {
            numCoeffC >>= 2;
        }
        UInt numCoeffIncC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> ((cu->getSlice()->getSPS()->getMaxCUDepth() << 1) + 2);
        TCoeff* coeffSrcU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
        TCoeff* coeffSrcV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
        TCoeff* coeffDstU = cu->getCoeffCb()            + (numCoeffIncC * absPartIdx);
        TCoeff* coeffDstV = cu->getCoeffCr()            + (numCoeffIncC * absPartIdx);
        ::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff) * numCoeffC);
        ::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff) * numCoeffC);

        Int* arlCoeffSrcU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
        Int* arlCoeffSrcV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
        Int* arlCoeffDstU = cu->getArlCoeffCb()            + (numCoeffIncC * absPartIdx);
        Int* arlCoeffDstV = cu->getArlCoeffCr()            + (numCoeffIncC * absPartIdx);
        ::memcpy(arlCoeffDstU, arlCoeffSrcU, sizeof(Int) * numCoeffC);
        ::memcpy(arlCoeffDstV, arlCoeffSrcV, sizeof(Int) * numCoeffC);

        //===== copy reconstruction =====
        UInt trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
        m_pcQTTempTComYuv[qtlayer].copyPartToPartChroma(reconYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
    }
    else
    {
        UInt numQPart = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
        for (UInt part = 0; part < 4; part++)
        {
            xSetIntraResultChromaQT(cu, trDepth + 1, absPartIdx + part * numQPart, reconYuv);
        }
    }
}

Void TEncSearch::preestChromaPredMode(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv)
{
    UInt width  = cu->getWidth(0) >> 1;
    UInt height = cu->getHeight(0) >> 1;
    UInt stride = fencYuv->getCStride();
    Pel* fencU  = fencYuv->getCbAddr(0);
    Pel* fencV  = fencYuv->getCrAddr(0);
    Pel* predU  = predYuv->getCbAddr(0);
    Pel* predV  = predYuv->getCrAddr(0);

    //===== init pattern =====
    assert(width == height);
    cu->getPattern()->initPattern(cu, 0, 0);
    cu->getPattern()->initAdiPatternChroma(cu, 0, 0, m_piPredBuf, m_iPredBufStride, m_iPredBufHeight);
    Pel* patChromaU = cu->getPattern()->getAdiCbBuf(width, height, m_piPredBuf);
    Pel* patChromaV = cu->getPattern()->getAdiCrBuf(width, height, m_piPredBuf);

    //===== get best prediction modes (using SAD) =====
    UInt minMode  = 0;
    UInt maxMode  = 4;
    UInt bestMode = MAX_UINT;
    UInt minSAD   = MAX_UINT;
    x265::pixelcmp_t sa8d = x265::primitives.sa8d[(int)g_convertToBit[width]];
    for (UInt mode = minMode; mode < maxMode; mode++)
    {
        //--- get prediction ---
        predIntraChromaAng(patChromaU, mode, predU, stride, width);
        predIntraChromaAng(patChromaV, mode, predV, stride, width);

        //--- get SAD ---
        UInt uiSAD = sa8d((pixel*)fencU, stride, (pixel*)predU, stride) +
                     sa8d((pixel*)fencV, stride, (pixel*)predV, stride);

        //--- check ---
        if (uiSAD < minSAD)
        {
            minSAD   = uiSAD;
            bestMode = mode;
        }
    }

    x265_emms();

    //===== set chroma pred mode =====
    cu->setChromIntraDirSubParts(bestMode, 0, cu->getDepth(0));
}

Void TEncSearch::estIntraPredQT(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv, TShortYUV* resiYuv, TComYuv* reconYuv, UInt& outDistC, Bool bLumaOnly)
{
    UInt depth        = cu->getDepth(0);
    UInt numPU        = cu->getNumPartInter();
    UInt initTrDepth  = cu->getPartitionSize(0) == SIZE_2Nx2N ? 0 : 1;
    UInt width        = cu->getWidth(0) >> initTrDepth;
    UInt height       = cu->getHeight(0) >> initTrDepth;
    UInt qNumParts    = cu->getTotalNumPart() >> 2;
    UInt widthBit     = cu->getIntraSizeIdx(0);
    UInt overallDistY = 0;
    UInt overallDistC = 0;
    UInt candNum;
    UInt64 candCostList[FAST_UDI_MAX_RDMODE_NUM];

    assert(width == height);

    //===== set QP and clear Cbf =====
    if (cu->getSlice()->getPPS()->getUseDQP() == true)
    {
        cu->setQPSubParts(cu->getQP(0), 0, depth);
    }
    else
    {
        cu->setQPSubParts(cu->getSlice()->getSliceQp(), 0, depth);
    }

    //===== loop over partitions =====
    UInt partOffset = 0;
    for (UInt pu = 0; pu < numPU; pu++, partOffset += qNumParts)
    {
        //===== init pattern for luma prediction =====
        cu->getPattern()->initPattern(cu, initTrDepth, partOffset);

        // Reference sample smoothing
        cu->getPattern()->initAdiPattern(cu, partOffset, initTrDepth, m_piPredBuf, m_iPredBufStride, m_iPredBufHeight, refAbove, refLeft, refAboveFlt, refLeftFlt);

        //===== determine set of modes to be tested (using prediction signal only) =====
        Int numModesAvailable = 35; //total number of Intra modes
        Pel* fenc   = fencYuv->getLumaAddr(pu, width);
        Pel* pred   = predYuv->getLumaAddr(pu, width);
        UInt stride = predYuv->getStride();
        UInt rdModeList[FAST_UDI_MAX_RDMODE_NUM];
        Int numModesForFullRD = g_intraModeNumFast[widthBit];
        Int log2SizeMinus2 = g_convertToBit[width];
        x265::pixelcmp_t sa8d = x265::primitives.sa8d[log2SizeMinus2];

        Bool doFastSearch = (numModesForFullRD != numModesAvailable);
        if (doFastSearch)
        {
            assert(numModesForFullRD < numModesAvailable);

            for (Int i = 0; i < numModesForFullRD; i++)
            {
                candCostList[i] = MAX_INT64;
            }

            candNum = 0;
            UInt modeCosts[35];
            Bool bFilter = (width <= 16);
            pixel *src = (pixel*) m_piPredBuf;

            // 1
            primitives.intra_pred_dc((pixel*)src + ADI_BUF_STRIDE + 1, ADI_BUF_STRIDE, (pixel*)pred, stride, width, bFilter);
            modeCosts[DC_IDX] = sa8d((pixel*)fenc, stride, (pixel*)pred, stride);

            // 0
            if (width >= 8 && width <= 32)
            {
                src += ADI_BUF_STRIDE * (2 * width + 1);
            }
            primitives.intra_pred_planar((pixel*)src + ADI_BUF_STRIDE + 1, ADI_BUF_STRIDE, (pixel*)pred, stride, width);
            modeCosts[PLANAR_IDX] = sa8d((pixel*)fenc, stride, (pixel*)pred, stride);

            // 33 Angle modes once
            if (width <= 32)
            {
                ALIGN_VAR_32(Pel, buf1[MAX_CU_SIZE * MAX_CU_SIZE]);
                ALIGN_VAR_32(Pel, tmp[33 * MAX_CU_SIZE * MAX_CU_SIZE]);

                // Transpose NxN
                x265::primitives.transpose[log2SizeMinus2]((pixel*)buf1, (pixel*)fenc, stride);

                Pel *pAbove0 = refAbove    + width - 1;
                Pel *pAbove1 = refAboveFlt + width - 1;
                Pel *pLeft0  = refLeft     + width - 1;
                Pel *pLeft1  = refLeftFlt  + width - 1;

                x265::primitives.intra_pred_allangs[log2SizeMinus2]((pixel*)tmp, (pixel*)pAbove0, (pixel*)pLeft0, (pixel*)pAbove1, (pixel*)pLeft1, (width <= 16));

                // TODO: We need SATD_x4 here
                for (UInt mode = 2; mode < numModesAvailable; mode++)
                {
                    bool modeHor = (mode < 18);
                    Pel *src = (modeHor ? buf1 : fenc);
                    intptr_t srcStride = (modeHor ? width : stride);
                    modeCosts[mode] = sa8d((pixel*)src, srcStride, (pixel*)&tmp[(mode - 2) * (width * width)], width);
                }
            }
            else
            {
                for (UInt mode = 2; mode < numModesAvailable; mode++)
                {
                    predIntraLumaAng(cu->getPattern(), mode, pred, stride, width);
                    modeCosts[mode] = sa8d((pixel*)fenc, stride, (pixel*)pred, stride);
                }
            }

            // Find N least cost modes. N = numModesForFullRD
            for (UInt mode = 0; mode < numModesAvailable; mode++)
            {
                UInt sad = modeCosts[mode];
                UInt bits = xModeBitsIntra(cu, mode, pu, partOffset, depth, initTrDepth);
                UInt64 cost = m_pcRdCost->calcRdSADCost(sad, bits);
                candNum += xUpdateCandList(mode, cost, numModesForFullRD, rdModeList, candCostList);
            }

            Int preds[3] = { -1, -1, -1 };
            Int mode = -1;
            Int numCand = cu->getIntraDirLumaPredictor(partOffset, preds, &mode);
            if (mode >= 0)
            {
                numCand = mode;
            }

            for (Int j = 0; j < numCand; j++)
            {
                Bool mostProbableModeIncluded = false;
                Int mostProbableMode = preds[j];

                for (Int i = 0; i < numModesForFullRD; i++)
                {
                    mostProbableModeIncluded |= (mostProbableMode == rdModeList[i]);
                }

                if (!mostProbableModeIncluded)
                {
                    rdModeList[numModesForFullRD++] = mostProbableMode;
                }
            }
        }
        else
        {
            for (Int i = 0; i < numModesForFullRD; i++)
            {
                rdModeList[i] = i;
            }
        }
        x265_emms();

        //===== check modes (using r-d costs) =====
        UInt    bestPUMode  = 0;
        UInt    bestPUDistY = 0;
        UInt    bestPUDistC = 0;
        UInt64  bestPUCost  = MAX_INT64;
        for (UInt mode = 0; mode < numModesForFullRD; mode++)
        {
            // set luma prediction mode
            UInt origMode = rdModeList[mode];

            cu->setLumaIntraDirSubParts(origMode, partOffset, depth + initTrDepth);

            // set context models
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);

            // determine residual for partition
            UInt   puDistY = 0;
            UInt   puDistC = 0;
            UInt64 puCost  = 0;
            xRecurIntraCodingQT(cu, initTrDepth, partOffset, bLumaOnly, fencYuv, predYuv, resiYuv, puDistY, puDistC, true, puCost);

            // check r-d cost
            if (puCost < bestPUCost)
            {
                bestPUMode  = origMode;
                bestPUDistY = puDistY;
                bestPUDistC = puDistC;
                bestPUCost  = puCost;

                xSetIntraResultQT(cu, initTrDepth, partOffset, bLumaOnly, reconYuv);

                UInt qPartNum = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + initTrDepth) << 1);
                ::memcpy(m_puhQTTempTrIdx,  cu->getTransformIdx()     + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempCbf[0], cu->getCbf(TEXT_LUMA)     + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempCbf[1], cu->getCbf(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempCbf[2], cu->getCbf(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempTransformSkipFlag[0], cu->getTransformSkip(TEXT_LUMA)     + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempTransformSkipFlag[1], cu->getTransformSkip(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempTransformSkipFlag[2], cu->getTransformSkip(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
            }
        } // Mode loop

        {
            UInt origMode = bestPUMode;

            cu->setLumaIntraDirSubParts(origMode, partOffset, depth + initTrDepth);

            // set context models
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);

            // determine residual for partition
            UInt   puDistY = 0;
            UInt   puDistC = 0;
            UInt64 puCost  = 0;
            xRecurIntraCodingQT(cu, initTrDepth, partOffset, bLumaOnly, fencYuv, predYuv, resiYuv, puDistY, puDistC, false, puCost);

            // check r-d cost
            if (puCost < bestPUCost)
            {
                bestPUMode  = origMode;
                bestPUDistY = puDistY;
                bestPUDistC = puDistC;
                bestPUCost  = puCost;

                xSetIntraResultQT(cu, initTrDepth, partOffset, bLumaOnly, reconYuv);

                UInt qPartNum = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + initTrDepth) << 1);
                ::memcpy(m_puhQTTempTrIdx,  cu->getTransformIdx()     + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempCbf[0], cu->getCbf(TEXT_LUMA)     + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempCbf[1], cu->getCbf(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempCbf[2], cu->getCbf(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempTransformSkipFlag[0], cu->getTransformSkip(TEXT_LUMA)     + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempTransformSkipFlag[1], cu->getTransformSkip(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
                ::memcpy(m_puhQTTempTransformSkipFlag[2], cu->getTransformSkip(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
            }
        } // Mode loop

        //--- update overall distortion ---
        overallDistY += bestPUDistY;
        overallDistC += bestPUDistC;

        //--- update transform index and cbf ---
        UInt qPartNum = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + initTrDepth) << 1);
        ::memcpy(cu->getTransformIdx()     + partOffset, m_puhQTTempTrIdx,  qPartNum * sizeof(UChar));
        ::memcpy(cu->getCbf(TEXT_LUMA)     + partOffset, m_puhQTTempCbf[0], qPartNum * sizeof(UChar));
        ::memcpy(cu->getCbf(TEXT_CHROMA_U) + partOffset, m_puhQTTempCbf[1], qPartNum * sizeof(UChar));
        ::memcpy(cu->getCbf(TEXT_CHROMA_V) + partOffset, m_puhQTTempCbf[2], qPartNum * sizeof(UChar));
        ::memcpy(cu->getTransformSkip(TEXT_LUMA)     + partOffset, m_puhQTTempTransformSkipFlag[0], qPartNum * sizeof(UChar));
        ::memcpy(cu->getTransformSkip(TEXT_CHROMA_U) + partOffset, m_puhQTTempTransformSkipFlag[1], qPartNum * sizeof(UChar));
        ::memcpy(cu->getTransformSkip(TEXT_CHROMA_V) + partOffset, m_puhQTTempTransformSkipFlag[2], qPartNum * sizeof(UChar));
        //--- set reconstruction for next intra prediction blocks ---
        if (pu != numPU - 1)
        {
            Bool bSkipChroma  = false;
            Bool bChromaSame  = false;
            UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> (cu->getDepth(0) + initTrDepth)] + 2;
            if (!bLumaOnly && trSizeLog2 == 2)
            {
                assert(initTrDepth  > 0);
                bSkipChroma  = (pu != 0);
                bChromaSame  = true;
            }

            UInt compWidth   = cu->getWidth(0) >> initTrDepth;
            UInt compHeight  = cu->getHeight(0) >> initTrDepth;
            UInt zorder      = cu->getZorderIdxInCU() + partOffset;
            Pel* dst         = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
            UInt dststride   = cu->getPic()->getPicYuvRec()->getStride();
            Pel* src         = reconYuv->getLumaAddr(partOffset);
            UInt srcstride   = reconYuv->getStride();
            x265::primitives.blockcpy_pp(compWidth, compHeight, dst, dststride, src, srcstride);

            if (!bLumaOnly && !bSkipChroma)
            {
                if (!bChromaSame)
                {
                    compWidth   >>= 1;
                    compHeight  >>= 1;
                }
                dst         = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder);
                dststride   = cu->getPic()->getPicYuvRec()->getCStride();
                src         = reconYuv->getCbAddr(partOffset);
                srcstride   = reconYuv->getCStride();
                x265::primitives.blockcpy_pp(compWidth, compHeight, dst, dststride, src, srcstride);

                dst         = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder);
                src         = reconYuv->getCrAddr(partOffset);
                x265::primitives.blockcpy_pp(compWidth, compHeight, dst, dststride, src, srcstride);
            }
        }

        //=== update PU data ====
        cu->setLumaIntraDirSubParts(bestPUMode, partOffset, depth + initTrDepth);
        cu->copyToPic(depth, pu, initTrDepth);
    } // PU loop

    if (numPU > 1)
    { // set Cbf for all blocks
        UInt combCbfY = 0;
        UInt combCbfU = 0;
        UInt combCbfV = 0;
        UInt partIdx  = 0;
        for (UInt part = 0; part < 4; part++, partIdx += qNumParts)
        {
            combCbfY |= cu->getCbf(partIdx, TEXT_LUMA,     1);
            combCbfU |= cu->getCbf(partIdx, TEXT_CHROMA_U, 1);
            combCbfV |= cu->getCbf(partIdx, TEXT_CHROMA_V, 1);
        }

        for (UInt offs = 0; offs < 4 * qNumParts; offs++)
        {
            cu->getCbf(TEXT_LUMA)[offs] |= combCbfY;
            cu->getCbf(TEXT_CHROMA_U)[offs] |= combCbfU;
            cu->getCbf(TEXT_CHROMA_V)[offs] |= combCbfV;
        }
    }

    //===== reset context models =====
    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);

    //===== set distortion (rate and r-d costs are determined later) =====
    outDistC                 = overallDistC;
    cu->getTotalDistortion() = overallDistY + overallDistC;
}

Void TEncSearch::estIntraPredChromaQT(TComDataCU* cu,
                                      TComYuv*    fencYuv,
                                      TComYuv*    predYuv,
                                      TShortYUV*  resiYuv,
                                      TComYuv*    reconYuv,
                                      UInt        preCalcDistC)
{
    UInt   depth     = cu->getDepth(0);
    UInt   bestMode  = 0;
    UInt   bestDist  = 0;
    UInt64 bestCost  = MAX_INT64;

    //----- init mode list -----
    UInt minMode = 0;
    UInt maxMode = NUM_CHROMA_MODE;
    UInt modeList[NUM_CHROMA_MODE];
    cu->getAllowedChromaDir(0, modeList);

    //----- check chroma modes -----
    for (UInt mode = minMode; mode < maxMode; mode++)
    {
        //----- restore context models -----
        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);

        //----- chroma coding -----
        UInt dist = 0;
        cu->setChromIntraDirSubParts(modeList[mode], 0, depth);
        xRecurIntraChromaCodingQT(cu, 0, 0, fencYuv, predYuv, resiYuv, dist);
        if (cu->getSlice()->getPPS()->getUseTransformSkip())
        {
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);
        }

        UInt   bits = xGetIntraBitsQT(cu, 0, 0, false, true);
        UInt64 cost  = m_pcRdCost->calcRdCost(dist, bits);

        //----- compare -----
        if (cost < bestCost)
        {
            bestCost = cost;
            bestDist = dist;
            bestMode = modeList[mode];
            UInt qpn = cu->getPic()->getNumPartInCU() >> (depth << 1);
            xSetIntraResultChromaQT(cu, 0, 0, reconYuv);
            ::memcpy(m_puhQTTempCbf[1], cu->getCbf(TEXT_CHROMA_U), qpn * sizeof(UChar));
            ::memcpy(m_puhQTTempCbf[2], cu->getCbf(TEXT_CHROMA_V), qpn * sizeof(UChar));
            ::memcpy(m_puhQTTempTransformSkipFlag[1], cu->getTransformSkip(TEXT_CHROMA_U), qpn * sizeof(UChar));
            ::memcpy(m_puhQTTempTransformSkipFlag[2], cu->getTransformSkip(TEXT_CHROMA_V), qpn * sizeof(UChar));
        }
    }

    //----- set data -----
    UInt qpn = cu->getPic()->getNumPartInCU() >> (depth << 1);
    ::memcpy(cu->getCbf(TEXT_CHROMA_U), m_puhQTTempCbf[1], qpn * sizeof(UChar));
    ::memcpy(cu->getCbf(TEXT_CHROMA_V), m_puhQTTempCbf[2], qpn * sizeof(UChar));
    ::memcpy(cu->getTransformSkip(TEXT_CHROMA_U), m_puhQTTempTransformSkipFlag[1], qpn * sizeof(UChar));
    ::memcpy(cu->getTransformSkip(TEXT_CHROMA_V), m_puhQTTempTransformSkipFlag[2], qpn * sizeof(UChar));
    cu->setChromIntraDirSubParts(bestMode, 0, depth);
    cu->getTotalDistortion() += bestDist - preCalcDistC;

    //----- restore context models -----
    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);
}

/** Function for encoding and reconstructing luma/chroma samples of a PCM mode CU.
 * \param cu pointer to current CU
 * \param absPartIdx part index
 * \param fenc pointer to original sample arrays
 * \param pcm pointer to PCM code arrays
 * \param pred pointer to prediction signal arrays
 * \param resi pointer to residual signal arrays
 * \param reco pointer to reconstructed sample arrays
 * \param stride stride of the original/prediction/residual sample arrays
 * \param width block width
 * \param height block height
 * \param ttText texture component type
 * \returns Void
 */
Void TEncSearch::xEncPCM(TComDataCU* cu, UInt absPartIdx, Pel* fenc, Pel* pcm, Pel* pred, Short* resi, Pel* recon, UInt stride, UInt width, UInt height, TextType eText)
{
    UInt x, y;
    UInt reconStride;
    Pel* pcmTmp = pcm;
    Pel* reconPic;
    Int shiftPcm;

    if (eText == TEXT_LUMA)
    {
        reconStride = cu->getPic()->getPicYuvRec()->getStride();
        reconPic    = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
        shiftPcm = g_bitDepthY - cu->getSlice()->getSPS()->getPCMBitDepthLuma();
    }
    else
    {
        reconStride = cu->getPic()->getPicYuvRec()->getCStride();
        if (eText == TEXT_CHROMA_U)
        {
            reconPic = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
        }
        else
        {
            reconPic = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
        }
        shiftPcm = g_bitDepthC - cu->getSlice()->getSPS()->getPCMBitDepthChroma();
    }

    // zero prediction and residual
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pred[x] = resi[x] = 0;
        }

        pred += stride;
        resi += stride;
    }

    // Encode
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pcmTmp[x] = fenc[x] >> shiftPcm;
        }

        pcmTmp += width;
        fenc += stride;
    }

    pcmTmp = pcm;

    // Reconstruction
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            recon[x] = pcmTmp[x] << shiftPcm;
            reconPic[x] = recon[x];
        }

        pcmTmp   += width;
        recon    += stride;
        reconPic += reconStride;
    }
}

/**  Function for PCM mode estimation.
 * \param cu
 * \param fencYuv
 * \param rpcPredYuv
 * \param rpcResiYuv
 * \param rpcRecoYuv
 * \returns Void
 */
Void TEncSearch::IPCMSearch(TComDataCU* cu, TComYuv* fencYuv, TComYuv*& predYuv, TShortYUV*& resiYuv, TComYuv*& reconYuv)
{
    UInt depth      = cu->getDepth(0);
    UInt width      = cu->getWidth(0);
    UInt height     = cu->getHeight(0);
    UInt stride     = predYuv->getStride();
    UInt strideC    = predYuv->getCStride();
    UInt widthC     = width  >> 1;
    UInt heightC    = height >> 1;
    UInt distortion = 0;
    UInt bits;
    UInt64 cost;

    UInt absPartIdx = 0;
    UInt minCoeffSize = cu->getPic()->getMinCUWidth() * cu->getPic()->getMinCUHeight();
    UInt lumaOffset   = minCoeffSize * absPartIdx;
    UInt chromaOffset = lumaOffset >> 2;

    // Luminance
    Pel*   fenc = fencYuv->getLumaAddr(0, width);
    Short* resi = resiYuv->getLumaAddr(0, width);
    Pel*   pred = predYuv->getLumaAddr(0, width);
    Pel*   recon = reconYuv->getLumaAddr(0, width);
    Pel*   pcm  = cu->getPCMSampleY() + lumaOffset;

    xEncPCM(cu, 0, fenc, pcm, pred, resi, recon, stride, width, height, TEXT_LUMA);

    // Chroma U
    fenc = fencYuv->getCbAddr();
    resi = resiYuv->getCbAddr();
    pred = predYuv->getCbAddr();
    recon = reconYuv->getCbAddr();
    pcm  = cu->getPCMSampleCb() + chromaOffset;

    xEncPCM(cu, 0, fenc, pcm, pred, resi, recon, strideC, widthC, heightC, TEXT_CHROMA_U);

    // Chroma V
    fenc = fencYuv->getCrAddr();
    resi = resiYuv->getCrAddr();
    pred = predYuv->getCrAddr();
    recon = reconYuv->getCrAddr();
    pcm  = cu->getPCMSampleCr() + chromaOffset;

    xEncPCM(cu, 0, fenc, pcm, pred, resi, recon, strideC, widthC, heightC, TEXT_CHROMA_V);

    m_pcEntropyCoder->resetBits();
    xEncIntraHeader(cu, depth, absPartIdx, true, false);
    bits = m_pcEntropyCoder->getNumberOfWrittenBits();
    cost = m_pcRdCost->calcRdCost(distortion, bits);

    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);

    cu->getTotalBits()       = bits;
    cu->getTotalCost()       = cost;
    cu->getTotalDistortion() = distortion;

    cu->copyToPic(depth, 0, 0);
}

UInt TEncSearch::xGetInterPredictionError(TComDataCU* cu, TComYuv* fencYuv, Int partIdx)
{
    UInt absPartIdx;
    Int width, height;

    motionCompensation(cu, &m_tmpYuvPred, REF_PIC_LIST_X, partIdx);
    cu->getPartIndexAndSize(partIdx, absPartIdx, width, height);
    UInt cost = m_me.bufSA8D((pixel*)m_tmpYuvPred.getLumaAddr(absPartIdx), m_tmpYuvPred.getStride());
    x265_emms();
    return cost;
}

/** estimation of best merge coding
 * \param cu
 * \param fencYuv
 * \param iPUIdx
 * \param uiInterDir
 * \param pacMvField
 * \param uiMergeIndex
 * \param outCost
 * \param outBits
 * \param puhNeighCands
 * \param bValid
 * \returns Void
 */
Void TEncSearch::xMergeEstimation(TComDataCU* cu, TComYuv* fencYuv, Int puIdx, UInt& interDir, TComMvField* mvField, UInt& mergeIndex, UInt& outCost, TComMvField* mvFieldNeighbours, UChar* interDirNeighbours, Int& numValidMergeCand)
{
    UInt absPartIdx = 0;
    Int width = 0;
    Int height = 0;

    cu->getPartIndexAndSize(puIdx, absPartIdx, width, height);
    UInt depth = cu->getDepth(absPartIdx);
    PartSize partSize = cu->getPartitionSize(0);
    if (cu->getSlice()->getPPS()->getLog2ParallelMergeLevelMinus2() && partSize != SIZE_2Nx2N && cu->getWidth(0) <= 8)
    {
        cu->setPartSizeSubParts(SIZE_2Nx2N, 0, depth);
        if (puIdx == 0)
        {
            cu->getInterMergeCandidates(0, 0, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);
        }
        cu->setPartSizeSubParts(partSize, 0, depth);
    }
    else
    {
        cu->getInterMergeCandidates(absPartIdx, puIdx, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);
    }
    xRestrictBipredMergeCand(cu, puIdx, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);

    outCost = MAX_UINT;
    for (UInt mergeCand = 0; mergeCand < numValidMergeCand; ++mergeCand)
    {
        UInt costCand = MAX_UINT;
        UInt bitsCand = 0;

        PartSize size = cu->getPartitionSize(0);

        cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(mvFieldNeighbours[0 + 2 * mergeCand], size, absPartIdx, 0, puIdx);
        cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(mvFieldNeighbours[1 + 2 * mergeCand], size, absPartIdx, 0, puIdx);

        costCand = xGetInterPredictionError(cu, fencYuv, puIdx);
        bitsCand = mergeCand + 1;
        if (mergeCand == m_pcEncCfg->getMaxNumMergeCand() - 1)
        {
            bitsCand--;
        }
        costCand = costCand + m_pcRdCost->getCost(bitsCand);
        if (costCand < outCost)
        {
            outCost = costCand;
            mvField[0] = mvFieldNeighbours[0 + 2 * mergeCand];
            mvField[1] = mvFieldNeighbours[1 + 2 * mergeCand];
            interDir = interDirNeighbours[mergeCand];
            mergeIndex = mergeCand;
        }
    }
}

/** convert bi-pred merge candidates to uni-pred
 * \param cu
 * \param puIdx
 * \param mvFieldNeighbours
 * \param interDirNeighbours
 * \param numValidMergeCand
 * \returns Void
 */
Void TEncSearch::xRestrictBipredMergeCand(TComDataCU* cu, UInt puIdx, TComMvField* mvFieldNeighbours, UChar* interDirNeighbours, Int numValidMergeCand)
{
    if (cu->isBipredRestriction(puIdx))
    {
        for (UInt mergeCand = 0; mergeCand < numValidMergeCand; ++mergeCand)
        {
            if (interDirNeighbours[mergeCand] == 3)
            {
                interDirNeighbours[mergeCand] = 1;
                mvFieldNeighbours[(mergeCand << 1) + 1].setMvField(MV(0, 0), -1);
            }
        }
    }
}

/** search of the best candidate for inter prediction
 * \param cu
 * \param fencYuv
 * \param rpcPredYuv
 * \param rpcResiYuv
 * \param rpcRecoYuv
 * \param bUseRes
 * \returns Void
 */
Void TEncSearch::predInterSearch(TComDataCU* cu, TComYuv* fencYuv, TComYuv*& predYuv, Bool bUseMRG)
{
    m_acYuvPred[0].clear();
    m_acYuvPred[1].clear();
    m_cYuvPredTemp.clear();
    predYuv->clear();

    MV mvmin;
    MV mvmax;
    MV mvzero(0, 0);
    MV mvtmp;
    MV mv[2];
    MV mvBidir[2];
    MV mvTemp[2][33];
    MV mvPred[2][33];
    MV mvPredBi[2][33];

    Int mvpIdxBi[2][33];
    Int mvpIdx[2][33];
    Int mvpNum[2][33];
    AMVPInfo AMVPInfo[2][33];

    UInt mbBits[3] = { 1, 1, 0 };
    Int refIdx[2] = { 0, 0 }; // If un-initialized, may cause SEGV in bi-directional prediction iterative stage.
    Int refIdxBidir[2];

    UInt partAddr;
    Int  roiWidth, roiHeight;
    Int refStart, refEnd;

    PartSize partSize = cu->getPartitionSize(0);
    Int bestBiPRefIdxL1 = 0;
    Int bestBiPMvpL1 = 0;
    UInt lastMode = 0;
    Int numPart = cu->getNumPartInter();
    Int numPredDir = cu->getSlice()->isInterP() ? 1 : 2;
    UInt biPDistTemp = MAX_INT;

    /* TODO: this could be as high as TEncSlice::compressSlice() */
    TComPicYuv *fenc = cu->getSlice()->getPic()->getPicYuvOrg();
    m_me.setSourcePlane((pixel*)fenc->getLumaAddr(), fenc->getStride());

    TComMvField mvFieldNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
    UChar interDirNeighbours[MRG_MAX_NUM_CANDS];
    Int numValidMergeCand = 0;

    if (!m_pcEncCfg->getUseRDO())
        cu->getTotalCost() = 0;

    for (Int partIdx = 0; partIdx < numPart; partIdx++)
    {
        UInt bitsTempL0[MAX_NUM_REF];
        UInt listCost[2] = { MAX_UINT, MAX_UINT };
        UInt bits[3];
        UInt costbi = MAX_UINT;
        UInt costTemp;
        UInt bitsTemp;
        UInt bestBiPDist = MAX_INT;
        MV   mvValidList1;
        Int  refIdxValidList1 = 0;
        UInt bitsValidList1 = MAX_UINT;
        UInt costValidList1 = MAX_UINT;
        UInt costTempL0[MAX_NUM_REF];
        for (Int ref = 0; ref < MAX_NUM_REF; ref++)
        {
            costTempL0[ref] = MAX_UINT;
        }

        xGetBlkBits(partSize, cu->getSlice()->isInterP(), partIdx, lastMode, mbBits);
        cu->getPartIndexAndSize(partIdx, partAddr, roiWidth, roiHeight);

        Pel* pu = fenc->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + partAddr);
        m_me.setSourcePU(pu - fenc->getLumaAddr(), roiWidth, roiHeight);

        cu->getMvPredLeft(m_mvPredictors[0]);
        cu->getMvPredAbove(m_mvPredictors[1]);
        cu->getMvPredAboveRight(m_mvPredictors[2]);

        Bool bTestNormalMC = true;

        if (bUseMRG && cu->getWidth(0) > 8 && numPart == 2)
        {
            bTestNormalMC = false;
        }

        if (bTestNormalMC)
        {
            // Uni-directional prediction
            for (Int refList = 0; refList < numPredDir; refList++)
            {
                RefPicList picList = (refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0);

                for (Int refIdxTmp = 0; refIdxTmp < cu->getSlice()->getNumRefIdx(picList); refIdxTmp++)
                {
                    bitsTemp = mbBits[refList];
                    if (cu->getSlice()->getNumRefIdx(picList) > 1)
                    {
                        bitsTemp += refIdxTmp + 1;
                        if (refIdxTmp == cu->getSlice()->getNumRefIdx(picList) - 1) bitsTemp--;
                    }
                    xEstimateMvPredAMVP(cu, fencYuv, partIdx, picList, refIdxTmp, mvPred[refList][refIdxTmp], false, &biPDistTemp);
                    mvpIdx[refList][refIdxTmp] = cu->getMVPIdx(picList, partAddr);
                    mvpNum[refList][refIdxTmp] = cu->getMVPNum(picList, partAddr);

                    if (cu->getSlice()->getMvdL1ZeroFlag() && refList == 1 && biPDistTemp < bestBiPDist)
                    {
                        bestBiPDist = biPDistTemp;
                        bestBiPMvpL1 = mvpIdx[refList][refIdxTmp];
                        bestBiPRefIdxL1 = refIdxTmp;
                    }

                    bitsTemp += m_mvpIdxCost[mvpIdx[refList][refIdxTmp]][AMVP_MAX_NUM_CANDS];

                    if (refList == 1) // list 1
                    {
                        if (cu->getSlice()->getList1IdxToList0Idx(refIdxTmp) >= 0)
                        {
                            mvTemp[1][refIdxTmp] = mvTemp[0][cu->getSlice()->getList1IdxToList0Idx(refIdxTmp)];
                            costTemp = costTempL0[cu->getSlice()->getList1IdxToList0Idx(refIdxTmp)];

                            /* first subtract the bit-rate part of the cost of the other list */
                            costTemp -= m_pcRdCost->getCost(bitsTempL0[cu->getSlice()->getList1IdxToList0Idx(refIdxTmp)]);

                            /* correct the bit-rate part of the current ref */
                            m_me.setMVP(mvPred[refList][refIdxTmp]);
                            bitsTemp += m_me.bitcost(mvTemp[1][refIdxTmp]);

                            /* calculate the correct cost */
                            costTemp += m_pcRdCost->getCost(bitsTemp);
                        }
                        else
                        {
                            if (m_iSearchMethod < X265_ORIG_SEARCH)
                            {
                                CYCLE_COUNTER_START(ME);
                                int merange = m_adaptiveRange[picList][refIdxTmp];
                                MV& mvp = mvPred[refList][refIdxTmp];
                                MV& outmv = mvTemp[refList][refIdxTmp];
                                xSetSearchRange(cu, mvp, merange, mvmin, mvmax);
                                TComPicYuv *refRecon = cu->getSlice()->getRefPic(picList, refIdxTmp)->getPicYuvRec();
                                int satdCost = m_me.motionEstimate(refRecon->getMotionReference(0),
                                                                   mvmin, mvmax, mvp, 3, m_mvPredictors, merange, outmv);

                                /* Get total cost of partition, but only include MV bit cost once */
                                bitsTemp += m_me.bitcost(outmv);
                                costTemp = (satdCost - m_me.mvcost(outmv)) + m_pcRdCost->getCost(bitsTemp);
                                CYCLE_COUNTER_STOP(ME);
                            }
                            else
                                xMotionEstimation(cu, fencYuv, partIdx, picList, &mvPred[refList][refIdxTmp], refIdxTmp, mvTemp[refList][refIdxTmp], bitsTemp, costTemp);
                        }
                    }
                    else
                    {
                        if (m_iSearchMethod < X265_ORIG_SEARCH)
                        {
                            CYCLE_COUNTER_START(ME);
                            int merange = m_adaptiveRange[picList][refIdxTmp];
                            MV& mvp = mvPred[refList][refIdxTmp];
                            MV& outmv = mvTemp[refList][refIdxTmp];
                            xSetSearchRange(cu, mvp, merange, mvmin, mvmax);
                            TComPicYuv *refRecon = cu->getSlice()->getRefPic(picList, refIdxTmp)->getPicYuvRec();
                            int satdCost = m_me.motionEstimate(refRecon->getMotionReference(0),
                                                               mvmin, mvmax, mvp, 3, m_mvPredictors, merange, outmv);

                            /* Get total cost of partition, but only include MV bit cost once */
                            bitsTemp += m_me.bitcost(outmv);
                            costTemp = (satdCost - m_me.mvcost(outmv)) + m_pcRdCost->getCost(bitsTemp);
                            CYCLE_COUNTER_STOP(ME);
                        }
                        else
                            xMotionEstimation(cu, fencYuv, partIdx, picList, &mvPred[refList][refIdxTmp], refIdxTmp, mvTemp[refList][refIdxTmp], bitsTemp, costTemp);
                    }
                    xCopyAMVPInfo(cu->getCUMvField(picList)->getAMVPInfo(), &AMVPInfo[refList][refIdxTmp]); // must always be done ( also when AMVP_MODE = AM_NONE )
                    xCheckBestMVP(cu, picList, mvTemp[refList][refIdxTmp], mvPred[refList][refIdxTmp], mvpIdx[refList][refIdxTmp], bitsTemp, costTemp);

                    if (refList == 0)
                    {
                        costTempL0[refIdxTmp] = costTemp;
                        bitsTempL0[refIdxTmp] = bitsTemp;
                    }
                    if (costTemp < listCost[refList])
                    {
                        listCost[refList] = costTemp;
                        bits[refList] = bitsTemp; // storing for bi-prediction

                        // set motion
                        mv[refList] = mvTemp[refList][refIdxTmp];
                        refIdx[refList] = refIdxTmp;
                    }

                    if (refList == 1 && costTemp < costValidList1 && cu->getSlice()->getList1IdxToList0Idx(refIdxTmp) < 0)
                    {
                        costValidList1 = costTemp;
                        bitsValidList1 = bitsTemp;

                        // set motion
                        mvValidList1     = mvTemp[refList][refIdxTmp];
                        refIdxValidList1 = refIdxTmp;
                    }
                }
            }

            //  Bi-directional prediction
            if ((cu->getSlice()->isInterB()) && (cu->isBipredRestriction(partIdx) == false))
            {
                UInt motBits[2];

                mvBidir[0] = mv[0];
                mvBidir[1] = mv[1];
                refIdxBidir[0] = refIdx[0];
                refIdxBidir[1] = refIdx[1];

                ::memcpy(mvPredBi, mvPred, sizeof(mvPred));
                ::memcpy(mvpIdxBi, mvpIdx, sizeof(mvpIdx));

                if (cu->getSlice()->getMvdL1ZeroFlag())
                {
                    xCopyAMVPInfo(&AMVPInfo[1][bestBiPRefIdxL1], cu->getCUMvField(REF_PIC_LIST_1)->getAMVPInfo());
                    cu->setMVPIdxSubParts(bestBiPMvpL1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
                    mvpIdxBi[1][bestBiPRefIdxL1] = bestBiPMvpL1;
                    mvPredBi[1][bestBiPRefIdxL1] = cu->getCUMvField(REF_PIC_LIST_1)->getAMVPInfo()->m_acMvCand[bestBiPMvpL1];

                    mvBidir[1] = mvPredBi[1][bestBiPRefIdxL1];
                    refIdxBidir[1] = bestBiPRefIdxL1;
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mvBidir[1], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdxBidir[1], partSize, partAddr, 0, partIdx);
                    TComYuv* predYuv = &m_acYuvPred[1];
                    motionCompensation(cu, predYuv, REF_PIC_LIST_1, partIdx);

                    motBits[0] = bits[0] - mbBits[0];
                    motBits[1] = mbBits[1];

                    if (cu->getSlice()->getNumRefIdx(REF_PIC_LIST_1) > 1)
                    {
                        motBits[1] += bestBiPRefIdxL1 + 1;
                        if (bestBiPRefIdxL1 == cu->getSlice()->getNumRefIdx(REF_PIC_LIST_1) - 1) motBits[1]--;
                    }

                    motBits[1] += m_mvpIdxCost[mvpIdxBi[1][bestBiPRefIdxL1]][AMVP_MAX_NUM_CANDS];

                    bits[2] = mbBits[2] + motBits[0] + motBits[1];

                    mvTemp[1][bestBiPRefIdxL1] = mvBidir[1];
                }
                else
                {
                    motBits[0] = bits[0] - mbBits[0];
                    motBits[1] = bits[1] - mbBits[1];
                    bits[2] = mbBits[2] + motBits[0] + motBits[1];
                }

                Int refList = 0;
                if (listCost[0] <= listCost[1])
                {
                    refList = 1;
                }
                else
                {
                    refList = 0;
                }
                if (!cu->getSlice()->getMvdL1ZeroFlag())
                {
                    cu->getCUMvField(RefPicList(1 - refList))->setAllMv(mv[1 - refList], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(RefPicList(1 - refList))->setAllRefIdx(refIdx[1 - refList], partSize, partAddr, 0, partIdx);
                    TComYuv*  predYuv = &m_acYuvPred[1 - refList];
                    motionCompensation(cu, predYuv, RefPicList(1 - refList), partIdx);
                }
                RefPicList  picList = (refList ? REF_PIC_LIST_1 : REF_PIC_LIST_0);

                if (cu->getSlice()->getMvdL1ZeroFlag())
                {
                    refList = 0;
                    picList = REF_PIC_LIST_0;
                }

                Bool bChanged = false;

                refStart = 0;
                refEnd   = cu->getSlice()->getNumRefIdx(picList) - 1;

                for (Int refIdxTmp = refStart; refIdxTmp <= refEnd; refIdxTmp++)
                {
                    bitsTemp = mbBits[2] + motBits[1 - refList];
                    if (cu->getSlice()->getNumRefIdx(picList) > 1)
                    {
                        bitsTemp += refIdxTmp + 1;
                        if (refIdxTmp == cu->getSlice()->getNumRefIdx(picList) - 1) bitsTemp--;
                    }
                    bitsTemp += m_mvpIdxCost[mvpIdxBi[refList][refIdxTmp]][AMVP_MAX_NUM_CANDS];
                    // call bidir ME
                    xMotionEstimation(cu, fencYuv, partIdx, picList, &mvPredBi[refList][refIdxTmp], refIdxTmp, mvTemp[refList][refIdxTmp], bitsTemp, costTemp, true);
                    xCopyAMVPInfo(&AMVPInfo[refList][refIdxTmp], cu->getCUMvField(picList)->getAMVPInfo());
                    xCheckBestMVP(cu, picList, mvTemp[refList][refIdxTmp], mvPredBi[refList][refIdxTmp], mvpIdxBi[refList][refIdxTmp], bitsTemp, costTemp);

                    if (costTemp < costbi)
                    {
                        bChanged = true;

                        mvBidir[refList]     = mvTemp[refList][refIdxTmp];
                        refIdxBidir[refList] = refIdxTmp;

                        costbi           = costTemp;
                        motBits[refList] = bitsTemp - mbBits[2] - motBits[1 - refList];
                        bits[2]          = bitsTemp;
                    }
                } // for loop-refIdxTmp

                if (!bChanged)
                {
                    if (costbi <= listCost[0] && costbi <= listCost[1])
                    {
                        xCopyAMVPInfo(&AMVPInfo[0][refIdxBidir[0]], cu->getCUMvField(REF_PIC_LIST_0)->getAMVPInfo());
                        xCheckBestMVP(cu, REF_PIC_LIST_0, mvBidir[0], mvPredBi[0][refIdxBidir[0]], mvpIdxBi[0][refIdxBidir[0]], bits[2], costbi);
                        if (!cu->getSlice()->getMvdL1ZeroFlag())
                        {
                            xCopyAMVPInfo(&AMVPInfo[1][refIdxBidir[1]], cu->getCUMvField(REF_PIC_LIST_1)->getAMVPInfo());
                            xCheckBestMVP(cu, REF_PIC_LIST_1, mvBidir[1], mvPredBi[1][refIdxBidir[1]], mvpIdxBi[1][refIdxBidir[1]], bits[2], costbi);
                        }
                    }
                }
            } // if (B_SLICE)
        } //end if bTestNormalMC

        //  Clear Motion Field
        cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(TComMvField(), partSize, partAddr, 0, partIdx);
        cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(TComMvField(), partSize, partAddr, 0, partIdx);
        cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);
        cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);

        cu->setMVPIdxSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
        cu->setMVPNumSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
        cu->setMVPIdxSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
        cu->setMVPNumSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

        UInt mebits = 0;
        // Set Motion Field_
        mv[1] = mvValidList1;
        refIdx[1] = refIdxValidList1;
        bits[1] = bitsValidList1;
        listCost[1] = costValidList1;

        if (bTestNormalMC)
        {
            if (costbi <= listCost[0] && costbi <= listCost[1])
            {
                lastMode = 2;
                {
                    cu->getCUMvField(REF_PIC_LIST_0)->setAllMv(mvBidir[0], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(REF_PIC_LIST_0)->setAllRefIdx(refIdxBidir[0], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mvBidir[1], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdxBidir[1], partSize, partAddr, 0, partIdx);
                }
                {
                    mvtmp = mvBidir[0] - mvPredBi[0][refIdxBidir[0]];
                    cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
                }
                {
                    mvtmp = mvBidir[1] - mvPredBi[1][refIdxBidir[1]];
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
                }

                cu->setInterDirSubParts(3, partAddr, partIdx, cu->getDepth(0));

                cu->setMVPIdxSubParts(mvpIdxBi[0][refIdxBidir[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPNumSubParts(mvpNum[0][refIdxBidir[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPIdxSubParts(mvpIdxBi[1][refIdxBidir[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPNumSubParts(mvpNum[1][refIdxBidir[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

                mebits = bits[2];
            }
            else if (listCost[0] <= listCost[1])
            {
                lastMode = 0;
                cu->getCUMvField(REF_PIC_LIST_0)->setAllMv(mv[0], partSize, partAddr, 0, partIdx);
                cu->getCUMvField(REF_PIC_LIST_0)->setAllRefIdx(refIdx[0], partSize, partAddr, 0, partIdx);
                {
                    mvtmp = mv[0] - mvPred[0][refIdx[0]];
                    cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
                }
                cu->setInterDirSubParts(1, partAddr, partIdx, cu->getDepth(0));

                cu->setMVPIdxSubParts(mvpIdx[0][refIdx[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPNumSubParts(mvpNum[0][refIdx[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));

                mebits = bits[0];
            }
            else
            {
                lastMode = 1;
                cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mv[1], partSize, partAddr, 0, partIdx);
                cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdx[1], partSize, partAddr, 0, partIdx);
                {
                    mvtmp = mv[1] - mvPred[1][refIdx[1]];
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
                }
                cu->setInterDirSubParts(2, partAddr, partIdx, cu->getDepth(0));

                cu->setMVPIdxSubParts(mvpIdx[1][refIdx[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPNumSubParts(mvpNum[1][refIdx[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

                mebits = bits[1];
            }
#if CU_STAT_LOGFILE
            meCost += listCost[0];
#endif
        } // end if bTestNormalMC

        if (cu->getPartitionSize(partAddr) != SIZE_2Nx2N)
        {
            UInt msgInterDir = 0;
            TComMvField mrgMvField[2];
            UInt msgIndex = 0;

            UInt meInterDir = 0;
            TComMvField meMvField[2];

            // calculate ME cost
            UInt meError = MAX_UINT;
            UInt meCost = MAX_UINT;

            if (bTestNormalMC)
            {
                meError = xGetInterPredictionError(cu, fencYuv, partIdx);
                meCost = meError + m_pcRdCost->getCost(mebits);
            }

            // save ME result.
            meInterDir = cu->getInterDir(partAddr);
            cu->getMvField(cu, partAddr, REF_PIC_LIST_0, meMvField[0]);
            cu->getMvField(cu, partAddr, REF_PIC_LIST_1, meMvField[1]);

            // find Merge result
            UInt mrgCost = MAX_UINT;
            xMergeEstimation(cu, fencYuv, partIdx, msgInterDir, mrgMvField, msgIndex, mrgCost, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);
            if (mrgCost < meCost)
            {
                // set Merge result
                cu->setMergeFlagSubParts(true, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMergeIndexSubParts(msgIndex, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setInterDirSubParts(msgInterDir, partAddr, partIdx, cu->getDepth(partAddr));
                {
                    cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(mrgMvField[0], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(mrgMvField[1], partSize, partAddr, 0, partIdx);
                }

                cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);
                cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);

                cu->setMVPIdxSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPNumSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPIdxSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setMVPNumSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
#if CU_STAT_LOGFILE
                meCost += mrgCost;
#endif
                if (!m_pcEncCfg->getUseRDO())
                    cu->getTotalCost() += mrgCost;
            }
            else
            {
                // set ME result
                cu->setMergeFlagSubParts(false, partAddr, partIdx, cu->getDepth(partAddr));
                cu->setInterDirSubParts(meInterDir, partAddr, partIdx, cu->getDepth(partAddr));
                {
                    cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(meMvField[0], partSize, partAddr, 0, partIdx);
                    cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(meMvField[1], partSize, partAddr, 0, partIdx);
                }
#if CU_STAT_LOGFILE
                meCost += meCost;
#endif
                if (!m_pcEncCfg->getUseRDO())
                    cu->getTotalCost() += meCost;
            }
        }
        else
        {
            if (!m_pcEncCfg->getUseRDO())
                cu->getTotalCost() += costTemp;
        }
        motionCompensation(cu, predYuv, REF_PIC_LIST_X, partIdx);
    }

    setWpScalingDistParam(cu, -1, REF_PIC_LIST_X);
}

// AMVP
Void TEncSearch::xEstimateMvPredAMVP(TComDataCU* cu, TComYuv* fencYuv, UInt partIdx, RefPicList picList, Int refIfx, MV& mvPred, Bool bFilled, UInt* distBiP)
{
    AMVPInfo* pcAMVPInfo = cu->getCUMvField(picList)->getAMVPInfo();

    MV   bestMv;
    Int  bestIdx = 0;
    UInt bestCost = MAX_INT;
    UInt partAddr = 0;
    Int  roiWidth, roiHeight;
    Int  i;

    cu->getPartIndexAndSize(partIdx, partAddr, roiWidth, roiHeight);
    // Fill the MV Candidates
    if (!bFilled)
    {
        cu->fillMvpCand(partIdx, partAddr, picList, refIfx, pcAMVPInfo);
    }

    bestIdx = 0;
    bestMv  = pcAMVPInfo->m_acMvCand[0];
    if (pcAMVPInfo->iN <= 1)
    {
        mvPred = bestMv;

        cu->setMVPIdxSubParts(bestIdx, picList, partAddr, partIdx, cu->getDepth(partAddr));
        cu->setMVPNumSubParts(pcAMVPInfo->iN, picList, partAddr, partIdx, cu->getDepth(partAddr));

        if (cu->getSlice()->getMvdL1ZeroFlag() && picList == REF_PIC_LIST_1)
        {
            (*distBiP) = xGetTemplateCost(cu, partIdx, partAddr, fencYuv, &m_cYuvPredTemp, mvPred, 0, AMVP_MAX_NUM_CANDS, picList, refIfx, roiWidth, roiHeight);
        }
        return;
    }

    if (bFilled)
    {
        assert(cu->getMVPIdx(picList, partAddr) >= 0);
        mvPred = pcAMVPInfo->m_acMvCand[cu->getMVPIdx(picList, partAddr)];
        return;
    }

    m_cYuvPredTemp.clear();

    //-- Check Minimum Cost.
    for (i = 0; i < pcAMVPInfo->iN; i++)
    {
        UInt cost = xGetTemplateCost(cu, partIdx, partAddr, fencYuv, &m_cYuvPredTemp, pcAMVPInfo->m_acMvCand[i], i, AMVP_MAX_NUM_CANDS, picList, refIfx, roiWidth, roiHeight);
        if (bestCost > cost)
        {
            bestCost = cost;
            bestMv   = pcAMVPInfo->m_acMvCand[i];
            bestIdx  = i;
            (*distBiP) = cost;
        }
    }

    m_cYuvPredTemp.clear();

    // Setting Best MVP
    mvPred = bestMv;
    cu->setMVPIdxSubParts(bestIdx, picList, partAddr, partIdx, cu->getDepth(partAddr));
    cu->setMVPNumSubParts(pcAMVPInfo->iN, picList, partAddr, partIdx, cu->getDepth(partAddr));
}

UInt TEncSearch::xGetMvpIdxBits(Int idx, Int num)
{
    assert(idx >= 0 && num >= 0 && idx < num);

    if (num == 1)
        return 0;

    UInt length = 1;
    Int temp = idx;
    if (temp == 0)
    {
        return length;
    }

    Bool bCodeLast = (num - 1 > temp);

    length += (temp - 1);

    if (bCodeLast)
    {
        length++;
    }

    return length;
}

Void TEncSearch::xGetBlkBits(PartSize cuMode, Bool bPSlice, Int partIdx, UInt lastMode, UInt blockBit[3])
{
    if (cuMode == SIZE_2Nx2N)
    {
        blockBit[0] = (!bPSlice) ? 3 : 1;
        blockBit[1] = 3;
        blockBit[2] = 5;
    }
    else if ((cuMode == SIZE_2NxN || cuMode == SIZE_2NxnU) || cuMode == SIZE_2NxnD)
    {
        UInt aauiMbBits[2][3][3] = { { { 0, 0, 3 }, { 0, 0, 0 }, { 0, 0, 0 } }, { { 5, 7, 7 }, { 7, 5, 7 }, { 9 - 3, 9 - 3, 9 - 3 } } };
        if (bPSlice)
        {
            blockBit[0] = 3;
            blockBit[1] = 0;
            blockBit[2] = 0;
        }
        else
        {
            ::memcpy(blockBit, aauiMbBits[partIdx][lastMode], 3 * sizeof(UInt));
        }
    }
    else if ((cuMode == SIZE_Nx2N || cuMode == SIZE_nLx2N) || cuMode == SIZE_nRx2N)
    {
        UInt aauiMbBits[2][3][3] = { { { 0, 2, 3 }, { 0, 0, 0 }, { 0, 0, 0 } }, { { 5, 7, 7 }, { 7 - 2, 7 - 2, 9 - 2 }, { 9 - 3, 9 - 3, 9 - 3 } } };
        if (bPSlice)
        {
            blockBit[0] = 3;
            blockBit[1] = 0;
            blockBit[2] = 0;
        }
        else
        {
            ::memcpy(blockBit, aauiMbBits[partIdx][lastMode], 3 * sizeof(UInt));
        }
    }
    else if (cuMode == SIZE_NxN)
    {
        blockBit[0] = (!bPSlice) ? 3 : 1;
        blockBit[1] = 3;
        blockBit[2] = 5;
    }
    else
    {
        printf("Wrong!\n");
        assert(0);
    }
}

Void TEncSearch::xCopyAMVPInfo(AMVPInfo* src, AMVPInfo* dst)
{
    dst->iN = src->iN;
    for (Int i = 0; i < src->iN; i++)
    {
        dst->m_acMvCand[i] = src->m_acMvCand[i];
    }
}

/* Check if using an alternative MVP would result in a smaller MVD + signal bits */
Void TEncSearch::xCheckBestMVP(TComDataCU* cu, RefPicList picList, MV mv, MV& mvPred, Int& outMvpIdx, UInt& outBits, UInt& outCost)
{
    AMVPInfo* amvpInfo = cu->getCUMvField(picList)->getAMVPInfo();
    assert(amvpInfo->m_acMvCand[outMvpIdx] == mvPred);
    if (amvpInfo->iN < 2) return;

    m_me.setMVP(mvPred);
    Int bestMvpIdx = outMvpIdx;
    Int mvBitsOrig = m_me.bitcost(mv) + m_mvpIdxCost[outMvpIdx][AMVP_MAX_NUM_CANDS];
    Int bestMvBits = mvBitsOrig;

    for (Int mvpIdx = 0; mvpIdx < amvpInfo->iN; mvpIdx++)
    {
        if (mvpIdx == outMvpIdx)
            continue;

        m_me.setMVP(amvpInfo->m_acMvCand[mvpIdx]);
        Int mvbits = m_me.bitcost(mv) + m_mvpIdxCost[mvpIdx][AMVP_MAX_NUM_CANDS];

        if (mvbits < bestMvBits)
        {
            bestMvBits = mvbits;
            bestMvpIdx = mvpIdx;
        }
    }

    if (bestMvpIdx != outMvpIdx) // if changed
    {
        mvPred = amvpInfo->m_acMvCand[bestMvpIdx];

        outMvpIdx = bestMvpIdx;
        UInt origOutBits = outBits;
        outBits = origOutBits - mvBitsOrig + bestMvBits;
        outCost = (outCost - m_pcRdCost->getCost(origOutBits))  + m_pcRdCost->getCost(outBits);
    }
}

UInt TEncSearch::xGetTemplateCost(TComDataCU* cu,
                                  UInt        partIdx,
                                  UInt        partAddr,
                                  TComYuv*    fencYuv,
                                  TComYuv*    templateCand,
                                  MV          mvCand,
                                  Int         mvpIdx,
                                  Int         mvpCandCount,
                                  RefPicList  picList,
                                  Int         refIfx,
                                  Int         sizex,
                                  Int         sizey)
{
    cu->clipMv(mvCand);

    // prediction pattern
    TComPicYuv* frefYuv = cu->getSlice()->getRefPic(picList, refIfx)->getPicYuvRec();
    if (cu->getSlice()->getPPS()->getUseWP() && cu->getSlice()->getSliceType() == P_SLICE)
    {
        TShortYUV *mbYuv = &m_acShortPred[0];
        xPredInterLumaBlk(cu, frefYuv, partAddr, &mvCand, sizex, sizey, mbYuv, true);
        xWeightedPredictionUni(cu, mbYuv, partAddr, sizex, sizey, picList, templateCand, refIfx);
    }
    else
    {
        xPredInterLumaBlk(cu, frefYuv, partAddr, &mvCand, sizex, sizey, templateCand, false);
    }

    // calc distortion
    UInt cost = m_me.bufSAD((pixel*)templateCand->getLumaAddr(partAddr), templateCand->getStride());
    x265_emms();
    return m_pcRdCost->calcRdSADCost(cost, m_mvpIdxCost[mvpIdx][mvpCandCount]);
}

Void TEncSearch::xMotionEstimation(TComDataCU* cu, TComYuv* fencYuv, Int partIdx, RefPicList picList, MV* mvp, Int refIdxPred, MV& outmv, UInt& outBits, UInt& outCost, Bool bi)
{
    CYCLE_COUNTER_START(ME);

    m_iSearchRange = m_adaptiveRange[picList][refIdxPred];

    Int merange = (bi ? m_bipredSearchRange : m_iSearchRange);

    UInt partAddr;
    Int width, height;
    cu->getPartIndexAndSize(partIdx, partAddr, width, height);

    TComYuv* yuv = fencYuv;
    int cost_shift = 0;
    if (bi)
    {
        TComYuv* yuvOther = &m_acYuvPred[1 - (Int)picList];
        yuv = &m_cYuvPredTemp;
        fencYuv->copyPartToPartYuv(yuv, partAddr, width, height);
        yuv->removeHighFreq(yuvOther, partAddr, width, height);
        cost_shift = 1;
    }

    // Search key pattern initialization
    TComPattern* patternKey = cu->getPattern();
    patternKey->initPattern(yuv->getLumaAddr(partAddr), yuv->getCbAddr(partAddr), yuv->getCrAddr(partAddr), width, height, yuv->getStride(), 0, 0);

    MV cMvPred = *mvp;
    MV mvmin, mvmax;

    if (bi)
        xSetSearchRange(cu, outmv, merange, mvmin, mvmax);
    else
        xSetSearchRange(cu, cMvPred, merange, mvmin, mvmax);

    setWpScalingDistParam(cu, refIdxPred, picList);

    Pel* fref = cu->getSlice()->getRefPic(picList, refIdxPred)->getPicYuvRec()->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + partAddr);
    Int  stride  = cu->getSlice()->getRefPic(picList, refIdxPred)->getPicYuvRec()->getStride();

    // Configure the MV bit cost calculator
    m_bc.setMVP(*mvp);

    // Do integer search
    m_pcRdCost->setCostScale(2);
    if (bi || m_iSearchMethod == X265_FULL_SEARCH)
    {
        xPatternSearch(patternKey, fref, stride, &mvmin, &mvmax, outmv, outCost);
    }
    else
    {
        outmv = *mvp;
        xPatternSearchFast(cu, patternKey, fref, stride, &mvmin, &mvmax, outmv, outCost);
    }

    TComPicYuv* refPic = cu->getSlice()->getRefPic(picList, refIdxPred)->getPicYuvRec(); //For new xPatternSearchFracDiff
    m_pcRdCost->setCostScale(1);
    MV mvHpel, mvQpel;
    xPatternSearchFracDIF(cu, patternKey, fref, stride, &outmv, mvHpel, mvQpel, outCost, bi, refPic, partAddr);
    m_pcRdCost->setCostScale(0);

    outmv <<= 2;
    outmv += (mvHpel <<= 1);
    outmv += mvQpel;

    UInt mvbits = m_bc.bitcost(outmv);

    outBits += mvbits;
    outCost = ((outCost - m_pcRdCost->getCost(mvbits)) >> cost_shift) + m_pcRdCost->getCost(outBits);

    CYCLE_COUNTER_STOP(ME);
}

Void TEncSearch::xSetSearchRange(TComDataCU* cu, MV mvp, Int merange, MV& mvmin, MV& mvmax)
{
    cu->clipMv(mvp);

    MV dist(merange << 2, merange << 2);
    mvmin = mvp - dist;
    mvmax = mvp + dist;

    cu->clipMv(mvmin);
    cu->clipMv(mvmax);

    mvmin >>= 2;
    mvmax >>= 2;
}

Void TEncSearch::xPatternSearch(TComPattern* patternKey, Pel* refY, Int stride, MV* mvmin, MV* mvmax, MV& outmv, UInt& outcost)
{
    Int srchRngHorLeft   = mvmin->x;
    Int srchRngHorRight  = mvmax->x;
    Int srchRngVerTop    = mvmin->y;
    Int srchRngVerBottom = mvmax->y;

    m_pcRdCost->setDistParam(patternKey, refY, stride, m_cDistParam);
    m_cDistParam.bitDepth = g_bitDepthY;
    refY += (srchRngVerTop * stride);

    // find min. distortion position
    UInt bcost = MAX_UINT;
    for (Int y = srchRngVerTop; y <= srchRngVerBottom; y++)
    {
        for (Int x = srchRngHorLeft; x <= srchRngHorRight; x++)
        {
            MV mv(x, y);
            m_cDistParam.pCur = refY + x;
            UInt cost = m_cDistParam.DistFunc(&m_cDistParam) + m_bc.mvcost(mv << 2);

            if (cost < bcost)
            {
                bcost = cost;
                mv = mv;
            }
        }
        refY += stride;
    }
    outcost = bcost - m_bc.mvcost(outmv << 2);
}

Void TEncSearch::xPatternSearchFast(TComDataCU* cu, TComPattern* patternKey, Pel* refY, Int stride, MV* mvmin, MV* mvmax, MV& outmv, UInt& outcost)
{
    const Int  rasterThreshold = 5;
    const Bool firstSearchStop = 1;
    const UInt firstSearchRounds = 3; /* first search stop X rounds after best match (must be >=1) */
    const UInt starRefinementRounds = 2; /* star refinement stop X rounds after best match (must be >=1) */

    Int srchRngHorLeft   = mvmin->x;
    Int srchRngHorRight  = mvmax->x;
    Int srchRngVerTop    = mvmin->y;
    Int srchRngVerBottom = mvmax->y;
    UInt merange = m_iSearchRange;

    cu->clipMv(outmv);
    outmv >>= 2;

    // init TZSearchStruct
    IntTZSearchStruct data;
    data.lumaStride = stride;
    data.fref       = refY;
    data.bcost      = MAX_UINT;

    // set outmv (Median predictor) as start point and as best point
    xTZSearchHelp(patternKey, data, outmv.x, outmv.y, 0, 0);

    // test whether zero Mv is better start point than Median predictor
    xTZSearchHelp(patternKey, data, 0, 0, 0, 0);

    // start search
    Int distance = 0;
    Int startx = data.bestx;
    Int starty = data.besty;

    // first search
    for (distance = 1; distance <= (Int)merange; distance *= 2)
    {
        xTZ8PointDiamondSearch(patternKey, data, mvmin, mvmax, startx, starty, distance);

        if (firstSearchStop && (data.bestRound >= firstSearchRounds)) // stop criterion
        {
            break;
        }
    }

    // calculate only 2 missing points instead 8 points if cStruct.uiBestDistance == 1
    if (data.bestDistance == 1)
    {
        data.bestDistance = 0;
        xTZ2PointSearch(patternKey, data, mvmin, mvmax);
    }

    // raster search if distance is too big
    if ((Int)(data.bestDistance) > rasterThreshold)
    {
        data.bestDistance = rasterThreshold;
        for (starty = srchRngVerTop; starty <= srchRngVerBottom; starty += rasterThreshold)
        {
            for (startx = srchRngHorLeft; startx <= srchRngHorRight; startx += rasterThreshold)
            {
                xTZSearchHelp(patternKey, data, startx, starty, 0, rasterThreshold);
            }
        }
    }

    // star refinement
    while (data.bestDistance > 0)
    {
        startx = data.bestx;
        starty = data.besty;
        data.bestDistance = 0;
        data.bestPointDir = 0;
        for (distance = 1; distance < (Int)merange + 1; distance *= 2)
        {
            xTZ8PointDiamondSearch(patternKey, data, mvmin, mvmax, startx, starty, distance);
        }

        // calculate only 2 missing points instead 8 points if cStrukt.uiBestDistance == 1
        if (data.bestDistance == 1)
        {
            data.bestDistance = 0;
            if (data.bestPointDir != 0)
            {
                xTZ2PointSearch(patternKey, data, mvmin, mvmax);
            }
        }
    }

    // write out best match
    outmv = MV(data.bestx, data.besty);
    outcost = data.bcost - m_bc.mvcost(outmv << 2);
}

Void TEncSearch::xPatternSearchFracDIF(TComDataCU*  cu,
                                       TComPattern* patternKey,
                                       Pel*         refY,
                                       Int          stride,
                                       MV*          mvfpel,
                                       MV&          outMvHPel,
                                       MV&          outMvQPel,
                                       UInt&        outCost,
                                       Bool         biPred,
                                       TComPicYuv * refPic,
                                       UInt         partAddr)
{
    Int offset = mvfpel->x + mvfpel->y * stride;

    MV baseRefMv(0, 0);

    outMvHPel = *mvfpel;
    outMvHPel <<= 1;

    outCost = xPatternRefinement(patternKey, baseRefMv, 2, outMvHPel, refPic, offset, cu, partAddr);
    m_pcRdCost->setCostScale(0);

    baseRefMv = outMvHPel;
    baseRefMv <<= 1;

    outMvQPel = *mvfpel;
    outMvQPel <<= 1;
    outMvQPel += outMvHPel;
    outMvQPel <<= 1;

    outCost = xPatternRefinement(patternKey, baseRefMv, 1, outMvQPel, refPic, offset, cu, partAddr);
}

/** encode residual and calculate rate-distortion for a CU block
 * \param cu
 * \param fencYuv
 * \param predYuv
 * \param outResiYuv
 * \param rpcYuvResiBest
 * \param outReconYuv
 * \param bSkipRes
 * \returns Void
 */
Void TEncSearch::encodeResAndCalcRdInterCU(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv, TShortYUV*& outResiYuv, TShortYUV*& outBestResiYuv, TComYuv*& outReconYuv, Bool bSkipRes)
{
    if (cu->isIntra(0))
    {
        return;
    }

    Bool bHighPass = cu->getSlice()->getDepth() ? true : false;
    UInt bits = 0, bestBits = 0;
    UInt distortion = 0, bestDistortion = 0;

    UInt width  = cu->getWidth(0);
    UInt height = cu->getHeight(0);

    //  No residual coding : SKIP mode
    if (bSkipRes)
    {
        cu->setSkipFlagSubParts(true, 0, cu->getDepth(0));

        outResiYuv->clear();

        predYuv->copyToPartYuv(outReconYuv, 0);

        distortion = primitives.sse_pp[PartitionFromSizes(width, height)]((pixel*)fencYuv->getLumaAddr(), (intptr_t)fencYuv->getStride(), (pixel*)outReconYuv->getLumaAddr(), outReconYuv->getStride());
        distortion += m_pcRdCost->scaleChromaDistCb(primitives.sse_pp[PartitionFromSizes(width >> 1, height >> 1)]((pixel*)fencYuv->getCbAddr(), (intptr_t)fencYuv->getCStride(), (pixel*)outReconYuv->getCbAddr(), outReconYuv->getCStride()));
        distortion += m_pcRdCost->scaleChromaDistCr(primitives.sse_pp[PartitionFromSizes(width >> 1, height >> 1)]((pixel*)fencYuv->getCrAddr(), (intptr_t)fencYuv->getCStride(), (pixel*)outReconYuv->getCrAddr(), outReconYuv->getCStride()));

        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[cu->getDepth(0)][CI_CURR_BEST]);
        m_pcEntropyCoder->resetBits();
        if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
        {
            m_pcEntropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
        }
        m_pcEntropyCoder->encodeSkipFlag(cu, 0, true);
        m_pcEntropyCoder->encodeMergeIndex(cu, 0, true);

        bits = m_pcEntropyCoder->getNumberOfWrittenBits();
        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[cu->getDepth(0)][CI_TEMP_BEST]);

        cu->getTotalBits()       = bits;
        cu->getTotalDistortion() = distortion;
        cu->getTotalCost()       = m_pcRdCost->calcRdCost(distortion, bits);

        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[cu->getDepth(0)][CI_TEMP_BEST]);

        cu->setCbfSubParts(0, 0, 0, 0, cu->getDepth(0));
        cu->setTrIdxSubParts(0, 0, cu->getDepth(0));
        return;
    }

    //  Residual coding.
    Int     qp, qpBest = 0;
    UInt64  cost, bcost = MAX_INT64;

    UInt trLevel = 0;
    if ((cu->getWidth(0) > cu->getSlice()->getSPS()->getMaxTrSize()))
    {
        while (cu->getWidth(0) > (cu->getSlice()->getSPS()->getMaxTrSize() << trLevel))
        {
            trLevel++;
        }
    }
    UInt maxTrLevel = 1 + trLevel;

    while ((width >> maxTrLevel) < (g_maxCUWidth >> g_maxCUDepth))
    {
        maxTrLevel--;
    }

    qp = bHighPass ? Clip3(-cu->getSlice()->getSPS()->getQpBDOffsetY(), MAX_QP, (Int)cu->getQP(0)) : cu->getQP(0);

    outResiYuv->subtract(fencYuv, predYuv, 0, width);

    cost = 0;
    bits = 0;
    distortion = 0;
    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[cu->getDepth(0)][CI_CURR_BEST]);

    UInt zeroDistortion = 0;
    xEstimateResidualQT(cu, 0, 0, 0, outResiYuv, cu->getDepth(0), cost, bits, distortion, &zeroDistortion);

    m_pcEntropyCoder->resetBits();
    m_pcEntropyCoder->encodeQtRootCbfZero(cu);
    UInt zeroResiBits = m_pcEntropyCoder->getNumberOfWrittenBits();

    UInt64 zeroCost = m_pcRdCost->calcRdCost(zeroDistortion, zeroResiBits);
    if (cu->isLosslessCoded(0))
    {
        zeroCost = cost + 1;
    }
    if (zeroCost < cost)
    {
        cost       = zeroCost;
        bits       = 0;
        distortion = zeroDistortion;

        const UInt uiQPartNum = cu->getPic()->getNumPartInCU() >> (cu->getDepth(0) << 1);
        ::memset(cu->getTransformIdx(), 0, uiQPartNum * sizeof(UChar));
        ::memset(cu->getCbf(TEXT_LUMA), 0, uiQPartNum * sizeof(UChar));
        ::memset(cu->getCbf(TEXT_CHROMA_U), 0, uiQPartNum * sizeof(UChar));
        ::memset(cu->getCbf(TEXT_CHROMA_V), 0, uiQPartNum * sizeof(UChar));
        ::memset(cu->getCoeffY(), 0, width * height * sizeof(TCoeff));
        ::memset(cu->getCoeffCb(), 0, width * height * sizeof(TCoeff) >> 2);
        ::memset(cu->getCoeffCr(), 0, width * height * sizeof(TCoeff) >> 2);
        cu->setTransformSkipSubParts(0, 0, 0, 0, cu->getDepth(0));
    }
    else
    {
        xSetResidualQTData(cu, 0, 0, 0, NULL, cu->getDepth(0), false);
    }

    m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[cu->getDepth(0)][CI_CURR_BEST]);

    bits = 0;
    {
        TShortYUV *tmpYuv = NULL;
        xAddSymbolBitsInter(cu, 0, 0, bits, tmpYuv, NULL, tmpYuv);
    }

    UInt64 exactCost = m_pcRdCost->calcRdCost(distortion, bits);
    cost = exactCost;

    if (cost < bcost)
    {
        if (!cu->getQtRootCbf(0))
        {
            outBestResiYuv->clear();
        }
        else
        {
            xSetResidualQTData(cu, 0, 0, 0, outBestResiYuv, cu->getDepth(0), true);
        }

        bestBits       = bits;
        bestDistortion = distortion;
        bcost          = cost;
        qpBest         = qp;
        m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[cu->getDepth(0)][CI_TEMP_BEST]);
    }

    assert(bcost != MAX_INT64);

    outReconYuv->addClip(predYuv, outBestResiYuv, 0, width);

    // update with clipped distortion and cost (qp estimation loop uses unclipped values)
    bestDistortion = primitives.sse_pp[PartitionFromSizes(width, height)]((pixel*)fencYuv->getLumaAddr(), (intptr_t)fencYuv->getStride(), (pixel*)outReconYuv->getLumaAddr(), outReconYuv->getStride());
    bestDistortion += m_pcRdCost->scaleChromaDistCb(primitives.sse_pp[PartitionFromSizes(width >> 1, height >> 1)]((pixel*)fencYuv->getCbAddr(), (intptr_t)fencYuv->getCStride(), (pixel*)outReconYuv->getCbAddr(), outReconYuv->getCStride()));
    bestDistortion += m_pcRdCost->scaleChromaDistCr(primitives.sse_pp[PartitionFromSizes(width >> 1, height >> 1)]((pixel*)fencYuv->getCrAddr(), (intptr_t)fencYuv->getCStride(), (pixel*)outReconYuv->getCrAddr(), outReconYuv->getCStride()));
    bcost = m_pcRdCost->calcRdCost(bestDistortion, bestBits);

    cu->getTotalBits()       = bestBits;
    cu->getTotalDistortion() = bestDistortion;
    cu->getTotalCost()       = bcost;

    if (cu->isSkipped(0))
    {
        cu->setCbfSubParts(0, 0, 0, 0, cu->getDepth(0));
    }

    cu->setQPSubParts(qpBest, 0, cu->getDepth(0));
}

#if _MSC_VER
#pragma warning(disable: 4701) // potentially uninitialized local variable
#endif

Void TEncSearch::xEstimateResidualQT(TComDataCU* cu,
                                     UInt        quadrant,
                                     UInt        absPartIdx,
                                     UInt        absTUPartIdx,
                                     TShortYUV*  resiYuv,
                                     const UInt  depth,
                                     UInt64 &    rdCost,
                                     UInt &      outBits,
                                     UInt &      outDist,
                                     UInt *      outZeroDist)
{
    assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
    const UInt trMode = depth - cu->getDepth(0);
    const UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;

    Bool bSplitFlag = ((cu->getSlice()->getSPS()->getQuadtreeTUMaxDepthInter() == 1) && cu->getPredictionMode(absPartIdx) == MODE_INTER && (cu->getPartitionSize(absPartIdx) != SIZE_2Nx2N));
    Bool bCheckFull;
    if (bSplitFlag && depth == cu->getDepth(absPartIdx) && (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx)))
        bCheckFull = false;
    else
        bCheckFull = (trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize());
    const Bool bCheckSplit = (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));
    assert(bCheckFull || bCheckSplit);

    Bool  bCodeChroma = true;
    UInt  trModeC     = trMode;
    UInt  trSizeCLog2 = trSizeLog2 - 1;
    if (trSizeLog2 == 2)
    {
        trSizeCLog2++;
        trModeC--;
        UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
        bCodeChroma = ((absPartIdx % qpdiv) == 0);
    }

    const UInt setCbf = 1 << trMode;
    // code full block
    UInt64 singleCost = MAX_INT64;
    UInt singleBits = 0;
    UInt singleDist = 0;
    UInt absSumY = 0, absSumU = 0, absSumV = 0;
    UInt bestTransformMode[3] = { 0 };

    m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_ROOT]);

    if (bCheckFull)
    {
        const UInt numCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
        const UInt qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
        TCoeff *coeffCurY = m_ppcQTTempCoeffY[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx);
        TCoeff *coeffCurU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
        TCoeff *coeffCurV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
        Int *arlCoeffCurY = m_ppcQTTempArlCoeffY[qtlayer] +  (numCoeffPerAbsPartIdxIncrement * absPartIdx);
        Int *arlCoeffCurU = m_ppcQTTempArlCoeffCb[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
        Int *arlCoeffCurV = m_ppcQTTempArlCoeffCr[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);

        Int trWidth = 0, trHeight = 0, trWidthC = 0, trHeightC = 0;
        UInt absTUPartIdxC = absPartIdx;

        trWidth  = trHeight  = 1 << trSizeLog2;
        trWidthC = trHeightC = 1 << trSizeCLog2;
        cu->setTrIdxSubParts(depth - cu->getDepth(0), absPartIdx, depth);
        UInt64 minCostY = MAX_INT64;
        UInt64 minCostU = MAX_INT64;
        UInt64 minCostV = MAX_INT64;
        Bool checkTransformSkipY  = cu->getSlice()->getPPS()->getUseTransformSkip() && trWidth == 4 && trHeight == 4;
        Bool checkTransformSkipUV = cu->getSlice()->getPPS()->getUseTransformSkip() && trWidthC == 4 && trHeightC == 4;

        checkTransformSkipY         &= (!cu->isLosslessCoded(0));
        checkTransformSkipUV        &= (!cu->isLosslessCoded(0));

        cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, depth);
        if (bCodeChroma)
        {
            cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
        }

        if (m_pcEncCfg->getUseRDOQ())
        {
            m_pcEntropyCoder->estimateBit(m_pcTrQuant->m_estBitsSbac, trWidth, trHeight, TEXT_LUMA);
        }

        m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);
        m_pcTrQuant->selectLambda(TEXT_LUMA);

        absSumY = m_pcTrQuant->transformNxN(cu, resiYuv->getLumaAddr(absTUPartIdx), resiYuv->width, coeffCurY,
                                            arlCoeffCurY, trWidth, trHeight, TEXT_LUMA, absPartIdx);

        cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

        if (bCodeChroma)
        {
            if (m_pcEncCfg->getUseRDOQ())
            {
                m_pcEntropyCoder->estimateBit(m_pcTrQuant->m_estBitsSbac, trWidthC, trHeightC, TEXT_CHROMA);
            }

            Int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
            m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

            m_pcTrQuant->selectLambda(TEXT_CHROMA);

            absSumU = m_pcTrQuant->transformNxN(cu, resiYuv->getCbAddr(absTUPartIdxC), resiYuv->Cwidth, coeffCurU,
                                                arlCoeffCurU, trWidthC, trHeightC, TEXT_CHROMA_U, absPartIdx);

            curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
            m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
            absSumV = m_pcTrQuant->transformNxN(cu, resiYuv->getCrAddr(absTUPartIdxC), resiYuv->Cwidth, coeffCurV,
                                                arlCoeffCurV, trWidthC, trHeightC, TEXT_CHROMA_V, absPartIdx);

            cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
        }

        m_pcEntropyCoder->resetBits();
        m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
        m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx,  trWidth,  trHeight, depth, TEXT_LUMA);
        const UInt uiSingleBitsY = m_pcEntropyCoder->getNumberOfWrittenBits();

        UInt singleBitsU = 0;
        UInt singleBitsV = 0;
        if (bCodeChroma)
        {
            m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trMode);
            m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_U);
            singleBitsU = m_pcEntropyCoder->getNumberOfWrittenBits() - uiSingleBitsY;

            m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trMode);
            m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_V);
            singleBitsV = m_pcEntropyCoder->getNumberOfWrittenBits() - (uiSingleBitsY + singleBitsU);
        }

        const UInt numSamplesLuma = 1 << (trSizeLog2 << 1);
        const UInt numSamplesChroma = 1 << (trSizeCLog2 << 1);

        ::memset(m_pTempPel, 0, sizeof(Pel) * numSamplesLuma); // not necessary needed for inside of recursion (only at the beginning)

        int partSize = PartitionFromSizes(trWidth, trHeight);
        UInt distY = primitives.sse_sp[partSize](resiYuv->getLumaAddr(absTUPartIdx), (intptr_t)resiYuv->width, (pixel*)m_pTempPel, trWidth);

        if (outZeroDist)
        {
            *outZeroDist += distY;
        }
        if (absSumY)
        {
            Short *curResiY = m_pcQTTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx);

            m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

            Int scalingListType = 3 + g_eTTable[(Int)TEXT_LUMA];
            assert(scalingListType < 6);
            m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_LUMA, REG_DCT, curResiY, m_pcQTTempTComYuv[qtlayer].width,  coeffCurY, trWidth, trHeight, scalingListType); //this is for inter mode only

            const UInt nonZeroDistY = primitives.sse_ss[partSize](resiYuv->getLumaAddr(absTUPartIdx), resiYuv->width, m_pcQTTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx),
                                                                  m_pcQTTempTComYuv[qtlayer].width);
            if (cu->isLosslessCoded(0))
            {
                distY = nonZeroDistY;
            }
            else
            {
                const UInt64 singleCostY = m_pcRdCost->calcRdCost(nonZeroDistY, uiSingleBitsY);
                m_pcEntropyCoder->resetBits();
                m_pcEntropyCoder->encodeQtCbfZero(cu, TEXT_LUMA,     trMode);
                const UInt nullBitsY = m_pcEntropyCoder->getNumberOfWrittenBits();
                const UInt64 nullCostY = m_pcRdCost->calcRdCost(distY, nullBitsY);
                if (nullCostY < singleCostY)
                {
                    absSumY = 0;
                    ::memset(coeffCurY, 0, sizeof(TCoeff) * numSamplesLuma);
                    if (checkTransformSkipY)
                    {
                        minCostY = nullCostY;
                    }
                }
                else
                {
                    distY = nonZeroDistY;
                    if (checkTransformSkipY)
                    {
                        minCostY = singleCostY;
                    }
                }
            }
        }
        else if (checkTransformSkipY)
        {
            m_pcEntropyCoder->resetBits();
            m_pcEntropyCoder->encodeQtCbfZero(cu, TEXT_LUMA, trMode);
            const UInt nullBitsY = m_pcEntropyCoder->getNumberOfWrittenBits();
            minCostY = m_pcRdCost->calcRdCost(distY, nullBitsY);
        }

        if (!absSumY)
        {
            Short *ptr =  m_pcQTTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx);
            const UInt stride = m_pcQTTempTComYuv[qtlayer].width;
            for (UInt y = 0; y < trHeight; ++y)
            {
                ::memset(ptr, 0, sizeof(Short) * trWidth);
                ptr += stride;
            }
        }

        UInt distU = 0;
        UInt distV = 0;

        int partSizeC = x265::PartitionFromSizes(trWidthC, trHeightC);
        if (bCodeChroma)
        {
            distU = m_pcRdCost->scaleChromaDistCb(primitives.sse_sp[partSizeC](resiYuv->getCbAddr(absTUPartIdxC), (intptr_t)resiYuv->Cwidth, (pixel*)m_pTempPel, trWidthC));

            if (outZeroDist)
            {
                *outZeroDist += distU;
            }
            if (absSumU)
            {
                Short *pcResiCurrU = m_pcQTTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);

                Int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
                m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

                Int scalingListType = 3 + g_eTTable[(Int)TEXT_CHROMA_U];
                assert(scalingListType < 6);
                m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_CHROMA, REG_DCT, pcResiCurrU, m_pcQTTempTComYuv[qtlayer].Cwidth, coeffCurU, trWidthC, trHeightC, scalingListType);

                UInt dist = primitives.sse_ss[partSizeC](resiYuv->getCbAddr(absTUPartIdxC), resiYuv->Cwidth, 
                                                         m_pcQTTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC),
                                                         m_pcQTTempTComYuv[qtlayer].Cwidth);
                const UInt nonZeroDistU = m_pcRdCost->scaleChromaDistCb(dist);

                if (cu->isLosslessCoded(0))
                {
                    distU = nonZeroDistU;
                }
                else
                {
                    const UInt64 singleCostU = m_pcRdCost->calcRdCost(nonZeroDistU, singleBitsU);
                    m_pcEntropyCoder->resetBits();
                    m_pcEntropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_U, trMode);
                    const UInt nullBitsU = m_pcEntropyCoder->getNumberOfWrittenBits();
                    const UInt64 nullCostU = m_pcRdCost->calcRdCost(distU, nullBitsU);
                    if (nullCostU < singleCostU)
                    {
                        absSumU = 0;
                        ::memset(coeffCurU, 0, sizeof(TCoeff) * numSamplesChroma);
                        if (checkTransformSkipUV)
                        {
                            minCostU = nullCostU;
                        }
                    }
                    else
                    {
                        distU = nonZeroDistU;
                        if (checkTransformSkipUV)
                        {
                            minCostU = singleCostU;
                        }
                    }
                }
            }
            else if (checkTransformSkipUV)
            {
                m_pcEntropyCoder->resetBits();
                m_pcEntropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_U, trModeC);
                const UInt nullBitsU = m_pcEntropyCoder->getNumberOfWrittenBits();
                minCostU = m_pcRdCost->calcRdCost(distU, nullBitsU);
            }
            if (!absSumU)
            {
                Short *ptr = m_pcQTTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);
                const UInt stride = m_pcQTTempTComYuv[qtlayer].Cwidth;
                for (UInt y = 0; y < trHeightC; ++y)
                {
                    ::memset(ptr, 0, sizeof(Short) * trWidthC);
                    ptr += stride;
                }
            }

            distV = m_pcRdCost->scaleChromaDistCr(primitives.sse_sp[partSizeC](resiYuv->getCrAddr(absTUPartIdxC), (intptr_t)resiYuv->Cwidth, (pixel*)m_pTempPel, trWidthC));
            if (outZeroDist)
            {
                *outZeroDist += distV;
            }
            if (absSumV)
            {
                Short *curResiV = m_pcQTTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC);
                Int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
                m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

                Int scalingListType = 3 + g_eTTable[(Int)TEXT_CHROMA_V];
                assert(scalingListType < 6);
                m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_CHROMA, REG_DCT, curResiV, m_pcQTTempTComYuv[qtlayer].Cwidth, coeffCurV, trWidthC, trHeightC, scalingListType);

                UInt dist = primitives.sse_ss[partSizeC](resiYuv->getCrAddr(absTUPartIdxC), resiYuv->Cwidth, 
                                                         m_pcQTTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC),
                                                         m_pcQTTempTComYuv[qtlayer].Cwidth);
                const UInt nonZeroDistV = m_pcRdCost->scaleChromaDistCr(dist);

                if (cu->isLosslessCoded(0))
                {
                    distV = nonZeroDistV;
                }
                else
                {
                    const UInt64 singleCostV = m_pcRdCost->calcRdCost(nonZeroDistV, singleBitsV);
                    m_pcEntropyCoder->resetBits();
                    m_pcEntropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_V, trMode);
                    const UInt nullBitsV = m_pcEntropyCoder->getNumberOfWrittenBits();
                    const UInt64 nullCostV = m_pcRdCost->calcRdCost(distV, nullBitsV);
                    if (nullCostV < singleCostV)
                    {
                        absSumV = 0;
                        ::memset(coeffCurV, 0, sizeof(TCoeff) * numSamplesChroma);
                        if (checkTransformSkipUV)
                        {
                            minCostV = nullCostV;
                        }
                    }
                    else
                    {
                        distV = nonZeroDistV;
                        if (checkTransformSkipUV)
                        {
                            minCostV = singleCostV;
                        }
                    }
                }
            }
            else if (checkTransformSkipUV)
            {
                m_pcEntropyCoder->resetBits();
                m_pcEntropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_V, trModeC);
                const UInt nullBitsV = m_pcEntropyCoder->getNumberOfWrittenBits();
                minCostV = m_pcRdCost->calcRdCost(distV, nullBitsV);
            }
            if (!absSumV)
            {
                Short *ptr =  m_pcQTTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC);
                const UInt stride = m_pcQTTempTComYuv[qtlayer].Cwidth;
                for (UInt y = 0; y < trHeightC; ++y)
                {
                    ::memset(ptr, 0, sizeof(Short) * trWidthC);
                    ptr += stride;
                }
            }
        }
        cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);
        if (bCodeChroma)
        {
            cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
        }

        if (checkTransformSkipY)
        {
            UInt nonZeroDistY = 0, absSumTransformSkipY;
            UInt64 singleCostY = MAX_INT64;

            Short *curResiY = m_pcQTTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx);
            UInt resiStride = m_pcQTTempTComYuv[qtlayer].width;

            TCoeff bestCoeffY[32 * 32];
            memcpy(bestCoeffY, coeffCurY, sizeof(TCoeff) * numSamplesLuma);

            TCoeff bestArlCoeffY[32 * 32];
            memcpy(bestArlCoeffY, arlCoeffCurY, sizeof(TCoeff) * numSamplesLuma);

            Short bestResiY[32 * 32];
            for (Int i = 0; i < trHeight; ++i)
            {
                memcpy(bestResiY + i * trWidth, curResiY + i * resiStride, sizeof(Short) * trWidth);
            }

            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_ROOT]);

            cu->setTransformSkipSubParts(1, TEXT_LUMA, absPartIdx, depth);

            if (m_pcEncCfg->getUseRDOQTS())
            {
                m_pcEntropyCoder->estimateBit(m_pcTrQuant->m_estBitsSbac, trWidth, trHeight, TEXT_LUMA);
            }

            m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

            m_pcTrQuant->selectLambda(TEXT_LUMA);
            absSumTransformSkipY = m_pcTrQuant->transformNxN(cu, resiYuv->getLumaAddr(absTUPartIdx), resiYuv->width, coeffCurY,
                                                             arlCoeffCurY, trWidth, trHeight, TEXT_LUMA, absPartIdx, true);
            cu->setCbfSubParts(absSumTransformSkipY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

            if (absSumTransformSkipY != 0)
            {
                m_pcEntropyCoder->resetBits();
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
                m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);
                const UInt skipSingleBitsY = m_pcEntropyCoder->getNumberOfWrittenBits();

                m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

                Int scalingListType = 3 + g_eTTable[(Int)TEXT_LUMA];
                assert(scalingListType < 6);

                m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_LUMA, REG_DCT, curResiY, m_pcQTTempTComYuv[qtlayer].width,  coeffCurY, trWidth, trHeight, scalingListType, true);

                nonZeroDistY = primitives.sse_ss[partSize](resiYuv->getLumaAddr(absTUPartIdx), resiYuv->width,
                                                           m_pcQTTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx),
                                                           m_pcQTTempTComYuv[qtlayer].width);

                singleCostY = m_pcRdCost->calcRdCost(nonZeroDistY, skipSingleBitsY);
            }

            if (!absSumTransformSkipY || minCostY < singleCostY)
            {
                cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, depth);
                memcpy(coeffCurY, bestCoeffY, sizeof(TCoeff) * numSamplesLuma);
                memcpy(arlCoeffCurY, bestArlCoeffY, sizeof(TCoeff) * numSamplesLuma);
                for (Int i = 0; i < trHeight; ++i)
                {
                    memcpy(curResiY + i * resiStride, &bestResiY[i * trWidth], sizeof(Short) * trWidth);
                }
            }
            else
            {
                distY = nonZeroDistY;
                absSumY = absSumTransformSkipY;
                bestTransformMode[0] = 1;
            }

            cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);
        }

        if (bCodeChroma && checkTransformSkipUV)
        {
            UInt nonZeroDistU = 0, nonZeroDistV = 0, absSumTransformSkipU, absSumTransformSkipV;
            UInt64 singleCostU = MAX_INT64;
            UInt64 singleCostV = MAX_INT64;

            Short *curResiU = m_pcQTTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);
            Short *curResiV = m_pcQTTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC);
            UInt stride = m_pcQTTempTComYuv[qtlayer].Cwidth;

            TCoeff bestCoeffU[32 * 32], bestCoeffV[32 * 32];
            memcpy(bestCoeffU, coeffCurU, sizeof(TCoeff) * numSamplesChroma);
            memcpy(bestCoeffV, coeffCurV, sizeof(TCoeff) * numSamplesChroma);

            TCoeff bestArlCoeffU[32 * 32], bestArlCoeffV[32 * 32];
            memcpy(bestArlCoeffU, arlCoeffCurU, sizeof(TCoeff) * numSamplesChroma);
            memcpy(bestArlCoeffV, arlCoeffCurV, sizeof(TCoeff) * numSamplesChroma);

            Short bestResiU[32 * 32], bestResiV[32 * 32];
            for (Int i = 0; i < trHeightC; ++i)
            {
                memcpy(&bestResiU[i * trWidthC], curResiU + i * stride, sizeof(Short) * trWidthC);
                memcpy(&bestResiV[i * trWidthC], curResiV + i * stride, sizeof(Short) * trWidthC);
            }

            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_ROOT]);

            cu->setTransformSkipSubParts(1, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setTransformSkipSubParts(1, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);

            if (m_pcEncCfg->getUseRDOQTS())
            {
                m_pcEntropyCoder->estimateBit(m_pcTrQuant->m_estBitsSbac, trWidthC, trHeightC, TEXT_CHROMA);
            }

            Int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
            m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
            m_pcTrQuant->selectLambda(TEXT_CHROMA);

            absSumTransformSkipU = m_pcTrQuant->transformNxN(cu, resiYuv->getCbAddr(absTUPartIdxC), resiYuv->Cwidth, coeffCurU,
                                                             arlCoeffCurU, trWidthC, trHeightC, TEXT_CHROMA_U, absPartIdx, true);
            curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
            m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
            absSumTransformSkipV = m_pcTrQuant->transformNxN(cu, resiYuv->getCrAddr(absTUPartIdxC), resiYuv->Cwidth, coeffCurV,
                                                             arlCoeffCurV, trWidthC, trHeightC, TEXT_CHROMA_V, absPartIdx, true);

            cu->setCbfSubParts(absSumTransformSkipU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setCbfSubParts(absSumTransformSkipV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);

            m_pcEntropyCoder->resetBits();
            singleBitsU = 0;
            singleBitsV = 0;

            if (absSumTransformSkipU)
            {
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trMode);
                m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_U);
                singleBitsU = m_pcEntropyCoder->getNumberOfWrittenBits();

                curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
                m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

                Int scalingListType = 3 + g_eTTable[(Int)TEXT_CHROMA_U];
                assert(scalingListType < 6);

                m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_CHROMA, REG_DCT, curResiU, m_pcQTTempTComYuv[qtlayer].Cwidth, coeffCurU, trWidthC, trHeightC, scalingListType, true);

                UInt dist = primitives.sse_ss[partSizeC](resiYuv->getCbAddr(absTUPartIdxC), resiYuv->Cwidth,
                                                         m_pcQTTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC),
                                                         m_pcQTTempTComYuv[qtlayer].Cwidth);
                nonZeroDistU = m_pcRdCost->scaleChromaDistCb(dist);
                singleCostU = m_pcRdCost->calcRdCost(nonZeroDistU, singleBitsU);
            }

            if (!absSumTransformSkipU || minCostU < singleCostU)
            {
                cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);

                memcpy(coeffCurU, bestCoeffU, sizeof(TCoeff) * numSamplesChroma);
                memcpy(arlCoeffCurU, bestArlCoeffU, sizeof(TCoeff) * numSamplesChroma);
                for (Int i = 0; i < trHeightC; ++i)
                {
                    memcpy(curResiU + i * stride, &bestResiU[i * trWidthC], sizeof(Short) * trWidthC);
                }
            }
            else
            {
                distU = nonZeroDistU;
                absSumU = absSumTransformSkipU;
                bestTransformMode[1] = 1;
            }

            if (absSumTransformSkipV)
            {
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trMode);
                m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_V);
                singleBitsV = m_pcEntropyCoder->getNumberOfWrittenBits() - singleBitsU;

                curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
                m_pcTrQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

                Int scalingListType = 3 + g_eTTable[(Int)TEXT_CHROMA_V];
                assert(scalingListType < 6);

                m_pcTrQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), TEXT_CHROMA, REG_DCT, curResiV, m_pcQTTempTComYuv[qtlayer].Cwidth, coeffCurV, trWidthC, trHeightC, scalingListType, true);

                UInt dist = primitives.sse_ss[partSizeC](resiYuv->getCrAddr(absTUPartIdxC), resiYuv->Cwidth,
                                                         m_pcQTTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC),
                                                         m_pcQTTempTComYuv[qtlayer].Cwidth);
                nonZeroDistV = m_pcRdCost->scaleChromaDistCr(dist);
                singleCostV = m_pcRdCost->calcRdCost(nonZeroDistV, singleBitsV);
            }

            if (!absSumTransformSkipV || minCostV < singleCostV)
            {
                cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);

                memcpy(coeffCurV, bestCoeffV, sizeof(TCoeff) * numSamplesChroma);
                memcpy(arlCoeffCurV, bestArlCoeffV, sizeof(TCoeff) * numSamplesChroma);
                for (Int i = 0; i < trHeightC; ++i)
                {
                    memcpy(curResiV + i * stride, &bestResiV[i * trWidthC], sizeof(Short) * trWidthC);
                }
            }
            else
            {
                distV = nonZeroDistV;
                absSumV = absSumTransformSkipV;
                bestTransformMode[2] = 1;
            }

            cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
        }

        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_ROOT]);

        m_pcEntropyCoder->resetBits();

        if (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx))
        {
            m_pcEntropyCoder->encodeTransformSubdivFlag(0, 5 - trSizeLog2);
        }

        if (bCodeChroma)
        {
            m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trMode);
            m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trMode);
        }

        m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA,     trMode);
        m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);

        if (bCodeChroma)
        {
            m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_U);
            m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_V);
        }

        singleBits = m_pcEntropyCoder->getNumberOfWrittenBits();
        singleDist = distY + distU + distV;
        singleCost = m_pcRdCost->calcRdCost(singleDist, singleBits);
    }

    // code sub-blocks
    if (bCheckSplit)
    {
        if (bCheckFull)
        {
            m_pcRDGoOnSbacCoder->store(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_TEST]);
            m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_ROOT]);
        }
        UInt subdivDist = 0;
        UInt subdivBits = 0;
        UInt64 subDivCost = 0;

        const UInt qPartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
        for (UInt i = 0; i < 4; ++i)
        {
            UInt nsAddr = absPartIdx + i * qPartNumSubdiv;
            xEstimateResidualQT(cu, i, absPartIdx + i * qPartNumSubdiv, nsAddr, resiYuv, depth + 1, subDivCost, subdivBits, subdivDist, bCheckFull ? NULL : outZeroDist);
        }

        UInt ycbf = 0;
        UInt ucbf = 0;
        UInt vcbf = 0;
        for (UInt i = 0; i < 4; ++i)
        {
            ycbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_LUMA,     trMode + 1);
            ucbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_CHROMA_U, trMode + 1);
            vcbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_CHROMA_V, trMode + 1);
        }

        for (UInt i = 0; i < 4 * qPartNumSubdiv; ++i)
        {
            cu->getCbf(TEXT_LUMA)[absPartIdx + i] |= ycbf << trMode;
            cu->getCbf(TEXT_CHROMA_U)[absPartIdx + i] |= ucbf << trMode;
            cu->getCbf(TEXT_CHROMA_V)[absPartIdx + i] |= vcbf << trMode;
        }

        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_ROOT]);
        m_pcEntropyCoder->resetBits();

        xEncodeResidualQT(cu, absPartIdx, depth, true,  TEXT_LUMA);
        xEncodeResidualQT(cu, absPartIdx, depth, false, TEXT_LUMA);
        xEncodeResidualQT(cu, absPartIdx, depth, false, TEXT_CHROMA_U);
        xEncodeResidualQT(cu, absPartIdx, depth, false, TEXT_CHROMA_V);

        subdivBits = m_pcEntropyCoder->getNumberOfWrittenBits();
        subDivCost  = m_pcRdCost->calcRdCost(subdivDist, subdivBits);

        if (ycbf || ucbf || vcbf || !bCheckFull)
        {
            if (subDivCost < singleCost)
            {
                rdCost += subDivCost;
                outBits += subdivBits;
                outDist += subdivDist;
                return;
            }
        }

        cu->setTransformSkipSubParts(bestTransformMode[0], TEXT_LUMA, absPartIdx, depth);
        if (bCodeChroma)
        {
            cu->setTransformSkipSubParts(bestTransformMode[1], TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
            cu->setTransformSkipSubParts(bestTransformMode[2], TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
        }
        assert(bCheckFull);
        m_pcRDGoOnSbacCoder->load(m_pppcRDSbacCoder[depth][CI_QT_TRAFO_TEST]);
    }

    rdCost += singleCost;
    outBits += singleBits;
    outDist += singleDist;

    cu->setTrIdxSubParts(trMode, absPartIdx, depth);
    cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

    if (bCodeChroma)
    {
        cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
        cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
    }
}

Void TEncSearch::xEncodeResidualQT(TComDataCU* cu, UInt absPartIdx, const UInt depth, Bool bSubdivAndCbf, TextType ttype)
{
    assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
    const UInt curTrMode = depth - cu->getDepth(0);
    const UInt trMode = cu->getTransformIdx(absPartIdx);
    const Bool bSubdiv = curTrMode != trMode;
    const UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;

    if (bSubdivAndCbf && trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() && trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx))
    {
        m_pcEntropyCoder->encodeTransformSubdivFlag(bSubdiv, 5 - trSizeLog2);
    }

    assert(cu->getPredictionMode(absPartIdx) != MODE_INTRA);
    if (bSubdivAndCbf)
    {
        const Bool bFirstCbfOfCU = curTrMode == 0;
        if (bFirstCbfOfCU || trSizeLog2 > 2)
        {
            if (bFirstCbfOfCU || cu->getCbf(absPartIdx, TEXT_CHROMA_U, curTrMode - 1))
            {
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, curTrMode);
            }
            if (bFirstCbfOfCU || cu->getCbf(absPartIdx, TEXT_CHROMA_V, curTrMode - 1))
            {
                m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, curTrMode);
            }
        }
        else if (trSizeLog2 == 2)
        {
            assert(cu->getCbf(absPartIdx, TEXT_CHROMA_U, curTrMode) == cu->getCbf(absPartIdx, TEXT_CHROMA_U, curTrMode - 1));
            assert(cu->getCbf(absPartIdx, TEXT_CHROMA_V, curTrMode) == cu->getCbf(absPartIdx, TEXT_CHROMA_V, curTrMode - 1));
        }
    }

    if (!bSubdiv)
    {
        const UInt numCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
        //assert( 16 == uiNumCoeffPerAbsPartIdxIncrement ); // check
        const UInt qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
        TCoeff *coeffCurY = m_ppcQTTempCoeffY[qtlayer] +  numCoeffPerAbsPartIdxIncrement * absPartIdx;
        TCoeff *coeffCurU = m_ppcQTTempCoeffCb[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
        TCoeff *coeffCurV = m_ppcQTTempCoeffCr[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);

        Bool  bCodeChroma = true;
        UInt  trModeC     = trMode;
        UInt  trSizeCLog2 = trSizeLog2 - 1;
        if (trSizeLog2 == 2)
        {
            trSizeCLog2++;
            trModeC--;
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
            bCodeChroma = ((absPartIdx % qpdiv) == 0);
        }

        if (bSubdivAndCbf)
        {
            m_pcEntropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
        }
        else
        {
            if (ttype == TEXT_LUMA && cu->getCbf(absPartIdx, TEXT_LUMA, trMode))
            {
                Int trWidth  = 1 << trSizeLog2;
                Int trHeight = 1 << trSizeLog2;
                m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);
            }
            if (bCodeChroma)
            {
                Int trWidth  = 1 << trSizeCLog2;
                Int trHeight = 1 << trSizeCLog2;
                if (ttype == TEXT_CHROMA_U && cu->getCbf(absPartIdx, TEXT_CHROMA_U, trMode))
                {
                    m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidth, trHeight, depth, TEXT_CHROMA_U);
                }
                if (ttype == TEXT_CHROMA_V && cu->getCbf(absPartIdx, TEXT_CHROMA_V, trMode))
                {
                    m_pcEntropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidth, trHeight, depth, TEXT_CHROMA_V);
                }
            }
        }
    }
    else
    {
        if (bSubdivAndCbf || cu->getCbf(absPartIdx, ttype, curTrMode))
        {
            const UInt qpartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
            for (UInt i = 0; i < 4; ++i)
            {
                xEncodeResidualQT(cu, absPartIdx + i * qpartNumSubdiv, depth + 1, bSubdivAndCbf, ttype);
            }
        }
    }
}

Void TEncSearch::xSetResidualQTData(TComDataCU* cu, UInt quadrant, UInt absPartIdx, UInt absTUPartIdx, TShortYUV* resiYuv, UInt depth, Bool bSpatial)
{
    assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
    const UInt curTrMode = depth - cu->getDepth(0);
    const UInt trMode = cu->getTransformIdx(absPartIdx);

    if (curTrMode == trMode)
    {
        const UInt trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;
        const UInt qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

        Bool  bCodeChroma   = true;
        UInt  trModeC     = trMode;
        UInt  trSizeCLog2 = trSizeLog2 - 1;
        if (trSizeLog2 == 2)
        {
            trSizeCLog2++;
            trModeC--;
            UInt qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
            bCodeChroma  = ((absPartIdx % qpdiv) == 0);
        }

        if (bSpatial)
        {
            Int trWidth  = 1 << trSizeLog2;
            Int trHeight = 1 << trSizeLog2;
            m_pcQTTempTComYuv[qtlayer].copyPartToPartLuma(resiYuv, absTUPartIdx, trWidth, trHeight);

            if (bCodeChroma)
            {
                m_pcQTTempTComYuv[qtlayer].copyPartToPartChroma(resiYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
            }
        }
        else
        {
            UInt uiNumCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
            UInt uiNumCoeffY = (1 << (trSizeLog2 << 1));
            TCoeff* pcCoeffSrcY = m_ppcQTTempCoeffY[qtlayer] +  uiNumCoeffPerAbsPartIdxIncrement * absPartIdx;
            TCoeff* pcCoeffDstY = cu->getCoeffY() + uiNumCoeffPerAbsPartIdxIncrement * absPartIdx;
            ::memcpy(pcCoeffDstY, pcCoeffSrcY, sizeof(TCoeff) * uiNumCoeffY);
            Int* pcArlCoeffSrcY = m_ppcQTTempArlCoeffY[qtlayer] +  uiNumCoeffPerAbsPartIdxIncrement * absPartIdx;
            Int* pcArlCoeffDstY = cu->getArlCoeffY() + uiNumCoeffPerAbsPartIdxIncrement * absPartIdx;
            ::memcpy(pcArlCoeffDstY, pcArlCoeffSrcY, sizeof(Int) * uiNumCoeffY);
            if (bCodeChroma)
            {
                UInt    uiNumCoeffC = (1 << (trSizeCLog2 << 1));
                TCoeff* pcCoeffSrcU = m_ppcQTTempCoeffCb[qtlayer] + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                TCoeff* pcCoeffSrcV = m_ppcQTTempCoeffCr[qtlayer] + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                TCoeff* pcCoeffDstU = cu->getCoeffCb() + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                TCoeff* pcCoeffDstV = cu->getCoeffCr() + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                ::memcpy(pcCoeffDstU, pcCoeffSrcU, sizeof(TCoeff) * uiNumCoeffC);
                ::memcpy(pcCoeffDstV, pcCoeffSrcV, sizeof(TCoeff) * uiNumCoeffC);
                Int* pcArlCoeffSrcU = m_ppcQTTempArlCoeffCb[qtlayer] + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                Int* pcArlCoeffSrcV = m_ppcQTTempArlCoeffCr[qtlayer] + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                Int* pcArlCoeffDstU = cu->getArlCoeffCb() + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                Int* pcArlCoeffDstV = cu->getArlCoeffCr() + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
                ::memcpy(pcArlCoeffDstU, pcArlCoeffSrcU, sizeof(Int) * uiNumCoeffC);
                ::memcpy(pcArlCoeffDstV, pcArlCoeffSrcV, sizeof(Int) * uiNumCoeffC);
            }
        }
    }
    else
    {
        const UInt qPartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
        for (UInt i = 0; i < 4; ++i)
        {
            UInt nsAddr = absPartIdx + i * qPartNumSubdiv;
            xSetResidualQTData(cu, i, absPartIdx + i * qPartNumSubdiv, nsAddr, resiYuv, depth + 1, bSpatial);
        }
    }
}

UInt TEncSearch::xModeBitsIntra(TComDataCU* cu, UInt mode, UInt pu, UInt partOffset, UInt depth, UInt initTrDepth)
{
    // Reload only contexts required for coding intra mode information
    m_pcRDGoOnSbacCoder->loadIntraDirModeLuma(m_pppcRDSbacCoder[depth][CI_CURR_BEST]);

    cu->setLumaIntraDirSubParts(mode, partOffset, depth + initTrDepth);

    m_pcEntropyCoder->resetBits();
    m_pcEntropyCoder->encodeIntraDirModeLuma(cu, partOffset);

    return m_pcEntropyCoder->getNumberOfWrittenBits();
}

UInt TEncSearch::xUpdateCandList(UInt mode, UInt64 cost, UInt fastCandNum, UInt* CandModeList, UInt64* CandCostList)
{
    UInt i;
    UInt shift = 0;

    while (shift < fastCandNum && cost < CandCostList[fastCandNum - 1 - shift])
    {
        shift++;
    }

    if (shift != 0)
    {
        for (i = 1; i < shift; i++)
        {
            CandModeList[fastCandNum - i] = CandModeList[fastCandNum - 1 - i];
            CandCostList[fastCandNum - i] = CandCostList[fastCandNum - 1 - i];
        }

        CandModeList[fastCandNum - shift] = mode;
        CandCostList[fastCandNum - shift] = cost;
        return 1;
    }

    return 0;
}

/** add inter-prediction syntax elements for a CU block
 * \param cu
 * \param uiQp
 * \param trMode
 * \param outBits
 * \param outReconYuv
 * \param predYuv
 * \param outResiYuv
 * \returns Void
 */
Void  TEncSearch::xAddSymbolBitsInter(TComDataCU* cu, UInt qp, UInt trMode, UInt& outBits, TShortYUV*& outReconYuv, TComYuv*predYuv, TShortYUV*& outResiYuv)
{
    if (cu->getMergeFlag(0) && cu->getPartitionSize(0) == SIZE_2Nx2N && !cu->getQtRootCbf(0))
    {
        cu->setSkipFlagSubParts(true, 0, cu->getDepth(0));

        m_pcEntropyCoder->resetBits();
        if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
        {
            m_pcEntropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
        }
        m_pcEntropyCoder->encodeSkipFlag(cu, 0, true);
        m_pcEntropyCoder->encodeMergeIndex(cu, 0, true);
        outBits += m_pcEntropyCoder->getNumberOfWrittenBits();
    }
    else
    {
        m_pcEntropyCoder->resetBits();
        if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
        {
            m_pcEntropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
        }
        m_pcEntropyCoder->encodeSkipFlag(cu, 0, true);
        m_pcEntropyCoder->encodePredMode(cu, 0, true);
        m_pcEntropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);
        m_pcEntropyCoder->encodePredInfo(cu, 0, true);
        Bool bDummy = false;
        m_pcEntropyCoder->encodeCoeff(cu, 0, cu->getDepth(0), cu->getWidth(0), cu->getHeight(0), bDummy);

        outBits += m_pcEntropyCoder->getNumberOfWrittenBits();
    }
}

/**** Function to estimate the header bits ************/
UInt  TEncSearch::estimateHeaderBits(TComDataCU* cu, UInt absPartIdx)
{
    UInt bits = 0;

    m_pcEntropyCoder->resetBits();

    UInt lpelx = cu->getCUPelX() + g_rasterToPelX[g_zscanToRaster[absPartIdx]];
    UInt rpelx = lpelx + (g_maxCUWidth >> cu->getDepth(0))  - 1;
    UInt tpely = cu->getCUPelY() + g_rasterToPelY[g_zscanToRaster[absPartIdx]];
    UInt bpely = tpely + (g_maxCUHeight >>  cu->getDepth(0)) - 1;

    TComSlice * pcSlice = cu->getPic()->getSlice();
    if ((rpelx < pcSlice->getSPS()->getPicWidthInLumaSamples()) && (bpely < pcSlice->getSPS()->getPicHeightInLumaSamples()))
    {
        m_pcEntropyCoder->encodeSplitFlag(cu, absPartIdx,  cu->getDepth(0));
    }

    if (cu->getMergeFlag(0) && cu->getPartitionSize(0) == SIZE_2Nx2N && !cu->getQtRootCbf(0))
    {
        m_pcEntropyCoder->encodeMergeFlag(cu, 0);
        m_pcEntropyCoder->encodeMergeIndex(cu, 0, true);
    }

    if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
    {
        m_pcEntropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
    }

    if (!cu->getSlice()->isIntra())
    {
        m_pcEntropyCoder->encodeSkipFlag(cu, 0, true);
    }

    m_pcEntropyCoder->encodePredMode(cu, 0, true);

    m_pcEntropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);
    bits += m_pcEntropyCoder->getNumberOfWrittenBits();

    return bits;
}

/**
 * \brief Generate half-sample interpolated block
 *
 * \param pattern Reference picture ROI
 * \param biPred    Flag indicating whether block is for biprediction
 */
Void TEncSearch::xExtDIFUpSamplingH(TComPattern* pattern, Bool biPred)
{
    assert(g_bitDepthY == 8);

    Int width      = pattern->getROIYWidth();
    Int height     = pattern->getROIYHeight();
    Int srcStride  = pattern->getPatternLStride();

    Int intStride = filteredBlockTmp[0].width;
    Int dstStride = m_filteredBlock[0][0].getStride();

    Pel *srcPtr;    //Contains raw pixels
    Short *intPtr;  // Intermediate results in short
    Pel *dstPtr;    // Final filtered blocks in Pel

    Int filterSize = NTAPS_LUMA;
    Int halfFilterSize = (filterSize >> 1);

    srcPtr = (Pel*)pattern->getROIY() - halfFilterSize * srcStride - 1;

    dstPtr = m_filteredBlock[0][0].getLumaAddr();

    primitives.blockcpy_pp(width, height, (pixel*)dstPtr, dstStride, (pixel*)(srcPtr + halfFilterSize * srcStride + 1), srcStride);

    intPtr = filteredBlockTmp[0].getLumaAddr();
    primitives.ipfilter_p2s(g_bitDepthY, (pixel*)srcPtr, srcStride, intPtr,
                            intStride, width + 1, height + filterSize);

    intPtr = filteredBlockTmp[0].getLumaAddr() + (halfFilterSize - 1) * intStride + 1;
    dstPtr = m_filteredBlock[2][0].getLumaAddr();
    primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr,
                                           dstStride, width, height + 1, m_lumaFilter[2]);

    intPtr = filteredBlockTmp[2].getLumaAddr();
    primitives.ipfilter_ps[FILTER_H_P_S_8](g_bitDepthY, (pixel*)srcPtr, srcStride, intPtr, intStride, width + 1, height + filterSize,  m_lumaFilter[2]);

    intPtr = filteredBlockTmp[2].getLumaAddr() + halfFilterSize * intStride;
    dstPtr = m_filteredBlock[0][2].getLumaAddr();
    primitives.ipfilter_s2p(g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width + 1, height + 0);

    intPtr = filteredBlockTmp[2].getLumaAddr() + (halfFilterSize - 1) * intStride;
    dstPtr = m_filteredBlock[2][2].getLumaAddr();
    primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width + 1, height + 1, m_lumaFilter[2]);
}

/**
 * \brief Generate quarter-sample interpolated blocks
 *
 * \param pattern    Reference picture ROI
 * \param halfPelRef Half-pel mv
 * \param biPred     Flag indicating whether block is for biprediction
 */
Void TEncSearch::xExtDIFUpSamplingQ(TComPattern* pattern, MV halfPelRef, Bool biPred)
{
    assert(g_bitDepthY == 8);

    Int width      = pattern->getROIYWidth();
    Int height     = pattern->getROIYHeight();
    Int srcStride  = pattern->getPatternLStride();

    Int intStride = filteredBlockTmp[0].width;
    Int dstStride = m_filteredBlock[0][0].getStride();

    Pel *srcPtr;    //Contains raw pixels
    Short *intPtr;  // Intermediate results in short
    Pel *dstPtr;    // Final filtered blocks in Pel

    Int filterSize = NTAPS_LUMA;

    Int halfFilterSize = (filterSize >> 1);

    Int extHeight = (halfPelRef.y == 0) ? height + filterSize : height + filterSize - 1;

    // Horizontal filter 1/4
    srcPtr = pattern->getROIY() - halfFilterSize * srcStride - 1;
    intPtr = filteredBlockTmp[1].getLumaAddr();
    if (halfPelRef.y > 0)
    {
        srcPtr += srcStride;
    }
    if (halfPelRef.x >= 0)
    {
        srcPtr += 1;
    }
    primitives.ipfilter_ps[FILTER_H_P_S_8](g_bitDepthY, (pixel*)srcPtr, srcStride, intPtr, intStride, width, extHeight, m_lumaFilter[1]);

    // Horizontal filter 3/4
    srcPtr = pattern->getROIY() - halfFilterSize * srcStride - 1;
    intPtr = filteredBlockTmp[3].getLumaAddr();
    if (halfPelRef.y > 0)
    {
        srcPtr += srcStride;
    }
    if (halfPelRef.x > 0)
    {
        srcPtr += 1;
    }
    primitives.ipfilter_ps[FILTER_H_P_S_8](g_bitDepthY, (pixel*)srcPtr, srcStride, intPtr, intStride, width, extHeight, m_lumaFilter[3]);

    // Generate @ 1,1
    intPtr = filteredBlockTmp[1].getLumaAddr() + (halfFilterSize - 1) * intStride;
    dstPtr = m_filteredBlock[1][1].getLumaAddr();
    if (halfPelRef.y == 0)
    {
        intPtr += intStride;
    }
    primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[1]);

    // Generate @ 3,1
    intPtr = filteredBlockTmp[1].getLumaAddr() + (halfFilterSize - 1) * intStride;
    dstPtr = m_filteredBlock[3][1].getLumaAddr();
    primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[3]);

    if (halfPelRef.y != 0)
    {
        // Generate @ 2,1
        intPtr = filteredBlockTmp[1].getLumaAddr() + (halfFilterSize - 1) * intStride;
        dstPtr = m_filteredBlock[2][1].getLumaAddr();
        if (halfPelRef.y == 0)
        {
            intPtr += intStride;
        }
        primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[2]);

        // Generate @ 2,3
        intPtr = filteredBlockTmp[3].getLumaAddr() + (halfFilterSize - 1) * intStride;
        dstPtr = m_filteredBlock[2][3].getLumaAddr();
        if (halfPelRef.y == 0)
        {
            intPtr += intStride;
        }
        primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[2]);
    }
    else
    {
        // Generate @ 0,1
        intPtr = filteredBlockTmp[1].getLumaAddr() + halfFilterSize * intStride;
        dstPtr = m_filteredBlock[0][1].getLumaAddr();
        primitives.ipfilter_s2p(g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height);

        // Generate @ 0,3
        intPtr = filteredBlockTmp[3].getLumaAddr() + halfFilterSize * intStride;
        dstPtr = m_filteredBlock[0][3].getLumaAddr();
        primitives.ipfilter_s2p(g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height);
    }

    if (halfPelRef.x != 0)
    {
        // Generate @ 1,2
        intPtr = filteredBlockTmp[2].getLumaAddr() + (halfFilterSize - 1) * intStride;
        dstPtr = m_filteredBlock[1][2].getLumaAddr();
        if (halfPelRef.x > 0)
        {
            intPtr += 1;
        }
        if (halfPelRef.y >= 0)
        {
            intPtr += intStride;
        }

        primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[1]);

        // Generate @ 3,2
        intPtr = filteredBlockTmp[2].getLumaAddr() + (halfFilterSize - 1) * intStride;
        dstPtr = m_filteredBlock[3][2].getLumaAddr();
        if (halfPelRef.x > 0)
        {
            intPtr += 1;
        }
        if (halfPelRef.y > 0)
        {
            intPtr += intStride;
        }
        primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[3]);
    }
    else
    {
        // Generate @ 1,0
        intPtr = filteredBlockTmp[0].getLumaAddr() + (halfFilterSize - 1) * intStride + 1;
        dstPtr = m_filteredBlock[1][0].getLumaAddr();
        if (halfPelRef.y >= 0)
        {
            intPtr += intStride;
        }
        primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[1]);

        // Generate @ 3,0
        intPtr = filteredBlockTmp[0].getLumaAddr() + (halfFilterSize - 1) * intStride + 1;
        dstPtr = (Pel*)m_filteredBlock[3][0].getLumaAddr();
        if (halfPelRef.y > 0)
        {
            intPtr += intStride;
        }
        primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[3]);
    }

    // Generate @ 1,3
    intPtr = filteredBlockTmp[3].getLumaAddr() + (halfFilterSize - 1) * intStride;
    dstPtr = m_filteredBlock[1][3].getLumaAddr();
    if (halfPelRef.y == 0)
    {
        intPtr += intStride;
    }
    primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[1]);

    // Generate @ 3,3
    intPtr = filteredBlockTmp[3].getLumaAddr() + (halfFilterSize - 1) * intStride;
    dstPtr = m_filteredBlock[3][3].getLumaAddr();
    primitives.ipfilter_sp[FILTER_V_S_P_8](g_bitDepthY, intPtr, intStride, (pixel*)dstPtr, dstStride, width, height, m_lumaFilter[3]);
}

/** set wp tables
 * \param TComDataCU* cu
 * \param refIfx
 * \param eRefPicListCur
 * \returns Void
 */
Void  TEncSearch::setWpScalingDistParam(TComDataCU* cu, Int refIfx, RefPicList picList)
{
    if (refIfx < 0)
    {
        m_cDistParam.bApplyWeight = false;
        return;
    }

    TComSlice       *pcSlice  = cu->getSlice();
    TComPPS         *pps      = cu->getSlice()->getPPS();
    wpScalingParam  *wp0, *wp1;
    m_cDistParam.bApplyWeight = (pcSlice->getSliceType() == P_SLICE && pps->getUseWP()) || (pcSlice->getSliceType() == B_SLICE && pps->getWPBiPred());
    if (!m_cDistParam.bApplyWeight) return;

    Int refIdx0 = (picList == REF_PIC_LIST_0) ? refIfx : (-1);
    Int refIdx1 = (picList == REF_PIC_LIST_1) ? refIfx : (-1);

    getWpScaling(cu, refIdx0, refIdx1, wp0, wp1);

    if (refIdx0 < 0) wp0 = NULL;
    if (refIdx1 < 0) wp1 = NULL;

    m_cDistParam.wpCur  = NULL;

    if (picList == REF_PIC_LIST_0)
    {
        m_cDistParam.wpCur = wp0;
    }
    else
    {
        m_cDistParam.wpCur = wp1;
    }
}

//! \}
