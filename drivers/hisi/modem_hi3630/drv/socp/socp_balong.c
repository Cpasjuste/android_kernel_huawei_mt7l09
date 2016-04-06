
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include "socp_balong.h"
#include "bsp_clk.h"
#include "bsp_version.h"
#include "bsp_dump.h"
#include "bsp_nvim.h"
#include "drv_nv_id.h"
/* log2.0 2014-03-19 Begin:*/
#include "drv_nv_def.h"
#include "bsp_om.h"
#include "bsp_softtimer.h"
/* log2.0 2014-03-19 End*/
#ifdef CONFIG_MODULE_VIC
#include "bsp_vic.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

SOCP_GBL_STATE g_strSocpStat = {0};
SOCP_DEBUG_INFO_S g_stSocpDebugInfo;

/* �������ȼ����� */
#define SOCP_ENCSRC_TASK_PRO    79
#define SOCP_ENCDST_TASK_PRO    81
#define SOCP_DECSRC_TASK_PRO    79
#define SOCP_DECDST_TASK_PRO    81
/* SOCP����ַ */
u32 g_SocpRegBaseAddr = 0;
/* �жϴ������� */
u32 socp_app_int_handler(void);
/*lint -save -e512*/
spinlock_t lock;
/*lint -restore +e512*/

/* log2.0 2014-03-19 Begin:*/
SOCP_ENC_DST_BUF_LOG_CFG_STRU g_stEncDstBufLogConfig = {0};
/* log2.0 2014-03-19 End*/

#if(FEATURE_SOCP_ON_DEMAND == FEATURE_ON)
/* ��ʼ�����������˯�� */
SOCP_VOTE_INFO_STRU g_stVoteInfo[SOCP_VOTE_ID_BUTT] = {{SOCP_VOTE_FOR_WAKE,0}, {SOCP_VOTE_FOR_WAKE,0}, \
                                                        {SOCP_VOTE_FOR_WAKE,0}, {SOCP_VOTE_FOR_WAKE,0}, \
                                                        {SOCP_VOTE_FOR_WAKE,0}, {SOCP_VOTE_FOR_WAKE,0}};
SOCP_POWER_STATE_ENUM_U32 g_ulSocpPowerState = SOCP_POWER_ON;
BSP_U32 g_ulSocpOnDemand = BSP_TRUE;
struct semaphore g_stSocpPowerSem;
SOCP_VCC_STRU * g_stSocpVcc;
/* ����SOCP����Ŀ�Ĵ����ж�����Ĵ��� */
u32 g_ul_encdst_tran_int_mask = 0;
/* ����SOCP����Ŀ������ж�����Ĵ��� */
u32 g_ul_encdst_over_int_mask = 0;
/* ����SOCP����Ŀ�Ĵ����ж�����Ĵ��� */
u32 g_ul_decdst_tran_int_mask = 0;

/*****************************************************************************
* �� �� ��  : socp_chan_is_idle
*
* ��������  : ����Ŀ��ͨ������Ƿ����
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : s32
*****************************************************************************/
s32 socp_chan_is_idle(void)
{
    u32 i;
    u32 u32WritePtr;
    u32 u32ReadPtr;
    u32 u32EncChanState;
    u32 u32DecChanState;

    /* �ж�ͨ�������Ƿ��Ѿ����� */
    SOCP_REG_READ(SOCP_REG_ENCSTAT, u32EncChanState);
    if(u32EncChanState != 0)
    {
        /* ׼���µ� */
        g_ulSocpPowerState = SOCP_POWER_OFF_REQ;
        printk("%s: enc src busy 0x%x, SOCP_POWER_OFF_REQ\n", __FUNCTION__, u32EncChanState);
        return BSP_ERROR;
    }

    /* �жϱ���Ŀ��ͨ����дָ���Ƿ���� */
    for(i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
    {
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFWPTR(i), u32WritePtr);
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFRPTR(i), u32ReadPtr);
        if(u32WritePtr != u32ReadPtr)
        {
            g_ulSocpPowerState = SOCP_POWER_OFF_REQ;
            printk("%s: enc dst chan 0x%x not read done, SOCP_POWER_OFF_REQ\n", __FUNCTION__, i);
            return BSP_ERROR;            
        }
    }

    /* �ж�ͨ�������Ƿ��Ѿ����� */
    SOCP_REG_READ(SOCP_REG_DECSTAT, u32DecChanState);
    if(u32DecChanState != 0)
    {
        /* ׼���µ� */
        g_ulSocpPowerState = SOCP_POWER_OFF_REQ;
        printk("%s: dec src busy 0x%x, SOCP_POWER_OFF_REQ\n", __FUNCTION__, u32DecChanState);
        return BSP_ERROR;
    }

    /* �жϽ���Ŀ��ͨ����дָ���Ƿ���� */
    for(i = 0; i < SOCP_MAX_DECDST_CHN; i++)
    {
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFWPTR(i), u32WritePtr);
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFRPTR(i), u32ReadPtr);
        if(u32WritePtr != u32ReadPtr)
        {
            g_ulSocpPowerState = SOCP_POWER_OFF_REQ;
            printk("%s: dec dst chan 0x%x not read done, SOCP_POWER_OFF_REQ\n", __FUNCTION__, i);
            return BSP_ERROR;            
        }
    }

    return BSP_OK;
}
#endif
/*****************************************************************************
* �� �� ��  : socp_get_idle_buffer
*
* ��������  : ��ѯ���л�����
*
* �������  :  pRingBuffer       ����ѯ�Ļ���buffer
                    pRWBuffer         ����Ļ���buffer
*
* �������  : ��
*
* �� �� ֵ  :  ��
*****************************************************************************/
void socp_get_idle_buffer(SOCP_RING_BUF_S *pRingBuffer, SOCP_BUFFER_RW_STRU *pRWBuffer)
{
    if(pRingBuffer->u32Write < pRingBuffer->u32Read)
    {
        /* ��ָ�����дָ�룬ֱ�Ӽ��� */
        pRWBuffer->pBuffer = (char *)((unsigned long)pRingBuffer->u32Write);
        pRWBuffer->u32Size = (u32)(pRingBuffer->u32Read - pRingBuffer->u32Write - 1);
        pRWBuffer->pRbBuffer = (char *)BSP_NULL;
        pRWBuffer->u32RbSize = 0;
    }
    else
    {
        /* дָ����ڶ�ָ�룬��Ҫ���ǻؾ� */
        if(pRingBuffer->u32Read != pRingBuffer->u32Start)
        {
            pRWBuffer->pBuffer = (char *)((unsigned long)pRingBuffer->u32Write);
            pRWBuffer->u32Size = (u32)(pRingBuffer->u32End - pRingBuffer->u32Write + 1);
            pRWBuffer->pRbBuffer = (char *)((unsigned long)pRingBuffer->u32Start);
            pRWBuffer->u32RbSize = (u32)(pRingBuffer->u32Read - pRingBuffer->u32Start - 1);
        }
        else
        {
            pRWBuffer->pBuffer = (char *)((unsigned long)pRingBuffer->u32Write);
            pRWBuffer->u32Size = (u32)(pRingBuffer->u32End - pRingBuffer->u32Write);
            pRWBuffer->pRbBuffer = (char *)BSP_NULL;
            pRWBuffer->u32RbSize = 0;
        }
    }

    return;
}

/*****************************************************************************
* �� �� ��  : socp_get_data_buffer
*
* ��������  : ��ȡ���л�����������
*
* �������  :  pRingBuffer       ����ѯ�Ļ���buffer
                    pRWBuffer         ����Ļ���buffer
*
* �������  : ��
*
* �� �� ֵ  :  ��
*****************************************************************************/
void socp_get_data_buffer(SOCP_RING_BUF_S *pRingBuffer, SOCP_BUFFER_RW_STRU *pRWBuffer)
{
    if(pRingBuffer->u32Read <= pRingBuffer->u32Write)
    {
        /* дָ����ڶ�ָ�룬ֱ�Ӽ��� */
        pRWBuffer->pBuffer = (char *)((unsigned long)pRingBuffer->u32Read);
        pRWBuffer->u32Size = (u32)(pRingBuffer->u32Write - pRingBuffer->u32Read);
        pRWBuffer->pRbBuffer = (char *)BSP_NULL;
        pRWBuffer->u32RbSize = 0;
    }
    else
    {
        /* ��ָ�����дָ�룬��Ҫ���ǻؾ� */
        pRWBuffer->pBuffer = (char *)((unsigned long)pRingBuffer->u32Read);
        pRWBuffer->u32Size = (u32)(pRingBuffer->u32End - pRingBuffer->u32Read + 1);
        pRWBuffer->pRbBuffer = (char *)((unsigned long)pRingBuffer->u32Start);
        pRWBuffer->u32RbSize = (u32)(pRingBuffer->u32Write - pRingBuffer->u32Start);
    }

    return;
}

/*****************************************************************************
* �� �� ��  : socp_write_done
*
* ��������  : ���»�������дָ��
*
* �������  :  pRingBuffer       �����µĻ���buffer
                    u32Size          ���µ����ݳ���
*
* �������  : ��
*
* �� �� ֵ  :  ��
*****************************************************************************/
void socp_write_done(SOCP_RING_BUF_S *pRingBuffer, u32 u32Size)
{
    u32 tmp_size;

    tmp_size = pRingBuffer->u32End - pRingBuffer->u32Write + 1;
    if(tmp_size > u32Size)
    {
        /* SCM�����CACHE�ڴ� */
        (void)SOCP_FLUSH_CACHE(pRingBuffer->u32Write, u32Size);
        pRingBuffer->u32Write += u32Size;
    }
    else
    {
        u32 rb_size = u32Size - tmp_size;
        /* SCM�����CACHE�ڴ� */
        (void)SOCP_FLUSH_CACHE(pRingBuffer->u32Write, tmp_size);
        (void)SOCP_FLUSH_CACHE(pRingBuffer->u32Start, rb_size);
        pRingBuffer->u32Write = pRingBuffer->u32Start + rb_size;
    }
    return;
}
/*****************************************************************************
* �� �� ��  : socp_read_done
*
* ��������  : ���»������Ķ�ָ��
*
* �������  :  pRingBuffer       �����µĻ���buffer
                    u32Size          ���µ����ݳ���
*
* �������  : ��
*
* �� �� ֵ  :  ��
*****************************************************************************/
void socp_read_done(SOCP_RING_BUF_S *pRingBuffer, u32 u32Size)
{
    pRingBuffer->u32Read += u32Size;
    if(pRingBuffer->u32Read > pRingBuffer->u32End)
    {
        pRingBuffer->u32Read -= pRingBuffer->u32Length;
    }
}

/*****************************************************************************
* �� �� ��  : bsp_socp_clean_encsrc_chan
*
* ��������  : ��ձ���Դͨ����ͬ��V9 SOCP�ӿ�
*
* �������  : enSrcChanID       ����ͨ����
*
* �������  : ��
*
* �� �� ֵ  : BSP_OK
*****************************************************************************/
u32 bsp_socp_clean_encsrc_chan(SOCP_CODER_SRC_ENUM_U32 enSrcChanID)
{
    u32 ulResetFlag;
    u32 i;
    u32 ulChanID;
    u32 ulChanType;

    ulChanID    = SOCP_REAL_CHAN_ID(enSrcChanID);
    ulChanType  = SOCP_REAL_CHAN_TYPE(enSrcChanID);

    SOCP_CHECK_CHAN_TYPE(ulChanType, SOCP_CODER_SRC_CHAN);
    SOCP_CHECK_ENCSRC_CHAN_ID(ulChanID);

    /* ��λͨ�� */
    SOCP_REG_SETBITS(SOCP_REG_ENCRST, ulChanID, 1, 1);

    /* �ȴ�ͨ������ */
    for(i=0; i< SOCP_RESET_TIME; i++)
    {
        ulResetFlag = SOCP_REG_GETBITS(SOCP_REG_ENCRST, ulChanID, 1);

        if(0 == ulResetFlag)
        {
            break;
        }
    }

    if(SOCP_RESET_TIME == i)
    {
        printk("SocpCleanEncChan failed!\n");
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_reset_enc_chan
*
* ��������  : ��λ����ͨ��
*
* �������  : u32ChanID       ����ͨ����
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
s32 socp_reset_enc_chan(u32 u32ChanID)
{
    u32  ResetFlag;
    u32  i;
    SOCP_ENCSRC_CHAN_S *pChan;

    pChan = &g_strSocpStat.sEncSrcChan[u32ChanID];

    /* ��λͨ�� */
    SOCP_REG_SETBITS(SOCP_REG_ENCRST, u32ChanID, 1, 1);

    /* �ȴ�ͨ������ */
    for(i=0; i<SOCP_RESET_TIME; i++)
    {
        ResetFlag = SOCP_REG_GETBITS(SOCP_REG_ENCRST, u32ChanID, 1);
        if(0 == ResetFlag)
        {
            break;
        }

        if((SOCP_RESET_TIME -1) == i)
        {
            printk("socp_reset_enc_chan 0x%x failed!\n", u32ChanID);
        }
    }

    /* ��������ͨ��*/
    /* д����ʼ��ַ��Դbuffer��ʼ��ַ�Ĵ���*/
    SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFADDR(u32ChanID),SOCP_VIRT_PHY(pChan->sEncSrcBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFWPTR(u32ChanID),SOCP_VIRT_PHY(pChan->sEncSrcBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFRPTR(u32ChanID), SOCP_VIRT_PHY(pChan->sEncSrcBuf.u32Start));
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG0(u32ChanID), 0, 27, pChan->sEncSrcBuf.u32Length);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG0(u32ChanID), 27, 5, 0);
    /* ���¶�дָ��*/
    g_strSocpStat.sEncSrcChan[u32ChanID].sEncSrcBuf.u32Read  = pChan->sEncSrcBuf.u32Start;
    g_strSocpStat.sEncSrcChan[u32ChanID].sEncSrcBuf.u32Write = pChan->sEncSrcBuf.u32Start;


    /* �������������������������RDbuffer����ʼ��ַ�ͳ��� */
    if(SOCP_ENCSRC_CHNMODE_LIST == pChan->eChnMode)
    {
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQADDR(u32ChanID),SOCP_VIRT_PHY(pChan->sRdBuf.u32Start));
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQRPTR(u32ChanID),SOCP_VIRT_PHY(pChan->sRdBuf.u32Start));
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQWPTR(u32ChanID), SOCP_VIRT_PHY(pChan->sRdBuf.u32Start));
        printk("\r\nsocp_reset_enc_chan: ID is %d, VBuf addr is 0x%X, PBuf addr is 0x%X.\r\n", u32ChanID, pChan->sRdBuf.u32Start, SOCP_VIRT_PHY(pChan->sRdBuf.u32Start));
        SOCP_REG_SETBITS(SOCP_REG_ENCSRC_RDQCFG(u32ChanID), 0, 16, pChan->sRdBuf.u32Length);
		SOCP_REG_SETBITS(SOCP_REG_ENCSRC_RDQCFG(u32ChanID), 16, 16, 0);

        g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf.u32Read  = pChan->sRdBuf.u32Start;
        g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf.u32Write = pChan->sRdBuf.u32Start;
    }

    /*������������*/
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 1, 2, pChan->eChnMode);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 4, 4, pChan->u32DestChanID);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 8, 2, pChan->ePriority);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 10, 1, pChan->u32BypassEn);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 16, 8, pChan->eDataType);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 11, 1, pChan->eDataTypeEn);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 31, 1, pChan->eDebugEn);

    /*���ͨ��������״̬��ʹ��ͨ��*/
    if(pChan->u32ChanEn)
    {
        SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32ChanID), 0, 1, 1);
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_reset_dec_chan
*
* ��������  : ��λ����ͨ��
*
* �������  : u32ChanID       ����ͨ����
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
s32 socp_reset_dec_chan(u32 u32ChanID)
{
    u32  u32ResetFlag;
    u32  i;
    SOCP_DECSRC_CHAN_S *pChan;

    pChan = &g_strSocpStat.sDecSrcChan[u32ChanID];

    /* ��λͨ�� */
    SOCP_REG_SETBITS(SOCP_REG_DECRST, u32ChanID, 1, 1);

    /* �ȴ�ͨ������ */
    for(i=0; i<SOCP_RESET_TIME; i++)
    {
        u32ResetFlag = SOCP_REG_GETBITS(SOCP_REG_DECRST, u32ChanID, 1);
        if(0 == u32ResetFlag)
        {
            break;
        }
        if((SOCP_RESET_TIME -1) == i)
        {
            printk("socp_reset_dec_chan 0x%x failed!\n", u32ChanID);
        }
    }

    /* ʹ�����ò����������� */
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFADDR(u32ChanID), SOCP_VIRT_PHY(pChan->sDecSrcBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFWPTR(u32ChanID), SOCP_VIRT_PHY(pChan->sDecSrcBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFRPTR(u32ChanID), SOCP_VIRT_PHY(pChan->sDecSrcBuf.u32Start));

    SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32ChanID), 0, 16, pChan->sDecSrcBuf.u32Length);
    SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32ChanID), 31, 1, pChan->eDataTypeEn);
    //SOCP_REG_SETBITS(SOCP_REG_DECSRC_RDQCFG(u32ChanID), 29, 1, 0);

    /* ���¶�Ӧͨ���Ķ�дָ��*/
    g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf.u32Read  = pChan->sDecSrcBuf.u32Start;
    g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf.u32Write = pChan->sDecSrcBuf.u32Start;

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_soft_free_encdst_chan
*
* ��������  : ���ͷű���Ŀ��ͨ��
*
* �������  : u32EncDstChanId       ����ͨ����
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
s32 socp_soft_free_encdst_chan(u32 u32EncDstChanId)
{
    u32 u32ChanID;
    u32 u32ChanType;
    SOCP_ENCDST_CHAN_S *pChan;

    u32ChanID   = SOCP_REAL_CHAN_ID(u32EncDstChanId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32EncDstChanId);

    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_CODER_DEST_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_ENCDST_CHN);

    pChan = &g_strSocpStat.sEncDstChan[u32ChanID];

    /* д����ʼ��ַ��Ŀ��buffer��ʼ��ַ�Ĵ���*/
    SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFADDR(u32ChanID), SOCP_VIRT_PHY(pChan->sEncDstBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFRPTR(u32ChanID), SOCP_VIRT_PHY(pChan->sEncDstBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFWPTR(u32ChanID), SOCP_VIRT_PHY(pChan->sEncDstBuf.u32Start));

    g_strSocpStat.sEncDstChan[u32ChanID].u32SetStat = SOCP_CHN_UNSET;
    g_strSocpStat.sEncDstChan[u32ChanID].sEncDstBuf.u32Write = pChan->sEncDstBuf.u32Start;
    g_strSocpStat.sEncDstChan[u32ChanID].sEncDstBuf.u32Read = pChan->sEncDstBuf.u32Start;

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_soft_free_decsrc_chan
*
* ��������  : ���ͷŽ���Դͨ��
*
* �������  : u32DecSrcChanId       ����ͨ����
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
s32 socp_soft_free_decsrc_chan(u32 u32DecSrcChanId)
{
    u32 u32ChanID;
    u32 u32ChanType;
    SOCP_DECSRC_CHAN_S *pDecSrcChan;

    u32ChanID   = SOCP_REAL_CHAN_ID(u32DecSrcChanId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DecSrcChanId);

    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECSRC_CHN);

    pDecSrcChan = &g_strSocpStat.sDecSrcChan[u32ChanID];

    /* д����ʼ��ַ��Ŀ��buffer��ʼ��ַ�Ĵ���*/
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFWPTR(u32ChanID), SOCP_VIRT_PHY(pDecSrcChan->sDecSrcBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFADDR(u32ChanID), SOCP_VIRT_PHY(pDecSrcChan->sDecSrcBuf.u32Start));
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFRPTR(u32ChanID), SOCP_VIRT_PHY(pDecSrcChan->sDecSrcBuf.u32Start));

    g_strSocpStat.sDecSrcChan[u32ChanID].u32SetStat = SOCP_CHN_UNSET;
    g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf.u32Write = pDecSrcChan->sDecSrcBuf.u32Start;
    g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf.u32Read = pDecSrcChan->sDecSrcBuf.u32Start;

    return BSP_OK;
}

#if 0
/*****************************************************************************
* �� �� ��  : socp_get_dec_rd_size
*
* ��������  :  ��ȡ����Դͨ��RDbuffer
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
u32 socp_get_dec_rd_size(u32 u32ChanID)
{
    SOCP_BUFFER_RW_STRU Buff;
    u32          PAddr;

    SOCP_REG_READ(SOCP_REG_DECSRC_RDQRPTR(u32ChanID), PAddr);
    g_strSocpStat.sDecSrcChan[u32ChanID].sDecRdBuf.u32Read = SOCP_PHY_VIRT(PAddr);
    SOCP_REG_READ(SOCP_REG_DECSRC_RDQWPTR(u32ChanID), PAddr);
    g_strSocpStat.sDecSrcChan[u32ChanID].sDecRdBuf.u32Write= SOCP_PHY_VIRT(PAddr);

    socp_get_data_buffer(&g_strSocpStat.sDecSrcChan[u32ChanID].sDecRdBuf, &Buff);
    return (Buff.u32Size + Buff.u32RbSize);
}
#endif

/*****************************************************************************
* �� �� ��  : socp_get_enc_rd_size
*
* ��������  :  ��ȡ����Դͨ��RDbuffer
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
u32 socp_get_enc_rd_size(u32 u32ChanID)
{
    SOCP_BUFFER_RW_STRU Buff;
    u32          PAddr;

    SOCP_REG_READ(SOCP_REG_ENCSRC_RDQRPTR(u32ChanID), PAddr);
    g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf.u32Read = SOCP_PHY_VIRT(PAddr);
    SOCP_REG_READ(SOCP_REG_ENCSRC_RDQWPTR(u32ChanID), PAddr);
    g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf.u32Write= SOCP_PHY_VIRT(PAddr);

    socp_get_data_buffer(&g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf, &Buff);
    return (Buff.u32Size + Buff.u32RbSize);
}

/*****************************************************************************
* �� �� ��   : socp_encsrc_rd_handler
*
* ��������  :  ����Դͨ��RDbuffer�жϴ�������
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_encsrc_rd_handler(u32 RdSize, u32 i)
{
    u32 u32ChanId;

    if (RdSize == g_strSocpStat.sEncSrcChan[i].u32LastRdSize)
    {
        if (g_strSocpStat.sEncSrcChan[i].rd_cb)
        {
            u32ChanId = SOCP_CHAN_DEF(SOCP_CODER_SRC_CHAN, i);
            (void)g_strSocpStat.sEncSrcChan[i].rd_cb(u32ChanId);

            g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpEncSrcTskRdCbCnt[i]++;
        }

        g_strSocpStat.sEncSrcChan[i].u32LastRdSize = 0;
    }
    else
    {
        g_strSocpStat.sEncSrcChan[i].u32LastRdSize = RdSize;
    }

    return;
}

/*****************************************************************************
* �� �� ��  : socp_encsrc_task
*
* ��������  : ģ��������:����Դ�жϣ�˫��
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
int socp_encsrc_task(void * data)
{
    u32 i;
    u32 IntHeadState = 0;
    u32 u32ChanId;
    unsigned long lock_flag;
    /* coverity[no_escape] */
    do{
        /* ��ʱ���߱��жϣ����������� */
        if(0 != down_interruptible(&g_strSocpStat.u32EncSrcSemID))
        {
            continue;
        }

        spin_lock_irqsave(&lock, lock_flag);
        IntHeadState = g_strSocpStat.u32IntEncSrcHeader;
        g_strSocpStat.u32IntEncSrcHeader = 0;
        g_strSocpStat.u32IntEncSrcRD = 0;
        spin_unlock_irqrestore(&lock, lock_flag);

        /* ���������ͷ'HISI'�������*/
        if (IntHeadState)
        {
            for (i = 0; i < SOCP_MAX_ENCSRC_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_ALLOCATED == g_strSocpStat.sEncSrcChan[i].u32AllocStat)
                {
                    if (IntHeadState & ((u32)1 << i))
                    {
                        if (g_strSocpStat.sEncSrcChan[i].event_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpEncSrcTskHeadCbOriCnt[i]++;
                            u32ChanId = SOCP_CHAN_DEF(SOCP_CODER_SRC_CHAN, i);
                            printk("\r\nsocp_encsrc_task: packet header Error Channel is %d\r\n", u32ChanId);
                            (void)g_strSocpStat.sEncSrcChan[i].event_cb(u32ChanId, SOCP_EVENT_PKT_HEADER_ERROR, 0);
                            g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpEncSrcTskHeadCbCnt[i]++;
                        }
                    }
                }
            }
        }
    } while (1);

    //return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_encdst_task
