/* mqtt_client.c
 *
 * Copyright (C) 2006-2020 wolfSSL Inc.
 *
 * This file is part of wolfMQTT.
 *
 * wolfMQTT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfMQTT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Include the autoconf generated config.h */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "wolfmqtt/mqtt_client.h"

/* Options */
//#define WOLFMQTT_DEBUG_CLIENT
#ifdef WOLFMQTT_NO_STDIO
#undef WOLFMQTT_DEBUG_CLIENT
#endif

/* Private functions */

/* forward declarations */
static int MqttClient_Publish_ReadPayload(MqttClient *client,
        MqttPublish *publish, int timeout_ms);

#ifdef WOLFMQTT_MULTITHREAD

#ifdef __MACH__
/* Apple style dispatch semaphore */
int wm_SemInit(wm_Sem *s)
{
    *s = dispatch_semaphore_create(1);
    return 0;
}
int wm_SemFree(wm_Sem *s)
{
    /* no free */
    (void)s;
    return 0;
}
int wm_SemLock(wm_Sem *s)
{
    dispatch_semaphore_wait(*s, DISPATCH_TIME_FOREVER);
    return 0;
}
int wm_SemUnlock(wm_Sem *s)
{
    dispatch_semaphore_signal(*s);
    return 0;
}
#elif defined(WOLFMQTT_POSIX_SEMAPHORES)
/* Posix style semaphore */
int wm_SemInit(wm_Sem *s)
{
    sem_init(s, 0, 1);
    return 0;
}
int wm_SemFree(wm_Sem *s)
{
    /* no free */
    (void)s;
    return 0;
}
int wm_SemLock(wm_Sem *s)
{
    sem_wait(s);
    return 0;
}
int wm_SemUnlock(wm_Sem *s)
{
    sem_post(s);
    return 0;
}
#elif defined(FREERTOS)
/* FreeRTOS binary semaphore */
int wm_SemInit(wm_Sem *s)
{
    *s = xSemaphoreCreateBinary();
    xSemaphoreGive(*s);
    return 0;
}
int wm_SemFree(wm_Sem *s)
{
    vSemaphoreDelete(*s);
    *s = NULL;
    return 0;
}
int wm_SemLock(wm_Sem *s)
{
    xSemaphoreTake(*s, portMAX_DELAY);
    return 0;
}
int wm_SemUnlock(wm_Sem *s)
{
    xSemaphoreGive(*s);
    return 0;
}
#elif defined(USE_WINDOWS_API)
/* Windows semaphore object */
int wm_SemInit(wm_Sem *s)
{
    *s = CreateSemaphore(NULL, 0, 1, NULL);
    return 0;
}
int wm_SemFree(wm_Sem *s)
{
    CloseHandle(*s);
    *s = NULL;
    return 0;
}
int wm_SemLock(wm_Sem *s)
{
    WaitForSingleObject(*s, 0);
    return 0;
}
int wm_SemUnlock(wm_Sem *s)
{
    ReleaseSemaphore(*s, 1, NULL);
    return 0;
}
#else
int wm_SemInit(wm_Sem *s)
{
    os_sem_create(s, 1);
    return 0;
}
int wm_SemFree(wm_Sem *s)
{
    os_sem_del(s, 0);
    return 0;
}
int wm_SemLock(wm_Sem *s)
{
    os_sem_pend(s, 0);
    return 0;
}
int wm_SemUnlock(wm_Sem *s)
{
    os_sem_post(s);
    return 0;
}
#endif

/* These RespList functions assume caller has locked client->lockClient mutex */
static int MqttClient_RespList_Add(MqttClient *client,
                                   MqttPacketType packet_type, word16 packet_id, MqttPendResp *newResp,
                                   void *packet_obj)
{
    MqttPendResp *tmpResp;

    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

#ifdef WOLFMQTT_DEBUG_CLIENT
    PRINTF("PendResp Add: %p, Type %s (%d), ID %d",
           newResp, MqttPacket_TypeDesc(packet_type), packet_type, packet_id);
#endif

    /* verify newResp is not already in the list */
    for (tmpResp = client->firstPendResp;
         tmpResp != NULL;
         tmpResp = tmpResp->next) {
        if (tmpResp == newResp) {
#ifdef WOLFMQTT_DEBUG_CLIENT
            PRINTF("Pending Response already in list!");
#endif
            return MQTT_CODE_ERROR_BAD_ARG;
        }
    }

    /* Initialize new response */
    XMEMSET(newResp, 0, sizeof(MqttPendResp));
    newResp->packet_id = packet_id;
    newResp->packet_type = packet_type;
    /* opaque pointer to struct based on type */
    newResp->packet_obj = packet_obj;

    if (client->lastPendResp == NULL) {
        /* This is the only list item */
        client->firstPendResp = newResp;
        client->lastPendResp = newResp;
    } else {
        /* Append to end of list */
        newResp->prev = client->lastPendResp;
        client->lastPendResp->next = newResp;
        client->lastPendResp = newResp;
    }
    return 0;
}

static void MqttClient_RespList_Remove(MqttClient *client, MqttPendResp *rmResp)
{
    MqttPendResp *tmpResp;

    if (client == NULL) {
        return;
    }

#ifdef WOLFMQTT_DEBUG_CLIENT
    PRINTF("PendResp Remove: %p", rmResp);
#endif

    /* Find the response entry */
    for (tmpResp = client->firstPendResp;
         tmpResp != NULL;
         tmpResp = tmpResp->next) {
        if (tmpResp == rmResp) {
            break;
        }
    }
    if (tmpResp) {
        /* Fix up the first and last pointers */
        if (client->firstPendResp == tmpResp) {
            client->firstPendResp = tmpResp->next;
        }
        if (client->lastPendResp == tmpResp) {
            client->lastPendResp = tmpResp->prev;
        }

        /* Remove the entry from the list */
        if (tmpResp->next != NULL) {
            tmpResp->next->prev = tmpResp->prev;
        }
        if (tmpResp->prev != NULL) {
            tmpResp->prev->next = tmpResp->next;
        }
    }
}

static int MqttClient_RespList_Find(MqttClient *client,
                                    MqttPacketType packet_type, word16 packet_id, MqttPendResp **retResp)
{
    int rc = 0;
    MqttPendResp *tmpResp;

    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

#ifdef WOLFMQTT_DEBUG_CLIENT
    PRINTF("PendResp Find: Type %s (%d), ID %d",
           MqttPacket_TypeDesc(packet_type), packet_type, packet_id);
#endif

    if (retResp) {
        *retResp = NULL;    /* clear */
    }

    /* Find pending response entry */
    for (tmpResp = client->firstPendResp;
         tmpResp != NULL;
         tmpResp = tmpResp->next) {
        if (packet_type == tmpResp->packet_type &&
            (packet_id == tmpResp->packet_id)) {
#ifdef WOLFMQTT_DEBUG_CLIENT
            PRINTF("PendResp Found: %p, Type %s (%d), ID %d",
                   tmpResp, MqttPacket_TypeDesc(tmpResp->packet_type),
                   tmpResp->packet_type, tmpResp->packet_id);
#endif

            if (retResp) {
                *retResp = tmpResp;
            }
            rc = 1;
            break;
        }
    }
    return rc;
}
#endif /* WOLFMQTT_MULTITHREAD */

/* Returns length decoded or error (as negative) */
/*! \brief      Take a received MQTT packet and try and decode it
 *  \param      client       MQTT client context
 *  \param      rx_buf       Incoming buffer data
 *  \param      rx_len       Incoming buffer length
 *  \param      p_decode     Opaque pointer to packet structure based on type
 *  \param      ppacket_type Decoded packet type
 *  \param      ppacket_qos  Decoded QoS level
 *  \param      ppacket_id   Decoded packet id

 *  \return     Returns length decoded or error (as negative) MQTT_CODE_ERROR_*
                (see enum MqttPacketResponseCodes)
 */
