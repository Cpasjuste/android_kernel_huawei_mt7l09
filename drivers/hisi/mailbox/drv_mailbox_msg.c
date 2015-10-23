/******************************************************************************

			  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

******************************************************************************
�� �� ��   : drv_mailbox_ifc.c
�� �� ��   : ����
��	  ��   : Ī�� 00176101
��������   : 2013��4��11��
����޸�   :
��������   : mailbox&�����������������˼����������롣
�����б�   :

�޸���ʷ   :
1.��	��	 : 2013��4��11��
��	  ��   : Ī�� 00176101
�޸�����   : �����ļ�
******************************************************************************/

/*****************************************************************************
1 ͷ�ļ�����
*****************************************************************************/
#include "drv_mailbox_cfg.h"
#include "drv_mailbox_debug.h"
#include "drv_mailbox_gut.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
	��ά�ɲ���Ϣ�а�����C�ļ���ź궨��
*****************************************************************************/
#undef	_MAILBOX_FILE_
#define _MAILBOX_FILE_	 "msg"
/*****************************************************************************
  2 ȫ�ֱ�������
*****************************************************************************/

#ifdef _HIFI_WD_DEBUG
extern int BSP_CPU_StateGet(int CpuID);
#else
int BSP_CPU_StateGet(int CpuID)
{
	return 1;
}
#endif
/*****************************************************************************
 �� �� ��  : MAILBOX_RegisterRecvFuncByHandle
 ��������  : �˼��ʼ����ݽ��ջص�������ע��
 �������  : struct mailbox 			  *pMailBox    -- ĳ���˵������ܾ����
			 unsigned long			  MailId	  -- ���յ����ʼ���id�š�
													�뷢�ͷ����ʼ�����Ŀ��ID��һ�¡�
			 mb_cb_func 			 pFun		  -- �ʼ����ݽ��մ���ص�����
 �������  : ��
 �� �� ֵ  : MAILBOX_OK, �쳣����ֵ
 ���ú���  :
 ��������  :

 �޸���ʷ	   :
  1.��	  ��   : 2012��9��29��
	��	  ��   : Ī�� 00176101
	�޸�����   : �����ɺ���

*****************************************************************************/

void mailbox_msg_receiver(void *mb_buf, void *handle, void *data)
{
	struct mb_queue * queue;  /*����buffer��ʱ��������ڴ����û��ص�*/
	struct mb_buff	* mbuf = ( struct mb_buff  *)mb_buf;
	mb_msg_cb  func = (mb_msg_cb)handle;

	queue = &mbuf->usr_queue;
	if (func) {
		func(data, (void *)queue, queue->size);
	} else {
		mailbox_logerro_p1(MAILBOX_ERR_GUT_READ_CALLBACK_NOT_FIND, mbuf->mailcode);
	}
}

MAILBOX_EXTERN unsigned long mailbox_reg_msg_cb(
	unsigned long			  mailcode,
	mb_msg_cb				  func,
	void					 *data)
{
	return (unsigned long)mailbox_register_cb(mailcode, mailbox_msg_receiver, func, data);
}

/*****************************************************************************
 �� �� ��  : mailbox_try_send_msg
 ��������  : ����һ���ʼ����Ͷ���
 �������  :
			 struct mailbox 	  *pMailBox 	   -- ĳ���˵������ܾ����
			 unsigned long		 MailTarget 	 -- �ʼ���id��, ָ���ʼ�����Ŀ�ꡣ
			 unsigned char		*pData			 -- ָ���ʼ����ݵ�ָ�롣
			 unsigned long		 Length/byte	 -- ���ݻ�����Ϣ�ܳ��� �ֽ�Ϊ��λ��
 �������  : ��
 �� �� ֵ  : unsigned long
			 �ɹ�OK, �쳣����ֵ
 ���ú���  :
 ��������  :

 �޸���ʷ	   :
  1.��	  ��   : 2012��9��28��
	��	  ��   : Ī�� 00176101
	�޸�����   : �����ɺ���

*****************************************************************************/
MAILBOX_EXTERN unsigned long mailbox_try_send_msg(
	unsigned long			 mailcode,
	void					*pdata,
	unsigned long			 length)
{
	struct mb_buff		*mb_buf = MAILBOX_NULL;
	struct mb_queue 	*queue	= MAILBOX_NULL;
	unsigned long		 ret_val	= MAILBOX_OK;

	if ((0 == pdata) || (0 == length)) {
		return (unsigned long)mailbox_logerro_p1(MAILBOX_ERRO, mailcode);
	}
	/*��ȡ����buffer*/
	ret_val = mailbox_request_buff(mailcode, &mb_buf);
	if (MAILBOX_OK != ret_val) {
		goto exit_out;
	}

	/*����û�����*/
	queue = &mb_buf->usr_queue;
	if ( length != mailbox_write_buff( queue, pdata, length)) {
		ret_val = mailbox_logerro_p1(MAILBOX_FULL, mailcode);
		goto exit_out;
	}

	/*����*/
	ret_val = mailbox_sealup_buff( mb_buf,	length);
	if (MAILBOX_OK == ret_val) {
		/*�����ʼ�*/
		ret_val = mailbox_send_buff(mb_buf);
	}

exit_out:
	/*�ͷ�����buffer*/
	if (MAILBOX_NULL != mb_buf) {
		mailbox_release_buff(mb_buf);
	}

	return ret_val;
}


