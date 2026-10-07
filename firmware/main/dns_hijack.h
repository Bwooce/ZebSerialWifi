#pragma once
/* Captive-portal DNS: answers every A query with the SoftAP address 192.168.4.1. */

void dns_hijack_start(void);
void dns_hijack_stop(void);