*
* ��������  : ģ��������:����Ŀ�ģ�App��
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
int socp_encdst_task(void * data)
{
    u32 i;
    u32 IntTfrState = 0;
    u32 IntOvfState = 0;
    u32 IntThresholdOvfState = 0;
    u32 u32ChanId;
    unsigned long lock_flag;

    /* log2.0 2014-03-19 Begin:*/
    SOCP_ENC_DST_BUF_LOG_CFG_STRU * cfg;
    u32 chanSet = 0;
    u32 transIntMask;
    
    cfg = bsp_socp_get_log_cfg(); 
    /* coverity[no_escape] */
    do{   
        if(TRUE == cfg->logOnFlag)
        {
            if(0 != down_timeout(&g_strSocpStat.u32EncDstSemID, (cfg->overTime)/5))
            {
                /* ��ʱ��������� */
            }            
        }
        else
        {
            /* ��ʱ���߱��жϣ����������� */
            if(0 != down_interruptible(&g_strSocpStat.u32EncDstSemID))
            {
                continue;
            }
        }

        spin_lock_irqsave(&lock, lock_flag);
        IntTfrState = g_strSocpStat.u32IntEncDstTfr;
        g_strSocpStat.u32IntEncDstTfr = 0;
        IntOvfState = g_strSocpStat.u32IntEncDstOvf;
        g_strSocpStat.u32IntEncDstOvf = 0;
        IntThresholdOvfState = g_strSocpStat.u32IntEncDstThresholdOvf;
        g_strSocpStat.u32IntEncDstThresholdOvf = 0;
        spin_unlock_irqrestore(&lock, lock_flag);

        /* �������봫������ж�*/
        if (IntTfrState)
        {
            for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_SET == g_strSocpStat.sEncDstChan[i].u32SetStat)
                {
                    if (IntTfrState & ((u32)1 << i))
                    {
                        if (g_strSocpStat.sEncDstChan[i].read_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstTskTrfCbOriCnt[i]++;
                            u32ChanId = SOCP_CHAN_DEF(SOCP_CODER_DEST_CHAN, i);
                            (void)g_strSocpStat.sEncDstChan[i].read_cb(u32ChanId);

                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstTskTrfCbCnt[i]++;
                        }
                    }
                }
            }
        }

        /* ��������Ŀ�� buffer ����ж�*/
        if (IntOvfState)
        {
            for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_SET == g_strSocpStat.sEncDstChan[i].u32SetStat)
                {
                    if (IntOvfState & ((u32)1 << i))
                    {
                        if (g_strSocpStat.sEncDstChan[i].event_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstTskOvfCbOriCnt[i]++;
                            u32ChanId = SOCP_CHAN_DEF(SOCP_CODER_DEST_CHAN, i);
                            (void)g_strSocpStat.sEncDstChan[i].event_cb(u32ChanId, SOCP_EVENT_OUTBUFFER_OVERFLOW, 0);

                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstTskOvfCbCnt[i]++;
                        }
                    }
                }
            }
        }

        /* ��������Ŀ�� buffer ��ֵ����ж�*/
        if (IntThresholdOvfState)
        {
            for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_SET == g_strSocpStat.sEncDstChan[i].u32SetStat)
                {
                    if (IntThresholdOvfState & ((u32)1 << (i + SOCP_ENC_DST_BUFF_THRESHOLD_OVF_BEGIN)))
                    {
                        if (g_strSocpStat.sEncDstChan[i].read_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstTskThresholdOvfCbOriCnt[i]++;
                            u32ChanId = SOCP_CHAN_DEF(SOCP_CODER_DEST_CHAN, i);
                            (void)g_strSocpStat.sEncDstChan[i].read_cb(u32ChanId);
                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstTskThresholdOvfCbCnt[i]++;
                        }
                    }
                }
            }
        }

        /* log2.0 2014-03-19 Begin:*/
        /* ��ʱ��������FLUSH���� */
        if((TRUE == cfg->logOnFlag) && ((!IntTfrState && !IntOvfState && !IntThresholdOvfState) || (TRUE == cfg->flushFlag)))
        {
#if(FEATURE_SOCP_ON_DEMAND ==FEATURE_ON)
            if(0 != down_interruptible(&g_stSocpPowerSem))
            {
                continue;
            }
            /* SOCPδ�ϵ� */
            if(SOCP_POWER_OFF == g_ulSocpPowerState)
            {
                up(&g_stSocpPowerSem);
                continue;
            }   
#endif            
            chanSet = socp_is_encdst_chan_empty();
            /* ��ȡ�����ж����μĴ��� */
            SOCP_REG_READ(SOCP_REG_ENC_MASK0, transIntMask);
#if(FEATURE_SOCP_ON_DEMAND ==FEATURE_ON)
            up(&g_stSocpPowerSem);
#endif
            for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_SET == g_strSocpStat.sEncDstChan[i].u32SetStat)
                {
                    /* ͨ�������ݣ����Ҵ����ж�δ���βŴ���������жϴ�������״̬����ʾ��һ��read doneδ��ɣ���Ҫ�ȴ� */
                    if((chanSet & ((BSP_U8)1 << i)) && (!(transIntMask & ((BSP_U8)1 << i))))
                    {
                        /* ���δ����ж� */
                        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, i, 1, 1);
                        if (g_strSocpStat.sEncDstChan[i].read_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstSoftOverTimeOriCnt[i]++;
                            u32ChanId = SOCP_CHAN_DEF(SOCP_CODER_DEST_CHAN, i);
                            (BSP_VOID)g_strSocpStat.sEncDstChan[i].read_cb(u32ChanId);
                            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstSoftOverTimeCnt[i]++;
                        }
                    }
                }
            }

            if(TRUE == cfg->flushFlag)
            {
                cfg->flushFlag = FALSE;
            }
        }   
        /* log2.0 2014-03-19 End*/
        
    } while (1);

    //return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_decsrc_event_handler
*
* ��������  : ����Դͨ���¼���������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void  socp_decsrc_event_handler(u32 id, u32 secIntState)
{
    u32 u32ChanId = SOCP_CHAN_DEF(SOCP_DECODER_SRC_CHAN, id);

    if (g_strSocpStat.sDecSrcChan[id].event_cb)
    {
        if (secIntState & 0x10)
        {
            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbOriCnt[id]++;

            (void)g_strSocpStat.sDecSrcChan[id].event_cb(u32ChanId, SOCP_EVENT_DECODER_UNDERFLOW, 0);

            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbCnt[id]++;
        }

        if (secIntState & 0x100)
        {
            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbOriCnt[id]++;

            (void)g_strSocpStat.sDecSrcChan[id].event_cb(u32ChanId, SOCP_EVENT_HDLC_HEADER_ERROR, 0);

            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbCnt[id]++;
        }

        if (secIntState & 0x1000)
        {
            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbOriCnt[id]++;

            (void)g_strSocpStat.sDecSrcChan[id].event_cb(u32ChanId, SOCP_EVENT_DATA_TYPE_ERROR, 0);

            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbCnt[id]++;
        }

        if (secIntState & 0x10000)
        {
            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbOriCnt[id]++;

            (void)g_strSocpStat.sDecSrcChan[id].event_cb(u32ChanId, SOCP_EVENT_CRC_ERROR, 0);

            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbCnt[id]++;
        }

        if (secIntState & 0x100000)
        {
            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbOriCnt[id]++;

            (void)g_strSocpStat.sDecSrcChan[id].event_cb(u32ChanId, SOCP_EVENT_PKT_LENGTH_ERROR, 0);

            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcTskErrCbCnt[id]++;
        }
    }
}

/*****************************************************************************
* �� �� ��  : socp_decsrc_handler
*
* ��������  : ����Դͨ���жϴ�������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void  socp_decsrc_handler(void)
{
    u32 IntState, secIntState;
    u32 u32ChanId;
    u32 i;

    if(g_strSocpStat.u32IntDecSrcErr)
    {
        IntState = g_strSocpStat.u32IntDecSrcErr;
        g_strSocpStat.u32IntDecSrcErr = 0;

        for(i=0;i<SOCP_MAX_DECSRC_CHN;i++)
        {
            /* ���ͨ���Ƿ�����*/

            if(SOCP_CHN_SET == g_strSocpStat.sDecSrcChan[i].u32SetStat)
            {
                secIntState = IntState>>i;
                u32ChanId = SOCP_CHAN_DEF(SOCP_DECODER_SRC_CHAN, i);

                if(g_strSocpStat.sDecSrcChan[i].rd_cb)
                {
                    if(secIntState & 0x1)
                    {
                        (void)g_strSocpStat.sDecSrcChan[i].rd_cb(u32ChanId);
                    }
                }

                socp_decsrc_event_handler(i, secIntState);
            }
        }
    }
#if 0
    else
    {
        for(i=0;i<SOCP_MAX_DECSRC_CHN;i++)
        {
            /* ���ͨ���Ƿ�����*/
            if(SOCP_CHN_SET == g_strSocpStat.sDecSrcChan[i].u32SetStat)
            {
                RdSize = socp_get_dec_rd_size(i);
                if ((RdSize > g_strSocpStat.sDecSrcChan[i].u32RdThreshold) && g_strSocpStat.sDecSrcChan[i].rd_cb)
                {
                    u32ChanId = SOCP_CHAN_DEF(SOCP_DECODER_SRC_CHAN, i);
                    (void)g_strSocpStat.sDecSrcChan[i].rd_cb(u32ChanId);
                }
            }
        }
    }
#endif
}

/*****************************************************************************
* �� �� ��  : socp_decsrc_task
*
* ��������  : ģ��������:����Դ��A��
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
int socp_decsrc_task(void * data)
{
    unsigned long lock_flag;
    /* coverity[no_escape] */
    do{
        /* ��ʱ���߱��жϣ����������� */
        if(0 != down_interruptible(&g_strSocpStat.u32DecSrcSemID))
        {
            continue;
        }
        spin_lock_irqsave(&lock, lock_flag);
        /* ��������Դ�ж�*/
        socp_decsrc_handler();
        spin_unlock_irqrestore(&lock, lock_flag);
    }while(1);

    //return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_decdst_task
*
* ��������  : ģ��������:����Ŀ�ģ�˫��
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
int socp_decdst_task(void * data)
{
    u32 i;
    u32 IntTfrState = 0;
    u32 IntOvfState = 0;
    u32 u32ChanId;
    unsigned long lock_flag;
    /* coverity[no_escape] */
    do{
        /* ��ʱ���߱��жϣ����������� */
        if(0 != down_interruptible(&g_strSocpStat.u32DecDstSemID))
        {
            continue;
        }

        spin_lock_irqsave(&lock, lock_flag);
        IntTfrState = g_strSocpStat.u32IntDecDstTfr;
        g_strSocpStat.u32IntDecDstTfr = 0;
        IntOvfState = g_strSocpStat.u32IntDecDstOvf;
        g_strSocpStat.u32IntDecDstOvf = 0;
        spin_unlock_irqrestore(&lock, lock_flag);

        /* �������봫������ж�*/
        if (IntTfrState)
        {
            for (i = 0; i < SOCP_MAX_DECDST_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_ALLOCATED == g_strSocpStat.sDecDstChan[i].u32AllocStat)
                {
                    if (IntTfrState & ((u32)1 << i))
                    {
                        if (g_strSocpStat.sDecDstChan[i].read_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpDecDstTskTrfCbOriCnt[i]++;

                            u32ChanId = SOCP_CHAN_DEF(SOCP_DECODER_DEST_CHAN, i);
                            (void)g_strSocpStat.sDecDstChan[i].read_cb(u32ChanId);

                            g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpDecDstTskTrfCbCnt[i]++;
                        }
                    }
                }
            }
        }

        /* ��������Ŀ�� buffer ����ж�*/
        if (IntOvfState)
        {
            for (i = 0; i < SOCP_MAX_DECDST_CHN; i++)
            {
                /* ���ͨ���Ƿ�����*/
                if (SOCP_CHN_ALLOCATED == g_strSocpStat.sDecDstChan[i].u32AllocStat)
                {
                    if (IntOvfState & ((u32)1 << i))
                    {
                        if (g_strSocpStat.sDecDstChan[i].event_cb)
                        {
                            g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpDecDstTskOvfCbOriCnt[i]++;

                            u32ChanId = SOCP_CHAN_DEF(SOCP_DECODER_DEST_CHAN, i);
                            printk("\r\nsocp_decdst_task: Call Event Cb, Channel is %d",u32ChanId);
                            (void)g_strSocpStat.sDecDstChan[i].event_cb(u32ChanId, SOCP_EVENT_OUTBUFFER_OVERFLOW, 0);

                            g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpDecDstTskOvfCbCnt[i]++;
                        }
                    }
                }
            }
        }
    } while (1);

    //return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_create_task
