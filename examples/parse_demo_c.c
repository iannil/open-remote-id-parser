/**
 * ORIP C API Demo
 *
 * This example demonstrates using the ORIP library from pure C code.
 * Compile with: gcc -o parse_demo_c parse_demo_c.c -lorip -lstdc++
 */

#include <stdio.h>
#include <string.h>
#include "orip/orip_c.h"

/* Callback for new UAV detection */
void on_new_uav(const orip_uav_t* uav, void* user_data) {
    (void)user_data;
    printf("[NEW UAV] ID: %s, Type: %d\n", uav->id, uav->uav_type);
}

/* Callback for UAV updates */
void on_uav_update(const orip_uav_t* uav, void* user_data) {
    (void)user_data;
    if (uav->location.valid) {
        printf("[UPDATE] %s: Lat=%.6f, Lon=%.6f, Alt=%.1fm\n",
               uav->id,
               uav->location.latitude,
               uav->location.longitude,
               uav->location.altitude_baro);
    }
}

int main(void) {
    printf("Open Remote ID Parser v%s (C API Demo)\n", orip_version());
    printf("========================================\n\n");

    /* Create parser with default config */
    orip_parser_t* parser = orip_create();
    if (!parser) {
        fprintf(stderr, "Failed to create parser\n");
        return 1;
    }

    /* Set callbacks */
    orip_set_on_new_uav(parser, on_new_uav, NULL);
    orip_set_on_uav_update(parser, on_uav_update, NULL);

    /* Simulated BLE advertisement with ODID Basic ID message */
    uint8_t sample_advertisement[] = {
        /* AD Structure header */
        0x1E,        /* Length (30 bytes) */
        0x16,        /* AD Type: Service Data - 16-bit UUID */
        0xFA, 0xFF,  /* UUID: 0xFFFA (ASTM Remote ID) */
        0x00,        /* Message counter */

        /* Basic ID Message (25 bytes) */
        0x02,        /* Message type (0x0) | Proto version (0x2) */
        0x12,        /* ID type: Serial (1) | UA type: Multirotor (2) */
        /* Serial number: "DJI1234567890ABCD" */
        'D', 'J', 'I', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', '0', 'A', 'B', 'C', 'D', 0, 0, 0,
        0, 0, 0, 0   /* Padding */
    };

    printf("Parsing sample advertisement...\n\n");

    orip_result_t result;
    int ret = orip_parse(parser,
                         sample_advertisement,
                         sizeof(sample_advertisement),
                         -65,
                         ORIP_TRANSPORT_BT_LEGACY,
                         &result);

    if (ret == 0 && result.success) {
        printf("\n[SUCCESS] Remote ID detected!\n");
        printf("  Protocol: ASTM F3411\n");
        printf("  UAV ID: %s\n", result.uav.id);
        printf("  UAV Type: %d\n", result.uav.uav_type);
        printf("  ID Type: %d\n", result.uav.id_type);
        printf("  RSSI: %d dBm\n", result.uav.rssi);
    } else {
        printf("[FAILED] %s\n", result.error);
    }

    printf("\nActive UAVs: %zu\n", orip_get_active_count(parser));

    /* Get UAV by ID */
    orip_uav_t uav;
    if (orip_get_uav(parser, "DJI1234567890ABCD", &uav) == 0) {
        printf("Found UAV: %s (messages: %u)\n", uav.id, uav.message_count);
    }

    /* Cleanup */
    orip_destroy(parser);
    printf("\nDone.\n");

    return 0;
}
