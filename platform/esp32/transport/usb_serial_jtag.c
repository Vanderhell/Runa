#include "esp32_platform.h"
#include "job_ir.h"
#include "driver/usb_serial_jtag.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static int write_all(const uint8_t *data, size_t size) {
    size_t written = 0u;
    while (written < size) {
        int count = usb_serial_jtag_write_bytes(data + written, size - written, pdMS_TO_TICKS(1000));
        if (count <= 0) return -1;
        written += (size_t)count;
    }
    return 0;
}

int runa_esp32_transport_init(void) {
    usb_serial_jtag_driver_config_t config = {
        .tx_buffer_size = 4096,
        .rx_buffer_size = 4096
    };
    return usb_serial_jtag_driver_install(&config) == ESP_OK ? 0 : -1;
}

int runa_esp32_transport_write(const uint8_t *data, size_t size) {
    if (data == NULL || size < 4u || size > RUNA_MAX_CAPABILITY_BYTES + 8u) return -1;
    return write_all(data, size);
}

int runa_esp32_transport_read_request(uint8_t *command, uint8_t *payload,
                                      size_t capacity, size_t *payload_size) {
    static const uint8_t magic[4] = { 'J', 'R', 'E', 'Q' };
    uint8_t header[RUNA_REQUEST_HEADER_SIZE];
    size_t matched = 0u;
    if (command == NULL || payload == NULL || payload_size == NULL) return -1;
    for (;;) {
        uint8_t byte;
        int count = usb_serial_jtag_read_bytes(&byte, 1u, pdMS_TO_TICKS(100));
        if (count <= 0) continue;
        if (byte == magic[matched]) {
            header[matched++] = byte;
            if (matched == sizeof magic) break;
        } else {
            matched = byte == magic[0] ? 1u : 0u;
            if (matched != 0u) header[0] = byte;
        }
    }
    for (matched = sizeof magic; matched < sizeof header; ) {
        int count = usb_serial_jtag_read_bytes(header + matched, sizeof header - matched, pdMS_TO_TICKS(1000));
        if (count <= 0) return -1;
        matched += (size_t)count;
    }
    if (header[4] != RUNA_PROTOCOL_VERSION) return -1;
    *command = header[5];
    *payload_size = runa_read_u16_le(header + 6);
    if (*payload_size > capacity || *payload_size > RUNA_MAX_JOB_BYTES) return -1;
    for (matched = 0u; matched < *payload_size; ) {
        int count = usb_serial_jtag_read_bytes(payload + matched, *payload_size - matched, pdMS_TO_TICKS(1000));
        if (count <= 0) return -1;
        matched += (size_t)count;
    }
    return 0;
}

static void frame_prefix(uint8_t *frame, uint8_t type, uint16_t size) {
    frame[0] = type;
    frame[1] = RUNA_PROTOCOL_VERSION;
    runa_write_u16_le(frame + 2, size);
    runa_write_u32_le(frame + 4, 0u);
}

void runa_esp32_send_info(void) {
    uint8_t frame[32] = {0};
    uint32_t flash_size = 0u;
    (void)esp_flash_get_size(NULL, &flash_size);
    frame_prefix(frame, RUNA_EVENT_INFO, 30u);
    frame[8] = RUNA_PROTOCOL_VERSION;
    frame[9] = RUNA_IR_VERSION_V1;
    frame[10] = 0u; frame[11] = 1u; frame[12] = 0u;
    frame[13] = RUNA_PLATFORM_ESP32S3;
    runa_write_u16_le(frame + 14, RUNA_MAX_JOB_BYTES);
    runa_write_u16_le(frame + 16, RUNA_MAX_INSTRUCTIONS);
    runa_write_u32_le(frame + 18, RUNA_MAX_STEPS);
    frame[22] = RUNA_REGISTER_COUNT;
    frame[23] = (uint8_t)runa_esp32_resources.count;
    runa_write_u32_le(frame + 24, flash_size);
    runa_write_u16_le(frame + 28, (uint16_t)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / (1024u * 1024u)));
    (void)write_all(frame, 30u);
}

void runa_esp32_send_capabilities_v1(void) {
    uint8_t frame[48] = {0};
    uint8_t opcodes[] = {
        JOB_OP_NOP, JOB_OP_LOAD_CONST, JOB_OP_MOV, JOB_OP_ADD, JOB_OP_SUB,
        JOB_OP_AND, JOB_OP_OR, JOB_OP_XOR, JOB_OP_NOT, JOB_OP_SHL, JOB_OP_SHR,
        JOB_OP_CMP_EQ, JOB_OP_CMP_NE, JOB_OP_CMP_LT, JOB_OP_CMP_LE,
        JOB_OP_CMP_GT, JOB_OP_CMP_GE, JOB_OP_JUMP, JOB_OP_JUMP_IF,
        JOB_OP_JUMP_IF_NOT, JOB_OP_GPIO_READ, JOB_OP_GPIO_WRITE, JOB_OP_ADC_READ,
        JOB_OP_PWM_WRITE, JOB_OP_DELAY_MS, JOB_OP_EMIT, JOB_OP_RETURN
    };
    size_t index;
    frame_prefix(frame, RUNA_EVENT_CAPABILITIES_V1, 41u);
    frame[8] = (uint8_t)sizeof opcodes;
    frame[9] = (uint8_t)runa_esp32_resources.count;
    for (index = 0u; index < sizeof opcodes; ++index) frame[10u + index] = opcodes[index];
    runa_write_u32_le(frame + 37, RUNA_MAX_RUNTIME_US);
    (void)write_all(frame, 41u);
}

void runa_esp32_send_capabilities_v2(const runa_module_registry_t *registry) {
    uint8_t frame[RUNA_MAX_CAPABILITY_BYTES + 8u];
    size_t payload_size = 0u;
    if (runa_capabilities_encode(registry, frame + 8u, sizeof frame - 8u,
                                 &payload_size) != RUNA_OK) return;
    frame_prefix(frame, RUNA_EVENT_CAPABILITIES_V2, (uint16_t)(8u + payload_size));
    (void)write_all(frame, 8u + payload_size);
}