static int MqttClient_DecodePacket(MqttClient *client, byte *rx_buf,
                                   word32 rx_len, void *packet_obj, MqttPacketType *ppacket_type,
                                   MqttQoS *ppacket_qos, word16 *ppacket_id)
{
    int rc = MQTT_CODE_SUCCESS;
    MqttPacket *header;
    MqttPacketType packet_type;
    MqttQoS packet_qos;
    word16 packet_id = 0;
#ifdef WOLFMQTT_V5
    MqttProp *props = NULL;
#endif

    /* must have rx buffer with at least 2 byes for header */
    if (rx_buf == NULL || rx_len < MQTT_PACKET_HEADER_MIN_SIZE) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Decode header */
    header = (MqttPacket *)rx_buf;
    packet_type = (MqttPacketType)MQTT_PACKET_TYPE_GET(header->type_flags);
    if (ppacket_type) {
        *ppacket_type = packet_type;
    }
    packet_qos = (MqttQoS)MQTT_PACKET_FLAGS_GET_QOS(header->type_flags);
    if (ppacket_qos) {
        *ppacket_qos = packet_qos;
    }

    /* Decode packet specific data (if requested) */
    if (ppacket_id || packet_obj) {
        switch (packet_type) {
        case MQTT_PACKET_TYPE_CONNECT_ACK: {
            MqttConnectAck connect_ack, *p_connect_ack = &connect_ack;
            if (packet_obj) {
                p_connect_ack = (MqttConnectAck *)packet_obj;
            } else {
                XMEMSET(p_connect_ack, 0, sizeof(MqttConnectAck));
            }
#ifdef WOLFMQTT_V5
            p_connect_ack->protocol_level = client->protocol_level;
#endif
            rc = MqttDecode_ConnectAck(rx_buf, rx_len, p_connect_ack);
#ifdef WOLFMQTT_V5
            if (packet_obj && rc >= 0) {
                props = p_connect_ack->props;
            }
#endif
            break;
        }
        case MQTT_PACKET_TYPE_PUBLISH: {
            MqttPublish publish, *p_publish = &publish;
            if (packet_obj) {
                p_publish = (MqttPublish *)packet_obj;
            } else {
                XMEMSET(p_publish, 0, sizeof(MqttPublish));
            }
#ifdef WOLFMQTT_V5
            p_publish->protocol_level = client->protocol_level;
#endif
            rc = MqttDecode_Publish(rx_buf, rx_len, p_publish);
            if (rc >= 0) {
                packet_id = p_publish->packet_id;
#ifdef WOLFMQTT_V5
                if (packet_obj) {
                    props = p_publish->props;
                }
#endif
            }
            break;
        }
        case MQTT_PACKET_TYPE_PUBLISH_ACK:
        case MQTT_PACKET_TYPE_PUBLISH_REC:
        case MQTT_PACKET_TYPE_PUBLISH_REL:
        case MQTT_PACKET_TYPE_PUBLISH_COMP: {
            MqttPublishResp publish_resp, *p_publish_resp = &publish_resp;
            if (packet_obj) {
                p_publish_resp = (MqttPublishResp *)packet_obj;
            } else {
                XMEMSET(p_publish_resp, 0, sizeof(MqttPublishResp));
            }
#ifdef WOLFMQTT_V5
            p_publish_resp->protocol_level = client->protocol_level;
#endif
            rc = MqttDecode_PublishResp(rx_buf, rx_len, packet_type,
                                        p_publish_resp);
            if (rc >= 0) {
                packet_id = p_publish_resp->packet_id;
#ifdef WOLFMQTT_V5
                if (packet_obj) {
                    props = p_publish_resp->props;
                }
#endif
            }
            break;
        }
        case MQTT_PACKET_TYPE_SUBSCRIBE_ACK: {
            MqttSubscribeAck subscribe_ack, *p_subscribe_ack = &subscribe_ack;
            if (packet_obj) {
                p_subscribe_ack = (MqttSubscribeAck *)packet_obj;
            } else {
                XMEMSET(p_subscribe_ack, 0, sizeof(MqttSubscribeAck));
            }
#ifdef WOLFMQTT_V5
            p_subscribe_ack->protocol_level = client->protocol_level;
#endif
            rc = MqttDecode_SubscribeAck(rx_buf, rx_len, p_subscribe_ack);
            if (rc >= 0) {
                packet_id = p_subscribe_ack->packet_id;
#ifdef WOLFMQTT_V5
                if (packet_obj) {
                    props = p_subscribe_ack->props;
                }
#endif
            }
            break;
        }
        case MQTT_PACKET_TYPE_UNSUBSCRIBE_ACK: {
            MqttUnsubscribeAck unsubscribe_ack, *p_unsubscribe_ack = &unsubscribe_ack;
            if (packet_obj) {
                p_unsubscribe_ack = (MqttUnsubscribeAck *)packet_obj;
            } else {
                XMEMSET(p_unsubscribe_ack, 0, sizeof(MqttUnsubscribeAck));
            }
#ifdef WOLFMQTT_V5
            p_unsubscribe_ack->protocol_level = client->protocol_level;
#endif
            rc = MqttDecode_UnsubscribeAck(rx_buf, rx_len, p_unsubscribe_ack);
            if (rc >= 0) {
                packet_id = p_unsubscribe_ack->packet_id;
#ifdef WOLFMQTT_V5
                if (packet_obj) {
                    props = p_unsubscribe_ack->props;
                }
#endif
            }
            break;
        }
        case MQTT_PACKET_TYPE_PING_RESP: {
            MqttPing ping, *p_ping = &ping;
            if (packet_obj) {
                p_ping = (MqttPing *)packet_obj;
            } else {
                XMEMSET(p_ping, 0, sizeof(MqttPing));
            }
            rc = MqttDecode_Ping(rx_buf, rx_len, p_ping);
            break;
        }
        case MQTT_PACKET_TYPE_AUTH: {
#ifdef WOLFMQTT_V5
            MqttAuth auth, *p_auth = &auth;
            if (packet_obj) {
                p_auth = (MqttAuth *)packet_obj;
            } else {
                XMEMSET(p_auth, 0, sizeof(MqttAuth));
            }
            rc = MqttDecode_Auth(rx_buf, rx_len, p_auth);
            if (rc >= 0 && packet_obj) {
                props = p_auth->props;
            }
#else
            rc = MQTT_CODE_ERROR_PACKET_TYPE;
#endif /* WOLFMQTT_V5 */
            break;
        }
        case MQTT_PACKET_TYPE_DISCONNECT: {
#ifdef WOLFMQTT_V5
            MqttDisconnect disc, *p_disc = &disc;
            if (packet_obj) {
                p_disc = (MqttDisconnect *)packet_obj;
            } else {
                XMEMSET(p_disc, 0, sizeof(MqttDisconnect));
            }
            rc = MqttDecode_Disconnect(rx_buf, rx_len, p_disc);
            if (rc >= 0 && packet_obj) {
                props = p_disc->props;
            }
#else
            rc = MQTT_CODE_ERROR_PACKET_TYPE;
#endif /* WOLFMQTT_V5 */
            break;
        }
        case MQTT_PACKET_TYPE_CONNECT:
        case MQTT_PACKET_TYPE_SUBSCRIBE:
        case MQTT_PACKET_TYPE_UNSUBSCRIBE:
        case MQTT_PACKET_TYPE_PING_REQ:
        case MQTT_PACKET_TYPE_ANY:
        case MQTT_PACKET_TYPE_RESERVED:
        default:
            /* these type are only encoded by client */
            rc = MQTT_CODE_ERROR_PACKET_TYPE;
            break;
        } /* switch (packet_type) */
    }

    if (ppacket_id) {
        *ppacket_id = packet_id;
    }

#ifdef WOLFMQTT_V5
    if (props) {
#ifdef WOLFMQTT_PROPERTY_CB
        /* Check for properties set by the server */
        if (client->property_cb) {
            /* capture error if returned */
            int rc_err = client->property_cb(client, props, client->property_ctx);
            if (rc_err < 0) {
                rc = rc_err;
            }
        }
#endif
        /* Free the properties */
        MqttProps_Free(props);
    }
#endif

    (void)client;

#ifdef WOLFMQTT_DEBUG_CLIENT
    PRINTF("MqttClient_DecodePacket: Rc %d, Len %d, Type %s (%d), ID %d, QoS %d",
           rc, rx_len, MqttPacket_TypeDesc(packet_type), packet_type, packet_id,
           packet_qos);
#endif

    return rc;
}

static int MqttClient_HandlePacket(MqttClient *client,
                                   MqttPacketType packet_type, void *packet_obj, int timeout_ms)
{
    int rc = MQTT_CODE_SUCCESS;
    MqttQoS packet_qos = MQTT_QOS_0;
    word16 packet_id = 0;

    if (client == NULL || packet_obj == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    switch (packet_type) {
    case MQTT_PACKET_TYPE_CONNECT_ACK: {
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, packet_obj, &packet_type, &packet_qos,
                                     &packet_id);
        break;
    }
    case MQTT_PACKET_TYPE_PUBLISH: {
        MqttPublish *publish = (MqttPublish *)packet_obj;
        MqttPacketType resp_type;

        if (publish->stat == MQTT_MSG_BEGIN || publish->stat == MQTT_MSG_READ) {
            rc = MqttClient_DecodePacket(client, client->rx_buf,
                                         client->packet.buf_len, packet_obj, &packet_type,
                                         &packet_qos, &packet_id);
            if (rc <= 0) {
                return rc;
            }
        }

        rc = MqttClient_Publish_ReadPayload(client, publish, timeout_ms);
        if (rc < 0) {
            break;
        }

        /* Handle QoS */
        if (packet_qos == MQTT_QOS_0) {
            /* we are done, no QoS response */
            break;
        }

        /* Determine packet type to write */
        resp_type = (packet_qos == MQTT_QOS_1) ?
                    MQTT_PACKET_TYPE_PUBLISH_ACK :
                    MQTT_PACKET_TYPE_PUBLISH_REC;
        publish->resp.packet_id = packet_id;

#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode publish response */
        rc = MqttEncode_PublishResp(client->tx_buf, client->tx_buf_len,
                                    resp_type, &publish->resp);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(resp_type), resp_type, packet_id,
               packet_qos);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        client->packet.buf_len = rc;

        /* Send publish response packet */
        rc = MqttPacket_Write(client, client->tx_buf,
                              client->packet.buf_len);

#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        break;
    }
    case MQTT_PACKET_TYPE_PUBLISH_ACK:
    case MQTT_PACKET_TYPE_PUBLISH_REC:
    case MQTT_PACKET_TYPE_PUBLISH_REL:
    case MQTT_PACKET_TYPE_PUBLISH_COMP: {
        MqttPublishResp publish_resp;
        XMEMSET(&publish_resp, 0, sizeof(publish_resp));

        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, &publish_resp, &packet_type,
                                     &packet_qos, &packet_id);
        if (rc <= 0) {
            return rc;
        }

        /* If publish Received or Release QoS then proceed */
        if (packet_type != MQTT_PACKET_TYPE_PUBLISH_REC &&
            packet_type != MQTT_PACKET_TYPE_PUBLISH_REL) {
            break;
        }

#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode publish response */
        publish_resp.packet_id = packet_id;
        packet_type = (MqttPacketType)((int)packet_type + 1); /* next ack */
        rc = MqttEncode_PublishResp(client->tx_buf, client->tx_buf_len,
                                    packet_type, &publish_resp);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(packet_type), packet_type, packet_id,
               packet_qos);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        client->packet.buf_len = rc;

        /* Send publish response packet */
        rc = MqttPacket_Write(client, client->tx_buf,
                              client->packet.buf_len);

#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        break;
    }
    case MQTT_PACKET_TYPE_SUBSCRIBE_ACK: {
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, packet_obj, &packet_type, &packet_qos,
                                     &packet_id);
        break;
    }
    case MQTT_PACKET_TYPE_UNSUBSCRIBE_ACK: {
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, packet_obj, &packet_type, &packet_qos,
                                     &packet_id);
        break;
    }
    case MQTT_PACKET_TYPE_PING_RESP: {
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, packet_obj, &packet_type, &packet_qos,
                                     &packet_id);
        break;
    }
    case MQTT_PACKET_TYPE_AUTH: {
#ifdef WOLFMQTT_V5
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, packet_obj, &packet_type, &packet_qos,
                                     &packet_id);
#else
        rc = MQTT_CODE_ERROR_PACKET_TYPE;
#endif
        break;
    }

    case MQTT_PACKET_TYPE_DISCONNECT: {
#ifdef WOLFMQTT_V5
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, packet_obj, &packet_type, &packet_qos,
                                     &packet_id);
#else
        rc = MQTT_CODE_ERROR_PACKET_TYPE;
#endif
        break;
    }
    case MQTT_PACKET_TYPE_CONNECT:
    case MQTT_PACKET_TYPE_SUBSCRIBE:
    case MQTT_PACKET_TYPE_UNSUBSCRIBE:
    case MQTT_PACKET_TYPE_PING_REQ:
    case MQTT_PACKET_TYPE_ANY:
    case MQTT_PACKET_TYPE_RESERVED:
    default:
        /* these types are only sent from client and should not be sent by broker */
        rc = MQTT_CODE_ERROR_PACKET_TYPE;
        break;
    } /* switch (packet_type) */

#ifdef WOLFMQTT_DEBUG_CLIENT
    if (rc < 0) {
        PRINTF("MqttClient_HandlePacket: Rc %d, Type %s (%d), QoS %d, ID %d",
               rc, MqttPacket_TypeDesc(packet_type), packet_type, packet_qos,
               packet_id);
    }
#endif

    return rc;
}

