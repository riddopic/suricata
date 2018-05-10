/* Copyright (C) 2007-2010 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 */

#ifndef __STREAM_TCP_PRIVATE_H__
#define __STREAM_TCP_PRIVATE_H__

#include "decode.h"
#include "util-pool.h"
#include "util-pool-thread.h"
#include "util-streaming-buffer.h"

#define STREAMTCP_QUEUE_FLAG_TS     0x01
#define STREAMTCP_QUEUE_FLAG_WS     0x02
#define STREAMTCP_QUEUE_FLAG_SACK   0x04

/** currently only SYN/ACK */
typedef struct TcpStateQueue_ {
    uint8_t flags;
    uint8_t wscale;
    uint16_t win;
    uint32_t seq;
    uint32_t ack;
    uint32_t ts;
    uint32_t pkt_ts;
    struct TcpStateQueue_ *next;
} TcpStateQueue;

typedef struct StreamTcpSackRecord_ {
    uint32_t le;    /**< left edge, host order */
    uint32_t re;    /**< right edge, host order */
    struct StreamTcpSackRecord_ *next;
} StreamTcpSackRecord;

typedef struct TcpSegment_ {
    PoolThreadReserved res;
    uint16_t payload_len;       /**< actual size of the payload */
    uint32_t seq;
    StreamingBufferSegment sbseg;
    struct TcpSegment_ *next;
    struct TcpSegment_ *prev;
} TcpSegment;

#define TCP_SEG_LEN(seg)        (seg)->payload_len
#define TCP_SEG_OFFSET(seg)     (seg)->sbseg.stream_offset

#define SEG_SEQ_RIGHT_EDGE(seg) ((seg)->seq + TCP_SEG_LEN((seg)))

typedef struct TcpStream_ {
    uint16_t flags:12;              /**< Flag specific to the stream e.g. Timestamp */
    /* coccinelle: TcpStream:flags:STREAMTCP_STREAM_FLAG_ */
    uint16_t wscale:4;              /**< wscale setting in this direction, 4 bits as max val is 15 */
    uint8_t os_policy;              /**< target based OS policy used for reassembly and handling packets*/
    uint8_t tcp_flags;              /**< TCP flags seen */

    uint32_t isn;                   /**< initial sequence number */
    uint32_t next_seq;              /**< next expected sequence number */
    uint32_t last_ack;              /**< last ack'd sequence number in this stream */
    uint32_t next_win;              /**< next max seq within window */
    uint32_t window;                /**< current window setting, after wscale is applied */

    uint32_t last_ts;               /**< Time stamp (TSVAL) of the last seen packet for this stream*/
    uint32_t last_pkt_ts;           /**< Time of last seen packet for this stream (needed for PAWS update)
                                         This will be used to validate the last_ts, when connection has been idle for
                                         longer time.(RFC 1323)*/
    /* reassembly */
    uint32_t base_seq;              /**< seq where we are left with reassebly. Matches STREAM_BASE_OFFSET below. */

    uint32_t app_progress_rel;      /**< app-layer progress relative to STREAM_BASE_OFFSET */
    uint32_t raw_progress_rel;      /**< raw reassembly progress relative to STREAM_BASE_OFFSET */
    uint32_t log_progress_rel;      /**< streaming logger progress relative to STREAM_BASE_OFFSET */

    StreamingBuffer sb;

    TcpSegment *seg_list;           /**< list of TCP segments that are not yet (fully) used in reassembly */
    TcpSegment *seg_list_tail;      /**< Last segment in the reassembled stream seg list*/

    StreamTcpSackRecord *sack_head; /**< head of list of SACK records */
    StreamTcpSackRecord *sack_tail; /**< tail of list of SACK records */
} TcpStream;

