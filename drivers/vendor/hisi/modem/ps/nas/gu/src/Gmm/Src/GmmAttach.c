
#include "GmmInc.h"
#include "GmmCasGlobal.h"
#include "GmmCasComm.h"
#include "GmmCasSend.h"
#include "GmmCasGsm.h"
#include "GmmCasMain.h"
#include "pslog.h"
#if (FEATURE_ON == FEATURE_LTE)
#include "NasCommDef.h"
#include "NasGmmProcLResult.h"
#endif

#include "NasUtranCtrlInterface.h"
#include "NasMmlCtx.h"
#include "NasUsimmApi.h"

#include "NasStkInterface.h"
#include "NasMmlLib.h"

#include "NasGmmSndOm.h"

#include "NasCcIe.h"

#ifdef  __cplusplus
  #if  __cplusplus
  extern "C"{
  #endif
#endif


/*****************************************************************************
    协议栈打印打点方式下的.C文件宏定义
*****************************************************************************/
#define    THIS_FILE_ID        PS_FILE_ID_GMM_ATTACH_C

extern VOS_UINT16 NAS_MMC_GetUserSetGsmBand(VOS_VOID);



VOS_VOID NAS_MNTN_ServiceUnavailableType(
    VOS_UINT16                          usCause,
    VOS_BOOL                           *pbEventRequired
)
{
    switch(usCause)
    {
        case NAS_MML_REG_FAIL_CAUSE_IMSI_UNKNOWN_IN_HLR:
        case NAS_MML_REG_FAIL_CAUSE_ILLEGAL_MS:
        case NAS_MML_REG_FAIL_CAUSE_ILLEGAL_ME:
        case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_AND_NON_GPRS_SERV_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_PLMN_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_LA_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_ROAM_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW_IN_PLMN:
        case NAS_MML_REG_FAIL_CAUSE_NO_SUITABL_CELL:

            /* 以上错误原因值都是需要直接记录事件的:
            UE因为上述原因值导致注册失败需要重选后才会发起新的注册请求 */
            *pbEventRequired = VOS_TRUE;
            break;
        default:
            *pbEventRequired = VOS_FALSE;
            break;
    }

    return;
}


VOS_VOID NAS_GMM_StorePsUnavailableInfo(
    VOS_UINT8                           ucCause,
    NAS_MNTN_PS_UNAVAILABLE_INFO_STRU  *pstPsUnavailableInfo
)
{
    VOS_UINT32                          ulPos;

    /* 事件记录标志设置一: 尝试次数达上限 */
    if (g_GmmRauCtrl.ucRauAttmptCnt > NAS_MNTN_PS_ATTEMPT_MAX)
    {
        return;
    }

    /* 事件记录标志设置二: 注册失败原因值指示当前驻留位无法提供服务，重选后再尝试注册 */
    NAS_MNTN_ServiceUnavailableType(ucCause, &pstPsUnavailableInfo->bRecordRequired);

    /* 异常数据直接退出一: 注册失败原因值没有指示当前驻留位无法提供服务，
       且注册失败计数器值为0 */
    if ((VOS_TRUE != pstPsUnavailableInfo->bRecordRequired)
     && (0 == g_GmmRauCtrl.ucRauAttmptCnt))
    {
        NAS_WARNING_LOG(WUEPS_PID_GMM, "NAS_GMM_StorePsUnavailableInfo: Error Counter.");
        return;
    }

    /* 异常数据直接退出二: 注册失败计数器值超过最大值 */
    if (NAS_MNTN_PS_ATTEMPT_MAX == pstPsUnavailableInfo->ucTimes)
    {
        pstPsUnavailableInfo->bRecordRequired = VOS_TRUE;
    }

    /* 记录注册失败原因值到全局变量 */
    if (0 != g_GmmRauCtrl.ucRauAttmptCnt)
    {
        ulPos = g_GmmRauCtrl.ucRauAttmptCnt - 1;
    }
    else
    {
        ulPos = 0;
    }

    pstPsUnavailableInfo->aulCause[ulPos]= ucCause;
    pstPsUnavailableInfo->ucTimes = g_GmmRauCtrl.ucRauAttmptCnt;

    return;
}

/*******************************************************************************
  Module   : Gmm_FillRaiForRcv
  Function : 将空口消息中的RAI填写到全局变量
  Input    : GMM_RAI_STRU *pRaiDes  指向GMM_RAI_STRU结构的指针变量
             VOS_UINT8        *pRaiSrc  指向RAI的指针变量
  Output   : 无
  NOTE     : 无
  Return   : 无
  History  :
    1. 张志勇  2003.12.09  新规作成
*******************************************************************************/
VOS_VOID Gmm_FillRaiForRcv(
                       GMM_RAI_STRU *pRaiDes,                                   /* 指向GMM_RAI_STRU结构的指针变量           */
                       VOS_UINT8        *pRaiSrc                                    /* 指向RAI的指针变量                        */
                       )
{
    /* 存储MCC */
    pRaiDes->Lai.PlmnId.aucMccDigit[0] = (VOS_UINT8)(pRaiSrc[0] & 0x0F);
    pRaiDes->Lai.PlmnId.aucMccDigit[1] = (VOS_UINT8)(pRaiSrc[0] >> 4);
    pRaiDes->Lai.PlmnId.aucMccDigit[2] = (VOS_UINT8)(pRaiSrc[1] & 0x0F);

    /* 存储MNC */
    pRaiDes->Lai.PlmnId.ucMncCnt = 3;
    pRaiDes->Lai.PlmnId.aucMncDigit[2] = (VOS_UINT8)(pRaiSrc[1] >> 4);
    if (0xF0 == (pRaiSrc[1] & 0xF0))
    {                                                                           /* 2个MNC                                   */
        pRaiDes->Lai.PlmnId.ucMncCnt = 2;
    }
    pRaiDes->Lai.PlmnId.aucMncDigit[0] = (VOS_UINT8)(pRaiSrc[2] & 0x0F);
    pRaiDes->Lai.PlmnId.aucMncDigit[1] = (VOS_UINT8)(pRaiSrc[2] >> 4);

    /* 存储LAC */
    pRaiDes->Lai.aucLac[0] = pRaiSrc[3];
    pRaiDes->Lai.aucLac[1] = pRaiSrc[4];

    /* 存储RAC */
    pRaiDes->ucRac = pRaiSrc[5];

    return;
}
VOS_UINT32 NAS_GMM_JudgeLaiIfLegal(NAS_MSG_FOR_PCLINT_STRU *pMsg)
{
    if (((pMsg->aucNasMsg[4] & 0x0F) > 9)
    || (((pMsg->aucNasMsg[4] >> 4 ) & 0x0F) > 9)
    || ((pMsg->aucNasMsg[5] & 0x0F) > 9)
    || ((pMsg->aucNasMsg[6] & 0x0F) > 9)
    || (((pMsg->aucNasMsg[6] >> 4) & 0x0F) > 9))
    {                                                                           /* MCC和MNC的取值错误                       */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_JudgeLaiIfLegal:WARNING: IE\"RAI\" is invalid in RAU ACCEPT");
        return VOS_FALSE;
    }

    if ((((pMsg->aucNasMsg[5] >> 4) & 0x0F) > 9)
     && (((pMsg->aucNasMsg[5] >> 4) & 0x0F) != 0x0F))
    {                                                                           /* MNC3取值错误                             */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_JudgeLaiIfLegal:WARNING: IE\"MNC3\" is invalid in RAU ACCEPT");
        return VOS_FALSE;
    }

    return VOS_TRUE;

}


VOS_UINT8 Gmm_AttachRauAcceptJudgeRai(
                                  NAS_MSG_FOR_PCLINT_STRU *pMsg
                                  )
{
    GMM_RAI_STRU    RaiTemp;                                                    /* 定义GMM_RAI_STRU类型的临时变量           */
    VOS_UINT8           ucRet = GMM_FALSE;                                          /* 初期化返回值为FALSE                      */

    NAS_MML_RAI_STRU                       *pstLastSuccRai;

    pstLastSuccRai    = NAS_MML_GetPsLastSuccRai();

    if (GMM_REGISTERED_INITIATED == g_GmmGlobalCtrl.ucState)
    {                                                                           /* 当前进行的是ATTACH过程                   */
        Gmm_FillRaiForRcv(&RaiTemp, &(pMsg->aucNasMsg[5]));                     /* 调用函数存储RAI到临时变量中              */
    }
    else
    {                                                                           /* 当前进行的是RAU流程                      */
        Gmm_FillRaiForRcv(&RaiTemp, &pMsg->aucNasMsg[4]);                       /* 调用函数存储RAI到临时变量中              */
    }
    if (GMM_TRUE == Gmm_Com_CmpPlmnid(&RaiTemp.Lai.PlmnId))
    {                                                                           /* PLMN ID一致                              */
        if ((RaiTemp.Lai.aucLac[0]
            == g_GmmGlobalCtrl.SysInfo.Rai.Lai.aucLac[0])
            && (RaiTemp.Lai.aucLac[1]
            == g_GmmGlobalCtrl.SysInfo.Rai.Lai.aucLac[1]))
        {                                                                       /* LAC一致                                  */
            if (RaiTemp.ucRac == g_GmmGlobalCtrl.SysInfo.Rai.ucRac)
            {                                                                   /* RAC一致                                  */
                pstLastSuccRai->ucRac               = RaiTemp.ucRac;
                pstLastSuccRai->stLai.aucLac[0]     = RaiTemp.Lai.aucLac[0];
                pstLastSuccRai->stLai.aucLac[1]     = RaiTemp.Lai.aucLac[1];
                NAS_GMM_ConvertPlmnIdToMmcFormat(&(RaiTemp.Lai.PlmnId),
                    &(pstLastSuccRai->stLai.stPlmnId));

                ucRet = GMM_TRUE;
            }
        }
    }

    if (GMM_FALSE == ucRet)
    {
        if (GMM_TRUE == g_GmmRauCtrl.ucPeriodicRauFlg)
        {
            if (VOS_TRUE == NAS_GMM_JudgeLaiIfLegal(pMsg))
            {
                pstLastSuccRai->ucRac               = RaiTemp.ucRac;
                pstLastSuccRai->stLai.aucLac[0]     = RaiTemp.Lai.aucLac[0];
                pstLastSuccRai->stLai.aucLac[1]     = RaiTemp.Lai.aucLac[1];
                NAS_GMM_ConvertPlmnIdToMmcFormat(&(RaiTemp.Lai.PlmnId),
                    &(pstLastSuccRai->stLai.stPlmnId));
            }
        }
        else
        {
            pstLastSuccRai->ucRac               = g_GmmGlobalCtrl.SysInfo.Rai.ucRac;
            pstLastSuccRai->stLai.aucLac[0]     = g_GmmGlobalCtrl.SysInfo.Rai.Lai.aucLac[0];
            pstLastSuccRai->stLai.aucLac[1]     = g_GmmGlobalCtrl.SysInfo.Rai.Lai.aucLac[1];
            NAS_GMM_ConvertPlmnIdToMmcFormat(&(g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId),
                &(pstLastSuccRai->stLai.stPlmnId));

        }
        ucRet = GMM_TRUE;
    }

    /* 保留注册成功时所在的小区信息 */
    g_GmmGlobalCtrl.SysInfo_Registered = g_GmmGlobalCtrl.SysInfo;

    return ucRet;
}


/*==>A32D11726*/
/*******************************************************************************
  Module   : GMM_FillReadyTimerParaIe
  Function : 填写空口消息中的Requested READY timer value
  Input    : VOS_UINT8 *pAddr  填写Requested READY timer value的首地址
  Output   : 无
  NOTE     : 无
  Return   : 无
  History  :
     1.日    期   : 2007年06月16日
       作    者   : luojian id:60022475
       修改内容   : 根据问题单号：A32D11726创建
*******************************************************************************/
VOS_VOID    GMM_FillReadyTimerParaIe (VOS_UINT8 *pAddr)
{
    /* IEI  */
    pAddr[0] = GMM_IEI_REQUESTED_READY_TIMER_VALUE;
    /* 填写Ready timer值                     */
    pAddr[1] = GMM_REQUESTED_READY_TIMER_VALUE;
    return;
}
/*<==A32D11726*/

VOS_VOID Gmm_FillPtmsi(
                   VOS_UINT8 *pAddr                                                 /* 填写P-TMSI的首地址                       */
                   )
{
    VOS_UINT8                          *pucMmlPtmsi;

    pAddr[0]  = 5;                                                              /* Length of mobile identity contents       */
    pAddr[1]  = 0xF0;                                                           /* 高4bit置"1111"                           */
    pAddr[1] |= GMM_MOBILE_ID_TMSI_PTMSI;                                       /* Type of identity                         */

    /* 填写P-TMSI           */
    pucMmlPtmsi = NAS_MML_GetUeIdPtmsi();
    pAddr[2] = pucMmlPtmsi[0];
    pAddr[3] = pucMmlPtmsi[1];
    pAddr[4] = pucMmlPtmsi[2];
    pAddr[5] = pucMmlPtmsi[3];

    return;
}
VOS_VOID NAS_GMM_Fill_IE_PtmsiFromSrc(
    VOS_UINT8                           *pucDestAddr,
    VOS_UINT8                           *pucSrcAddr
)
{
    pucDestAddr[0]  = 5;                                                        /* Length of mobile identity contents       */
    pucDestAddr[1]  = 0xF0;                                                     /* 高4bit置"1111"                           */
    pucDestAddr[1] |= GMM_MOBILE_ID_TMSI_PTMSI;                                 /* Type of identity                         */

    /* 填写P-TMSI */
    PS_MEM_CPY(&pucDestAddr[2], pucSrcAddr, NAS_MML_MAX_PTMSI_LEN);

    return;
}



VOS_VOID NAS_GMM_Fill_IE_RaiFromSrc(
    VOS_UINT8                          *pucDestAddr,                           /* 填写P-TMSI的首地址                       */
    GMM_RAI_STRU                       *pstRaiAddr
)
{
    pucDestAddr[0]  = pstRaiAddr->Lai.PlmnId.aucMccDigit[0];         /* 填写MCC                                  */

    pucDestAddr[0] |=
        (VOS_UINT8)( pstRaiAddr->Lai.PlmnId.aucMccDigit[1] << 4);
    pucDestAddr[1]  = pstRaiAddr->Lai.PlmnId.aucMccDigit[2];

    pucDestAddr[1] |=
        (VOS_UINT8)( pstRaiAddr->Lai.PlmnId.aucMncDigit[2] << 4);  /* 填写MNC                                  */
    pucDestAddr[2]  = pstRaiAddr->Lai.PlmnId.aucMncDigit[0];
    pucDestAddr[2] |=
        (VOS_UINT8)(pstRaiAddr->Lai.PlmnId.aucMncDigit[1] << 4);

    pucDestAddr[3]  = pstRaiAddr->Lai.aucLac[0];                     /* 填写LAC                                  */
    pucDestAddr[4]  = pstRaiAddr->Lai.aucLac[1];

    pucDestAddr[5]  = pstRaiAddr->ucRac;                             /* 填写RAC                                  */

    return;
}
VOS_VOID NAS_GMM_Fill_IE_AdditionalOldRaiFromSrc(
    VOS_UINT8                          *pucDestAddr,                            /* 填写P-TMSI的首地址                       */
    GMM_RAI_STRU                       *pucSrcRaiAddr
)
{
    /* 24008:10.5.5.15a Routing area identification 2 */
    /* TS 24.008: ATTACH REQUEST message content */
    pucDestAddr[0]  = GMM_IEI_ADDITIONAL_OLD_RAI_VALUE;


    /* Length of IEI */
    pucDestAddr[1]  = NAS_MML_ADDITIONAL_OLD_RAI_IEI_LEN;

    pucDestAddr[2]  = pucSrcRaiAddr->Lai.PlmnId.aucMccDigit[0];                 /* 填写MCC                                  */

    pucDestAddr[2] |=
        (VOS_UINT8)( pucSrcRaiAddr->Lai.PlmnId.aucMccDigit[1] << 4);
    pucDestAddr[3]  = pucSrcRaiAddr->Lai.PlmnId.aucMccDigit[2];

    pucDestAddr[3] |=
        (VOS_UINT8)( pucSrcRaiAddr->Lai.PlmnId.aucMncDigit[2] << 4);            /* 填写MNC                                  */
    pucDestAddr[4]  = pucSrcRaiAddr->Lai.PlmnId.aucMncDigit[0];
    pucDestAddr[4] |=
        (VOS_UINT8)(pucSrcRaiAddr->Lai.PlmnId.aucMncDigit[1] << 4);

    pucDestAddr[5]  = pucSrcRaiAddr->Lai.aucLac[0];                             /* 填写LAC                                  */
    pucDestAddr[6]  = pucSrcRaiAddr->Lai.aucLac[1];

    pucDestAddr[7]  = pucSrcRaiAddr->ucRac;                                     /* 填写RAC                                  */

    return;
}


VOS_UINT8 NAS_Gmm_Fill_IE_AttachTypeAndCksn(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                           ucNumber;
    VOS_UINT8                           ucAttachType;
    VOS_UINT8                           ucSpecProc;
    VOS_UINT8                           ucCksn;
    NAS_MML_NET_RAT_TYPE_ENUM_UINT8     enNetRatType;
    NAS_MML_SIM_TYPE_ENUM_UINT8         enSimType;

    enNetRatType                        = NAS_MML_GetCurrNetRatType();
    enSimType                           = NAS_MML_GetSimType();
    ucSpecProc                          = NAS_GMM_GetSpecProc();
    ucNumber                            = 1;

    /* 有CS业务存在时,RAU被拒导致的重新attach依然需要进行普通的attach
       否则会导致CS业务中断 */
    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
     || (GMM_ATTACH_NORMAL          == g_GmmGlobalCtrl.ucSpecProc)
     || (GMM_TRUE                   == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg))
    {                                                                           /* 根据当前的specific流程判断attach的类型   */
        ucAttachType = GMM_GPRS_ATTACH;                                         /* 设置ATTACH类型                           */
    }
    else if (GMM_ATTACH_WHILE_IMSI_ATTACHED == ucSpecProc)
    {
        /*ucAttachType = GMM_GPRS_ATTACH_WHILE_IMSI_ATTACH;*/                   /* 设置ATTACH类型                           */
        ucAttachType = GMM_COMBINED_GPRS_IMSI_ATTACH;
    }
    else
    {
        ucAttachType = GMM_COMBINED_GPRS_IMSI_ATTACH;                           /* 设置ATTACH类型                           */
    }

    pucAddr[0] = ucAttachType;                                                  /* attach type                              */

    if (VOS_TRUE == NAS_GMM_IsFollowOnPend())
    {                                                                           /* 需要设置follow on                        */
        pucAddr[0] |= GMM_FOLLOW_ON_REQ_PENDING;                                /* follow on                                */
    }

    /* SIM卡在W网络中发起ATTACH,将CKSN置为无效 */
    if ((NAS_MML_NET_RAT_TYPE_WCDMA == enNetRatType)
     && (NAS_MML_SIM_TYPE_SIM == enSimType))
    {
        NAS_MML_SetSimPsSecurityCksn(NAS_MML_CKSN_INVALID);
    }
    ucCksn          = NAS_MML_GetSimPsSecurityCksn();
    pucAddr[0]     |= ((ucCksn << 4) & 0xF0);                                   /* CKSN */

    return ucNumber;
}
VOS_VOID NAS_GMM_Fill_IE_PtmsiSignFromSrc(
    VOS_UINT8                           *pucDestAddr,                           /* 填写P-TMSI的首地址                       */
    VOS_UINT8                           *pucSrcAddr
)
{
    pucDestAddr[0]  = GMM_IEI_P_TMSI_SIGNATURE;                                                              /* Length of mobile identity contents       */

    /* 填写P-TMSI Signature*/
    PS_MEM_CPY(&pucDestAddr[1], pucSrcAddr, NAS_MML_MAX_PTMSI_SIGNATURE_LEN);

    return;
}
VOS_VOID GMM_Fill_IE_ClassMark2(
    VOS_UINT8                          *pClassMark2                             /* 填写ClassMark2 IE的首地址   */
)
{
    VOS_UINT8                           aucClassMark2[GMM_MSG_LEN_CLASSMARK2];

    PS_MEM_SET(aucClassMark2, 0, sizeof(aucClassMark2));

    aucClassMark2[0] = GMM_IEI_CLASSMARK2;

    NAS_MML_Fill_IE_ClassMark2(&aucClassMark2[1]);

    PS_MEM_CPY(pClassMark2, aucClassMark2, GMM_MSG_LEN_CLASSMARK2);

    return;
}
VOS_UINT8 GMM_Fill_IE_FDD_ClassMark3(
    VOS_UINT8                          *pClassMark3                             /* 填写ClassMark3 IE的首地址   */
)
{
    VOS_UINT8                           aucClassMark3[MS_CLASSMARK3_MAX_LEN];
    VOS_UINT8                           ucLen;

    PS_MEM_SET(aucClassMark3, 0, sizeof(aucClassMark3));

    aucClassMark3[0] = GMM_IEI_CLASSMARK3;

    NAS_MML_Fill_IE_FddClassMark3(&aucClassMark3[1]);

    /* 第一位为长度,加上tag 和 len */
    ucLen = aucClassMark3[1] + 2;
    PS_MEM_CPY(pClassMark3, aucClassMark3, ucLen);

    return ucLen;
}


VOS_UINT8 GMM_Fill_IE_TDD_ClassMark3(
    VOS_UINT8                          *pClassMark3                             /* 填写ClassMark3 IE的首地址   */
)
{
    VOS_UINT8                           aucClassMark3[MS_CLASSMARK3_MAX_LEN];
    VOS_UINT8                           ucLen;

    PS_MEM_SET(aucClassMark3, 0, sizeof(aucClassMark3));

    aucClassMark3[0] = GMM_IEI_CLASSMARK3;

    NAS_MML_Fill_IE_TddClassMark3(&aucClassMark3[1]);

    /* 第一位为长度,加上tag 和 len */
    ucLen = aucClassMark3[1] + 2;
    PS_MEM_CPY(pClassMark3, aucClassMark3, ucLen);

    return ucLen;
}


VOS_VOID  GMM_CALL_FillUmtsSupCodecList(
    GMM_CALL_SUPPORT_CODEC_UNION       *punCodecList
)
{
    NAS_MML_CALL_UMTS_CODEC_TYPE_STRU  *pstUmtsCodecType = VOS_NULL_PTR;
    VOS_UINT32                          i;

    pstUmtsCodecType = NAS_MML_CALL_GetCallUmtsCodecType();

    for ( i = 0 ; i < pstUmtsCodecType->ucCnt ; i++ )
    {
        switch (pstUmtsCodecType->aucUmtsCodec[i])
        {
            case NAS_MML_CALL_UMTS_CODEC_TYPE_AMR :
                punCodecList->stSupportCodec.CodecType_UMTSAMR     = VOS_TRUE;
                break;

            case NAS_MML_CALL_UMTS_CODEC_TYPE_AMR2 :
                punCodecList->stSupportCodec.CodecType_UMTSAMR2    = VOS_TRUE;
                break;

            case NAS_MML_CALL_UMTS_CODEC_TYPE_AMRWB :
                punCodecList->stSupportCodec.CodecType_UMTSAMRWB   = VOS_TRUE;
                break;
            default:
                NAS_WARNING_LOG(WUEPS_PID_GMM, "GMM_CALL_FillUmtsSupCodecList: invalid speech version");
                break;
        }
    }
}


VOS_VOID  GMM_CALL_FillGsmSupCodecList(
    GMM_CALL_SUPPORT_CODEC_UNION       *punCodecList
)
{
    NAS_MML_CALL_GSM_CODEC_TYPE_STRU   *pstGsmCodecType = VOS_NULL_PTR;
    VOS_UINT32                          i;

    pstGsmCodecType = NAS_MML_CALL_GetCallGsmCodeType();

    for ( i = 0 ; i < pstGsmCodecType->ucCodecTypeNum; i++ )
    {
        switch (pstGsmCodecType->aucCodecType[i])
        {
            case NAS_MML_CALL_BC_VAL_SPH_VER_FR_1 :
                punCodecList->stSupportCodec.CodecType_GSMFR        = VOS_TRUE;
                break;

            case NAS_MML_CALL_BC_VAL_SPH_VER_FR_2 :
                punCodecList->stSupportCodec.CodecType_GSMEFR       = VOS_TRUE;
                break;

            case NAS_MML_CALL_BC_VAL_SPH_VER_FR_3 :
                punCodecList->stSupportCodec.CodecType_GSMFRAMR     = VOS_TRUE;
                break;

            case NAS_MML_CALL_BC_VAL_SPH_VER_HR_1 :
                punCodecList->stSupportCodec.CodecType_GSMHR        = VOS_TRUE;
                break;

            case NAS_MML_CALL_BC_VAL_SPH_VER_HR_3 :
                punCodecList->stSupportCodec.CodecType_GSMHRAMR     = VOS_TRUE;
                break;

            case NAS_MML_CALL_BC_VAL_SPH_VER_FR_5 :
                punCodecList->stSupportCodec.CodecType_GSMFRAMRWB   = VOS_TRUE;
                break;
            default:
                NAS_WARNING_LOG(WUEPS_PID_GMM, "GMM_CALL_FillGsmSupCodecList: invalid speech version");
                break;
        }
    }
}


VOS_UINT8  GMM_CALL_FillIeSupCodecList(
    VOS_UINT8                          *pstCodecList                            /* 填写support codec IE的首地址   */
)
{
    GMM_CALL_SUPPORT_CODEC_UNION        unCodec;
    NAS_CC_IE_CODEC_INFO_STRU           astCodecList[2];
    VOS_UINT8                           ucLen;

    /* 设置支持的列表，目前填入了GSM和UMTS支持的codec list,
       00 表示GSM, 04表示UMTS */
    /* 具体赋值的含义参见26.103 */

    PS_MEM_SET(&unCodec, 0x00, sizeof(unCodec));
    astCodecList[0].SysId = 0x04;
    astCodecList[0].LastOctOffsetengthOfBitMap = 0x02;
    GMM_CALL_FillUmtsSupCodecList(&(unCodec));
    astCodecList[0].CodecBitmapL = (VOS_UINT8)(unCodec.usSupportCodec);
    astCodecList[0].CodecBitmapH = (VOS_UINT8)(unCodec.usSupportCodec >> 8);

    /* 填写GSM支持的codec类型 */
    PS_MEM_SET(&unCodec, 0x00, sizeof(unCodec));
    GMM_CALL_FillGsmSupCodecList(&(unCodec));
    astCodecList[1].SysId = 0x00;
    astCodecList[1].LastOctOffsetengthOfBitMap = 0x02;
    astCodecList[1].CodecBitmapL = (VOS_UINT8)(unCodec.usSupportCodec);
    astCodecList[1].CodecBitmapH = (VOS_UINT8)(unCodec.usSupportCodec >> 8);

    pstCodecList[0] = GMM_IEI_SUPPORTED_CODEC_LIST;
    pstCodecList[1] = sizeof(astCodecList);
    PS_MEM_CPY(&pstCodecList[2], astCodecList, sizeof(astCodecList));

    ucLen = sizeof(astCodecList) + 2;
    return ucLen;
}



