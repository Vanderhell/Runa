#include "job_runtime.h"
#include "job_validator.h"
#include "job_mock_hal.h"
#include "test_builder.h"
#include "runa_capabilities.h"
#include "runa_gpio.h"
#include "runa_adc.h"
#include "runa_pwm.h"
#include "runa_spi.h"
#include "runa_i2c.h"
#include "runa_uart.h"
#include "runa_can.h"
#include "runa_pulse.h"
#include "runa_dac.h"
#include "runa_encoder.h"
#include "runa_onewire.h"
#include "runa_block_device.h"
#include "runa_rtc.h"
#include "runa_watchdog.h"
#include "runa_validator.h"
#include "runa_ir.h"
#include "runa_limits.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t state;
} verify_prng_t;

typedef struct {
    uint64_t cases;
    uint64_t assertions;
    uint64_t failures;
    uint64_t seed;
    uint64_t case_id;
    const char *family;
} verify_stats_t;

typedef struct {
    uint8_t bytes[4][JOB_MAX_EVENT_BYTES];
    size_t sizes[4];
    uint32_t count;
} verify_sink_t;

static verify_stats_t g_stats = { 0u, 0u, 0u, 0x6d637572756e61ULL, 0u, "" };
static const job_resource_table_t g_empty_resources = { NULL, 0u };

static uint32_t next_u32(verify_prng_t *prng) {
    uint64_t x = prng->state;
    x ^= x >> 12u;
    x ^= x << 25u;
    x ^= x >> 27u;
    prng->state = x;
    return (uint32_t)((x * UINT64_C(2685821657736338717)) >> 32u);
}

static uint32_t value_at(verify_prng_t *prng, uint32_t index) {
    static const uint32_t boundary[] = {
        0u, 1u, UINT32_MAX, UINT32_MAX - 1u, UINT32_C(0x80000000),
        UINT32_C(0x7fffffff), UINT32_C(0xaaaaaaaa), UINT32_C(0x55555555),
        UINT32_C(0x80000001), UINT32_C(0x40000000)
    };
    return index < (uint32_t)(sizeof boundary / sizeof boundary[0])
        ? boundary[index]
        : next_u32(prng);
}

static void check_value(int condition, const char *expression, uint64_t case_id,
                        const char *family, uint32_t expected, uint32_t actual) {
    ++g_stats.assertions;
    if (!condition) {
        ++g_stats.failures;
        printf("FAIL family=%s seed=0x%016" PRIx64 " case=%" PRIu64
               " expr=%s expected=%" PRIu32 " actual=%" PRIu32 "\n",
               family, g_stats.seed, case_id, expression, expected, actual);
    }
}

#define CHECK_EQ(family, case_id, expected, actual) do { \
    const uint32_t expected_value = (uint32_t)(expected); \
    const uint32_t actual_value = (uint32_t)(actual); \
    check_value(expected_value == actual_value, #actual, (case_id), (family), \
                expected_value, actual_value); \
} while (0)
#define CHECK_TRUE(family, case_id, expression) do { \
    const int expression_value = ((expression) != 0); \
    check_value(expression_value != 0, #expression, (case_id), (family), 1u, \
                (uint32_t)expression_value); \
} while (0)

static int capture(void *context, const uint8_t *data, size_t size) {
    verify_sink_t *sink = (verify_sink_t *)context;
    if (sink->count >= 4u || size > JOB_MAX_EVENT_BYTES) return -1;
    memcpy(sink->bytes[sink->count], data, size);
    sink->sizes[sink->count] = size;
    ++sink->count;
    return 0;
}

static void model_binary(uint8_t opcode, uint32_t left, uint32_t right,
                         uint32_t *value) {
    switch (opcode) {
    case JOB_OP_ADD: *value = left + right; break;
    case JOB_OP_SUB: *value = left - right; break;
    case JOB_OP_AND: *value = left & right; break;
    case JOB_OP_OR: *value = left | right; break;
    case JOB_OP_XOR: *value = left ^ right; break;
    case JOB_OP_SHL: *value = left << (right & 31u); break;
    case JOB_OP_SHR: *value = left >> (right & 31u); break;
    case JOB_OP_CMP_EQ: *value = left == right; break;
    case JOB_OP_CMP_NE: *value = left != right; break;
    case JOB_OP_CMP_LT: *value = left < right; break;
    case JOB_OP_CMP_LE: *value = left <= right; break;
    case JOB_OP_CMP_GT: *value = left > right; break;
    case JOB_OP_CMP_GE: *value = left >= right; break;
    default: *value = 0u; break;
    }
}