#define STREAM_BASE_OFFSET(stream)  ((stream)->sb.stream_offset)
#define STREAM_APP_PROGRESS(stream) (STREAM_BASE_OFFSET((stream)) + (stream)->app_progress_rel)
#define STREAM_RAW_PROGRESS(stream) (STREAM_BASE_OFFSET((stream)) + (stream)->raw_progress_rel)
#define STREAM_LOG_PROGRESS(stream) (STREAM_BASE_OFFSET((stream)) + (stream)->log_progress_rel)

/* from /usr/include/netinet/tcp.h */
enum
{
    TCP_NONE,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_LAST_ACK,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_CLOSED,
};

/*
 * Per SESSION flags
 */

/** Flag for mid stream session */
#define STREAMTCP_FLAG_MIDSTREAM                    0x0001
/** Flag for mid stream established session */
#define STREAMTCP_FLAG_MIDSTREAM_ESTABLISHED        0x0002
/** Flag for mid session when syn/ack is received */
#define STREAMTCP_FLAG_MIDSTREAM_SYNACK             0x0004
/** Flag for TCP Timestamp option */
#define STREAMTCP_FLAG_TIMESTAMP                    0x0008
/** Server supports wscale (even though it can be 0) */
#define STREAMTCP_FLAG_SERVER_WSCALE                0x0010
// vacancy
/** Flag to indicate that the session is handling asynchronous stream.*/
#define STREAMTCP_FLAG_ASYNC                        0x0040
/** Flag to indicate we're dealing with 4WHS: SYN, SYN, SYN/ACK, ACK
 * (http://www.breakingpointsystems.com/community/blog/tcp-portals-the-three-way-handshake-is-a-lie) */
#define STREAMTCP_FLAG_4WHS                         0x0080
/** Flag to indicate that this session is possible trying to evade the detection
 *  (http://www.packetstan.com/2010/06/recently-ive-been-on-campaign-to-make.html) */
#define STREAMTCP_FLAG_DETECTION_EVASION_ATTEMPT    0x0100
/** Flag to indicate the client (SYN pkt) permits SACK */
#define STREAMTCP_FLAG_CLIENT_SACKOK                0x0200
/** Flag to indicate both sides of the session permit SACK (SYN + SYN/ACK) */
#define STREAMTCP_FLAG_SACKOK                       0x0400
// vacancy
/** 3WHS confirmed by server -- if suri sees 3whs ACK but server doesn't (pkt
 *  is lost on the way to server), SYN/ACK is retransmitted. If server sends
 *  normal packet we assume 3whs to be completed. Only used for SYN/ACK resend
 *  event. */
#define STREAMTCP_FLAG_3WHS_CONFIRMED               0x1000
/** App Layer tracking/reassembly is disabled */
#define STREAMTCP_FLAG_APP_LAYER_DISABLED           0x2000
/** Stream can be bypass */
#define STREAMTCP_FLAG_BYPASS                       0x4000

/*
 * Per STREAM flags
 */

/** stream is in a gap state */
#define STREAMTCP_STREAM_FLAG_GAP               0x0001
/** Flag to avoid stream reassembly/app layer inspection for the stream */
#define STREAMTCP_STREAM_FLAG_NOREASSEMBLY      0x0002
/** we received a keep alive */
#define STREAMTCP_STREAM_FLAG_KEEPALIVE         0x0004
/** Stream has reached it's reassembly depth, all further packets are ignored */
#define STREAMTCP_STREAM_FLAG_DEPTH_REACHED     0x0008
/** Trigger reassembly next time we need 'raw' */
#define STREAMTCP_STREAM_FLAG_TRIGGER_RAW       0x0010
/** Stream supports TIMESTAMP -- used to set ssn STREAMTCP_FLAG_TIMESTAMP
 *  flag. */