VOS_VOID Gmm_FillDrxPara(
                     VOS_UINT8 *pAddr                                               /* 填写DRX parameter的首地址                */
                     )
{
    pAddr[0]  = NAS_MML_GetSplitPgCycleCode();                                  /* 填写SPLIT PG CYCLE CODE                  */
    pAddr[1]  = NAS_MML_GetNonDrxTimer();                                       /* 填写non-DRXtimer                         */
    pAddr[1] |= (VOS_UINT8)(NAS_MML_GetSplitOnCcch() << NAS_MML_OCTET_MOVE_THREE_BITS);     /* 填写SPLIT on CCCH                        */
    pAddr[1] |= (VOS_UINT8)(NAS_MML_GetUeUtranPsDrxLen() << NAS_MML_OCTET_MOVE_FOUR_BITS);  /* CN Specific DRX cycle length coefficient */

    return;
}
VOS_UINT8 Gmm_Fill_IE_NetworkCapability(
                                     VOS_UINT8 *pAddr                           /* 填写NetworkCapability IE的首地址                */
                                    )
{
    VOS_UINT8                           ucLen;
    VOS_INT8                            cVersion;
    NAS_MML_MS_CAPACILITY_INFO_STRU    *pstMsCapability = VOS_NULL_PTR;

#if (FEATURE_ON == FEATURE_LTE)
    NAS_MML_MS_MODE_ENUM_UINT8          enMsMode;
    NAS_MML_SIM_TYPE_ENUM_UINT8         enSimType;

    VOS_UINT32                          ulLteCapDisablFlg;
    VOS_UINT8                           ucIsrSupport;

    ucIsrSupport = NAS_MML_GetIsrSupportFlg();

    enMsMode = NAS_MML_GetMsMode();
#endif

    pstMsCapability = NAS_MML_GetMsCapability();


    cVersion = NAS_Common_Get_Supported_3GPP_Version(MM_COM_SRVDOMAIN_PS);


    ucLen = NAS_MML_DEFAULT_NETWORKCAPABILITY_LEN;
    Gmm_MemCpy (pAddr + 1, pstMsCapability->stMsNetworkCapability.aucNetworkCapability, ucLen);

    /* Update Revision Level Indicator IE
 */
    if (PS_PTL_VER_PRE_R99 == cVersion)
    {
        pAddr[1] &= 0xfe;
    }
    else
    {
        pAddr[1] |= 0x01;
    }

    if (PS_PTL_VER_R7 <= cVersion)
    {
        /* 目前NV项中Network cap填写长度为2,如果协议版本为R7或R7之后,其长度为3 */
        ucLen++;

        if (pstMsCapability->stMsNetworkCapability.ucNetworkCapabilityLen > 2)
        {
            pAddr[3] = pstMsCapability->stMsNetworkCapability.aucNetworkCapability[2];
        }
        else
        {
            pAddr[3] = 0x0;
        }

#if (FEATURE_ON == FEATURE_LTE)


        /* 当前SIM卡时,不能上报支持EPC的能力 */
        enSimType = NAS_MML_GetSimType();

        if (NAS_MML_SIM_TYPE_SIM == enSimType)
        {
            pAddr[3] &= 0xFB;
        }



        /* 如果支持LTE且模式为CS_PS时认为支持EMM Combined procedures capability */
        if ((VOS_TRUE == NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_LTE))
         && (NAS_MML_MS_MODE_PS_CS == enMsMode))
        {
            pAddr[3] |= 0x20;
        }
        else
#endif
        {
            pAddr[3] &= 0xDF;
        }

        /* 如果支持LTE,则认为支持ISR, ISR support对应bit填1 */
#if (FEATURE_ON == FEATURE_LTE)
        /* L如果已经被disable，无需上报ISR能力 */
        ulLteCapDisablFlg = NAS_MML_IsLteCapabilityDisabled(NAS_MML_GetLteCapabilityStatus(),
                                                            NAS_MML_GetDisableLteRoamFlg());

        /* 不支持LTE时，不需要上报SRVCC能力 */
        if (VOS_FALSE == NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_LTE)
        || (VOS_TRUE  == ulLteCapDisablFlg))
        {
            pAddr[3] &= 0xF7;


            /* 不支持PS INTER RAT HO FORM GERAN TO E-UTRAN */
            pAddr[3] &= 0xBF;

        }

        if ((VOS_TRUE == NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_LTE)
         && (VOS_FALSE == ulLteCapDisablFlg))
         && (VOS_TRUE == ucIsrSupport))
        {
            pAddr[3] |= 0x10;
        }
        else
#endif
        {
            pAddr[3] &= 0xEF;
        }
    }

    pAddr[0] = ucLen;

    /* 需要返回MS Net Cap IE(LV组成)总长度，因此ucLen需要增1 */
    ucLen++;

    return ucLen;
}

VOS_UINT8 Gmm_Fill_IE_RadioAccessCapability(VOS_UINT8 *pAddr)
{
    VOS_UINT8                                               ucLen;
    VOS_UINT8                                              *pucRadioCpb            = VOS_NULL_PTR;
#if (FEATURE_ON == FEATURE_LTE)
	VOS_UINT32                                              ulRst;
    NAS_GAS_MS_CAP_TYPE_ENUM_UINT8                          enMsCapType;
    GRRMM_MS_RADIO_ACCESS_CAPABILITIES_IND_STRU            *pstMsRadioAccessCapInd = VOS_NULL_PTR;
    NAS_MML_NET_RAT_TYPE_ENUM_UINT8                         enCurNetRatType;
    RRC_PLMN_ID_STRU                                        stRrmmPlmnId;
#endif

#if (FEATURE_ON == FEATURE_LTE)
    enCurNetRatType        = NAS_MML_GetCurrNetRatType();

    pstMsRadioAccessCapInd = (GRRMM_MS_RADIO_ACCESS_CAPABILITIES_IND_STRU*)PS_MEM_ALLOC(
                                    WUEPS_PID_MMC, sizeof(GRRMM_MS_RADIO_ACCESS_CAPABILITIES_IND_STRU));
#endif

    if (NAS_UTRANCTRL_UTRAN_MODE_TDD == NAS_UTRANCTRL_GetCurrUtranMode())
    {
#if (FEATURE_ON == FEATURE_LTE)
        enMsCapType = NAS_GAS_MS_CAP_TYPE_UTRAN_TDD_RAC;
#endif
        pucRadioCpb = g_GmmGlobalCtrl.UeInfo.aucTdsRadioCpb;
    }
    else
    {
#if (FEATURE_ON == FEATURE_LTE)
        enMsCapType = NAS_GAS_MS_CAP_TYPE_UTRAN_FDD_RAC;
#endif
        pucRadioCpb = g_GmmGlobalCtrl.UeInfo.aucRadioCpb;
    }

#if (FEATURE_ON == FEATURE_LTE)
    /* 当前NV打开，申请内存成功且当前模不是GSM */
    if ( (VOS_TRUE == NAS_MML_GetIsDelFddBandSwitchOnFlg())
      && (VOS_NULL_PTR != pstMsRadioAccessCapInd)
      && (NAS_MML_NET_RAT_TYPE_GSM != enCurNetRatType) )
    {

        PS_MEM_SET(pstMsRadioAccessCapInd, 0x00, sizeof(GRRMM_MS_RADIO_ACCESS_CAPABILITIES_IND_STRU));

        stRrmmPlmnId.ulMcc = NAS_MML_GetCurrCampPlmnId()->ulMcc;
        stRrmmPlmnId.ulMnc = NAS_MML_GetCurrCampPlmnId()->ulMnc;

        /* 调用GAS接口获取UE RADIO能力 */
        ulRst = GASGCOM_GetMsCapability(&stRrmmPlmnId, enMsCapType,
                                                sizeof(GRRMM_MS_RADIO_ACCESS_CAPABILITIES_IND_STRU),
                                                (VOS_UINT8*)pstMsRadioAccessCapInd);

        /* 可维可测 */
        NAS_GMM_LogGasGmmRadioAccessCapability(ulRst, enMsCapType,
                                                sizeof(pstMsRadioAccessCapInd->aucData),
                                                pstMsRadioAccessCapInd->aucData);

        if ( VOS_TRUE == ulRst )
        {
            pucRadioCpb = pstMsRadioAccessCapInd->aucData;
        }
    }
#endif

    ucLen = pucRadioCpb[0] + 1;
    Gmm_MemCpy (pAddr, pucRadioCpb, ucLen);

#if (FEATURE_ON == FEATURE_LTE)
    if ( VOS_NULL_PTR != pstMsRadioAccessCapInd )
    {
        PS_MEM_FREE(WUEPS_PID_MMC, pstMsRadioAccessCapInd);
    }
#endif

    return ucLen;
}

#if (FEATURE_ON == FEATURE_LTE)
VOS_UINT8 NAS_Gmm_FillPtmsiIeWhenGUL(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                          *pucImsi             = VOS_NULL_PTR;
    VOS_UINT8                          *pucPtmsiAddr        = VOS_NULL_PTR;
    VOS_UINT8                          *pucMappedPtmsiAddr  = VOS_NULL_PTR;
    VOS_UINT8                           ucNumber;
    VOS_UINT8                           ucUeIdMask;
    VOS_UINT32                          ulGetGutiRst;
    VOS_UINT32                          ulLocalPtmsiFlg;
    NAS_MML_TIN_TYPE_ENUM_UINT8         enTinType;

    VOS_UINT32                          ulIsSupportLteCapability;

    /* 取得本地维护的IMSI的地址 */
    pucImsi             = NAS_MML_GetSimImsi();

    /* 取得存储PTMSI数据的地址 */
    pucPtmsiAddr        = NAS_MML_GetUeIdPtmsi();
    pucMappedPtmsiAddr  = NAS_GMM_GetMappedPtmsiAddr();

    /* 长度默认置为PTMSI的长度6 */
    ucNumber            = GMM_MSG_LEN_LV_PTMSI;

    /* GU本地的PTMSI是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    ucUeIdMask          = NAS_GMM_GetUeIdMask();
    ulLocalPtmsiFlg     = VOS_FALSE;

    if ( GMM_UEID_P_TMSI == ( ucUeIdMask & GMM_UEID_P_TMSI) )
    {
        ulLocalPtmsiFlg = VOS_TRUE;
    }


    /* reference to 24.008 4.7.3.1.1 */

    ulIsSupportLteCapability = NAS_MML_IsSupportLteCapability();

    /* syscfg设置不支持L或者L 已经disable则无需从guti中获取ptimsi */
    if (VOS_FALSE == ulIsSupportLteCapability)
    {
        if (VOS_TRUE == ulLocalPtmsiFlg)
        {
            NAS_GMM_Fill_IE_PtmsiFromSrc(pucAddr, pucPtmsiAddr);
        }
        else
        {
            PS_MEM_CPY( pucAddr, pucImsi, pucImsi[0]+1 );
            ucNumber = pucImsi[0]+1;
        }

        return ucNumber;
    }

    ulGetGutiRst            = NAS_GMM_MapPtmsiFromGUTI();
    enTinType               = NAS_MML_GetTinType();

    /* 4.7.3.1.1 GPRS attach procedure initiation (GMM_MAPPED_INFO_FROM_EPS_STRU)
       If the MS supports S1 mode, the MS shall handle the P-TMSI or IMSI IE as follows
    */
    if ( (NAS_MML_TIN_TYPE_GUTI == enTinType) && (VOS_OK == ulGetGutiRst) )
    {
        NAS_GMM_Fill_IE_PtmsiFromSrc(pucAddr, pucMappedPtmsiAddr);
    }
    else if ( ((NAS_MML_TIN_TYPE_RAT_RELATED_TMSI == enTinType) || (NAS_MML_TIN_TYPE_PTMSI  == enTinType))
           && (VOS_TRUE == ulLocalPtmsiFlg) )
    {
        NAS_GMM_Fill_IE_PtmsiFromSrc(pucAddr, pucPtmsiAddr);
    }
    /*If the TIN is deleted,and the MS doesnot holds a valid P-TMSI and a RAI,and the MS holds a valid GUTI, the MS shall map the GUTI into the P-TMSI  */
    else if ( (NAS_MML_TIN_TYPE_INVALID  == enTinType)
           && (VOS_FALSE            == ulLocalPtmsiFlg)
           && (VOS_OK               == ulGetGutiRst) )
    {
        NAS_GMM_Fill_IE_PtmsiFromSrc(pucAddr, pucMappedPtmsiAddr);
    }
    else if ( (NAS_MML_TIN_TYPE_INVALID == enTinType) && (VOS_TRUE == ulLocalPtmsiFlg) )
    {
        NAS_GMM_Fill_IE_PtmsiFromSrc(pucAddr, pucPtmsiAddr);
    }
    else
    {
        PS_MEM_CPY( pucAddr, pucImsi, pucImsi[0]+1 );

        /* 取得IMSI的长度 */
        ucNumber = pucImsi[0]+1;
    }

    return ucNumber;

}
#endif


VOS_UINT8 NAS_Gmm_FillPtmsiIeForSnd(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                           ucNumber;
#if (FEATURE_ON != FEATURE_LTE)
    VOS_UINT8                          *pucPtmsiAddr = VOS_NULL_PTR;
    VOS_UINT8                           ucUeIdMask;
    VOS_UINT8                          *pucImsi      = VOS_NULL_PTR;
#endif

    /* 长度默认置为PTMSI的长度6 */
    ucNumber           = GMM_MSG_LEN_LV_PTMSI;

#if (FEATURE_ON == FEATURE_LTE)

    /* 支持L模时的PTMSI IE填充    */
    ucNumber = NAS_Gmm_FillPtmsiIeWhenGUL(pucAddr);

    return ucNumber;

#else


    /* 取得存储PTMSI数据的地址 */
    pucPtmsiAddr       = NAS_MML_GetUeIdPtmsi();

    /* GU本地的PTMSI是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    ucUeIdMask         = NAS_GMM_GetUeIdMask();
    if ( GMM_UEID_P_TMSI == ( ucUeIdMask & GMM_UEID_P_TMSI) )
    {
        NAS_GMM_Fill_IE_PtmsiFromSrc(pucAddr, pucPtmsiAddr);
    }
    else
    {                                                                           /* IMSI                                     */
        /* 取得本地维护的IMSI的地址 */
        pucImsi = NAS_MML_GetSimImsi();
        PS_MEM_CPY( pucAddr, pucImsi, pucImsi[0]+1 );

        /* 取得IMSI的长度 */
        ucNumber = pucImsi[0]+1;
    }

    return ucNumber;
#endif

}
VOS_UINT8 NAS_Gmm_FillPtmsiSignatureIeForSnd(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                          *pucPtmsiSign       = VOS_NULL_PTR;
    VOS_UINT8                           ucNumber;
    VOS_UINT8                           ucUeIdMask;
    VOS_UINT32                          ulLocalPtmsiFlg;
    VOS_BOOL                            bLocalPtmsiSignFlg;

#if (FEATURE_ON == FEATURE_LTE)
    VOS_UINT8                          *pucMappedPtmsiSign = VOS_NULL_PTR;
    NAS_MML_TIN_TYPE_ENUM_UINT8         enTinType;
    VOS_UINT32                          ulGetGutiRst;

    VOS_UINT32                          ulIsSupportLteCapability;

    VOS_UINT8                           ucLDisabledRauUseLInfoFlag;

    ucLDisabledRauUseLInfoFlag = NAS_MML_GetLteDisabledRauUseLteInfoFlag();
#endif

    /* 长度默认置为PTMSI Signatrue的长度4*/
    ucNumber  = GMM_MSG_LEN_PTMSI_SIGNATURE;

    ucUeIdMask          = NAS_GMM_GetUeIdMask();

    /* GU本地的PTMSI是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    ulLocalPtmsiFlg     = VOS_FALSE;
    if ( GMM_UEID_P_TMSI == ( ucUeIdMask & GMM_UEID_P_TMSI) )
    {
        ulLocalPtmsiFlg = VOS_TRUE;
    }
    /* GU本地的PTMSI Signatrue 是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    bLocalPtmsiSignFlg  = VOS_FALSE;
    if ( GMM_UEID_P_TMSI_SIGNATURE == ( ucUeIdMask & GMM_UEID_P_TMSI_SIGNATURE) )
    {
        bLocalPtmsiSignFlg = VOS_TRUE;
    }

    /* 获取存储PTMSI signature的数据地址 */
    pucPtmsiSign        = NAS_MML_GetUeIdPtmsiSignature();

#if (FEATURE_ON == FEATURE_LTE)

    /* reference to 24.008 4.7.3.1.1 and 4.7.5.1.1 */
    ulIsSupportLteCapability = NAS_MML_IsSupportLteCapability();

    /* syscfg设置不支持L或者L 已经disable attach 则无需从guti中获取ptimsi signature, rau nv 控制是否需要从guti获取ptmsi signature */
    if (VOS_FALSE == ulIsSupportLteCapability)
    {
        if (!((GMM_RAU_COMBINED == (g_GmmGlobalCtrl.ucSpecProc & GMM_RAU_COMBINED))
            && (VOS_TRUE == ucLDisabledRauUseLInfoFlag)))
        {
            if ( (VOS_TRUE == ulLocalPtmsiFlg)
              && (VOS_TRUE == bLocalPtmsiSignFlg) )
            {
                NAS_GMM_Fill_IE_PtmsiSignFromSrc(pucAddr, pucPtmsiSign);
            }
            else
            {
                ucNumber = 0;
            }

            return ucNumber;
        }
    }


    /* 4.7.3.1.1 GPRS attach procedure initiation (GMM_MAPPED_INFO_FROM_EPS_STRU)
       If the MS supports S1 mode, the MS shall handle the P-TMSI or IMSI IE as follows
    */
    ulGetGutiRst        = NAS_GMM_MapPtmsiSignFromGUTIAndNasToken();
    enTinType           = NAS_MML_GetTinType();
    pucMappedPtmsiSign  = NAS_GMM_GetMappedPtmsiSignAddr();

    if ( ((NAS_MML_TIN_TYPE_GUTI == enTinType)
        && (VOS_OK  == ulGetGutiRst))
      || ((NAS_MML_TIN_TYPE_INVALID == enTinType)
       && (VOS_OK == ulGetGutiRst)
       && (VOS_FALSE == ulLocalPtmsiFlg)) )
    {
        NAS_GMM_Fill_IE_PtmsiSignFromSrc(pucAddr, pucMappedPtmsiSign);
    }
    else if ( ( (NAS_MML_TIN_TYPE_PTMSI == enTinType)
              || (NAS_MML_TIN_TYPE_RAT_RELATED_TMSI == enTinType)
              || (NAS_MML_TIN_TYPE_INVALID == enTinType) )
           && (VOS_TRUE  == ulLocalPtmsiFlg )
           && ( VOS_TRUE == bLocalPtmsiSignFlg ))
    {
        NAS_GMM_Fill_IE_PtmsiSignFromSrc(pucAddr, pucPtmsiSign);
    }
    else
    {
        ucNumber = 0;
    }

    return ucNumber;

#else
    if ( (VOS_TRUE == ulLocalPtmsiFlg)
      && (VOS_TRUE == bLocalPtmsiSignFlg) )
    {
        NAS_GMM_Fill_IE_PtmsiSignFromSrc(pucAddr, pucPtmsiSign);
    }
    else
    {
        ucNumber = 0;
    }

    return ucNumber;
#endif

}


VOS_UINT8 NAS_Gmm_FillRaiForSnd(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                           ucNumber;
#if (FEATURE_ON == FEATURE_LTE)
    VOS_UINT8                           ucUeIdMask;
    VOS_UINT32                          ulLocalPtmsiFlg;
    NAS_MML_TIN_TYPE_ENUM_UINT8         enTinType;
    GMM_RAI_STRU                       *pstMappedRai = VOS_NULL_PTR;
    VOS_UINT32                          ulGetGutiRst;

    VOS_UINT32                          ulIsSupportLteCapability;


    VOS_UINT8                           ucLDisabledRauUseLInfoFlag;
#endif

    NAS_MML_RAI_STRU                       *pstLastSuccRai;
    GMM_RAI_STRU                            stGmmRai;

    pstLastSuccRai    = NAS_MML_GetPsLastSuccRai();
    NAS_GMM_ConvertPlmnIdToGmmFormat(&(pstLastSuccRai->stLai.stPlmnId), &stGmmRai.Lai.PlmnId);

    stGmmRai.ucRac          = pstLastSuccRai->ucRac;
    stGmmRai.Lai.aucLac[0]  = pstLastSuccRai->stLai.aucLac[0];
    stGmmRai.Lai.aucLac[1]  = pstLastSuccRai->stLai.aucLac[1];

    /* 长度默认置为RAI的长度6 */
    ucNumber  = GMM_MSG_LEN_OLD_RAI;

#if (FEATURE_ON == FEATURE_LTE)
    ucLDisabledRauUseLInfoFlag = NAS_MML_GetLteDisabledRauUseLteInfoFlag();

    /* reference to 24.008 4.7.3.1.1 and 4.7.5.1.1 */
    ulIsSupportLteCapability = NAS_MML_IsSupportLteCapability();

    /* syscfg设置不支持L或者L 已经disable attach无需从guti中获取ptimsi,rau nv项控制是否从guti中获取 */
    if (VOS_FALSE == ulIsSupportLteCapability)
    {
        if (!((GMM_RAU_COMBINED == (g_GmmGlobalCtrl.ucSpecProc & GMM_RAU_COMBINED))
            && (VOS_TRUE == ucLDisabledRauUseLInfoFlag)))
        {
            NAS_GMM_Fill_IE_RaiFromSrc(pucAddr, &stGmmRai);

            return ucNumber;
        }
    }



    /* GU本地的PTMSI是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    ulLocalPtmsiFlg     = VOS_FALSE;
    ucUeIdMask         = NAS_GMM_GetUeIdMask();
    if ( GMM_UEID_P_TMSI == (ucUeIdMask & GMM_UEID_P_TMSI) )
    {
        ulLocalPtmsiFlg = VOS_TRUE;
    }

    ulGetGutiRst            = NAS_GMM_MapRaiFromGUTI();
    enTinType               = NAS_MML_GetTinType();
    pstMappedRai            = NAS_GMM_GetMappedRaiAddr();

    /* 4.7.3.1.1 GPRS attach procedure initiation */
    if ( ((NAS_MML_TIN_TYPE_GUTI  == enTinType)
        && (VOS_OK  == ulGetGutiRst))
      || ((NAS_MML_TIN_TYPE_INVALID == enTinType)
        && (VOS_OK  == ulGetGutiRst)
        && (VOS_FALSE == ulLocalPtmsiFlg)) )
    {
        NAS_GMM_Fill_IE_RaiFromSrc(pucAddr, pstMappedRai);
        return ucNumber;
    }

    /* If the TIN indicates "P-TMSI" or "RAT related TMSI" and the MS holds a valid P-TMSI and a RAI,
    the MS shall indicate the P-TMSI in the P-TMSI or IMSI IE and the RAI in the Old routing area identification IE.  */
    if ( ((NAS_MML_TIN_TYPE_PTMSI == enTinType) || (NAS_MML_TIN_TYPE_RAT_RELATED_TMSI == enTinType))
      && (VOS_TRUE == ulLocalPtmsiFlg) )
    {
        NAS_GMM_Fill_IE_RaiFromSrc(pucAddr, &stGmmRai);
        return ucNumber;
    }
#endif

    NAS_GMM_Fill_IE_RaiFromSrc(pucAddr, &stGmmRai);

    return ucNumber;
}


#if (FEATURE_ON == FEATURE_LTE)


VOS_UINT8 NAS_Gmm_Fill_IE_AdditionalOldRAI(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                           ucNumber;
    VOS_UINT32                          ulGetGutiRst;
    NAS_MML_TIN_TYPE_ENUM_UINT8         enTinType;
    NAS_LMM_INFO_STRU                   stLmmInfo;
    VOS_UINT32                          ulLocalPtmsiFlg;

    NAS_MML_RAI_STRU                       *pstLastSuccRai;
    GMM_RAI_STRU                            stGmmRai;

    pstLastSuccRai    = NAS_MML_GetPsLastSuccRai();
    NAS_GMM_ConvertPlmnIdToGmmFormat(&(pstLastSuccRai->stLai.stPlmnId), &stGmmRai.Lai.PlmnId);

    stGmmRai.ucRac          = pstLastSuccRai->ucRac;
    stGmmRai.Lai.aucLac[0]  = pstLastSuccRai->stLai.aucLac[0];
    stGmmRai.Lai.aucLac[1]  = pstLastSuccRai->stLai.aucLac[1];

    /* 分别获取存储GU的RAI以及存储从L模映射来的RAI地址 */

    /* GU本地的PTMSI是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    ulLocalPtmsiFlg     = VOS_FALSE;
    if ( GMM_UEID_P_TMSI == ( NAS_GMM_GetUeIdMask() & GMM_UEID_P_TMSI) )
    {
        ulLocalPtmsiFlg = VOS_TRUE;
    }

    /* 获取GUTI是否有效 */
    ulGetGutiRst        = NAS_GMM_GetLteInfo( NAS_LMM_GUTI, &stLmmInfo );

    enTinType           = NAS_MML_GetTinType();
    if ( (NAS_MML_TIN_TYPE_GUTI == enTinType)
      && (VOS_TRUE              == ulGetGutiRst)
      && (VOS_TRUE              == ulLocalPtmsiFlg) )
    {
        NAS_GMM_Fill_IE_AdditionalOldRaiFromSrc(pucAddr, &stGmmRai);

        /* 填充的长度为8 */
        ucNumber  = GMM_MSG_LEN_ADDITIONAL_OLD_RAI;
    }
    else
    {
        ucNumber = 0;
    }

    return ucNumber;

}