static job_execution_summary_t run_job(test_job_t *job, job_mock_hal_t *hal,
                                       verify_sink_t *sink) {
    job_event_sink_t event_sink = { capture, sink };
    job_hal_t job_hal = job_mock_hal_interface(hal);
    test_job_finish(job);
    return job_process(job->data, job->size, &g_empty_resources, &job_hal, &event_sink);
}

/* The scalar model is intentionally independent of the production executor. */
static void scalar_model_family(verify_prng_t *prng) {
    static const uint8_t ops[] = {
        JOB_OP_ADD, JOB_OP_SUB, JOB_OP_AND, JOB_OP_OR, JOB_OP_XOR,
        JOB_OP_SHL, JOB_OP_SHR, JOB_OP_CMP_EQ, JOB_OP_CMP_NE, JOB_OP_CMP_LT,
        JOB_OP_CMP_LE, JOB_OP_CMP_GT, JOB_OP_CMP_GE
    };
    uint32_t i;
    for (i = 0u; i < 5000u; ++i) {
        const uint32_t left = value_at(prng, i % 10u);
        const uint32_t right = value_at(prng, (i + 3u) % 10u);
        const uint8_t opcode = ops[i % (uint32_t)(sizeof ops / sizeof ops[0])];
        uint32_t expected;
        test_job_t job;
        job_mock_hal_t hal;
        verify_sink_t sink = { 0 };
        job_execution_summary_t summary;
        model_binary(opcode, left, right, &expected);
        test_job_init(&job, i);
        test_load(&job, 1u, left);
        test_load(&job, 2u, right);
        test_tri(&job, opcode, 0u, 1u, 2u);
        test_return(&job, 1u);
        job_mock_hal_init(&hal);
        summary = run_job(&job, &hal, &sink);
        ++g_stats.cases;
        CHECK_EQ("scalar", i, JOB_OK, summary.error);
        CHECK_EQ("scalar", i, 2u, sink.count);
        CHECK_EQ("scalar", i, expected,
                 job_read_u32_le(sink.bytes[sink.count - 1u] + 20u));
    }
}

static void control_flow_family(verify_prng_t *prng) {
    uint32_t i;
    for (i = 0u; i < 2000u; ++i) {
        uint8_t jump[3];
        test_job_t job;
        job_mock_hal_t hal;
        verify_sink_t sink = { 0 };
        job_execution_summary_t summary;
        const uint32_t condition = next_u32(prng) & 1u;
        test_job_init(&job, UINT32_C(0x40000000) + i);
        test_load(&job, 0u, condition);
        jump[0] = 0u;
        job_write_u16_le(jump + 1u, 3u);
        test_job_ins(&job, JOB_OP_JUMP_IF, jump, sizeof jump);
        test_load(&job, 0u, 0x12340000u + i);
        test_return(&job, 1u);
        job_mock_hal_init(&hal);
        summary = run_job(&job, &hal, &sink);
        ++g_stats.cases;
        CHECK_EQ("control-flow", i, JOB_OK, summary.error);
        CHECK_EQ("control-flow", i, 2u, sink.count);
        CHECK_EQ("control-flow", i, condition != 0u ? 1u : 0x12340000u + i,
                 job_read_u32_le(sink.bytes[sink.count - 1u] + 20u));
    }
}

static void malformed_and_prefix_family(verify_prng_t *prng) {
    static const job_resource_table_t no_resources = { NULL, 0u };
    uint8_t data[JOB_MAX_BYTES];
    job_header_t header;
    job_validation_error_t error;
    test_job_t valid;
    uint32_t i;
    test_job_init(&valid, 77u);
    test_load(&valid, 0u, 17u);
    test_return(&valid, 0u);
    test_job_finish(&valid);
    for (i = 0u; i <= valid.size; ++i) {
        ++g_stats.cases;
        if (i == valid.size) {
            CHECK_EQ("valid-prefix", i, JOB_OK,
                     job_validate(valid.data, i, &no_resources, &header, &error));
        } else {
            CHECK_TRUE("valid-prefix", i, job_validate(valid.data, i, &no_resources,
                                                        &header, &error) != JOB_OK);
        }
    }
    for (i = 0u; i < 20000u; ++i) {
        size_t size = (size_t)(next_u32(prng) % (JOB_MAX_BYTES + 1u));
        size_t k;
        for (k = 0u; k < size; ++k) data[k] = (uint8_t)next_u32(prng);
        ++g_stats.cases;
        (void)job_decode_header(data, size, &header);
        (void)job_validate(data, size, &no_resources, &header, &error);
    }
}