*
* ��������  : socp���񴴽�����
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : �����ɹ����ı�ʶ��
*****************************************************************************/
s32 socp_create_task( s8 * puchName,
                        unsigned long * pu32TaskID,
                        socp_task_entry pfnFunc,
                        u32 u32Priority,
                        u32 u32StackSize,
                        void * pParam)
{
    struct task_struct  *tsk;
    struct sched_param  param;

    tsk = kthread_run(pfnFunc, pParam, puchName);
    if (IS_ERR(tsk))
    {
        printk("socp_create_task: create kthread failed!\n");
        return BSP_ERROR;
    }

    param.sched_priority = u32Priority;
    if (BSP_OK != sched_setscheduler(tsk, SCHED_FIFO, &param))
    {
        printk("\nsocp_create_task: Creat Task %s ID %p sched_setscheduler Error", puchName, pu32TaskID);
        return BSP_ERROR;
    }

    *pu32TaskID = (unsigned long)tsk;

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : socp_init_task
*
* ��������  : �������������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : �����ɹ����ı�ʶ��
*****************************************************************************/
s32 socp_init_task(void)
{
    u32 aulArguments[4] = {0,0,0,0};

    /* ����Դͨ������*/
    sema_init(&g_strSocpStat.u32EncSrcSemID, 0);
    if(!g_strSocpStat.u32EncSrcTskID)
    {
        if ( BSP_OK != socp_create_task( "EncSrc", (unsigned long *)&g_strSocpStat.u32EncSrcTskID, (socp_task_entry)socp_encsrc_task,
                            SOCP_ENCSRC_TASK_PRO, 0x1000, aulArguments) )
        {
            printk("socp_init_task: create socp_encsrc_task failed.\n");
            return BSP_ERR_SOCP_TSK_CREATE;
        }
    }

    /* �������ͨ������*/
    sema_init(&g_strSocpStat.u32EncDstSemID, 0);
    if(!g_strSocpStat.u32EncDstTskID)
    {
        if ( BSP_OK != socp_create_task( "EncDst", (unsigned long *)&g_strSocpStat.u32EncDstTskID, (socp_task_entry)socp_encdst_task,
                            SOCP_ENCDST_TASK_PRO, 0x1000, aulArguments) )
        {
            printk("socp_init_task: create socp_encdst_task failed.\n");
            return BSP_ERR_SOCP_TSK_CREATE;
        }
    }

    /* ����Դͨ������*/
    sema_init(&g_strSocpStat.u32DecSrcSemID, 0);
    if(!g_strSocpStat.u32DecSrcTskID)
    {
        if ( BSP_OK != socp_create_task( "DecSrc", (unsigned long *)&g_strSocpStat.u32DecSrcTskID, (socp_task_entry)socp_decsrc_task,
                            SOCP_DECSRC_TASK_PRO, 0x1000, aulArguments) )
        {
            printk("socp_init_task: create u32DecSrcTskID failed.\n");
            return BSP_ERR_SOCP_TSK_CREATE;
        }
    }

    /* ����Ŀ��ͨ������*/
    sema_init(&g_strSocpStat.u32DecDstSemID, 0);
    if(!g_strSocpStat.u32DecDstTskID)
    {
        if ( BSP_OK != socp_create_task( "DecDst", (unsigned long *)&g_strSocpStat.u32DecDstTskID, (socp_task_entry)socp_decdst_task,
                            SOCP_DECDST_TASK_PRO, 0x1000, aulArguments) )
        {
            printk("socp_init_task: create u32DecDstTskID failed.\n");
            return BSP_ERR_SOCP_TSK_CREATE;
        }
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_handler_encsrc
*
* ��������  : ����Դͨ������������RD�������ϲ���ɣ�����RD�жϿ��Բ�������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void  socp_handler_encsrc(void)
{
    u32 IntFlag   = 0;
    u32 IntState  = 0;
    int bHandle  = BSP_FALSE;
    u32 i = 0;

    /*read and clear the interrupt flags*/
    SOCP_REG_READ(SOCP_REG_GBL_INTSTAT, IntFlag);
    /* ������ͷ���� */
    if (IntFlag & SOCP_APP_ENC_FLAGINT_MASK)
    {
        SOCP_REG_READ(SOCP_REG_APP_INTSTAT1, IntState);
        SOCP_REG_WRITE(SOCP_REG_ENC_RAWINT1, IntState);

        g_strSocpStat.u32IntEncSrcHeader |= IntState;
        bHandle = BSP_TRUE;

        for (i = 0; i < SOCP_MAX_ENCSRC_CHN; i++)
        {
            if (IntState & ((u32)1 << i))
            {
                /* debugģʽ���ΰ�ͷ�����ж� */
        		if(SOCP_REG_GETBITS(SOCP_REG_ENCSRC_BUFCFG1(i), 31, 1))
        		{
        			SOCP_REG_SETBITS(SOCP_REG_APP_MASK1, i, 1,1);
        		}
                g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpEncSrcIsrHeadIntCnt[i]++;
            }
        }
    }

    /*���ٴ���RD����жϣ���ʼ��ʱ�������� */

    if(bHandle)
    {
        up(&g_strSocpStat.u32EncSrcSemID);
    }

    return ;
}


/*****************************************************************************
* �� �� ��   : socp_handler_encdst
*
* ��������  : ����Ŀ���жϴ�������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
/*lint -save -e550*/
void socp_handler_encdst(void)
{
    u32  IntFlag = 0;
    u32  IntState = 0;
    int  bHandle = BSP_FALSE;
    u32  i;
    u32  mask;
    u32  mask2;
    unsigned long lock_flag;

    /*����Ŀ�Ĵ����ж�*/
    SOCP_REG_READ(SOCP_REG_GBL_INTSTAT, IntFlag);
    if (IntFlag & SOCP_APP_ENC_TFRINT_MASK)
    {
        spin_lock_irqsave(&lock, lock_flag);
        SOCP_REG_READ(SOCP_REG_ENC_INTSTAT0, IntState);
        SOCP_REG_READ(SOCP_REG_ENC_MASK0, mask);
        SOCP_REG_WRITE(SOCP_REG_ENC_MASK0, (IntState | mask));   // mask int 2011.7.27 by yangzhi
        /* ��������ж� */
        SOCP_REG_READ(SOCP_REG_ENC_MASK2, mask2);
        SOCP_REG_WRITE(SOCP_REG_ENC_MASK2, ((IntState << 16) | mask2));
        SOCP_REG_WRITE(SOCP_REG_ENC_RAWINT0, IntState);

        g_strSocpStat.u32IntEncDstTfr |= IntState;
        spin_unlock_irqrestore(&lock, lock_flag);
        bHandle = BSP_TRUE;

        for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
        {
            if (IntState & ((u32)1 << i))
            {
                g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstIsrTrfIntCnt[i]++;
            }
        }
    }
    // �����ж�����ֵ�жϹ���һ���Ĵ���
    else if (IntFlag & SOCP_APP_ENC_OUTOVFINT_MASK)
    {
        SOCP_REG_READ(SOCP_REG_ENC_INTSTAT2, IntState);
        // ����Ŀ��buffer��ֵ�жϴ���
        if(0 != (IntState & SOCP_ENC_DST_BUFF_THRESHOLD_OVF_MASK))
        {
            spin_lock_irqsave(&lock, lock_flag);
            SOCP_REG_READ(SOCP_REG_ENC_MASK2, mask);
            SOCP_REG_WRITE(SOCP_REG_ENC_MASK2, (IntState | mask));
            SOCP_REG_WRITE(SOCP_REG_ENC_RAWINT2, IntState);
            g_strSocpStat.u32IntEncDstThresholdOvf |= IntState;
            spin_unlock_irqrestore(&lock, lock_flag);

            bHandle = BSP_TRUE;

            for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
            {
                if (IntState & ((u32)1 << (i + SOCP_ENC_DST_BUFF_THRESHOLD_OVF_BEGIN)))
                {
                    g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstIsrThresholdOvfIntCnt[i]++;
                }
            }

        }
        // ����Ŀ��buffer�����ж�
        if (0 != (IntState & SOCP_ENC_DST_BUFF_OVF_MASK))
        {
            spin_lock_irqsave(&lock, lock_flag);
            SOCP_REG_READ(SOCP_REG_ENC_MASK2, mask);
            SOCP_REG_WRITE(SOCP_REG_ENC_MASK2, (IntState | mask));
            SOCP_REG_WRITE(SOCP_REG_ENC_RAWINT2, IntState);
            g_strSocpStat.u32IntEncDstOvf |= IntState;
            spin_unlock_irqrestore(&lock, lock_flag);

            bHandle = BSP_TRUE;

            for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
            {
                if (IntState & ((u32)1 << i))
                {
                    g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpEncDstIsrOvfIntCnt[i]++;
                }
            }
        }
    }
    else
    {
        bHandle = BSP_FALSE;
    }

    if(bHandle)
    {
        up(&g_strSocpStat.u32EncDstSemID);
    }

    return ;
}
/*lint -restore +e550*/

/*****************************************************************************
* �� �� ��   : socp_handler_decsrc
*
* ��������  : ����Դͨ���жϴ�������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_handler_decsrc(void)
{
    u32 IntFlag  = 0;
    u32 IntState = 0;
    int bHandle = BSP_FALSE;
    u32 i = 0;

    /*�����������*/
    SOCP_REG_READ(SOCP_REG_GBL_INTSTAT, IntFlag);
    if (IntFlag & SOCP_DEC_INERRINT_MASK)
    {
        SOCP_REG_READ(SOCP_REG_DEC_INTSTAT1, IntState);
        SOCP_REG_WRITE(SOCP_REG_DEC_RAWINT1, IntState);

        g_strSocpStat.u32IntDecSrcErr |= IntState;
        bHandle = BSP_TRUE;

        for (i = 0; i < SOCP_MAX_DECSRC_CHN; i++)
        {
            if (IntState & 0x1)
            {
                g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpDecSrcIsrErrIntCnt[i]++;
            }
        }
    }

    if(bHandle)
    {
        up(&g_strSocpStat.u32DecSrcSemID);
    }

    return;
}


/*****************************************************************************
* �� �� ��   : socp_handler_decdst
*
* ��������  : ����Ŀ��ͨ���жϴ�������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
/*lint -save -e774*/
/*lint -save -e550*/
void socp_handler_decdst(void)
{
    u32 IntFlag  = 0;
    u32 IntState = 0;
    int bHandle = BSP_FALSE;
    u32 TfMask  = 0;
    u32 TfState = 0;
    u32 OvMask    = 0;
    u32 OvState   = 0;
    u32 TfMaskReg = 0;
    u32 i = 0;

    TfMask    = SOCP_CORE0_DEC_TFRINT_MASK;
    TfState   = SOCP_REG_DEC_CORE0ISTAT0;
    TfMaskReg = SOCP_REG_DEC_CORE0MASK0;
    OvMask  = SOCP_CORE0_DEC_OUTOVFINT_MASK;
    OvState = SOCP_REG_DEC_CORE0ISTAT2;

    /*���봫���ж�*/
    SOCP_REG_READ(SOCP_REG_GBL_INTSTAT, IntFlag);
    if (IntFlag & TfMask)
    {
        u32 mask;

        SOCP_REG_READ(TfState, IntState);
        IntState = IntState & 0xffff;
        SOCP_REG_READ(TfMaskReg, mask);
        SOCP_REG_WRITE(TfMaskReg, (IntState | mask));  //added by yangzhi 2011.7.27
        SOCP_REG_WRITE(SOCP_REG_DEC_RAWINT0, IntState);

        g_strSocpStat.u32IntDecDstTfr |= IntState;
        bHandle = BSP_TRUE;

        for (i = 0; i < SOCP_MAX_DECDST_CHN; i++)
        {
            if (IntState & ((u32)1 << i))
            {
                g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpDecDstIsrTrfIntCnt[i]++;
            }
        }
    }

    /*����Ŀ��buffer ����*/
    SOCP_REG_READ(SOCP_REG_GBL_INTSTAT, IntFlag);
    if (IntFlag & OvMask)
    {
        SOCP_REG_READ(OvState, IntState);
        IntState = IntState & 0xffff;
        SOCP_REG_WRITE(SOCP_REG_DEC_RAWINT2, IntState);

        g_strSocpStat.u32IntDecDstOvf |= IntState;
        bHandle = BSP_TRUE;

        for (i = 0; i < SOCP_MAX_DECDST_CHN; i++)
        {
            if (IntState & ((u32)1 << i))
            {
                g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpDecDstIsrOvfIntCnt[i]++;
            }
        }
    }

    if(bHandle)
    {
        up(&g_strSocpStat.u32DecDstSemID);
    }

    return;
}
/*lint -restore +e550*/
/*lint -restore +e774*/

/*****************************************************************************
* �� �� ��   : socp_app_int_handler
*
* ��������  : APP ���жϴ�������
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_app_int_handler(void)
{
    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpAppEtrIntCnt++;

    socp_handler_encsrc();

    socp_handler_encdst();
    socp_handler_decsrc();
    socp_handler_decdst();

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpAppSucIntCnt++;

    return 1;
}

/*****************************************************************************
* �� �� ��  : socp_init
*
* ��������  : ģ���ʼ������
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : ��ʼ���ɹ��ı�ʶ��
*****************************************************************************/
/*lint -save -e529*/
s32 socp_init(void)
{
    s32 ret;
    u32 i;
    //u32 u32ResetValue = 0;
#ifndef BSP_CONFIG_HI3630
	struct clk *cSocp;
#endif
    /* log2.0 2014-03-19 Begin:*/
    DRV_NV_SOCP_LOG_CFG_STRU nvCfg = {0};
    SOCP_ENC_DST_BUF_LOG_CFG_STRU cfg = {0};
	/* log2.0 2014-03-19 End*/
	
    if(BSP_TRUE == g_strSocpStat.bInitFlag)
    {
        return BSP_OK;
    }

    spin_lock_init(&lock);

    /* log2.0 2014-03-19 Begin:*/
    ret = bsp_nvm_read(NV_ID_DRV_SOCP_LOG_CFG, (u8*)&nvCfg, sizeof(DRV_NV_SOCP_LOG_CFG_STRU));
    if(BSP_OK == ret)
    {
        cfg.guWaterMark = nvCfg.ulGuSocpLevel;
        cfg.lWaterMark  = nvCfg.ulLSocpLevel;
        cfg.overTime    = nvCfg.ulTimeOutValue;
        cfg.flushFlag   = FALSE; 
        cfg.logOnFlag   = nvCfg.ulSocpDelayWriteFlg;
    }
    else
    {
        cfg.flushFlag   = FALSE;
        cfg.logOnFlag   = FALSE;    /* ��NVʧ�ܣ�������LOG2.0���� */
    }

    bsp_socp_set_log_cfg(&cfg);   
    /* log2.0 2014-03-19 End*/
    
    //g_strSocpStat.baseAddr = (u32)ioremap (SOCP_REG_BASE, SOCP_REGISTER_SIZE);
    g_strSocpStat.baseAddr = (unsigned long)HI_SOCP_REGBASE_ADDR_VIRT;
    if(0 == g_strSocpStat.baseAddr)
    {
        printk("socp_init: Socp Reg Base Addr ioremap Error.\n"); /* [false alarm]:����Fortify���� */
        return BSP_ERROR; /* [false alarm]:����Fortify���� */
    }

    /* bsp_memmap.h���滹û�ж�BBP�Ĵ����ռ���ӳ�� */
    g_strSocpStat.armBaseAddr = (unsigned long)BBP_REG_ARM_BASEADDR;
    memset(&g_stSocpDebugInfo, 0x0 ,sizeof(SOCP_DEBUG_INFO_S));

#ifndef BSP_CONFIG_HI3630
    /* ��SOCPʱ�� */
	cSocp = clk_get(NULL, "socp_clk");
	if(BSP_OK !=clk_enable(cSocp))
	{
		printk("socp_init: Socp clk open failed.\n");
		return BSP_ERROR;
	}
#endif

    /* log2.0 2014-03-19 Begin*/
    if(TRUE == cfg.logOnFlag)
    {
        SOCP_REG_WRITE(SOCP_REG_INTTIMEOUT, 0);
    }
    /* log2.0 2014-03-19 End*/
    
    /* ��ʼ��ȫ��״̬�ṹ�� */
    /* ����ID��ʼ�� */
    g_strSocpStat.u32EncSrcTskID     = 0;
    g_strSocpStat.u32DecDstTskID     = 0;
    g_strSocpStat.u32EncDstTskID     = 0;
    g_strSocpStat.u32DecSrcTskID     = 0;
    /* ��ͷ�����־��ʼ�� */
    g_strSocpStat.u32IntEncSrcHeader = 0;
    g_strSocpStat.u32IntEncSrcRD     = 0;
    g_strSocpStat.u32IntDecDstTfr    = 0;
    g_strSocpStat.u32IntDecDstOvf    = 0;
    g_strSocpStat.u32IntEncDstTfr    = 0;
    g_strSocpStat.u32IntEncDstOvf    = 0;
    g_strSocpStat.u32IntDecSrcErr    = 0;

    for(i=0; i<SOCP_MAX_ENCSRC_CHN; i++)
    {
        g_strSocpStat.sEncSrcChan[i].u32ChanID      = i;
        g_strSocpStat.sEncSrcChan[i].u32ChanEn      = SOCP_CHN_DISABLE;
        g_strSocpStat.sEncSrcChan[i].u32AllocStat   = SOCP_CHN_UNALLOCATED;
        g_strSocpStat.sEncSrcChan[i].u32LastRdSize  = 0;
        g_strSocpStat.sEncSrcChan[i].u32DestChanID  = 0xff;
        g_strSocpStat.sEncSrcChan[i].u32BypassEn    = 0;
        g_strSocpStat.sEncSrcChan[i].ePriority      = SOCP_CHAN_PRIORITY_3;
        g_strSocpStat.sEncSrcChan[i].eDataType      = SOCP_DATA_TYPE_BUTT;
        g_strSocpStat.sEncSrcChan[i].eDataTypeEn    = SOCP_DATA_TYPE_EN_BUTT;
        g_strSocpStat.sEncSrcChan[i].eDebugEn       = SOCP_ENC_DEBUG_EN_BUTT;
        g_strSocpStat.sEncSrcChan[i].event_cb       = (socp_event_cb)BSP_NULL;
        g_strSocpStat.sEncSrcChan[i].rd_cb          = (socp_rd_cb)BSP_NULL;
    }

    for(i=0; i<SOCP_MAX_ENCDST_CHN; i++)
    {
        g_strSocpStat.sEncDstChan[i].u32ChanID      = i;
        g_strSocpStat.sEncDstChan[i].u32Thrh        = 0;
        g_strSocpStat.sEncDstChan[i].u32SetStat     = SOCP_CHN_UNSET;
        g_strSocpStat.sEncDstChan[i].event_cb       = (socp_event_cb)BSP_NULL;
        g_strSocpStat.sEncDstChan[i].read_cb        = (socp_rd_cb)BSP_NULL;
        g_strSocpStat.sEncDstChan[i].eChnEvent      = (SOCP_EVENT_ENUM_UIN32)0;
    }

    for(i=0; i<SOCP_MAX_DECSRC_CHN; i++)
    {
        g_strSocpStat.sDecSrcChan[i].u32ChanID      = i;
        g_strSocpStat.sDecSrcChan[i].u32ChanEn      = SOCP_CHN_DISABLE;
        g_strSocpStat.sDecSrcChan[i].eDataTypeEn    = SOCP_DATA_TYPE_EN_BUTT;
        g_strSocpStat.sDecSrcChan[i].u32SetStat     = SOCP_CHN_UNSET;
        g_strSocpStat.sDecSrcChan[i].event_cb       = (socp_event_cb)BSP_NULL;
        g_strSocpStat.sDecSrcChan[i].rd_cb          = (socp_rd_cb)BSP_NULL;
    }

    for(i=0; i<SOCP_MAX_DECDST_CHN; i++)
    {
        g_strSocpStat.sDecDstChan[i].u32ChanID      = i;
        g_strSocpStat.sDecDstChan[i].u32AllocStat   = SOCP_CHN_UNALLOCATED;
        g_strSocpStat.sDecDstChan[i].eDataType      = SOCP_DATA_TYPE_BUTT;
        g_strSocpStat.sDecDstChan[i].event_cb       = (socp_event_cb)BSP_NULL;
        g_strSocpStat.sDecDstChan[i].read_cb        = (socp_rd_cb)BSP_NULL;
    }

#if 0
    /* ��ȫ�ּĴ������и�λ */
    SOCP_REG_WRITE(SOCP_REG_GBLRST, 1);

    /* �ȴ�ͨ����λ״̬���� */
    for(i=0; i<SOCP_RESET_TIME; i++)
    {
        SOCP_REG_READ(SOCP_REG_GBLRST, u32ResetValue);
        if(0 == u32ResetValue)
        {
            break;
        }
        if((SOCP_RESET_TIME -1) == i)
        {
            printk("socp_init: reset global regs failed!\n");
        }
    }

    /* ����ram�мĴ����ĸ�λ����*/
    for(i=0; i<SOCP_TOTAL_ENCSRC_CHN; i++)
    {
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFADDR(i),0);
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFCFG0(i),0);
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQWPTR(i),0);
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQRPTR(i),0);
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQADDR(i),0);
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQCFG(i),0);
    }
    for(i=0; i<SOCP_MAX_ENCDST_CHN; i++)
    {
        SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFADDR(i),0);
    }
#endif

//#ifdef BSP_CONFIG_HI3630
#if 0
    /* ���ô����жϳ�ʱʱ���buffer�����ʱʱ�� */
    SOCP_REG_WRITE(SOCP_REG_INTTIMEOUT, SOCP_TRANS_TIMEOUT_DEFAULT);
    SOCP_REG_SETBITS(SOCP_REG_BUFTIMEOUT, 31, 1, 0);
    SOCP_REG_SETBITS(SOCP_REG_BUFTIMEOUT, 0, 16, SOCP_OVERFLOW_TIMEOUT_DEFAULT);
    /* ���ý���ͨ·���������üĴ���*/
    SOCP_REG_SETBITS(SOCP_REG_DEC_PKTLEN, 0, 12,  SOCP_DEC_PKTLGTH_MAX);
    SOCP_REG_SETBITS(SOCP_REG_DEC_PKTLEN, 12, 5,  SOCP_DEC_PKTLGTH_MIN);
    /* ʱ���ſأ�����ʱʱ���Զ��ر� */
    writel(0xffffffff, (volatile void *)(SOCP_REG_BASEADDR + 0x14));

    /* �����ж� */
    SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, 0, 7, 0x7f);
    SOCP_REG_WRITE(SOCP_REG_APP_MASK1, 0xffffffff);
    SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 0, 7, 0x7f);
    SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 16, 7, 0x7f);
    SOCP_REG_WRITE(SOCP_REG_APP_MASK3, 0xffffffff);
    SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, 0, 16, 0xffff);
    SOCP_REG_SETBITS(SOCP_REG_DEC_MASK1, 0, 24, 0xffffff);
    SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK2, 0, 16, 0xffff);
#endif

#if 0
//#ifdef CONFIG_MODULE_VIC
    /* FPGA�汾��Ҫʹ�ö����жϣ�ʹ��VIC�ӿڹ��жϣ��жϺ�: 19,20 */
    ret = bsp_vic_connect(INT_LVL_SOCP, socp_app_int_handler, 0);
    printk("socp vic connect\n");
    if (BSP_OK != ret)
    {
        printk("socp_init: connect app core int failed.\n");
        return BSP_ERROR;
    }
    /* ʹ���ж� */
    ret = bsp_vic_enable(INT_LVL_SOCP);
    if (BSP_OK != ret)
    {
        printk("socp_init: enable app core int failed.\n");
        return BSP_ERROR;
    }

#else
    /* ���ж� */
    ret = request_irq(INT_LVL_SOCP, (irq_handler_t)socp_app_int_handler, 0, "SOCP_APP_IRQ",  BSP_NULL);
    if (BSP_OK != ret)
    {
        printk("socp_init: connect app core int failed.\n");
        return BSP_ERROR;
    }
#endif
    /* ������������� */
    ret = socp_init_task();
    if (BSP_OK != ret)
    {
        printk("socp_init: create task failed(0x%x).\n", ret);
        return (s32)ret;
    }

    /* ���ó�ʼ��״̬ */
    g_strSocpStat.bInitFlag = BSP_TRUE;

    printk("socp_init: succeed\n");

    return BSP_OK;
}

/*lint -restore +e529*/

/*****************************************************************************
* �� �� ��  : bsp_socp_coder_set_src_chan
*
* ��������  : ����Դͨ�����ú���
*
* �������  : pSrcAttr     ����Դͨ�����ò���
*             ulSrcChanID  ����Դͨ��ID
*
* �������  : ��
*
* �� �� ֵ  : ���뼰���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_coder_set_src_chan(SOCP_CODER_SRC_ENUM_U32 enSrcChanID, SOCP_CODER_SRC_CHAN_STRU *pSrcAttr)
{
    u32 start;
    u32 end;
    u32 rdstart = 0;
    u32 rdend = 0;
    u32 buflength   = 0;
    u32 Rdbuflength = 0;
    u32 i;
    u32 srcChanId;
    u32 u32SrcChanType;
    u32 u32DstChanID;
    u32 u32DstChanType;
    u32 ResetFlag;
    SOCP_ENCSRC_CHAN_S *pChan;

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpAllocEncSrcCnt++;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pSrcAttr);
    SOCP_CHECK_CHAN_PRIORITY(pSrcAttr->ePriority);
    SOCP_CHECK_DATA_TYPE(pSrcAttr->eDataType);
    SOCP_CHECK_DATA_TYPE_EN(pSrcAttr->eDataTypeEn);
    SOCP_CHECK_ENC_DEBUG_EN(pSrcAttr->eDebugEn);

    srcChanId       = SOCP_REAL_CHAN_ID(enSrcChanID);
    u32SrcChanType  = SOCP_REAL_CHAN_TYPE(enSrcChanID);

    SOCP_CHECK_CHAN_TYPE(u32SrcChanType, SOCP_CODER_SRC_CHAN);
    SOCP_CHECK_ENCSRC_CHAN_ID(srcChanId);

    u32DstChanID   = SOCP_REAL_CHAN_ID(pSrcAttr->u32DestChanID);
    u32DstChanType = SOCP_REAL_CHAN_TYPE(pSrcAttr->u32DestChanID);
    SOCP_CHECK_CHAN_TYPE(u32DstChanType, SOCP_CODER_DEST_CHAN);
    SOCP_CHECK_CHAN_ID(u32DstChanID, SOCP_MAX_ENCDST_CHN);

    if ((SOCP_ENCSRC_CHNMODE_CTSPACKET != pSrcAttr->eMode)
        && (SOCP_ENCSRC_CHNMODE_LIST != pSrcAttr->eMode))
    {
        printk("bsp_socp_coder_set_src_chan: chnnel mode is invalid %d!\n", pSrcAttr->eMode);
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    /* ʹ�����ò����������� */
    /* �ж���ʼ��ַ�Ƿ�8�ֽڶ��� */
    start   = pSrcAttr->sCoderSetSrcBuf.u32InputStart;
    end     = pSrcAttr->sCoderSetSrcBuf.u32InputEnd;

    SOCP_CHECK_PARA(start);
    SOCP_CHECK_8BYTESALIGN(start);
    SOCP_CHECK_PARA(end);
    SOCP_CHECK_BUF_ADDR(start, end);
    buflength = end - start + 1;
    SOCP_CHECK_8BYTESALIGN(buflength);
    if(buflength > SOCP_ENC_SRC_BUFLGTH_MAX)
    {
        printk("bsp_socp_coder_set_src_chan: buffer length is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    /* �������������������������RDbuffer����ʼ��ַ�ͳ��� */
    if(SOCP_ENCSRC_CHNMODE_LIST == pSrcAttr->eMode)
    {
        /* �ж�RDBuffer����ʼ��ַ�Ƿ�8�ֽڶ��� */
        rdstart = pSrcAttr->sCoderSetSrcBuf.u32RDStart;
        rdend   = pSrcAttr->sCoderSetSrcBuf.u32RDEnd;

        SOCP_CHECK_PARA(rdstart);
        SOCP_CHECK_8BYTESALIGN(rdstart);
        SOCP_CHECK_PARA(rdend);
        SOCP_CHECK_BUF_ADDR(rdstart, rdend);
        /* RD��ֵû��ʹ�� */
        //SOCP_CHECK_PARA(pSrcAttr->sCoderSetSrcBuf.u32RDThreshold);
        Rdbuflength = rdend - rdstart + 1;
        SOCP_CHECK_8BYTESALIGN(Rdbuflength);
        if(Rdbuflength > SOCP_ENC_SRC_RDLGTH_MAX)
        {
            printk("bsp_socp_coder_set_src_chan: RD buffer length is too large!\n");
            return BSP_ERR_SOCP_INVALID_PARA;
        }
    }

    /* ��λͨ�� */
    SOCP_REG_SETBITS(SOCP_REG_ENCRST, srcChanId, 1, 1);

    /* �ȴ�ͨ������ */
    for (i = 0; i < SOCP_RESET_TIME; i++)
    {
        ResetFlag = SOCP_REG_GETBITS(SOCP_REG_ENCRST, srcChanId, 1);
        if(0 == ResetFlag)
        {
            break;
        }

        if ((SOCP_RESET_TIME - 1) == i)
        {
            printk("bsp_socp_coder_set_src_chan: reset channel 0x%x failed!\n", srcChanId);
        }
    }

    /* д����ʼ��ַ��Դbuffer��ʼ��ַ�Ĵ���*/
    SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFADDR(srcChanId),SOCP_VIRT_PHY(start));
    SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFWPTR(srcChanId),SOCP_VIRT_PHY(start));
    SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFRPTR(srcChanId), SOCP_VIRT_PHY(start));
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG0(srcChanId), 0, 27, buflength);
	SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG0(srcChanId), 27, 5, 0);

    /* �������������������������RDbuffer����ʼ��ַ�ͳ��� */
    if(SOCP_ENCSRC_CHNMODE_LIST == pSrcAttr->eMode)
    {
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQADDR(srcChanId),SOCP_VIRT_PHY(rdstart));
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQRPTR(srcChanId),SOCP_VIRT_PHY(rdstart));
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQWPTR(srcChanId), SOCP_VIRT_PHY(rdstart));
        SOCP_REG_SETBITS(SOCP_REG_ENCSRC_RDQCFG(srcChanId), 0, 16, Rdbuflength);
		SOCP_REG_SETBITS(SOCP_REG_ENCSRC_RDQCFG(srcChanId), 16, 16, 0);
    }

    /*������������*/
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 1, 2, pSrcAttr->eMode);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 4, 4, pSrcAttr->u32DestChanID);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 8, 2, pSrcAttr->ePriority);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 10, 1, pSrcAttr->u32BypassEn);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 11, 1, pSrcAttr->eDataTypeEn);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 31, 1, pSrcAttr->eDebugEn);
    SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(srcChanId), 16, 8, pSrcAttr->eDataType);

    /* ��g_strSocpStat�б������*/
    pChan = &g_strSocpStat.sEncSrcChan[srcChanId];
    pChan->eChnMode               = pSrcAttr->eMode;
    pChan->ePriority              = pSrcAttr->ePriority;
    pChan->eDataType              = pSrcAttr->eDataType;
    pChan->eDataTypeEn            = pSrcAttr->eDataTypeEn;
    pChan->eDebugEn               = pSrcAttr->eDebugEn;
    pChan->u32DestChanID          = pSrcAttr->u32DestChanID;
    pChan->u32BypassEn            = pSrcAttr->u32BypassEn;
    pChan->sEncSrcBuf.u32Start    = start;
    pChan->sEncSrcBuf.u32End      = end;
    pChan->sEncSrcBuf.u32Write    = start;
    pChan->sEncSrcBuf.u32Read     = start;
    pChan->sEncSrcBuf.u32Length   = buflength;
    pChan->sEncSrcBuf.u32IdleSize = 0;

    if(SOCP_ENCSRC_CHNMODE_LIST == pSrcAttr->eMode)
    {
        pChan->sRdBuf.u32Start    = rdstart;
        pChan->sRdBuf.u32End      = rdend;
        pChan->sRdBuf.u32Write    = rdstart;
        pChan->sRdBuf.u32Read     = rdstart;
        pChan->sRdBuf.u32Length   = Rdbuflength;
        pChan->u32RdThreshold     = pSrcAttr->sCoderSetSrcBuf.u32RDThreshold;
    }

    /* ���ͨ��״̬ */
    g_strSocpStat.sEncSrcChan[srcChanId].u32AllocStat = SOCP_CHN_ALLOCATED;
    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpAllocEncSrcSucCnt++;
    return BSP_OK;

}