VOS_UINT8 NAS_Gmm_Fill_IE_AdditionalMobileIdentity(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                          *pucPtmsiAddr = VOS_NULL_PTR;
    VOS_UINT8                           ucNumber;
    VOS_UINT32                          ulGetGutiRst;
    VOS_UINT32                          ulLocalPtmsiFlg;
    MMC_LMM_TIN_TYPE_ENUM_UINT32        enTinType;
    NAS_LMM_INFO_STRU                   stLmmInfo;

    /* Additional mobile identity 长度默认置为 7 */
    ucNumber            = GMM_MSG_LEN_ADDITIONAL_MOBILE_IDENTITY;

    /* 取得存储PTMSI数据的地址 */
    pucPtmsiAddr = NAS_MML_GetUeIdPtmsi();

    /* GU本地的PTMSI是否有效，VOS_TRUE:有效;VOS_FALSE:无效; */
    ulLocalPtmsiFlg     = VOS_FALSE;
    if ( GMM_UEID_P_TMSI == ( NAS_GMM_GetUeIdMask() & GMM_UEID_P_TMSI) )
    {
        ulLocalPtmsiFlg = VOS_TRUE;
    }

    /* 获取GUTI是否有效  */
    ulGetGutiRst        = NAS_GMM_GetLteInfo( NAS_LMM_GUTI, &stLmmInfo );

    enTinType           = NAS_MML_GetTinType();

    /* 4.7.3.1.1 GPRS attach procedure initiation  */
    if ( (NAS_MML_TIN_TYPE_GUTI == enTinType)
      && (VOS_TRUE == ulGetGutiRst)
      && (VOS_TRUE == ulLocalPtmsiFlg) )
    {
        /* 填写IEI 为0x1A */
        pucAddr[0] = GMM_IEI_ADDITIONAL_MOBILE_IDENTITY_VALUE;

        /* 填写PTMSI */
        NAS_GMM_Fill_IE_PtmsiFromSrc(&pucAddr[1], pucPtmsiAddr);
    }
    else
    {
        ucNumber = 0;
    }

    return ucNumber;

}
VOS_UINT8  NAS_GMM_Fill_IE_UeNetwrokCapacity(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                           ucMsgLen;

    NAS_MML_UE_NETWORK_CAPABILITY_STRU *pstUeNetworkCap = VOS_NULL_PTR;

    pstUeNetworkCap = NAS_MML_GetUeNetWorkCapability();

    ucMsgLen   = 0x0;

    if ( (0 == pstUeNetworkCap->ucUeNetCapLen)
      || (pstUeNetworkCap->ucUeNetCapLen > NAS_MML_MAX_UE_NETWORK_CAPABILITY_LEN))
    {
        /* 长度为0或长度非法时不需要填充UeNetwrokCapacity */
        return ucMsgLen;
    }

    /* 长度 */
    ucMsgLen = pstUeNetworkCap->ucUeNetCapLen + 2;

    /* 填写IEI 为0x58 */
    pucAddr[0] = GMM_IEI_UE_NETWORK_CAPACITY_VALUE;

    pucAddr[1] = pstUeNetworkCap->ucUeNetCapLen;

    /* 从MML获取NV中读取的UE NETWORK CAPACITY,填充到pucAddr[2] */
    PS_MEM_CPY(&(pucAddr[2]), pstUeNetworkCap->aucUeNetCap, pstUeNetworkCap->ucUeNetCapLen);


    return ucMsgLen;
}
VOS_UINT8  NAS_GMM_Fill_IE_VoiceDomainPreferenceAndUeUsageSetting(
    VOS_UINT8                          *pucAddr
)
{
    VOS_UINT8                                               ucMsgLen;
    VOS_UINT8                                               ucIndex;
    NAS_MML_VOICE_DOMAIN_PREFERENCE_ENUM_UINT8              enVoiceDomainPreference;
    NAS_MML_LTE_UE_USAGE_SETTING_ENUM_UINT8                 enLteUeUsageSetting;

    /* 获得VoiceDomain信息 */
    enVoiceDomainPreference = NAS_MML_GetVoiceDomainPreference();

    /* 获得UsageSetting信息 */
    enLteUeUsageSetting     = NAS_MML_GetLteUeUsageSetting();

    /* 若获得VoiceDomain, UsageSetting的信息异常，则不添加 */
    if ( (NAS_MML_VOICE_DOMAIN_PREFER_BUTT == enVoiceDomainPreference)
      || (NAS_MML_LTE_UE_USAGE_SETTIN_BUTT == enLteUeUsageSetting) )
    {
        NAS_WARNING_LOG(WUEPS_PID_GMM, "NAS_GMM_Fill_IE_VoiceDomainPreferenceAndUeUsageSetting:VOICE DOMAIN INVALID");
        ucMsgLen = 0;
        return ucMsgLen;
    }

    /* 初始化填充IE的索引  */
    ucIndex = 0;

    /* 初始化消息长度为3个字节 */
    ucMsgLen = GMM_MSG_LEN_TLV_VOICE_DOMAIN_PREFERANCE_AND_UE_USAGE_SETTING_LEN;

    /* 填写IEI为0x5D */
    pucAddr[ucIndex] = GMM_IEI_VOICE_DOMAIN_PREFERANCE_AND_UE_USAGE_SETTING;
    ucIndex++;

    /* 填写长度项为1 */
    pucAddr[ucIndex] = GMM_MSG_LEN_V_VOICE_DOMAIN_PREFERANCE_AND_UE_USAGE_SETTING_LEN;
    ucIndex++;

    /* 第三个字节初始化为0 */
    pucAddr[ucIndex] = 0;

    /* 第三个字节BIT位1，BIT位2填写的是VoiceDomainPreference */
    pucAddr[ucIndex] |= enVoiceDomainPreference & NAS_MML_OCTET_LOW_TWO_BITS;

    /* 第三个字节BIT位3填写的是LteUeUsageSetting */
    pucAddr[ucIndex] |= (VOS_UINT8)(enLteUeUsageSetting << NAS_MML_OCTET_MOVE_TWO_BITS);

    return ucMsgLen;
}


#endif





NAS_MSG_STRU *Gmm_AttachRequestMsgMake(VOS_VOID)
{
    NAS_MSG_STRU                               *pAttachRequest = VOS_NULL_PTR;          /* 指向NAS_MSG_STRU结构的指针变量           */
    VOS_UINT8                                   ucNumber;                               /* 填写IE时用来计数                         */
    VOS_UINT8                                   ucMsgLen;
    VOS_UINT8                                  *pucImsi = VOS_NULL_PTR;
    NAS_MML_ROUTING_UPDATE_STATUS_ENUM_UINT8    enPsUpdateStatus;

    VOS_UINT8                                  *pucNetworkCapabilityBuf = VOS_NULL_PTR;

#if (FEATURE_ON == FEATURE_LTE)
    NAS_LMM_INFO_STRU                   stLmmInfo;
    VOS_INT8                                    cVersion;
    VOS_UINT32                                  ulSupportLteFlg;

    VOS_UINT32                                  ulIsLteCapabilityDisabled;

    ulSupportLteFlg = NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_LTE);
#endif

    /* 给填充消息分配的内存字节长度，请确保足够合理分配，目前最多能用到153个字节 */
    ucMsgLen                            = 160;
    ucNumber                            = 2;

    pAttachRequest = (NAS_MSG_STRU*)
        PS_MEM_ALLOC(WUEPS_PID_GMM, ((sizeof(NAS_MSG_STRU) + ucMsgLen) - 4));     /* 申请内存空间                             */

    if (VOS_NULL_PTR == pAttachRequest)
    {
        NAS_ERROR_LOG(WUEPS_PID_GMM, "Gmm_AttachRequestMsgMake:ERROR: MALLOC FAIL!");
        return pAttachRequest;                                                  /* 返回                                     */
    }

    pAttachRequest->aucNasMsg[0] = GMM_PD_GMM;                                  /* 填写Protocol discriminator               */
    pAttachRequest->aucNasMsg[1] = GMM_MSG_ATTACH_REQUEST;                      /* 填写Attach request message identity      */

    /*填写网络能力*/

    pucNetworkCapabilityBuf = &pAttachRequest->aucNasMsg[2];
    ucNumber += Gmm_Fill_IE_NetworkCapability(pucNetworkCapabilityBuf);

    /*填写Attach type and GPRS ciphering key sequence number*/
    ucNumber += NAS_Gmm_Fill_IE_AttachTypeAndCksn(&pAttachRequest->aucNasMsg[ucNumber]);

    Gmm_FillDrxPara(&pAttachRequest->aucNasMsg[ucNumber]);                      /* DRX parameter                            */
    ucNumber += 2;                                                              /* DRX parameter长度为2                     */

    g_GmmGlobalCtrl.UeInfo.enLatestAttachOrRauContainDrx = NAS_MML_GU_PS_REG_CONTAIN_DRX_PARA;

    /* 填写 P-TMSI or imsi IE */
    /* 状态为DEREGISTERED_NORMAL_SERVICE,U2时 填充使用IMSI    */
    enPsUpdateStatus = NAS_MML_GetPsUpdateStatus();
    if ( (GMM_DEREGISTERED_NORMAL_SERVICE == NAS_GMM_GetGmmState())
      && (NAS_MML_ROUTING_UPDATE_STATUS_NOT_UPDATED == enPsUpdateStatus) )
    {
        /* 取得本地维护的IMSI的地址 */
        pucImsi            = NAS_MML_GetSimImsi();
        Gmm_MemCpy(&pAttachRequest->aucNasMsg[ucNumber],
                   &pucImsi[0],
                   pucImsi[0] + 1);
        ucNumber += pucImsi[0] + 1;
    }
    else
    {
        ucNumber += NAS_Gmm_FillPtmsiIeForSnd(&pAttachRequest->aucNasMsg[ucNumber]);
    }

    /* 填写 RAI IE */
    ucNumber += NAS_Gmm_FillRaiForSnd(&pAttachRequest->aucNasMsg[ucNumber]);                    /* 填写RAI                                  */

    /* 无线接入能力填写 */
#if (FEATURE_ON == FEATURE_LTE)
    g_GmmGlobalCtrl.UeInfo.ucMsRadioCapSupportLteFromRegReq = g_GmmGlobalCtrl.UeInfo.ucMsRadioCapSupportLteFromAs;
#endif

    ucNumber += Gmm_Fill_IE_RadioAccessCapability(&pAttachRequest->aucNasMsg[ucNumber]);

    /* 填写 P-TMSI Signature IE */
    ucNumber += NAS_Gmm_FillPtmsiSignatureIeForSnd(&pAttachRequest->aucNasMsg[ucNumber]);

    /*==>A32D11726*/
    GMM_FillReadyTimerParaIe(&pAttachRequest->aucNasMsg[ucNumber]);
    ucNumber += 2;
    /*<==A32D11726*/

    if ((VOS_FALSE  == NAS_MML_IsTmsiValid())
     && ( (GMM_ATTACH_WHILE_IMSI_ATTACHED == NAS_GMM_GetSpecProc())
        || (GMM_ATTACH_COMBINED          == NAS_GMM_GetSpecProc())))
    {                                                                           /* ATTACH流程为combined并且TMSI无效         */

        /* 填写TMSI status */
        pAttachRequest->aucNasMsg[ucNumber] = GMM_IEI_TMSI_STATUS_NO_VALID;     /* [false alarm]: 数组已动态分配 */
        ucNumber += 1;
    }

#if (FEATURE_ON == FEATURE_LTE)

    cVersion = NAS_Common_Get_Supported_3GPP_Version(MM_COM_SRVDOMAIN_PS);
    if (cVersion >= PS_PTL_VER_R9)
    {
        if (VOS_TRUE == NAS_MML_GetSupportSrvccFlg())
        {
            /* 填写classmark2 */
            GMM_Fill_IE_ClassMark2(&pAttachRequest->aucNasMsg[ucNumber]);
            ucNumber += GMM_MSG_LEN_CLASSMARK2;

            /* 填写classmark3 */
            if (NAS_UTRANCTRL_UTRAN_MODE_FDD == NAS_UTRANCTRL_GetCurrUtranMode())
            {
                ucNumber += GMM_Fill_IE_FDD_ClassMark3(&pAttachRequest->aucNasMsg[ucNumber]);
            }
            else if (NAS_UTRANCTRL_UTRAN_MODE_TDD == NAS_UTRANCTRL_GetCurrUtranMode())
            {
                ucNumber += GMM_Fill_IE_TDD_ClassMark3(&pAttachRequest->aucNasMsg[ucNumber]);
            }
            else
            {
            }

            /* 填写support code list */
            ucNumber += GMM_CALL_FillIeSupCodecList(&pAttachRequest->aucNasMsg[ucNumber]);

        }

        /* An MS supporting S1 mode shall include this IE to indicate its capabilities to the network */
        if (VOS_TRUE == NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_LTE))
        {
            /* 填写UE network Capacility */
            ulIsLteCapabilityDisabled = NAS_MML_IsLteCapabilityDisabled(NAS_MML_GetLteCapabilityStatus(),
                                                                NAS_MML_GetDisableLteRoamFlg());

            if (VOS_FALSE == ulIsLteCapabilityDisabled)
            {
                ucNumber += NAS_GMM_Fill_IE_UeNetwrokCapacity(&pAttachRequest->aucNasMsg[ucNumber]);
            }



            /* 填写Additional mobile identity */
            ucNumber += NAS_Gmm_Fill_IE_AdditionalMobileIdentity(&pAttachRequest->aucNasMsg[ucNumber]);

            /* 填写Additional old routing area identification */
            ucNumber += NAS_Gmm_Fill_IE_AdditionalOldRAI(&pAttachRequest->aucNasMsg[ucNumber]);

        }

        /* 记录本次attach时voice domain，后续voice domain发生变化时，需要做RAU */
        g_GmmGlobalCtrl.UeInfo.enVoiceDomainFromRegReq  = NAS_MML_GetVoiceDomainPreference();

        /* 说明如果UE支持E-UTRAN能力且支持CS fallback and SMS over SGs，
           则ATTACH和RAU请求消息中需要带Voice domain preference and UE's usage setting IE项 */
        if ((NAS_MML_LTE_SUPPORT_CSFB_AND_SMS_OVER_SGS == NAS_MML_GetLteCsServiceCfg())
        && (VOS_TRUE == ulSupportLteFlg))
        {
            /* 填写Voice domain preference and UE's usage setting IE项,长度为3 */
            ucNumber += NAS_GMM_Fill_IE_VoiceDomainPreferenceAndUeUsageSetting(&pAttachRequest->aucNasMsg[ucNumber]);
        }

        /* 输出可维可测信息 */
        if ( VOS_TRUE == NAS_GMM_GetLteInfo(NAS_LMM_GUTI, &stLmmInfo))
        {
            NAS_GMM_LogGutiInfo(&(stLmmInfo.u.stGuti));
        }
    }
#endif

    /* 填写空口消息长度*/
    pAttachRequest->ulNasMsgSize = ucNumber;

    /* 发起Attach流程，更新GMM链路释放控制变量 */
    NAS_GMM_UpdateGmmLinkCtrlStru();
    g_GmmAttachCtrl.ucRetryAttachForRelCtrlFlg = VOS_FALSE;



    return pAttachRequest;
}


VOS_UINT16 NAS_GMM_GetAttachComPleteMsgLen()
{
    VOS_UINT16                          usMsgLen;
    VOS_UINT8                           ucInterRatInfoFlag;

#if (FEATURE_ON == FEATURE_LTE)
    VOS_UINT16                          ucEUeCapDataLen = 0;
    VOS_UINT8                           aucEUeCapData[LTE_NAS_INTER_RAT_UETRAN_CAP_LEN];
    MMC_LMM_PLMN_ID_STRU                stLmmPlmn;
    VOS_UINT8                           ucEUtranInterRatInfoFlag = VOS_FALSE;


    PS_MEM_SET(&stLmmPlmn, 0x00, sizeof(MMC_LMM_PLMN_ID_STRU));
#endif

#if (FEATURE_ON == FEATURE_LTE)
        if (VOS_TRUE == g_GmmInterRatInfoCtrl.ucEutranInterRatInfoRequestedFlag)
        {
            NAS_MML_ConvertNasPlmnToLMMFormat(&stLmmPlmn,NAS_MML_GetCurrCampPlmnId());

            ucEUeCapDataLen = LTE_NAS_INTER_RAT_UETRAN_CAP_LEN;
            if (MMC_LMM_SUCC == Nas_GetEutraUeCap(stLmmPlmn, (VOS_UINT16*)&ucEUeCapDataLen, aucEUeCapData))
            {
                if (0 != ucEUeCapDataLen)
                {
                    ucEUtranInterRatInfoFlag = VOS_TRUE;
                }
            }

        }
#endif

    if ( (VOS_TRUE == g_GmmInterRatInfoCtrl.ucAttachCmpWaitInterRatCnfMsg)
      && (VOS_TRUE == g_GmmInterRatInfoCtrl.ucInterRatCnfMsgRcvdFlg)
      && (0 != g_GmmInterRatInfoCtrl.ucDataLen))
    {
        ucInterRatInfoFlag = VOS_TRUE;
    }
    else
    {
        ucInterRatInfoFlag = VOS_FALSE;
    }

    usMsgLen = sizeof(NAS_MSG_STRU);

    if ((VOS_TRUE == ucInterRatInfoFlag)
#if (FEATURE_ON == FEATURE_LTE)
      || (VOS_TRUE == ucEUtranInterRatInfoFlag)
#endif
     )
     {
         usMsgLen = (sizeof(NAS_MSG_STRU) - 4) + GMM_MSG_LEN_ATTACH_COMPLETE;
     }

    /* 包含UTRAN INTER RAT INFOMATION */
    if (VOS_TRUE == ucInterRatInfoFlag)
    {
        usMsgLen += 2 + g_GmmInterRatInfoCtrl.ucDataLen ;
    }

    /* 包含E-UTRAN INTER RAT INFORMATION */
#if (FEATURE_ON == FEATURE_LTE)
    if (VOS_TRUE == ucEUtranInterRatInfoFlag)
    {
        usMsgLen += 2 + ucEUeCapDataLen;
    }
#endif

    return usMsgLen;

}

NAS_MSG_STRU* Gmm_AttachCompleteMsgMake(VOS_VOID)
{
    NAS_MSG_STRU                       *pAttachComplete = VOS_NULL_PTR;                                       /* 指向NAS_MSG_STRU结构的指针变量           */
    VOS_UINT16                          usMsgLen;
    VOS_BOOL                            bInterRatInfoFlg;
    VOS_UINT16                          usIeOffset       = GMM_MSG_LEN_ATTACH_COMPLETE;

#if (FEATURE_ON == FEATURE_LTE)
    VOS_UINT8                           ucEUeCapDataLen = 0;
    VOS_UINT8                           aucEUeCapData[LTE_NAS_INTER_RAT_UETRAN_CAP_LEN] = {0};
    MMC_LMM_PLMN_ID_STRU                stLmmPlmn;


    PS_MEM_SET(&stLmmPlmn, 0x00, sizeof(stLmmPlmn));
#endif

    if ( (VOS_TRUE == g_GmmInterRatInfoCtrl.ucAttachCmpWaitInterRatCnfMsg)
      && (VOS_TRUE == g_GmmInterRatInfoCtrl.ucInterRatCnfMsgRcvdFlg)
      && (0 != g_GmmInterRatInfoCtrl.ucDataLen))
    {
        bInterRatInfoFlg = VOS_TRUE;
    }
    else
    {
        bInterRatInfoFlg = VOS_FALSE;
    }

    usMsgLen = NAS_GMM_GetAttachComPleteMsgLen();

    pAttachComplete = (NAS_MSG_STRU *)Gmm_MemMalloc(usMsgLen);
    if (VOS_NULL_PTR == pAttachComplete)
    {
        NAS_ERROR_LOG(WUEPS_PID_GMM, "Gmm_AttachCompleteMsgMake:ERROR: MALLOC FAIL!");
        return VOS_NULL_PTR;
    }

    Gmm_MemSet(pAttachComplete, 0, usMsgLen);

    /* 填写空口消息参数             */
    pAttachComplete->ulNasMsgSize = GMM_MSG_LEN_ATTACH_COMPLETE;
    pAttachComplete->aucNasMsg[0] = GMM_PD_GMM;
    pAttachComplete->aucNasMsg[1] = GMM_MSG_ATTACH_COMPLETE;

    if (VOS_TRUE == bInterRatInfoFlg)
    {
        pAttachComplete->ulNasMsgSize = GMM_MSG_LEN_ATTACH_COMPLETE
                                        + 2 + g_GmmInterRatInfoCtrl.ucDataLen;
        pAttachComplete->aucNasMsg[usIeOffset++] = GMM_IEI_INTER_RAT_HANDOVER_INFORMATION;
        pAttachComplete->aucNasMsg[usIeOffset++] = g_GmmInterRatInfoCtrl.ucDataLen;
        Gmm_MemCpy(&(pAttachComplete->aucNasMsg[4]), g_GmmInterRatInfoCtrl.aucData,
                    g_GmmInterRatInfoCtrl.ucDataLen);
        usIeOffset += g_GmmInterRatInfoCtrl.ucDataLen;
    }

#if (FEATURE_ON == FEATURE_LTE)
    /* 获取UTRAN INTER RAT INFO */
    if (VOS_TRUE == g_GmmInterRatInfoCtrl.ucEutranInterRatInfoRequestedFlag)
    {
        NAS_MML_ConvertNasPlmnToLMMFormat(&stLmmPlmn,NAS_MML_GetCurrCampPlmnId());

        ucEUeCapDataLen = LTE_NAS_INTER_RAT_UETRAN_CAP_LEN;
        if (MMC_LMM_SUCC != Nas_GetEutraUeCap(stLmmPlmn, (VOS_UINT16*)&ucEUeCapDataLen, aucEUeCapData))
        {
            ucEUeCapDataLen = 0;
        }
    }

    if ((VOS_TRUE == g_GmmInterRatInfoCtrl.ucEutranInterRatInfoRequestedFlag)
     && (0 != ucEUeCapDataLen))
    {
        pAttachComplete->ulNasMsgSize += 2 + ucEUeCapDataLen;
        pAttachComplete->aucNasMsg[usIeOffset++] = GMM_IEI_E_UTRAN_INTER_RAT_HANDOVER_INFORMATION;
        pAttachComplete->aucNasMsg[usIeOffset++] = ucEUeCapDataLen;
        Gmm_MemCpy(&(pAttachComplete->aucNasMsg[usIeOffset]), aucEUeCapData,
                    ucEUeCapDataLen);
    }

    /* 获取UTRAN INTER RAT INFO 后将标志清除 */
    g_GmmInterRatInfoCtrl.ucEutranInterRatInfoRequestedFlag = VOS_FALSE;

#endif

    /* 全局变量复位 */
    Gmm_MemSet(&g_GmmInterRatInfoCtrl, 0, sizeof(GMM_INTER_RAT_HANDOVER_INFO_CTRL_STRU));

    return pAttachComplete;
}
VOS_VOID Gmm_DelPsLocInfoUpdateUsim(VOS_VOID)
{
    NAS_MML_RAI_STRU                       *pstLastSuccRai;
    GMM_PLMN_ID_STRU                        stGmmPlmnId;

    pstLastSuccRai    = NAS_MML_GetPsLastSuccRai();
    NAS_GMM_ConvertPlmnIdToGmmFormat(&(pstLastSuccRai->stLai.stPlmnId), &stGmmPlmnId);

    g_GmmGlobalCtrl.UeInfo.UeId.ucUeIdMask  &= ~GMM_UEID_P_TMSI;                /* 清除P_TMSI存在MASK                       */
    g_GmmGlobalCtrl.UeInfo.UeId.ucUeIdMask  &= ~GMM_UEID_P_TMSI_SIGNATURE;      /* 清除P_TMSI_SIGNATURE存在MASK             */

    if (VOS_FALSE == NAS_MML_GetSimPsRegStatus())
    {                                                                       /* sim卡无效                                */
        /* 清除相关标志 */
        NAS_GMM_ClearIMSIOfUeID();
    }

    /* 将PTMSI、 Ptmsi Signature设置为无效  */
    NAS_MML_InitUeIdPtmisInvalid();
    NAS_MML_InitUeIdPtmsiSignatureInvalid();

    pstLastSuccRai->stLai.aucLac[0]            = NAS_MML_LAC_LOW_BYTE_INVALID;
    pstLastSuccRai->stLai.aucLac[1]            = NAS_MML_LAC_HIGH_BYTE_INVALID;


    Gmm_SndAgentUsimUpdateFileReq(GMM_USIM_FILE_PS_LOC_INFO);                   /* 通知AGENT删除PSlocation information      */
    Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_PTMSI_RAI);                        /* 通知RRC NAS层信息变化                    */

#if (FEATURE_ON == FEATURE_PTM)
    /* 工程菜单打开后，PTMSI发生改变需要上报给OM */
    NAS_GMM_SndAcpuOmChangePtmsi();
#endif

    /*modify,sunxibo,2007-08-22,for plmnsearch begin*/
    if ((GMM_TRUE == GMM_IsCasGsmMode())
     && (GMM_TRUE == gstGmmCasGlobalCtrl.ucTlliAssignFlg))
    /*modify,sunxibo,2007-08-22,for plmnsearch end*/
    {
        Gmm_ClearLlcData(LL_GMM_CLEAR_DATA_TYPE_USER);
    }
    if (NAS_MML_CKSN_INVALID != NAS_MML_GetSimPsSecurityCksn())
    {                                                                           /* 原来的Key情报有效                        */
        NAS_MML_SetSimPsSecurityCksn(NAS_MML_CKSN_INVALID);            /* 删除CKSN                                 */
        NAS_MML_InitPsSecurityUmtsCkInvalid();
        NAS_MML_InitPsSecurityUmtsIkInvalid();

        Gmm_SndAgentUsimUpdateFileReq(GMM_USIM_FILE_PS_KEY);                    /* 通知AGENT删除PS域加密信息                */

        Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_DEL_KEY);                      /* 通知RRC NAS层信息变化                    */

    }
    else
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_DelPsLocInfoUpdateUsim: NAS_MML_CKSN_INVALID");
    }

    return;
}


VOS_VOID Gmm_AttachRejectCause3(
                            VOS_UINT32 ulGmmCause                                    /* 失败原因                                 */
                            )
{
    NAS_MML_SetPsUpdateStatus(NAS_MML_ROUTING_UPDATE_STATUS_PLMN_NOT_ALLOWED);      /* 更新状态置为GU3                          */

    /* 设置GMM的卡无效 */
    NAS_MML_SetSimPsRegStatus(VOS_FALSE);

    Gmm_DelPsLocInfoUpdateUsim();                                               /* 调用函数删除PS LOCATION INFO             */

    Gmm_ComStaChg(GMM_DEREGISTERED_NO_IMSI);                                    /* 调用状态公共处理                         */

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || ( GMM_ATTACH_NORMAL      == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* NORMAL ATTACH                            */
        NAS_GMM_SndMmGmmActionResultInd(GMMMM_ACTION_FAILURE,
                                        ulGmmCause);                                     /* 发送MMCGMM_GMM_ACTION_RESULT_IND给MMC    */
    }
    else
    {                                                                           /* 当前流程为combined attach流程            */
        NAS_GMM_SndMmCombinedAttachRejected(ulGmmCause);                        /* 发送MMCGMM_COMBINED_ATTACH_REJECTED给MMC */

    }

    /* 向MMC发送PS注册结果 */
    NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                 GMM_MMC_ACTION_RESULT_FAILURE,
                                 (NAS_MML_REG_FAIL_CAUSE_ENUM_UINT16)ulGmmCause);

    /* 事件上报 */
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_FAIL,
                    &ulGmmCause,
                    NAS_OM_EVENT_ATTACH_FAIL_LEN
                    );

    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;

    NAS_GMM_DeleteEPlmnList();

    return;
}


VOS_VOID Gmm_AttachRejectCause11(VOS_VOID)
{
    NAS_MML_REG_FAIL_CAUSE_ENUM_UINT16  enCause;
    enCause = NAS_MML_REG_FAIL_CAUSE_PLMN_NOT_ALLOW;

    NAS_MML_SetPsUpdateStatus(NAS_MML_ROUTING_UPDATE_STATUS_ROUTING_AREA_NOT_ALLOWED);      /* 更新状态置为GU3                          */
    Gmm_DelPsLocInfoUpdateUsim();                                               /* 调用函数删除PS LOCATION INFO             */
    g_GmmGlobalCtrl.SysInfo.ucForbMask |= GMM_FORBIDDEN_PLMN;
    Gmm_ComStaChg(GMM_DEREGISTERED_LIMITED_SERVICE);                            /* 调用状态公共处理                         */

    g_GmmAttachCtrl.ucAttachAttmptCnt = 0;                                      /* attach attempt counter清0                */

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_NORMAL       == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* NORMAL ATTACH                            */
        NAS_GMM_SndMmGmmActionResultInd(GMMMM_ACTION_FAILURE,
                                        enCause);                                     /* 发送MMCGMM_GMM_ACTION_RESULT_IND给MMC    */
    }
    else
    {                                                                           /* combined attach流程                      */
        g_GmmRauCtrl.ucRauAttmptCnt = 0;                                        /* RAU attempt counter清0                   */

        NAS_GMM_SndMmCombinedAttachRejected(enCause);          /* 发送MMCGMM_COMBINED_ATTACH_REJECTED给MMC */
    }


    /* 向MMC发送PS注册结果 */
    NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                 GMM_MMC_ACTION_RESULT_FAILURE,
                                 enCause);

    /* 事件上报 */
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_FAIL,
                    &enCause,
                    NAS_OM_EVENT_ATTACH_FAIL_LEN
                    );

    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;

    NAS_GMM_DeleteEPlmnList();


    return;
}

