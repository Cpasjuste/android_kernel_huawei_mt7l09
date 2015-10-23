

#ifndef __NASEMMMRRC_H__
#define __NASEMMMRRC_H__



/*****************************************************************************
  1 Include Headfile
*****************************************************************************/

/*****************************************************************************
  1.1 Cplusplus Announce
*****************************************************************************/
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  #pragma pack(*)    �����ֽڶ��뷽ʽ
*****************************************************************************/
#if (VOS_OS_VER != VOS_WIN32)
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif
/*****************************************************************************
  2 macro
*****************************************************************************/
/*lint -e961*/
#define NAS_LMM_RRC_OPID_NOT_VAILID                (0xFFFFFFFF)
/*lint +e961*/

#define NAS_LMM_MRRC_NAS_MSG_MAX_LEN                   1024

#define NAS_LMM_RRC_SAVE_MSG_MAX_NUM                6
#define NAS_EMM_GET_MRRC_MGMT_DATA_ADDR()         (&(g_stEmmMrrcMgmtData[0]))

#if 0
#define NAS_EMM_GET_MRRC_SEND_STATE()             (gstEmmMrrcMgmtData.enMrrcSendState)
#define NAS_EMM_CHANGE_MRRC_SEND_STATE(STATE)     (gstEmmMrrcMgmtData.enMrrcSendState = STATE)
#endif

#define NAS_EMM_INIT_MM_RRC_OPID()                (gulMmRrcOpId = 0)
/*****************************************************************************
  3 Massage Declare
*****************************************************************************/


/*****************************************************************************
  4 Enum
*****************************************************************************/
#if 0
/*****************************************************************************
 ???��??    : NAS_EMM_MRRC_SEND_STATE_ENUM
 D-��������??  :
 ASN.1?����? :
 ???��?��?��  : MRRC��?��?���䨬?
*****************************************************************************/
enum NAS_EMM_MRRC_SEND_STATE_ENUM
{
    NAS_EMM_MRRC_SEND_STATE_READY       = 0,
    NAS_EMM_MRRC_SEND_STATE_PENDING        ,
    NAS_EMM_MRRC_SEND_STATE_BUTT
};
typedef VOS_UINT32 NAS_EMM_MRRC_SEND_STATE_ENUM_UINT32;
#endif
/*****************************************************************************
  5 STRUCT
*****************************************************************************/
/*****************************************************************************
 �ṹ��    : NAS_EMM_MRRC_MGMT_DATA_STRU
 Э�����  :
 ASN.1���� :
 �ṹ˵��  : NAS_EMM_MRRC_MGMT_DATA_STRU���ݽṹ
*****************************************************************************/
typedef struct
{
    VOS_UINT32                          ulEsmMmOpId;       /* ��¼Esm��EMM���OPID*/
    VOS_UINT32                          ulMmRrcOpId;       /* ��¼EMM��RRC���OPID*/
    NAS_EMM_MSG_TYPE_ENUM_UINT32        enLastMsgId;       /* ��¼EMM��RRC���͵���ϢID*/

    #if 0
    NAS_EMM_MRRC_SEND_STATE_ENUM_UINT32 enMrrcSendState;    /* ????MRRC��?����?�����䨬?*/
    #endif
    VOS_UINT32                          ulHoWaitSysInfoFlag;
    VOS_UINT32                          ulNasMsgLength;     /* ??????RRC����?��???���?3��?��*/
    VOS_UINT8                           aucNasMsgContent[NAS_LMM_MRRC_NAS_MSG_MAX_LEN]; /*??????RRC����?��???���??����Y */
}NAS_EMM_MRRC_MGMT_DATA_STRU;

/* ?�����¡�|����o����y��?����D��?����? */
typedef VOS_UINT32 ( * NAS_LMM_SEND_MSG_RESULT_FAIL_ACTION_FUN )
(
    VOS_VOID *  /* ???��???? */,VOS_UINT32 *
);

typedef VOS_UINT32 ( * NAS_LMM_SEND_MSG_RESULT_SUCC_ACTION_FUN )
(
    VOS_VOID *  /* ???��???? */
);


/* ?�����¡���?��11 */
typedef struct
{
    NAS_EMM_MSG_TYPE_ENUM_UINT32               enMsgId;            /* ???��ID*/
    NAS_LMM_SEND_MSG_RESULT_FAIL_ACTION_FUN         pfFailActionFun;    /* ����㨹?������o����y */
    NAS_LMM_SEND_MSG_RESULT_SUCC_ACTION_FUN         pfSuccActionFun;    /* 3��1|?������o����y */
}NAS_LMM_SEND_MSG_RESULT_ACT_STRU;
/*****************************************************************************
  6 UNION
*****************************************************************************/


/*****************************************************************************
  7 Extern Global Variable
*****************************************************************************/
extern NAS_EMM_MRRC_MGMT_DATA_STRU      g_stEmmMrrcMgmtData[NAS_LMM_RRC_SAVE_MSG_MAX_NUM];