static void header_boundary_family(void) {
    test_job_t job;
    job_header_t header;
    job_validation_error_t error;
    static const job_resource_table_t no_resources = { NULL, 0u };
    const uint32_t fields[] = { 0u, 1u, JOB_MAX_BYTES - 1u, JOB_MAX_BYTES,
                                JOB_MAX_BYTES + 1u, UINT32_MAX };
    uint32_t i;
    test_job_init(&job, 88u);
    test_return(&job, 0u);
    test_job_finish(&job);
    for (i = 0u; i < (uint32_t)(sizeof fields / sizeof fields[0]); ++i) {
        const uint32_t original = job_read_u32_le(job.data + 12u);
        job_write_u32_le(job.data + 12u, fields[i]);
        ++g_stats.cases;
        if (fields[i] == original) {
            CHECK_EQ("header-boundary", i, JOB_OK,
                     job_validate(job.data, job.size, &no_resources, &header, &error));
        } else {
            CHECK_TRUE("header-boundary", i,
                       job_validate(job.data, job.size, &no_resources, &header, &error) != JOB_OK);
        }
        job_write_u32_le(job.data + 12u, original);
    }
}

static void deterministic_replay_and_soak(void) {
    test_job_t job;
    job_mock_hal_t hal;
    verify_sink_t first = { 0 };
    uint32_t i;
    test_job_init(&job, 99u);
    test_load(&job, 0u, 42u);
    test_return(&job, 0u);
    for (i = 0u; i < 100u; ++i) {
        verify_sink_t sink = { 0 };
        job_execution_summary_t summary;
        job_mock_hal_init(&hal);
        summary = run_job(&job, &hal, &sink);
        ++g_stats.cases;
        CHECK_EQ("determinism", i, JOB_OK, summary.error);
        if (i == 0u) first = sink;
        CHECK_TRUE("determinism", i, memcmp(&first, &sink, sizeof first) == 0);
    }
    for (i = 0u; i < 1000000u; ++i) {
        verify_sink_t sink = { 0 };
        job_mock_hal_init(&hal);
        CHECK_EQ("soak", i, JOB_OK, run_job(&job, &hal, &sink).error);
        ++g_stats.cases;
    }
}

static void capability_family(void) {
    runa_module_t modules[14] = {
        runa_gpio_module(NULL), runa_adc_module(NULL), runa_pwm_module(NULL),
        runa_spi_module(NULL), runa_i2c_module(NULL), runa_uart_module(NULL),
        runa_can_module(NULL), runa_pulse_module(NULL), runa_dac_module(NULL),
        runa_encoder_module(NULL), runa_onewire_module(NULL),
        runa_block_device_module(NULL), runa_rtc_module(NULL), runa_watchdog_module(NULL)
    };
    runa_module_registry_t registry;
    uint8_t encoded[RUNA_MAX_CAPABILITY_BYTES];
    size_t written;
    runa_capabilities_view_t decoded;
    uint32_t i;
    runa_registry_init(&registry);
    for (i = 0u; i < 14u; ++i) (void)runa_registry_add(&registry, &modules[i]);
    for (i = 0u; i < 1000u; ++i) {
        size_t cut;
        ++g_stats.cases;
        CHECK_EQ("capability-round-trip", i, RUNA_OK,
                 runa_capabilities_encode(&registry, encoded, sizeof encoded, &written));
        CHECK_EQ("capability-round-trip", i, RUNA_OK,
                 runa_capabilities_decode(encoded, written, &decoded));
        CHECK_EQ("capability-round-trip", i, 14u, decoded.module_count);
        for (cut = 0u; cut <= written; ++cut) {
            ++g_stats.assertions;
            (void)runa_capabilities_decode(encoded, cut, &decoded);
        }
    }
}

static runa_status_t synthetic_validate(void *context, const runa_module_job_t *job,
                                        const runa_module_instruction_t *instruction,
                                        uint32_t *detail) {
    (void)context; (void)job; (void)instruction; (void)detail;
    return RUNA_OK;
}

static runa_status_t synthetic_execute(void *context, runa_module_job_t *job,
                                       const runa_module_instruction_t *instruction,
                                       uint32_t *detail) {
    (void)context; (void)job; (void)instruction; (void)detail;
    return RUNA_OK;
}

static runa_status_t synthetic_resource(void *context, const runa_resource_t *resource,
                                        uint32_t *detail) {
    (void)context; (void)detail;
    return resource != NULL && resource->resource_type == 7u ? RUNA_OK : RUNA_ERR_RESOURCE_TYPE;
}