VOS_VOID Gmm_AttachRejectCause12(
                             VOS_UINT32 ulGmmCause,                                   /* 失败原因                                 */
                             VOS_BOOL   bUpdateSimFlg
                             )
{
    if (NAS_MML_REG_FAIL_CAUSE_LA_NOT_ALLOW == ulGmmCause)
    {
        g_GmmGlobalCtrl.ucLaNotAllowFlg = GMM_TRUE;
    }
    if (VOS_TRUE == bUpdateSimFlg)
    {
        NAS_MML_SetPsUpdateStatus(NAS_MML_ROUTING_UPDATE_STATUS_ROUTING_AREA_NOT_ALLOWED);      /* 更新状态置为GU3                          */
        Gmm_DelPsLocInfoUpdateUsim();                                               /* 调用函数删除PS LOCATION INFO             */
    }
    Gmm_ComStaChg(GMM_DEREGISTERED_LIMITED_SERVICE);                            /* 调用状态公共处理                         */
    g_GmmAttachCtrl.ucAttachAttmptCnt = 0;                                      /* attach attempt counter清0                */

    if (NAS_MML_REG_FAIL_CAUSE_LA_NOT_ALLOW == ulGmmCause)
    {                                                                           /* 原因值为LA_NOT_ALLOW(12)                 */
        g_GmmGlobalCtrl.SysInfo.ucForbMask |= GMM_FORBIDDEN_LA_FOR_REG_PRVS_SVS;
    }
    else if (NAS_MML_REG_FAIL_CAUSE_ROAM_NOT_ALLOW == ulGmmCause)
    {
        NAS_GMM_DeleteEPlmnList();

        g_GmmGlobalCtrl.SysInfo.ucForbMask |= GMM_FORBIDDEN_LA_FOR_ROAM;
    }
    else
    {                                                                           /* 原因值为13,15  */
        g_GmmGlobalCtrl.SysInfo.ucForbMask |= GMM_FORBIDDEN_LA_FOR_ROAM;
    }

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_NORMAL       == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* NORMAL ATTACH                            */
        NAS_GMM_SndMmGmmActionResultInd(GMMMM_ACTION_FAILURE,
                                        ulGmmCause);                                     /* 发送MMCGMM_GMM_ACTION_RESULT_IND给MMC    */
    }
    else
    {                                                                           /* combined attach流程                      */
        NAS_GMM_SndMmCombinedAttachRejected(ulGmmCause);                           /* 发送MMCGMM_COMBINED_ATTACH_REJECTED给MMC */
    }


    /* 向MMC发送PS注册结果 */
    NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                 GMM_MMC_ACTION_RESULT_FAILURE,
                                 (NAS_MML_REG_FAIL_CAUSE_ENUM_UINT16)ulGmmCause);

    /* 事件上报 */
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_FAIL,
                    &ulGmmCause,
                    NAS_OM_EVENT_ATTACH_FAIL_LEN
                    );

    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
    return;
}


VOS_VOID Gmm_AttachRejectCause14(VOS_BOOL   bUpdateSimFlg)
{
    NAS_MML_REG_FAIL_CAUSE_ENUM_UINT16  enCause;

    if (VOS_TRUE == bUpdateSimFlg)
    {
        NAS_MML_SetPsUpdateStatus(NAS_MML_ROUTING_UPDATE_STATUS_ROUTING_AREA_NOT_ALLOWED);      /* 更新状态置为GU3                          */
	    Gmm_DelPsLocInfoUpdateUsim();                                               /* 调用函数删除PS LOCATION INFO             */
    }
    g_GmmGlobalCtrl.SysInfo.ucForbMask |= GMM_FORBIDDEN_PLMN_FOR_GPRS;
    Gmm_ComStaChg(GMM_DEREGISTERED_LIMITED_SERVICE);                            /* 调用状态公共处理                         */

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_NORMAL       == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* NORMAL ATTACH                            */
        NAS_GMM_SndMmGmmActionResultInd(GMMMM_ACTION_FAILURE,
                                        NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW_IN_PLMN);                                    /* 发送MMCGMM_GMM_ACTION_RESULT_IND给MMC    */
    }
    else
    {                                                                           /* combined attach                          */
        NAS_GMM_SndMmCombinedAttachRejected(
                                     NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW_IN_PLMN); /* 发送MMCGMM_COMBINED_ATTACH_REJECTED给MMC */
    }

    enCause = NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW_IN_PLMN;
    /* 向MMC发送PS注册结果 */
    NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                 GMM_MMC_ACTION_RESULT_FAILURE,
                                 enCause);

    /* 事件上报 */
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_FAIL,
                    &enCause,
                    NAS_OM_EVENT_ATTACH_FAIL_LEN
                    );

    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
    return;
}


VOS_VOID Gmm_AttachAttemptCounter(
                              VOS_UINT32 ulGmmCause                                  /* 失败原因                                 */
                              )
{
    g_GmmAttachCtrl.ucAttachAttmptCnt++;                                        /* Attach Attempt Counter加1                */
    Gmm_ComStaChg(GMM_DEREGISTERED_ATTEMPTING_TO_ATTACH);                       /* 调用状态公共处理                         */


    if (5 > g_GmmAttachCtrl.ucAttachAttmptCnt)
    {                                                                           /* Attach attempt counter<5                 */
        Gmm_TimerStart(GMM_TIMER_T3311);                                        /* 启动Timer3311                            */
    }
    else
    {                                                                           /* Attach attempt counter>=5                */
        NAS_MML_SetPsUpdateStatus(NAS_MML_ROUTING_UPDATE_STATUS_NOT_UPDATED);     /* 更新状态置为GU2                          */
        Gmm_DelPsLocInfoUpdateUsim();                                           /* 调用函数删除PS LOCATION INFO             */

        Gmm_TimerStart(GMM_TIMER_T3302);                                        /* 启动Timer3302                            */
        if (GMM_TRUE == g_GmmAttachCtrl.ucSmCnfFlg)
        {                                                                           /* ATTACH过程是由SM触发的                   */
            Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_ATTACH_MAX_TIMES);       /* 通知SM GMM注册失败                       */

            g_GmmAttachCtrl.ucSmCnfFlg = GMM_FALSE;                                 /* 清ucSmCnfFlg标志                         */
        }
        g_GmmGlobalCtrl.ucFollowOnFlg = GMM_FALSE;
        NAS_MML_SetPsServiceBufferStatusFlg(VOS_FALSE);
        if ((NAS_MML_MS_MODE_CS_ONLY == NAS_MML_GetMsMode())
         && (VOS_FALSE == g_GmmGlobalCtrl.ucUserPsAttachFlag))
        {
            Gmm_ComStaChg(GMM_DEREGISTERED_NORMAL_SERVICE);
            NAS_MML_SetPsAttachAllowFlg(VOS_FALSE);
        }

        NAS_GMM_DeleteEPlmnList();

    }

    if (NAS_MML_REG_FAIL_CAUSE_TIMER_TIMEOUT == ulGmmCause)
    {                                                                       /* T3310 time-out 5次                       */
        Gmm_SndRrmmRelReq(RRC_CELL_UNBARRED);                                                /* 释放PS 域系统消息                        */
    }

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_NORMAL       == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* NORMAL ATTACH                            */
        NAS_GMM_SndMmGmmActionResultInd(GMMMM_ACTION_FAILURE,
                                        ulGmmCause);                                     /* 发送MMCGMM_GMM_ACTION_RESULT_IND给MMC    */
    }
    else
    {                                                                           /* combined attach                          */
        NAS_GMM_SndMmCombinedAttachRejected(ulGmmCause);                           /* 发送MMCGMM_COMBINED_ATTACH_REJECTED给MMC */
    }


    /* 向MMC发送PS注册结果 */
    NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                 GMM_MMC_ACTION_RESULT_FAILURE,
                                 (NAS_MML_REG_FAIL_CAUSE_ENUM_UINT16)ulGmmCause);

    /* 事件上报 */
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_FAIL,
                    (VOS_VOID*)&ulGmmCause,
                    NAS_OM_EVENT_ATTACH_FAIL_LEN
                    );

    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
    return;
}
VOS_UINT8 Gmm_RcvAttachRejectMsg_T3302_Handling(NAS_MSG_FOR_PCLINT_STRU *pMsg)
{
    NAS_MSG_STRU        *pGmmStatus;

    /* 24.008: 9.4.4
      In Iu mode, the MS shall ignore the contents of this IE if this message is received without
      integrity protection.
      If this IE is not included or if in Iu mode the message is not integrity protected,
      the MS shall use the default value. */
    if (GMM_IEI_T3302_VALUE == pMsg->aucNasMsg[GMM_MSG_LEN_ATTACH_REJECT])
    {                                                                       /* 消息中含有T3302 value                    */
        if (((GMM_MSG_LEN_ATTACH_REJECT + 1)==pMsg->ulNasMsgSize)
            || ((GMM_MSG_LEN_ATTACH_REJECT + 2)==pMsg->ulNasMsgSize))
        {
        }
        else
        {
            if (1 != pMsg->aucNasMsg[GMM_MSG_LEN_ATTACH_REJECT + 1])
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachRejectMsg:WARNING: Semantically incorrect message!");
                pGmmStatus = Gmm_GmmStatusMsgMake(
                                 NAS_MML_REG_FAIL_CAUSE_SEMANTICALLY_INCORRECT_MSG);
                Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH , pGmmStatus);

                return GMM_FAILURE;
            }

            if ( (VOS_TRUE == NAS_UTRANCTRL_IsUtranPsSmcNeeded())
              && (NAS_MML_RRC_INTEGRITY_PROTECT_ACTIVE != g_MmSubLyrShare.GmmShare.ucPsIntegrityProtect) )
            {                                                               /* If in Iu mode the message is not integrity protected, the MS shall use the default value */
                g_GmmTimerMng.aTimerInf[GMM_TIMER_T3302].ulTimerVal = GMM_TIMER_T3302_VALUE;
            }
            else
            {
                Gmm_SaveTimerValue(GMM_TIMER_T3302,
                   pMsg->aucNasMsg[GMM_MSG_LEN_ATTACH_REJECT + 2]); /* 存储T3302时长                            */
            }
        }
    }
    else
    {                                                                       /* If this IE is not included , the MS shall use the default value */
        g_GmmTimerMng.aTimerInf[GMM_TIMER_T3302].ulTimerVal = GMM_TIMER_T3302_VALUE;
    }

    return GMM_SUCCESS;
}


VOS_VOID Gmm_RcvAttachRejectMsg_Cause_Handling_Other_Cause(VOS_UINT8 ucCause)
{
    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachRejectMsg:WARNING: GMM CAUSE is abnormal");

    NAS_GMM_UpdateAttemptCounterForSpecialCause(VOS_TRUE, ucCause);

    if ((NAS_MML_REG_FAIL_CAUSE_RETRY_UPON_ENTRY_CELL_MIN    <= ucCause)
        && (NAS_MML_REG_FAIL_CAUSE_RETRY_UPON_ENTRY_CELL_MAX >= ucCause))
    {
        g_GmmAttachCtrl.ucRetryFlg = GMM_TRUE;                              /* 置换小区进行ATTACH标志                   */
    }
    else
    {
        g_GmmAttachCtrl.ucRetryFlg = GMM_FALSE;                             /* 清除换小区进行ATTACH标志                 */
    }

    Gmm_AttachAttemptCounter(ucCause);                           /* 调用异常处理函数                         */

    return;

}
VOS_VOID Gmm_RcvAttachRejectMsg_Cause_Handling(VOS_UINT8 ucCause)
{
    /* 保存注册失败原因值 */
    NAS_GMM_SetAttach2DetachErrCode(NAS_GMM_TransGmmNwCause2GmmSmCause(ucCause));

    switch (ucCause)
    {                                                                           /* GMM CAUSE                                */
    case NAS_MML_REG_FAIL_CAUSE_ILLEGAL_MS:
    case NAS_MML_REG_FAIL_CAUSE_ILLEGAL_ME:
    case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW:
    case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_AND_NON_GPRS_SERV_NOT_ALLOW:
        Gmm_AttachRejectCause3(ucCause);                             /* 调用#3,#6,#7,#8处理函数                  */
        if (NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW == ucCause)
        {                                                                       /* 原因值7                                  */
            if ((GMM_TRUE == g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg)
                && (GMM_TRUE == g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg))
            {                                                                   /* 要触发LU                                 */
                NAS_GMM_SndMmLuInitiation(MMC_GMM_NO_TIMER_EXPIRED);              /* 通知MM进行LU                             */
            }
        }
        break;
    case NAS_MML_REG_FAIL_CAUSE_PLMN_NOT_ALLOW:
        Gmm_AttachRejectCause11();                                              /* 调用#11处理函数                          */
        break;
    case NAS_MML_REG_FAIL_CAUSE_LA_NOT_ALLOW:
    case NAS_MML_REG_FAIL_CAUSE_ROAM_NOT_ALLOW:
    case NAS_MML_REG_FAIL_CAUSE_NO_SUITABL_CELL:
        Gmm_AttachRejectCause12(ucCause, VOS_TRUE);                            /* 调用#12,#13,#15处理函数                  */
        break;
    case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW_IN_PLMN:
        Gmm_AttachRejectCause14(VOS_TRUE);                                              /* 调用#14处理函数                          */
        if ((GMM_TRUE == g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg)
            && (GMM_TRUE == g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg))
        {                                                                       /* 要触发LU                                 */
            NAS_GMM_SndMmLuInitiation(MMC_GMM_NO_TIMER_EXPIRED);                  /* 通知MM进行LU                             */
        }
        break;
    default:
        Gmm_RcvAttachRejectMsg_Cause_Handling_Other_Cause(ucCause);
        break;
    }

    NAS_GMM_CheckCauseToStartT3340(ucCause);
}
NAS_STK_UPDATE_TYPE_ENUM_UINT8 NAS_GMM_ConvertGmmAttachTypeToStkRauType (
    NAS_GMM_SPEC_PROC_TYPE_ENUM_UINT8   enGmmAttachType
)
{
    switch (enGmmAttachType)
    {
        case GMM_ATTACH_COMBINED:
             return NAS_STK_ATTACH_COMBINED;

        case GMM_ATTACH_WHILE_IMSI_ATTACHED:
             return NAS_STK_ATTACH_WHILE_IMSI_ATTACHED;

        case GMM_ATTACH_NORMAL:
             return NAS_STK_ATTACH_NORMAL;

        case GMM_ATTACH_NORMAL_CS_TRANS:
             return NAS_STK_ATTACH_NORMAL_CS_TRANS;

        default:
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_ConvertGmmAttachTypeToStkRauType:WARNING: The input in invalid");
            return NAS_STK_UPDATE_TYPE_ENUM_BUTT;
    }
}



VOS_VOID NAS_GMM_SndStkAttachRej(VOS_UINT8 ucCause)
{
    NAS_STK_NETWORK_REJECTION_EVENT_STRU                   *pstMsg = VOS_NULL_PTR;
    VOS_UINT32                                              ulLength;
    NAS_MML_LAI_STRU                                       *pstRai = VOS_NULL_PTR;

    /* 申请内存 */
    ulLength = sizeof(NAS_STK_NETWORK_REJECTION_EVENT_STRU) - VOS_MSG_HEAD_LENGTH;
    pstMsg   = (NAS_STK_NETWORK_REJECTION_EVENT_STRU *)PS_ALLOC_MSG(WUEPS_PID_GMM, ulLength);

    if (VOS_NULL_PTR == pstMsg)
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_ReportStkAttachRej: Alloc Msg");
        return;
    }

    PS_MEM_SET(&(pstMsg->stNetworkRejectionEvent), 0, sizeof(NAS_STK_NETWORK_REJECTION_EVENT_INFO_STRU));

    /* 填写新消息内容 */
    pstMsg->ulReceiverPid   = NAS_GetOamReceivePid(MAPS_STK_PID);

    pstRai =  NAS_MML_GetCurrCampLai();
    pstMsg->stNetworkRejectionEvent.uRejectInfo.stRauInfo.stCurPlmnID.Mcc = pstRai->stPlmnId.ulMcc;
    pstMsg->stNetworkRejectionEvent.uRejectInfo.stRauInfo.stCurPlmnID.Mnc = pstRai->stPlmnId.ulMnc;
    pstMsg->stNetworkRejectionEvent.uRejectInfo.stRauInfo.usLac           = (pstRai->aucLac[0] << 8) | (pstRai->aucLac[1]);
    pstMsg->stNetworkRejectionEvent.uRejectInfo.stRauInfo.ucRac           = NAS_MML_GetCurrCampRac();
    pstMsg->stNetworkRejectionEvent.enRejType                 = NAS_STK_ATTACH_REJECT;

    pstMsg->stNetworkRejectionEvent.enRat                     = NAS_GMM_ConvertGmmRatTypeToStkRatType(pstRai->enCampPlmnNetRat);

    pstMsg->stNetworkRejectionEvent.ucCauseCode               = ucCause;

    pstMsg->stNetworkRejectionEvent.enUpdateAttachType        = NAS_GMM_ConvertGmmAttachTypeToStkRauType(NAS_GMM_GetSpecProc());

    pstMsg->ulMsgName                                         = ID_NAS_STK_NETWORK_REJECTION_EVENT;

    /* 发送消息 */
    if (VOS_OK != PS_SEND_MSG( WUEPS_PID_GMM, pstMsg))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_ReportStkAttachRej: PS_SEND_MSG ERROR");
    }

    return;
}


VOS_VOID NAS_GMM_ResetHplmnRejCauseChangedCounter (VOS_VOID)
{
    NAS_MML_HPLMN_REJ_CAUSE_CHANGED_COUNTER_STRU           *pstChangeCauseCounterInfo = VOS_NULL_PTR;

    pstChangeCauseCounterInfo = NAS_MML_GetHplmnRejCauseChangedCounter();

    pstChangeCauseCounterInfo->ucGmmHplmnRejCauseChangedCounter = 0;

    return;

}



VOS_VOID NAS_GMM_HandleHplmnRejCauseChange(
    NAS_MSG_FOR_PCLINT_STRU            *pstMsg,
    VOS_UINT8                           ucConfigCauseNvim
)
{
    NAS_MML_HPLMN_REJ_CAUSE_CHANGED_COUNTER_STRU           *pstChangeCauseCounterInfo = VOS_NULL_PTR;

    pstChangeCauseCounterInfo = NAS_MML_GetHplmnRejCauseChangedCounter();

    /* 如果NV中配置的拒绝原因值为#17，则判断修改的次数是否已经达到上限，如果达到上限则不再修改为#17 */
    if (NAS_MML_REG_FAIL_CAUSE_NETWORK_FAILURE == ucConfigCauseNvim)
    {
        if (NAS_MML_MAX_HPLMN_REJ_CAUSE_CHANGE_COUNTER > pstChangeCauseCounterInfo->ucGmmHplmnRejCauseChangedCounter)
        {
            pstMsg->aucNasMsg[2] = ucConfigCauseNvim;
            pstChangeCauseCounterInfo->ucGmmHplmnRejCauseChangedCounter++;
        }
    }
    else
    {
        pstMsg->aucNasMsg[2] = ucConfigCauseNvim;
    }

    return;
}
VOS_VOID NAS_GMM_ChangeRegRejCauseAvoidInvalidSim(
    NAS_MSG_FOR_PCLINT_STRU            *pstMsg
)
{
    NAS_MML_PLMN_ID_STRU               *pstCurrCampPlmnId = VOS_NULL_PTR;
    VOS_UINT8                           ucRejCause;
    NAS_MML_CHANGE_REG_REJ_CAUSE_TYPE_ENUM_UINT8 enChangeRegRejType;

    enChangeRegRejType = NAS_MML_GetChangeRegRejCauFlg();

    ucRejCause = 0;

    /* 增加测试卡保护，测试卡时不修改，直接返回 */
    if (VOS_TRUE == NAS_USIMMAPI_IsTestCard())
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_ChangeRegRejCauseAvoidInvalidSim: The sim is Test card");
        return;
    }

    if ((NAS_MML_CHANGE_REG_REJ_CAUSE_TYPE_CS_PS    != enChangeRegRejType)
     && (NAS_MML_CHANGE_REG_REJ_CAUSE_TYPE_PS_ONLY  != enChangeRegRejType))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "NAS_GMM_ChangeRegRejCauseAvoidInvalidSim: The Nvim is not actived");
        return;
    }

    /* 取得当前驻留的PLMN ID */
    pstCurrCampPlmnId = NAS_MML_GetCurrCampPlmnId();

    ucRejCause = pstMsg->aucNasMsg[2];

    if ((NAS_MML_REG_FAIL_CAUSE_ILLEGAL_MS == ucRejCause)
     || (NAS_MML_REG_FAIL_CAUSE_ILLEGAL_ME == ucRejCause)
     /* 由于GMM收到#7后可能影响CS的正常使用，因此去掉#7的修改 */
     || (NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_AND_NON_GPRS_SERV_NOT_ALLOW == ucRejCause))
    {
        /* 如果是HPLMN/EHPLMN则修改为#17，否则修改为#13 */
        if (VOS_TRUE == NAS_MML_ComparePlmnIdWithHplmn(pstCurrCampPlmnId))
        {
            NAS_GMM_HandleHplmnRejCauseChange(pstMsg, NAS_MML_GetPreferredRegRejCause_HPLMN_EHPLMN());
        }
        else
        {
            pstMsg->aucNasMsg[2] = NAS_MML_GetPreferredRegRejCause_NOT_HPLMN_EHPLMN();
        }
    }

}


VOS_VOID Gmm_RcvAttachRejectMsg(
                            NAS_MSG_FOR_PCLINT_STRU *pMsg
                            )
{
    NAS_MSG_STRU        *pGmmStatus;
    VOS_UINT8            ucRst;


    if (GMM_REGISTERED_INITIATED != g_GmmGlobalCtrl.ucState)
    {
        /* GMM状态不是GMM_REGISTERED_INITIATED，丢弃此消息 */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachRejectMsg:WARNING: GMM's state is not GMM_REGISTERED_INITIATED");

        return;
    }

    if (GMM_MSG_LEN_ATTACH_REJECT > pMsg->ulNasMsgSize)
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachRejectMsg:WARNING: The Msg's length is not enough!");
        /* 消息过短                                 */
        pGmmStatus = Gmm_GmmStatusMsgMake(NAS_MML_REG_FAIL_CAUSE_INVALID_MANDATORY_INF);

        Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH , pGmmStatus);
        return;
    }

    ucRst = Gmm_RcvAttachRejectMsg_T3302_Handling(pMsg);

    if( GMM_FAILURE == ucRst)
    {
        return;
    }

    NAS_GMM_ChangeRegRejCauseAvoidInvalidSim(pMsg);

    NAS_GMM_SndStkAttachRej(pMsg->aucNasMsg[2]);

    Gmm_TimerStop(GMM_TIMER_T3310);                                             /* 停止Timer3310                            */
    Gmm_TimerStop(GMM_TIMER_T3318);                                             /* 停止T3318                                */
    Gmm_TimerStop(GMM_TIMER_T3320);                                             /* 停止T3320                                */

    Gmm_RcvAttachRejectMsg_Cause_Handling(pMsg->aucNasMsg[2]);

    if (VOS_FALSE == NAS_GMM_IsAttachOtherCause(pMsg->aucNasMsg[2]))
    {
        if (GMM_TRUE == g_GmmAttachCtrl.ucSmCnfFlg)
        {                                                                           /* ATTACH过程是由SM触发的                   */

            Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE,
                                  NAS_GMM_TransGmmNwCause2GmmSmCause(pMsg->aucNasMsg[2]));    /* 通知SM GMM注册失败                       */

            g_GmmAttachCtrl.ucSmCnfFlg = GMM_FALSE;                                 /* 清ucSmCnfFlg标志                         */
        }

        if ((NAS_MML_MS_MODE_CS_ONLY == NAS_MML_GetMsMode())
         && (VOS_FALSE == g_GmmGlobalCtrl.ucUserPsAttachFlag))
        {
            NAS_MML_SetPsAttachAllowFlg(VOS_FALSE);
        }
    }

    g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg = GMM_FALSE;                         /* 清除标志                                 */
    g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg = GMM_FALSE;                        /* 清除标志                                 */
    g_GmmGlobalCtrl.ucSpecProc    = GMM_NULL_PROCEDURE;                         /* 清除正在进行的specific过程标志           */

    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_RcvAttachRejectMsg:INFO: specific procedure ended");

    g_GmmGlobalCtrl.ucFollowOnFlg = GMM_FALSE;                                  /* 清除fallowon标志                         */
    NAS_MML_SetPsServiceBufferStatusFlg(VOS_FALSE);

    return;
}


VOS_VOID Gmm_AttachRauAcceptCause16(VOS_VOID)
{
    if (NAS_MML_MS_MODE_PS_ONLY == NAS_MML_GetMsMode())
    {
        Gmm_ComStaChg(GMM_REGISTERED_NORMAL_SERVICE);
        g_GmmAttachCtrl.ucAttachAttmptCnt = 0;
        g_GmmRauCtrl.ucRauAttmptCnt = 0;
    }
    else
    {
        /* 调用状态公共处理  */
        Gmm_ComStaChg(GMM_REGISTERED_ATTEMPTING_TO_UPDATE_MM);

        /**************************************************************************
        3gpp 24.008 4.7.3,4.7.3.2.3.2 The GPRS attach attempt counter shall be reset
        and the routing area updating attempt counter shall be incremented  when:a
        combine GPRS attach procedure is completed for GPRS services only with cause
        #2, #16, #17 or #22;
        ***************************************************************************/
        g_GmmAttachCtrl.ucAttachAttmptCnt = 0;
        g_GmmRauCtrl.ucRauAttmptCnt++;

        if (GMM_ATTACH_RAU_ATTEMPT_MAX_CNT > g_GmmRauCtrl.ucRauAttmptCnt)
        {                                                                           /* RAU attempt counter没有达到最大          */
            Gmm_TimerStart(GMM_TIMER_T3311);                                        /* 启动Timer3311                            */
        }
        else
        {                                                                           /* RAU attempt counter达到最大              */
            Gmm_TimerStart(GMM_TIMER_T3302);                                        /* 启动Timer3302                            */
        }
    }

    return;
}

/*******************************************************************************
  Module   : Gmm_AttachRauAcceptCause2
  Function : ATTACH或RAU被接收时原因值为2的处理
  Input    : 无
  Output   : 无
  NOTE     : 无
  Return   : 无
  History  :
    1. 张志勇  2003.12.08  新规作成
*******************************************************************************/
VOS_VOID Gmm_AttachRauAcceptCause2(VOS_VOID)
{
    Gmm_ComStaChg(GMM_REGISTERED_NORMAL_SERVICE);                               /* 调用状态公共处理                         */
    g_GmmAttachCtrl.ucAttachAttmptCnt = 0;                                      /* attach attempt counter清0                */
}