/*****************************************************************************
* �� �� ��  : bsp_socp_decoder_set_dest_chan
*
* ��������  : ����Ŀ��ͨ�����뼰���ú���
*
* �������  : pAttr           ����Ŀ��ͨ�����ò���
*             pDestChanID     ��ʼ������Ŀ��ͨ��ID������ͨ��Դ��Ŀ��ID�ж�Ӧ��ϵ
                              SrcID = DestChanID%4
*
* �������  :
*
* �� �� ֵ  : ���뼰���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_decoder_set_dest_chan(SOCP_DECODER_DST_ENUM_U32 enDestChanID,
                                                SOCP_DECODER_DEST_CHAN_STRU *pAttr)
{
    u32 start;
    u32 end;
    u32 bufThreshold;
    u32 buflength;
    u32 u32ChanID;
    u32 u32SrcChanID;
    u32 u32ChanType;
    SOCP_DECDST_CHAN_S *pChan;

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpAllocDecDstCnt++;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pAttr);
    SOCP_CHECK_DATA_TYPE(pAttr->eDataType);

    u32ChanID    = SOCP_REAL_CHAN_ID(enDestChanID);
    u32ChanType  = SOCP_REAL_CHAN_TYPE(enDestChanID);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_DEST_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECDST_CHN);

    u32SrcChanID = SOCP_REAL_CHAN_ID(pAttr->u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(pAttr->u32SrcChanID);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);
    SOCP_CHECK_CHAN_ID(u32SrcChanID, SOCP_MAX_DECSRC_CHN);

    /* �ж�ͨ��ID��Ӧ��ϵ */
    if(u32SrcChanID != u32ChanID%4)
    {
        printk("bsp_socp_decoder_set_dest_chan: dest ID(%d) is not matching src ID(%d)!\n", u32ChanID, u32SrcChanID);
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    /* �жϸ����ĵ�ַ�ͳ����Ƿ�Ϊ���ֽڱ���*/
    start           = pAttr->sDecoderDstSetBuf.u32OutputStart;
    end             = pAttr->sDecoderDstSetBuf.u32OutputEnd;
    bufThreshold    = pAttr->sDecoderDstSetBuf.u32Threshold;
    SOCP_CHECK_PARA(start);
    SOCP_CHECK_8BYTESALIGN(start);
    SOCP_CHECK_PARA(end);
    SOCP_CHECK_BUF_ADDR(start, end);
    SOCP_CHECK_PARA(bufThreshold);
    if (bufThreshold > SOCP_DEC_DST_TH_MAX)
    {
        printk("bsp_socp_decoder_set_dest_chan: buffer threshold is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    buflength = end - start + 1;
    SOCP_CHECK_8BYTESALIGN(buflength);
    if (buflength > SOCP_DEC_DST_BUFLGTH_MAX)
    {
        printk("bsp_socp_decoder_set_dest_chan: buffer length is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    /* д����ʼ��ַ��Դbuffer��ʼ��ַ�Ĵ���*/
    SOCP_REG_SETBITS(SOCP_REG_DECDEST_BUFCFG(u32ChanID), 24, 8, pAttr->eDataType);
    SOCP_REG_WRITE(SOCP_REG_DECDEST_BUFRPTR(u32ChanID), SOCP_VIRT_PHY(start));
    SOCP_REG_WRITE(SOCP_REG_DECDEST_BUFADDR(u32ChanID), SOCP_VIRT_PHY(start));
    SOCP_REG_WRITE(SOCP_REG_DECDEST_BUFWPTR(u32ChanID), SOCP_VIRT_PHY(start));
    SOCP_REG_SETBITS(SOCP_REG_DECDEST_BUFCFG(u32ChanID), 0, 16, buflength);
    SOCP_REG_SETBITS(SOCP_REG_DECDEST_BUFCFG(u32ChanID), 16, 8, bufThreshold);

    pChan = &g_strSocpStat.sDecDstChan[u32ChanID];
    pChan->eDataType = pAttr->eDataType;
    pChan->sDecDstBuf.u32Start    = start;
    pChan->sDecDstBuf.u32End      = end;
    pChan->sDecDstBuf.u32Length   = buflength;
    pChan->sDecDstBuf.u32Read     = start;
    pChan->sDecDstBuf.u32Write    = start;

    /* �����жϣ��ٴ��ж�*/
    SOCP_REG_SETBITS(SOCP_REG_DEC_RAWINT0, u32ChanID, 1, 1);
    SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, u32ChanID, 1, 0);
    SOCP_REG_SETBITS(SOCP_REG_DEC_RAWINT2, u32ChanID, 1, 1);
    SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK2, u32ChanID, 1, 0);

    /* ���ͨ������״̬ */
    g_strSocpStat.sDecDstChan[u32ChanID].u32AllocStat = SOCP_CHN_ALLOCATED;
    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpAllocDecDstSucCnt++;

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_coder_set_dest_chan_attr
*
* ��������  : ����Ŀ��ͨ�����ú���
*
* �������  : u32DestChanID      ����Ŀ��ͨ��ID
              pDestAttr          ����Ŀ��ͨ�����ò���
*
* �������  : ��
* �� �� ֵ  : ���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_coder_set_dest_chan_attr(u32 u32DestChanID, SOCP_CODER_DEST_CHAN_STRU *pDestAttr)
{
    u32 start;
    u32 end;
    u32 bufThreshold;
    u32 buflength;
    u32 u32ChanID;
    u32 u32ChanType;
    u32 u32Thrh;
    SOCP_ENCDST_CHAN_S *pChan;

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpSetEncDstCnt++;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pDestAttr);
    u32ChanID   = SOCP_REAL_CHAN_ID(u32DestChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DestChanID);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_CODER_DEST_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_ENCDST_CHN);

    start = pDestAttr->sCoderSetDstBuf.u32OutputStart;
    end   = pDestAttr->sCoderSetDstBuf.u32OutputEnd;
    bufThreshold = pDestAttr->sCoderSetDstBuf.u32Threshold;
    u32Thrh = pDestAttr->u32EncDstThrh;

    SOCP_CHECK_PARA(start);
    SOCP_CHECK_8BYTESALIGN(start);
    SOCP_CHECK_PARA(end);
    SOCP_CHECK_BUF_ADDR(start, end);
    SOCP_CHECK_PARA(bufThreshold);
    buflength = end - start + 1;
    SOCP_CHECK_8BYTESALIGN(buflength);
    if (buflength > SOCP_ENC_DST_BUFLGTH_MAX)
    {
        printk("CoderSetDestChanAttr: buffer length is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    if (bufThreshold > SOCP_ENC_DST_TH_MAX)
    {
        printk("CoderSetDestChanAttr: buffer threshold is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    /* ����������������ٴ�����,ͨ����λ֮��ֻ����һ�� */
    /* ʹ�����ò����������� */
    pChan = &g_strSocpStat.sEncDstChan[u32ChanID];
    if (!pChan->u32SetStat)
    {
        /* д����ʼ��ַ��Ŀ��buffer��ʼ��ַ�Ĵ���*/
        SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFADDR(u32ChanID),SOCP_VIRT_PHY(start));
        SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFRPTR(u32ChanID),SOCP_VIRT_PHY(start));
 	    SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFWPTR(u32ChanID), SOCP_VIRT_PHY(start));
        SOCP_REG_SETBITS(SOCP_REG_ENCDEST_BUFCFG(u32ChanID), 0, 21, buflength);
        SOCP_REG_SETBITS(SOCP_REG_ENCDEST_BUFCFG(u32ChanID), 21, 11, bufThreshold);
        SOCP_REG_SETBITS(SOCP_REG_ENCDEST_BUFTHRH(u32ChanID), 0, 31, u32Thrh);

        /* ��g_strSocpStat�б������*/
        pChan->u32ChanID              = u32ChanID;
        pChan->u32Thrh = u32Thrh;
        pChan->sEncDstBuf.u32Start    = start;
        pChan->sEncDstBuf.u32End      = end;
        pChan->sEncDstBuf.u32Write    = start;
        pChan->sEncDstBuf.u32Read     = start;
        pChan->sEncDstBuf.u32Length   = buflength;

        /* ������ͨ���Ѿ����� */
        pChan->u32SetStat = SOCP_CHN_SET;

        /* �����жϣ��ٴ��ж� */
        SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT0, u32ChanID, 1, 1);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, u32ChanID, 1, 0);
        SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT2, u32ChanID, 1, 1);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, u32ChanID, 1, 0);
        // ����Ŀ��buffer��ֵ�ж�
        SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT2, u32ChanID+16, 1, 1);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, u32ChanID+16, 1, 0);
    }
    else
    {
        printk("CoderSetDestChanAttr: channel 0x%x can't be set twice!\n", u32ChanID);
        return BSP_ERR_SOCP_SET_FAIL;
    }

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpSetEncDstSucCnt++;

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_decoder_set_src_chan_attr
*
* ��������  : ����Դͨ�����ú���
*
* �������  : u32SrcChanID    ����Դͨ��ID
*             pInputAttr      ����Դͨ�����ò���
*
* �������  :
*
* �� �� ֵ  : ���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_decoder_set_src_chan_attr(u32 u32SrcChanID, SOCP_DECODER_SRC_CHAN_STRU *pInputAttr)
{
    u32 start;
    u32 end;
    u32 buflength = 0;
    u32 u32ChanID;
    u32 u32ChanType;
    u32 i;
    u32 u32ResetFlag;
    SOCP_DECSRC_CHAN_S *pDecSrcChan;

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpSetDecSrcCnt++;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pInputAttr);
    u32ChanID   = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECSRC_CHN);
    SOCP_CHECK_DATA_TYPE_EN(pInputAttr->eDataTypeEn);

    start     = pInputAttr->sDecoderSetSrcBuf.u32InputStart;
    end       = pInputAttr->sDecoderSetSrcBuf.u32InputEnd;
    SOCP_CHECK_PARA(start);
    SOCP_CHECK_8BYTESALIGN(start);
    SOCP_CHECK_PARA(end);
    SOCP_CHECK_BUF_ADDR(start, end);
    buflength = end - start + 1;
    SOCP_CHECK_8BYTESALIGN(buflength);
    if (buflength > SOCP_DEC_SRC_BUFLGTH_MAX)
    {
        printk("DecoderSetSrcChanAttr: buffer length is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    pDecSrcChan = &g_strSocpStat.sDecSrcChan[u32ChanID];
    if (SOCP_CHN_SET == pDecSrcChan->u32SetStat)
    {
        printk("DecoderSetSrcChanAttr: channel 0x%x has been configed!\n", u32ChanID);
        return BSP_ERR_SOCP_DECSRC_SET;
    }

    /* ���ȸ�λͨ�� */
    SOCP_REG_SETBITS(SOCP_REG_DECRST, u32ChanID, 1, 1);

    /* �ȴ�ͨ����λ״̬���� */
    for (i = 0; i < SOCP_RESET_TIME; i++)
    {
        u32ResetFlag = SOCP_REG_GETBITS(SOCP_REG_DECRST, u32ChanID, 1);
        if (0 == u32ResetFlag)
        {
            break;
        }

        if ((SOCP_RESET_TIME - 1) == i)
        {
            printk("DecoderSetSrcChanAttr: reset channel 0x%x failed!\n", u32ChanID);
        }
    }

    /* ʹ�����ò����������� */
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFWPTR(u32ChanID),SOCP_VIRT_PHY(start));
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFADDR(u32ChanID),SOCP_VIRT_PHY(start));
    SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFRPTR(u32ChanID), SOCP_VIRT_PHY(start));
    SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32ChanID), 0, 16, buflength);
    SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32ChanID), 31, 1, pInputAttr->eDataTypeEn);
    //SOCP_REG_SETBITS(SOCP_REG_DECSRC_RDQCFG(u32ChanID), 29, 1, 0);

    /* ��ȫ�ֱ����б��氲װ�Ĳ��� */
    pDecSrcChan->u32ChanID              = u32ChanID;
    pDecSrcChan->eDataTypeEn= pInputAttr->eDataTypeEn;
    pDecSrcChan->sDecSrcBuf.u32Start    = start;
    pDecSrcChan->sDecSrcBuf.u32End      = end;
    pDecSrcChan->sDecSrcBuf.u32Length   = buflength;
    pDecSrcChan->sDecSrcBuf.u32Read     = start;
    pDecSrcChan->sDecSrcBuf.u32Write    = start;

    pDecSrcChan->u32SetStat = SOCP_CHN_SET;

    g_stSocpDebugInfo.sSocpDebugGBl.u32SocpSetDeSrcSucCnt++;

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_decoder_get_err_cnt
*
* ��������  : ����ͨ���л�ȡ�����������
*
* �������  : u32ChanID       ����ͨ��ID