extern NAS_LMM_SEND_MSG_RESULT_ACT_STRU  gstEmmMrrcSendMsgResultActTbl[17];
extern VOS_UINT32        g_ulSendMsgResultActTblLen;

extern VOS_UINT32              g_ulSaveMsgNum;


/*****************************************************************************
  8 Fuction Extern
*****************************************************************************/
extern VOS_VOID    NAS_EMM_IntraEstReq(const NAS_EMM_MRRC_DATA_REQ_STRU *pMsg);
extern VOS_VOID    NAS_EMM_RrcEstReq(const NAS_EMM_MRRC_DATA_REQ_STRU *pMsg);
extern VOS_VOID    NAS_EMM_RrcDataReq(const NAS_EMM_MRRC_DATA_REQ_STRU *pMsg,VOS_UINT32 ulMmRrcOpId);
extern VOS_VOID    NAS_EMM_MrrcConnectFailInd(LRRC_LNAS_EST_RESULT_ENUM_UINT32 enEstResult);
extern VOS_VOID    NAS_EMM_SndRrcRelReq(NAS_LMM_BARRED_IND_ENUM_UINT32 enBarStatus);

extern VOS_VOID    NAS_EMM_MrrcEstInit(VOS_VOID);
extern VOS_VOID    NAS_EMM_MrrcRelInit(VOS_VOID);

extern VOS_UINT32  NAS_EMM_MsRrcConnRelInitSsWaitRrcRelMsgDrbSetupInd(VOS_UINT32 ulMsgId, const VOS_VOID *pMsgStru);
extern VOS_UINT32  NAS_EMM_MsRrcConnRelInitSsWaitRrcRelMsg3440Exp(VOS_UINT32 ulMsgId, const VOS_VOID *pMsgStru);
extern VOS_UINT32 NAS_EMM_MsRrcConnRelInitSsWaitRrcRelMsgReattachDelayTmrExp(VOS_UINT32 ulMsgId, const VOS_VOID *pMsgStru);

extern VOS_VOID    NAS_EMM_MrrcMgmtDataInit( VOS_VOID );
extern VOS_UINT32  NAS_EMM_MrrcAllocMmRrcOpId( VOS_VOID );
/*extern VOS_UINT32  NAS_EMM_ChkMmRrcOpId( VOS_UINT32 ulOpId );*/
/*extern VOS_VOID    NAS_EMM_ClrMrrcMgmtData( NAS_EMM_STATE_TI_ENUM_UINT16 enTmr );*/

extern VOS_VOID  NAS_EMM_SaveMrrcDataReqMsg
(
    const NAS_EMM_MRRC_DATA_REQ_STRU        *pstMsg,
    VOS_UINT32                              ulMmRrcOpId
);
extern NAS_EMM_MRRC_MGMT_DATA_STRU* NAS_EMM_FindMsgInDataReqBuffer(VOS_UINT32 ulOpId);

extern VOS_VOID  NAS_EMM_ClrAllUlDataReqBufferMsg( VOS_VOID );

extern VOS_VOID  NAS_EMM_ClrAllUlDataReqBufferMsgExceptMtDetachAcc( VOS_VOID );

extern VOS_VOID  NAS_EMM_ClrDataReqBufferMsg
(
    VOS_UINT32                          ulMmRrcOpId
);
extern VOS_VOID  NAS_EMM_SaveCurMrrcDataReqMsg( NAS_EMM_MRRC_MGMT_DATA_STRU    *pEmmMrrcMgmtData );
extern VOS_UINT32  NAS_EMM_ClrHoWaitSysInfoBufferMsg( VOS_VOID );

extern VOS_UINT32  NAS_EMM_MsRrcConnEstInitSsWaitRrcConnMsgSysinfo( VOS_UINT32  ulMsgId,
                                                             VOS_VOID   *pMsgStru );
extern VOS_UINT32  NAS_EMM_MsRrcConnEstInitSsWaitRrcConnMsgT3412Exp( VOS_UINT32  ulMsgId,
                                                              VOS_VOID   *pMsgStru  );
extern VOS_UINT32  NAS_EMM_MsRrcConnEstInitWaitRrcConnMsgEsmDataReq (VOS_UINT32  ulMsgId,
                                                              VOS_VOID   *pMsgStru  );
extern VOS_VOID    NAS_EMM_FillEstInfo(
                                LRRC_LMM_EST_REQ_STRU              *pstEstReqMsg,
                                NAS_EMM_MSG_TYPE_ENUM_UINT32        enEmmMsgType );
/*leili modify for isr begin*/
extern VOS_VOID  NAS_EMM_CcoInterSystemSuccProc( VOS_VOID );          
extern VOS_VOID  NAS_EMM_CcoInterSystemFailProc( VOS_VOID );
/*leili modify for isr end*/
/*****************************************************************************
  9 OTHERS
*****************************************************************************/


#if (VOS_OS_VER != VOS_WIN32)
#pragma pack()
#else
#pragma pack(pop)
#endif


#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of ����ģ��+����ģ��+Interface.h */