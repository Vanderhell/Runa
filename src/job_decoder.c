#include "job_validator.h"
uint16_t job_read_u16_le(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u)); }
uint32_t job_read_u32_le(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) | ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u); }
void job_write_u16_le(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8u); }
void job_write_u32_le(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8u); p[2]=(uint8_t)(v>>16u); p[3]=(uint8_t)(v>>24u); }
job_error_t job_decode_header(const uint8_t *d, size_t n, job_header_t *h) {
    if (!d || !h) return JOB_ERR_INVALID_FORMAT;
    if (n > JOB_MAX_BYTES) return JOB_ERR_JOB_TOO_LARGE;
    if (n < JOB_HEADER_SIZE) return JOB_ERR_INVALID_FORMAT;
    if (d[0]!=JOB_MAGIC_0 || d[1]!=JOB_MAGIC_1 || d[2]!=JOB_MAGIC_2 || d[3]!=JOB_MAGIC_3) return JOB_ERR_BAD_MAGIC;
    if (d[4]!=JOB_PROTOCOL_VERSION) return JOB_ERR_UNSUPPORTED_PROTOCOL;
    if (d[5]!=JOB_IR_VERSION) return JOB_ERR_UNSUPPORTED_IR_VERSION;
    if (job_read_u16_le(d+6)!=JOB_HEADER_SIZE || job_read_u16_le(d+22)!=0u) return JOB_ERR_INVALID_FORMAT;
    h->job_id=job_read_u32_le(d+8); h->total_size=job_read_u32_le(d+12);
    h->instruction_bytes=job_read_u32_le(d+16); h->instruction_count=job_read_u16_le(d+20);
    h->max_steps=job_read_u32_le(d+24); h->max_runtime_us=job_read_u32_le(d+28);
    h->max_result_bytes=job_read_u16_le(d+32); h->max_emits=job_read_u16_le(d+34); h->max_emit_bytes=job_read_u32_le(d+36);
    if (h->total_size != n || h->instruction_bytes > JOB_MAX_BYTES-JOB_HEADER_SIZE || h->total_size != JOB_HEADER_SIZE+h->instruction_bytes) return JOB_ERR_INVALID_FORMAT;
    if (h->instruction_count==0u || h->instruction_count>JOB_MAX_INSTRUCTIONS) return JOB_ERR_INVALID_FORMAT;
    if (h->max_steps==0u || h->max_steps>JOB_MAX_STEPS || h->max_runtime_us==0u || h->max_runtime_us>JOB_MAX_RUNTIME_US ||
        h->max_result_bytes>JOB_MAX_RESULT_BYTES || h->max_emits>JOB_MAX_EMITS || h->max_emit_bytes>JOB_MAX_EMIT_BYTES) return JOB_ERR_OUT_OF_RANGE;
    return JOB_OK;
}
job_error_t job_decode_instruction(const uint8_t *d, size_t n, uint32_t off, job_instruction_t *i) {
    size_t p=(size_t)off;
    if (!d || !i || p>n || n-p<2u) return JOB_ERR_INVALID_FORMAT;
    i->opcode=d[p]; i->operand_size=d[p+1u];
    if ((size_t)i->operand_size>n-p-2u) return JOB_ERR_INVALID_FORMAT;
    i->operands=d+p+2u; i->offset=off; return JOB_OK;
}
