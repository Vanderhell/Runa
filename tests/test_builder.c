#include "test_builder.h"
#include <string.h>
void test_job_init(test_job_t*j,uint32_t id){memset(j,0,sizeof *j);j->size=JOB_HEADER_SIZE;j->job_id=id;}
int test_job_ins(test_job_t*j,uint8_t op,const uint8_t*p,uint8_t n){if(!j||j->size+2u+n>JOB_MAX_BYTES||j->count==JOB_MAX_INSTRUCTIONS)return 0;j->data[j->size]=op;j->data[j->size+1u]=n;if(n)memcpy(j->data+j->size+2u,p,n);j->size+=2u+n;j->count++;return 1;}
void test_job_finish(test_job_t*j){uint8_t*d=j->data;d[0]=JOB_MAGIC_0;d[1]=JOB_MAGIC_1;d[2]=JOB_MAGIC_2;d[3]=JOB_MAGIC_3;d[4]=JOB_PROTOCOL_VERSION;d[5]=JOB_IR_VERSION;job_write_u16_le(d+6,JOB_HEADER_SIZE);job_write_u32_le(d+8,j->job_id);job_write_u32_le(d+12,j->size);job_write_u32_le(d+16,j->size-JOB_HEADER_SIZE);job_write_u16_le(d+20,j->count);job_write_u16_le(d+22,0u);job_write_u32_le(d+24,JOB_MAX_STEPS);job_write_u32_le(d+28,JOB_MAX_RUNTIME_US);job_write_u16_le(d+32,JOB_MAX_RESULT_BYTES);job_write_u16_le(d+34,JOB_MAX_EMITS);job_write_u32_le(d+36,JOB_MAX_EMIT_BYTES);}
int test_load(test_job_t*j,uint8_t r,uint32_t v){uint8_t p[5];p[0]=r;job_write_u32_le(p+1,v);return test_job_ins(j,JOB_OP_LOAD_CONST,p,5u);}
int test_tri(test_job_t*j,uint8_t op,uint8_t d,uint8_t a,uint8_t b){uint8_t p[3]={d,a,b};return test_job_ins(j,op,p,3u);}
int test_return(test_job_t*j,uint8_t mask){return test_job_ins(j,JOB_OP_RETURN,&mask,1u);}