static int MqttClient_WaitType(MqttClient *client, void *packet_obj,
                               byte wait_type, word16 wait_packet_id, int timeout_ms)
{
    int rc;
    word16 packet_id;
    MqttPacketType packet_type;
#ifdef WOLFMQTT_MULTITHREAD
    MqttPendResp *pendResp;
    int readLocked;
#endif
    MqttMsgStat *mms_stat;
    int waitMatchFound;

    if (client == NULL || packet_obj == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* all packet type structures must have MqttMsgStat at top */
    mms_stat = (MqttMsgStat *)packet_obj;

wait_again:

    /* initialize variables */
    packet_id = 0;
    packet_type = MQTT_PACKET_TYPE_RESERVED;
#ifdef WOLFMQTT_MULTITHREAD
    pendResp = NULL;
    readLocked = 0;
#endif
    waitMatchFound = 0;

#ifdef WOLFMQTT_DEBUG_CLIENT
    PRINTF("MqttClient_WaitType: Type %s (%d), ID %d",
           MqttPacket_TypeDesc(wait_type), wait_type, wait_packet_id);
#endif

    switch ((int)*mms_stat) {
    case MQTT_MSG_BEGIN: {
        /* reset the packet state */
        client->packet.stat = MQTT_PK_BEGIN;
        client->read.pos = 0;

        FALL_THROUGH;
    }
#ifdef WOLFMQTT_V5
    case MQTT_MSG_AUTH:
#endif
    case MQTT_MSG_WAIT: {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock recv socket mutex */
        rc = wm_SemLock(&client->lockRecv);
        if (rc != 0) {
            printf("!!!!LOCK ERROR!!!!\n");
            return rc;
        }
        readLocked = 1;

        /* Check to see if packet type and id have already completed */
        pendResp = NULL;
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            if (MqttClient_RespList_Find(client, (MqttPacketType)wait_type,
                                         wait_packet_id, &pendResp)) {
                if (pendResp->packetDone) {
                    /* pending response is already done, so return */
                    rc = pendResp->packet_ret;
#ifdef WOLFMQTT_DEBUG_CLIENT
                    PRINTF("PendResp Done %p: Rc %d", pendResp, rc);
#endif
                    MqttClient_RespList_Remove(client, pendResp);
                    wm_SemUnlock(&client->lockClient);
                    wm_SemUnlock(&client->lockRecv);
                    return rc;
                }
            }
            wm_SemUnlock(&client->lockClient);
        } else {
            wm_SemUnlock(&client->lockRecv);
            return rc;
        }
#endif /* WOLFMQTT_MULTITHREAD */

        *mms_stat = MQTT_MSG_WAIT;

        /* Wait for packet */
        rc = MqttPacket_Read(client, client->rx_buf, client->rx_buf_len,
                             timeout_ms);
        /* handle failure */
        if (rc <= 0) {
            break;
        }
        /* capture length read */
        client->packet.buf_len = rc;

        /* Decode Packet - get type and id */
        rc = MqttClient_DecodePacket(client, client->rx_buf,
                                     client->packet.buf_len, NULL, &packet_type, NULL, &packet_id);
        if (rc < 0) {
            break;
        }

        *mms_stat = MQTT_MSG_READ;

        FALL_THROUGH;
    }

    case MQTT_MSG_READ:
    case MQTT_MSG_READ_PAYLOAD: {
        MqttPacketType use_packet_type;
        void *use_packet_obj;

#ifdef WOLFMQTT_MULTITHREAD
        readLocked = 1; /* if in this state read is locked */
#endif

        /* read payload state only happens for publish messages */
        if (*mms_stat == MQTT_MSG_READ_PAYLOAD) {
            packet_type = MQTT_PACKET_TYPE_PUBLISH;
        }

        /* Determine if we received data for this request */
        if ((wait_type == MQTT_PACKET_TYPE_ANY || wait_type == packet_type) &&
            (wait_packet_id == 0 || wait_packet_id == packet_id)) {
            use_packet_obj = packet_obj;
            waitMatchFound = 1;
        } else {
            /* use generic packet object */
            use_packet_obj = &client->msg;
        }
        use_packet_type = packet_type;

#ifdef WOLFMQTT_MULTITHREAD
        /* Check to see if we have a pending response for this packet */
        pendResp = NULL;
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            if (MqttClient_RespList_Find(client, packet_type, packet_id,
                                         &pendResp)) {
                /* we found packet match this incoming read packet */
                pendResp->packetProcessing = 1;
                use_packet_obj = pendResp->packet_obj;
                use_packet_type = pendResp->packet_type;
                waitMatchFound = 0; /* req from another thread... not a match */
            }
            wm_SemUnlock(&client->lockClient);
        } else {
            break; /* error */
        }
#endif /* WOLFMQTT_MULTITHREAD */

        /* Perform packet handling for publish callback and QoS */
        rc = MqttClient_HandlePacket(client, use_packet_type,
                                     use_packet_obj, timeout_ms);

#ifdef WOLFMQTT_NONBLOCK
        if (rc == MQTT_CODE_CONTINUE) {
            /* we have received some data, so keep the recv
                mutex lock active and return */
            return rc;
        }
#endif

        /* handle success case */
        if (rc >= 0) {
            rc = MQTT_CODE_SUCCESS;
        }

#ifdef WOLFMQTT_MULTITHREAD
        if (pendResp) {
            /* Mark pending response entry done */
            if (wm_SemLock(&client->lockClient) == 0) {
                pendResp->packetDone = 1;
                pendResp->packet_ret = rc;
#ifdef WOLFMQTT_DEBUG_CLIENT
                PRINTF("PendResp Done %p", pendResp);
#endif
                pendResp = NULL;
                wm_SemUnlock(&client->lockClient);
            }
        }
#endif /* WOLFMQTT_MULTITHREAD */
        break;
    }

    case MQTT_MSG_WRITE:
    case MQTT_MSG_WRITE_PAYLOAD:
    default: {
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_WaitType: Invalid state %d!", *mms_stat);
#endif
        rc = MQTT_CODE_ERROR_STAT;
        break;
    }
    } /* switch (*mms_stat) */

#ifdef WOLFMQTT_MULTITHREAD
    if (readLocked) {
        wm_SemUnlock(&client->lockRecv);
    }
#endif

    /* reset state */
    *mms_stat = MQTT_MSG_BEGIN;

    if (rc < 0) {
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_WaitType: Failure: %s (%d)",
               MqttClient_ReturnCodeToString(rc), rc);
#endif
        return rc;
    }

    if (!waitMatchFound) {
        /* if we get here, then the we are still waiting for a packet */
        goto wait_again;
    }

    return rc;
}


/* Public Functions */
int MqttClient_Init(MqttClient *client, MqttNet *net,
                    MqttMsgCb msg_cb,
                    byte *tx_buf, int tx_buf_len,
                    byte *rx_buf, int rx_buf_len,
                    int cmd_timeout_ms)
{
    int rc = MQTT_CODE_SUCCESS;

    /* Check arguments */
    if (client == NULL ||
        tx_buf == NULL || tx_buf_len <= 0 ||
        rx_buf == NULL || rx_buf_len <= 0) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Initialize the client structure to zero */
    XMEMSET(client, 0, sizeof(MqttClient));

    /* Setup client structure */
    client->msg_cb = msg_cb;
    client->tx_buf = tx_buf;
    client->tx_buf_len = tx_buf_len;
    client->rx_buf = rx_buf;
    client->rx_buf_len = rx_buf_len;
    client->cmd_timeout_ms = cmd_timeout_ms;
#ifdef WOLFMQTT_V5
    client->max_qos = MQTT_QOS_2;
    client->retain_avail = 1;
    client->protocol_level = MQTT_CONNECT_PROTOCOL_LEVEL;
#endif

#ifdef WOLFMQTT_MULTITHREAD
    rc = wm_SemInit(&client->lockSend);

    if (rc == 0) {
        rc = wm_SemInit(&client->lockRecv);
    }
    if (rc == 0) {
        rc = wm_SemInit(&client->lockClient);
    }
#endif

    if (rc == 0) {
        /* Init socket */
        rc = MqttSocket_Init(client, net);
    }

    if (rc != 0) {
        /* Cleanup if init failed */
        MqttClient_DeInit(client);
    }

    return rc;
}

void MqttClient_DeInit(MqttClient *client)
{
    if (client != NULL) {
#ifdef WOLFMQTT_MULTITHREAD
        (void)wm_SemFree(&client->lockSend);
        (void)wm_SemFree(&client->lockRecv);
        (void)wm_SemFree(&client->lockClient);
#endif
    }
}

#ifdef WOLFMQTT_DISCONNECT_CB
int MqttClient_SetDisconnectCallback(MqttClient *client,
                                     MqttDisconnectCb discCb, void *ctx)
{
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    client->disconnect_cb = discCb;
    client->disconnect_ctx = ctx;

    return MQTT_CODE_SUCCESS;
}
#endif

#ifdef WOLFMQTT_PROPERTY_CB
int MqttClient_SetPropertyCallback(MqttClient *client, MqttPropertyCb propCb,
                                   void *ctx)
{
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    client->property_cb = propCb;
    client->property_ctx = ctx;

    return MQTT_CODE_SUCCESS;
}
#endif


int MqttClient_Connect(MqttClient *client, MqttConnect *mc_connect)
{
    int rc, len = 0;

    /* Validate required arguments */
    if (client == NULL || mc_connect == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (mc_connect->stat == MQTT_MSG_BEGIN) {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

#ifdef WOLFMQTT_V5
        /* Use specified protocol version if set */
        mc_connect->protocol_level = client->protocol_level;
#endif

        /* Encode the connect packet */
        rc = MqttEncode_Connect(client->tx_buf, client->tx_buf_len, mc_connect);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_CONNECT),
               MQTT_PACKET_TYPE_CONNECT, 0, 0);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        len = rc;

#ifdef WOLFMQTT_MULTITHREAD
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            /* inform other threads of expected response */
            rc = MqttClient_RespList_Add(client, MQTT_PACKET_TYPE_CONNECT_ACK, 0,
                                         &mc_connect->pendResp, &mc_connect->ack);
            wm_SemUnlock(&client->lockClient);
        }
        if (rc != 0) {
            wm_SemUnlock(&client->lockSend);
            return rc; /* Error locking client */
        }
#endif

        /* Send connect packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        if (rc != len) {
            return rc;
        }
#ifdef WOLFMQTT_V5
        /* Enhanced authentication */
        if (client->enable_eauth == 1) {
            mc_connect->stat = MQTT_MSG_AUTH;
        } else
#endif
        {
            mc_connect->stat = MQTT_MSG_WAIT;
        }
    }

#ifdef WOLFMQTT_V5
    /* Enhanced authentication */
    if (mc_connect->protocol_level > MQTT_CONNECT_PROTOCOL_LEVEL_4 &&
        mc_connect->stat == MQTT_MSG_AUTH) {
        MqttAuth auth, *p_auth = &auth;
        MqttProp *prop, *conn_prop;

        /* Find the AUTH property in the connect structure */
        for (conn_prop = mc_connect->props;
             (conn_prop != NULL) && (conn_prop->type != MQTT_PROP_AUTH_METHOD);
             conn_prop = conn_prop->next) {
        }
        if (conn_prop == NULL) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &mc_connect->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            /* AUTH property was not set in connect structure */
            return MQTT_CODE_ERROR_BAD_ARG;
        }

        XMEMSET((void *)p_auth, 0, sizeof(MqttAuth));

        /* Set the authentication reason */
        p_auth->reason_code = MQTT_REASON_CONT_AUTH;

        /* Use the same authentication method property from connect */
        prop = MqttProps_Add(&p_auth->props);
        prop->type = MQTT_PROP_AUTH_METHOD;
        prop->data_str.str = conn_prop->data_str.str;
        prop->data_str.len = conn_prop->data_str.len;

        /* Send the AUTH packet */
        rc = MqttClient_Auth(client, p_auth);
        MqttClient_PropsFree(p_auth->props);
