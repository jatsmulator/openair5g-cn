/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the Apache License, Version 2.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file nas_mme_task.c
   \brief
   \author  Sebastien ROUX, Lionel GAUTHIER
   \date
   \email: lionel.gauthier@eurecom.fr
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "bstrlib.h"

#include "log.h"
#include "msc.h"
#include "assertions.h"
#include "common_defs.h"
#include "intertask_interface.h"
#include "itti_free_defined_msg.h"
#include "mme_config.h"
#include "nas_defs.h"
#include "nas_network.h"
#include "nas_proc.h"
#include "emm_main.h"
#include "nas_timer.h"
#include "nas_proc.h"
#include "esm_sap.h"
#include "emm_reg.h"
#include "emm_proc.h"
#include "esm_data.h"
#include "esm_proc.h"
#include "esm_cause.h"
#include "esm_ebr.h"
#include "esm_ebr_context.h"
#include "emm_sap.h"
#include "esm_send.h"
#include "esm_information.c"
static void nas_exit(void);

//------------------------------------------------------------------------------
static void *nas_intertask_interface (void *args_p)
{
  itti_mark_task_ready (TASK_NAS_MME);
  emm_context_t *emm_context;
  while (1) {
    MessageDef                             *received_message_p = NULL;

    itti_receive_msg (TASK_NAS_MME, &received_message_p);

    switch (ITTI_MSG_ID (received_message_p)) {
    case EMM_REG_MSG:{
        emm_reg_send((emm_reg_t *)&(EMM_REG_DATA_IND(received_message_p)));
       }
       break;
    case NAS_EMMAS_ESTABLISH_REJ:{ //not tested yet
	nas_emm_attach_reject( & NAS_EMMAS_ESTABLISH_REJ(received_message_p ));
	}
	break;
    case MESSAGE_TEST:{
        OAI_FPRINTF_INFO("TASK_NAS_MME received MESSAGE_TEST\n");
      }
      break;

    case MME_APP_CREATE_DEDICATED_BEARER_REQ:
      nas_proc_create_dedicated_bearer(&MME_APP_CREATE_DEDICATED_BEARER_REQ (received_message_p));
      break;

    case NAS_DOWNLINK_DATA_CNF:{
        nas_proc_dl_transfer_cnf (NAS_DL_DATA_CNF (received_message_p).ue_id, NAS_DL_DATA_CNF (received_message_p).err_code, &NAS_DL_DATA_REJ (received_message_p).nas_msg);
      }
      break;

    case NAS_DOWNLINK_DATA_REJ:{
        nas_proc_dl_transfer_rej (NAS_DL_DATA_REJ (received_message_p).ue_id, NAS_DL_DATA_REJ (received_message_p).err_code, &NAS_DL_DATA_REJ (received_message_p).nas_msg);
      }
      break;

    case NAS_PDN_CONFIG_RSP:{
      nas_proc_pdn_config_res (&NAS_PDN_CONFIG_RSP (received_message_p));
    }
    break;

    case NAS_PDN_CONNECTIVITY_FAIL:{
        nas_proc_pdn_connectivity_fail (&NAS_PDN_CONNECTIVITY_FAIL (received_message_p));
      }
      break;

    case NAS_PDN_CONNECTIVITY_RSP:{
        nas_proc_pdn_connectivity_res (&NAS_PDN_CONNECTIVITY_RSP (received_message_p));
      }
      break;

    case NAS_IMPLICIT_DETACH_UE_IND:{
        nas_proc_implicit_detach_ue_ind (NAS_IMPLICIT_DETACH_UE_IND (received_message_p).ue_id);
      }
      break;

    case S1AP_DEREGISTER_UE_REQ:{
        nas_proc_deregister_ue (S1AP_DEREGISTER_UE_REQ (received_message_p).mme_ue_s1ap_id);
      }
      break;

    case S6A_AUTH_INFO_ANS:{
        /*
         * We received the authentication vectors from HSS, trigger a ULR
         * for now. Normaly should trigger an authentication procedure with UE.
         */
        nas_proc_authentication_info_answer (&S6A_AUTH_INFO_ANS(received_message_p));
      }
      break;

    case TERMINATE_MESSAGE:{
        nas_exit();
        OAI_FPRINTF_INFO("TASK_NAS_MME terminated\n");
        itti_free_msg_content(received_message_p);
        itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
        itti_exit_task ();
      }
      break;

    case TIMER_HAS_EXPIRED:{
        /*
         * Call the NAS timer api
         */
        nas_timer_handle_signal_expiry (TIMER_HAS_EXPIRED (received_message_p).timer_id, TIMER_HAS_EXPIRED (received_message_p).arg);
      }
      break;

    default:{
        OAILOG_DEBUG (LOG_NAS, "Unkwnon message ID %d:%s from %s\n", ITTI_MSG_ID (received_message_p), ITTI_MSG_NAME (received_message_p), ITTI_MSG_ORIGIN_NAME (received_message_p));
      }
      break;
    }

    itti_free_msg_content(received_message_p);
    itti_free (ITTI_MSG_ORIGIN_ID (received_message_p), received_message_p);
    received_message_p = NULL;

  }

  return NULL;
}