* �������  : pErrCnt         ����ͨ����������ṹ��ָ��
*
* �� �� ֵ  : ��ȡ�ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_decoder_get_err_cnt(u32 u32DstChanID, SOCP_DECODER_ERROR_CNT_STRU *pErrCnt)
{
    u32 u32ChanID;
    u32 u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �ж�ͨ��ID�Ƿ���Ч */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32DstChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DstChanID);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECSRC_CHN);
    SOCP_CHECK_DECSRC_SET(u32ChanID);

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pErrCnt);

    /* �ж�ͨ���Ƿ�򿪣�������Ϊdebugģʽ*/
    if (g_strSocpStat.sDecSrcChan[u32ChanID].u32ChanEn)
    {
        pErrCnt->u32PktlengthCnt = SOCP_REG_GETBITS(SOCP_REG_DEC_BUFSTAT0(u32ChanID), 16, 16);
        pErrCnt->u32CrcCnt = SOCP_REG_GETBITS(SOCP_REG_DEC_BUFSTAT0(u32ChanID), 0, 16);
        pErrCnt->u32DataTypeCnt   = SOCP_REG_GETBITS(SOCP_REG_DEC_BUFSTAT1(u32ChanID), 16, 16);
        pErrCnt->u32HdlcHeaderCnt = SOCP_REG_GETBITS(SOCP_REG_DEC_BUFSTAT1(u32ChanID), 0, 16);
    }
    else
    {
        printk("DecoderGetErrCnt: debug mode is closed!\n");
        return BSP_ERR_SOCP_SET_FAIL;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_set_timeout
*
* ��������  : ��ʱ��ֵ���ú���
*
* �������  :   eTmOutEn          ���ö���ѡ��ʹ��
                u32Timeout        ��ʱ��ֵ
*
* �������  :
*
* �� �� ֵ  : ���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_set_timeout (SOCP_TIMEOUT_EN_ENUM_UIN32 eTmOutEn, u32 u32Timeout)
{
#if (FEATURE_SOCP_DECODE_INT_TIMEOUT == FEATURE_ON) 
    DECODE_TIMEOUT_MODULE decode_timeout_module = DECODE_TIMEOUT_INT_TIMEOUT;
#endif
    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ��� */
    if (u32Timeout > SOCP_TIMEOUT_MAX)
    {
        printk("SetTimeout: the value is too large!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    switch (eTmOutEn)
    {
        case SOCP_TIMEOUT_BUFOVF_DISABLE:
        {
            SOCP_REG_SETBITS(SOCP_REG_BUFTIMEOUT, 31, 1, 0);
            break;
        }
        case SOCP_TIMEOUT_BUFOVF_ENABLE:
        {
            SOCP_REG_SETBITS(SOCP_REG_BUFTIMEOUT, 31, 1, 1);
            /* ���ӻ���ķ��� */
            SOCP_REG_SETBITS(SOCP_REG_BUFTIMEOUT, 0, 16, u32Timeout);
            break;
        }
        case SOCP_TIMEOUT_TRF:
        {
#if (FEATURE_SOCP_DECODE_INT_TIMEOUT == FEATURE_ON)
            /* ���䳬ʱʱ�����ò���Ҫ�漰ͨ��ID*/
            /*
                            ��GLOBAL_CTRL[1]=0, bit[7:0]��Ч
                            ��GLOBAL_CTRL[1]=1, bit[31:0]��Ч
                    */
            /*SOCP_REG_WRITE(SOCP_REG_INTTIMEOUT, u32Timeout);*/
            decode_timeout_module = SOCP_REG_GETBITS(SOCP_REG_GBLRST, 1, 1);
            if(decode_timeout_module == DECODE_TIMEOUT_INT_TIMEOUT)
            {
                if (u32Timeout > 0xFF)
                {
                   printk("SetTimeout: the value is too large!\n");
                   return BSP_ERR_SOCP_INVALID_PARA;
                }

                SOCP_REG_WRITE(SOCP_REG_INTTIMEOUT, u32Timeout);
            }
            else
            {
                /*SOCP_REG_SETBITS(SOCP_REG_INTTIMEOUT, 0, 31, u32Timeout);*/
                SOCP_REG_WRITE(SOCP_REG_INTTIMEOUT, u32Timeout);
            }
#else
			SOCP_REG_WRITE(SOCP_REG_INTTIMEOUT, u32Timeout);
#endif
            break;
        }
#if (FEATURE_SOCP_DECODE_INT_TIMEOUT == FEATURE_ON)
        case SOCP_TIMEOUT_DECODE_TRF:
        {
			decode_timeout_module = SOCP_REG_GETBITS(SOCP_REG_GBLRST, 1, 1);
            if(decode_timeout_module == DECODE_TIMEOUT_INT_TIMEOUT)
            {
                return BSP_ERR_SOCP_INVALID_PARA;
            }
            if (u32Timeout > 0xFFFF)
            {
                printk("SetTimeout: the value is too large!\n");
                return BSP_ERR_SOCP_INVALID_PARA;
            }
            SOCP_REG_WRITE(SOCP_REG_DEC_INT_TIMEOUT, u32Timeout);
			break;
        }
#endif
        default:
        {
            printk("SetTimeout: invalid timeout choice type!\n");
            return BSP_ERR_SOCP_SET_FAIL;
        }
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_set_dec_pkt_lgth
*
* ��������  : ��ʱ��ֵ���ú���
*
* �������  :   pPktlgth          ������������ò����ṹ��
*
* �������  :
*
* �� �� ֵ  : ���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_set_dec_pkt_lgth(SOCP_DEC_PKTLGTH_STRU *pPktlgth)
{
    u32 u32PktMaxLgth;
    u32 u32PktMinLgth;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pPktlgth);
    u32PktMaxLgth = pPktlgth->u32PktMax;
    u32PktMinLgth = pPktlgth->u32PktMin;

    SOCP_CHECK_PARA(u32PktMaxLgth);
    if (u32PktMaxLgth*1024 > SOCP_DEC_MAXPKT_MAX)
    {
        printk("SetDecPktLgth: u32PktMaxLgth 0x%x is too large!\n", u32PktMaxLgth);
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    if (u32PktMinLgth > SOCP_DEC_MINPKT_MAX)
    {
        printk("SetDecPktLgth: u32PktMinLgth 0x%x is too large!\n", u32PktMinLgth);
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    /* ���ý���ͨ·���������üĴ���*/
    SOCP_REG_SETBITS(SOCP_REG_DEC_PKTLEN, 0, 12, u32PktMaxLgth);
    SOCP_REG_SETBITS(SOCP_REG_DEC_PKTLEN, 12, 5, u32PktMinLgth);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_set_debug
*
* ��������  : ���ý���Դͨ��debugģʽ����
*
* �������  : u32DecChanID  ����Դͨ��ID
              u32DebugEn    debugģʽʹ�ܱ�ʶ
*
* �������  :
*
* �� �� ֵ  : ���óɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_set_debug(u32 u32DecChanID, u32 u32DebugEn)
{
    u32 u32ChanID;
    u32 u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �ж�ͨ��ID�Ƿ���Ч */
    u32ChanID = SOCP_REAL_CHAN_ID(u32DecChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DecChanID);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);
    SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECSRC_CHN);
    SOCP_CHECK_DECSRC_SET(u32ChanID);

    /* �жϸ�ͨ����ģʽ��û�д򿪵Ļ����������� */
    if(g_strSocpStat.sDecSrcChan[u32ChanID].u32ChanEn)
    {
        printk("SetDebug: decoder channel is open, can't set debug bit\n");
        return BSP_ERR_SOCP_SET_FAIL;
    }
    else
    {
        SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32ChanID), 29, 1, u32DebugEn);
    }

    return BSP_OK;
}


/*****************************************************************************
* �� �� ��  : bsp_socp_free_channel
*
* ��������  : ͨ���ͷź���
*
* �������  : u32ChanID       �����ͨ��ָ��
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_free_channel(u32 u32ChanID)
{
    u32 u32RealChanID;
    u32 u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �ж�ͨ��ID�Ƿ���Ч */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32ChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32ChanID);

    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        SOCP_ENCSRC_CHAN_S *pChan;

        SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32RealChanID);

        pChan = &g_strSocpStat.sEncSrcChan[u32RealChanID];
        if (SOCP_CHN_ENABLE == pChan->u32ChanEn)
        {
            printk("FreeChannel: chan 0x%x is running!\n", u32ChanID);
            return BSP_ERR_SOCP_CHAN_RUNNING;
        }

        (void)socp_reset_enc_chan(u32RealChanID);

        pChan->u32AllocStat = SOCP_CHN_UNALLOCATED;

        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpFreeEncSrcCnt[u32RealChanID]++;
    }
    else if (SOCP_DECODER_DEST_CHAN == u32ChanType)
    {
        SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECDST_CHN);
        SOCP_CHECK_DECDST_ALLOC(u32RealChanID);

        /* ��������������Ч */
        SOCP_REG_SETBITS(SOCP_REG_DECDEST_BUFCFG(u32RealChanID), 24, 8, 0xff);

        g_strSocpStat.sDecDstChan[u32RealChanID].u32AllocStat = SOCP_CHN_UNALLOCATED;

        g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpFreeDecDstCnt[u32RealChanID]++;
    }
    else
    {
        printk("FreeChannel: invalid chan type 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_chan_soft_reset
*
* ��������  : ͨ������λ����
*
* �������  : u32ChanID       �����ͨ��ID
*
* �������  : ��
*
* �� �� ֵ  : �ͷųɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_chan_soft_reset(u32 u32ChanID)
{
    u32 u32RealChanID;
    u32 u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �ж�ͨ��ID�Ƿ���Ч */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32ChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32ChanID);
    /* ����Դͨ����λ��ֻ������̬���估LTE DSP/BBPͨ�� */
    /* ����ͨ���ĸ�λ���������漰����Ĵ�������������ӽӿ� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32RealChanID);
        (void)socp_reset_enc_chan(u32ChanID);
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpSoftResetEncSrcCnt[u32RealChanID]++;
    }
    else if (SOCP_DECODER_SRC_CHAN == u32ChanType)
    {
        SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECSRC_CHN);
        SOCP_CHECK_DECSRC_SET(u32RealChanID);

        (void)socp_reset_dec_chan(u32RealChanID);

        g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpSoftResetDecSrcCnt[u32RealChanID]++;
    }
    else
    {
        printk("ChnSoftReset: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_start
*
* ��������  : ������߽�����������
*
* �������  : u32SrcChanID      ͨ��ID
* �������  :
*
* �� �� ֵ  : �����ɹ����ı�ʶ��
*****************************************************************************/
/*lint -save -e529*/
s32 bsp_socp_start(u32 u32SrcChanID)
{
    u32 u32RealChanID;
    u32 u32ChanType;
    u32 u32Logic = (u32)1;
    u32 u32DstId;
    u32 i;
    u32 IntIDMask = 0;


    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �ж�ͨ��ID�Ƿ���Ч */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        if (u32RealChanID < SOCP_MAX_ENCSRC_CHN)
        {
            SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);
            SOCP_CHECK_ENCSRC_ALLOC(u32RealChanID);
        }
        else
        {
            printk("Start: enc src 0x%x is valid!\n", u32SrcChanID);
            return BSP_ERR_SOCP_INVALID_CHAN;
        }

        u32DstId = SOCP_REG_GETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32RealChanID), 4, 4);
        if (SOCP_CHN_SET != g_strSocpStat.sEncDstChan[u32DstId].u32SetStat)
        {
            printk("AppStart: enc dst chan is valid!\n");
            return BSP_ERR_SOCP_DEST_CHAN;
        }

        /* �����жϣ��ٴ��ж�*/
        SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT1, u32RealChanID, 1, 1);
        SOCP_REG_SETBITS(SOCP_REG_APP_MASK1, u32RealChanID, 1, 0);
        if (SOCP_ENCSRC_CHNMODE_LIST == g_strSocpStat.sEncSrcChan[u32RealChanID].eChnMode)
        {
            SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT3, u32RealChanID, 1, 1);
            /* ����RD�ж����� */
            //SOCP_REG_SETBITS(SOCP_REG_APP_MASK3, u32RealChanID, 1, 0);
        }

        /* ���ô�״̬*/
        SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32RealChanID), 0, 1, 1);
        if(u32RealChanID < SOCP_MAX_ENCSRC_CHN)
        {
            g_strSocpStat.sEncSrcChan[u32RealChanID].u32ChanEn = SOCP_CHN_ENABLE;
            g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpStartEncSrcCnt[u32RealChanID]++;
        }
    }
    else if (SOCP_DECODER_SRC_CHAN == u32ChanType)
    {
        SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECSRC_CHN);
        SOCP_CHECK_DECSRC_SET(u32RealChanID);

        /* ��rd����ж�*/
        if (SOCP_DECSRC_CHNMODE_LIST == g_strSocpStat.sDecSrcChan[u32RealChanID].eChnMode)
        {
            SOCP_REG_SETBITS(SOCP_REG_DEC_RAWINT1, u32RealChanID, 1, 1);
            SOCP_REG_SETBITS(SOCP_REG_DEC_MASK1, u32RealChanID, 1, 0);
        }

        for (i = 1; i < SOCP_DEC_SRCINT_NUM; i++)
        {
            IntIDMask  = SOCP_REG_GETBITS(SOCP_REG_DEC_RAWINT1, i * 4, 4);
            IntIDMask |= 1 << u32RealChanID;
            SOCP_REG_SETBITS(SOCP_REG_DEC_RAWINT1, i * 4, 4, IntIDMask);

            IntIDMask  = SOCP_REG_GETBITS(SOCP_REG_DEC_MASK1, i * 4, 4);
            IntIDMask &= ~(u32Logic << u32RealChanID);
            SOCP_REG_SETBITS(SOCP_REG_DEC_MASK1, i * 4, 4, IntIDMask);
        }

        /* ���ô�״̬*/
        SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32RealChanID), 30, 1, 1);
        g_strSocpStat.sDecSrcChan[u32RealChanID].u32ChanEn = SOCP_CHN_ENABLE;

        g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpStartDecSrcCnt[u32RealChanID]++;
    }
    else
    {
        printk("Start: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*lint -restore +e529*/

/*****************************************************************************
* �� �� ��  : bsp_socp_stop
*
* ��������  : ������߽���ֹͣ����
*
* �������  : u32SrcChanID      ͨ��ID
*
* �������  :
*
* �� �� ֵ  : ֹͣ�ɹ����ı�ʶ��
*****************************************************************************/
/*lint -save -e529*/
s32 bsp_socp_stop(u32 u32SrcChanID)
{
    u32 u32RealChanID;
    u32 u32ChanType;
    u32 IntIDMask = 0;
    u32 i;


    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �ж�ͨ��ID�Ƿ���Ч */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        if (u32RealChanID < SOCP_MAX_ENCSRC_CHN)
        {
            SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);
            SOCP_CHECK_ENCSRC_ALLOC(u32RealChanID);
        }
        else
        {
            printk("Stop: enc src 0x%x is valid!\n", u32SrcChanID);
            return BSP_ERR_SOCP_INVALID_CHAN;
        }

        SOCP_REG_SETBITS(SOCP_REG_APP_MASK1, u32RealChanID, 1, 1);
        if (SOCP_ENCSRC_CHNMODE_LIST == g_strSocpStat.sEncSrcChan[u32RealChanID].eChnMode)
        {
            SOCP_REG_SETBITS(SOCP_REG_APP_MASK3, u32RealChanID, 1, 1);
        }

        /* ����ͨ���ر�״̬*/
        SOCP_REG_SETBITS(SOCP_REG_ENCSRC_BUFCFG1(u32RealChanID), 0, 1, 0);
        if (u32RealChanID < SOCP_MAX_ENCSRC_CHN)
        {
            g_strSocpStat.sEncSrcChan[u32RealChanID].u32ChanEn = SOCP_CHN_DISABLE;
            g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpStopEncSrcCnt[u32RealChanID]++;
        }
    }
    else if (SOCP_DECODER_SRC_CHAN == u32ChanType)
    {
        SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECSRC_CHN);
        SOCP_CHECK_DECSRC_SET(u32RealChanID);

        /* �ر��ж�*/
        if (SOCP_DECSRC_CHNMODE_LIST == g_strSocpStat.sDecSrcChan[u32RealChanID].eChnMode)
        {
            SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, u32RealChanID, 1, 1);
        }

        for (i = 1; i < SOCP_DEC_SRCINT_NUM; i++)
        {
            IntIDMask  = SOCP_REG_GETBITS(SOCP_REG_DEC_MASK1, i * 4, 4);
            IntIDMask |= 1 << u32RealChanID;
            SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, i * 4, 4, IntIDMask);
        }

        /* ����ͨ���ر�״̬*/
        SOCP_REG_SETBITS(SOCP_REG_DECSRC_BUFCFG0(u32RealChanID), 30, 1,0);
        g_strSocpStat.sDecSrcChan[u32RealChanID].u32ChanEn = SOCP_CHN_DISABLE;

        g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpStopDecSrcCnt[u32RealChanID]++;
    }
    else
    {
        printk("Stop: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*lint -restore +e529*/
/*****************************************************************************
* �� �� ��  : bsp_socp_register_event_cb
*
* ��������  : �쳣�¼��ص�����ע�ắ��
*
* �������  : u32ChanID      ͨ��ID
*             EventCB        �쳣�¼��Ļص�����
* �������  :
*
* �� �� ֵ  : ע��ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_register_event_cb(u32 u32ChanID, socp_event_cb EventCB)
{
    u32  u32RealChanID;
    u32  u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* ��ȡͨ�����ͺ�ʵ�ʵ�ͨ��ID */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32ChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32ChanID);

    switch (u32ChanType)
    {
        case SOCP_CODER_SRC_CHAN:      /* ����Դͨ�� */
        {
            if (u32RealChanID < SOCP_MAX_ENCSRC_CHN)
            {
                SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);
                SOCP_CHECK_ENCSRC_ALLOC(u32RealChanID);
                g_strSocpStat.sEncSrcChan[u32RealChanID].event_cb = EventCB;

                g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpRegEventEncSrcCnt[u32RealChanID]++;
            }
            break;
        }
        case SOCP_CODER_DEST_CHAN:       /* ����Ŀ��ͨ��*/
        {
            SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_ENCDST_CHN);
            SOCP_CHECK_ENCDST_SET(u32RealChanID);

            g_strSocpStat.sEncDstChan[u32RealChanID].event_cb = EventCB;

            g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpRegEventEncDstCnt[u32RealChanID]++;
            break;
        }
        case SOCP_DECODER_SRC_CHAN:       /* ����Դͨ��*/
        {
            SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECSRC_CHN);
            SOCP_CHECK_DECSRC_SET(u32RealChanID);

            g_strSocpStat.sDecSrcChan[u32RealChanID].event_cb = EventCB;

            g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpRegEventDecSrcCnt[u32RealChanID]++;
            break;
        }
        case SOCP_DECODER_DEST_CHAN:       /* ����Ŀ��ͨ��*/
        {
            SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECDST_CHN);
            SOCP_CHECK_DECDST_ALLOC(u32RealChanID);

            g_strSocpStat.sDecDstChan[u32RealChanID].event_cb = EventCB;

            g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpRegEventDecDstCnt[u32RealChanID]++;
            break;
        }
        default:
        {
            printk("RegisterEventCB: invalid chan type: 0x%x!\n", u32ChanType);
            return BSP_ERR_SOCP_INVALID_CHAN;
        }
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_get_write_buff
*
* ��������  : �ϲ��ȡд����buffer����
*
* �������  : u32SrcChanID    Դͨ��ID
* �������  : pBuff           ��ȡ��buffer
*
* �� �� ֵ  : ��ȡ�ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_get_write_buff(u32 u32SrcChanID, SOCP_BUFFER_RW_STRU *pBuff)
{
    u32 u32ChanID;
    u32 u32ChanType;
    u32 uPAddr;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pBuff);

    /* ���ʵ��ͨ��id */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpGetWBufEncSrcEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_ENCSRC_CHAN_ID(u32ChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32ChanID);

        /* ���ݶ�дָ���ȡbuffer */
        SOCP_REG_READ(SOCP_REG_ENCSRC_BUFRPTR(u32ChanID), uPAddr);
        g_strSocpStat.sEncSrcChan[u32ChanID].sEncSrcBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_ENCSRC_BUFWPTR(u32ChanID), uPAddr);
        g_strSocpStat.sEncSrcChan[u32ChanID].sEncSrcBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        socp_get_idle_buffer(&g_strSocpStat.sEncSrcChan[u32ChanID].sEncSrcBuf, pBuff);
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpGetWBufEncSrcSucCnt[u32ChanID]++;
    }
    else if (SOCP_DECODER_SRC_CHAN == u32ChanType) /* ����ͨ�� */
    {
        g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpGetWBufDecSrcEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECSRC_CHN);
        SOCP_CHECK_DECSRC_SET(u32ChanID);
        SOCP_REG_READ(SOCP_REG_DECSRC_BUFRPTR(u32ChanID), uPAddr);
        g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_DECSRC_BUFWPTR(u32ChanID), uPAddr);
        g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        socp_get_idle_buffer(&g_strSocpStat.sDecSrcChan[u32ChanID].sDecSrcBuf, pBuff);
        g_stSocpDebugInfo.sSocpDebugDecSrc.u32SocpGetWBufDecSrcSucCnt[u32ChanID]++;
    }
    else
    {
        printk("GetWriteBuff: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_write_done
*
* ��������  : д������ɺ���
*
* �������  : u32SrcChanID    Դͨ��ID
              u32WrtSize      д�����ݵĳ���
*
* �������  :
*
* �� �� ֵ  : ����������ı�ʶ��
*****************************************************************************/
s32 bsp_socp_write_done(u32 u32SrcChanID, u32 u32WrtSize)
{
    u32 u32ChanID;
    u32 u32ChanType;
    u32 u32WrtPad;
    SOCP_BUFFER_RW_STRU RwBuff;
    u32  uPAddr;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(u32WrtSize);

    /* ���ʵ��ͨ��id */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        SOCP_ENCSRC_CHAN_S *pChan;

        g_stSocpDebugInfo.sSocpDebugEncSrc.u32socp_write_doneEncSrcEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_ENCSRC_CHAN_ID(u32ChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32ChanID);

        pChan = &g_strSocpStat.sEncSrcChan[u32ChanID];
        u32WrtPad = u32WrtSize % 8;
        if (0 != u32WrtPad)
        {
            u32WrtSize += (8 - u32WrtPad);
        }

        SOCP_REG_READ(SOCP_REG_ENCSRC_BUFWPTR(u32ChanID), uPAddr);
        pChan->sEncSrcBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_ENCSRC_BUFRPTR(u32ChanID), uPAddr);
        pChan->sEncSrcBuf.u32Read =SOCP_PHY_VIRT(uPAddr);

        socp_get_idle_buffer(&pChan->sEncSrcBuf, &RwBuff);

        if (RwBuff.u32Size + RwBuff.u32RbSize < u32WrtSize)
        {
            printk("WriteDone: enc src, too large write size!\n");
            printk("WriteDone: enc src, write ptr is 0x%x, read ptr is 0x%x\n", pChan->sEncSrcBuf.u32Write, pChan->sEncSrcBuf.u32Read);
            printk("WriteDone: enc src, u32Size is 0x%x, u32RbSize is 0x%x\n", RwBuff.u32Size, RwBuff.u32RbSize);
            printk("WriteDone: enc src, u32WrtSize is 0x%x, u32SrcChanID is 0x%x\n", u32WrtSize, u32SrcChanID);
            g_stSocpDebugInfo.sSocpDebugEncSrc.u32socp_write_doneEncSrcFailCnt[u32ChanID]++;
            return BSP_ERR_SOCP_INVALID_PARA;
        }

        /* ���ö�дָ�� */
        socp_write_done(&pChan->sEncSrcBuf, u32WrtSize);

        /* д��дָ�뵽дָ��Ĵ���*/
        uPAddr = SOCP_VIRT_PHY(pChan->sEncSrcBuf.u32Write); /* [false alarm]:����Fortify���� */
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_BUFWPTR(u32ChanID), uPAddr);
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32socp_write_doneEncSrcSucCnt[u32ChanID]++;
    }
    else  if(SOCP_DECODER_SRC_CHAN == u32ChanType) /* ����ͨ�� */
    {
        SOCP_DECSRC_CHAN_S  *pChan;

        g_stSocpDebugInfo.sSocpDebugDecSrc.u32socp_write_doneDecSrcEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECSRC_CHN);
        SOCP_CHECK_DECSRC_SET(u32ChanID);
        pChan = &g_strSocpStat.sDecSrcChan[u32ChanID];
        SOCP_REG_READ(SOCP_REG_DECSRC_BUFWPTR(u32ChanID), uPAddr);
        pChan->sDecSrcBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_DECSRC_BUFRPTR(u32ChanID), uPAddr);
        pChan->sDecSrcBuf.u32Read =SOCP_PHY_VIRT(uPAddr);
        socp_get_idle_buffer(&pChan->sDecSrcBuf, &RwBuff);

        if (RwBuff.u32Size + RwBuff.u32RbSize < u32WrtSize)
        {
            printk("WriteDone: dec src, too large write size!\n");
            printk("WriteDone: dec src, write ptr is 0x%x, read ptr is 0x%x\n", pChan->sDecSrcBuf.u32Write, pChan->sDecSrcBuf.u32Read);
            printk("WriteDone: dec src, u32Size is 0x%x, u32RbSize is 0x%x\n", RwBuff.u32Size, RwBuff.u32RbSize);
            printk("WriteDone: dec src, u32WrtSize is 0x%x, u32SrcChanID is 0x%x\n", u32WrtSize, u32SrcChanID);
            g_stSocpDebugInfo.sSocpDebugDecSrc.u32socp_write_doneDecSrcFailCnt[u32ChanID]++;

            return BSP_ERR_SOCP_INVALID_PARA;
        }

        /* ���ö�дָ�� */
        socp_write_done(&pChan->sDecSrcBuf, u32WrtSize);

        /* д��дָ�뵽дָ��Ĵ���*/
        uPAddr = SOCP_VIRT_PHY(pChan->sDecSrcBuf.u32Write); /* [false alarm]:����Fortify���� */
        SOCP_REG_WRITE(SOCP_REG_DECSRC_BUFWPTR(u32ChanID), uPAddr);
        g_stSocpDebugInfo.sSocpDebugDecSrc.u32socp_write_doneDecSrcSucCnt[u32ChanID]++;
    }
    else
    {
        printk("WriteDone: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_register_rd_cb
*
* ��������  : RDbuffer�ص�����ע�ắ��
*
* �������  : u32SrcChanID    Դͨ��ID
              RdCB            RDbuffer��ɻص�����
*
* �������  :
*
* �� �� ֵ  : ע��ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_register_rd_cb(u32 u32SrcChanID, socp_rd_cb RdCB)
{
    u32 u32RealChanID;
    u32 u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* ��ȡͨ�����ͺ�ʵ�ʵ�ͨ��ID */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32RealChanID);

        if (SOCP_ENCSRC_CHNMODE_LIST == g_strSocpStat.sEncSrcChan[u32RealChanID].eChnMode)
        {
            /* ���ö�Ӧͨ���Ļص�����*/
            g_strSocpStat.sEncSrcChan[u32RealChanID].rd_cb = RdCB;
        }
        else
        {
            printk("RegisterRdCB: invalid chan mode!\n");
            return BSP_ERR_SOCP_CHAN_MODE;
        }

        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpRegRdCBEncSrcCnt[u32RealChanID]++;
    }
    else
    {
        printk("RegisterRdCB: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_get_rd_buffer
*
* ��������  : ��ȡRDbuffer����
*
* �������  : u32SrcChanID    Դͨ��ID
*
* �������  : pBuff           ��ȡ��RDbuffer
*
* �� �� ֵ  : ��ȡ�ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_get_rd_buffer(u32 u32SrcChanID, SOCP_BUFFER_RW_STRU *pBuff)
{
    u32 u32ChanID;
    u32 u32ChanType;
    u32 uPAddr;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pBuff);

    /* ���ʵ��ͨ��id */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpGetRdBufEncSrcEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_ENCSRC_CHAN_ID(u32ChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32ChanID);

        /* ���ݶ�дָ���ȡbuffer */
        SOCP_REG_READ(SOCP_REG_ENCSRC_RDQRPTR(u32ChanID), uPAddr);
        g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_ENCSRC_RDQWPTR(u32ChanID), uPAddr);
        g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        socp_get_data_buffer(&g_strSocpStat.sEncSrcChan[u32ChanID].sRdBuf, pBuff);
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpGetRdBufEncSrcSucCnt[u32ChanID]++;
    }
    else
    {
        printk("GetRDBuffer: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}


/*****************************************************************************
* �� �� ��  : bsp_socp_read_rd_done
*
* ��������  : ��ȡRDbuffer������ɺ���
*
* �������  : u32SrcChanID   Դͨ��ID
              u32RDSize      ��ȡ��RDbuffer���ݳ���
*
* �������  :
*
* �� �� ֵ  : ��ȡ�ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_read_rd_done(u32 u32SrcChanID, u32 u32RDSize)
{
    u32 u32ChanID;
    u32 u32ChanType;
    SOCP_BUFFER_RW_STRU RwBuff;
    u32  uPAddr;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(u32RDSize);

    /* ���ʵ��ͨ��id */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32SrcChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32SrcChanID);

    /* ����ͨ�� */
    if (SOCP_CODER_SRC_CHAN == u32ChanType)
    {
        SOCP_ENCSRC_CHAN_S *pChan;

        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpReadRdDoneEncSrcEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_ENCSRC_CHAN_ID(u32ChanID);
        SOCP_CHECK_ENCSRC_ALLOC(u32ChanID);

        pChan = &g_strSocpStat.sEncSrcChan[u32ChanID];
        g_strSocpStat.sEncSrcChan[u32ChanID].u32LastRdSize = 0;

        /* ���ö�дָ�� */
        SOCP_REG_READ(SOCP_REG_ENCSRC_RDQWPTR(u32ChanID), uPAddr);
        pChan->sRdBuf.u32Write = SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_ENCSRC_RDQRPTR(u32ChanID), uPAddr);
        pChan->sRdBuf.u32Read  = SOCP_PHY_VIRT(uPAddr);
        socp_get_data_buffer(&pChan->sRdBuf, &RwBuff);

        if (RwBuff.u32Size + RwBuff.u32RbSize < u32RDSize)
        {
            printk("ReadRDDone: enc src, too large rd size!\n");
            printk("ReadRDDone: enc src, write ptr is 0x%x, read ptr is 0x%x\n", pChan->sRdBuf.u32Write, pChan->sRdBuf.u32Read);
            printk("ReadRDDone: enc src, u32Size is 0x%x, u32RbSize is 0x%x\n", RwBuff.u32Size, RwBuff.u32RbSize);
            printk("ReadRDDone: enc src, u32RDSize is 0x%x, u32SrcChanID is 0x%x\n", u32RDSize, u32SrcChanID);
            g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpReadRdDoneEncSrcFailCnt[u32ChanID]++;

            return BSP_ERR_SOCP_INVALID_PARA;
        }

        socp_read_done(&pChan->sRdBuf, u32RDSize);

        /* д��дָ�뵽дָ��Ĵ���*/
        uPAddr= SOCP_VIRT_PHY(pChan->sRdBuf.u32Read); /* [false alarm]:����Fortify���� */
        SOCP_REG_WRITE(SOCP_REG_ENCSRC_RDQRPTR(u32ChanID), uPAddr);
        g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpReadRdDoneEncSrcSucCnt[u32ChanID]++;
    }
    else
    {
        printk("ReadRDDone: invalid chan type: 0x%x!", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

//����Ŀ��ͨ��ר��

/*****************************************************************************
* �� �� ��  : bsp_socp_register_read_cb
*
* ��������  : �����ݻص�����ע�ắ��
*
* �������  : u32DestChanID  Ŀ��ͨ����ID
              ReadCB         �����ݻص�����
*
* �������  :
*
* �� �� ֵ  : ע��ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_register_read_cb(u32 u32DestChanID, socp_read_cb ReadCB)
{
    u32 u32RealChanID;
    u32 u32ChanType;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* ��ȡͨ�����ͺ�ʵ�ʵ�ͨ��ID */
    u32RealChanID = SOCP_REAL_CHAN_ID(u32DestChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DestChanID);

    if (SOCP_DECODER_DEST_CHAN == u32ChanType) /* ����ͨ�� */
    {
        SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_DECDST_CHN);
        SOCP_CHECK_DECDST_ALLOC(u32RealChanID);

        g_strSocpStat.sDecDstChan[u32RealChanID].read_cb = ReadCB;

        g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpRegReadCBDecDstCnt[u32RealChanID]++;
    }
    else if (SOCP_CODER_DEST_CHAN == u32ChanType)/* ����ͨ�� */
    {
        SOCP_CHECK_CHAN_ID(u32RealChanID, SOCP_MAX_ENCDST_CHN);
        SOCP_CHECK_ENCDST_SET(u32RealChanID);

        /* ���ö�Ӧͨ���Ļص�����*/
        g_strSocpStat.sEncDstChan[u32RealChanID].read_cb = ReadCB;

        g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpRegReadCBEncDstCnt[u32RealChanID]++;
    }
    else
    {
        printk("RegisterReadCB: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_get_read_buff
*
* ��������  : ��ȡ������buffer����
*
* �������  : u32DestChanID  Ŀ��ͨ��buffer

* �������  : pBuffer        ��ȡ�Ķ�����buffer
*
* �� �� ֵ  : ��ȡ�ɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_get_read_buff(u32 u32DestChanID, SOCP_BUFFER_RW_STRU *pBuffer)
{
    u32 u32ChanID;
    u32 u32ChanType;
    u32  uPAddr;

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /* �жϲ�����Ч�� */
    SOCP_CHECK_PARA(pBuffer);

    /* ���ʵ��ͨ��id */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32DestChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DestChanID);
    pBuffer->u32Size   = 0;
    pBuffer->u32RbSize = 0;

    if (SOCP_DECODER_DEST_CHAN == u32ChanType) /* ����ͨ�� */
    {
        g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpGetReadBufDecDstEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECDST_CHN);
        SOCP_CHECK_DECDST_ALLOC(u32ChanID);

        /* ���ݶ�дָ���ȡbuffer */
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFRPTR(u32ChanID), uPAddr);
        g_strSocpStat.sDecDstChan[u32ChanID].sDecDstBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFWPTR(u32ChanID), uPAddr);
        g_strSocpStat.sDecDstChan[u32ChanID].sDecDstBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        socp_get_data_buffer(&g_strSocpStat.sDecDstChan[u32ChanID].sDecDstBuf, pBuffer);
        g_stSocpDebugInfo.sSocpDebugDecDst.u32SocpGetReadBufDecDstSucCnt[u32ChanID]++;
    }
    else if (SOCP_CODER_DEST_CHAN == u32ChanType)
    {
        g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpGetReadBufEncDstEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_ENCDST_CHN);
        SOCP_CHECK_ENCDST_SET(u32ChanID);

        /* ���ݶ�дָ���ȡbuffer */
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFRPTR(u32ChanID), uPAddr);
        g_strSocpStat.sEncDstChan[u32ChanID].sEncDstBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFWPTR(u32ChanID), uPAddr);
        g_strSocpStat.sEncDstChan[u32ChanID].sEncDstBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        socp_get_data_buffer(&g_strSocpStat.sEncDstChan[u32ChanID].sEncDstBuf, pBuffer);
        g_stSocpDebugInfo.sSocpDebugEncDst.u32SocpGetReadBufEncDstSucCnt[u32ChanID]++;
    }
    else
    {
        printk("GetReadBuff: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_read_data_done
*
* ��������  : ��������ɺ���
*
* �������  : u32DestChanID    Ŀ��ͨ��ID
*             u32ReadSize      ��ȡ���ݴ�С
* �������  : ��
*
* �� �� ֵ  : �����ݳɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_read_data_done(u32 u32DestChanID, u32 u32ReadSize)
{
    u32 u32ChanID;
    u32 u32ChanType;
    SOCP_BUFFER_RW_STRU RwBuff;
    u32  uPAddr;
    unsigned long lock_flag;

    SOCP_ENC_DST_BUF_LOG_CFG_STRU * cfg;    
    cfg = bsp_socp_get_log_cfg(); 

    /* �ж��Ƿ��Ѿ���ʼ�� */
    SOCP_CHECK_INIT();

    /*����MSPҪ��ȥ���ü�⣬��֤���Ը���0�ֽڣ�2011-04-29*/
    //SOCP_CHECK_PARA(u32ReadSize);

    /* ���ʵ��ͨ��id */
    u32ChanID   = SOCP_REAL_CHAN_ID(u32DestChanID);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32DestChanID);

    if (SOCP_DECODER_DEST_CHAN == u32ChanType) /* ����ͨ�� */
    {
        u32 TfMaskReg = 0;
        SOCP_DECDST_CHAN_S *pChan;

        g_stSocpDebugInfo.sSocpDebugDecDst.u32socp_read_doneDecDstEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_DECDST_CHN);
        SOCP_CHECK_DECDST_ALLOC(u32ChanID);

        //added by yangzhi 2011.7.25
        TfMaskReg = SOCP_REG_DEC_CORE0MASK0;
        pChan = &g_strSocpStat.sDecDstChan[u32ChanID];
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFWPTR(u32ChanID), uPAddr);
        pChan->sDecDstBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFRPTR(u32ChanID), uPAddr);
        pChan->sDecDstBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        socp_get_data_buffer(&pChan->sDecDstBuf, &RwBuff);

        if(RwBuff.u32Size + RwBuff.u32RbSize < u32ReadSize)
        {
            printk("ReadDataDone: dec dst, too large read size!\n");
            printk("ReadDataDone: dec dst, write ptr is 0x%x, read ptr is 0x%x\n", pChan->sDecDstBuf.u32Write, pChan->sDecDstBuf.u32Read);
            printk("ReadDataDone: dec dst, u32Size is 0x%x, u32RbSize is 0x%x\n", RwBuff.u32Size, RwBuff.u32RbSize);
            printk("ReadDataDone: dec dst, u32ReadSize is 0x%x, u32DestChanID is 0x%x\n", u32ReadSize, u32DestChanID);
            spin_lock_irqsave(&lock, lock_flag);
            SOCP_REG_SETBITS(SOCP_REG_DEC_RAWINT0, u32ChanID, 1, 1);
            SOCP_REG_SETBITS(TfMaskReg, u32ChanID, 1, 0);
            spin_unlock_irqrestore(&lock, lock_flag);
            g_stSocpDebugInfo.sSocpDebugDecDst.u32socp_read_doneDecDstFailCnt[u32ChanID]++;

            return BSP_ERR_SOCP_INVALID_PARA;
        }

        /* ���ö�дָ�� */
        socp_read_done(&pChan->sDecDstBuf, u32ReadSize);

        /* д��дָ�뵽дָ��Ĵ���*/
        uPAddr = SOCP_VIRT_PHY(pChan->sDecDstBuf.u32Read); /* [false alarm]:����Fortify���� */
        SOCP_REG_WRITE(SOCP_REG_DECDEST_BUFRPTR(u32ChanID), uPAddr);
        //added by yangzhi 2011.7.25
        spin_lock_irqsave(&lock, lock_flag);
        SOCP_REG_SETBITS(SOCP_REG_DEC_RAWINT0, u32ChanID, 1, 1);
        SOCP_REG_SETBITS(TfMaskReg, u32ChanID, 1, 0);
        spin_unlock_irqrestore(&lock, lock_flag);

        if (0 == u32ReadSize)
        {
            g_stSocpDebugInfo.sSocpDebugDecDst.u32socp_read_doneZeroDecDstCnt[u32ChanID]++;
        }
        else
        {
            g_stSocpDebugInfo.sSocpDebugDecDst.u32socp_read_doneValidDecDstCnt[u32ChanID]++;
        }

        g_stSocpDebugInfo.sSocpDebugDecDst.u32socp_read_doneDecDstSucCnt[u32ChanID]++;