#ifdef WOLFMQTT_NONBLOCK
        if (rc == MQTT_CODE_CONTINUE) {
            return rc;
        }
#endif
        if (rc != len) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &mc_connect->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            return rc;
        }
    }
#endif /* WOLFMQTT_V5 */

    /* Wait for connect ack packet */
    rc = MqttClient_WaitType(client, &mc_connect->ack,
                             MQTT_PACKET_TYPE_CONNECT_ACK, 0, client->cmd_timeout_ms);
#ifdef WOLFMQTT_NONBLOCK
    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
#endif

#ifdef WOLFMQTT_MULTITHREAD
    if (wm_SemLock(&client->lockClient) == 0) {
        MqttClient_RespList_Remove(client, &mc_connect->pendResp);
        wm_SemUnlock(&client->lockClient);
    }
#endif

    /* reset state */
    mc_connect->stat = MQTT_MSG_BEGIN;

    return rc;
}

#ifdef WOLFMQTT_TEST_NONBLOCK
static int testNbAlt = 0;
#endif

static int MqttClient_Publish_ReadPayload(MqttClient *client,
        MqttPublish *publish, int timeout_ms)
{
    int rc = MQTT_CODE_SUCCESS;
    byte msg_done;

    /* Handle packet callback and read remaining payload */
    do {
        /* Determine if message is done */
        msg_done = ((publish->buffer_pos + publish->buffer_len) >=
                    publish->total_len) ? 1 : 0;

        if (publish->buffer_new) {
            /* Issue callback for new message (first time only) */
            if (client->msg_cb) {
                /* if using the temp publish message buffer,
                   then populate message context with client context */
                if (publish->ctx == NULL && &client->msg.publish == publish) {
                    publish->ctx = client->ctx;
                }
                rc = client->msg_cb(client, publish, publish->buffer_new,
                                    msg_done);
                if (rc != MQTT_CODE_SUCCESS) {
                    return rc;
                };
            }

            /* Reset topic name since valid on new message only */
            publish->topic_name = NULL;
            publish->topic_name_len = 0;

            publish->buffer_new = 0;
        }

        /* Read payload */
        if (!msg_done) {
            int msg_len;

            /* add last length to position and reset len */
            publish->buffer_pos += publish->buffer_len;
            publish->buffer_len = 0;

            /* set state to reading payload */
            publish->stat = MQTT_MSG_READ_PAYLOAD;

            msg_len = (publish->total_len - publish->buffer_pos);
            if (msg_len > client->rx_buf_len) {
                msg_len = client->rx_buf_len;
            }

            /* make sure there is something to read */
            if (msg_len > 0) {
#ifdef WOLFMQTT_TEST_NONBLOCK
                if (!testNbAlt) {
                    testNbAlt = 1;
                    return MQTT_CODE_CONTINUE;
                }
                testNbAlt = 0;
#endif

                rc = MqttSocket_Read(client, client->rx_buf, msg_len,
                                     timeout_ms);
                if (rc < 0) {
                    break;
                }

                /* Update message */
                publish->buffer = client->rx_buf;
                publish->buffer_len = rc;
                rc = MQTT_CODE_SUCCESS; /* mark success */

                msg_done = ((publish->buffer_pos + publish->buffer_len) >=
                            publish->total_len) ? 1 : 0;

                /* Issue callback for additional publish payload */
                if (client->msg_cb) {
                    rc = client->msg_cb(client, publish, publish->buffer_new,
                                        msg_done);
                    if (rc != MQTT_CODE_SUCCESS) {
                        return rc;
                    };
                }
            }
        }
    } while (!msg_done);

    return rc;
}

static int MqttClient_Publish_WritePayload(MqttClient *client,
        MqttPublish *publish, MqttPublishCb pubCb)
{
    int rc = MQTT_CODE_SUCCESS;

    if (client == NULL || publish == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (pubCb) {
        word32 tmp_len = publish->buffer_len;

        do {
            /* Use the callback to get payload */
            if ((client->write.len = pubCb(publish)) < 0) {
                return MQTT_CODE_ERROR_CALLBACK;
            }

            if ((word32)client->write.len < publish->buffer_len) {
                /* Last read */
                tmp_len = (word32)client->write.len;
            }

            /* Send payload */
            do {
                if (client->write.len > client->tx_buf_len) {
                    client->write.len = client->tx_buf_len;
                }
                publish->intBuf_len = client->write.len;
                XMEMCPY(client->tx_buf, &publish->buffer[publish->intBuf_pos],
                        client->write.len);

                rc = MqttPacket_Write(client, client->tx_buf,
                                      client->write.len);
                if (rc < 0) {
                    return rc;
                }

                publish->intBuf_pos += publish->intBuf_len;
                publish->intBuf_len = 0;

            } while (publish->intBuf_pos < tmp_len);

            publish->buffer_pos += publish->intBuf_pos;
            publish->intBuf_pos = 0;

        } while (publish->buffer_pos < publish->total_len);
    } else if (publish->buffer_pos < publish->total_len) {
        if (publish->buffer_pos > 0) {
            XMEMCPY(client->tx_buf, publish->buffer,
                    client->write.len);
        }

        /* Send packet and payload */
        do {
            rc = MqttPacket_Write(client, client->tx_buf,
                                  client->write.len);
            if (rc < 0) {
                return rc;
            }

            publish->intBuf_pos += publish->intBuf_len;
            publish->intBuf_len = 0;

            /* Check if we are done sending publish message */
            if (publish->intBuf_pos >= publish->buffer_len) {
                rc = MQTT_CODE_SUCCESS;
                break;
            }

            /* Build packet payload to send */
            client->write.len = (publish->buffer_len - publish->intBuf_pos);
            if (client->write.len > client->tx_buf_len) {
                client->write.len = client->tx_buf_len;
            }
            publish->intBuf_len = client->write.len;
            XMEMCPY(client->tx_buf, &publish->buffer[publish->intBuf_pos],
                    client->write.len);

#ifdef WOLFMQTT_NONBLOCK
            return MQTT_CODE_CONTINUE;
#endif
        } while (publish->intBuf_pos < publish->buffer_len);

        if (rc >= 0) {
            /* If transferring more chunks */
            publish->buffer_pos += publish->intBuf_pos;
            if (publish->buffer_pos < publish->total_len) {
                /* Build next payload to send */
                client->write.len = (publish->total_len - publish->buffer_pos);
                if (client->write.len > client->tx_buf_len) {
                    client->write.len = client->tx_buf_len;
                }
                rc = MQTT_CODE_CONTINUE;
            }
        }
    }
    return rc;
}

int MqttClient_Publish(MqttClient *client, MqttPublish *publish)
{
    return MqttClient_Publish_ex(client, publish, NULL);
}

int MqttClient_Publish_ex(MqttClient *client, MqttPublish *publish,
                          MqttPublishCb pubCb)
{
    int rc = MQTT_CODE_SUCCESS;
    MqttPacketType resp_type;

    /* Validate required arguments */
    if (client == NULL || publish == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

#ifdef WOLFMQTT_V5
    /* Use specified protocol version if set */
    publish->protocol_level = client->protocol_level;

    /* Validate publish request against server properties */
    if ((publish->qos > client->max_qos) ||
        ((publish->retain == 1) && (client->retain_avail == 0))) {
        return MQTT_CODE_ERROR_SERVER_PROP;
    }
#endif

    switch (publish->stat) {
    case MQTT_MSG_BEGIN: {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode the publish packet */
        rc = MqttEncode_Publish(client->tx_buf, client->tx_buf_len,
                                publish, pubCb ? 1 : 0);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_PUBLISH),
               MQTT_PACKET_TYPE_PUBLISH, publish->packet_id,
               publish->qos);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        client->write.len = rc;

#ifdef WOLFMQTT_MULTITHREAD
        if (publish->qos > MQTT_QOS_0) {
            resp_type = (publish->qos == MQTT_QOS_1) ?
                        MQTT_PACKET_TYPE_PUBLISH_ACK :
                        MQTT_PACKET_TYPE_PUBLISH_COMP;

            rc = wm_SemLock(&client->lockClient);
            if (rc == 0) {
                /* inform other threads of expected response */
                rc = MqttClient_RespList_Add(client, resp_type,
                                             publish->packet_id, &publish->pendResp, &publish->resp);
                wm_SemUnlock(&client->lockClient);
            }
            if (rc != 0) {
                wm_SemUnlock(&client->lockSend);
                return rc; /* Error locking client */
            }
        }
#endif

        publish->stat = MQTT_MSG_WRITE;

        FALL_THROUGH;
    }
    case MQTT_MSG_WRITE: {
        /* Send packet */
        rc = MqttPacket_Write(client, client->tx_buf, client->write.len);
#ifdef WOLFMQTT_NONBLOCK
        if (rc == MQTT_CODE_CONTINUE) {
            return rc;
        }
#endif
        if (rc < 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &publish->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            return rc;
        }

        /* advance state */
        publish->stat = MQTT_MSG_WRITE_PAYLOAD;

        FALL_THROUGH;
    }
    case MQTT_MSG_WRITE_PAYLOAD: {
        rc = MqttClient_Publish_WritePayload(client, publish, pubCb);
#ifdef WOLFMQTT_NONBLOCK
        if (rc == MQTT_CODE_CONTINUE) {
            return rc;
        }
#endif
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif

        if (rc < 0) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &publish->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            break;
        }

        /* if not expecting a reply then we are done */
        if (publish->qos == MQTT_QOS_0) {
            break;
        }
        publish->stat = MQTT_MSG_WAIT;

        FALL_THROUGH;
    }

    case MQTT_MSG_WAIT: {
        /* Handle QoS */
        if (publish->qos > MQTT_QOS_0) {
            /* Determine packet type to wait for */
            resp_type = (publish->qos == MQTT_QOS_1) ?
                        MQTT_PACKET_TYPE_PUBLISH_ACK :
                        MQTT_PACKET_TYPE_PUBLISH_COMP;

            /* Wait for publish response packet */
            rc = MqttClient_WaitType(client, &publish->resp, resp_type,
                                     publish->packet_id, client->cmd_timeout_ms);
#ifdef WOLFMQTT_NONBLOCK
            if (rc == MQTT_CODE_CONTINUE) {
                break;
            }
#endif
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &publish->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
        }
        break;
    }

#ifdef WOLFMQTT_V5
    case MQTT_MSG_AUTH:
#endif
    case MQTT_MSG_READ:
    case MQTT_MSG_READ_PAYLOAD:
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_Publish: Invalid state %d!",
               publish->stat);
#endif
        rc = MQTT_CODE_ERROR_STAT;
        break;
    } /* switch (publish->stat) */

    /* reset state */
#ifdef WOLFMQTT_NONBLOCK
    if (rc != MQTT_CODE_CONTINUE)
#endif
    {
        publish->stat = MQTT_MSG_BEGIN;
    }
    if (rc > 0) {
        rc = MQTT_CODE_SUCCESS;
    }

    return rc;
}

