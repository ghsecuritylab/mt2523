/* Copyright Statement:
 *
 * (C) 2005-2016  MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. ("MediaTek") and/or its licensors.
 * Without the prior written permission of MediaTek and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 * You may only use, reproduce, modify, or distribute (as applicable) MediaTek Software
 * if you have agreed to and been bound by the applicable license agreement with
 * MediaTek ("License Agreement") and been granted explicit permission to do so within
 * the License Agreement ("Permitted User").  If you are not a Permitted User,
 * please cease any access or use of MediaTek Software immediately.
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT MEDIATEK SOFTWARE RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES
 * ARE PROVIDED TO RECEIVER ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 */

/*
** $Id: //Department/DaVinci/TRUNK/MT6620_5931_WiFi_Driver/mgmt/rlm_obss.c#15 $
*/

/*! \file   "rlm_obss.c"
    \brief

*/




/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID
rlmObssScanTimeout(
    P_ADAPTER_T prAdapter,
    UINT_32     u4Data
);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
rlmObssInit(
    P_ADAPTER_T     prAdapter
)
{
    P_BSS_INFO_T    prBssInfo;
    UINT_8          ucNetIdx;

    RLM_NET_FOR_EACH(ucNetIdx) {
        prBssInfo = &prAdapter->rWifiVar.arBssInfo[ucNetIdx];
        ASSERT(prBssInfo);

        cnmTimerInitTimer(prAdapter, &prBssInfo->rObssScanTimer,
                          rlmObssScanTimeout, (UINT_32) prBssInfo);
    } /* end of RLM_NET_FOR_EACH */
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
rlmObssUpdateChnlLists(
    P_ADAPTER_T prAdapter,
    P_SW_RFB_T  prSwRfb
)
{
    return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
rlmObssScanDone(
    P_ADAPTER_T prAdapter,
    P_MSG_HDR_T prMsgHdr
)
{
    P_MSG_SCN_SCAN_DONE             prScanDoneMsg;
    P_BSS_INFO_T                    prBssInfo;
    P_MSDU_INFO_T                   prMsduInfo;
    P_ACTION_20_40_COEXIST_FRAME    prTxFrame;
    UINT_16                         i, u2PayloadLen;

    ASSERT(prMsgHdr);

    prScanDoneMsg = (P_MSG_SCN_SCAN_DONE) prMsgHdr;
    prBssInfo = &prAdapter->rWifiVar.arBssInfo[prScanDoneMsg->ucNetTypeIndex];
    ASSERT(prBssInfo);

    DBGLOG(RLM, INFO, ("OBSS Scan Done (NetIdx=%d, Mode=%d)\n",
                       prScanDoneMsg->ucNetTypeIndex, prBssInfo->eCurrentOPMode));

    cnmMemFree(prAdapter, prMsgHdr);

#if CFG_ENABLE_WIFI_DIRECT
    /* AP mode */
    if ((prAdapter->fgIsP2PRegistered) &&
            (IS_NET_ACTIVE(prAdapter, prBssInfo->ucNetTypeIndex)) &&
            (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {
        return;
    }
#endif

    /* STA mode */
    if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
            !RLM_NET_PARAM_VALID(prBssInfo) || prBssInfo->u2ObssScanInterval == 0) {
        DBGLOG(RLM, WARN, ("OBSS Scan Done (NetIdx=%d) -- Aborted!!\n",
                           prBssInfo->ucNetTypeIndex));
        return;
    }

    /* To do: check 2.4G channel list to decide if obss mgmt should be
     *        sent to associated AP. Note: how to handle concurrent network?
     * To do: invoke rlmObssChnlLevel() to decide if 20/40 BSS coexistence
     *        management frame is needed.
     */
    if ((prBssInfo->auc2G_20mReqChnlList[0] > 0 ||
            prBssInfo->auc2G_NonHtChnlList[0] > 0) &&
            (prMsduInfo = (P_MSDU_INFO_T) cnmMgtPktAlloc(prAdapter,
                          MAC_TX_RESERVED_FIELD + PUBLIC_ACTION_MAX_LEN)) != NULL) {

        DBGLOG(RLM, INFO, ("Send 20/40 coexistence mgmt(20mReq=%d, NonHt=%d)\n",
                           prBssInfo->auc2G_20mReqChnlList[0],
                           prBssInfo->auc2G_NonHtChnlList[0]));

        prTxFrame = (P_ACTION_20_40_COEXIST_FRAME)
                    ((UINT_32)(prMsduInfo->prPacket) + MAC_TX_RESERVED_FIELD);

        prTxFrame->u2FrameCtrl = MAC_FRAME_ACTION;
        COPY_MAC_ADDR(prTxFrame->aucDestAddr, prBssInfo->aucBSSID);
        COPY_MAC_ADDR(prTxFrame->aucSrcAddr, prBssInfo->aucOwnMacAddr);
        COPY_MAC_ADDR(prTxFrame->aucBSSID, prBssInfo->aucBSSID);

        prTxFrame->ucCategory = CATEGORY_PUBLIC_ACTION;
        prTxFrame->ucAction = ACTION_PUBLIC_20_40_COEXIST;

        /* To do: find correct algorithm */
        prTxFrame->rBssCoexist.ucId = ELEM_ID_20_40_BSS_COEXISTENCE;
        prTxFrame->rBssCoexist.ucLength = 1;
        prTxFrame->rBssCoexist.ucData =
            (prBssInfo->auc2G_20mReqChnlList[0] > 0) ? BSS_COEXIST_20M_REQ : 0;

        u2PayloadLen = 2 + 3;

        if (prBssInfo->auc2G_NonHtChnlList[0] > 0) {
            ASSERT(prBssInfo->auc2G_NonHtChnlList[0] <= CHNL_LIST_SZ_2G);

            prTxFrame->rChnlReport.ucId = ELEM_ID_20_40_INTOLERANT_CHNL_REPORT;
            prTxFrame->rChnlReport.ucLength =
                prBssInfo->auc2G_NonHtChnlList[0] + 1;
            prTxFrame->rChnlReport.ucRegulatoryClass = 81; /* 2.4GHz, ch1~13 */
            for (i = 0; i < prBssInfo->auc2G_NonHtChnlList[0] &&
                    i < CHNL_LIST_SZ_2G; i++) {
                prTxFrame->rChnlReport.aucChannelList[i] =
                    prBssInfo->auc2G_NonHtChnlList[i + 1];
            }

            u2PayloadLen += IE_SIZE(&prTxFrame->rChnlReport);
        }
        ASSERT((WLAN_MAC_HEADER_LEN + u2PayloadLen) <= PUBLIC_ACTION_MAX_LEN);

        /* Clear up channel lists in 2.4G band */
        prBssInfo->auc2G_20mReqChnlList[0] = 0;
        prBssInfo->auc2G_NonHtChnlList[0] = 0;


        //4 Update information of MSDU_INFO_T
        prMsduInfo->ucPacketType = HIF_TX_PACKET_TYPE_MGMT;   /* Management frame */
        prMsduInfo->ucStaRecIndex = prBssInfo->prStaRecOfAP->ucIndex;
        prMsduInfo->ucNetworkType = prBssInfo->ucNetTypeIndex;
        prMsduInfo->ucMacHeaderLength = WLAN_MAC_MGMT_HEADER_LEN;
        prMsduInfo->fgIs802_1x = FALSE;
        prMsduInfo->fgIs802_11 = TRUE;
        prMsduInfo->u2FrameLength = WLAN_MAC_MGMT_HEADER_LEN + u2PayloadLen;
        prMsduInfo->ucTxSeqNum = nicIncreaseTxSeqNum(prAdapter);
        prMsduInfo->pfTxDoneHandler = NULL;
        prMsduInfo->fgIsBasicRate = FALSE;

        //4 Enqueue the frame to send this action frame.
        nicTxEnqueueMsdu(prAdapter, prMsduInfo);
    } /* end of prMsduInfo != NULL */

    if (prBssInfo->u2ObssScanInterval > 0) {
        DBGLOG(RLM, INFO, ("Set OBSS timer (NetIdx=%d, %d sec)\n",
                           prBssInfo->ucNetTypeIndex, prBssInfo->u2ObssScanInterval));

        cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer,
                           prBssInfo->u2ObssScanInterval * MSEC_PER_SEC);
    }
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID
rlmObssScanTimeout(
    P_ADAPTER_T prAdapter,
    UINT_32     u4Data
)
{
    P_BSS_INFO_T        prBssInfo;

    prBssInfo = (P_BSS_INFO_T) u4Data;
    ASSERT(prBssInfo);

#if CFG_ENABLE_WIFI_DIRECT
    /* AP mode */
    if (prAdapter->fgIsP2PRegistered &&
            (IS_NET_ACTIVE(prAdapter, prBssInfo->ucNetTypeIndex)) &&
            (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)) {

        prBssInfo->fgObssActionForcedTo20M = FALSE;

        /* Check if Beacon content need to be updated */
        ASSERT(prAdapter->rP2pFuncLkr.prRlmUpdateParamsForAp);
        prAdapter->rP2pFuncLkr.prRlmUpdateParamsForAp(
            prAdapter, prBssInfo, FALSE);
        return;
    }
#endif /* end of CFG_ENABLE_WIFI_DIRECT */


    /* STA mode */
    if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE ||
            !RLM_NET_PARAM_VALID(prBssInfo) || prBssInfo->u2ObssScanInterval == 0) {
        DBGLOG(RLM, WARN, ("OBSS Scan timeout (NetIdx=%d) -- Aborted!!\n",
                           prBssInfo->ucNetTypeIndex));
        return;
    }

    rlmObssTriggerScan(prAdapter, prBssInfo);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param[in]
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
rlmObssTriggerScan(
    P_ADAPTER_T         prAdapter,
    P_BSS_INFO_T        prBssInfo
)
{

    P_MSG_SCN_SCAN_REQ  prScanReqMsg;

    ASSERT(prBssInfo);

    prScanReqMsg = (P_MSG_SCN_SCAN_REQ)
                   cnmMemAlloc(prAdapter, RAM_TYPE_MSG, sizeof(MSG_SCN_SCAN_REQ));
    if (!prScanReqMsg) {
        //ASSERT(0);
        MT5931_TRACE0(TRACE_WARNING, MT5931_INFO_119, "rlmObssTriggerScan():cnmMemAlloc() fail");
        /* NOTE(Nelson): OBSS SCAN is triggerred from FW, not by users,
                         if allocate buf fail, wait certain interval and then re-perform OBSS SCAN
        */
        cnmTimerStartTimer(prAdapter, &prBssInfo->rObssScanTimer,
                           prBssInfo->u2ObssScanInterval * MSEC_PER_SEC);
        return;
    }

    /* It is ok that ucSeqNum is set to fixed value because the same network
     * OBSS scan interval is limited to OBSS_SCAN_MIN_INTERVAL (min 10 sec)
     * and scan module don't care seqNum of OBSS scanning
     */
    prScanReqMsg->rMsgHdr.eMsgId = MID_RLM_SCN_SCAN_REQ;
    prScanReqMsg->ucSeqNum = 0x33;
    prScanReqMsg->ucNetTypeIndex = prBssInfo->ucNetTypeIndex;
    prScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
    prScanReqMsg->ucSSIDType = SCAN_REQ_SSID_WILDCARD;
    prScanReqMsg->ucSSIDLength = 0;
    prScanReqMsg->eScanChannel = SCAN_CHANNEL_2G4;
    prScanReqMsg->u2IELen = 0;

    mboxSendMsg(prAdapter,
                MBOX_ID_0,
                (P_MSG_HDR_T) prScanReqMsg,
                MSG_SEND_METHOD_BUF);

    DBGLOG(RLM, INFO, ("Timeout to trigger OBSS scan (NetIdx=%d)!!\n",
                       prBssInfo->ucNetTypeIndex));
}