#if(FEATURE_SOCP_ON_DEMAND == FEATURE_ON)
        if(0 != down_interruptible(&g_stSocpPowerSem))
        {
            printk("%s:socp sem interruptible\n", __FUNCTION__);
        }

        /* ��������Ѿ�ͶƱ��׼���µ� */
        if(g_ulSocpPowerState == SOCP_POWER_OFF_REQ)
        {  
            if(BSP_OK == socp_can_sleep())
            {
                /* �µ�ǰ�������ж����μĴ��� */
                SOCP_REG_READ(SOCP_REG_ENC_MASK0, g_ul_encdst_tran_int_mask);
                SOCP_REG_READ(SOCP_REG_ENC_MASK2, g_ul_encdst_over_int_mask);
                SOCP_REG_READ(SOCP_REG_DEC_CORE0MASK0, g_ul_decdst_tran_int_mask);
                /* �����ж� */
                SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, 0, 7, 0x7f);
                SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 0, 7, 0x7f);
                SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 16, 7, 0x7f);
                SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, 0, 16, 0xffff);

                if(regulator_bulk_disable(1, &(g_stSocpVcc->socp_vcc)))
                {
                    printk("[%s]:fetal error, regulator disable fail\n", __FUNCTION__);
                }
                else
                {
                    g_ulSocpPowerState = SOCP_POWER_OFF;
                    printk("%s: socp goto power off\n", __FUNCTION__);
                }
            }
        }

        up(&g_stSocpPowerSem);
#endif

    }
    else if (SOCP_CODER_DEST_CHAN == u32ChanType)/* ����ͨ�� */
    {
        SOCP_ENCDST_CHAN_S *pChan;

        g_stSocpDebugInfo.sSocpDebugEncDst.u32socp_read_doneEncDstEtrCnt[u32ChanID]++;

        /* ����ͨ��id */
        SOCP_CHECK_CHAN_ID(u32ChanID, SOCP_MAX_ENCDST_CHN);
        SOCP_CHECK_ENCDST_SET(u32ChanID);

        if (0 == u32ReadSize)
        {
            g_stSocpDebugInfo.sSocpDebugEncDst.u32socp_read_doneZeroEncDstCnt[u32ChanID]++;
        }
        else
        {
            g_stSocpDebugInfo.sSocpDebugEncDst.u32socp_read_doneValidEncDstCnt[u32ChanID]++;
        }

        pChan = &g_strSocpStat.sEncDstChan[u32ChanID];
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFWPTR(u32ChanID), uPAddr);
        pChan->sEncDstBuf.u32Write= SOCP_PHY_VIRT(uPAddr);
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFRPTR(u32ChanID), uPAddr);
        pChan->sEncDstBuf.u32Read = SOCP_PHY_VIRT(uPAddr);
        socp_get_data_buffer(&pChan->sEncDstBuf, &RwBuff);

        if(RwBuff.u32Size + RwBuff.u32RbSize < u32ReadSize)
        {
            printk("ReadDataDone: enc dst, too large read size!\n");
            printk("ReadDataDone: enc dst, write ptr is 0x%x, read ptr is 0x%x\n", pChan->sEncDstBuf.u32Write, pChan->sEncDstBuf.u32Read);
            printk("ReadDataDone: enc dst, u32Size is 0x%x, u32RbSize is 0x%x\n", RwBuff.u32Size, RwBuff.u32RbSize);
            printk("ReadDataDone: enc dst, u32ReadSize is 0x%x, u32DestChanID is 0x%x\n", u32ReadSize, u32DestChanID);
            spin_lock_irqsave(&lock, lock_flag);
            SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT0, u32ChanID, 1, 1);
            SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, u32ChanID, 1, 0);
            spin_unlock_irqrestore(&lock, lock_flag);
            g_stSocpDebugInfo.sSocpDebugEncDst.u32socp_read_doneEncDstFailCnt[u32ChanID]++;
            return BSP_ERR_SOCP_INVALID_PARA;
        }

        /* ���ö�дָ�� */
        socp_read_done(&pChan->sEncDstBuf, u32ReadSize);

        /* д��дָ�뵽дָ��Ĵ���*/
        uPAddr = SOCP_VIRT_PHY(pChan->sEncDstBuf.u32Read); /* [false alarm]:����Fortify���� */
        SOCP_REG_WRITE(SOCP_REG_ENCDEST_BUFRPTR(u32ChanID), uPAddr);
        spin_lock_irqsave(&lock, lock_flag);
        SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT0, u32ChanID, 1, 1);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, u32ChanID, 1, 0);
        /* overflow int */
        SOCP_REG_SETBITS(SOCP_REG_ENC_RAWINT2, u32ChanID + 16, 1, 1);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, u32ChanID + 16, 1, 0);
        spin_unlock_irqrestore(&lock, lock_flag);
        g_stSocpDebugInfo.sSocpDebugEncDst.u32socp_read_doneEncDstSucCnt[u32ChanID]++;

#if(FEATURE_SOCP_ON_DEMAND == FEATURE_ON)
        if(0 != down_interruptible(&g_stSocpPowerSem))
        {
            printk("%s:socp sem interruptible\n", __FUNCTION__);
        }

        /* ��������Ѿ�ͶƱ��׼���µ� */
        if(g_ulSocpPowerState == SOCP_POWER_OFF_REQ)
        {  
            if(BSP_OK == socp_can_sleep())
            {
                /* �µ�ǰ�������ж����μĴ��� */
                SOCP_REG_READ(SOCP_REG_ENC_MASK0, g_ul_encdst_tran_int_mask);
                SOCP_REG_READ(SOCP_REG_ENC_MASK2, g_ul_encdst_over_int_mask);
                SOCP_REG_READ(SOCP_REG_DEC_CORE0MASK0, g_ul_decdst_tran_int_mask);
                /* �����ж� */
                SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, 0, 7, 0x7f);
                SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 0, 7, 0x7f);
                SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 16, 7, 0x7f);
                SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, 0, 16, 0xffff);

                if(regulator_bulk_disable(1, &(g_stSocpVcc->socp_vcc)))
                {
                    printk("[%s]:fetal error, regulator disable fail\n", __FUNCTION__);
                }
                else
                {
                    g_ulSocpPowerState = SOCP_POWER_OFF;
                    printk("%s: socp goto power off\n", __FUNCTION__);
                }
            }
        }

        up(&g_stSocpPowerSem);
#endif

        /* �ӳ��ϱ����� */
        if(TRUE == cfg->logOnFlag)
        {
#if(FEATURE_SOCP_ON_DEMAND == FEATURE_ON)
            if(g_ulSocpPowerState == SOCP_POWER_OFF)
            {
                return BSP_OK;
            }
#endif
            /* ���ݷǿգ���������Ŀ������ */
            if(0 != socp_is_encdst_chan_empty())
            {
                up(&g_strSocpStat.u32EncDstSemID);
            }
        }

    }
    else
    {
        printk("ReadDataDone: invalid chan type: 0x%x!\n", u32ChanType);
        return BSP_ERR_SOCP_INVALID_CHAN;
    }

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_set_bbp_enable
*
* ��������  : ʹ��/��ֹBPP LOG������
*
* �������  : bEnable       ʹ�ܱ�ʶ
*
* �������  : ��
*
* �� �� ֵ  : �����ݳɹ����ı�ʶ��
*****************************************************************************/
s32 bsp_socp_set_bbp_enable(int bEnable)
{
    if(bEnable)
    {
        BBP_REG_SETBITS(BBP_REG_CH_EN, 0, 1, 1);
    }
    else
    {
        BBP_REG_SETBITS(BBP_REG_CH_EN, 0, 1, 0);
    }
    return BSP_OK;
}