VOS_VOID Gmm_SaveTimerValue(
                        VOS_UINT8 ucTimNam,                                         /* Timer名称                                */
                        VOS_UINT8 ucMsgByte                                         /* 消息中时长位置的字节                     */
                        )
{
    VOS_UINT8   ucUnit       = 0;                                                   /* 定义临时变量存储时长单位                 */
    VOS_UINT8   ucTimerValue = 0;                                                   /* 定义临时变量存储时长                     */
    VOS_UINT32  ulT3312OldValue = 0;                                                /* 定义临时变量存储T3312当前时长            */

    ulT3312OldValue = g_GmmTimerMng.aTimerInf[GMM_TIMER_T3312].ulTimerVal;

    ucUnit = (VOS_UINT8)(ucMsgByte >> 5);                                           /* 得到时长单位                             */
    ucTimerValue = (VOS_UINT8)(ucMsgByte & 0x1F);                                   /* 得到Timer Value                          */
    switch (ucUnit)
    {                                                                           /* 判断时长单位                             */
    case 0:                                                                     /* 2 seconds                                */
        g_GmmTimerMng.aTimerInf[ucTimNam].ulTimerVal
            = ucTimerValue * 2 * 1000;
        break;
    case 1:                                                                     /* 1 minute                                 */
        g_GmmTimerMng.aTimerInf[ucTimNam].ulTimerVal
            = ucTimerValue * 60 * 1000;
        break;
    case 2:                                                                     /* decihours                                */
        g_GmmTimerMng.aTimerInf[ucTimNam].ulTimerVal
            = ucTimerValue * 360 * 1000;
        break;
    case 7:                                                                     /* timer is deactivated                     */
        g_GmmTimerMng.aTimerInf[ucTimNam].ulTimerVal = 0;
        break;
    default:
        g_GmmTimerMng.aTimerInf[ucTimNam].ulTimerVal
            = ucTimerValue * 60 * 1000;
        break;
    }

    /* 24.008: 4.7.2.2
      In Iu mode, if the value of timer T3312 is received in a message without integrity protection
      and the indicated value is larger than the last received value,
      or the indicated value is "deactivated" or zero, the MS shall use the last received value.*/
    if (GMM_TIMER_T3312 == ucTimNam)
    {

        /* 如果当前在TD下，设置不需要检测PS域的SMC，则认为SMC已经收到了 */
        if ((VOS_TRUE == NAS_UTRANCTRL_IsUtranPsSmcNeeded())
         && (NAS_MML_RRC_INTEGRITY_PROTECT_ACTIVE != g_MmSubLyrShare.GmmShare.ucPsIntegrityProtect))
        {
            if ((g_GmmTimerMng.aTimerInf[GMM_TIMER_T3312].ulTimerVal > ulT3312OldValue)
                || (0 == g_GmmTimerMng.aTimerInf[GMM_TIMER_T3312].ulTimerVal))
            {
                if (0 != ulT3312OldValue)
                {                                                           /* 旧的3312值有效，更新为旧的值                 */
                    g_GmmTimerMng.aTimerInf[GMM_TIMER_T3312].ulTimerVal = ulT3312OldValue;
                }
                else
                {                                                           /* 旧的3312值无效，更新为3312的缺省值           */
                    g_GmmTimerMng.aTimerInf[GMM_TIMER_T3312].ulTimerVal = GMM_TIMER_T3312_VALUE;
                }

            }
        }
    }

    return;
}
VOS_VOID Gmm_AttachAcceptResolveIe(
                               GMM_MSG_RESOLVE_STRU *pAttachAcceptIe,           /* 指向GMM_MSG_RESOLVE_STRU结构的指针       */
                               NAS_MSG_FOR_PCLINT_STRU         *pMsg
                               )
{
    VOS_UINT8                           ucIeOffset = GMM_MSG_LEN_ATTACH_ACCEPT;  /* 定义临时变量存储存储IE的偏移量           */
    VOS_UINT8                           ucEndFlg   = GMM_FALSE;                  /* 初期化结束标志为FALSE                    */

    VOS_UINT16                          usIeOffsetAdapt;

    usIeOffsetAdapt         = 0;

    Gmm_SaveTimerValue(GMM_TIMER_T3312, pMsg->aucNasMsg[3]);                    /* 存储T3312时长                            */

    if (NAS_MML_NET_RAT_TYPE_WCDMA == NAS_MML_GetCurrNetRatType())
    {
        if (0 == ((pMsg->aucNasMsg[2]) & 0x08))
        {
            g_GmmGlobalCtrl.ucFopFlg = GMM_TRUE;
        }

        else
        {
            g_GmmGlobalCtrl.ucFopFlg = GMM_FALSE;
        }
    }

    for (; ((ucIeOffset<pMsg->ulNasMsgSize) && (GMM_FALSE == ucEndFlg));)
    {                                                                           /* 存储IE的偏移量小于空口消息的长度         */
        switch (pMsg->aucNasMsg[ucIeOffset])
        {                                                                       /* IEI                                      */
        case GMM_IEI_P_TMSI_SIGNATURE:                                          /* P-TMSI signature                         */
            if ((ucIeOffset + 4) > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"P-TMSI\" length error in ATTACH ACCEPT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE_FLG ) )
            {
                ucIeOffset += 4;
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE]
                = ucIeOffset;
            ucIeOffset += 4;
            break;
        case GMM_IEI_NEGOTIATED_READY_TIMER_VALUE:                              /* Negotiated READY timervalue              */
            if ((ucIeOffset + 2) > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"READY timer\" length error in ATTACH ACCEPT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_READY_TIMER_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_READY_TIMER_FLG ) )
            {
                ucIeOffset += 2;
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_READY_TIMER_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_READY_TIMER]
                = ucIeOffset;
            ucIeOffset += 2;
            break;
        case GMM_IEI_ALLOCATED_PTMSI:                                           /* Allocated P-TMSI                         */
            if ((ucIeOffset + 7) > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"P-TMSI\" length error in ATTACH ACCEPT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_PTMSI_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_PTMSI_FLG ) )
            {
                ucIeOffset += 7;
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_PTMSI_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_PTMSI]
                = ucIeOffset;
            ucIeOffset += 7;
            break;
        case GMM_IEI_MS_IDENTITY:                                               /* MS identity                              */
            if (GMM_MOBILE_ID_IMSI == (pMsg->aucNasMsg[ucIeOffset + 2] & 0x07))
            {                                                                   /* IE中是IMSI                               */
                if ((ucIeOffset + 10) > (VOS_UINT8)pMsg->ulNasMsgSize )
                {
                    /* IMSI存在时,即使长度不对,也需要处理 */
                    pAttachAcceptIe->usOptionalIeMask
                        |= GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG;
                    pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_MS_IDENTITY]
                        = ucIeOffset;
                    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"MS ID\" length error in ATTACH ACCEPT");
                    return;
                }
                if (GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG ==
                    (pAttachAcceptIe->usOptionalIeMask
                     & GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG ) )
                {
                    ucIeOffset += 10;
                    break;
                }
            }
            else if (GMM_MOBILE_ID_TMSI_PTMSI == (pMsg->aucNasMsg[ucIeOffset + 2] & 0x07))
            {                                                                   /* IE中是TMSI                               */
                if ((ucIeOffset + 7) > (VOS_UINT8)pMsg->ulNasMsgSize )
                {
                    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"MS ID\" length error in ATTACH ACCEPT");
                    return;
                }
                if (GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG ==
                    (pAttachAcceptIe->usOptionalIeMask
                     & GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG ) )
                {
                    ucIeOffset += 7;
                    break;
                }
            }
            else
            {
                ucIeOffset += pMsg->aucNasMsg[ucIeOffset + 1] + 2 ;
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_MS_IDENTITY]
                = ucIeOffset;

            ucIeOffset += pMsg->aucNasMsg[ucIeOffset + 1] + 2 ;
            break;
        case GMM_IEI_GMM_CAUSE:                                                 /* GMM cause                                */
            if ( (ucIeOffset + 2) > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"GMM cause\" length error in ATTACH ACCEPTT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_GMM_CAUSE_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_GMM_CAUSE_FLG ) )
            {
                ucIeOffset += 2;
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_GMM_CAUSE_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_GMM_CAUSE]
                = ucIeOffset;
            ucIeOffset += 2;
            break;
        case GMM_IEI_T3302_VALUE:                                               /* T3302 value                              */
            if ( (ucIeOffset + 3) > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"T3302\" length error in ATTACH ACCEPT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_T3302_VALUE_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_T3302_VALUE_FLG ) )
            {
                ucIeOffset += 3;
                break;
            }

            Gmm_SaveTimerValue(GMM_TIMER_T3302,
                               pMsg->aucNasMsg[ucIeOffset + 2]);                /* 存储T3302时长                            */
            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_T3302_VALUE_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_T3302_VALUE]
                = ucIeOffset;
            ucIeOffset += 3;
            break;
        case GMM_IEI_CELL_NOTIFICATION:                                         /* Cell Notification                        */
            if (GMM_ATTACH_ACCEPT_IE_CELL_NOTIFICATION_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_CELL_NOTIFICATION_FLG ) )
            {
                ucIeOffset += 1;
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_CELL_NOTIFICATION_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_CELL_NOTIFICATION]
                = ucIeOffset;
            ucIeOffset += 1;
            break;
        case GMM_IEI_EQUIVALENT_PLMNS:                                          /* Equivalent PLMNs                         */
            if ( (ucIeOffset + (2 + pMsg->aucNasMsg[ucIeOffset + 1]))
                 > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"Equivalent PLMNs\" length error in ATTACH ACCEPT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_EQUIVALENT_PLMN_FLG ==
                (pAttachAcceptIe->usOptionalIeMask
                 & GMM_ATTACH_ACCEPT_IE_EQUIVALENT_PLMN_FLG ) )
            {
                ucIeOffset += (2 + pMsg->aucNasMsg[ucIeOffset + 1]);
                break;
            }

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_EQUIVALENT_PLMN_FLG;
            pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_EQUIVALENT_PLMN]
                = ucIeOffset;
            ucIeOffset += (2 + pMsg->aucNasMsg[ucIeOffset + 1]);
            break;
        case GMM_IEI_EMERGENCY_NUMBER_LIST:                                     /* Emergency Number List                    */
            if ( (ucIeOffset + (2 + pMsg->aucNasMsg[ucIeOffset + 1]))
                 > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"emergency number\" length error in ATTACH ACCEPT");
                return;
            }
            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_EMERGENCY_NUMBER_LIST_FLG;

            usIeOffsetAdapt = ucIeOffset;
            NAS_MML_DecodeEmergencyNumList(&usIeOffsetAdapt, pMsg->aucNasMsg, (VOS_UINT16)pMsg->ulNasMsgSize);
            ucIeOffset      = (VOS_UINT8)usIeOffsetAdapt;

            break;

        case NAS_GMM_IEI_T3323_VALUE:
            if ( (ucIeOffset + 3) > (VOS_UINT8)pMsg->ulNasMsgSize )
            {
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAcceptResolveIe:WARNING: IE\"T332\" length error in ATTACH ACCEPT");
                return;
            }
            if (GMM_ATTACH_ACCEPT_IE_T3323_VALUE_FLG == (pAttachAcceptIe->usOptionalIeMask & GMM_ATTACH_ACCEPT_IE_T3323_VALUE_FLG) )
            {
                ucIeOffset += 3;
                break;
            }

            Gmm_SaveTimerValue(GMM_TIMER_T3323,
                               pMsg->aucNasMsg[ucIeOffset + 2]);

            pAttachAcceptIe->usOptionalIeMask
                |= GMM_ATTACH_ACCEPT_IE_T3323_VALUE_FLG;

            pAttachAcceptIe->aucIeOffset[NAS_GMM_ATTACH_ACCEPT_IE_T3323_VALUE] = ucIeOffset;
            ucIeOffset += 3;
            break;
        default:
            if(GMM_IEI_NETWORK_FEATURE_SUPPORT == (pMsg->aucNasMsg[ucIeOffset] & 0xF0))
            { /* Network feature support                  */
                ucIeOffset += 1;
            }
            else if (GMM_IEI_REQUESTED_MS_INFORMATION == (pMsg->aucNasMsg[ucIeOffset] & 0xF0))
            {/* Requested MS information                 */
                pAttachAcceptIe->usOptionalIeMask
                    |= GMM_ATTACH_ACCEPT_IE_REQUESTED_MS_INFORMATION_FLG;
                pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_REQUESTED_MS_INFORMATION]
                    = ucIeOffset;
                ucIeOffset += 1;
            }
            /* TV类型的IE, reference 27.007 11.2.4 */
            else if(GMM_IEI_FORMAT_T_TYPE == (pMsg->aucNasMsg[ucIeOffset] & GMM_IEI_FORMAT_TYPE_INFO))
            {
                ucIeOffset += 1;
            }
            /* TLV类型的IE */
            else if (ucIeOffset + 2 + pMsg->aucNasMsg[ucIeOffset + 1] < pMsg->ulNasMsgSize)
            {
                ucIeOffset += (2 + pMsg->aucNasMsg[ucIeOffset + 1]);
            }
            else
            {
                ucEndFlg = GMM_TRUE;
            }
            break;
        }
    }

    return;
}
VOS_VOID Gmm_AttachAcceptGprsOnly(
    GMM_MSG_RESOLVE_STRU                *pAttachAcceptIe,
    NAS_MSG_FOR_PCLINT_STRU             *pMsg,
    VOS_BOOL                            bTlliUpdateFlg,
    VOS_UINT8                           ucInterRatInfoFlg
)
{
    NAS_MML_REG_FAIL_CAUSE_ENUM_UINT16  enGmmCause;                                                    /* 定义临时变量存储GMM CAUSE                */

    VOS_UINT8                           ucDrxLength;
    VOS_UINT8                           ucSysInfoDrxLength;

    enGmmCause = NAS_MML_REG_FAIL_CAUSE_NULL;

    if (GMM_ATTACH_ACCEPT_IE_GMM_CAUSE_FLG
        == (pAttachAcceptIe->usOptionalIeMask
        & GMM_ATTACH_ACCEPT_IE_GMM_CAUSE_FLG))
    {
        enGmmCause = pMsg->aucNasMsg
            [pAttachAcceptIe->aucIeOffset[GMM_ATTACH_ACCEPT_IE_GMM_CAUSE] + 1]; /* 得到GMM CAUSE                            */
    }

    /* R11升级:   24008 4.7.3.2.3.2    Other GMM causevalues and the case that no GMM cause IE
       was received are considered as abnormal cases. The combined attach procedure shall be
       considered as failed for non-GPRS services */
    if ((GMM_ATTACH_WHILE_IMSI_ATTACHED == g_GmmGlobalCtrl.ucSpecProc)
     || (GMM_ATTACH_COMBINED            == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                       /* 当前的specific过程是COMBINED ATTACH      */
        if ((NAS_MML_REG_FAIL_CAUSE_MSC_UNREACHABLE         != enGmmCause)
         && (NAS_MML_REG_FAIL_CAUSE_NETWORK_FAILURE         != enGmmCause)
         && (NAS_MML_REG_FAIL_CAUSE_PROCEDURE_CONGESTION    != enGmmCause)
         && (NAS_MML_REG_FAIL_CAUSE_IMSI_UNKNOWN_IN_HLR     != enGmmCause))
        {                                                                  /* 其他原因值                               */

            enGmmCause = NAS_MML_REG_FAIL_CAUSE_MSC_UNREACHABLE;
        }
    }

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_NORMAL       == g_GmmGlobalCtrl.ucSpecProc)
        || ((VOS_FALSE == bTlliUpdateFlg) && (GMM_NULL_PROCEDURE == g_GmmGlobalCtrl.ucSpecProc)))
    {                                                                           /* 当前的specific过程是NORMAL ATTACH        */
        if ((GMM_TRUE == g_GmmAttachCtrl.ucSmCnfFlg) && (GMM_FALSE == ucInterRatInfoFlg))
        {
            /* 如果Attach Accept消息中带有Request Ms Info IE,则GMM需要向WRR发送消息，请求
               InterRatHandover信息，等待WRR回复后，向网络回复Attach Complete消息，整个Attach
               流程才算结束，在此之前，不向SM回复Est_Cnf消息，等流程结束后再通知SM. */

            Gmm_SndSmEstablishCnf(GMM_SM_EST_SUCCESS, GMM_SM_CAUSE_SUCCESS);

            g_GmmAttachCtrl.ucSmCnfFlg = GMM_FALSE;
        }

        Gmm_ComStaChg(GMM_REGISTERED_NORMAL_SERVICE);                           /* 调用状态公共处理                         */
        g_GmmAttachCtrl.ucAttachAttmptCnt = 0;                                  /* attach attempt counter清0                */
        g_GmmRauCtrl.ucRauAttmptCnt = 0;                                        /* routing area updating attempt counter清0 */

        NAS_GMM_SndMmGmmActionResultInd(GMMMM_ACTION_SUCCESS,
                                        NAS_MML_REG_FAIL_CAUSE_NULL);


        /* 向MMC发送PS注册结果 */
        NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                     GMM_MMC_ACTION_RESULT_SUCCESS,
                                     NAS_MML_REG_FAIL_CAUSE_NULL);

        /* 事件上报 */
        NAS_EventReport(WUEPS_PID_GMM,
                        NAS_OM_EVENT_ATTACH_SUCC,
                        VOS_NULL_PTR,
                        NAS_OM_EVENT_NO_PARA
                        );

        /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
        if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                 & GMM_WAIT_PS_ATTACH))
        {
            NAS_GMM_SndMmcAttachCnf();
        }
        g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;

    }
    else
    {                                                                           /* combined流程                             */
        if (NAS_MML_REG_FAIL_CAUSE_IMSI_UNKNOWN_IN_HLR == enGmmCause)
        {                                                                       /* 原因值为2                                */
            if (GMM_TRUE == g_GmmAttachCtrl.ucSmCnfFlg)
            {                                                                   /* ATTACH过程是由SM触发的                   */
                Gmm_SndSmEstablishCnf(GMM_SM_EST_SUCCESS, GMM_SM_CAUSE_SUCCESS);  /* 通知SM GMM注册失败                       */

                g_GmmAttachCtrl.ucSmCnfFlg = GMM_FALSE;                         /* 清ucSmCnfFlg标志                         */
            }
            Gmm_AttachRauAcceptCause2();                                        /* 调用原因值2处理函数                      */
            NAS_GMM_SndMmCombinedAttachAccept(GMMMM_ATTACH_RESLUT_PS_ONLY,
                                              enGmmCause,
                                              pMsg,
                                              pAttachAcceptIe);                    /* 发送MMCGMM_COMBINED_ATTACH_ACCEPTED给MMC */

            /* 向MMC发送PS注册结果 */


            NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                         GMM_MMC_ACTION_RESULT_SUCCESS,
                                         enGmmCause);

            /* 更新ATTACH记数器 */
            if(5 <= g_GmmAttachCtrl.ucAttachAttmptCnt)
            {
                g_GmmAttachCtrl.ucAttachAttmptCnt = 0;
            }

            /* 事件上报 */
            NAS_EventReport(WUEPS_PID_GMM,
                            NAS_OM_EVENT_ATTACH_SUCC,
                            VOS_NULL_PTR,
                            NAS_OM_EVENT_NO_PARA);

            /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
            if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                     & GMM_WAIT_PS_ATTACH))
            {
                NAS_GMM_SndMmcAttachCnf();
            }
            g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
        }
        else
        {                                                                       /* 原因值为16,17,22                         */
            if (GMM_TRUE == g_GmmAttachCtrl.ucSmCnfFlg)
            {                                                                   /* ATTACH过程是由SM触发的                   */
                Gmm_SndSmEstablishCnf(GMM_SM_EST_SUCCESS, GMM_SM_CAUSE_SUCCESS); /* 通知SM GMM注册失败                       */

                g_GmmAttachCtrl.ucSmCnfFlg = GMM_FALSE;                         /* 清ucSmCnfFlg标志                         */
            }
            Gmm_AttachRauAcceptCause16();                                       /* 调用原因值16，17，22处理函数             */
            if((NAS_MML_NET_RAT_TYPE_WCDMA == NAS_MML_GetCurrNetRatType())
                ||(5 > g_GmmRauCtrl.ucRauAttmptCnt))
            {
                NAS_GMM_SndMmCombinedAttachAccept(GMMMM_ATTACH_RESLUT_PS_ONLY,
                                                  enGmmCause,
                                                  pMsg,
                                                  pAttachAcceptIe);             /* 发送MMCGMM_COMBINED_ATTACH_ACCEPTED给MMC */


                /* 向MMC发送PS注册结果 */

                NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                             GMM_MMC_ACTION_RESULT_SUCCESS,
                                             enGmmCause);

                /* 更新ATTACH记数器 */
                if(5 <= g_GmmAttachCtrl.ucAttachAttmptCnt)
                {
                    g_GmmAttachCtrl.ucAttachAttmptCnt = 0;
                }

                /* 事件上报 */
                NAS_EventReport(WUEPS_PID_GMM,
                                NAS_OM_EVENT_ATTACH_SUCC,
                                VOS_NULL_PTR,
                                NAS_OM_EVENT_NO_PARA);

                /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
                if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                         & GMM_WAIT_PS_ATTACH))
                {
                    NAS_GMM_SndMmcAttachCnf();
                }
                g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
            }
        }
    }

    ucDrxLength        = NAS_MML_GetUeUtranPsDrxLen();
    ucSysInfoDrxLength = NAS_MML_GetWSysInfoDrxLen();
    /* BEGIN: Modified by liurui id:40632, 2006/6/14   PN:A32D04282 */
    if (GMM_TRUE == GMM_IsCasGsmMode())
    {
        Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_DRX);                          /* 通知GAS DRX参数变更 */
    }
    else if ((NAS_MML_NET_RAT_TYPE_WCDMA == NAS_MML_GetCurrNetRatType())
         &&  (ucSysInfoDrxLength != ucDrxLength) )
    /* END:   Modified by liurui id:40632, 2006/6/14   PN:A32D04282 */
    {                                                                           /* DRX length参数变更                       */
        Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_DRX);                          /* 通知RRC DRX参数变更                      */
    }
    else
    {
    }

    return;
}


VOS_VOID Gmm_AttachAcceptCombined(
                              GMM_MSG_RESOLVE_STRU  *pAttachAcceptIe,           /* 指向GMM_MSG_RESOLVE_STRU结构的指针       */
                              NAS_MSG_FOR_PCLINT_STRU          *pMsg
                              )
{
    VOS_UINT8                           ucDrxLength;
    VOS_UINT8                           ucSysInfoDrxLength;

    ucDrxLength                       = NAS_MML_GetUeUtranPsDrxLen();
    ucSysInfoDrxLength                = NAS_MML_GetWSysInfoDrxLen();
	g_GmmAttachCtrl.ucAttachAttmptCnt = 0;                                      /* attach attempt counter清0                */
    g_GmmRauCtrl.ucRauAttmptCnt       = 0;                                      /* routing area updating attempt counter清0 */

	/* BEGIN: Modified by liurui id:40632, 2006/6/14   PN:A32D04282 */
    if (GMM_TRUE == GMM_IsCasGsmMode())
    {
        Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_DRX);                          /* 通知GAS DRX参数变更 */
    }
    else if ((NAS_MML_NET_RAT_TYPE_WCDMA == NAS_MML_GetCurrNetRatType())
          && (ucSysInfoDrxLength != ucDrxLength))
    /* END:   Modified by liurui id:40632, 2006/6/14   PN:A32D04282 */
    {                                                                           /* DRX length参数变更                       */
        Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_DRX);                          /* 通知RRC DRX参数变更                      */
    }
    else
    {
    }
    Gmm_ComStaChg(GMM_REGISTERED_NORMAL_SERVICE);                               /* 调用状态公共处理                         */

    /* 向MMC发送PS注册结果 */
    NAS_GMM_SndMmcPsRegResultInd(GMM_MMC_ACTION_ATTACH,
                                 GMM_MMC_ACTION_RESULT_SUCCESS,
                                 NAS_MML_REG_FAIL_CAUSE_NULL);

    /* 事件上报 */
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_SUCC,
                    VOS_NULL_PTR,
                    NAS_OM_EVENT_NO_PARA
                    );

    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;

    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_NORMAL       == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* 当前的specific过程是NORMAL ATTACH        */
        NAS_GMM_SndMmGmmActionResultInd(
                      GMMMM_ACTION_SUCCESS,
                      NAS_MML_REG_FAIL_CAUSE_NULL);  /* 发送MMCGMM_GMM_ACTION_RESULT_IND给MMC    */
    }
    else
    {                                                                           /* combined流程                             */
        NAS_GMM_SndMmCombinedAttachAccept(GMMMM_ATTACH_RESLUT_COMBINED,
                                          NAS_MML_REG_FAIL_CAUSE_NULL,
                                          pMsg,
                                          pAttachAcceptIe);                        /* 发送MMCGMM_COMBINED_ATTACH_ACCEPTED给MMC */

        /* 更新ATTACH记数器 */
        if(5 <= g_GmmAttachCtrl.ucAttachAttmptCnt)
        {
            g_GmmAttachCtrl.ucAttachAttmptCnt = 0;
        }
    }

    return;
}
VOS_VOID Gmm_AttachAcceptHandle_Emergency_Num_Handling(
                                   GMM_MSG_RESOLVE_STRU        AttachAcceptIe
                            )
{
    VOS_UINT8           i,j;

    NAS_MML_EMERGENCY_NUM_LIST_STRU    *pstEmergencyNumList;
    pstEmergencyNumList     = NAS_MML_GetEmergencyNumList();

    if (GMM_ATTACH_ACCEPT_IE_EMERGENCY_NUMBER_LIST_FLG
        == (AttachAcceptIe.usOptionalIeMask
                & GMM_ATTACH_ACCEPT_IE_EMERGENCY_NUMBER_LIST_FLG))
    {                                                                           /* 消息中包含紧急呼列表                     */
        pstEmergencyNumList->ulMcc =
            g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId.aucMccDigit[0]
            | (g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId.aucMccDigit[1] << 8)
            | (g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId.aucMccDigit[2] << 16);

        Gmm_SndCcEmergencyNumberList(pstEmergencyNumList);
    }
    else if (0 != pstEmergencyNumList->ucEmergencyNumber)
    {                                                                           /* 消息中包不含紧急呼列表但之前存储了紧急呼列表 */
        if (((VOS_UINT8)(pstEmergencyNumList->ulMcc & 0x0F)
            != g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId.aucMccDigit[0])
            || ((VOS_UINT8)((pstEmergencyNumList->ulMcc >> 8) & 0x0F)
            != g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId.aucMccDigit[1])
            || ((VOS_UINT8)((pstEmergencyNumList->ulMcc >> 16) & 0x0F)
            != g_GmmGlobalCtrl.SysInfo.Rai.Lai.PlmnId.aucMccDigit[2]))
        {
            pstEmergencyNumList->ulMcc = 0xFFFFFFFF;

            for (i = 0; i < pstEmergencyNumList->ucEmergencyNumber; i++)
            {
                pstEmergencyNumList->aucEmergencyList[i].ucCategory = 0xFF;

                for (j = 0; j < NAS_MML_EMERGENCY_NUM_MAX_LEN; j++)
                {
                    pstEmergencyNumList->aucEmergencyList[i].aucEmcNum[j] = 0xFF;
                }
            }

            pstEmergencyNumList->ucEmergencyNumber = 0;
        }
        Gmm_SndCcEmergencyNumberList(pstEmergencyNumList);
    }
    else
    {
    }

    return;
}


