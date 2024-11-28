#ifndef __BIG_H__
#define __BIG_H__

#include "typedef.h"

/* Max BIG numbers. */
#define BIG_MAX_NUMS            1
/* Max BIS numbers in a BIG. */
#define BIG_MAX_BIS_NUMS        5

/* BIG operation status. */
typedef enum {
    BIG_OPST_SUCC,                              // Operation is successful.
    BIG_OPST_MEMORY_EXCEEDED,                   // Memory capacity exceeded.
    BIG_OPST_DISALLOWED,                        // Operation is disallowed.
    BIG_OPST_UNKNOWN_IDENTIFIER,                // Unknown link idenitfier.
    BIG_OPST_ALREADY_EXISTS,                    // Link already exists.
    BIG_OPST_LIMIT_EXCEEDED,                    // Links limit exceeded.
} BIG_OPST;

/* BIG events. */
typedef enum {
    // for tramsmitter
    BIG_EVENT_TRANSMITTER_CONNECT,              // Transmitter connect succ.
    BIG_EVENT_TRANSMITTER_DISCONNECT,           // Transmitter disconnect.
    BIG_EVENT_TRANSMITTER_ALIGN,                // Transmitter send align.
    BIG_EVENT_TRANSMITTER_READ_TX_SYNC,         // Transmitter get last tx clk.
    BIG_EVENT_TRANSMITTER_SYNC_DELAY,           // Transmitter sync delay.
    // for receiver
    BIG_EVENT_RECEIVER_CONNECT,                 // Receiver connect succ.
    BIG_EVENT_RECEIVER_DISCONNECT,              // Receiver disconnect.
} BIG_EVENT;

/* BIG handles. */
typedef struct {
    uint8_t         big_hdl;                    // BIG handle.
    uint16_t        bis_hdl[BIG_MAX_BIS_NUMS];  // Max BIS handles in BIG.
    uint32_t        big_sync_delay;             // BIG Sync Delay
} big_hdl_t;

/* BIG ISO stream parameter. */
typedef struct {
    uint32_t        ts;                         // TimeStamp.
    uint16_t        bis_hdl;                    // BIS handle.
} big_stream_param_t;

/* BIG device callback. */
typedef struct {

    /* data callback */
    void (*receive_packet_cb)(const void *const buf, size_t length, void *priv);

    /* padv_data callback */
    void (*receive_padv_data_cb)(const void *const buf, size_t length, u8 big_hdl);

    /* event callback */
    void (*event_cb)(const BIG_EVENT event, void *priv);

} big_callback_t;

/* BIG device parameter. */
typedef struct {
    uint8_t         pair_name[28];  // Pair name.
    big_callback_t  *cb;            // Callback of BIG.
    uint8_t         big_hdl;        // BIG handle.
    uint8_t         num_bis;        // Total number of BISes in the BIG.
    uint8_t         ext_phy;        // Primary   adv phy, 1:1M, 3:Coded.
    uint8_t         enc;            // 0:Unencrypted,1:Encrypted.
    uint8_t         bc[16];         // Broadcast Code, encrypt BIS payloads.

    union {
        struct {
            uint8_t  rtn;           // The number of retransmitted times.
            uint8_t  phy;           // BIS phy, BIT(0):1M, BIT(1):2M, BIT(2):Coded.
            uint8_t  aux_phy;       // Secondary adv phy, 1:1M, 2:2M, 3:Coded.
            uint8_t  packing;       // 0:Sequential, 1:Interleaved.
            uint8_t  framing;       // 0:Unframed,   1:Framed.
            uint16_t max_sdu;       // Max sdu packet size           (uints:octets).
            uint16_t mtl;           // Max transport latency         (uints:ms).
            uint16_t eadv_int_ms;   // Primary/Secondary adv interval(uints:ms).
            uint16_t padv_int_ms;   // Periodic adv interval         (uints:ms).
            uint32_t sdu_int_us;    // Sdu interval                  (uints:us).
            struct {
                uint32_t sync_offset;   // Sync offset                   (uints:us).
                uint32_t tx_delay;      // Tx delay                      (uints:us).
                uint32_t big_offset;    // Big offset                    (uints:us).
                uint8_t  max_pdu;       // Max pdu packet size           (uints:octets).
            } vdr;
        } tx;

        struct {
            uint8_t  bis[BIG_MAX_BIS_NUMS]; // Index of a BIS          (range:1~0x1f)
            uint16_t ext_scan_int;          // Primary scan interval   (uints:slot)
            uint16_t ext_scan_win;          // Primary scan window     (uints:slot)
            uint32_t bsync_to_ms;           // Sync timeout for the BIG(uints:ms).
        } rx;
    };
} big_parameter_t;

/* BIG device ops. */
typedef struct {

    /* BIG init. */
    int (*init)(void);

    /* BIG uninit. */
    int (*uninit)(void);

    /* BIG open. */
    int (*open)(big_parameter_t *param);

    /* BIG close. */
    int (*close)(uint8_t big_hdl);

    /* Set btctrler's sync clock enable. */
    void (*sync_set)(uint16_t bis_hdl, bool sel);

    /* Get current packet's clock. */
    void (*sync_get)(uint16_t bis_hdl, uint16_t *us_1per12, uint32_t *clk_us, uint32_t *evt_cnt);

    /* For transmitter, send SDU packet. */
    int (*send_packet)(const uint8_t *packet, size_t size, big_stream_param_t *param);

    /* Get current btctrler's clock. */
    void (*get_ble_clk)(uint32_t *clk);

    /* For transmitter, get bis last sent packet's clock. */
    int (*get_tx_sync)(uint16_t bis_hdl);

    /* For transmitter, set padv data. */
    void (*padv_set_data)(void *p, uint8_t *data, size_t size);
} big_ops_t;

/* Big api ops for upper layer. */
extern const big_ops_t big_tx_ops;
extern const big_ops_t big_rx_ops;

#endif /* __BIG_H__ */