/*****************************************************************************
* �� �� ��  : bsp_socp_set_bbp_ds_mode
*
* ��������  : ����BPP����ģʽ
*
* �������  : eDsMode    ����ģʽ
*
* �������  : ��
*
* �� �� ֵ  :
*****************************************************************************/
s32 bsp_socp_set_bbp_ds_mode(SOCP_BBP_DS_MODE_ENUM_UIN32 eDsMode)
{
    if(eDsMode >= SOCP_BBP_DS_MODE_BUTT)
    {
        printk("SetBbpDsMode: invalid DS mode!\n");
        return BSP_ERR_SOCP_INVALID_PARA;
    }

    BBP_REG_SETBITS(BBP_REG_DS_CFG, 31, 1, eDsMode);
    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_help
*
* ��������  : ��ȡsocp��ӡ��Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_help(void)
{
    printk("\r |*************************************|\n");
    printk("\r socp_show_debug_gbl   : �鿴ȫ��ͳ����Ϣ:ͨ�����롢���ú��ж��ܼ������޲���\n");
    printk("\r socp_show_enc_src_chan_cur : �鿴����Դͨ�����ԣ�����Ϊͨ��ID\n");
    printk("\r socp_show_enc_src_chan_add : �鿴����Դͨ������ͳ��ֵ������Ϊͨ��ID\n");
    printk("\r socp_show_enc_src_chan_add : �鿴���б���Դͨ�����Ժ�ͳ��ֵ���޲���\n");
    printk("\r socp_show_enc_dst_chan_cur : �鿴����Ŀ��ͨ�����ԣ�����Ϊͨ��ID\n");
    printk("\r socp_show_enc_dst_chan_add : �鿴����Ŀ��ͨ������ͳ��ֵ������Ϊͨ��ID\n");
    printk("\r socp_show_enc_dst_chan_all : �鿴���б���Ŀ��ͨ�����Ժ�ͳ��ֵ���޲���\n");
    printk("\r socp_show_dec_src_chan_cur : �鿴����Դͨ�����ԣ�����Ϊͨ��ID\n");
    printk("\r socp_show_dec_src_chan_add : �鿴����Դͨ������ͳ��ֵ������Ϊͨ��ID\n");
    printk("\r socp_show_dec_src_chan_all : �鿴���н���Դͨ�����Ժ�ͳ��ֵ���޲���\n");
    printk("\r socp_show_dec_dst_chan_cur : �鿴����Ŀ��ͨ�����ԣ�����Ϊͨ��ID\n");
    printk("\r socp_show_dec_dst_chan_add : �鿴����Ŀ��ͨ������ͳ��ֵ������Ϊͨ��ID\n");
    printk("\r socp_show_dec_dst_chan_all : �鿴���н���Ŀ��ͨ�����Ժ�ͳ��ֵ���޲���\n");
    printk("\r socp_show_ccore_head_err_cnt : �鿴C�����б���Դͨ����ͷ����ͳ��ֵ���޲���\n");
    printk("\r socp_debug_cnt_show : �鿴ȫ��ͳ����Ϣ���޲���\n");
}

/*****************************************************************************
* �� �� ��   : socp_show_debug_gbl
*
* ��������  : ��ʾȫ��debug ������Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_show_debug_gbl(void)
{
    SOCP_DEBUG_GBL_S *sSocpDebugGblInfo;

    sSocpDebugGblInfo = &g_stSocpDebugInfo.sSocpDebugGBl;

    printk("\r SOCPȫ��״̬ά����Ϣ:\n");
    printk("\r socp����ַ:                                            : 0x%x\n",
           (s32)g_strSocpStat.baseAddr);
    printk("\r socp�������Դͨ���Ĵ���                               : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpAllocEncSrcCnt);
    printk("\r socp�������Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpAllocEncSrcSucCnt);
    printk("\r socp���ñ���Ŀ��ͨ���Ĵ���                              : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpSetEncDstCnt);
    printk("\r socp���ñ���Ŀ��ͨ���ɹ��Ĵ���                        : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpSetEncDstSucCnt);
    printk("\r socp���ý���Դͨ���Ĵ���                                  : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpSetDecSrcCnt);
    printk("\r socp���ý���Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpSetDeSrcSucCnt);
    printk("\r socp�������Ŀ��ͨ���Ĵ���                              : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpAllocDecDstCnt);
    printk("\r socp�������Ŀ��ͨ���ɹ��Ĵ���                        : 0x%x\n",
           (s32)sSocpDebugGblInfo->u32SocpAllocDecDstSucCnt);
    printk("\r socp����APP�жϵĴ���          : 0x%x\n", (s32)sSocpDebugGblInfo->u32SocpAppEtrIntCnt);
    printk("\r socp���APP�жϵĴ���          : 0x%x\n", (s32)sSocpDebugGblInfo->u32SocpAppSucIntCnt);
}

/*****************************************************************************
* �� �� ��  : socp_show_ccore_head_err_cnt
*
* ��������  : ��ӡC�˱���Դͨ����ͷ����ͳ��ֵ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_show_ccore_head_err_cnt(void)
{
    int i;
    for(i = SOCP_CCORE_ENCSRC_CHN_BASE; i < SOCP_CCORE_ENCSRC_CHN_BASE + SOCP_CCORE_ENCSRC_CHN_NUM; i++)
    {
        printk("================== ����Դͨ�� 0x%x  ��ͷ�����ۼ�ͳ��ֵ:=================\n", i);
        printk("\r socp ISR �н������Դͨ����ͷ�����жϴ���                  : 0x%x\n",
            (s32)g_stSocpDebugInfo.sSocpDebugEncSrc.u32SocpEncSrcIsrHeadIntCnt[i]);
    }
}

/*****************************************************************************
* �� �� ��   : socp_show_enc_src_chan_cur
*
* ��������  : ��ӡ����Դͨ����ǰ����
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/

/*lint -save -e529*/
u32 socp_show_enc_src_chan_cur(u32 u32UniqueId)
{
    u32 u32RealId   = 0;
    u32 u32ChanType = 0;

    u32RealId   = SOCP_REAL_CHAN_ID(u32UniqueId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);

    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_CODER_SRC_CHAN);
    SOCP_CHECK_ENCSRC_CHAN_ID(u32RealId);

    printk("================== ����ı���Դͨ�� 0x%x  ����:=================\n", u32UniqueId);
    printk("ͨ��ID:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].u32ChanID);
    printk("ͨ������״̬:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].u32AllocStat);
    printk("ͨ��ʹ��״̬:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].u32ChanEn);
    printk("Ŀ��ͨ��ID:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].u32DestChanID);
    printk("ͨ�����ȼ�:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].ePriority);
    printk("ͨ����·״̬:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].u32BypassEn);
    printk("ͨ�����ݸ�ʽ����:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].eChnMode);
    printk("ͨ������ģ����:\t\t%d\n", g_strSocpStat.sEncSrcChan[u32RealId].eDataType);
    printk("ͨ��buffer ��ʼ��ַ:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sEncSrcBuf.u32Start);
    printk("ͨ��buffer ������ַ:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sEncSrcBuf.u32End);
    printk("ͨ��buffer ��ָ��:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sEncSrcBuf.u32Read);
    printk("ͨ��buffer дָ��:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sEncSrcBuf.u32Write);
    printk("ͨ��buffer ����:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sEncSrcBuf.u32Length);
    if (SOCP_ENCSRC_CHNMODE_LIST == g_strSocpStat.sEncSrcChan[u32RealId].eChnMode)
    {
        printk("ͨ��RD buffer ��ʼ��ַ:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sRdBuf.u32Start);
        printk("ͨ��RD buffer ������ַ:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sRdBuf.u32End);
        printk("ͨ��RD buffer ��ָ��:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sRdBuf.u32Read);
        printk("ͨ��RD buffer дָ��:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sRdBuf.u32Write);
        printk("ͨ��RD buffer ����:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].sRdBuf.u32Length);
        printk("ͨ��RD buffer ����:\t\t0x%x\n", g_strSocpStat.sEncSrcChan[u32RealId].u32RdThreshold);
    }

    return BSP_OK;
}

/*lint -restore +e529*/

/*****************************************************************************
* �� �� ��   : socp_show_enc_src_chan_add
*
* ��������  : ��ӡ����Դͨ���ۼ�ͳ��ֵ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_enc_src_chan_add(u32 u32UniqueId)
{
    u32 u32ChanType;
    u32 u32RealChanID;
    SOCP_DEBUG_ENCSRC_S *sSocpAddDebugEncSrc;

    u32RealChanID = SOCP_REAL_CHAN_ID(u32UniqueId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_CODER_SRC_CHAN);

    sSocpAddDebugEncSrc = &g_stSocpDebugInfo.sSocpDebugEncSrc;
    SOCP_CHECK_ENCSRC_CHAN_ID(u32RealChanID);

    printk("================== ����Դͨ�� 0x%x  �ۼ�ͳ��ֵ:=================\n", u32UniqueId);
    printk("\r socp�ͷű���Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpFreeEncSrcCnt[u32RealChanID]);
    printk("\r socp��������Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpStartEncSrcCnt[u32RealChanID]);
    printk("\r socpֹͣ����Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpStopEncSrcCnt[u32RealChanID]);
    printk("\r socp����λ����Դͨ���ɹ��Ĵ���                        : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpSoftResetEncSrcCnt[u32RealChanID]);
    printk("\r socpע�����Դͨ���쳣���������Ĵ���               : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpRegEventEncSrcCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����Ի��дbuffer�Ĵ���                  : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpGetWBufEncSrcEtrCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����дbuffer�ɹ��Ĵ���                  : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpGetWBufEncSrcSucCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����Ը���дbufferָ��Ĵ���            : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32socp_write_doneEncSrcEtrCnt[u32RealChanID]);
    printk("\r socp����Դͨ������дbufferָ��ɹ��Ĵ���            : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32socp_write_doneEncSrcSucCnt[u32RealChanID]);
    printk("\r socp����Դͨ������дbufferָ��ʧ�ܵĴ���            : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32socp_write_doneEncSrcFailCnt[u32RealChanID]);
    printk("\r socp����Դͨ��ע��RD buffer�ص������ɹ��Ĵ���      : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpRegRdCBEncSrcCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����Ի��RD buffer�Ĵ���                   : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpGetRdBufEncSrcEtrCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����RD buffer�ɹ��Ĵ���                   : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpGetRdBufEncSrcSucCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����Ը���RDbufferָ��Ĵ���              : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpReadRdDoneEncSrcEtrCnt[u32RealChanID]);
    printk("\r socp����Դͨ������RDbufferָ��ɹ��Ĵ���              : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpReadRdDoneEncSrcSucCnt[u32RealChanID]);
    printk("\r socp����Դͨ������RDbufferָ��ʧ�ܵĴ���              : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpReadRdDoneEncSrcFailCnt[u32RealChanID]);
    printk("\r socp ISR �н������Դͨ����ͷ�����жϴ���                  : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpEncSrcIsrHeadIntCnt[u32RealChanID]);
    printk("\r socp �����лص�����Դͨ����ͷ�����жϴ�����������   : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpEncSrcTskHeadCbOriCnt[u32RealChanID]);
    printk("\r socp �ص�����Դͨ����ͷ�����жϴ��������ɹ��Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpEncSrcTskHeadCbCnt[u32RealChanID]);
    printk("\r socp ISR �н������Դͨ��Rd ����жϴ���                     : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpEncSrcIsrRdIntCnt[u32RealChanID]);
    printk("\r socp �����лص�����Դͨ��Rd ����жϴ�����������       : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpEncSrcTskRdCbOriCnt[u32RealChanID]);
    printk("\r socp �ص�����Դͨ��Rd ����жϴ��������ɹ��Ĵ���       : 0x%x\n",
           (s32)sSocpAddDebugEncSrc->u32SocpEncSrcTskRdCbCnt[u32RealChanID]);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_enc_src_chan_add
*
* ��������  : ��ӡ���б���Դͨ����Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void  socp_show_enc_src_chan_all(void)
{
    u32 i;

    for (i = 0; i < SOCP_MAX_ENCSRC_CHN; i++)
    {
        (void)socp_show_enc_src_chan_cur(i);
        (void)socp_show_enc_src_chan_add(i);
    }

    return;
}

/*****************************************************************************
* �� �� ��   : socp_show_enc_dst_chan_cur
*
* ��������  : ��ӡ����Ŀ��ͨ����Ϣ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_enc_dst_chan_cur(u32 u32UniqueId)
{
    u32 u32RealId   = 0;
    u32 u32ChanType = 0;

    u32RealId   = SOCP_REAL_CHAN_ID(u32UniqueId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);

    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_CODER_DEST_CHAN);

    printk("================== ����Ŀ��ͨ�� 0x%x  ����:=================\n", u32UniqueId);
    printk("ͨ��ID:\t\t%d\n", g_strSocpStat.sEncDstChan[u32RealId].u32ChanID);
    printk("ͨ������״̬:\t\t%d\n", g_strSocpStat.sEncDstChan[u32RealId].u32SetStat);
    printk("ͨ��buffer ��ʼ��ַ:\t\t0x%x\n", g_strSocpStat.sEncDstChan[u32RealId].sEncDstBuf.u32Start);
    printk("ͨ��buffer ������ַ:\t\t0x%x\n", g_strSocpStat.sEncDstChan[u32RealId].sEncDstBuf.u32End);
    printk("ͨ��buffer ��ָ��:\t\t0x%x\n", g_strSocpStat.sEncDstChan[u32RealId].sEncDstBuf.u32Read);
    printk("ͨ��buffer дָ��:\t\t0x%x\n", g_strSocpStat.sEncDstChan[u32RealId].sEncDstBuf.u32Write);
    printk("ͨ��buffer ����:\t\t0x%x\n", g_strSocpStat.sEncDstChan[u32RealId].sEncDstBuf.u32Length);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_enc_dst_chan_add
*
* ��������  : ��ӡ����Ŀ��ͨ���ۼ�ͳ��ֵ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_enc_dst_chan_add(u32 u32UniqueId)
{
    u32 u32RealChanID;
    u32 u32ChanType = 0;
    SOCP_DEBUG_ENCDST_S *sSocpAddDebugEncDst;

    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_CODER_DEST_CHAN);

    u32RealChanID = SOCP_REAL_CHAN_ID(u32UniqueId);
    sSocpAddDebugEncDst = &g_stSocpDebugInfo.sSocpDebugEncDst;

    printk("================== ����Ŀ��ͨ�� 0x%x  �ۼ�ͳ��ֵ:=================\n", u32UniqueId);
    printk("\r socpע�����Ŀ��ͨ���쳣���������Ĵ���           : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpRegEventEncDstCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ��ע������ݻص������ɹ��Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpRegReadCBEncDstCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����Ի�ö�buffer �Ĵ���                : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpGetReadBufEncDstEtrCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ����ö�buffer�ɹ��Ĵ���                : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpGetReadBufEncDstSucCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����Ը��¶�bufferָ��Ĵ���          : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32socp_read_doneEncDstEtrCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ��ɹ��Ĵ���          : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32socp_read_doneEncDstSucCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ��ʧ�ܵĴ���          : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32socp_read_doneEncDstFailCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ���ƶ�0 �ֽڳɹ��Ĵ���       : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32socp_read_doneZeroEncDstCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ���ƶ���0 �ֽڳɹ��Ĵ���    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32socp_read_doneValidEncDstCnt[u32RealChanID]);
    printk("\r socpISR �н������Ŀ��ͨ����������жϴ���  : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstIsrTrfIntCnt[u32RealChanID]);
    printk("\r socp�����лص�����Ŀ��ͨ����������жϴ�����������  : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstTskTrfCbOriCnt[u32RealChanID]);
    printk("\r socp�ص�����Ŀ��ͨ����������жϴ��������ɹ��Ĵ���  : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstTskTrfCbCnt[u32RealChanID]);
    printk("\r socpISR �н������Ŀ��ͨ��buf ����жϴ���    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstIsrOvfIntCnt[u32RealChanID]);
    printk("\r socp�����лص�����Ŀ��ͨ��buf ����жϴ�����������    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstTskOvfCbOriCnt[u32RealChanID]);
    printk("\r socp�ص�����Ŀ��ͨ��buf ����жϴ��������ɹ��Ĵ���    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstTskOvfCbCnt[u32RealChanID]);
    printk("\r socpISR �н������Ŀ��ͨ��buf��ֵ����жϴ���    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstIsrThresholdOvfIntCnt[u32RealChanID]);
    printk("\r socp�����лص�����Ŀ��ͨ��buf��ֵ����жϴ�����������    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstTskThresholdOvfCbOriCnt[u32RealChanID]);
    printk("\r socp�ص�����Ŀ��ͨ��buf��ֵ����жϴ��������ɹ��Ĵ���    : 0x%x\n",
           (s32)sSocpAddDebugEncDst->u32SocpEncDstTskThresholdOvfCbCnt[u32RealChanID]);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_enc_dst_chan_all
*
* ��������  : ��ӡ����Ŀ��ͨ����Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_show_enc_dst_chan_all(void)
{
    u32 i;
    u32 u32UniqueId = 0;

    for (i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
    {
        u32UniqueId = SOCP_CHAN_DEF(SOCP_CODER_DEST_CHAN, i);
        (void)socp_show_enc_dst_chan_cur(u32UniqueId);
        (void)socp_show_enc_dst_chan_add(u32UniqueId);
    }

    return;
}

/*****************************************************************************
* �� �� ��   : socp_show_dec_src_chan_cur
*
* ��������  : ��ӡ����Դͨ����Ϣ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_dec_src_chan_cur(u32 u32UniqueId)
{
    u32 u32RealId   = 0;
    u32 u32ChanType = 0;

    u32RealId   = SOCP_REAL_CHAN_ID(u32UniqueId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);

    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);

    printk("================== ����Դͨ�� 0x%x  ����:=================\n", u32UniqueId);
    printk("ͨ��ID:\t\t%d\n", g_strSocpStat.sDecSrcChan[u32RealId].u32ChanID);
    printk("ͨ������״̬:\t\t%d\n", g_strSocpStat.sDecSrcChan[u32RealId].u32SetStat);
    printk("ͨ��ʹ��״̬:\t\t%d\n", g_strSocpStat.sDecSrcChan[u32RealId].u32ChanEn);
    printk("ͨ��ģʽ:\t\t%d\n", g_strSocpStat.sDecSrcChan[u32RealId].eChnMode);
    printk("ͨ��buffer ��ʼ��ַ:\t\t0x%x\n", g_strSocpStat.sDecSrcChan[u32RealId].sDecSrcBuf.u32Start);
    printk("ͨ��buffer ������ַ:\t\t0x%x\n", g_strSocpStat.sDecSrcChan[u32RealId].sDecSrcBuf.u32End);
    printk("ͨ��buffer ��ָ��:\t\t0x%x\n", g_strSocpStat.sDecSrcChan[u32RealId].sDecSrcBuf.u32Read);
    printk("ͨ��buffer дָ��:\t\t0x%x\n", g_strSocpStat.sDecSrcChan[u32RealId].sDecSrcBuf.u32Write);
    printk("ͨ��buffer ����:\t\t0x%x\n", g_strSocpStat.sDecSrcChan[u32RealId].sDecSrcBuf.u32Length);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_dec_src_chan_add
*
* ��������  : ��ӡ����Դͨ���ۼ�ͳ��ֵ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_dec_src_chan_add(u32 u32UniqueId)
{
    u32 u32RealChanID;
    SOCP_DEBUG_DECSRC_S *sSocpAddDebugDecSrc;
    u32 u32ChanType = 0;

    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_SRC_CHAN);

    u32RealChanID = SOCP_REAL_CHAN_ID(u32UniqueId);
    sSocpAddDebugDecSrc = &g_stSocpDebugInfo.sSocpDebugDecSrc;

    printk("================== ����Դͨ�� 0x%x  �ۼ�ͳ��ֵ:=================\n", u32UniqueId);
    printk("\r socp����λ����Դͨ���ɹ��Ĵ���                        : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpSoftResetDecSrcCnt[u32RealChanID]);
    printk("\r socp��������Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpStartDecSrcCnt[u32RealChanID]);
    printk("\r socpֹͣ����Դͨ���ɹ��Ĵ���                           : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpStopDecSrcCnt[u32RealChanID]);
    printk("\r socpע�����Դͨ���쳣���������Ĵ���              : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpRegEventDecSrcCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����Ի��дbuffer�Ĵ���                  : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpGetWBufDecSrcEtrCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����дbuffer�ɹ��Ĵ���                  : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpGetWBufDecSrcSucCnt[u32RealChanID]);
    printk("\r socp����Դͨ�����Ը���дbufferָ��Ĵ���            : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32socp_write_doneDecSrcEtrCnt[u32RealChanID]);
    printk("\r socp����Դͨ������дbufferָ��ɹ��Ĵ���            : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32socp_write_doneDecSrcSucCnt[u32RealChanID]);
    printk("\r socp����Դͨ������дbufferָ��ʧ�ܵĴ���            : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32socp_write_doneDecSrcFailCnt[u32RealChanID]);
    printk("\r socpISR �н������Դͨ�������жϴ���                      : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpDecSrcIsrErrIntCnt[u32RealChanID]);
    printk("\r socp�����лص�����Դͨ�������жϴ�����������       : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpDecSrcTskErrCbOriCnt[u32RealChanID]);
    printk("\r socp�ص�����Դͨ�������жϴ��������ɹ��Ĵ���       : 0x%x\n",
           (s32)sSocpAddDebugDecSrc->u32SocpDecSrcTskErrCbCnt[u32RealChanID]);
    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_dec_src_chan_all
*
* ��������  : ��ӡ����Դͨ����Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_show_dec_src_chan_all(void)
{
    u32 i;
    u32 u32UniqueId = 0;

    for (i = 0; i < SOCP_MAX_DECSRC_CHN; i++)
    {
        u32UniqueId = SOCP_CHAN_DEF(SOCP_DECODER_SRC_CHAN, i);
        (void)socp_show_dec_src_chan_cur(u32UniqueId);
        (void)socp_show_dec_src_chan_add(u32UniqueId);
    }

    return;
}

/*****************************************************************************
* �� �� ��   : socp_show_dec_dst_chan_cur
*
* ��������  : ��ӡ����Ŀ��ͨ����Ϣ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_dec_dst_chan_cur(u32 u32UniqueId)
{
    u32 u32RealId   = 0;
    u32 u32ChanType = 0;

    u32RealId   = SOCP_REAL_CHAN_ID(u32UniqueId);
    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);

    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_DEST_CHAN);

    printk("================== ����Ŀ��ͨ�� 0x%x  ����:=================\n", u32UniqueId);
    printk("ͨ��ID:\t\t%d\n", g_strSocpStat.sDecDstChan[u32RealId].u32ChanID);
    printk("ͨ������״̬:\t\t%d\n", g_strSocpStat.sDecDstChan[u32RealId].u32AllocStat);
    printk("ͨ��ʹ��ģ����:\t\t%d\n", g_strSocpStat.sDecDstChan[u32RealId].eDataType);
    printk("ͨ��buffer ��ʼ��ַ:\t\t0x%x\n", g_strSocpStat.sDecDstChan[u32RealId].sDecDstBuf.u32Start);
    printk("ͨ��buffer ������ַ:\t\t0x%x\n", g_strSocpStat.sDecDstChan[u32RealId].sDecDstBuf.u32End);
    printk("ͨ��buffer ��ָ��:\t\t0x%x\n", g_strSocpStat.sDecDstChan[u32RealId].sDecDstBuf.u32Read);
    printk("ͨ��buffer дָ��:\t\t0x%x\n", g_strSocpStat.sDecDstChan[u32RealId].sDecDstBuf.u32Write);
    printk("ͨ��buffer ����:\t\t0x%x\n", g_strSocpStat.sDecDstChan[u32RealId].sDecDstBuf.u32Length);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_dec_dst_chan_add
*
* ��������  : ��ӡ����Ŀ��ͨ���ۼ�ͳ��ֵ
*
* �������  : ͨ��ID
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
u32 socp_show_dec_dst_chan_add(u32 u32UniqueId)
{
    u32 u32RealChanID;
    SOCP_DEBUG_DECDST_S *sSocpAddDebugDecDst;
    u32 u32ChanType = 0;

    u32ChanType = SOCP_REAL_CHAN_TYPE(u32UniqueId);
    SOCP_CHECK_CHAN_TYPE(u32ChanType, SOCP_DECODER_DEST_CHAN);

    u32RealChanID = SOCP_REAL_CHAN_ID(u32UniqueId);
    sSocpAddDebugDecDst = &g_stSocpDebugInfo.sSocpDebugDecDst;

    printk("================== ����Ŀ��ͨ�� 0x%x  �ۼ�ͳ��ֵ:=================\n", u32UniqueId);
    printk("\r socp�ͷŽ���Ŀ��ͨ���ɹ��Ĵ���                        : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpFreeDecDstCnt[u32RealChanID]);
    printk("\r socpע�����Ŀ��ͨ���쳣���������Ĵ���           : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpRegEventDecDstCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ��ע������ݻص������ɹ��Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpRegReadCBDecDstCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����Ի�ö�buffer�Ĵ���                : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpGetReadBufDecDstEtrCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ����ö�buffer�ɹ��Ĵ���                : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpGetReadBufDecDstSucCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����Ը��¶�bufferָ��Ĵ���          : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32socp_read_doneDecDstEtrCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ��ɹ��Ĵ���          : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32socp_read_doneDecDstSucCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ��ʧ�ܵĴ���          : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32socp_read_doneDecDstFailCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ���ƶ�0 �ֽڳɹ��Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32socp_read_doneZeroDecDstCnt[u32RealChanID]);
    printk("\r socp����Ŀ��ͨ�����¶�bufferָ���ƶ���0 �ֽڳɹ��Ĵ���: 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32socp_read_doneValidDecDstCnt[u32RealChanID]);
    printk("\r socpISR �н������Ŀ��ͨ����������жϴ���                : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpDecDstIsrTrfIntCnt[u32RealChanID]);
    printk("\r socp������ �ص�����Ŀ��ͨ����������жϴ��������Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpDecDstTskTrfCbOriCnt[u32RealChanID]);
    printk("\r socp�ص�����Ŀ��ͨ����������жϴ��������ɹ��Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpDecDstTskTrfCbCnt[u32RealChanID]);
    printk("\r socpISR �н������Ŀ��ͨ��buf ����жϴ���                  : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpDecDstIsrOvfIntCnt[u32RealChanID]);
    printk("\r socp������ �ص�����Ŀ��ͨ��buf ����жϴ�����������  : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpDecDstTskOvfCbOriCnt[u32RealChanID]);
    printk("\r socp�ص�����Ŀ��ͨ��buf ����жϴ��������ɹ��Ĵ���   : 0x%x\n",
           (s32)sSocpAddDebugDecDst->u32SocpDecDstTskOvfCbCnt[u32RealChanID]);

    return BSP_OK;
}

/*****************************************************************************
* �� �� ��   : socp_show_dec_dst_chan_all
*
* ��������  : ��ӡ����Ŀ��ͨ����Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_show_dec_dst_chan_all(void)
{
    u32 i;
    u32 u32UniqueId = 0;

    for (i = 0; i < SOCP_MAX_DECDST_CHN; i++)
    {
        u32UniqueId = SOCP_CHAN_DEF(SOCP_DECODER_DEST_CHAN, i);
        (void)socp_show_dec_dst_chan_cur(u32UniqueId);
        (void)socp_show_dec_dst_chan_add(u32UniqueId);
    }

    return;
}

/*****************************************************************************
* �� �� ��   : socp_debug_cnt_show
*
* ��������  : ��ʾdebug ������Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ   : ��
*****************************************************************************/
void socp_debug_cnt_show(void)
{
    socp_show_debug_gbl();
    socp_show_enc_src_chan_all();
    socp_show_enc_dst_chan_all();
    socp_show_dec_src_chan_all();
    socp_show_dec_dst_chan_all();
}

#define MALLOC_MAX_SIZE     0x100000
#define MALLOC_MAX_INDEX    8           /*page_size Ϊ4K*/
#define SOCP_PAGE_SIZE      0x1000

//__inline
s32 socp_get_index(u32 u32Size,u32 *index)
{
    u32 i = 0;
    if(u32Size > MALLOC_MAX_SIZE)
    {
        return BSP_ERROR;
    }
    for(i=0;i<=MALLOC_MAX_INDEX;i++)
    {
        if(u32Size <= (u32)(SOCP_PAGE_SIZE * (1<<i)))
        {
            *index = i;
            break;
        }
    }
    return BSP_OK;
}

void* socp_malloc(u32 u32Size)
{
    u8 *pItem= NULL;
    u32 index = 0;

    if(BSP_OK != socp_get_index(u32Size,&index))
    {
        return BSP_NULL;
    }

    index = 4;
    /* �����ڴ� */
    pItem = (u8*)__get_free_pages(GFP_KERNEL,index);
    if(!pItem)
    {
        printk("%s: malloc failed\n", __FUNCTION__);
        return BSP_NULL;
    }

    return (void*)SOCP_VIRT_PHY(pItem);
}

s32 socp_free(void* pMem)
{
    u32 *pItem;

    pItem = SOCP_PHY_VIRT(pMem);

    free_pages((unsigned long)pItem,4);
    return BSP_OK;
}

/* �͹������ begin */
/* �͹��Ĳ����ݲ��޸� */
void BSP_SOCP_DrxRestoreRegAppOnly(void)
{
    u32 i= 0;

    for(i=0;i<SOCP_MAX_ENCDST_CHN;i++)
    {
	    SOCP_REG_READ(SOCP_REG_ENCDEST_BUFRPTR(i),g_strSocpStat.sEncDstChan[i].sEncDstBuf.u32Read);
	    SOCP_REG_READ(SOCP_REG_ENCDEST_BUFWPTR(i),g_strSocpStat.sEncDstChan[i].sEncDstBuf.u32Write);
    }

    for(i=0;i<SOCP_MAX_DECDST_CHN;i++)
    {
    	SOCP_REG_READ(SOCP_REG_DECDEST_BUFRPTR(i),g_strSocpStat.sDecDstChan[i].sDecDstBuf.u32Read);
	    SOCP_REG_READ(SOCP_REG_DECDEST_BUFWPTR(i),g_strSocpStat.sDecDstChan[i].sDecDstBuf.u32Write);
    }

    for(i=0;i<SOCP_MAX_ENCSRC_CHN;i++)
    {
        SOCP_REG_READ(SOCP_REG_ENCSRC_BUFRPTR(i),  g_strSocpStat.sEncSrcChan[i].sEncSrcBuf.u32Read);
	    SOCP_REG_READ(SOCP_REG_ENCSRC_BUFWPTR(i),g_strSocpStat.sEncSrcChan[i].sEncSrcBuf.u32Write);

        SOCP_REG_READ(SOCP_REG_ENCSRC_RDQRPTR(i),  g_strSocpStat.sEncSrcChan[i].sRdBuf.u32Read);
	    SOCP_REG_READ(SOCP_REG_ENCSRC_RDQWPTR(i),g_strSocpStat.sEncSrcChan[i].sRdBuf.u32Write);
    }

    for(i=0;i<SOCP_MAX_DECSRC_CHN;i++)
    {
        SOCP_REG_READ(SOCP_REG_DECSRC_BUFRPTR(i),  g_strSocpStat.sDecSrcChan[i].sDecSrcBuf.u32Read);
	    SOCP_REG_READ(SOCP_REG_DECSRC_BUFWPTR(i),g_strSocpStat.sDecSrcChan[i].sDecSrcBuf.u32Write);
    }
}

/*****************************************************************************
* �� �� ��      : SOCP_CanSleep
*
* ��������  : �͹��ĺ���
*
*
* �������  : BSP_OK  ���Խ���͹���״̬
*             ����ֵ�����ܽ���͹���״̬
*
* �� �� ֵ      : ��
*****************************************************************************/
u32 BSP_SOCP_CanSleep(void)
{
	u32 i;
	u32 u32EncBusy, u32DecBusy;
	u32 u32BufAddr, u32WritePtr, u32ReadPtr;
	u32 u32IntFlag;


	//�жϸ�ͨ���Ķ�дָ���Ƿ����
    //����ı���Դͨ����A��:0--3;C��: 4--8
    for(i = 0;i<SOCP_MAX_ENCSRC_CHN;i++)
    {
        /* C�˱���Դͨ�� */
        if((i >= SOCP_CCORE_ENCSRC_CHN_BASE) && (i <= (SOCP_CCORE_ENCSRC_CHN_BASE + SOCP_CCORE_ENCSRC_CHN_NUM)))
        {
            continue;
        }
        SOCP_REG_READ(SOCP_REG_ENCSRC_BUFADDR(i), u32BufAddr);
        /*lint -save -e774*/
        if(0!=u32BufAddr)
        /*lint -restore +e774*/
        {
            SOCP_REG_READ(SOCP_REG_ENCSRC_BUFWPTR(i), u32WritePtr);
            SOCP_REG_READ(SOCP_REG_ENCSRC_BUFRPTR(i), u32ReadPtr);
            /*lint -save -e774*/
            if(u32WritePtr != u32ReadPtr)
            /*lint -restore +e774*/
            {
                //BSP_TRACE(BSP_LOG_LEVEL_ERROR, BSP_MODU_SOCP, "SOCP_PwDown: enc src %d is busy: !\n", i);
                    return 1;
            }
        }
    }

    //����Ŀ��ͨ��0--6
    for(i = 0;i<SOCP_MAX_ENCDST_CHN;i++)
    {
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFADDR(i), u32BufAddr);
        /*lint -save -e774*/
        if(0!=u32BufAddr)
        /*lint -restore +e774*/
        {
            SOCP_REG_READ(SOCP_REG_ENCDEST_BUFWPTR(i), u32WritePtr);
            SOCP_REG_READ(SOCP_REG_ENCDEST_BUFRPTR(i), u32ReadPtr);
            /*lint -save -e774*/
            if(u32WritePtr != u32ReadPtr)
            /*lint -restore +e774*/
            {
                //BSP_TRACE(BSP_LOG_LEVEL_ERROR, BSP_MODU_SOCP, "SOCP_PwDown: enc dst %d is busy: !\n", i);
                    return 3;
            }
        }
    }
    //����Դͨ��0--3
    for(i = 0;i<SOCP_MAX_DECSRC_CHN;i++)
    {
        SOCP_REG_READ(SOCP_REG_DECSRC_BUFADDR(i), u32BufAddr);
        /*lint -save -e774*/
        if(0!=u32BufAddr)
        /*lint -restore +e774*/
        {
            SOCP_REG_READ(SOCP_REG_DECSRC_BUFWPTR(i), u32WritePtr);
            SOCP_REG_READ(SOCP_REG_DECSRC_BUFRPTR(i), u32ReadPtr);
            /*lint -save -e774*/
            if(u32WritePtr != u32ReadPtr)
            /*lint -restore +e774*/
            {
                //BSP_TRACE(BSP_LOG_LEVEL_ERROR, BSP_MODU_SOCP, "SOCP_PwDown: dec src %d is busy: !\n", i);
                return 4;
            }
        }
    }

    /* ����Ŀ��ͨ�� */
    for(i = 0;i<SOCP_MAX_DECDST_CHN;i++)
    {
        SOCP_REG_READ(SOCP_REG_DECDEST_BUFADDR(i), u32BufAddr);
        /*lint -save -e774*/
        if(0!=u32BufAddr)
        /*lint -restore +e774*/
        {
            SOCP_REG_READ(SOCP_REG_DECDEST_BUFWPTR(i), u32WritePtr);
            SOCP_REG_READ(SOCP_REG_DECDEST_BUFRPTR(i), u32ReadPtr);
            /*lint -save -e774*/
            if(u32WritePtr != u32ReadPtr)
            /*lint -restore +e774*/
            {
                //BSP_TRACE(BSP_LOG_LEVEL_ERROR, BSP_MODU_SOCP, "SOCP_PwDown: dec dst %d is busy: !\n", i);
                return 5;
            }
        }
    }

    //�ж�ͨ��æ��
    SOCP_REG_READ(SOCP_REG_ENCSTAT, u32EncBusy);
    SOCP_REG_READ(SOCP_REG_DECSTAT, u32DecBusy);

    /*lint -save -e774*/
    if((0 !=u32EncBusy) || (0 !=u32DecBusy) )
    /*lint -restore +e774*/
    {
        //BSP_TRACE(BSP_LOG_LEVEL_ERROR, BSP_MODU_SOCP, "SOCP_PwDown: chan is busy,enc is : 0x%x, dec is 0x%x!\n", u32EncBusy, u32DecBusy);
        return 6;
    }

    SOCP_REG_READ(SOCP_REG_GBL_INTSTAT, u32IntFlag);
    /*lint -save -e774*/
    if((0 != u32IntFlag ))
    /*lint -restore +e774*/
    {
        //BSP_TRACE(BSP_LOG_LEVEL_ERROR, BSP_MODU_SOCP, "SOCP_PwDown: chan is busy,enc is : 0x%x, dec is 0x%x!\n", u32EncBusy, u32DecBusy);
        return 7;
    }

    return BSP_OK;
}
/* �͹������ end */

/*****************************************************************************
* �� �� ��  : bsp_socp_get_state
*
* ��������  : ��ȡSOCP״̬
*
* �� �� ֵ  : SOCP_IDLE    ����
*             SOCP_BUSY    æµ
*****************************************************************************/
SOCP_STATE_ENUM_UINT32 bsp_socp_get_state(void)
{
    u32 reg_value;

#ifdef BSP_CONFIG_HI3630
    reg_value = readl((const volatile void *)(INTEGRATOR_SC_BASE + SOCP_STATE_SC_OFFSET));
#else
    reg_value = readl((u32)(INTEGRATOR_SC_BASE + SOCP_STATE_SC_OFFSET));
#endif
    /* P530BB bit0ָʾ״̬ 0 --- ������1 --- ���� */
    /*lint -save -e774*/
    if(1 == (reg_value & SOCP_STATE_SC_MASK))
    /*lint -restore +e774*/
    {
        return SOCP_IDLE;
    }

    return SOCP_BUSY;
}

/* log2.0 2014-03-19 Begin:*/
/*****************************************************************************
* �� �� ��  : bsp_socp_get_log_cfg
*
* ��������  : ��ȡLOG2.0 SOCPˮ�ߡ���ʱ������Ϣ
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : SOCP_ENC_DST_BUF_LOG_CFG_STRUָ��
*****************************************************************************/
SOCP_ENC_DST_BUF_LOG_CFG_STRU * bsp_socp_get_log_cfg(void)
{
    return &g_stEncDstBufLogConfig;
}

/*****************************************************************************
* �� �� ��  : BSP_SOCP_SetLogCfg
*
* ��������  : LOG2.0 SOCPˮ�ߡ���ʱ����
*
* �������  : SOCP_ENC_DST_BUF_LOG_CFG_STRU * cfg
*
* �������  : ��
*
* �� �� ֵ  : BSP_S32 BSP_OK:�ɹ� BSP_ERROR:ʧ�� 
*****************************************************************************/
s32 bsp_socp_set_log_cfg(SOCP_ENC_DST_BUF_LOG_CFG_STRU * cfg)
{
    SOCP_ENC_DST_BUF_LOG_CFG_STRU * logCfg;
    
    if(NULL == cfg)
    {
        return BSP_ERROR;
    }

    logCfg = bsp_socp_get_log_cfg();
    /* �������� */
    logCfg->guWaterMark = cfg->guWaterMark;
    logCfg->lWaterMark  = cfg->lWaterMark;
    logCfg->overTime    = cfg->overTime;
    logCfg->flushFlag   = cfg->flushFlag;
    logCfg->logOnFlag   = cfg->logOnFlag;

    return BSP_OK;
}

void set_flag(u32 flag)
{
    if(flag)
    {
        g_stEncDstBufLogConfig.logOnFlag = TRUE;
    }
    else
    {
        g_stEncDstBufLogConfig.logOnFlag = FALSE;
    }
}
/*****************************************************************************
* �� �� ��  : socp_is_encdst_chan_empty
*
* ��������  : SOCP����Ŀ��ͨ���Ƿ�������
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : u32 0:������ ��0:��Ӧͨ����λ
*****************************************************************************/
u32 socp_is_encdst_chan_empty(void)
{
    u32 chanSet = 0;
    u32 i;
    u32 u32ReadPtr;
    u32 u32WritePtr;

    /* �ж�Ŀ��ͨ����дָ���Ƿ���� */
    for(i = 0; i < SOCP_MAX_ENCDST_CHN; i++)
    {
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFWPTR(i), u32WritePtr);
        SOCP_REG_READ(SOCP_REG_ENCDEST_BUFRPTR(i), u32ReadPtr);
        if(u32WritePtr != u32ReadPtr)
        {
            chanSet = chanSet | (1 << i);           
        }
    }

    return chanSet;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_enc_dst_buf_flush
*
* ��������  : SOCP����Ŀ��buffer���������������
*
* �������  : ��
*
* �������  : ��
*
* �� �� ֵ  : BSP_S32 BSP_OK:�ɹ� BSP_ERROR:ʧ�� 
*****************************************************************************/
s32 bsp_socp_enc_dst_buf_flush()
{
    SOCP_ENC_DST_BUF_LOG_CFG_STRU * cfg;
    unsigned int tick;

#if(FEATURE_SOCP_ON_DEMAND == FEATURE_ON)
    if(g_ulSocpPowerState == SOCP_POWER_OFF)
    {
        return BSP_ERROR;
    }
#endif

    /* ��ȡ���� */
    cfg = bsp_socp_get_log_cfg();

    /* ��һ��FLUSH������δ��ɻ���LOG2.0δ���� */
    if(TRUE == cfg->flushFlag || FALSE == cfg->logOnFlag)
    {
        return BSP_ERROR;
    }
    cfg->flushFlag = TRUE;
    /* ����Ŀ��������SOCP���� */
    up(&g_strSocpStat.u32EncDstSemID);

    tick = om_timer_tick_get();
    while(FALSE != cfg->flushFlag)
    {
        msleep(10);
        if(om_timer_tick_get() - tick > SOCP_LOG_FLUSH_MAX_OVER_TIME)
        {
            printk("SOCP enc dst channel flush time out\n");
            return BSP_ERROR;
        }
    }

    return BSP_OK;
}
/* log2.0 2014-03-19 End*/

#if(FEATURE_SOCP_ON_DEMAND == FEATURE_ON)
/*****************************************************************************
* �� �� ��  : socp_show_vote_info
* ��������  : ��ʾͶƱ״̬
* �������  : ��
* �������  : ��
* �� �� ֵ  : ��
*****************************************************************************/
void socp_show_vote_info(void)
{
    u8 * power_flag;
    
    printk("************socp vote info************\n");
    printk("GU OM APP   : %-5s  0x%x\n", (g_stVoteInfo[0].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[0].vote_time);
    printk("GU OM COMM  : %-5s  0x%x\n", (g_stVoteInfo[1].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[1].vote_time);
    printk("GU DSP      : %-5s  0x%x\n", (g_stVoteInfo[2].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[2].vote_time);    
    printk("DIAG APP    : %-5s  0x%x\n", (g_stVoteInfo[3].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[3].vote_time);
    printk("DIAG COMM   : %-5s  0x%x\n", (g_stVoteInfo[4].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[4].vote_time);
#if (FEATURE_OFF == FEATURE_MERGE_OM_CHAN)
    printk("DIAG DEC    : %-5s  0x%x\n", (g_stVoteInfo[5].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[5].vote_time);
#else
    printk("DIAG FW     : %-5s  0x%x\n", (g_stVoteInfo[5].vote_type == 0) ? "SLEEP" : "WAKE", g_stVoteInfo[5].vote_time);
#endif
    printk("************socp flag info************\n");
    printk("SOCP ON DEMAND FEATUR: %s\n", (g_ulSocpOnDemand == BSP_TRUE) ? "ON" : "OFF");
    switch(g_ulSocpPowerState)
    {
        case SOCP_POWER_ON:
            power_flag = "POWER ON";
            break;

        case SOCP_POWER_OFF_REQ:
            power_flag = "POWER OFF REQ";
            break;

        case SOCP_POWER_OFF:
            power_flag = "POWER OFF";
            break;

        default:
            power_flag = "UNKNOWN";
            break;
    }
    printk("SOCP POWER ON STATE  : %s\n", power_flag);    
}

/*****************************************************************************
* �� �� ��  : socp_can_sleep
* ��������  : SOCP�Ƿ���˯�ߣ�����������Ͷ�޳�Ʊ, ͨ�����У�������˯��
* �������  : ��
* �������  : ��
* �� �� ֵ  : BSP_S32 0 --- ����˯�ߣ�0xFFFFFFFF --- ������˯��
*****************************************************************************/
s32 socp_can_sleep(void)
{
    u32 i;
   
    /* �ж��Ƿ�����������Ѿ�ͶƱ */
    for(i=0; i<SOCP_VOTE_ID_BUTT; i++)
    {
        if(SOCP_VOTE_FOR_WAKE == g_stVoteInfo[i].vote_type)
        {
            return BSP_ERROR;
        }
    }

    /* ͨ���Ƿ���� */
    if(BSP_OK != socp_chan_is_idle())
    {
        return BSP_ERROR;
    }
    
    return BSP_OK;
}

/*****************************************************************************
* �� �� ��  : bsp_socp_vote
* ��������  : SOCPͶƱ�ӿڣ�����ͶƱ�������SOCP�Ƿ�˯�ߣ��ýӿ�ֻ��A���ṩ
* �������  : id --- ͶƱ���ID��type --- ͶƱ����
* �������  : ��
* �� �� ֵ  : BSP_S32 0 --- ͶƱ�ɹ���0xFFFFFFFF --- ͶƱʧ��
*****************************************************************************/
BSP_S32 bsp_socp_vote(SOCP_VOTE_ID_ENUM_U32 id, SOCP_VOTE_TYPE_ENUM_U32 type)
{
    if(id >= SOCP_VOTE_ID_BUTT || type >= SOCP_VOTE_TYPE_BUTT)
    {
        printk("%s invalid param id 0x%x type 0x%x\n", __FUNCTION__, id, type);
        return BSP_ERROR;
    }

    g_stVoteInfo[id].vote_type = type;
    g_stVoteInfo[id].vote_time = om_timer_get();

    printk("%s: id 0x%x vote 0x%x\n", __FUNCTION__, id, type);
    socp_show_vote_info();

    if(g_ulSocpOnDemand == BSP_FALSE)
    {
        printk("%s: socp on demand feature off\n", __FUNCTION__);
        return BSP_OK;
    }
    
    /* ����Ʊ����SOCP�ϵ� */
    if(type == SOCP_VOTE_FOR_WAKE)
    {
        /*����regulator�ӿ��ϵ�*/
        if(0 != down_interruptible(&g_stSocpPowerSem))
        {
            printk("%s:socp vote wake interruptible\n", __FUNCTION__);
        }
        
        if(g_ulSocpPowerState == SOCP_POWER_ON)
        {
            up(&g_stSocpPowerSem);
            return BSP_OK;
        }

        /* read done����δ��ɣ�����Ϊ�ϵ�״̬ */
        if(g_ulSocpPowerState == SOCP_POWER_OFF_REQ)
        {
            g_ulSocpPowerState = SOCP_POWER_ON;
            up(&g_stSocpPowerSem);
            return BSP_OK;
        }
        
        if(regulator_bulk_enable(1, &(g_stSocpVcc->socp_vcc)))
        {
            printk("[%s]:fetal error, regulator enable fail\n", __FUNCTION__);
            up(&g_stSocpPowerSem);
            return BSP_ERROR;
        }
        else
        {
            g_ulSocpPowerState = SOCP_POWER_ON;
            printk("%s: socp goto power on\n", __FUNCTION__);
        }

        /* �ϵ�ɹ�������ж����� */
        SOCP_REG_WRITE(SOCP_REG_ENC_MASK0, g_ul_encdst_tran_int_mask);
        SOCP_REG_WRITE(SOCP_REG_ENC_MASK2, g_ul_encdst_over_int_mask);
        SOCP_REG_WRITE(SOCP_REG_DEC_CORE0MASK0, g_ul_decdst_tran_int_mask);

        up(&g_stSocpPowerSem);
        return BSP_OK;
    }

    /* �޳�Ʊ���ж��Ƿ��ܹ�˯�� */
    if(0 != down_interruptible(&g_stSocpPowerSem))
    {
        printk("%s:socp vote sleep interruptible\n", __FUNCTION__);
    }
    
    if(g_ulSocpPowerState == SOCP_POWER_OFF)
    {
        up(&g_stSocpPowerSem);
        return BSP_OK;
    } 

    if(BSP_OK == socp_can_sleep())
    {
        /* �µ�ǰ�������ж����μĴ��� */
        SOCP_REG_READ(SOCP_REG_ENC_MASK0, g_ul_encdst_tran_int_mask);
        SOCP_REG_READ(SOCP_REG_ENC_MASK2, g_ul_encdst_over_int_mask);
        SOCP_REG_READ(SOCP_REG_DEC_CORE0MASK0, g_ul_decdst_tran_int_mask);
        /* �����ж� */
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK0, 0, 7, 0x7f);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 0, 7, 0x7f);
        SOCP_REG_SETBITS(SOCP_REG_ENC_MASK2, 16, 7, 0x7f);
        SOCP_REG_SETBITS(SOCP_REG_DEC_CORE0MASK0, 0, 16, 0xffff);
        /* regulator disalbe�ӿ���Ҫ��enableƥ�䣬���û�õ���enable��������쳣 */
        if(regulator_bulk_disable(1, &(g_stSocpVcc->socp_vcc)))
        {
            printk("[%s]:fetal error, regulator disable fail\n", __FUNCTION__);
        }
        else
        {
            g_ulSocpPowerState = SOCP_POWER_OFF;
            printk("%s: socp goto power off\n", __FUNCTION__);
        }
    }
    up(&g_stSocpPowerSem);

    return BSP_OK;
}

static int socp_probe(struct platform_device *pdev)
{  
	struct device *dev = &pdev->dev;
	int ret = 0;
    SOCP_ON_DEMAND_STRU socp_config;

    /* LPM3�ȱ�֤SOCP�����ϵ�״̬ */
    g_ulSocpPowerState = BSP_TRUE;

	g_stSocpVcc = kzalloc(sizeof(*g_stSocpVcc), GFP_KERNEL);
	if (!g_stSocpVcc) {
		dev_err(dev, "cannot allocate modem_socp device info\n");
		ret = -ENOMEM;
		return ret;
	}

	g_stSocpVcc->dev = &(pdev->dev);
	platform_set_drvdata(pdev, g_stSocpVcc);
    g_stSocpVcc->socp_vcc.supply = "socp";

    sema_init(&g_stSocpPowerSem, 1);
    /* ��ȡNV���� */
    if(BSP_OK != bsp_nvm_read(NV_ID_DRV_SOCP_ON_DEMAND, (u8*)&socp_config, sizeof(SOCP_ON_DEMAND_STRU)))
    {
        printk("%s:read nv 0x%x fail\n", __FUNCTION__, NV_ID_DRV_SOCP_ON_DEMAND);
        g_ulSocpOnDemand = BSP_FALSE;
    }
    else
    {
        if(socp_config.en_flag == 1)
        {
            g_ulSocpOnDemand = BSP_TRUE;
        }
        else
        {
            g_ulSocpOnDemand = BSP_FALSE;
        }   
    }

    ret = devm_regulator_bulk_get(g_stSocpVcc->dev, 1, &(g_stSocpVcc->socp_vcc));
    if(ret)
    {
        printk("%s: fetal error, get regulator fail\n", __FUNCTION__);
        return ret;
    }

    if(regulator_bulk_enable(1, &(g_stSocpVcc->socp_vcc)))
    {
        printk("[%s]:fetal error, regulator enable fail\n", __FUNCTION__);
        return BSP_ERROR;        
    }   

    g_ulSocpPowerState = SOCP_POWER_ON;

    (void)socp_init();   

	return BSP_OK;
}

static const struct of_device_id socp_dev_of_match[] = {
        {.compatible = "hisilicon,modem_socp"},
	{ },
};

static struct platform_driver socp_driver = {
        .driver = {
                   .name = "modem_socp",
                   .owner = THIS_MODULE,
                   .of_match_table = socp_dev_of_match,
        },
        .probe = socp_probe,
};

static int __init socp_drvier_init(void)
{
	return platform_driver_register(&socp_driver);
}


/*lint -save -e19*/
module_init(socp_drvier_init);
#else
module_init(socp_init);
#endif
/*lint -restore*/

/*****************************************************************************
* �� �� ��  : bsp_socp_set_decode_timeout_register
*
* ��������  :������жϳ�ʱ���üĴ���ѡ��
                            1 - ����ͨ������INT_TIMEOUT(0x024)������ͨ������DEC_INT_TIMEOUT(0x20);
                            0 - �����ͨ��������INT_TIMEOUT(0x024)

*
* �� �� ֵ  :  ��
* 
*            
*****************************************************************************/
#if (FEATURE_SOCP_DECODE_INT_TIMEOUT == FEATURE_ON)
s32 bsp_socp_set_decode_timeout_register(DECODE_TIMEOUT_MODULE module)
{
    if(module > DECODE_TIMEOUT_DEC_INT_TIMEOUT)
    {
        return BSP_ERR_SOCP_INVALID_PARA;
    }
    SOCP_REG_SETBITS(SOCP_REG_GBLRST, 1, 1, module);

    return BSP_OK;
    
}
#endif 


#ifdef __cplusplus
}
#endif

