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
#include "net/gnrc/ipv6/nc.h"
#include "net/gnrc/ipv6/netif.h"
#ifdef NET_ROUTER
#include "net/gnrc/rpl.h"
#endif
#include "net/gcoap.h"

#define _NETWORK_ADDR_MAXLEN  (60)
#define _DEFAULT_PREFIX_LEN   (64)

#define DATAHEAD_HELLO_PATH   ("/dh/lo")
#define DATAHEAD_TEMP_PATH    ("/dh/tmp")

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
static void _hello_handler(unsigned req_state, coap_pkt_t* pdu,
                           sock_udp_ep_t *remote)
{
    (void)remote;

    if (req_state == GCOAP_MEMO_TIMEOUT) {
        printf("Timeout on 'hello' response; msg ID %02u\n", coap_get_id(pdu));
    }
    else if (req_state == GCOAP_MEMO_ERR) {
        puts("Error in 'hello' response");
    }
    else {
        puts("'hello' response OK");
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

#ifndef BOARD_NATIVE
static int _alloc_addr(kernel_pid_t dev, char *addr_str, ipv6_addr_t *addr) {
    ipv6_addr_t *ifaddr;
    uint8_t prefix_len, flags = 0;

    /* Add address for serial interface, like the command line:
     *    ifconfig 8 add unicast bbbb::2/64 */
    if (strlen(addr_str) > _NETWORK_ADDR_MAXLEN) {
        puts("error: network address too long.");
        return 0;
    }

    prefix_len = ipv6_addr_split(addr_str, '/', _DEFAULT_PREFIX_LEN);

    if ((prefix_len < 1) || (prefix_len > IPV6_ADDR_BIT_LEN)) {
        prefix_len = _DEFAULT_PREFIX_LEN;
    }

    if (ipv6_addr_from_str(addr, addr_str) == NULL) {
        puts("error: unable to parse SLIP address.");
        return 0;
    }

    flags = GNRC_IPV6_NETIF_ADDR_FLAGS_NDP_AUTO | GNRC_IPV6_NETIF_ADDR_FLAGS_UNICAST;

    if ((ifaddr = gnrc_ipv6_netif_add_addr(dev, addr, prefix_len, flags)) == NULL) {
        printf("error: unable to add IPv6 address\n");
        return 0;
    }
    /* Address shall be valid infinitely */
    gnrc_ipv6_netif_addr_get(ifaddr)->valid = UINT32_MAX;
    /* Address shall be preferred infinitely */
    gnrc_ipv6_netif_addr_get(ifaddr)->preferred = UINT32_MAX;

    return 1;
}
#endif

static int _init_border(void)
{
#ifdef NET_SLIP
    ipv6_addr_t addr;
     /* must be writable for ipv6_addr_split() */
    char addr_str[_NETWORK_ADDR_MAXLEN];

    /* Add address for serial interface, like the command line:
     *    ifconfig 8 add unicast bbbb::2/64 */
    kernel_pid_t dev = (kernel_pid_t) BORDER_IF;

    if (strlen(SLIP_ADDR) > _NETWORK_ADDR_MAXLEN) {
        puts("error: network address too long.");
        return 0;
    }
    memcpy(&addr_str[0], SLIP_ADDR, strlen(SLIP_ADDR));

    if (!_alloc_addr(dev, &addr_str[0], &addr)) {
        return 0;
    }

    /* Cache address for opposite end of tunslip interface, like command line:
     *    ncache add 8 bbbb::1 */
    if (ipv6_addr_from_str(&addr, SLIP_PEER_ADDR) == NULL) {
        puts("error: unable to parse SLIP peer address.");
        return 0;
    }

    if (gnrc_ipv6_nc_add(dev, &addr, NULL, 0, 0) == NULL) {
        puts("error: unable to add address to neighbor cache.");
        return 0;
    }

    puts("Initialized border router SLIP networking");
    return 1;
#else
    return 1;
#endif
}

static int _init_router(void)
{
#ifdef NET_ROUTER
    ipv6_addr_t addr;
     /* must be writable for ipv6_addr_split() */
    char addr_str[_NETWORK_ADDR_MAXLEN];

    /* Add address for serial interface, like the command line:
     *    ifconfig 7 add unicast aaaa::1/64 */
    kernel_pid_t dev = (kernel_pid_t) ROUTER_IF;

    if (strlen(HOST_ADDR) > _NETWORK_ADDR_MAXLEN) {
        puts("error: network address too long.");
        return 0;
    }
    memcpy(&addr_str[0], HOST_ADDR, strlen(HOST_ADDR));
    
    if (_alloc_addr(dev, &addr_str[0], &addr)) {
        puts("Initialized router address");
    }
    else {
        return 0;
    }

    /* Intialize RPL, like the command line:
     *    rpl init 7 */
    gnrc_rpl_init(dev);
    puts("Initialized RPL; pause 10 sec.");
    xtimer_sleep(10);

#ifdef NET_SLIP
    uint8_t instance_id = 1;

    /* Intialize RPL root, like the command line:
     *     rpl root 1 aaaa::1 */
    gnrc_rpl_instance_t *inst = gnrc_rpl_root_init(instance_id, &addr, false, false);
    if (inst == NULL) {
        char addr_str[IPV6_ADDR_MAX_STR_LEN];
        printf("error: could not add DODAG (%s) to instance (%d)\n",
                ipv6_addr_to_str(addr_str, &addr, strlen(HOST_ADDR)), instance_id);
        return 0;
    }
    else {
        puts("Initialized RPL root; pause 10 sec.");
        xtimer_sleep(10);
    }
#endif     

    puts("Initialized router RPL networking");
    return 1;
#else
    return 1;
#endif
}

/* Sends a 'hello' message to the Datahead server. */
static int _send_hello(void)
{
#if defined(DATAHEAD_ADDR) && defined(DATAHEAD_PORT)
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    size_t len;
    sock_udp_ep_t server_sock;
    ipv6_addr_t addr;

    gcoap_req_init(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE, COAP_METHOD_POST,
                    DATAHEAD_HELLO_PATH);
    coap_hdr_set_type(pdu.hdr, COAP_TYPE_CON);
    len = gcoap_finish(&pdu, 0, COAP_FORMAT_NONE);

    server_sock.family = AF_INET6;
    server_sock.netif  = SOCK_ADDR_ANY_NETIF;

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, DATAHEAD_ADDR) == NULL) {
        puts("error: unable to parse datahead address");
        return 0;
    }
    memcpy(&server_sock.addr.ipv6[0], &addr.u8[0], sizeof(addr.u8));

    /* parse port */
    server_sock.port = (uint16_t)atoi(DATAHEAD_PORT);
    if (DATAHEAD_PORT == 0) {
        puts("error: unable to parse datahead port");
        return 0;
    }

    puts("Sending hello message to datahead");
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

#if defined(NET_SLIP) || defined(NET_ROUTER)
    puts("Delay 20 sec. before network setup");
    xtimer_sleep(20);
#endif

    /* initialize border router SLIP networking, if required */
    if (!_init_border()) {
        return 1;
    }
    /* initialize RPL router networking, if required */
    if (!_init_router()) {
        return 1;
    }

    gcoap_register_listener(&_listener);
    
    if (!_send_hello()) {
        return 1;
    }

    _run_sensor_loop();

    return 0;
}