#define STREAMTCP_STREAM_FLAG_TIMESTAMP         0x0020
/** Flag to indicate the zero value of timestamp */
#define STREAMTCP_STREAM_FLAG_ZERO_TIMESTAMP    0x0040
/** App proto detection completed */
#define STREAMTCP_STREAM_FLAG_APPPROTO_DETECTION_COMPLETED 0x0080
/** App proto detection skipped */
#define STREAMTCP_STREAM_FLAG_APPPROTO_DETECTION_SKIPPED 0x0100
/** Raw reassembly disabled for new segments */
#define STREAMTCP_STREAM_FLAG_NEW_RAW_DISABLED 0x0200
/** Raw reassembly disabled completely */
#define STREAMTCP_STREAM_FLAG_DISABLE_RAW 0x400

#define STREAMTCP_STREAM_FLAG_RST_RECV  0x800

/** NOTE: flags field is 12 bits */




#define PAWS_24DAYS         2073600         /**< 24 days in seconds */

#define PKT_IS_IN_RIGHT_DIR(ssn, p)        ((ssn)->flags & STREAMTCP_FLAG_MIDSTREAM_SYNACK ? \
                                            PKT_IS_TOSERVER(p) ? (p)->flowflags &= ~FLOW_PKT_TOSERVER \
                                            (p)->flowflags |= FLOW_PKT_TOCLIENT : (p)->flowflags &= ~FLOW_PKT_TOCLIENT \
                                            (p)->flowflags |= FLOW_PKT_TOSERVER : 0)

/* Macro's for comparing Sequence numbers
 * Page 810 from TCP/IP Illustrated, Volume 2. */
#define SEQ_EQ(a,b)  ((int32_t)((a) - (b)) == 0)
#define SEQ_LT(a,b)  ((int32_t)((a) - (b)) <  0)
#define SEQ_LEQ(a,b) ((int32_t)((a) - (b)) <= 0)
#define SEQ_GT(a,b)  ((int32_t)((a) - (b)) >  0)
#define SEQ_GEQ(a,b) ((int32_t)((a) - (b)) >= 0)

#define STREAMTCP_SET_RA_BASE_SEQ(stream, seq) { \
    do { \
        (stream)->base_seq = (seq) + 1;    \
    } while(0); \
}

#define StreamTcpSetEvent(p, e) { \
    SCLogDebug("setting event %"PRIu8" on pkt %p (%"PRIu64")", (e), p, (p)->pcap_cnt); \
    ENGINE_SET_EVENT((p), (e)); \
}

typedef struct TcpSession_ {
    PoolThreadReserved res;
    uint8_t state:4;                        /**< tcp state from state enum */
    uint8_t pstate:4;                       /**< previous state */
    uint8_t queue_len;                      /**< length of queue list below */
    int8_t data_first_seen_dir;
    /** track all the tcp flags we've seen */
    uint8_t tcp_packet_flags;
    /* coccinelle: TcpSession:flags:STREAMTCP_FLAG */
    uint16_t flags;
    uint32_t reassembly_depth;      /**< reassembly depth for the stream */
    TcpStream server;
    TcpStream client;
    TcpStateQueue *queue;                   /**< list of SYN/ACK candidates */
} TcpSession;

#define StreamTcpSetStreamFlagAppProtoDetectionCompleted(stream) \
    ((stream)->flags |= STREAMTCP_STREAM_FLAG_APPPROTO_DETECTION_COMPLETED)
#define StreamTcpIsSetStreamFlagAppProtoDetectionCompleted(stream) \
    ((stream)->flags & STREAMTCP_STREAM_FLAG_APPPROTO_DETECTION_COMPLETED)
#define StreamTcpResetStreamFlagAppProtoDetectionCompleted(stream) \
    ((stream)->flags &= ~STREAMTCP_STREAM_FLAG_APPPROTO_DETECTION_COMPLETED);
#define StreamTcpDisableAppLayerReassembly(ssn) do { \
        SCLogDebug("setting STREAMTCP_FLAG_APP_LAYER_DISABLED on ssn %p", ssn); \
        ((ssn)->flags |= STREAMTCP_FLAG_APP_LAYER_DISABLED); \
    } while (0);

#endif /* __STREAM_TCP_PRIVATE_H__ */