int MqttClient_Subscribe(MqttClient *client, MqttSubscribe *subscribe)
{
    int rc, len, i;
    MqttTopic *topic;

    /* Validate required arguments */
    if (client == NULL || subscribe == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

#ifdef WOLFMQTT_V5
    /* Use specified protocol version if set */
    subscribe->protocol_level = client->protocol_level;
#endif

    if (subscribe->stat == MQTT_MSG_BEGIN) {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode the subscribe packet */
        rc = MqttEncode_Subscribe(client->tx_buf, client->tx_buf_len,
                                  subscribe);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_SUBSCRIBE),
               MQTT_PACKET_TYPE_SUBSCRIBE, subscribe->packet_id, 0);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        len = rc;

#ifdef WOLFMQTT_MULTITHREAD
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            /* inform other threads of expected response */
            rc = MqttClient_RespList_Add(client, MQTT_PACKET_TYPE_SUBSCRIBE_ACK,
                                         subscribe->packet_id, &subscribe->pendResp, &subscribe->ack);
            wm_SemUnlock(&client->lockClient);
        }
        if (rc != 0) {
            wm_SemUnlock(&client->lockSend);
            return rc; /* Error locking client */
        }
#endif

        /* Send subscribe packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        if (rc != len) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &subscribe->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            return rc;
        }

        subscribe->stat = MQTT_MSG_WAIT;
    }

    /* Wait for subscribe ack packet */
    rc = MqttClient_WaitType(client, &subscribe->ack,
                             MQTT_PACKET_TYPE_SUBSCRIBE_ACK, subscribe->packet_id,
                             client->cmd_timeout_ms);
#ifdef WOLFMQTT_NONBLOCK
    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
#endif

#ifdef WOLFMQTT_MULTITHREAD
    if (wm_SemLock(&client->lockClient) == 0) {
        MqttClient_RespList_Remove(client, &subscribe->pendResp);
        wm_SemUnlock(&client->lockClient);
    }
#endif

    /* Populate return codes */
    if (rc == MQTT_CODE_SUCCESS) {
        for (i = 0; i < subscribe->topic_count; i++) {
            topic = &subscribe->topics[i];
            if (subscribe->ack.return_codes) {
                topic->return_code = subscribe->ack.return_codes[i];
            }
        }
    }

    /* reset state */
    subscribe->stat = MQTT_MSG_BEGIN;

    return rc;
}

int MqttClient_Unsubscribe(MqttClient *client, MqttUnsubscribe *unsubscribe)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL || unsubscribe == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

#ifdef WOLFMQTT_V5
    /* Use specified protocol version if set */
    unsubscribe->protocol_level = client->protocol_level;
#endif

    if (unsubscribe->stat == MQTT_MSG_BEGIN) {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode the subscribe packet */
        rc = MqttEncode_Unsubscribe(client->tx_buf, client->tx_buf_len,
                                    unsubscribe);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_UNSUBSCRIBE),
               MQTT_PACKET_TYPE_UNSUBSCRIBE, unsubscribe->packet_id, 0);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        len = rc;

#ifdef WOLFMQTT_MULTITHREAD
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            /* inform other threads of expected response */
            rc = MqttClient_RespList_Add(client,
                                         MQTT_PACKET_TYPE_UNSUBSCRIBE_ACK, unsubscribe->packet_id,
                                         &unsubscribe->pendResp, &unsubscribe->ack);
            wm_SemUnlock(&client->lockClient);
        }
        if (rc != 0) {
            wm_SemUnlock(&client->lockSend); /* Error locking client */
            return rc;
        }
#endif

        /* Send unsubscribe packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        if (rc != len) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &unsubscribe->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            return rc;
        }

        unsubscribe->stat = MQTT_MSG_WAIT;
    }

    /* Wait for unsubscribe ack packet */
    rc = MqttClient_WaitType(client, &unsubscribe->ack,
                             MQTT_PACKET_TYPE_UNSUBSCRIBE_ACK, unsubscribe->packet_id,
                             client->cmd_timeout_ms);
#ifdef WOLFMQTT_NONBLOCK
    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
#endif

#ifdef WOLFMQTT_MULTITHREAD
    if (wm_SemLock(&client->lockClient) == 0) {
        MqttClient_RespList_Remove(client, &unsubscribe->pendResp);
        wm_SemUnlock(&client->lockClient);
    }
#endif

#ifdef WOLFMQTT_V5
    if (unsubscribe->ack.props != NULL) {
        /* Release the allocated properties */
        MqttClient_PropsFree(unsubscribe->ack.props);
    }
#endif

    /* reset state */
    unsubscribe->stat = MQTT_MSG_BEGIN;

    return rc;
}

int MqttClient_Ping_ex(MqttClient *client, MqttPing *ping)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL || ping == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (ping->stat == MQTT_MSG_BEGIN) {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode the subscribe packet */
        rc = MqttEncode_Ping(client->tx_buf, client->tx_buf_len, ping);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_PING_REQ),
               MQTT_PACKET_TYPE_PING_REQ, 0, 0);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        len = rc;

#ifdef WOLFMQTT_MULTITHREAD
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            /* inform other threads of expected response */
            rc = MqttClient_RespList_Add(client, MQTT_PACKET_TYPE_PING_RESP, 0,
                                         &ping->pendResp, ping);
            wm_SemUnlock(&client->lockClient);
        }
        if (rc != 0) {
            wm_SemUnlock(&client->lockSend);
            return rc; /* Error locking client */
        }
#endif

        /* Send ping req packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        if (rc != len) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &ping->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            return rc;
        }

        ping->stat = MQTT_MSG_WAIT;
    }
//     Wait for ping resp packet
    rc = MqttClient_WaitType(client, ping, MQTT_PACKET_TYPE_PING_RESP, 0,
                             client->cmd_timeout_ms);
#ifdef WOLFMQTT_NONBLOCK
    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
#endif

#ifdef WOLFMQTT_MULTITHREAD
    if (wm_SemLock(&client->lockClient) == 0) {
        MqttClient_RespList_Remove(client, &ping->pendResp);
        wm_SemUnlock(&client->lockClient);
    }
#endif

    /* reset state */
    ping->stat = MQTT_MSG_BEGIN;
    return rc;
}

int MqttClient_Ping(MqttClient *client)
{
    MqttPing ping;
    XMEMSET(&ping, 0, sizeof(ping));
    return MqttClient_Ping_ex(client, &ping);
}

int MqttClient_Disconnect(MqttClient *client)
{
    return MqttClient_Disconnect_ex(client, NULL);
}

int MqttClient_Disconnect_ex(MqttClient *client, MqttDisconnect *disconnect)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

#ifdef WOLFMQTT_V5
    if (disconnect != NULL) {
        /* Use specified protocol version if set */
        disconnect->protocol_level = client->protocol_level;
    }
#endif

#ifdef WOLFMQTT_MULTITHREAD
    /* Lock send socket mutex */
    rc = wm_SemLock(&client->lockSend);
    if (rc != 0) {
        return rc;
    }
#endif

    /* Encode the disconnect packet */
    rc = MqttEncode_Disconnect(client->tx_buf, client->tx_buf_len, disconnect);
#ifdef WOLFMQTT_DEBUG_CLIENT
    PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
           rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_DISCONNECT),
           MQTT_PACKET_TYPE_DISCONNECT, 0, 0);
#endif
    if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        return rc;
    }
    len = rc;

    /* Send disconnect packet */
    rc = MqttPacket_Write(client, client->tx_buf, len);
#ifdef WOLFMQTT_MULTITHREAD
    wm_SemUnlock(&client->lockSend);
#endif
    if (rc != len) {
        return rc;
    }

    /* No response for MQTT disconnect packet */

    return MQTT_CODE_SUCCESS;
}

#ifdef WOLFMQTT_V5
int MqttClient_Auth(MqttClient *client, MqttAuth *auth)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (auth->stat == MQTT_MSG_BEGIN) {
#ifdef WOLFMQTT_MULTITHREAD
        /* Lock send socket mutex */
        rc = wm_SemLock(&client->lockSend);
        if (rc != 0) {
            return rc;
        }
#endif

        /* Encode the authentication packet */
        rc = MqttEncode_Auth(client->tx_buf, client->tx_buf_len, auth);
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("MqttClient_EncodePacket: Len %d, Type %s (%d), ID %d, QoS %d",
               rc, MqttPacket_TypeDesc(MQTT_PACKET_TYPE_AUTH),
               MQTT_PACKET_TYPE_AUTH, 0, 0);
#endif
        if (rc <= 0) {
#ifdef WOLFMQTT_MULTITHREAD
            wm_SemUnlock(&client->lockSend);
#endif
            return rc;
        }
        len = rc;

#ifdef WOLFMQTT_MULTITHREAD
        rc = wm_SemLock(&client->lockClient);
        if (rc == 0) {
            /* inform other threads of expected response */
            rc = MqttClient_RespList_Add(client, MQTT_PACKET_TYPE_AUTH, 0,
                                         &auth->pendResp, auth);
            wm_SemUnlock(&client->lockClient);
        }
        if (rc != 0) {
            wm_SemUnlock(&client->lockSend);
            return rc; /* Error locking client */
        }
#endif

        /* Send authentication packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
#ifdef WOLFMQTT_MULTITHREAD
        wm_SemUnlock(&client->lockSend);
#endif
        if (rc != len) {
#ifdef WOLFMQTT_MULTITHREAD
            if (wm_SemLock(&client->lockClient) == 0) {
                MqttClient_RespList_Remove(client, &auth->pendResp);
                wm_SemUnlock(&client->lockClient);
            }
#endif
            return rc;
        }

        auth->stat = MQTT_MSG_WAIT;
    }

    /* Wait for auth packet */
    rc = MqttClient_WaitType(client, auth, MQTT_PACKET_TYPE_AUTH, 0,
                             client->cmd_timeout_ms);
#ifdef WOLFMQTT_NONBLOCK
    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
#endif

#ifdef WOLFMQTT_MULTITHREAD
    if (wm_SemLock(&client->lockClient) == 0) {
        MqttClient_RespList_Remove(client, &auth->pendResp);
        wm_SemUnlock(&client->lockClient);
    }
#endif

    /* reset state */
    auth->stat = MQTT_MSG_BEGIN;

    return rc;
}

MqttProp *MqttClient_PropsAdd(MqttProp **head)
{
    return MqttProps_Add(head);
}

void MqttClient_PropsFree(MqttProp *head)
{
    MqttProps_Free(head);
}

#endif /* WOLFMQTT_V5 */

int MqttClient_WaitMessage_ex(MqttClient *client, MqttObject *msg, int timeout_ms)
{
    return MqttClient_WaitType(client, msg, MQTT_PACKET_TYPE_ANY, 0,
                               timeout_ms);
}
int MqttClient_WaitMessage(MqttClient *client, int timeout_ms)
{
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    return MqttClient_WaitMessage_ex(client, &client->msg, timeout_ms);
}

int MqttClient_NetConnect(MqttClient *client, const char *host,
                          word16 port, int timeout_ms, int use_tls, MqttTlsCb cb)
{
    return MqttSocket_Connect(client, host, port, timeout_ms, use_tls, cb);
}

int MqttClient_NetDisconnect(MqttClient *client)
{
    return MqttSocket_Disconnect(client);
}