//这个函数是自己加的
static void *guti_msg_process (void *args_p)
{
    itti_mark_task_ready (TASK_GUTI_RECEIVER);

    while (1) {
        MessageDef                             *message_p = NULL;

        itti_receive_msg (TASK_GUTI_RECEIVER, &message_p);

        switch (ITTI_MSG_ID (message_p)) {
            case GUTI_MSG_TEST:
                ;
                int task=GUTI_DATA_IND(message_p).task;
                printf("Receive GUTI_MSG_TEST:%d\n",task);

                struct esm_context_s * esm_p;
                esm_get_inplace(GUTI_DATA_IND(message_p)._guti,&esm_p);

                printf("IN ITTI: ESM_GET_INPLACE OK:%p\n",esm_p);
                struct esm_context_s esm_t;
                guti_t _guti=GUTI_DATA_IND(message_p)._guti;
                if(esm_p)
                {
                    const_bstring  apn=GUTI_DATA_IND(message_p).apn;
                    protocol_configuration_options_t *  pco=GUTI_DATA_IND(message_p).pco;
                    esm_ebr_timer_data_t * data = GUTI_DATA_IND(message_p).data;



                    proc_tid_t pti=GUTI_DATA_IND(message_p).pti;
                    esm_cause_t *esm_cause=GUTI_DATA_IND(message_p).esm_cause;
                    emm_context_t* emm_context=GUTI_DATA_IND(message_p).emm_context;

                    switch(task)
                    {
                        case 0:
                            if (esm_p->esm_proc_data->apn) {
                                bdestroy_wrapper(&esm_p->esm_proc_data->apn);
                            }
                            esm_p->esm_proc_data->apn = bstrcpy(apn);
                            break;

                        case 1:
                            if (esm_p->esm_proc_data->pco.num_protocol_or_container_id) {
                                clear_protocol_configuration_options(&esm_p->esm_proc_data->pco);
                            }
                            copy_protocol_configuration_options(&esm_p->esm_proc_data->pco,pco);
                            break;
                        case 2:
                            nas_stop_T3489(esm_p);
                            break;
                        case 3:
                            esm_p->T3489.id = NAS_TIMER_INACTIVE_ID;
                            break;
                        case 4:
                            esm_p->T3489.id = nas_timer_start (esm_p->T3489.sec, 0 /*usec*/,_esm_information_t3489_handler, data);
                            break;
                        case 5:
                            esm_p->n_pdns+=1;
                            break;
                        case 6:
                            esm_init_context(&esm_t);
                            esm_insert(_guti,esm_t);
                            break;
                        case 7:
                            if (esm_p->n_active_pdns > 1) {
                                MessageDef *message_p = itti_alloc_new_message(TASK_RTN_SENDER,GUTI_RTN_TEST);
                                if (message_p) {
                                    RTN_DATA_IND(message_p).task=0;
                                    RTN_DATA_IND(message_p).pti=pti;
                                    RTN_DATA_IND(message_p).esm_cause=esm_cause;
                                    RTN_DATA_IND(message_p).emm_context=emm_context;
                                    int send_res = itti_send_msg_to_task(TASK_RTN_RECEIVER, INSTANCE_DEFAULT, message_p);
                                }

                            } else {
                                MessageDef *message_p = itti_alloc_new_message(TASK_RTN_SENDER,GUTI_RTN_TEST);
                                if (message_p) {
                                    RTN_DATA_IND(message_p).task=1;
                                    RTN_DATA_IND(message_p).pti=pti;
                                    RTN_DATA_IND(message_p).esm_cause=esm_cause;
                                    RTN_DATA_IND(message_p).emm_context=emm_context;
                                    int send_res = itti_send_msg_to_task(TASK_RTN_RECEIVER, INSTANCE_DEFAULT, message_p);
                                }
                            }
                    }
                }else
                {
                    if(task==6)
                    {
                        struct esm_context_s esm_t;
                        esm_init_context(&esm_t);
                        esm_insert(_guti,esm_t);
                        break;

                    }
                }

                break;


        }
        itti_free_msg_content(message_p);
        itti_free (ITTI_MSG_ORIGIN_ID (message_p), message_p);
        message_p = NULL;
    }

    return NULL;

}

