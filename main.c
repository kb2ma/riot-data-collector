/*
 * Copyright (C) 2017 Ken Bannister <kb2ma@runbox.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       Collect jc42 sensor readings and send to Datahead server
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "saul_reg.h"
#include "xtimer.h"
#include "fmt.h"
#include "net/gcoap.h"

#define DATAHEAD_HELLO_PATH   ("/dh/lo")
#define DATAHEAD_TEMP_PATH    ("/dh/tmp")

static void _hello_handler(unsigned req_state, coap_pkt_t* pdu);
static int _init_datahead(void);
static void _run_sensor_loop(void);
static ssize_t _temp_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len);

static saul_reg_t* _saul_dev;

/* CoAP resources */
static const coap_resource_t _resources[] = {
    { DATAHEAD_TEMP_PATH, COAP_GET, _temp_handler },
};
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};


/* Handles 'hello' response from Datahead server. */
static void _hello_handler(unsigned req_state, coap_pkt_t* pdu)
{
    if (req_state == GCOAP_MEMO_TIMEOUT) {
        printf("Timeout on 'hello' response; msg ID %02u\n", coap_get_id(pdu));
    }
    else if (req_state == GCOAP_MEMO_ERR) {
        puts("Error in 'hello' response");
    }
    else {
        puts("'hello' response OK; wait for Observe request");
    }
}

/*
 * Server callback for /dh/tmp request. */
static ssize_t _temp_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len)
{
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);

    size_t payload_len = fmt_u16_dec((char *)pdu->payload, 0);

    return gcoap_finish(pdu, payload_len, COAP_FORMAT_TEXT);
}

/* Sends a 'hello' message to the Datahead server. */
static int _init_datahead(void)
{
#if defined(DATAHEAD_ADDR) && defined(DATAHEAD_PORT)
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    size_t len;
    sock_udp_ep_t server_sock;
    ipv6_addr_t addr;

    len = gcoap_request(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE, COAP_METHOD_POST,
                        DATAHEAD_HELLO_PATH);

    server_sock.family = AF_INET6;
    server_sock.netif  = SOCK_ADDR_ANY_NETIF;

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, DATAHEAD_ADDR) == NULL) {
        puts("Unable to parse datahead address");
        return 0;
    }
    memcpy(&server_sock.addr.ipv6[0], &addr.u8[0], sizeof(addr.u8));

    /* parse port */
    server_sock.port = (uint16_t)atoi(DATAHEAD_PORT);
    if (DATAHEAD_PORT == 0) {
        puts("Unable to parse datahead port");
        return 0;
    }

    return gcoap_req_send2(buf, len, &server_sock, _hello_handler);
#else
#error Datahead address or port macro not defined
    return 0;
#endif
}

/* Reads the sensor in an infinite loop. */
static void _run_sensor_loop(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    size_t len, payload_len;

    while (1) {
        phydat_t phy;

#ifdef BOARD_NATIVE        
        phy.val[0] = 0;
        int res = 1;
#else
        int res = saul_reg_read(_saul_dev, &phy);
#endif

        if (res) {
            printf("temperature: %d.%02d C\n", phy.val[0] / 100, phy.val[0] % 100);

            res = gcoap_obs_init(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE, &_resources[0]);
            if (res == GCOAP_OBS_INIT_OK) {
                payload_len = fmt_u16_dec((char *)pdu.payload, phy.val[0]);
                len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_TEXT);
                gcoap_obs_send(&buf[0], len, &_resources[0]);
            }
        }
        else {
            printf("Sensor read failure: %d\n", res);
        }

        xtimer_usleep(20 * US_PER_SEC);
    }
}

int main(void)
{
    puts("Data collector initializing...");

    _saul_dev = saul_reg_find_name("jc42");
    if (_saul_dev == NULL) {
#ifdef BOARD_NATIVE
        puts("Ignoring sensor init on native");
#else
        puts("Can't find jc42 sensor");
        return 1;
#endif
    }

    gcoap_register_listener(&_listener);
    if (!_init_datahead()) {
        return 1;
    }

    _run_sensor_loop();

    return 0;
}