VOS_VOID Gmm_AttachAcceptHandle_PTMSI_Handling(
                                   NAS_MSG_FOR_PCLINT_STRU    *pMsg,
                                   GMM_MSG_RESOLVE_STRU        AttachAcceptIe,
                                   VOS_UINT8                  *pucSndCompleteFlg
                            )
{
    if (GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE_FLG
        == (AttachAcceptIe.usOptionalIeMask
            & GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE_FLG))
    {                                                                           /* 空口消息中有P-TMSI signature             */
        /* 存储P-TMSI signature         */
        NAS_MML_SetUeIdPtmsiSignature(&(pMsg->aucNasMsg[AttachAcceptIe.aucIeOffset[GMM_ATTACH_ACCEPT_IE_PTMSI_SIGNATURE] + 1]));
        g_GmmGlobalCtrl.UeInfo.UeId.ucUeIdMask |= GMM_UEID_P_TMSI_SIGNATURE;    /* 设置UE ID存在标志                        */
    }
    else
    {                                                                           /* 空口消息中没有P-TMSI signature           */
        /* 删除P-TMSI signature         */
        NAS_MML_InitUeIdPtmsiSignatureInvalid();

        g_GmmGlobalCtrl.UeInfo.UeId.ucUeIdMask &= ~GMM_UEID_P_TMSI_SIGNATURE;   /* 清UE ID存在标志                          */
    }
    if (GMM_ATTACH_ACCEPT_IE_PTMSI_FLG
        == (AttachAcceptIe.usOptionalIeMask & GMM_ATTACH_ACCEPT_IE_PTMSI_FLG))
    {                                                                           /* 空口消息中有P-TMSI                       */
        /* 存储P-TMSI                   */
        NAS_MML_SetUeIdPtmsi(&(pMsg->aucNasMsg[AttachAcceptIe.aucIeOffset[GMM_ATTACH_ACCEPT_IE_PTMSI] + 3]));

        g_GmmGlobalCtrl.UeInfo.UeId.ucUeIdMask |= GMM_UEID_P_TMSI;              /* 设置UE ID存在标志                        */
        Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_PTMSI_RAI);                    /* 通知RRC NAS层信息变化                    */
        *pucSndCompleteFlg = GMM_TRUE;                                            /* 设定需要发送RAU COMPLETE消息             */

#if (FEATURE_ON == FEATURE_PTM)
        /* 工程菜单打开后，PTMSI发生改变需要上报给OM */
        NAS_GMM_SndAcpuOmChangePtmsi();
#endif
    }
    else
    {
        /* 2G网络下ATTACH完成后必须向GAS通知PTMSI值 */
        if (GMM_TRUE == GMM_IsCasGsmMode())
        {
            Gmm_SndRrmmNasInfoChangeReq(RRC_NAS_MASK_PTMSI_RAI);
        }
    }

    return;
}
VOS_VOID Gmm_AttachAcceptHandle_REQUESTED_MS_INFORMATION_IE_Handling(
    NAS_MSG_FOR_PCLINT_STRU            *pMsg,
    GMM_MSG_RESOLVE_STRU                AttachAcceptIe,
    VOS_UINT8                          *pucSndCompleteFlg,
    VOS_UINT8                          *pucInterRatInfoFlg
)
{
    NAS_MML_PLMN_RAT_PRIO_STRU         *pstRatList;

#if   (FEATURE_ON == FEATURE_LTE)
    VOS_UINT32                          ulSurpportLteFlag;
#endif

    pstRatList = NAS_MML_GetMsPrioRatList();

#if   (FEATURE_ON == FEATURE_LTE)
    ulSurpportLteFlag = NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_LTE);
#endif

    /* 双模下才可获得该信息 */

    if ( ( VOS_FALSE == NAS_MML_IsNetRatSupported(NAS_MML_NET_RAT_TYPE_WCDMA)
#if   (FEATURE_ON == FEATURE_LTE)
      && (VOS_FALSE == ulSurpportLteFlag)
#endif
      )
      || ( 1 == pstRatList->ucRatNum ))
    {
        return;
    }



    if (GMM_ATTACH_ACCEPT_IE_REQUESTED_MS_INFORMATION_FLG
        == (AttachAcceptIe.usOptionalIeMask
            & GMM_ATTACH_ACCEPT_IE_REQUESTED_MS_INFORMATION_FLG))
    {                                                                           /* 消息中包含 IE:Requested MS information   */
        g_GmmInterRatInfoCtrl.ucRauCmpWaitInterRatCnfMsg = VOS_FALSE;

        /* 平台支持UTRAN */
        if (VOS_TRUE == NAS_MML_IsPlatformSupportUtran())
        {
            if (0x08 == (0x08 & (pMsg->aucNasMsg
                    [AttachAcceptIe.aucIeOffset[GMM_ATTACH_ACCEPT_IE_REQUESTED_MS_INFORMATION]])))
            {                                                                   /* 消息中指示 Inter RAT information container IE requested */
                *pucSndCompleteFlg = GMM_TRUE;                                        /* 设定需要发送RAU COMPLETE消息             */
                *pucInterRatInfoFlg = GMM_TRUE;
            }
        }

#if   (FEATURE_ON == FEATURE_LTE)
        /* 平台支持LTE */
        if (VOS_TRUE == NAS_MML_IsPlatformSupportLte())
        {
            if (0x04 == (0x04 & (pMsg->aucNasMsg
                                            [AttachAcceptIe.aucIeOffset[GMM_ATTACH_ACCEPT_IE_REQUESTED_MS_INFORMATION]])))
            {
                *pucSndCompleteFlg = GMM_TRUE;                                        /* 设定需要发送RAU COMPLETE消息             */
                g_GmmInterRatInfoCtrl.ucEutranInterRatInfoRequestedFlag = VOS_TRUE;
            }
        }
#endif
    }

    return;
}
VOS_UINT8 Gmm_AttachAcceptHandle_Attach_Result_IE_Handling(
                                   NAS_MSG_FOR_PCLINT_STRU    *pMsg,
                                   GMM_MSG_RESOLVE_STRU        AttachAcceptIe,
                                   VOS_UINT8                   ucUpdateStaPre,
                                   VOS_UINT8                  *pucSndCompleteFlg,
                                   VOS_BOOL                    bTlliUpdateFlg,
                                   VOS_UINT8                   ucInterRatInfoFlg
                            )
{

    if (GMM_COMBINED_GPRS_IMSI_ATTACHED == (pMsg->aucNasMsg[2] & 0x03))
    {
        if ((GMM_TRUE == g_GmmAttachCtrl.ucSmCnfFlg) && (GMM_FALSE == ucInterRatInfoFlg))
        {
            /* 如果Attach Accept消息中带有Request Ms Info IE,则GMM需要向WRR发送消息，请求
               InterRatHandover信息，等待WRR回复后，向网络回复Attach Complete消息，整个Attach
               流程才算结束，在此之前，不向SM回复Est_Cnf消息，等流程结束后再通知SM. */

            Gmm_SndSmEstablishCnf(GMM_SM_EST_SUCCESS, GMM_SM_CAUSE_SUCCESS);

            g_GmmAttachCtrl.ucSmCnfFlg = GMM_FALSE;
        }
        g_MmSubLyrShare.GmmShare.ucGsAssociationFlg = VOS_TRUE;
        Gmm_AttachAcceptCombined(&AttachAcceptIe, pMsg);                        /* 调用函数处理combined attached            */
        if (GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG
            == (AttachAcceptIe.usOptionalIeMask
            & GMM_ATTACH_ACCEPT_IE_MS_IDENTITY_FLG))
        {                                                                       /* 该IE存在                                 */
            if (GMM_MOBILE_ID_TMSI_PTMSI
                == (GMM_MOBILE_ID_TMSI_PTMSI &
                pMsg->aucNasMsg[AttachAcceptIe.aucIeOffset
                [GMM_ATTACH_ACCEPT_IE_MS_IDENTITY] + 2]))
            {                                                                   /* IE_MS_IDENTITY中携带TMSI                 */
                *pucSndCompleteFlg = GMM_TRUE;                                    /* 设定需要发送RAU COMPLETE消息             */
            }
        }
    }
    else
    {                                                                           /* Attach结果是GPRS attached only           */
        Gmm_AttachAcceptGprsOnly(&AttachAcceptIe, pMsg, bTlliUpdateFlg, ucInterRatInfoFlg);        /* 调用函数处理GPRS only attached           */
        g_MmSubLyrShare.GmmShare.ucGsAssociationFlg = VOS_FALSE;

    }

    return GMM_TRUE;
}


VOS_VOID Gmm_AttachAcceptHandle_ucSndCompleteFlg_Handling(
    VOS_UINT8                     ucSndCompleteFlg,
    VOS_UINT8                     ucInterRatInfoFlg,
    VOS_UINT8                     ucReadychangFLG
                            )
{
    NAS_MSG_STRU    *pSendNasMsg = VOS_NULL_PTR;
    VOS_UINT32      ulRst;

    if (VOS_TRUE == ucSndCompleteFlg)
    {
        /* 需要向WRR获取InterRatInfo信息 */
        if (VOS_TRUE == ucInterRatInfoFlg)
        {
            ulRst = NAS_GMM_SndRrmmInterRatHandoverInfoReq();

            if (VOS_OK == ulRst)
            {
                g_GmmInterRatInfoCtrl.ucAttachCmpWaitInterRatCnfMsg = VOS_TRUE;
                g_GmmInterRatInfoCtrl.ucInterRatCnfMsgRcvdFlg       = VOS_FALSE;
                g_GmmInterRatInfoCtrl.ucDataLen                     = 0;
            }
            else
            {
                ucInterRatInfoFlg = VOS_FALSE;
            }
        }

        /* 不需要向WRR获取InterRatInfo信息, 或者获取信息失败时，直接回复网侧
           Attach Complete消息 */
        if (VOS_FALSE == ucInterRatInfoFlg)
        {
            g_GmmInterRatInfoCtrl.ucAttachCmpWaitInterRatCnfMsg = VOS_FALSE;

            pSendNasMsg = Gmm_AttachCompleteMsgMake();
            Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH, pSendNasMsg);
        }

        if (GMM_TRUE == GMM_IsCasGsmMode())
        {
            /* attach结束后，只启动READY TIMER但不通知LLC cell update */
            gstGmmCasGlobalCtrl.ucCellUpdateFlg = VOS_FALSE;

            if ( VOS_TRUE == ucReadychangFLG )
            {
                NAS_GMM_SndGasInfoChangeReq(NAS_GSM_MASK_READY_TIMER);
            }

            GMM_InitCellUpdate(ucReadychangFLG);       /* CELL update */
        }
    }
    else
    {
        if ( GMM_TRUE == GMM_IsCasGsmMode() )
        {
            if ( VOS_TRUE == ucReadychangFLG )
            {

                NAS_GMM_SndGasInfoChangeReq(NAS_GSM_MASK_READY_TIMER);

                gstGmmCasGlobalCtrl.ucCellUpdateFlg = VOS_TRUE;

                /* 根据24.008 4.7.2.1.1 If a new READY timer value is negotiated,
                the MS shall upon the reception of the ATTACH ACCEPT or ROUTING
                AREA UPDATE ACCEPT message perform a initial cell update
                (either by transmitting a LLC frame or, if required, a ATTACH
                COMPLETE or ROUTING AREA UPDATE COMPLETE message), in order to
                apply the new READY timer value immediately. If both the network
                and the MS supports the Cell Notification, the initial cell
                update shall useany LLC frame except the LLC NULL frame. */
                GMM_InitCellUpdate(ucReadychangFLG);
            }
        }
    }

    return;
}


VOS_VOID Gmm_AttachAcceptHandle_TLLI_Handling(
                                   VOS_BOOL                bTlliUpdateFlg
                            )
{
    VOS_UINT32                  ulNewTlli = 0;

    if ((GMM_TRUE==GMM_IsCasGsmMode())
      &&(GMM_UEID_P_TMSI==(g_GmmGlobalCtrl.UeInfo.UeId.ucUeIdMask
         & GMM_UEID_P_TMSI))
       )
    {
        if (VOS_TRUE == bTlliUpdateFlg )
        {
            GMM_CharToUlong(&ulNewTlli,
                            NAS_MML_GetUeIdPtmsi());                            /* 新的PTMSI */
            GMM_CreateNewTlli(&ulNewTlli, GMM_LOCAL_TLLI);                      /* 生成新TLLI */
            if ( GMM_FALSE == GMM_AssignNewTlli(ulNewTlli, GMM_LOCAL_TLLI) )
            {
                GMM_LOG_WARN("Gmm_AttachAcceptHandle():Error: new TLLI assigned unsuccessfully!");
            }
            else
            {
                Gmm_TimerStart(GMM_TIMER_PROTECT_OLD_TLLI);
            }
        }
        else
        {
            Gmm_TimerStop(GMM_TIMER_PROTECT_OLD_TLLI);
            Gmm_TimerStart(GMM_TIMER_PROTECT_OLD_TLLI);
        }
    }

    return;
}


VOS_VOID Gmm_AttachAcceptHandle(
                            NAS_MSG_FOR_PCLINT_STRU *pMsg,
                            VOS_BOOL                bTlliUpdateFlg
                            )
{
    VOS_UINT8                           ucUpdateStaPre  = 0;                                                /* 定义临时变量存储原更新状态               */
    GMM_MSG_RESOLVE_STRU                AttachAcceptIe;                                 /* 定义临时变量                             */
    VOS_UINT8                           ucSndCompleteFlg = GMM_FALSE;                   /* 是否发送RAU COMPLETE消息                 */
    VOS_UINT8                           ucReadychangFLG = GMM_FALSE;
    VOS_UINT8                           ucInterRatInfoFlg = GMM_FALSE;
    VOS_UINT8                           ucRst;

#if   (FEATURE_ON == FEATURE_LTE)
    NAS_MML_RPLMN_CFG_INFO_STRU        *pstRplmnCfgInfo = VOS_NULL_PTR;
#endif

    Gmm_MemSet(&AttachAcceptIe, 0, sizeof(GMM_MSG_RESOLVE_STRU));               /* 临时变量清零处理                         */
    ucUpdateStaPre = NAS_MML_GetPsUpdateStatus();
    NAS_MML_SetPsUpdateStatus(NAS_MML_ROUTING_UPDATE_STATUS_UPDATED);             /* 置更新状态为GU1                          */
    Gmm_AttachAcceptResolveIe(&AttachAcceptIe, pMsg);                           /* 调用函数解析ATTACH ACCEPT消息的IE        */

#if   (FEATURE_ON == FEATURE_LTE)
     pstRplmnCfgInfo = NAS_MML_GetRplmnCfg();

     if ( NAS_MML_TIN_TYPE_PTMSI != pstRplmnCfgInfo->enTinType)
     {
         NAS_GMM_WriteTinInfoNvim(NAS_MML_TIN_TYPE_PTMSI, pstRplmnCfgInfo->aucLastImsi);
     }
#endif


    Gmm_AttachAcceptHandle_REQUESTED_MS_INFORMATION_IE_Handling(pMsg,
                                                                AttachAcceptIe,
                                                               &ucSndCompleteFlg,
                                                               &ucInterRatInfoFlg);

    Gmm_AttachAcceptHandle_Emergency_Num_Handling(AttachAcceptIe);


    /* report network feature information in UMTS. According to VoLTE SRS document,
     * In utran network, the related capability information reported is not supported
     * now.
     */
    NAS_GMM_SndMmcNetworkCapabilityInfoInd(GMM_MMC_NW_EMC_BS_NOT_SUPPORTED,
                                           NAS_MML_NW_IMS_VOICE_NOT_SUPPORTED,
                                           GMM_MMC_LTE_CS_CAPBILITY_NOT_SUPPORTED);


    ucRst = Gmm_AttachAcceptHandle_Attach_Result_IE_Handling(pMsg,
                                                             AttachAcceptIe,
                                                             ucUpdateStaPre,
                                                             &ucSndCompleteFlg,
                                                             bTlliUpdateFlg,
                                                             ucInterRatInfoFlg);
    if(GMM_FALSE == ucRst)
    {
        return;
    }

    Gmm_AttachAcceptHandle_PTMSI_Handling(pMsg,
                                          AttachAcceptIe,
                                         &ucSndCompleteFlg);

    NAS_GMM_AttachAcceptHandle_UpdateEPlmn(&AttachAcceptIe, pMsg);

    /* 2G模式计算新的local TLLI值 */
    Gmm_AttachAcceptHandle_TLLI_Handling(bTlliUpdateFlg);

    if (GMM_ATTACH_ACCEPT_IE_READY_TIMER_FLG
        == (AttachAcceptIe.usOptionalIeMask & GMM_ATTACH_ACCEPT_IE_READY_TIMER_FLG))
    {
        ucReadychangFLG = GMM_TRUE;
        GMM_SaveReadyTimerValue(
            pMsg->aucNasMsg[AttachAcceptIe.aucIeOffset\
                [GMM_ATTACH_ACCEPT_IE_READY_TIMER] + 1]);                       /* 保存READY TIMER协商值 */
    }
    else
    {
        if ( GMM_TRUE == gstGmmCasGlobalCtrl.ucReadyTimerNgtFlg )
        {
            ucReadychangFLG = GMM_TRUE;
            GMM_SaveReadyTimerValue( GMM_REQUESTED_READY_TIMER_VALUE );
            gstGmmCasGlobalCtrl.ucReadyTimerNgtFlg = GMM_FALSE;
        }
    }

    if (GMM_ATTACH_ACCEPT_IE_CELL_NOTIFICATION_FLG
        == (AttachAcceptIe.usOptionalIeMask
            & GMM_ATTACH_ACCEPT_IE_CELL_NOTIFICATION_FLG))
    {
        gstGmmCasGlobalCtrl.ucCellNotifyFlg = GMM_TRUE;     /* cell notificaion有效标志 */
    }
    else
    {
        gstGmmCasGlobalCtrl.ucCellNotifyFlg = GMM_FALSE;     /* cell notificaion无效标志 */
    }

    /* V200R001 Add End: */

    Gmm_SndAgentUsimUpdateFileReq(GMM_USIM_FILE_PS_LOC_INFO);                   /* 发送AGENT_USIM_UPDATE_FILE_REQ给AGENT    */

    Gmm_AttachAcceptHandle_ucSndCompleteFlg_Handling(ucSndCompleteFlg,
                                                     ucInterRatInfoFlg,
                                                     ucReadychangFLG);

    if (GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
    {                                                                           /* CS通信中的NORMAL ATTACH                  */
        if (GMM_FALSE == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg)
        {                                                                       /* CS不在通信中                             */
            g_GmmGlobalCtrl.ucSpecProc = GMM_NULL_PROCEDURE;                    /* 清除当前进行的specific 流程标志          */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachAcceptHandle:INFO: specific procedure ended");
            Gmm_RoutingAreaUpdateInitiate(GMM_COMBINED_RALAU_WITH_IMSI_ATTACH); /* 调用RAU流程                              */
        }
        else
        {                                                                       /* CS通信中                                 */
            g_GmmGlobalCtrl.ucSpecProc = GMM_NULL_PROCEDURE;                    /* 清除当前进行的specific 流程标志          */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachAcceptHandle:INFO: specific procedure ended");
            g_GmmGlobalCtrl.ucSpecProcInCsTrans = GMM_ATTACH_NORMAL;            /* 存储CS通信中进行specific流程标志         */
        }
    }
    else
    {
        g_GmmGlobalCtrl.ucSpecProc = GMM_NULL_PROCEDURE;                        /* 清除当前进行的specific 流程标志          */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachAcceptHandle:INFO: specific procedure ended");
    }


    return;
}
VOS_VOID Gmm_SndAttachReq(VOS_VOID)
{
    NAS_MSG_STRU    *pSendNasMsg = VOS_NULL_PTR;                                /* 定义NAS_MSG_STRU类型的指针变量           */
    VOS_UINT8        ucAttachType;

    /* BEGIN: Added by liurui id:40632, 2006/4/5   PN:A32D02352 */
    GMM_CasFsmStateChangeTo(GMM_REGISTERED_INITIATED);
    /* END:   Added by liurui id:40632, 2006/4/5   PN:A32D02352 */

    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_NORMAL, "Gmm_SndAttachReq:NORMAL: STATUS IS GMM_REGISTERED_INITIATED");

    g_MmSubLyrShare.GmmShare.ucDeatchEnableFlg = GMM_DETACH_ABLE;

    if (VOS_TRUE == NAS_MML_GetCsRestrictRegisterFlg())
    {
        g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL;
    }

    pSendNasMsg = Gmm_AttachRequestMsgMake();                                   /* 制作ATTACH REQUEST消息                   */

    /* V200R001 Added Begin: */
    gstGmmCasGlobalCtrl.ucSysRauFlg = GMM_FALSE;                                /* 系统信息要求的RAU过程标志清除 */
    if (GMM_TRUE == GMM_IsCasGsmMode())
    {


        /*When ciphering is requested at GPRS attach, the authentication and
        ciphering procedure shall be performed since the MS does not store
        the ciphering algorithm at detach. Upon GPRS attach, if ciphering is
        to be used, an AUTHENTICATION AND CIPHERING REQUEST message shall
        be sent to the MS to start ciphering.*/
        gstGmmCasGlobalCtrl.ucGprsCipher          = VOS_FALSE;

        NAS_GMM_SndMmcCipherInfoInd();

        GMM_UpdateSysTlli(GMM_FOREIGN_TLLI);                                    /* 获得foreign TLLI */
        /* 指配TLLI，准备向LLC发送GMM消息 */

        if ((VOS_FALSE == NAS_USIMMAPI_IsTestCard())
         && (GMM_TRUE == gstGmmCasGlobalCtrl.ucTlliAssignFlg))
        {
            /* 此处需要释放系统的TILI,指示LLC重新分配TILI，恢复数传 */
            GMM_FreeSysTlli();
        }
        GMM_AssignSysTlli();                                                /* 指配当前系统TLLI值 */

        /* 在发起attach时，有可能原来曾经把LLC挂起，所以在发送ATTACH前，需要恢复LLC */
        if (GMM_NOT_SUSPEND_LLC != gstGmmCasGlobalCtrl.ucSuspendLlcCause)
        {
            Gmm_SndLlcAbortReq(LL_GMM_CLEAR_DATA_TYPE_ALL);
            GMM_SndLlcResumeReq(LL_GMM_RESUME_TYPE_ALL);
            gstGmmCasGlobalCtrl.ucSuspendLlcCause = GMM_NOT_SUSPEND_LLC;
        }
    }
    /* V200R001 Added End: */

    if (GMM_TRUE == g_GmmGlobalCtrl.ucSigConFlg)
    {                                                                           /* 有PS信令连接                             */
        Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH, pSendNasMsg);              /* 用RRMM_DATA_REQ发送ATTACH REQUEST消息    */
        Gmm_TimerStart(GMM_TIMER_T3310);                                        /* 启动Timer3310                            */
    }
    else
    {                                                                           /* 没有PS信令连接                           */
        if (GMM_TRUE == g_GmmAttachCtrl.ucPagingWithImsiFlg)
        {                                                                       /* 本次attach是pagging的响应                */
            g_GmmAttachCtrl.ucPagingWithImsiFlg = GMM_FALSE;                    /* 清除标志                                 */

            Gmm_SndRrmmEstReq(RRC_EST_CAUSE_REGISTRATION,
                              RRC_IDNNS_IMSI_PAGE_RSP,
                              pSendNasMsg);                                     /* 用RRMM_EST_REQ发送ATTACH REQUEST消息     */
        }
        else
        {                                                                       /* 本次attach不是pagging的响应              */

            Gmm_SndRrmmEstReq(RRC_EST_CAUSE_REGISTRATION,
                              GMM_RRC_IDNNS_UNKNOWN,
                              pSendNasMsg);
        }

        Gmm_TimerStart(GMM_TIMER_PROTECT_FOR_SIGNALING);                        /* 启动建立信令连接保护Timer                */

    }
    if ((GMM_ATTACH_WHILE_IMSI_ATTACHED == g_GmmGlobalCtrl.ucSpecProc)
        || (GMM_ATTACH_COMBINED         == g_GmmGlobalCtrl.ucSpecProc))
    {                                                                           /* ATTACH类型不是normal attach              */
        NAS_GMM_SndMmCombinedAttachInitiation();                                   /* 发MMCGMM_COMBINED_ATTACH_INITIATION给MMC */
    }

    /* 有CS业务存在时,RAU被拒导致的重新attach依然需要进行普通的attach
       否则会导致CS业务中断 */
    if ((GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc)
     || (GMM_ATTACH_NORMAL          == g_GmmGlobalCtrl.ucSpecProc)
     || (GMM_TRUE                   == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg))
    {
        ucAttachType = NAS_OM_GMM_GPRS_ATTACH;
    }
    else if (GMM_ATTACH_WHILE_IMSI_ATTACHED == g_GmmGlobalCtrl.ucSpecProc)
    {
        ucAttachType = NAS_OM_GMM_COMBINED_GPRS_IMSI_ATTACH;
    }
    else
    {
        ucAttachType = NAS_OM_GMM_COMBINED_GPRS_IMSI_ATTACH;
    }
    NAS_EventReport(WUEPS_PID_GMM,
                    NAS_OM_EVENT_ATTACH_REQ,
                    (VOS_VOID *)&ucAttachType,
                    NAS_OM_EVENT_ATTACH_REQ_LEN);

    return;
}

VOS_VOID NAS_GMM_SndAttachReq(VOS_VOID)
{

#if (FEATURE_ON == FEATURE_LTE)
    VOS_UINT32                          ucIsSupportLteCapaFlg;

    ucIsSupportLteCapaFlg  = NAS_MML_IsSupportLteCapability();

    /* 1、L disable状态或syscfg设置设置不支持L，但全局变量记录GAS接入层发送的MS Radio Access capability IE是支持LTE的，
       2、L enalbe状态或syscfg设置支持L,但全局变量记录GAS接入层发送的MS Radio Access capability IE是不支持LTE的，
       需要启动定时器等待GAS接入层上报最新的MS Radio Access capability IE信息*/
    if ( ucIsSupportLteCapaFlg != g_GmmGlobalCtrl.UeInfo.ucMsRadioCapSupportLteFromAs)
    {
        /* 启动定时器 */
        Gmm_TimerStart(GMM_TIMER_WAIT_AS_MS_RADIO_CAPA_INFO);

        Gmm_ComStaChg(GMM_REGISTERED_INITIATED);
        return;
    }


    /* 支持L能力，tin值为guti情况下才需获取L的安全上下文 */
    if ((NAS_MML_TIN_TYPE_GUTI == NAS_MML_GetTinType() )
     && (VOS_FALSE == gstGmmSuspendCtrl.ucGetLteSecContext )
     && (VOS_TRUE == ucIsSupportLteCapaFlg))
    {
        if ( VOS_TRUE == NAS_GMM_GetLmmSecInfoFlg() )
        {
            return;
        }

        NAS_GMM_SetSpecProcNeedSecInfo(NAS_GMM_SPEC_PROC_ATTACH);

        NAS_GMM_SetLmmSecInfoFlg(VOS_TRUE);

        /* 给L模发送GMM_LMM_RESEL_SECU_INFO_REQ */
        NAS_GMM_SndLmmReselSecuInfoReq();

        /* 启动定时器 */
        Gmm_TimerStart(GMM_TIMER_WAIT_GET_RESEL_SECU_INFO_CNF);

        Gmm_ComStaChg(GMM_REGISTERED_INITIATED);
    }
    else
#endif

    {
        Gmm_SndAttachReq();
    }

}

/* BEGIN: Modified by liurui id:40632, 2006/7/26   PN:A32D03975 */