static void *guti_rtn_process (void *args_p)
{
    itti_mark_task_ready (TASK_RTN_RECEIVER);

    while (1) {
        MessageDef                             *message_p = NULL;

        itti_receive_msg (TASK_RTN_RECEIVER, &message_p);


        switch (ITTI_MSG_ID (message_p)) {
            case GUTI_RTN_TEST:
                printf("收到RECEIVED GUTI_RTN_TEST:%d\n",RTN_DATA_IND(message_p).task);
                // bian liang fu zhi
                //
                int task=RTN_DATA_IND(message_p).task;
                proc_tid_t pti=RTN_DATA_IND(message_p).task;
                esm_cause_t *esm_cause=RTN_DATA_IND(message_p).task;
                emm_context_t * emm_context=RTN_DATA_IND(message_p).task;
                pdn_cid_t pid = RETURNerror;
                printf("Begin task\n");
                switch (task)
                {
                    case 0:

                        ;

                            if (pid >= MAX_APN_PER_UE) {
                                OAILOG_ERROR (LOG_NAS_ESM, "ESM-PROC  - No PDN connection found (pti=%d)\n", pti);
                                *esm_cause = ESM_CAUSE_PROTOCOL_ERROR;
                                OAILOG_FUNC_RETURN (LOG_NAS_ESM, RETURNerror);
                            }
                        OAILOG_FUNC_RETURN (LOG_NAS_ESM, pid);
                        break;
                    case 1:
                        ;
                        if(esm_cause)
                        *esm_cause = ESM_CAUSE_LAST_PDN_DISCONNECTION_NOT_ALLOWED;
                        OAILOG_FUNC_RETURN (LOG_NAS_ESM, pid);
                        break;


                }
                printf("TASK end\n");
                break;

        }
        itti_free_msg_content(message_p);
        itti_free (ITTI_MSG_ORIGIN_ID (message_p), message_p);
        message_p = NULL;
    }
    return NULL;

}

//------------------------------------------------------------------------------
int nas_init (mme_config_t * mme_config_p)
{
  OAILOG_DEBUG (LOG_NAS, "Initializing NAS task interface\n");
  nas_network_initialize (mme_config_p);

  if (itti_create_task (TASK_NAS_MME, &nas_intertask_interface, NULL) < 0) {
    OAILOG_ERROR (LOG_NAS, "Create task failed");
    OAILOG_DEBUG (LOG_NAS, "Initializing NAS task interface: FAILED\n");
    return -1;
  }
  if (itti_create_task (TASK_ESM_SAP, &esm_sap_message_process, NULL) < 0) {
    OAILOG_ERROR (LOG_NAS, "TASK ESM SAP create task failed\n");
    OAILOG_FUNC_RETURN (LOG_NAS, RETURNerror);
    }
  if (itti_create_task (TASK_GUTI_RECEIVER, &guti_msg_process, NULL) < 0) {
    OAILOG_ERROR (LOG_NAS, "guti_receiver esm sap create task failed\n");
    OAILOG_FUNC_RETURN (LOG_NAS, RETURNerror);
    }
  if (itti_create_task (TASK_RTN_RECEIVER, &guti_rtn_process, NULL) < 0) {
    OAILOG_ERROR (LOG_NAS, "guti_rtn_receiver esm sap create task failed\n");
    OAILOG_FUNC_RETURN (LOG_NAS, RETURNerror);
    }

  if(0)
  {
      MessageDef *message_p = itti_alloc_new_message(TASK_RTN_SENDER,GUTI_RTN_TEST);
      if (message_p) {
          RTN_DATA_IND(message_p).task=1;
          int send_res = itti_send_msg_to_task(TASK_RTN_RECEIVER, INSTANCE_DEFAULT, message_p);
      }
  }
  if(0)
  {
      MessageDef *message_p = itti_alloc_new_message(TASK_GUTI_SENDER,GUTI_MSG_TEST);
      if (message_p) {
          GUTI_DATA_IND(message_p)._guti;
          int send_res = itti_send_msg_to_task(TASK_GUTI_RECEIVER, INSTANCE_DEFAULT, message_p);
          //
      }
  }


  OAILOG_DEBUG (LOG_NAS, "Initializing NAS task interface: DONE\n");
  OAILOG_DEBUG (LOG_NAS, "Initializing ESM_SAP task interface: DONE\n");
  OAILOG_DEBUG (LOG_NAS, "Initializing GUIT_MSG task interface: DONE\n");
  return 0;
}

//------------------------------------------------------------------------------
static void nas_exit(void)
{
  OAILOG_DEBUG (LOG_NAS, "Cleaning NAS task interface\n");
  nas_network_cleanup();
  OAILOG_DEBUG (LOG_NAS, "Cleaning NAS task interface: DONE\n");
}