int MqttClient_GetProtocolVersion(MqttClient *client)
{
#ifdef WOLFMQTT_V5
    if (client && client->protocol_level == MQTT_CONNECT_PROTOCOL_LEVEL_5) {
        return MQTT_CONNECT_PROTOCOL_LEVEL_5;
    }
#else
    (void)client;
#endif
    return MQTT_CONNECT_PROTOCOL_LEVEL_4;
}
const char *MqttClient_GetProtocolVersionString(MqttClient *client)
{
    const char *str = NULL;
    int ver = MqttClient_GetProtocolVersion(client);
    switch (ver) {
    case MQTT_CONNECT_PROTOCOL_LEVEL_4:
        return "v3.1.1";
#ifdef WOLFMQTT_V5
    case MQTT_CONNECT_PROTOCOL_LEVEL_5:
        return "v5";
#endif
    default:
        break;
    }
    return str;
}

#ifndef WOLFMQTT_NO_ERROR_STRINGS
const char *MqttClient_ReturnCodeToString(int return_code)
{
    switch (return_code) {
    case MQTT_CODE_SUCCESS:
        return "Success";
    case MQTT_CODE_CONTINUE:
        return "Continue"; /* would block */
    case MQTT_CODE_STDIN_WAKE:
        return "STDIN Wake";
    case MQTT_CODE_ERROR_BAD_ARG:
        return "Error (Bad argument)";
    case MQTT_CODE_ERROR_OUT_OF_BUFFER:
        return "Error (Out of buffer)";
    case MQTT_CODE_ERROR_MALFORMED_DATA:
        return "Error (Malformed Remaining Length)";
    case MQTT_CODE_ERROR_PACKET_TYPE:
        return "Error (Packet Type Mismatch)";
    case MQTT_CODE_ERROR_PACKET_ID:
        return "Error (Packet Id Mismatch)";
    case MQTT_CODE_ERROR_TLS_CONNECT:
        return "Error (TLS Connect)";
    case MQTT_CODE_ERROR_TIMEOUT:
        return "Error (Timeout)";
    case MQTT_CODE_ERROR_NETWORK:
        return "Error (Network)";
    case MQTT_CODE_ERROR_MEMORY:
        return "Error (Memory)";
    case MQTT_CODE_ERROR_STAT:
        return "Error (State)";
    case MQTT_CODE_ERROR_PROPERTY:
        return "Error (Property)";
    case MQTT_CODE_ERROR_SERVER_PROP:
        return "Error (Server Property)";
    case MQTT_CODE_ERROR_CALLBACK:
        return "Error (Error in Callback)";

    }
    return "Unknown";
}
#endif /* !WOLFMQTT_NO_ERROR_STRINGS */

#ifdef WOLFMQTT_SN

/* Private functions */
static int SN_Client_HandlePacket(MqttClient *client, SN_MsgType packet_type,
                                  void *packet_obj, word16 *ppacket_id, int timeout)
{
    int rc = MQTT_CODE_SUCCESS;
    word16 packet_id = 0;

    (void)timeout;

    switch ((int)packet_type) {
    case SN_MSG_TYPE_GWINFO: {
        SN_GwInfo info, *p_info = &info;
        if (packet_obj) {
            p_info = (SN_GwInfo *)packet_obj;
        } else {
            XMEMSET(p_info, 0, sizeof(SN_GwInfo));
        }

        rc = SN_Decode_GWInfo(client->rx_buf, client->packet.buf_len, p_info);
        if (rc <= 0) {
            return rc;
        }
        break;
    }
    case SN_MSG_TYPE_CONNACK: {
        /* Decode connect ack */
        SN_ConnectAck connect_ack, *p_connect_ack = &connect_ack;
        if (packet_obj) {
            p_connect_ack = (SN_ConnectAck *)packet_obj;
        } else {
            XMEMSET(p_connect_ack, 0, sizeof(SN_ConnectAck));
        }
        p_connect_ack->return_code = client->rx_buf[client->packet.buf_len - 1];

        break;
    }
    case SN_MSG_TYPE_REGACK: {
        /* Decode register ack */
        SN_RegAck regack_s, *p_regack = &regack_s;
        if (packet_obj) {
            p_regack = (SN_RegAck *)packet_obj;
        } else {
            XMEMSET(p_regack, 0, sizeof(SN_RegAck));
        }

        rc = SN_Decode_RegAck(client->rx_buf, client->packet.buf_len, p_regack);

        if (rc > 0) {
            packet_id = p_regack->packet_id;
        }

        break;
    }
    case SN_MSG_TYPE_PUBLISH: {
        SN_Publish pub, *p_pub = &pub;
        if (packet_obj) {
            p_pub = (SN_Publish *)packet_obj;
        } else {
            XMEMSET(p_pub, 0, sizeof(SN_Publish));
        }

        /* Decode publish message */
        rc = SN_Decode_Publish(client->rx_buf, client->packet.buf_len,
                               p_pub);
        if (rc <= 0) {
            return rc;
        }

        /* Issue callback for new message */
        if (client->msg_cb) {
            /* if using the temp publish message buffer,
               then populate message context with client context */
            if (&client->msgSN.publish == p_pub) {
                p_pub->ctx = client->ctx;
            }
            rc = client->msg_cb(client, (MqttMessage *)p_pub, 1, 1);
            if (rc != MQTT_CODE_SUCCESS) {
                return rc;
            };
        }

        /* Handle Qos */
        if (p_pub->qos > MQTT_QOS_0) {
            SN_MsgType type;

            packet_id = p_pub->packet_id;

            /* Determine packet type to write */
            type = (p_pub->qos == MQTT_QOS_1) ?
                   SN_MSG_TYPE_PUBACK :
                   SN_MSG_TYPE_PUBREC;
            p_pub->resp.packet_id = packet_id;

            /* Encode publish response */
            rc = SN_Encode_PublishResp(client->tx_buf,
                                       client->tx_buf_len, type, &p_pub->resp);
            if (rc <= 0) {
                return rc;
            }
            client->packet.buf_len = rc;

            /* Send packet */
            rc = MqttPacket_Write(client, client->tx_buf,
                                  client->packet.buf_len);
        }
        break;
    }
    case SN_MSG_TYPE_PUBACK:
    case SN_MSG_TYPE_PUBCOMP:
    case SN_MSG_TYPE_PUBREC:
    case SN_MSG_TYPE_PUBREL: {
        SN_PublishResp publish_resp;
        XMEMSET(&publish_resp, 0, sizeof(SN_PublishResp));

        /* Decode publish response message */
        rc = SN_Decode_PublishResp(client->rx_buf, client->packet.buf_len,
                                   packet_type, &publish_resp);
        if (rc <= 0) {
            return rc;
        }
        packet_id = publish_resp.packet_id;

        /* If Qos then send response */
        if (packet_type == SN_MSG_TYPE_PUBREC ||
            packet_type == SN_MSG_TYPE_PUBREL) {

            byte resp_type = (packet_type == SN_MSG_TYPE_PUBREC) ?
                             SN_MSG_TYPE_PUBREL : SN_MSG_TYPE_PUBCOMP;

            /* Encode publish response */
            publish_resp.packet_id = packet_id;
            rc = SN_Encode_PublishResp(client->tx_buf,
                                       client->tx_buf_len, resp_type, &publish_resp);
            if (rc <= 0) {
                return rc;
            }
            client->packet.buf_len = rc;

            /* Send packet */
            rc = MqttPacket_Write(client, client->tx_buf,
                                  client->packet.buf_len);
        }
        break;
    }
    case SN_MSG_TYPE_SUBACK: {
        /* Decode subscribe ack */
        SN_SubAck subscribe_ack, *p_subscribe_ack = &subscribe_ack;
        if (packet_obj) {
            p_subscribe_ack = (SN_SubAck *)packet_obj;
        } else {
            XMEMSET(p_subscribe_ack, 0, sizeof(SN_SubAck));
        }

        rc = SN_Decode_SubscribeAck(client->rx_buf, client->packet.buf_len,
                                    p_subscribe_ack);
        if (rc <= 0) {
            return rc;
        }
        packet_id = p_subscribe_ack->packet_id;

        break;
    }
    case SN_MSG_TYPE_UNSUBACK: {
        /* Decode unsubscribe ack */
        SN_UnsubscribeAck unsubscribe_ack, *p_unsubscribe_ack = &unsubscribe_ack;
        if (packet_obj) {
            p_unsubscribe_ack = (SN_UnsubscribeAck *)packet_obj;
        } else {
            XMEMSET(p_unsubscribe_ack, 0, sizeof(SN_UnsubscribeAck));
        }
        rc = SN_Decode_UnsubscribeAck(client->rx_buf,
                                      client->packet.buf_len, p_unsubscribe_ack);
        if (rc <= 0) {
            return rc;
        }
        packet_id = p_unsubscribe_ack->packet_id;

        break;
    }
    case SN_MSG_TYPE_PING_RESP: {
        /* Decode ping */
        rc = SN_Decode_Ping(client->rx_buf, client->packet.buf_len);
        break;
    }
    default: {
        /* Other types are server side only, ignore */
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("SN_Client_HandlePacket: Invalid client packet type %u!",
               packet_type);
#endif
        break;
    }
    } /* switch (packet_type) */

    if (ppacket_id) {
        *ppacket_id = packet_id;
    }

    return rc;
}

static int SN_Client_WaitType(MqttClient *client, void *packet_obj,
                              byte wait_type, word16 wait_packet_id, int timeout_ms)
{
    int rc;
    SN_MsgType packet_type;
    word16 packet_id;
    MqttMsgStat *mms_stat;

    if (client == NULL || packet_obj == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* all packet type structures must have MqttMsgStat at top */
    mms_stat = (MqttMsgStat *)packet_obj;

wait_again:

    /* initialize variables */
    packet_id = 0;
    packet_type = SN_MSG_TYPE_RESERVED;

    switch ((int)*mms_stat) {
    case MQTT_MSG_BEGIN: {
        /* reset the packet state */
        client->packet.stat = MQTT_PK_BEGIN;

        FALL_THROUGH;
    }
    case MQTT_MSG_WAIT: {
        /* Wait for packet */
        rc = SN_Packet_Read(client, client->rx_buf, client->rx_buf_len,
                            timeout_ms);
        if (rc <= 0) {
            return rc;
        }

        *mms_stat = MQTT_MSG_WAIT;
        client->packet.buf_len = rc;

        /* Decode header */
        rc = SN_Decode_Header(client->rx_buf, rc, &packet_type);
        if (rc < 0) {
            return rc;
        }

#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("Read Packet: Len %d, Type %d",
               client->packet.buf_len, packet_type);
#endif

        *mms_stat = MQTT_MSG_READ;

        FALL_THROUGH;
    }

    case MQTT_MSG_READ:
    case MQTT_MSG_READ_PAYLOAD: {
        if (*mms_stat == MQTT_MSG_READ_PAYLOAD) {
            packet_type = SN_MSG_TYPE_PUBLISH;
        }
        rc = SN_Client_HandlePacket(client, packet_type, packet_obj,
                                    &packet_id, timeout_ms);
        if (rc < 0) {
            return rc;
        }
        rc = MQTT_CODE_SUCCESS;

        /* Check for type and packet id */
        if ((wait_type == packet_type) &&
            (wait_packet_id == 0 || wait_packet_id == packet_id)) {
            /* We found the packet type and id */
            break;
        }

        *mms_stat = MQTT_MSG_BEGIN;
        goto wait_again;
    }

#ifdef WOLFMQTT_V5
    case MQTT_MSG_AUTH:
#endif
    case MQTT_MSG_WRITE:
    default: {
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("SN_Client_WaitType: Invalid state %d!", *mms_stat);
#endif
        rc = MQTT_CODE_ERROR_STAT;
        break;
    }
    } /* switch (msg->stat) */

    /* reset state */
    *mms_stat = MQTT_MSG_BEGIN;

    return rc;
}