VOS_VOID Gmm_AttachInitiate(VOS_UINT8 ucSpecProc)
{
    VOS_UINT8                           ucCsRestrictRegister;
    ucCsRestrictRegister = NAS_MML_GetCsRestrictRegisterFlg();

    /*发起Attach时，清除收到网侧detach标识*/
    g_GmmGlobalCtrl.ucRcvNetDetachFlg = VOS_FALSE;

    if ((GMM_REGISTERED_INITIATED != g_GmmGlobalCtrl.ucState)
        && (0x10 != (g_GmmGlobalCtrl.ucState & 0xF0)))
    {                                                                           /* GMM context存在                          */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachInitiate:WARNING: Attach procedure shall not be activated");
        return;
    }
    if (VOS_FALSE == NAS_MML_GetPsAttachAllowFlg())
    {                                                                           /* PS域ATTACH被禁止                         */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: Attach procedure is not allowed");


        NAS_GMM_SndMmcActionResultIndWhenBarorNotSupportGprs(NAS_MML_REG_FAIL_CAUSE_MS_CFG_DOMAIN_NOT_SUPPORT);


        return;
    }

    g_GmmAttachCtrl.ucT3310outCnt = 0;

    /* attach发起后，清除3311定时器超时标志 */
    g_GmmRauCtrl.ucT3311ExpiredFlg  = GMM_FALSE;


    /* 存在CS信令链接时，进行NORMAL ATTACH */
    if ( VOS_TRUE == NAS_MML_GetCsSigConnStatusFlg() )
    {
        /* 网络模式I, 若PS域单独注册成功后，收到系统消息后，需要再次触发COMBINED ATTACH */
        if ( GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod )
        {
            NAS_GMM_SetSpecProcInCsTrans(GMM_ATTACH_NORMAL);
        }
        g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL;                         /* 存储当前进行的specific流程               */
        NAS_GMM_SndAttachReq();                                                 /* 调用函数发送ATTACH REQUEST               */
        return;
    }


    if (GMM_NULL_PROCEDURE != ucSpecProc)
    {/* 指定attach过程类型有效 */
        g_GmmGlobalCtrl.ucSpecProc = ucSpecProc;
        NAS_GMM_SndAttachReq();
        return;
    }

    /* 参考标杆做法,CS允许attach时,NMO 1下不区分手机模式直接进行联合注册 */
    if ((VOS_TRUE != NAS_MML_GetCsAttachAllowFlg())
     || (GMM_NET_MODE_I != g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 不是用户模式A且网络模式I                 */
        g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL;                         /* 存储当前进行的specific流程               */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: normal attach procedure start");

        NAS_GMM_SndAttachReq();                                                     /* 调用函数发送ATTACH REQUEST               */
        return;
    }

    if (VOS_TRUE == NAS_MML_GetSimCsRegStatus())
    {                                                                           /* CS域SIM有效                              */
        if (MM_STATUS_ATTACHED == g_MmSubLyrShare.MmShare.ucCsAttachState)
        {                                                                       /* CS域已经注册                             */
            if (GMM_TRUE == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg)
            {                                                                   /* CS域在通信中                             */
                g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL_CS_TRANS;        /* 存储当前进行的specific流程               */
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: normal attach procedure during cs transaction started");

                g_GmmGlobalCtrl.ucSpecProcInCsTrans = GMM_NULL_PROCEDURE;       /* 清除CS通信中的流程                       */
            }
            else
            {
                g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_WHILE_IMSI_ATTACHED;    /* 存储当前进行的specific流程               */
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: GPRS attach while IMSI attached procedure started");
            }
        }
        else
        {                                                                       /* CS域没注册                               */

            if ((VOS_FALSE == NAS_MML_GetCsAttachAllowFlg())
             || (VOS_TRUE == ucCsRestrictRegister))
            {                                                                   /* CS域ATTACH被禁止                         */
                g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL;                 /* 存储当前进行的specific流程               */
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: normal attach procedure start");
            }
            else
            {                                                                   /* CS域ATTACH没被禁止                       */
                if (GMM_TRUE == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg)
                {                                                               /* CS域在通信中                             */
                    g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL_CS_TRANS;    /* 存储当前进行的specific流程               */
                    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: normal attach procedure during cs transaction started");
                    g_GmmGlobalCtrl.ucSpecProcInCsTrans = GMM_NULL_PROCEDURE;   /* 清除CS通信中的流程                       */
                }
                else
                {                                                               /* CS域不在通信中                           */
                    g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_COMBINED;           /* 存储当前进行的specific流程               */
                    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: combined attach procedure started");
                }
            }
        }
    }
    else
    {                                                                           /* SIM无效                                  */
        g_GmmGlobalCtrl.ucSpecProc = GMM_ATTACH_NORMAL;                         /* 存储当前进行的specific流程               */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_AttachInitiate:INFO: normal attach procedure start");
    }

    NAS_GMM_SndAttachReq();                                                         /* 调用函数发送ATTACH REQUEST               */

    return;
}
VOS_VOID Gmm_RcvSmEstablishReq(VOS_VOID  *pMsg)
{
#if (FEATURE_ON == FEATURE_LTE)
    NAS_MML_LTE_CAPABILITY_STATUS_ENUM_UINT32               enLteCapabilityStatus;
#endif
    VOS_UINT32                                              ulIsTestCard;
    VOS_UINT32                                              ulPsAttachAllowFlg;
    VOS_UINT32                                              ulCsOnlyDataServiceSupportFlg;

    ulIsTestCard    = NAS_USIMMAPI_IsTestCard();

    if (GMM_STATUS_ATTACHED == g_MmSubLyrShare.GmmShare.ucAttachSta)
    {                                                                           /* PS已经注册                               */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvSmEstablishReq:WARNING: GMM has been attached");

        Gmm_SndSmEstablishCnf(GMM_SM_EST_SUCCESS, GMM_SM_CAUSE_SUCCESS);        /* 向SM回CNF                                */

        return;
    }

    if (GMM_NOT_SUPPORT_GPRS == g_GmmGlobalCtrl.SysInfo.ucNotSupportGprs)
    {
        Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_GPRS_NOT_SUPPORT);

        return;
    }

    if (MMC_GMM_NO_FORBIDDEN != g_GmmGlobalCtrl.SysInfo.ucForbMask)
    {                                                                           /* PS已经注册                               */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvSmEstablishReq:WARNING: GMM has been attached");

        Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_FORBID_LA);        /* 向SM回CNF                                */

        return;
    }

    ulPsAttachAllowFlg              = NAS_MML_GetPsAttachAllowFlg();
    ulCsOnlyDataServiceSupportFlg   = NAS_MML_GetCsOnlyDataServiceSupportFlg();

    if ((VOS_TRUE != ulPsAttachAllowFlg)
     && (VOS_TRUE != ulCsOnlyDataServiceSupportFlg))

    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING,
        "Gmm_RcvSmEstablishReq:WARNING: GMM attached not allow.");

        Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_PS_DETACH);        /* 向SM回CNF                                */

        return;
    }

    switch (g_GmmGlobalCtrl.ucState)
    {
    case GMM_REGISTERED_INITIATED:
        g_GmmAttachCtrl.ucSmCnfFlg = GMM_TRUE;                                  /* 置给SM回EST_CNF标志为TRUE                */
        break;

    case GMM_DEREGISTERED_NORMAL_SERVICE:
        if (VOS_TRUE == NAS_MML_GetPsRestrictRegisterFlg())
        {                                                                           /* PS已经注册                               */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvSmEstablishReq:WARNING: GMM has been attached");

            Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_ACCESS_BARRED);        /* 向SM回CNF                                */

            return;
        }

        NAS_MML_SetPsAttachAllowFlg(VOS_TRUE);

        Gmm_SndMmcGprsServiceInd(NAS_MMC_GMM_GPRS_SERVICE_ATTACH);

        g_GmmAttachCtrl.ucSmCnfFlg             = GMM_TRUE;                      /* 置给SM回EST_CNF标志为TRUE                */
        g_GmmGlobalCtrl.ucFollowOnFlg          = GMM_TRUE;                      /* 设置follow on 标志                       */
        NAS_MML_SetPsServiceBufferStatusFlg(VOS_TRUE);

#if (FEATURE_ON == FEATURE_LTE)
        enLteCapabilityStatus = NAS_MML_GetLteCapabilityStatus();

        if (VOS_TRUE == NAS_GMM_IsNeedEnableLte_AttachPs())
        {
            /* LTE国际漫游导致的disable LTE,此时不能通知GU接入层 */
            if ((VOS_FALSE == NAS_MML_GetDisableLteRoamFlg())
             && (NAS_MML_LTE_CAPABILITY_STATUS_DISABLE_NOTIFIED_AS == enLteCapabilityStatus)
             && (VOS_FALSE == g_GmmGlobalCtrl.UeInfo.ucMsRadioCapSupportLteFromAs))
            {
                g_GmmGlobalCtrl.ucSpecProc = NAS_GMM_GetAttachType();

                /* 需要enable lte 场景 ，需要启动定时器等待gas上报ms radio capability能力 */
                Gmm_TimerStart(GMM_TIMER_WAIT_AS_MS_RADIO_CAPA_INFO);
                Gmm_ComStaChg(GMM_REGISTERED_INITIATED);
                return;
            }
        }
#endif

        Gmm_AttachInitiate(GMM_NULL_PROCEDURE);                                                  /* 调用函数触发attach 过程                  */

        break;

    case GMM_DEREGISTERED_ATTEMPTING_TO_ATTACH:
        Gmm_RcvCmServReq_DeregAtmpToAtch(pMsg);
        break;

    case GMM_DEREGISTERED_PLMN_SEARCH:
        if (VOS_TRUE == NAS_MML_GetSimPsRegStatus())
        {
            /* GCF测试12.9.6，PS注册允许，当前未注册，且处于搜网状态，此时收到SM的业务请求，不缓冲，直接回复失败。 */
            if ( (VOS_TRUE == NAS_MML_GetPsAttachAllowFlg())
              && (VOS_TRUE == ulIsTestCard))
            {
                Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_ACCESS_BARRED);
            }
            else
            {
                NAS_MML_SetPsAttachAllowFlg(VOS_TRUE);

                g_GmmAttachCtrl.ucSmCnfFlg             = GMM_TRUE;              /* 置给SM回EST_CNF标志为TRUE                */
                g_GmmGlobalCtrl.ucFollowOnFlg          = GMM_TRUE;              /* 设置follow on 标志                       */
                NAS_MML_SetPsServiceBufferStatusFlg(VOS_TRUE);

                Gmm_SndMmcGprsServiceInd(NAS_MMC_GMM_GPRS_SERVICE_ATTACH);

            }
        }
        else
        {
                Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_SIM_PS_DOMAIN_REG_INVALID); /* 向SM回CNF                                */
        }
        break;

    default:
        Gmm_SndSmEstablishCnf(GMM_SM_EST_FAILURE, GMM_SM_CAUSE_UNKNOWN);         /* 向SM回CNF                                */
        break;
    }

    return;
}


VOS_VOID Gmm_RcvAttachAcceptMsg(
                            NAS_MSG_FOR_PCLINT_STRU *pMsg
                            )
{
    NAS_MSG_STRU        *pGmmStatus;
    VOS_BOOL            bTlliUpdateFlag = VOS_TRUE;

    NAS_MML_RAI_STRU                   *pstLastSuccRai = VOS_NULL_PTR;
    NAS_MML_IGNORE_AUTH_REJ_INFO_STRU      *pstAuthRejInfo;

    if ((GMM_TRUE == GMM_IsCasGsmMode())
        && (GMM_REGISTERED_NORMAL_SERVICE == g_GmmGlobalCtrl.ucState))
    {
        bTlliUpdateFlag = VOS_FALSE;
    }
    else if ( ( (NAS_MML_NET_RAT_TYPE_WCDMA == NAS_MML_GetCurrNetRatType()) && (GMM_REGISTERED_INITIATED != g_GmmGlobalCtrl.ucState) )
           || (GMM_DEREGISTERED_INITIATED    == g_GmmGlobalCtrl.ucState))
    {                                                                           /* GMM状态不是GMM_REGISTERED_INITIATED      */
        pGmmStatus = Gmm_GmmStatusMsgMake(NAS_MML_REG_FAIL_CAUSE_MSG_TYPE_NOT_COMPATIBLE);

        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachAcceptMsg:WARNING: GMM is not in GMM_REGISTERED_INITIATED");

        Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH , pGmmStatus);
        return;
    }
    else
    {
    }

    if (GMM_MSG_LEN_ATTACH_ACCEPT > pMsg->ulNasMsgSize)
    {                                                                           /* 消息过短                                 */
        pGmmStatus = Gmm_GmmStatusMsgMake(NAS_MML_REG_FAIL_CAUSE_INVALID_MANDATORY_INF);

        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachAcceptMsg:WARNING: ATTACH ACCEPT is too short");

        Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH , pGmmStatus);
        return;
    }

    if (GMM_FAILURE == Gmm_IECheck_AttachAccept(pMsg))                          /* 调用函数判断ATTACH ACCEPT消息的正确性    */
    {
        pGmmStatus =
            Gmm_GmmStatusMsgMake(NAS_MML_REG_FAIL_CAUSE_SEMANTICALLY_INCORRECT_MSG);

        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachAcceptMsg:WARNING: ATTACH ACCEPT is semantically incorrect");

        Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH , pGmmStatus);
        return;
    }

    if (GMM_FALSE == Gmm_AttachRauAcceptJudgeRai(pMsg))
    {                                                                           /* ATTACH ACCEPT的RAI与系统信息的RAI不一致  */
        pGmmStatus =
            Gmm_GmmStatusMsgMake(NAS_MML_REG_FAIL_CAUSE_SEMANTICALLY_INCORRECT_MSG);

        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvAttachAcceptMsg:WARNING: RAI in ATTACH ACCEPT is differenct with current RAI");

        Gmm_SndRrmmDataReq(RRC_NAS_MSG_PRIORTY_HIGH , pGmmStatus);

        return;
    }
    NAS_GMM_ResetHplmnRejCauseChangedCounter();

    g_GmmGlobalCtrl.ucGprsResumeFlg = GMM_FALSE;

    Gmm_TimerStop(GMM_TIMER_T3310);                                             /* 停止Timer3310                            */
    Gmm_TimerStop(GMM_TIMER_T3318);                                             /* 停止T3318                                */
    Gmm_TimerStop(GMM_TIMER_T3320);                                             /* 停止T3320                                */

    /* 4.7.2.1.2 Handling of READY timer in the MS in Iu mode and S1 mode
       The READY timer is not applicable for Iu mode and S1 mode.
       Upon completion of a successful GPRS attach or routing area updating
       procedure in Iu mode, the MS may stop the READY timer, if running.
       Upon completion of a successful EPS attach or tracking area updating
       procedure, the MS may stop the READY timer, if running. */
    if (NAS_MML_NET_RAT_TYPE_WCDMA == NAS_MML_GetCurrNetRatType())
    {
        Gmm_TimerStop(GMM_TIMER_T3314);
        gstGmmCasGlobalCtrl.GmmSrvState = GMM_AGB_GPRS_STANDBY;
    }

    Gmm_AttachAcceptHandle(pMsg, bTlliUpdateFlag);                                               /* 调用函数解析ATTACH REQUEST的IE           */

    pstAuthRejInfo = NAS_MML_GetAuthRejInfo();
    pstAuthRejInfo->ucHplmnPsAuthRejCounter = 0;


    /*attach 成功，更新Drx参数携带状态*/
    NAS_MML_SetPsRegContainDrx(g_GmmGlobalCtrl.UeInfo.enLatestAttachOrRauContainDrx);

    /* 可维可测增加消息打印 */
    NAS_GMM_LogPsRegContainDrxInfo(g_GmmGlobalCtrl.UeInfo.enLatestAttachOrRauContainDrx);



    /* 取得当前注册成功的RAI信息，注意:此信息的获取要放在解析完RAI信息之后 */
    pstLastSuccRai    = NAS_MML_GetPsLastSuccRai();

    /* 若GU模的RPlmn信息发生了改变，则保存RPlmn信息到全局变量中，并更新到NVIM中 */
    if ( VOS_TRUE == NAS_GMM_IsGURplmnChanged( &(pstLastSuccRai->stLai.stPlmnId),
                                               NAS_MML_GetCurrNetRatType() ) )
    {
        /* 保存RPlmn信息到全局变量中 */
        NAS_MML_UpdateGURplmn( &(pstLastSuccRai->stLai.stPlmnId),
                               NAS_MML_GetCurrNetRatType() );

        /* 将全局变量中的RPlmn信息更新到NVIM中 */
        NAS_GMM_WriteRplmnWithRatNvim( NAS_MML_GetRplmnCfg() );
    }

    g_GmmGlobalCtrl.ucFollowOnFlg = GMM_FALSE;                                  /* 清楚follow on 标志                       */
    NAS_MML_SetPsServiceBufferStatusFlg(VOS_FALSE);

    if (GMM_TRUE == g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg)
    {                                                                           /* 等待触发RAU                              */
        if (0x20 == (g_GmmGlobalCtrl.ucState & 0xF0))
        {                                                                       /* ATTACH流程成功结束                       */
            if (GMM_TRUE == g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg)
            {                                                                   /* 需要触发MM进行LU                         */
                if ((GMM_NET_MODE_II == g_GmmGlobalCtrl.ucNetMod)
                    && (GMM_NET_MODE_I == g_GmmGlobalCtrl.SysInfo.ucNetMod))
                {                                                               /* 网络模式改变                             */
                    NAS_GMM_SndMmLuInitiation(GMMMM_NO_TIMER_EXPIRED);          /* 通知MM进行LU                             */
                    g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg = GMM_FALSE;
                    g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg = GMM_FALSE;

                    if (   (GMM_FALSE == GMM_IsCasGsmMode())
                        && (GMM_FALSE == g_GmmGlobalCtrl.ucFopFlg))
                    {
                        Gmm_TimerStart(GMM_TIMER_T3340);
                    }

                    return;
                }
            }
            Gmm_RoutingAreaUpdateInitiate(GMM_UPDATING_TYPE_INVALID);           /* 触发RAU                                  */
        }
        g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg = GMM_FALSE;
        g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg = GMM_FALSE;
    }
    else
    {
        if (GMM_MSG_HOLD_FOR_DETACH
            == (g_GmmGlobalCtrl.MsgHold.ulMsgHoldMsk & GMM_MSG_HOLD_FOR_DETACH))
        {                                                                           /* 如果有保留的DETACH原语                   */
            Gmm_MsgDistribute((VOS_VOID *)g_GmmGlobalCtrl.MsgHold.ulMsgAddrForDetach);  /* 保留的消息分发                           */
            g_GmmGlobalCtrl.MsgHold.ulMsgHoldMsk &= ~GMM_MSG_HOLD_FOR_DETACH;       /* 清除缓存标志                             */
            Gmm_MemFree(g_GmmGlobalCtrl.MsgHold.ulMsgAddrForDetach);
        }
    }


    if (   (GMM_FALSE == GMM_IsCasGsmMode())
        && (GMM_FALSE == g_GmmGlobalCtrl.ucFopFlg))
    {
        Gmm_TimerStart(GMM_TIMER_T3340);
    }
    return;
}
VOS_VOID Gmm_RcvMmcGmmAttachReq(
    VOS_VOID                           *pMsg                                       /* 消息指针                                 */
)
{
    MMCGMM_ATTACH_REQ_STRU             *pAttach;                                         /* 定义指针                                 */
    NAS_MML_NET_RAT_TYPE_ENUM_UINT8    enRatType;

    pAttach     = (MMCGMM_ATTACH_REQ_STRU *)pMsg;                                   /* 初始化指针                               */
    enRatType   = NAS_MML_GetCurrNetRatType();
    /* 此处对attach进行修改,MMC不对PS进行缓存,收到ATTACH请求时,
    在G模下先判断以下MM的状态,MM存在链接则丢掉该Attach,后续MM
    注册成功后有系统消息上来时,GMM会上报当前服务状态*/
    if ((MMC_MM_SIGNALING_STATUS_PRESENT == MM_GetSignalingStatus ())
     && (NAS_MML_NET_RAT_TYPE_GSM == enRatType))
    {

        /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF,防止AT口一直被CGATT命令阻塞 */
        if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                 & GMM_WAIT_PS_ATTACH))
        {
            NAS_GMM_SndMmcAttachCnf();
        }
        g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;


        NAS_INFO_LOG(WUEPS_PID_GMM,"Gmm_RcvMmcGmmAttachReq:Mm Conn is exist");
        return;
    }

    switch (g_GmmGlobalCtrl.ucState)
    {                                                                           /* GMM状态                                  */
    case GMM_REGISTERED_INITIATED:
        Gmm_RcvMmcGmmAttachReq_RegInit(pAttach);                                /* 调用函数处理                             */
        break;
    case GMM_DEREGISTERED_INITIATED:
        Gmm_RcvMmcGmmAttachReq_DeregInit(pAttach);                              /* 调用函数处理                             */
        break;
    case GMM_ROUTING_AREA_UPDATING_INITIATED:
        Gmm_RcvMmcGmmAttachReq_RauInit(pAttach);                                /* 调用函数处理                             */
        break;
    case GMM_SERVICE_REQUEST_INITIATED:
        Gmm_RcvMmcGmmAttachReq_ServReqInit(pAttach);                            /* 调用函数处理                             */
        break;
    case GMM_DEREGISTERED_NORMAL_SERVICE:
        Gmm_RcvMmcGmmAttachReq_DeregNmlServ(pAttach);                           /* 调用函数处理                             */
        break;
    case GMM_DEREGISTERED_LIMITED_SERVICE:
        Gmm_RcvMmcGmmAttachReq_DeregLimitServ(pAttach);                         /* 调用函数处理                             */
        break;
    case GMM_DEREGISTERED_ATTEMPTING_TO_ATTACH:
        Gmm_RcvMmcGmmAttachReq_DeregAtmpToAtch(pAttach);                        /* 调用函数处理                             */
        break;
    case GMM_REGISTERED_NORMAL_SERVICE:
        Gmm_RcvMmcGmmAttachReq_RegNmlServ((MMCGMM_ATTACH_REQ_STRU *)pMsg);                                /* 调用函数处理                             */
        break;
    case GMM_REGISTERED_ATTEMPTING_TO_UPDATE:
        Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdt(pAttach);                          /* 调用函数处理                             */
        break;
    case GMM_REGISTERED_LIMITED_SERVICE:
        Gmm_RcvMmcGmmAttachReq_RegLimitServ(pAttach);                           /* 调用函数处理                             */
        break;
    case GMM_REGISTERED_ATTEMPTING_TO_UPDATE_MM:
        Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdtMm(pAttach);                        /* 调用函数处理                             */
        break;

    #if ((defined(__PS_WIN32_RECUR__)) || ( VOS_WIN32 == VOS_OS_VER ))
    case GMM_DEREGISTERED_PLMN_SEARCH:
        break;

    case GMM_REGISTERED_PLMN_SEARCH:
        break;
    #endif

    /* 在搜网状态直接回复ATTACH CNF,防止AT口一致被CGATT=1命令阻塞 */
    default:


        /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
        if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                 & GMM_WAIT_PS_ATTACH))
        {
            NAS_GMM_SndMmcAttachCnf();
        }
        g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;

        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq:WARNING: Attach can't be proceeded g_GmmGlobalCtrl.ucState!");
        GMM_PrintState();
        break;
    }
    return;
}
VOS_VOID Gmm_RcvMmcGmmAttachReq_RegInit(
                                    MMCGMM_ATTACH_REQ_STRU    *pMsg             /* 消息指针                                 */
                                    )
{
    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if ((MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
            || (MMC_GMM_ATTACH_TYPE_GPRS_IMSI == pMsg->enAttachType))
        {                                                                       /* 注册CS域或者PS,CS都注册                  */
            if ((GMM_ATTACH_NORMAL == g_GmmGlobalCtrl.ucSpecProc)
                || (GMM_ATTACH_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc))
            {                                                                   /* 当前流程为NORMAL ATTACH                  */
                g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg = GMM_TRUE;              /* 流程结束之后进行RAU                      */
                g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg = GMM_TRUE;             /* 标记GMM不能进行RAU时要触发MM的LU过程     */
            }
        }
    }
    return;
}
VOS_VOID Gmm_RcvMmcGmmAttachReq_DeregInit(
                                    MMCGMM_ATTACH_REQ_STRU    *pMsg             /* 消息指针*/
                                    )
{
    if ((GMM_FALSE == g_GmmGlobalCtrl.ucSigConFlg)
     && (GMM_RRC_RRMM_EST_CNF_FLG == (g_GmmReqCnfMng.ucCnfMask & GMM_RRC_RRMM_EST_CNF_FLG)))
    {
        /*链接正在建立时，释放链接，认为detach、attach都已经完成*/
        Gmm_ComStaChg(GMM_REGISTERED_NORMAL_SERVICE);
        Gmm_SndRrmmRelReq(RRC_CELL_UNBARRED);
        Gmm_TimerStop(GMM_TIMER_PROTECT_FOR_SIGNALING);
        g_GmmReqCnfMng.ucCnfMask &= ~GMM_RRC_RRMM_EST_CNF_FLG;

        g_GmmGlobalCtrl.ucSpecProcHold = g_GmmGlobalCtrl.ucSpecProc;
        g_GmmGlobalCtrl.ucSpecProc     = GMM_NULL_PROCEDURE;

    }
    else
    {
       /*链接建立完成，认为detach完成，重新进行attach*/
       Gmm_ComStaChg(GMM_DEREGISTERED_NORMAL_SERVICE);

       Gmm_AttachInitiate(GMM_NULL_PROCEDURE);                                  /* 进行attach */
    }

    return;
}


VOS_VOID Gmm_RcvMmcGmmAttachReq_RauInit(
                                     MMCGMM_ATTACH_REQ_STRU    *pMsg            /* 消息指针                                 */
                                     )
{
    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if ((MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
            || (MMC_GMM_ATTACH_TYPE_GPRS_IMSI == pMsg->enAttachType))
        {                                                                       /* 注册CS域或者PS,CS都注册                  */
            if ((GMM_RAU_NORMAL == g_GmmGlobalCtrl.ucSpecProc)
                || (GMM_RAU_NORMAL_CS_TRANS == g_GmmGlobalCtrl.ucSpecProc))
            {                                                                   /* 当前流程为NORMAL ATTACH                  */
                g_GmmGlobalCtrl.MsgHold.ucHandleRauFlg = GMM_TRUE;              /* 流程结束之后进行RAU                      */
                g_GmmGlobalCtrl.MsgHold.ucInitiateLuFlg = GMM_TRUE;             /* 标记GMM不能进行RAU时要触发MM的LU过程     */
            }
        }
    }
    return;
}