/****************************************************************************
 �� �� ��  : mailbox_read_msg_data
 ��������  : �����û������ݽ��ջص������е���, �������ж�ȡһ�����ȵ�����ʼ�
 �������  : MailHandle -- ������, ���ݽ��ջص��������
			 pData		-- ������������ݵĻ����ַ
			 *pSize 	-- ���泤��, ��λbyte, ��Ҫ���ڴ���ȡ�ʼ�����
 �������  : *pSize 	-- ʵ�ʶ�ȡ����, ��λbyte
 �� �� ֵ  : MAILBOX_OK / MAILBOX_ERRO
 ���ú���  :
 ��������  :

 �޸���ʷ	   :
  1.��	  ��   : 2012��10��26��
	��	  ��   : Ī�� 00176101
	�޸�����   : �����ɺ���

*****************************************************************************/
MAILBOX_GLOBAL unsigned long mailbox_read_msg_data(
	void				   *mail_handle,
	char				   *buff,
	unsigned long		   *size)
{
	struct mb_queue *pMailQueue = (struct mb_queue *)mail_handle;

	if ((MAILBOX_NULL == pMailQueue) || (MAILBOX_NULL == buff) || (MAILBOX_NULL == size)) {
		return (unsigned long)mailbox_logerro_p1(MAILBOX_ERR_GUT_INPUT_PARAMETER, 0);
	}

	if (pMailQueue->size  >  *size) {
		return (unsigned long)mailbox_logerro_p1(MAILBOX_ERR_GUT_USER_BUFFER_SIZE_TOO_SMALL, *size);
	}

	/*����û����ص��������ݶ��о������Ч��*/
	if ((0 == pMailQueue->length) ||
		((pMailQueue->front - pMailQueue->base) >  pMailQueue->length ) ||
		((pMailQueue->rear - pMailQueue->base) >  pMailQueue->length )) {
		return (unsigned long)mailbox_logerro_p1(MAILBOX_CRIT_GUT_INVALID_USER_MAIL_HANDLE, pMailQueue);
	}

	*size =  mailbox_read_buff(pMailQueue, buff, pMailQueue->size);

	return MAILBOX_OK;
}

/*****************************************************************************
 �� �� ��  : mailbox_send_msg
 ��������  : �ṩ����ģ����к˼����ݷ��͵Ľӿڡ�
 �������  :
			 struct mailbox 	  *pMailBox 	   -- ĳ���˵������ܾ����
			 unsigned long		 MailTarget 	 -- �ʼ���id��, ָ���ʼ�����Ŀ�ꡣ
			 unsigned char		*pData			 -- ָ���ʼ����ݵ�ָ�롣
			 unsigned long		 Length/byte	 -- ���ݻ�����Ϣ�ܳ��� �ֽ�Ϊ��λ��
 �������  : ��
 �� �� ֵ  : unsigned long
			 �ɹ�OK, �쳣����ֵ
 ���ú���  :
 ��������  :

 �޸���ʷ	   :
  1.��	  ��   : 2012��9��28��
	��	  ��   : Ī�� 00176101
	�޸�����   : �����ɺ���

*****************************************************************************/
MAILBOX_EXTERN unsigned long mailbox_send_msg(
	unsigned long			 mailcode,
	void					*data,
	unsigned long			 length)
{
	unsigned long  ret_val;
	unsigned long  try_go_on = MAILBOX_TRUE;
	unsigned long  try_times = 0;

	extern bool hifi_is_loaded(void);
	if (!hifi_is_loaded()) {
		return MAILBOX_TARGET_NOT_READY;
	}

	ret_val= BSP_CPU_StateGet(mailbox_get_dst_id(mailcode));
	if(!ret_val) {
		return MAILBOX_TARGET_NOT_READY;
	}
	ret_val = mailbox_try_send_msg(mailcode, data, length);

	if (MAILBOX_FALSE == mailbox_int_context()) {
		/*�������ȴ���ѯ����*/
		while (MAILBOX_FULL == ret_val) {
			mailbox_delivery(mailbox_get_channel_id(mailcode));
			try_go_on = mailbox_scene_delay(MAILBOX_DELAY_SCENE_MSG_FULL, &try_times);

			if (MAILBOX_TRUE == try_go_on) {
				ret_val = mailbox_try_send_msg(mailcode, data, length);
			} else {
				break;
			}
		}
	}

	if (MAILBOX_OK != ret_val) {
		mailbox_show(mailcode,0);
		mailbox_assert(ret_val);
		if (MAILBOX_FULL != ret_val) {
			ret_val = MAILBOX_ERRO;
		}
		return ret_val;
	}

	return ret_val;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

