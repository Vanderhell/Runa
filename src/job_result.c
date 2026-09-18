#include "job_result.h"
#include "job_ir.h"
static size_t pre(uint8_t *o,size_t c,uint8_t t,uint16_t n,uint32_t id){if(!o||c<n)return 0u;o[0]=t;o[1]=JOB_PROTOCOL_VERSION;job_write_u16_le(o+2,n);job_write_u32_le(o+4,id);return n;}
size_t job_encode_ack(uint8_t *o,size_t c,uint32_t id){return pre(o,c,JOB_EVENT_ACK,8u,id);}
size_t job_encode_emit(uint8_t *o,size_t c,uint32_t id,uint16_t ip,uint32_t v){size_t n=pre(o,c,JOB_EVENT_EMIT,14u,id);if(n){job_write_u16_le(o+8,ip);job_write_u32_le(o+10,v);}return n;}
size_t job_encode_result(uint8_t *o,size_t c,uint32_t id,job_result_status_t s,job_error_t e,uint16_t ip,uint32_t detail,const uint8_t *p,uint16_t pn){
    uint16_t n;size_t k;if(pn>JOB_MAX_RESULT_BYTES||(pn&&!p))return 0u;n=(uint16_t)(20u+pn);if(!pre(o,c,JOB_EVENT_RESULT,n,id))return 0u;
    o[8]=(uint8_t)s;o[9]=(uint8_t)e;job_write_u16_le(o+10,ip);job_write_u32_le(o+12,detail);job_write_u16_le(o+16,pn);job_write_u16_le(o+18,0u);for(k=0;k<pn;k++)o[20u+k]=p[k];return n;
}