static void registry_resource_family(verify_prng_t *prng) {
    runa_module_t modules[RUNA_MAX_MODULES + 1u];
    runa_module_registry_t registry;
    runa_resource_t resources[4];
    uint32_t i;
    for (i = 0u; i <= RUNA_MAX_MODULES; ++i) {
        memset(&modules[i], 0, sizeof modules[i]);
        modules[i].module_id = (uint16_t)(100u + i);
        modules[i].abi_version = RUNA_MODULE_ABI_VERSION;
        modules[i].validate = synthetic_validate;
        modules[i].execute = synthetic_execute;
        modules[i].validate_resource = synthetic_resource;
    }
    for (i = 0u; i <= RUNA_MAX_MODULES + 1u; ++i) {
        runa_registry_init(&registry);
        {
            uint32_t count;
            const uint32_t wanted = i;
            for (count = 0u; count < wanted && count < RUNA_MAX_MODULES; ++count)
                CHECK_EQ("registry-capacity", i, RUNA_OK,
                         runa_registry_add(&registry, &modules[count]));
            if (wanted == RUNA_MAX_MODULES + 1u) {
                CHECK_EQ("registry-capacity", i, RUNA_ERR_OUT_OF_RANGE,
                         runa_registry_add(&registry, &modules[RUNA_MAX_MODULES]));
            }
            ++g_stats.cases;
            CHECK_EQ("registry-capacity", i, wanted <= RUNA_MAX_MODULES ? wanted : RUNA_MAX_MODULES,
                     registry.count);
        }
    }
    runa_registry_init(&registry);
    CHECK_EQ("registry-duplicate", 0u, RUNA_OK, runa_registry_add(&registry, &modules[0]));
    CHECK_EQ("registry-duplicate", 1u, RUNA_ERR_INVALID_FORMAT,
             runa_registry_add(&registry, &modules[0]));
    modules[1].abi_version = 99u;
    CHECK_EQ("registry-abi", 2u, RUNA_ERR_INVALID_FORMAT,
             runa_registry_add(&registry, &modules[1]));
    modules[1].abi_version = RUNA_MODULE_ABI_VERSION;
    modules[1].validate = NULL;
    CHECK_EQ("registry-callback", 3u, RUNA_ERR_INVALID_FORMAT,
             runa_registry_add(&registry, &modules[1]));
    modules[1].validate = synthetic_validate;

    for (i = 0u; i < 4000u; ++i) {
        const uint32_t random_id = next_u32(prng) & 3u;
        const uint32_t random_permissions = next_u32(prng) & 7u;
        const uint32_t random_type = next_u32(prng) & 7u;
        resources[0].id = (uint16_t)random_id;
        resources[0].module_id = 100u;
        resources[0].resource_type = (uint16_t)random_type;
        resources[0].reserved = 0u;
        resources[0].permissions = random_permissions;
        resources[0].platform_handle = 0u;
        resources[0].config = NULL;
        {
            runa_resource_table_t table = { resources, 1u };
            const runa_status_t actual = runa_resource_table_validate(&table, &registry);
            const int valid = random_type == 7u && (random_permissions & ~3u) == 0u;
            const runa_status_t expected = valid ? RUNA_OK :
                ((random_permissions & ~3u) != 0u ? RUNA_ERR_INVALID_RESOURCE :
                 RUNA_ERR_RESOURCE_TYPE);
            ++g_stats.cases;
            CHECK_EQ("resource-matrix", i, expected, actual);
        }
    }
    resources[0].id = 1u; resources[1] = resources[0]; resources[1].id = 1u;
    {
        runa_resource_table_t table = { resources, 2u };
        CHECK_EQ("resource-duplicate", 0u, RUNA_ERR_INVALID_RESOURCE,
                 runa_resource_table_validate(&table, &registry));
    }
    {
        runa_resource_table_t table = { resources, RUNA_MAX_RESOURCES + 1u };
        CHECK_EQ("resource-capacity", 0u, RUNA_ERR_INVALID_FORMAT,
                 runa_resource_table_validate(&table, &registry));
    }
}

int main(void) {
    verify_prng_t prng = { UINT64_C(0x9e3779b97f4a7c15) };
    g_stats.family = "campaign";
    scalar_model_family(&prng);
    control_flow_family(&prng);
    malformed_and_prefix_family(&prng);
    header_boundary_family();
    deterministic_replay_and_soak();
    capability_family();
    registry_resource_family(&prng);
    printf("verification cases=%" PRIu64 " assertions=%" PRIu64
           " failures=%" PRIu64 " seed=0x%016" PRIx64 "\n",
           g_stats.cases, g_stats.assertions, g_stats.failures, g_stats.seed);
    return g_stats.failures == 0u ? 0 : 1;
}