VOS_VOID Gmm_RcvMmcGmmAttachReq_ServReqInit(
                                        MMCGMM_ATTACH_REQ_STRU    *pMsg         /* 消息指针                                 */
                                        )
{
    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
        {                                                                       /* 注册CS域                                 */
            Gmm_TimerStop(GMM_TIMER_T3317);                                     /* 停T3317                                  */
            Gmm_TimerStop(GMM_TIMER_T3318);                                     /* 停止T3318                                */
            Gmm_TimerStop(GMM_TIMER_T3320);                                     /* 停止T3320                                */

            Gmm_RoutingAreaUpdateInitiate(GMM_UPDATING_TYPE_INVALID);           /* 触发RAU流程                              */
        }
    }
    return;
}
VOS_VOID Gmm_RcvMmcGmmAttachReq_DeregNmlServ(
                                         MMCGMM_ATTACH_REQ_STRU    *pMsg        /* 消息指针                                 */
                                         )
{
    if (VOS_FALSE == NAS_MML_GetPsAttachAllowFlg())
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_DeregNmlServ:WARNING: PS ATTACH is not allowed");
        return;
    }
    if ((NAS_MML_MS_MODE_PS_CS == NAS_MML_GetMsMode())
     && (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
     && (GMM_NET_MODE_II == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 注册CS域                                 */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_DeregNmlServ:WARNING: CS shall be registered by MM in A+II");
        return;
    }

    if (VOS_TRUE == NAS_MML_GetPsRestrictRegisterFlg())
    {

        Gmm_ComStaChg(GMM_DEREGISTERED_ATTACH_NEEDED);                          /* 状态迁移                                 */

        /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
        if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                 & GMM_WAIT_PS_ATTACH))
        {
            NAS_GMM_SndMmcAttachCnf();
        }
        g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
        return;
    }
    if (GMM_NOT_SUPPORT_GPRS == g_GmmGlobalCtrl.SysInfo.ucNotSupportGprs)
    {
        Gmm_ComStaChg(GMM_DEREGISTERED_LIMITED_SERVICE);                      /* 状态迁移                                 */

        /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
        if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                                 & GMM_WAIT_PS_ATTACH))
        {
            NAS_GMM_SndMmcAttachCnf();
        }
        g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;
    }
    else if (0 != g_GmmGlobalCtrl.SysInfo.ucForbMask)
    {
        Gmm_ComForbiddenList(g_GmmGlobalCtrl.SysInfo.ucForbMask);
    }
    else
    {
        Gmm_AttachInitiate(GMM_NULL_PROCEDURE);                                                   /* 触发ATTACH流程                           */
    }
    return;
}
VOS_VOID Gmm_RcvMmcGmmAttachReq_DeregLimitServ(
                                          MMCGMM_ATTACH_REQ_STRU    *pMsg       /* 消息指针                                 */
                                          )
{
    VOS_UINT32                          ulCause = NAS_MML_REG_FAIL_CAUSE_NULL;                                /* 定义临时变量                             */


    if (VOS_FALSE == NAS_MML_GetPsAttachAllowFlg())
    {                                                                           /* PS域不允许注册                           */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_DeregLimitServ:WARNING: PS ATTACH is not allowed");
        return;
    }
    if (GMM_FORBIDDEN_PLMN
        == (g_GmmGlobalCtrl.SysInfo.ucForbMask & GMM_FORBIDDEN_PLMN))
    {                                                                           /* 在GMM_FORBIDDEN_PLMN列表                 */
        ulCause = NAS_MML_REG_FAIL_CAUSE_FORBIDDEN_PLMN;
    }
    else if (GMM_FORBIDDEN_PLMN_FOR_GPRS
        == (g_GmmGlobalCtrl.SysInfo.ucForbMask & GMM_FORBIDDEN_PLMN_FOR_GPRS))
    {                                                                           /* 在GMM_FORBIDDEN_PLMN_FOR_GPRS列表        */
        ulCause = NAS_MML_REG_FAIL_CAUSE_FORBIDDEN_PLMN_FOR_GPRS;
    }
    else if (GMM_FORBIDDEN_LA_FOR_ROAM
        == (g_GmmGlobalCtrl.SysInfo.ucForbMask & GMM_FORBIDDEN_LA_FOR_ROAM))
    {                                                                           /* 在GMM_FORBIDDEN_LA_FOR_ROAM列表          */
        ulCause = NAS_MML_REG_FAIL_CAUSE_FORBIDDEN_LA_FOR_ROAM;
    }
    else if (GMM_FORBIDDEN_LA_FOR_REG_PRVS_SVS
                    == (g_GmmGlobalCtrl.SysInfo.ucForbMask
                                        & GMM_FORBIDDEN_LA_FOR_REG_PRVS_SVS))
    {                                                                           /* 在GMM_FORBIDDEN_LA_FOR_REG_PRVS_SVS列表  */
        ulCause = NAS_MML_REG_FAIL_CAUSE_FORBIDDEN_LA_FOR_REG_PRVS_SVS;
    }
    else
    {
    }

    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if (NAS_MML_REG_FAIL_CAUSE_FORBIDDEN_PLMN_FOR_GPRS == ulCause)
        {                                                                       /* 原因值为＃14                             */
            if (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
            {                                                                   /* 注册CS域                                 */
                PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_NORMAL, "Gmm_RcvMmcGmmAttachReq_DeregLimitServ:NORMAL: Attach Type is MMC_GMM_ATTACH_TYPE_IMSI");
                g_GmmGlobalCtrl.stAttachInfo.enAttachType &= ~GMM_WAIT_CS_ATTACH;
                return;
            }
        }
        else
        {
            if(GMM_TRUE == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg)
            {                                                                   /* CS 通信中                                */
                g_GmmGlobalCtrl.ucSpecProcInCsTrans = GMM_ATTACH_NORMAL;
            }
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_RcvMmcGmmAttachReq_DeregLimitServ:INFO: normal attach procedure during cs transaction started");
        }
    }


    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;

    return;
}

VOS_VOID Gmm_RcvMmcGmmAttachReq_RegLimitServ(
                                         MMCGMM_ATTACH_REQ_STRU    *pMsg        /* 消息指针                                 */
                                         )
{

    if (VOS_FALSE == NAS_MML_GetPsAttachAllowFlg())
    {                                                                           /* PS域不允许注册                           */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_RegLimitServ:WARNING: PS ATTACH is not allowed");
        return;
    }

    g_GmmGlobalCtrl.ucRealProFlg = GMM_UNREAL_PROCEDURE;                        /* 设置假流程标志                           */

    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if (GMM_TRUE == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg)
        {                                                                       /* CS 通信中                                */
            g_GmmGlobalCtrl.ucSpecProcInCsTrans = GMM_RAU_NORMAL;
        }
    }


    /* 判断等待标志是否存在，如果存在则发送MMCGMM_ATTACH CNF */
    if (GMM_WAIT_PS_ATTACH == (g_GmmGlobalCtrl.stAttachInfo.enAttachType
                             & GMM_WAIT_PS_ATTACH))
    {
        NAS_GMM_SndMmcAttachCnf();
    }
    g_GmmGlobalCtrl.stAttachInfo.enAttachType = GMM_WAIT_NULL_ATTACH;


    PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_INFO, "Gmm_RcvMmcGmmAttachReq_RegLimitServ:INFO: specific procedure ended");

    return;
}
/*******************************************************************************
  Module   : Gmm_RcvMmcGmmAttachReq_DeregAtmpToAtch
  Function : 收到原语MMCGMM_ATTACH_REQ在GMM_DEREGISTERED_ATTEMPTING_TO_ATTACH
             状态的处理
  Input    : MMCGMM_ATTACH_REQ_STRU   *pMsg        消息指针
  Output   : 无
  NOTE     : 无
  Return   : 无
  History  :
    1. 张志勇  2004.03.12  新规作成
*******************************************************************************/
VOS_VOID Gmm_RcvMmcGmmAttachReq_DeregAtmpToAtch(
                                            MMCGMM_ATTACH_REQ_STRU    *pMsg     /* 消息指针                                 */
                                            )
{
    if ((NAS_MML_MS_MODE_PS_CS == NAS_MML_GetMsMode())
        && (GMM_NET_MODE_II == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为II                */
        if (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
        {                                                                       /* 注册CS域                                 */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_DeregAtmpToAtch:WARNING: CS shall be registered by MM in A+II");
            return;
        }
    }

    if (GMM_TIMER_T3302_FLG
        == (g_GmmTimerMng.ulTimerRunMask & GMM_TIMER_T3302_FLG))
    {                                                                           /* T3302正在运行                            */
        Gmm_TimerStop(GMM_TIMER_T3302);                                         /* 停止T3302                                */
    }
    else
    {
        Gmm_TimerStop(GMM_TIMER_T3311);                                         /* 停止T3311                                */
    }
    Gmm_AttachInitiate(GMM_NULL_PROCEDURE);                                                       /* 触发ATTACH流程                           */
    return;
}
VOS_VOID Gmm_RcvMmcGmmAttachReq_RegNmlServ(
                                       MMCGMM_ATTACH_REQ_STRU    *pMsg          /* 消息指针                                 */
                                       )
{
    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if ((VOS_TRUE == NAS_MML_GetSimCsRegStatus())
         && (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType))
        {                                                                       /* 注册CS域                                 */
            if (GMM_TRUE == g_GmmGlobalCtrl.CsInfo.ucCsTransFlg)
            {                                                                   /* CS 通信中                                */
                g_GmmGlobalCtrl.ucSpecProcInCsTrans = GMM_ATTACH_NORMAL;        /* 设置CS通信中的流程,为了CS通信结束后触发  *
                                                                                 * combined RAU with IMSI attach            */
            }
            else
            {
                Gmm_RoutingAreaUpdateInitiate(
                    GMM_COMBINED_RALAU_WITH_IMSI_ATTACH);                       /* 触发RAU流程                              */
            }
        }
        else
        {
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_RegNmlServ:WARNING: PS has been attached");
        }
    }
    else
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_RegNmlServ:WARNING: PS has been attached");
    }

    return;
}
/*******************************************************************************
  Module   : Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdt
  Function : 收到原语MMCGMM_ATTACH_REQ在GMM_REGISTERED_ATTEMPTING_TO_UPDATE
             状态的处理
  Input    : MMCGMM_ATTACH_REQ_STRU   *pMsg        消息指针
  Output   : 无
  NOTE     : 无
  Return   : 无
  History  :
    1. 张志勇  2004.03.12  新规作成
*******************************************************************************/
VOS_VOID Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdt(
                                          MMCGMM_ATTACH_REQ_STRU    *pMsg       /* 消息指针                                 */
                                          )
{
    if ((NAS_MML_MS_MODE_PS_CS == NAS_MML_GetMsMode())
        && (GMM_NET_MODE_II == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为II                */
        if (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
        {                                                                       /* 注册CS域                                 */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdt:WARNING: CS shall be registered by MM in A+II");
            return;
        }
    }
    g_GmmRauCtrl.ucRauAttmptCnt = 0;                                            /* RAU attempt counter 清0                  */
    if (GMM_TIMER_T3302_FLG
        == (g_GmmTimerMng.ulTimerRunMask & GMM_TIMER_T3302_FLG))
    {                                                                           /* T3302正在运行                            */
        Gmm_TimerStop(GMM_TIMER_T3302);                                         /* 停止T3302                                */
    }
    else
    {
        Gmm_TimerStop(GMM_TIMER_T3311);                                         /* 停止T3311                                */
    }
    Gmm_RoutingAreaUpdateInitiate(GMM_UPDATING_TYPE_INVALID);                   /* 触发RAU流程                              */

    return;
}

VOS_VOID Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdtMm(
                                            MMCGMM_ATTACH_REQ_STRU    *pMsg     /* 消息指针                                 */
                                            )
{
    /* 参考标杆做法,NMO1下,不通过手机模式判断发起联合注册 */
    if ((VOS_TRUE == NAS_MML_GetCsAttachAllowFlg())
     && (GMM_NET_MODE_I == g_GmmGlobalCtrl.ucNetMod))
    {                                                                           /* 用户模式为A，网络模式为I                 */
        if (MMC_GMM_ATTACH_TYPE_IMSI == pMsg->enAttachType)
        {                                                                       /* 注册CS域                                 */
            if (GMM_TIMER_T3302_FLG
                == (g_GmmTimerMng.ulTimerRunMask & GMM_TIMER_T3302_FLG))
            {                                                                   /* T3302正在运行                            */
                Gmm_TimerStop(GMM_TIMER_T3302);                                 /* 停止T3302                                */
            }
            else
            {
                Gmm_TimerStop(GMM_TIMER_T3311);                                 /* 停止T3311                                */
            }
            Gmm_RoutingAreaUpdateInitiate(GMM_COMBINED_RALAU_WITH_IMSI_ATTACH); /* 触发RAU流程                              */
        }
        else
        {
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdtMm:WARNING: PS has been attached");
        }
    }
    else
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_RcvMmcGmmAttachReq_RegAtmpToUpdtMm:WARNING: CS shall be registered by MM ");
    }

    return;
}



VOS_UINT8 Gmm_AttachAccept_Mandatory_Ie_Check(NAS_MSG_FOR_PCLINT_STRU *pMsg,
                                                  VOS_UINT8    *pNeedReturn)
{
    VOS_UINT8   ucForceToStandby = 0;

    if ((VOS_NULL == pMsg) || (VOS_NULL == pNeedReturn))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAccept_Mandatory_Ie_Check:ERROR: Null Pointer");
        return GMM_FAILURE;
    }

    if ((1 != ((pMsg->aucNasMsg[2]) & 0x07))
       && (3 != ((pMsg->aucNasMsg[2]) & 0x07)))
    {                                                                           /* wrong Attach Result                      */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAccept_Mandatory_Ie_Check:WARNING: IE\"Attach result\" is error in ATTACH ACCEPT");
        return GMM_FAILURE;
    }

    ucForceToStandby = (VOS_UINT8)((pMsg->aucNasMsg[2] >> 4) & 0x07);

    if (1 == ucForceToStandby)
    {
        gstGmmCasGlobalCtrl.ucFtsFlg = GMM_TRUE;
    }

    else if (0 == ucForceToStandby)
    {
        gstGmmCasGlobalCtrl.ucFtsFlg = GMM_FALSE;
    }

    else
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_AttachAccept_Mandatory_Ie_Check:WARNING:WARNING: IE\"Force to standby\" is error in ATTACH ACCEPT");
        gstGmmCasGlobalCtrl.ucFtsFlg = GMM_FALSE;
    }

    gstGmmCasGlobalCtrl.enRaPrioLevFosSms = pMsg->aucNasMsg[4] & 0x07;

    *pNeedReturn = GMM_FALSE;

    return GMM_SUCCESS;
}
VOS_UINT8 Gmm_IeCheck_Allocated_Ptmsi(NAS_MSG_FOR_PCLINT_STRU *pMsg,
                                                  VOS_UINT8    ucIeOffset,
                                                  VOS_UINT8    *pNeedReturn)
{
    if ((VOS_NULL == pMsg) || (VOS_NULL == pNeedReturn))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IECheck_Allocated_Ptmsi:ERROR: Null Pointer");
        return GMM_FAILURE;
    }

    if ((VOS_UINT8)(pMsg->ulNasMsgSize) < (ucIeOffset + 2))
    {
        return GMM_SUCCESS;
    }

    if (pMsg->aucNasMsg[ucIeOffset + 1] != 5)
    {                                                                           /* wrong Length of mobile identity contents */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IECheck_Allocated_Ptmsi:WARNING: Length of IE\"P-TMSI\" is invalid");
        return GMM_FAILURE;
    }

    if ((VOS_UINT8)(pMsg->ulNasMsgSize)
         < (ucIeOffset + pMsg->aucNasMsg[ucIeOffset + 1] + 2))
    {
        return GMM_SUCCESS;
    }

    if (pMsg->aucNasMsg[ucIeOffset + 2] != 0xF4)
    {                                                                           /* wrong Type of identity& Identity digit 1 */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IECheck_Allocated_Ptmsi:WARNING: IE\"P-TMSI\" is invalid");
        return GMM_FAILURE;
    }

    *pNeedReturn = GMM_FALSE;

    return GMM_SUCCESS;
}

VOS_UINT8 Gmm_IeCheck_Ms_Identity_Imsi(NAS_MSG_FOR_PCLINT_STRU *pMsg,
                                                  VOS_UINT8    *ucIeOffset,
                                                  VOS_UINT8    *pNeedReturn)
{
    VOS_UINT8   ucImsiLen  = 0;

    ucImsiLen = pMsg->aucNasMsg[*ucIeOffset + 1];


    *ucIeOffset += (ucImsiLen + 2);

    *pNeedReturn = GMM_FALSE;

    return GMM_SUCCESS;
}

VOS_UINT8 Gmm_IeCheck_Ms_Identity(NAS_MSG_FOR_PCLINT_STRU *pMsg,
                                                  VOS_UINT8    *ucIeOffset,
                                                  VOS_UINT8    *pNeedReturn)
{
    VOS_UINT8   ucNeedReturn = GMM_TRUE;
    VOS_UINT8   ucResult   = GMM_SUCCESS;

    if ((VOS_NULL == pMsg) || (VOS_NULL == pNeedReturn) || (VOS_NULL == ucIeOffset))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IeCheck_Ms_Identity:ERROR: Null Pointer");
        return GMM_FAILURE;
    }

    if ((VOS_UINT8)(pMsg->ulNasMsgSize) < (*ucIeOffset + 2))
    {
        return GMM_SUCCESS;
    }


    if ((VOS_UINT8)(pMsg->ulNasMsgSize)
         < (*ucIeOffset + pMsg->aucNasMsg[*ucIeOffset + 1] + 2))
    {
        return GMM_SUCCESS;
    }

    if (GMM_MOBILE_ID_IMSI == (pMsg->aucNasMsg[*ucIeOffset + 2] & 0x07))
    {                                                                           /* IE中是IMSI                               */
        ucResult = Gmm_IeCheck_Ms_Identity_Imsi(pMsg, ucIeOffset, &ucNeedReturn);

        if (GMM_TRUE == ucNeedReturn)
        {
            return ucResult;
        }
    }
    else if (GMM_MOBILE_ID_TMSI_PTMSI == (pMsg->aucNasMsg[*ucIeOffset + 2] & 0x07))
    {                                                                           /* IE中是TMSI                               */
        if (pMsg->aucNasMsg[*ucIeOffset + 2] != 0xF4)
        {                                                                       /* wrong Type of identity& Identity digit 1 */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IeCheck_Ms_Identity:WARNING: TMSI shall not be occured");
            return GMM_FAILURE;
        }

        if (pMsg->aucNasMsg[*ucIeOffset + 1] != 5)
        {                                                                       /* wrong Length of mobile identity contents */
            PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IeCheck_Ms_Identity:WARNING: Length of IE\"TMSI\" is invalid");
            return GMM_FAILURE;
        }

        *ucIeOffset += 7;
    }
    else
    {
        /* 其他IE直接跳过,eg:imei,imeisv,no identity */
        *ucIeOffset += pMsg->aucNasMsg[*ucIeOffset + 1] + 2;
    }

    *pNeedReturn = GMM_FALSE;

    return GMM_SUCCESS;
}
VOS_UINT8 Gmm_IeCheck_T3302_Value(NAS_MSG_FOR_PCLINT_STRU *pMsg,
                                              VOS_UINT8    ucIeOffset,
                                              VOS_UINT8    *pNeedReturn)
{

    if ((VOS_NULL == pMsg) || (VOS_NULL == pNeedReturn))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IeCheck_T3302_Value:ERROR: Null Pointer");
        return GMM_FAILURE;
    }

    if ((VOS_UINT8)(pMsg->ulNasMsgSize) < (ucIeOffset + 2))
    {
        return GMM_SUCCESS;
    }


    *pNeedReturn = GMM_FALSE;

    return GMM_SUCCESS;
}

VOS_UINT8 Gmm_IeCheck_Equivalent_Plmns(NAS_MSG_FOR_PCLINT_STRU *pMsg,
                                              VOS_UINT8    ucIeOffset,
                                              VOS_UINT8    *pNeedReturn)
{

    if ((VOS_NULL == pMsg) || (VOS_NULL == pNeedReturn))
    {
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IeCheck_Equivalent_Plmns:ERROR: Null Pointer");
        return GMM_FAILURE;
    }

    if ((VOS_UINT8)(pMsg->ulNasMsgSize) < (ucIeOffset + 2))
    {
        return GMM_SUCCESS;
    }

    if ((pMsg->aucNasMsg[ucIeOffset + 1] > 45)
        || (pMsg->aucNasMsg[ucIeOffset + 1] < 3)
        || ((pMsg->aucNasMsg[ucIeOffset + 1] % 3 ) != 0))
    {                                                                           /* wrong Length of PLMN List contents       */
        PS_LOG(WUEPS_PID_GMM, VOS_NULL, PS_PRINT_WARNING, "Gmm_IeCheck_Equivalent_Plmns:WARNING: Length of \"Equivalent PLMNs\" is error");
        return GMM_SUCCESS;
    }

    if ((VOS_UINT8)(pMsg->ulNasMsgSize)
         < (ucIeOffset + pMsg->aucNasMsg[ucIeOffset + 1] + 2))
    {
        return GMM_SUCCESS;
    }

    *pNeedReturn = GMM_FALSE;

    return GMM_SUCCESS;
}
VOS_UINT8 Gmm_IECheck_AttachAccept(NAS_MSG_FOR_PCLINT_STRU *pMsg)
{
    VOS_UINT8   ucIeOffset = GMM_MSG_LEN_ATTACH_ACCEPT;                         /* 定义临时变量存储存储IE的偏移量           */
    VOS_UINT8   ucEndFlg   = GMM_FALSE;                                         /* 初期化结束标志为FALSE                    */
    VOS_UINT8   ucResult   = GMM_SUCCESS;
    VOS_UINT8   ucNeedReturn = GMM_TRUE;

    ucResult = Gmm_AttachAccept_Mandatory_Ie_Check(pMsg, &ucNeedReturn);

    if (GMM_TRUE == ucNeedReturn)
    {
        return ucResult;
    }

    /* 可选IE检查 */
    for (; ((ucIeOffset < pMsg->ulNasMsgSize) && (GMM_FALSE == ucEndFlg)); )
    {                                                                           /* 存储IE的偏移量小于空口消息的长度         */
        switch (pMsg->aucNasMsg[ucIeOffset])
        {                                                                       /* IEI                                      */
        case GMM_IEI_P_TMSI_SIGNATURE:                                          /* P-TMSI signature，不需要检查             */
            ucIeOffset += 4;
            break;
        case GMM_IEI_NEGOTIATED_READY_TIMER_VALUE:                              /* Negotiated READY timervalue，不需要检查  */
            ucIeOffset += 2;
            break;
        case GMM_IEI_ALLOCATED_PTMSI:                                           /* Allocated P-TMSI                         */
            ucNeedReturn = GMM_TRUE;

            ucResult = Gmm_IeCheck_Allocated_Ptmsi(pMsg, ucIeOffset, &ucNeedReturn);

            if (GMM_TRUE == ucNeedReturn)
            {
                return ucResult;
            }
            ucIeOffset += 7;
            break;
        case GMM_IEI_MS_IDENTITY:                                               /* MS identity                              */
            ucNeedReturn = GMM_TRUE;

            ucResult = Gmm_IeCheck_Ms_Identity(pMsg, &ucIeOffset, &ucNeedReturn);

            if (GMM_TRUE == ucNeedReturn)
            {
                return ucResult;
            }
            break;
        case GMM_IEI_GMM_CAUSE:                                                 /* GMM cause，不需要检查                    */
            ucIeOffset += 2;
            break;
        case GMM_IEI_T3302_VALUE:                                               /* T3302 value，不需要检查                  */
            ucNeedReturn = GMM_TRUE;

            ucResult = Gmm_IeCheck_T3302_Value(pMsg, ucIeOffset, &ucNeedReturn);

            if (GMM_TRUE == ucNeedReturn)
            {
                return ucResult;
            }
            ucIeOffset += 3;
            break;
        case GMM_IEI_CELL_NOTIFICATION:                                         /* Cell Notification，不需要检查            */
            ucIeOffset += 1;
            break;
        case GMM_IEI_EQUIVALENT_PLMNS:                                          /* Equivalent PLMNs                         */
            ucNeedReturn = GMM_TRUE;

            ucResult = Gmm_IeCheck_Equivalent_Plmns(pMsg, ucIeOffset, &ucNeedReturn);

            if (GMM_TRUE == ucNeedReturn)
            {
                return ucResult;
            }
            ucIeOffset += (2 + pMsg->aucNasMsg[ucIeOffset + 1]);
            break;
        case GMM_IEI_EMERGENCY_NUMBER_LIST:                                     /* Emergency Number List                    */
            if (ucIeOffset + 2 + pMsg->aucNasMsg[ucIeOffset + 1] <= pMsg->ulNasMsgSize)
            {
                ucIeOffset += (2 + pMsg->aucNasMsg[ucIeOffset + 1]);
            }
            else
            {
                ucEndFlg = GMM_TRUE;
            }
            break;
        default:
            if(GMM_IEI_NETWORK_FEATURE_SUPPORT == (pMsg->aucNasMsg[ucIeOffset] & 0xF0))
            { /* Network feature support                  */
                ucIeOffset += 1;
            }
            /* referenc to 24.008 8.5 */
            else if(GMM_IEI_COMPREHENSION_REQUIRED == (pMsg->aucNasMsg[ucIeOffset] & 0xF0))
            {
                return GMM_FAILURE;
            }
            /* TV类型的IE */
            else if(GMM_IEI_FORMAT_T_TYPE == (pMsg->aucNasMsg[ucIeOffset] & GMM_IEI_FORMAT_TYPE_INFO))
            {
                ucIeOffset += 1;
            }
            /* TLV类型的IE */
            else if (ucIeOffset + 2 + pMsg->aucNasMsg[ucIeOffset + 1] < pMsg->ulNasMsgSize)
            {
                ucIeOffset += (2 + pMsg->aucNasMsg[ucIeOffset + 1]);
            }
            else
            {
                ucEndFlg = GMM_TRUE;
            }
            break;
        }
    }
    return GMM_SUCCESS;

}

VOS_VOID NAS_GMM_AttachAcceptHandle_UpdateEPlmn(
    GMM_MSG_RESOLVE_STRU               *pstAcceptIe,
    NAS_MSG_FOR_PCLINT_STRU            *pstMsg
)
{
    NAS_MML_EQUPLMN_INFO_STRU          *pstEPlmnList    = VOS_NULL_PTR;
    NAS_MML_PLMN_ID_STRU               *pstCurrCampPlmn = VOS_NULL_PTR;
    NAS_MML_EQUPLMN_INFO_STRU           stTmpEPlmnList;


    PS_MEM_SET(&stTmpEPlmnList, 0x00, sizeof(stTmpEPlmnList));

    pstEPlmnList = NAS_MML_GetEquPlmnList();
    pstCurrCampPlmn = NAS_MML_GetCurrCampPlmnId();

    /* 如果空口消息中没有EPLMN,保存当前网络为EPLMN */
    if (GMM_ATTACH_ACCEPT_IE_EQUIVALENT_PLMN_FLG
        != (pstAcceptIe->usOptionalIeMask
        & GMM_ATTACH_ACCEPT_IE_EQUIVALENT_PLMN_FLG))
    {
        stTmpEPlmnList.ucEquPlmnNum = 0x1;
        stTmpEPlmnList.astEquPlmnAddr[0] = *pstCurrCampPlmn;
    }
    else
    {
        /* 从IE中解析EquPlmn信息 */
        NAS_GMM_DecodeEquPlmnInfoIE(GMM_MMC_ACTION_ATTACH,
                                    pstAcceptIe,
                                    pstMsg,
                                    &stTmpEPlmnList);
    }

    /* 空口消息中EPLMN与保存的不同,将EPLMN保存到MML全局变量和更新NV */


    /* 将EPLMN保存到MML全局变量 */
    PS_MEM_CPY(pstEPlmnList, &stTmpEPlmnList, sizeof(NAS_MML_EQUPLMN_INFO_STRU));

    NAS_MML_SetEplmnValidFlg(VOS_TRUE);

    /* 将EPLMN更新到NV中 */
    NAS_GMM_WriteEplmnNvim(pstEPlmnList);

    return;
}
VOS_UINT32  NAS_GMM_IsAttachOtherCause(VOS_UINT16 usGmmCause )
{
     switch (usGmmCause)
     {
        /* 以下原因值都是协议明确描述的不能提供CM服务    */
        case NAS_MML_REG_FAIL_CAUSE_ILLEGAL_MS:
        case NAS_MML_REG_FAIL_CAUSE_ILLEGAL_ME:
        case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_AND_NON_GPRS_SERV_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_PLMN_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_LA_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_ROAM_NOT_ALLOW:
        case NAS_MML_REG_FAIL_CAUSE_GPRS_SERV_NOT_ALLOW_IN_PLMN:
        case NAS_MML_REG_FAIL_CAUSE_NO_SUITABL_CELL:
            return VOS_FALSE;
        default:
            return VOS_TRUE;
     }

}

#ifdef  __cplusplus
  #if  __cplusplus
  }
  #endif
#endif

