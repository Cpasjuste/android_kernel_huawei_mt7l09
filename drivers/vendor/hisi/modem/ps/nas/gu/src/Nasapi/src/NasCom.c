
#include "vos.h"
#include "NasComm.h"
#include "MM_Inc.h"
#include "NasMmlCtx.h"
#include "SmInclude.h"

#include "rabmdef.h"


#ifdef  __cplusplus
  #if  __cplusplus
  extern "C"{
  #endif
#endif


#define    THIS_FILE_ID        PS_FILE_ID_NAS_COM_C

/* 改RABM_TIMER_NAME_FD_FLUX_DETECT定时器为32k定时器 */
/* 当前默认挂32K时钟，否则需在下表明确指出 */
NAS_TIMER_PRECISION_STRU g_stNasTimerPrcision[]=
{
   {WUEPS_PID_MMA,  MMA_TIMER_FOR_PC_REPLAY,            VOS_TIMER_NO_PRECISION},
   {WUEPS_PID_RABM, RABM_TIMER_NAME_FD_FLUX_DETECT,     VOS_TIMER_NO_PRECISION},
};

/*lint -save -e958 */

VOS_TIMER_PRECISION_ENUM_UINT32 NAS_GetTimerPrecision(
    VOS_PID                             Pid,
    VOS_UINT32                          ulName
)
{
    VOS_UINT32                      i;
    VOS_TIMER_PRECISION_ENUM_UINT32 ulPrecision;

    /* 默认为有精度要求，挂在32K时钟上 */
    ulPrecision = VOS_TIMER_PRECISION_5;

    for(i=0; i<( sizeof(g_stNasTimerPrcision)/sizeof(NAS_TIMER_PRECISION_STRU) ); i++)
    {
        if (  (Pid == g_stNasTimerPrcision[i].ulPid)
            &&( (ulName == g_stNasTimerPrcision[i].ulTimerName)
              ||(NAS_TIMER_ALL == g_stNasTimerPrcision[i].ulTimerName)))
        {
            ulPrecision = g_stNasTimerPrcision[i].ulPrecision;
            break;
        }
    }

    return ulPrecision;
}
VOS_UINT32 NAS_StartRelTimer(HTIMER *phTm, VOS_PID Pid, VOS_UINT32 ulLength,
    VOS_UINT32 ulName, VOS_UINT32 ulParam, VOS_UINT8 ucMode)
{
    VOS_UINT32                          ulRet;
    NAS_TIMER_OPERATION_STRU            stTimer;
    VOS_TIMER_PRECISION_ENUM_UINT32     ulPrecision;

    if ( ulLength >= VOS_TIMER_MAX_LENGTH )
    {
        ulLength = VOS_TIMER_MAX_LENGTH - 1;
    }

    ulPrecision = NAS_GetTimerPrecision(Pid, ulName);

    ulRet = VOS_StartRelTimer(phTm, Pid, ulLength, ulName, ulParam, ucMode, ulPrecision);

    stTimer.MsgHeader.ulSenderCpuId    = VOS_LOCAL_CPUID;
    stTimer.MsgHeader.ulSenderPid      = Pid;
    stTimer.MsgHeader.ulReceiverCpuId  = VOS_LOCAL_CPUID;
    stTimer.MsgHeader.ulReceiverPid    = VOS_PID_TIMER;
    stTimer.MsgHeader.ulLength         = sizeof(NAS_TIMER_OPERATION_STRU) - VOS_MSG_HEAD_LENGTH;

    if ((WUEPS_PID_MMA == Pid) || (WUEPS_PID_RABM == Pid))
    {
        stTimer.MsgHeader.ulMsgName = ulParam;
    }
    else if (WUEPS_PID_SM == Pid)
    {
        if (0xff == ulParam)
        {
            stTimer.MsgHeader.ulMsgName = ulParam;
        }
        else
        {
            stTimer.MsgHeader.ulMsgName = ulName;
        }
    }
    else
    {
        stTimer.MsgHeader.ulMsgName = ulName;
    }

    stTimer.ulTimeAction        = NAS_TIMER_OPERATION_START;
    stTimer.ulTimeLen           = ulLength;

    OM_TraceMsgHook(&stTimer);

    NAS_TIMER_EventReport(stTimer.MsgHeader.ulMsgName,Pid, NAS_OM_EVENT_TIMER_OPERATION_START);

    return ulRet;
}


VOS_UINT32 NAS_StopRelTimer(VOS_PID Pid, VOS_UINT32 ulName, HTIMER *phTm)
{
    VOS_UINT32                          ulRet;
    NAS_TIMER_OPERATION_STRU            stTimer;

    VOS_UINT32                          ulTimerRemainLen;

    ulTimerRemainLen = 0;

    if ((VOS_NULL_PTR != phTm)
     && (VOS_NULL_PTR != *phTm))
    {
        if ( VOS_OK != VOS_GetRelTmRemainTime(phTm, &ulTimerRemainLen ) )
        {
            ulTimerRemainLen = 0;
        }
    }

    if ((0 != ulTimerRemainLen) && (VOS_FALSE == g_ucSmTimerFiveExpireFlag))
    {
        NAS_TIMER_EventReport(ulName, Pid, NAS_OM_EVENT_TIMER_OPERATION_STOP);
    }

    ulRet = VOS_StopRelTimer(phTm);


    stTimer.MsgHeader.ulSenderCpuId    = VOS_LOCAL_CPUID;
    stTimer.MsgHeader.ulSenderPid      = Pid;
    stTimer.MsgHeader.ulReceiverCpuId  = VOS_LOCAL_CPUID;
    stTimer.MsgHeader.ulReceiverPid    = VOS_PID_TIMER;
    stTimer.MsgHeader.ulLength         = sizeof(NAS_TIMER_OPERATION_STRU) - VOS_MSG_HEAD_LENGTH;

    stTimer.MsgHeader.ulMsgName = ulName;

    stTimer.ulTimeAction        = NAS_TIMER_OPERATION_STOP;
    stTimer.ulTimeLen           = 0x0;

    OM_TraceMsgHook(&stTimer);

    return ulRet;
}


VOS_INT8 NAS_ConvertCustomMsRelToPsUeRel(
    NAS_MML_3GPP_REL_ENUM_UINT8         enCustomMsRel
)
{
    VOS_INT8                            cVersion;

    switch (enCustomMsRel)
    {
        case NAS_MML_3GPP_REL_R4:
            cVersion = PS_PTL_VER_R4;
            break;

        case NAS_MML_3GPP_REL_R5:
            cVersion = PS_PTL_VER_R5;
            break;

        case NAS_MML_3GPP_REL_R6:
            cVersion = PS_PTL_VER_R6;
            break;

        case NAS_MML_3GPP_REL_R7:
            cVersion = PS_PTL_VER_R7;
            break;

        case NAS_MML_3GPP_REL_R8:
            cVersion = PS_PTL_VER_R8;
            break;

        case NAS_MML_3GPP_REL_R9:
            cVersion = PS_PTL_VER_R9;
            break;

        default:
            cVersion = PS_UE_REL_VER;
            break;
    }

    return cVersion;
}


VOS_INT8 NAS_Common_Get_Supported_3GPP_Version(VOS_UINT32 ulCnDomainId)
{
    VOS_INT8                           cVersion;
    VOS_INT8                           cVersionFromNV;
    NAS_MML_MS_3GPP_REL_STRU          *pstMs3GppRel     = VOS_NULL_PTR;
    NAS_MML_NET_RAT_TYPE_ENUM_UINT8    enRatType;
    NAS_MML_NETWORK_3GPP_REL_STRU     *pstNetwork3GppRel= VOS_NULL_PTR    ;

    cVersion            = PS_UE_REL_VER;
    pstMs3GppRel        = NAS_MML_GetMs3GppRel();
    enRatType           = NAS_MML_GetCurrNetRatType();
    pstNetwork3GppRel   = NAS_MML_GetNetwork3GppRel();

    if (NAS_MML_NET_RAT_TYPE_GSM == enRatType)
    {
        cVersionFromNV = NAS_ConvertCustomMsRelToPsUeRel(pstMs3GppRel->enMsGsmRel);

        if (cVersionFromNV > PS_UE_REL_VER)
        {
            cVersion = PS_UE_REL_VER;
        }
        else
        {
            cVersion = cVersionFromNV;
        }

        if (MMA_MMC_SRVDOMAIN_CS == ulCnDomainId)
        {
            switch (pstMs3GppRel->enMsMscRel)
            {
                case PS_CUSTOM_REL_VER_R97:
                    cVersion = PS_PTL_VER_PRE_R99;
                    break;

                case PS_CUSTOM_REL_VER_AUTO:
                    if (GRRMM_MSC_RELEASE98_OR_OLDER == pstNetwork3GppRel->enNetMscRel)
                    {
                        cVersion = PS_PTL_VER_PRE_R99;
                    }
                    break;

                default:
                    break;
            }
        }
        else
        {/* PS DOMAIN */
            switch ( pstMs3GppRel->enMsSgsnRel)
            {
                case PS_CUSTOM_REL_VER_R97:
                    cVersion = PS_PTL_VER_PRE_R99;
                    break;

                case PS_CUSTOM_REL_VER_AUTO:
                    if (GRRMM_SGSN_RELEASE98_OR_OLDER == pstNetwork3GppRel->enNetSgsnRel)
                    {
                        cVersion = PS_PTL_VER_PRE_R99;
                    }
                    break;

                default:
                    break;
            }
        }
    }
    else
    {/* WCDMA */
        cVersionFromNV = NAS_ConvertCustomMsRelToPsUeRel(pstMs3GppRel->enMsWcdmaRel);

        if (cVersionFromNV > PS_UE_REL_VER)
        {
            cVersion = PS_UE_REL_VER;
        }
        else
        {
            cVersion = cVersionFromNV;
        }
    }

    return cVersion;
}



VOS_VOID NAS_ImsiToImsiStr(VOS_CHAR  *pcImsi)
{
    VOS_UINT32                          i;
    VOS_UINT8                          *pucImsi;

    pucImsi = NAS_MML_GetSimImsi();

    for (i = 1; i < 9; i++)
    {
        if (1 == i)
        {
            *pcImsi++ = ((pucImsi[i] >> 4) & 0x0f) + 0x30;
        }
        else
        {
            *pcImsi++ = (pucImsi[i] & 0x0f) + 0x30;
            *pcImsi++ = ((pucImsi[i] >> 4) & 0x0f) + 0x30;
        }
    }
}



VOS_BOOL NAS_PreventTestImsiRegFlg(VOS_VOID)
{
    VOS_CHAR                            *pcLabImsi1="00101";
    VOS_CHAR                            *pcLabImsi2="00102";
    VOS_CHAR                            *pcLabImsi3="00201";
    VOS_INT                             iImsiCmpRslt1,iImsiCmpRslt2,iImsiCmpRslt3;
    VOS_UINT32                          ulResult;
    NAS_PREVENT_TEST_IMSI_REG_STRU      stTestImsiFlg;
    VOS_CHAR                            acTmpImsi[16] = {0};
    VOS_UINT32                          ulLength;

    ulLength = 0;


    stTestImsiFlg.ucStatus = VOS_FALSE;
    stTestImsiFlg.ucActFlg = VOS_FALSE;

    NV_GetLength(en_NV_Item_PREVENT_TEST_IMSI_REG, &ulLength);
    if (ulLength > sizeof(stTestImsiFlg))
    {
        return VOS_FALSE;
    }
    ulResult = NV_Read(en_NV_Item_PREVENT_TEST_IMSI_REG,
                       &stTestImsiFlg,
                       ulLength);
    if (NV_OK != ulResult)
    {
        PS_LOG(WUEPS_PID_MM, VOS_NULL, PS_PRINT_WARNING, "NAS_PreventTestImsiRegFlg:WARNING: Read en_NV_Item_PREVENT_TEST_IMIS_REG Err");
        return VOS_FALSE;
    }

    /* stTestImsiFlg.ucStatus = 0; */
    /* stTestImsiFlg.ucActFlg = 0; */

    /* 在NV项激活的情况下，判定是否有特殊的号段 */
    if ((VOS_TRUE == stTestImsiFlg.ucStatus)
     && (VOS_TRUE == stTestImsiFlg.ucActFlg))
    {
        NAS_ImsiToImsiStr(acTmpImsi);

        iImsiCmpRslt1 = VOS_MemCmp(acTmpImsi, pcLabImsi1, 5);
        iImsiCmpRslt2 = VOS_MemCmp(acTmpImsi, pcLabImsi2, 5);
        iImsiCmpRslt3 = VOS_MemCmp(acTmpImsi, pcLabImsi3, 5);

        if ((0 == iImsiCmpRslt1)
          ||(0 == iImsiCmpRslt2)
          ||(0 == iImsiCmpRslt3))
        {
            return VOS_TRUE;
        }
    }

    return VOS_FALSE;

}


/*lint -restore */

#ifdef  __cplusplus
  #if  __cplusplus
  }
  #endif
#endif