/* Public Functions */
int SN_Client_SearchGW(MqttClient *client, SN_SearchGw *search)
{
    int rc, len = 0;

    /* Validate required arguments */
    if (client == NULL || search == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (search->stat == MQTT_MSG_BEGIN) {

        /* Encode the search packet */
        rc = SN_Encode_SearchGW(client->tx_buf, client->tx_buf_len,
                                search->radius);
        if (rc <= 0) {
            return rc;
        }
        len = rc;

        /* Send search for gateway packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc != len) {
            return rc;
        }
        search->stat = MQTT_MSG_WAIT;
    }

    /* Wait for gateway info packet */
    rc = SN_Client_WaitType(client, &search->gwInfo, SN_MSG_TYPE_GWINFO, 0,
                            client->cmd_timeout_ms);

    return rc;
}

int SN_Client_Connect(MqttClient *client, SN_Connect *mc_connect)
{
    int rc = 0, len = 0;

    /* Validate required arguments */
    if ((client == NULL) || (mc_connect == NULL)) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (mc_connect->stat == MQTT_MSG_BEGIN) {

        /* Encode the connect packet */
        rc = SN_Encode_Connect(client->tx_buf, client->tx_buf_len, mc_connect);
        if (rc <= 0) {
            return rc;
        }
        len = rc;

        /* Send connect packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc == len) {
            rc = 0;
            mc_connect->stat = MQTT_MSG_WAIT;
        } else {
            if (rc == 0) {
                /* Some other error */
                rc = -1;
            }
        }
    }

    if ((rc == 0) && (mc_connect->enable_lwt != 0)) {
        /* If the will is enabled, then the gateway requests the topic and
           message in separate packets. */
        rc = SN_Client_Will(client, &mc_connect->will);
    }

    if (rc == 0) {
        mc_connect->enable_lwt = 0;

        /* Wait for connect ack packet */
        rc = SN_Client_WaitType(client, &mc_connect->ack,
                                SN_MSG_TYPE_CONNACK, 0, client->cmd_timeout_ms);
    }

    return rc;
}

int SN_Client_Will(MqttClient *client, SN_Will *will)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Wait for Will Topic Request packet */
    rc = SN_Client_WaitType(client, will,
                            SN_MSG_TYPE_WILLTOPICREQ, 0, client->cmd_timeout_ms);
    if (rc == 0) {

        /* Encode Will Topic */
        len = rc = SN_Encode_WillTopic(client->tx_buf, client->tx_buf_len, will);
        if (rc > 0) {

            /* Send Will Topic packet */
            rc = MqttPacket_Write(client, client->tx_buf, len);
            if ((will != NULL) && (rc == len)) {

                /* Wait for Will Message Request */
                rc = SN_Client_WaitType(client, &will->msgResp,
                                        SN_MSG_TYPE_WILLMSGREQ, 0, client->cmd_timeout_ms);

                if (rc == 0) {

                    /* Encode Will Message */
                    len = rc = SN_Encode_WillMsg(client->tx_buf,
                                                 client->tx_buf_len, will);
                    if (rc > 0) {

                        /* Send Will Topic packet */
                        rc = MqttPacket_Write(client, client->tx_buf, len);
                        if (rc == len) {
                            rc = 0;
                        }
                    }
                }
            }
        }
    }

    return rc;

}

int SN_Client_WillTopicUpdate(MqttClient *client, SN_Will *will)
{
    int rc = 0, len = 0;

    /* Validate required arguments */
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Encode Will Topic Update */
    len = rc = SN_Encode_WillTopicUpdate(client->tx_buf, client->tx_buf_len, will);
    if (rc > 0) {

        /* Send Will Topic Update packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if ((will != NULL) && (rc == len)) {

            if (will != NULL) {
                /* Wait for Will Topic Update Response packet */
                rc = SN_Client_WaitType(client, &will->topicResp,
                                        SN_MSG_TYPE_WILLTOPICREQ, 0, client->cmd_timeout_ms);
            }
        }
    }

    return rc;

}

int SN_Client_WillMsgUpdate(MqttClient *client, SN_Will *will)
{
    int rc = 0, len = 0;

    /* Validate required arguments */
    if ((client == NULL) || (will == NULL)) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Encode Will Message Update */
    len = rc = SN_Encode_WillMsgUpdate(client->tx_buf, client->tx_buf_len, will);
    if (rc > 0) {

        /* Send Will Message Update packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc == len) {
            /* Wait for Will Message Update Response packet */
            rc = SN_Client_WaitType(client, &will->msgUpd,
                                    SN_MSG_TYPE_WILLMSGRESP, 0, client->cmd_timeout_ms);
        }
    }

    return rc;

}

int SN_Client_Subscribe(MqttClient *client, SN_Subscribe *subscribe)
{
    int rc = -1, len;

    /* Validate required arguments */
    if (client == NULL || subscribe == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (subscribe->stat == MQTT_MSG_BEGIN) {
        /* Encode the subscribe packet */
        rc = SN_Encode_Subscribe(client->tx_buf, client->tx_buf_len,
                                 subscribe);
        if (rc <= 0) {
            return rc;
        }
        len = rc;

        /* Send subscribe packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc != len) {
            return rc;
        }

        subscribe->stat = MQTT_MSG_WAIT;
    }

    /* Wait for subscribe ack packet */
    rc = SN_Client_WaitType(client, &subscribe->subAck,
                            SN_MSG_TYPE_SUBACK, subscribe->packet_id, client->cmd_timeout_ms);

    return rc;
}

int SN_Client_Publish(MqttClient *client, SN_Publish *publish)
{
    int rc = MQTT_CODE_SUCCESS;

    /* Validate required arguments */
    if (client == NULL || publish == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    switch ((int)publish->stat) {
    case MQTT_MSG_BEGIN: {
        /* Encode the publish packet */
        rc = SN_Encode_Publish(client->tx_buf, client->tx_buf_len,
                               publish);
        if (rc <= 0) {
            return rc;
        }

        client->write.len = rc;
        publish->buffer_pos = 0;

        FALL_THROUGH;
    }
    case MQTT_MSG_WRITE: {
        publish->stat = MQTT_MSG_WRITE;

        /* Send packet and payload */
        rc = MqttPacket_Write(client, client->tx_buf,
                              client->write.len);
        if (rc < 0) {
            return rc;
        }

        if (rc == client->write.len) {
            rc = MQTT_CODE_SUCCESS;
        } else {
            rc = -1;
        }

        /* if not expecting a reply, the reset state and exit */
        if (publish->qos == MQTT_QOS_0) {
            publish->stat = MQTT_MSG_BEGIN;
            break;
        }

        FALL_THROUGH;
    }

    case MQTT_MSG_WAIT: {
        publish->stat = MQTT_MSG_WAIT;

        /* Handle QoS */
        if (publish->qos > MQTT_QOS_0) {

            XMEMSET(&publish->resp, 0, sizeof(SN_PublishResp));

            /* Determine packet type to wait for */
            SN_MsgType type = (publish->qos == MQTT_QOS_1) ?
                              SN_MSG_TYPE_PUBACK :
                              SN_MSG_TYPE_PUBCOMP;

            /* Wait for publish response packet */
            rc = SN_Client_WaitType(client, &publish->resp,
                                    type, publish->packet_id, client->cmd_timeout_ms);

            publish->return_code = publish->resp.return_code;
        }

        break;
    }

#ifdef WOLFMQTT_V5
    case MQTT_MSG_AUTH:
#endif
    case MQTT_MSG_READ:
    case MQTT_MSG_READ_PAYLOAD:
#ifdef WOLFMQTT_DEBUG_CLIENT
        PRINTF("SN_Client_Publish: Invalid state %d!",
               publish->stat);
#endif
        rc = MQTT_CODE_ERROR_STAT;
        break;
    } /* switch (publish->stat) */

    return rc;
}


int SN_Client_Unsubscribe(MqttClient *client, SN_Unsubscribe *unsubscribe)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL || unsubscribe == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (unsubscribe->stat == MQTT_MSG_BEGIN) {
        /* Encode the subscribe packet */
        rc = SN_Encode_Unsubscribe(client->tx_buf, client->tx_buf_len,
                                   unsubscribe);
        if (rc <= 0) {
            return rc;
        }
        len = rc;

        /* Send unsubscribe packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc != len) {
            return rc;
        }

        /* Clear local structure */
        XMEMSET(&unsubscribe->ack, 0, sizeof(SN_UnsubscribeAck));

        unsubscribe->stat = MQTT_MSG_WAIT;
    }

    /* Wait for unsubscribe ack packet */
    rc = SN_Client_WaitType(client, &unsubscribe->ack,
                            SN_MSG_TYPE_UNSUBACK, unsubscribe->packet_id,
                            client->cmd_timeout_ms);

    return rc;
}

int SN_Client_Register(MqttClient *client, SN_Register *regist)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL || regist == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (regist->stat == MQTT_MSG_BEGIN) {
        /* Encode the register packet */
        rc = SN_Encode_Register(client->tx_buf, client->tx_buf_len, regist);
        if (rc <= 0) {
            return rc;
        }
        len = rc;

        /* Send packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc != len) {
            return rc;
        }

        regist->stat = MQTT_MSG_WAIT;
    }

    /* Wait for register acknowledge packet */
    rc = SN_Client_WaitType(client, &regist->regack,
                            SN_MSG_TYPE_REGACK, regist->packet_id, client->cmd_timeout_ms);

    return rc;
}

int SN_Client_Ping(MqttClient *client, SN_PingReq *ping)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    if (ping == NULL) {
        /* use client global */
        ping = &client->msgSN.pingReq;
    }

    if (ping->stat == MQTT_MSG_BEGIN) {
        /* Encode the ping packet */
        rc = SN_Encode_Ping(client->tx_buf, client->tx_buf_len, ping);
        if (rc <= 0) {
            return rc;
        }
        len = rc;

        /* Send ping req packet */
        rc = MqttPacket_Write(client, client->tx_buf, len);
        if (rc != len) {
            return rc;
        }

        ping->stat = MQTT_MSG_WAIT;
    }

    /* Wait for ping resp packet */
    rc = SN_Client_WaitType(client, ping,
                            SN_MSG_TYPE_PING_RESP, 0, client->cmd_timeout_ms);

    return rc;
}

int SN_Client_Disconnect(MqttClient *client)
{
    return SN_Client_Disconnect_ex(client, NULL);
}

int SN_Client_Disconnect_ex(MqttClient *client, SN_Disconnect *disconnect)
{
    int rc, len;

    /* Validate required arguments */
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }

    /* Encode the disconnect packet */
    rc = SN_Encode_Disconnect(client->tx_buf, client->tx_buf_len, disconnect);
    if (rc <= 0) {
        return rc;
    }
    len = rc;

    /* Send disconnect packet */
    rc = MqttPacket_Write(client, client->tx_buf, len);
    if (rc != len) {
        return rc;
    }

    /* No response for MQTT disconnect packet */

    return MQTT_CODE_SUCCESS;
}

