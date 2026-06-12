#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

int main()
{
    // Setup USB
    stdio_init_all();
    
    // wait to open the serial monitor
    sleep_ms(3000); 
    printf("--- System Starting ---\n");

    // Initialize wifi hardware
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // Enable Station Mode (Client Mode)
    cyw43_arch_enable_sta_mode();

    // Retrieve the MAC address
    uint8_t mac[6];
    cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);

    printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);


    // Connect
    printf("Connecting to Wi-Fi...\n");
    //if (cyw43_arch_wifi_connect_timeout_ms("Your_SSID", "Your_Password", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    //    printf("Failed to connect.\n");
    //} else {
    //    printf("Connected.\n");
    //}

    while (true) {
        // Keep printing 
        printf("Heartbeat: MAC is %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        sleep_ms(2000);
    }
}