int SN_Client_WaitMessage_ex(MqttClient *client, SN_Object *packet_obj, int timeout_ms)
{
    return SN_Client_WaitType(client, packet_obj,
                              MQTT_PACKET_TYPE_ANY, 0, timeout_ms);
}

int SN_Client_WaitMessage(MqttClient *client, int timeout_ms)
{
    if (client == NULL) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    return SN_Client_WaitMessage_ex(client, &client->msgSN, timeout_ms);
}

#endif /* defined WOLFMQTT_SN */






/**************************兼容处理START********************************/

extern int MqttClientNet_Init(MqttNet *net);
void NewNetwork(Network *n)
{
    mqttCtx.use_tls = 0;
    MqttClientNet_Init(n);

}

void NetWorkSetTLS(Network *n)
{
    mqttCtx.use_tls = 1;
    MqttClientNet_Init(n);

}

void NetWorkSetTLS_key(Network *n, const char *cas_pem, int cas_pem_len, const char *cli_pem, int cli_pem_len, const char *pkey, int pkey_len)
{
    n->root_ca          = cas_pem;
    n->device_cert      = cli_pem;
    n->device_priv_key 	= pkey;

    n->ca_len 	    = cas_pem_len;
    n->cert_len 	= cli_pem_len;
    n->key_len	   	= pkey_len;
}

void SetNetworkCb(Network *n, int (*cb_func)(enum sock_api_msg_type, void *), void *priv)
{
    return;
}

void SetNetworkRecvTimeout(Network *n, int timeout_ms)
{
    return;
}

void SetNetworkSendTimeout(Network *n, int timeout_ms)
{
    return;
}


int ConnectNetwork(Network *n, char *addr, int port)
{
    mqttCtx.host = addr;
    mqttCtx.port = port;
    return 0;
}


static int mqtt_message_cb(MqttClient *client, MqttMessage *msg,
                           byte msg_new, byte msg_done)
{
    MQTTCtx *mqttCtx = (MQTTCtx *)client->ctx;
    byte buf[PRINT_BUFFER_SIZE + 1];
    word32 len;

    struct list_head *pos = NULL;
    struct subscribe_topic *subscribe_topic = NULL;
    int json_str = 0;

    (void)mqttCtx;

    if (msg_new) {
        /* Determine min size to dump */
        len = msg->topic_name_len;
        if (len > PRINT_BUFFER_SIZE) {
            len = PRINT_BUFFER_SIZE;
        }
        XMEMCPY(buf, msg->topic_name, len);
        buf[len] = '\0'; /* Make sure its null terminated */

        /* Print incoming message */
        PRINTF("MQTT Message: Topic %s, Qos %d, Len %u",
               buf, msg->qos, msg->total_len);
    }

    /* Print message payload */
    len = msg->buffer_len;
    if (len > PRINT_BUFFER_SIZE) {
        len = PRINT_BUFFER_SIZE;
    }
    XMEMCPY(buf, msg->buffer, len);
    buf[len] = '\0'; /* Make sure its null terminated */
    PRINTF("Payload (%d - %d): %s",
           msg->buffer_pos, msg->buffer_pos + len, buf);

    if (msg_done) {
        PRINTF("MQTT Message: Done");

        /*********************************/

        MessageData md = {0};
        MQTTMessage message = {0};
        message.payload = (void *)msg->buffer;
        message.payloadlen = msg->buffer_len;
        md.message = &message;
        list_for_each(pos, &client->topic_head.entry) {

            subscribe_topic = list_entry(pos, struct subscribe_topic, entry);
            json_str = strchr(subscribe_topic->topic, '#') - subscribe_topic->topic;
            json_str = json_str ? json_str : msg->topic_name_len;
            if (!strncmp(subscribe_topic->topic, msg->topic_name, json_str)) {
                subscribe_topic->cb(&md);
                malloc_stats();
            }
        }

        /*********************************/

    }

    /* Return negative to terminate publish processing */
    return MQTT_CODE_SUCCESS;
}

#ifdef ENABLE_MQTT_TLS
static int mqtt_aws_tls_verify_cb(int preverify, WOLFSSL_X509_STORE_CTX *store)
{
    char buffer[WOLFSSL_MAX_ERROR_SZ];

    PRINTF("MQTT TLS Verify Callback: PreVerify %d, Error %d (%s)", preverify,
           store->error, store->error != 0 ?
           wolfSSL_ERR_error_string(store->error, buffer) : "none");
    PRINTF("  Subject's domain name is %s", store->domain);

    if (store->error != 0) {
        /* Allowing to continue */
        /* Should check certificate and return 0 if not okay */
        PRINTF("  Allowing cert anyways");
    }

    return 1;
}

// Use this callback to setup TLS certificates and verify callbacks
static int mqtt_aws_tls_cb(MqttClient *client)
{
    int rc = WOLFSSL_FAILURE;

    client->tls.ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (client->tls.ctx) {
        wolfSSL_CTX_set_verify(client->tls.ctx, WOLFSSL_VERIFY_PEER,
                               mqtt_aws_tls_verify_cb);

        /* Load CA certificate buffer */
        rc = wolfSSL_CTX_load_verify_buffer(client->tls.ctx,                                            client->net->root_ca, client->net->ca_len, WOLFSSL_FILETYPE_PEM);

        /* Load Client Cert */
        if (rc == WOLFSSL_SUCCESS) {
            rc = wolfSSL_CTX_use_certificate_buffer(client->tls.ctx,                                                      client->net->device_cert, client->net->cert_len, WOLFSSL_FILETYPE_PEM);
        }

        /* Load Private Key */
        if (rc == WOLFSSL_SUCCESS) {
            rc = wolfSSL_CTX_use_PrivateKey_buffer(client->tls.ctx,                                                     client->net->device_priv_key, client->net->key_len, WOLFSSL_FILETYPE_PEM);
        }
    }

    PRINTF("MQTT TLS Setup (%d)", rc);

    return rc;
}
#endif

void MQTTClient(Client *c, Network *network, unsigned int command_timeout_ms, unsigned char *buf, size_t buf_size, unsigned char *readbuf, size_t readbuf_size)
{
    int rc;
    rc = MqttClient_Init(c, network, mqtt_message_cb, buf, buf_size, readbuf, readbuf_size, command_timeout_ms);
    if (rc != MQTT_CODE_SUCCESS) {
        printf("MqttClient_Init rc = %d\n", rc);
        goto exit;
    }
#ifdef ENABLE_MQTT_TLS
    rc = MqttClient_NetConnect(c, mqttCtx.host, mqttCtx.port, DEFAULT_CON_TIMEOUT_MS, mqttCtx.use_tls, mqtt_aws_tls_cb);
#else
    rc = MqttClient_NetConnect(c, mqttCtx.host, mqttCtx.port, DEFAULT_CON_TIMEOUT_MS, mqttCtx.use_tls, NULL);
#endif
    if (rc != MQTT_CODE_SUCCESS) {
        printf("MqttClient_NetConnect rc = %d\n", rc);
        goto exit;
    }
    return;

exit:
    MqttClient_DeInit(c);

}

int MQTTConnect(Client *c, MQTTPacket_connectData *data)
{
    int rc = 0;
    if ((c->flags & MQTT_CLIENT_FLAG_IS_CONNECTED) == 0) {
        return -1;
    }
#ifdef WOLFMQTT_V5
    c->protocol_level = data.MQTTVersion;
#endif // WOLFMQTT_V5

    MqttConnect connect = {0};
//    connect.protocol_level  = data.MQTTVersion;
    connect.client_id       = data->clientID.cstring;
    connect.username        = data->username.cstring;
    connect.password        = data->password.cstring;
    connect.keep_alive_sec  = data->keepAliveInterval;
    connect.clean_session   = data->cleansession;
    connect.enable_lwt      = data->willFlag;

    rc = MqttClient_Connect(c, &connect);
    if (rc == 0) {
        INIT_LIST_HEAD(&c->topic_head.entry);
    }

    return rc;

}


int MQTTDisconnect(Client *c)
{
    int rc = -1;
    rc = MqttClient_Disconnect(c);
    if (rc == 0) {
        rc = MqttSocket_Disconnect(c);
    } else {
        printf("MqttClient_Disconnect failed----rc = %d\r\n", rc);
    }
    return rc;
}



static int packet_id = 0;
//static int subscribe_id;    //订阅主题函数定时器id
int MQTTSubscribe(Client *c, const char *topicFilter, enum _MqttQoS qos, messageHandler messageHandler)
{
    struct list_head *pos = NULL;
    struct subscribe_topic *subscribe_topic = NULL;
    MqttSubscribe subscribe = {0};
    MqttTopic topics = {0};
    topics.topic_filter = topicFilter;
    topics.qos = qos;

    subscribe.packet_id = ++packet_id;
    subscribe.topic_count = 1;
    subscribe.topics = &topics;


    subscribe_topic = (struct subscribe_topic *)calloc(1, sizeof(struct subscribe_topic));
    if (!subscribe_topic) {
        printf("subscribe_topic malloc fialed");
        return -1;
    }
    subscribe_topic->topic  = topicFilter;
    subscribe_topic->cb     = messageHandler;

    list_add_tail(&subscribe_topic->entry, &c->topic_head.entry);

    return MqttClient_Subscribe(c, &subscribe);
}

int MQTTUnsubscribe(Client *c, const char *topicFilter)
{
    struct list_head *pos = NULL;
    struct subscribe_topic *subscribe_topic = NULL;
    MqttUnsubscribe unsubscribe = {0};

    MqttTopic topics = {0};
    topics.topic_filter = topicFilter;

    unsubscribe.packet_id = ++packet_id;
    unsubscribe.topic_count = 1;
    unsubscribe.topics = &topics;

    list_for_each(pos, &c->topic_head.entry) {
        subscribe_topic = list_entry(pos, struct subscribe_topic, entry);
        if (!strcmp(subscribe_topic->topic, topicFilter)) {
            list_del(&subscribe_topic->entry);
        }
    }


    return MqttClient_Unsubscribe(c, &unsubscribe);
}


int MQTTPublish(Client *c, const char *topicName, MQTTMessage *message)
{
    MqttPublish publish = {0};

    publish.qos = message->qos;
    publish.retain = message->retained;
    publish.duplicate = message->dup;
    publish.buffer = message->payload;
    publish.total_len = message->payloadlen;
    publish.topic_name = topicName;

    publish.packet_id = message->id;        //目前不知道该句效果

    return MqttClient_Publish(c, &publish);

}


int MQTTYield(Client *c, int timeout_ms)    //timeout_ms目前不知道怎么用
{
    int rc = 0;
    rc = MqttClient_Ping(c);
    return rc;
}















/**************************兼容处理END********************************/







