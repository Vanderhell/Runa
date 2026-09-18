#include "job_runtime.h"
#include "job_validator.h"
#include "job_mock_hal.h"
#include "test_builder.h"
#include <stdio.h>
#include <string.h>

static unsigned tests,failures;
#define CHECK(x) do{tests++;if(!(x)){failures++;printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);}}while(0)
typedef struct{uint8_t bytes[512][JOB_MAX_EVENT_BYTES];size_t sizes[512];uint32_t count;} capture_t;
static int capture(void*c,const uint8_t*p,size_t n){capture_t*x=c;if(x->count>=512u||n>JOB_MAX_EVENT_BYTES)return -1;memcpy(x->bytes[x->count],p,n);x->sizes[x->count]=n;x->count++;return 0;}
static const job_resource_t resource_items[]={
 {1u,JOB_RESOURCE_GPIO,JOB_PERMISSION_READ|JOB_PERMISSION_WRITE,1u,1u},
 {2u,JOB_RESOURCE_ADC,JOB_PERMISSION_READ,2u,4095u},
 {3u,JOB_RESOURCE_PWM,JOB_PERMISSION_WRITE,3u,10000u},
 {65535u,JOB_RESOURCE_GPIO,JOB_PERMISSION_READ,4u,1u}};
static const job_resource_table_t resources={resource_items,sizeof resource_items/sizeof resource_items[0]};
static job_execution_summary_t run(test_job_t*j,job_mock_hal_t*m,capture_t*c){job_hal_t h=job_mock_hal_interface(m);job_event_sink_t s={capture,c};test_job_finish(j);return job_process(j->data,j->size,&resources,&h,&s);}
static uint32_t result_value(capture_t*c,uint32_t index){uint8_t*p=c->bytes[c->count-1u];return job_read_u32_le(p+20u+index*4u);}

static void decoder_tests(void){test_job_t j;job_header_t h;uint32_t n;test_job_init(&j,7u);test_return(&j,0u);test_job_finish(&j);CHECK(job_decode_header(j.data,j.size,&h)==JOB_OK);CHECK(h.job_id==7u);
 for(n=0;n<JOB_HEADER_SIZE;n++)CHECK(job_decode_header(j.data,n,&h)==JOB_ERR_INVALID_FORMAT);
 j.data[0]='X';CHECK(job_decode_header(j.data,j.size,&h)==JOB_ERR_BAD_MAGIC);j.data[0]=JOB_MAGIC_0;j.data[4]=2u;CHECK(job_decode_header(j.data,j.size,&h)==JOB_ERR_UNSUPPORTED_PROTOCOL);j.data[4]=1u;j.data[5]=2u;CHECK(job_decode_header(j.data,j.size,&h)==JOB_ERR_UNSUPPORTED_IR_VERSION);
 j.data[5]=1u;job_write_u32_le(j.data+12,j.size+1u);CHECK(job_decode_header(j.data,j.size,&h)==JOB_ERR_INVALID_FORMAT);CHECK(job_decode_header(NULL,0u,&h)==JOB_ERR_INVALID_FORMAT);CHECK(job_decode_header(j.data,j.size,NULL)==JOB_ERR_INVALID_FORMAT);}
static void validator_tests(void){test_job_t j;job_header_t h;job_validation_error_t e;uint8_t p[3];uint32_t cut;
 test_job_init(&j,1u);test_load(&j,0u,1u);test_return(&j,1u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)==JOB_OK);
 for(cut=JOB_HEADER_SIZE;cut<j.size;cut++){uint32_t old=j.size;job_write_u32_le(j.data+12,cut);job_write_u32_le(j.data+16,cut-JOB_HEADER_SIZE);CHECK(job_validate(j.data,cut,&resources,&h,&e)!=JOB_OK);job_write_u32_le(j.data+12,old);job_write_u32_le(j.data+16,old-JOB_HEADER_SIZE);}
 test_job_init(&j,1u);p[0]=8u;test_job_ins(&j,JOB_OP_EMIT,p,1u);test_return(&j,0u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)==JOB_ERR_INVALID_REGISTER);
 test_job_init(&j,1u);p[0]=99u;test_job_ins(&j,99u,p,0u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)==JOB_ERR_INVALID_OPCODE);
 test_job_init(&j,1u);job_write_u16_le(p,99u);p[2]=0u;test_job_ins(&j,JOB_OP_ADC_READ,p,3u);test_return(&j,0u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)==JOB_ERR_INVALID_RESOURCE);
 test_job_init(&j,1u);job_write_u16_le(p,1u);p[2]=0u;test_job_ins(&j,JOB_OP_ADC_READ,p,3u);test_return(&j,0u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)==JOB_ERR_RESOURCE_TYPE);
 test_job_init(&j,1u);job_write_u16_le(p,2u);p[2]=0u;test_job_ins(&j,JOB_OP_GPIO_WRITE,p,3u);test_return(&j,0u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)!=JOB_OK);
 test_job_init(&j,1u);job_write_u16_le(p,10u);test_job_ins(&j,JOB_OP_JUMP,p,2u);test_job_finish(&j);CHECK(job_validate(j.data,j.size,&resources,&h,&e)==JOB_ERR_INVALID_JUMP);}
static void arithmetic_tests(void){static const uint8_t ops[]={JOB_OP_ADD,JOB_OP_SUB,JOB_OP_AND,JOB_OP_OR,JOB_OP_XOR,JOB_OP_SHL,JOB_OP_SHR,JOB_OP_CMP_EQ,JOB_OP_CMP_NE,JOB_OP_CMP_LT,JOB_OP_CMP_LE,JOB_OP_CMP_GT,JOB_OP_CMP_GE};static const uint32_t want[]={0u,0xfffffffeu,1u,0xffffffffu,0xfffffffeu,0xfffffffeu,0x7fffffffu,0u,1u,0u,0u,1u,1u};size_t z;
 for(z=0;z<sizeof ops;z++){test_job_t j;job_mock_hal_t m;capture_t c={0};job_execution_summary_t s;test_job_init(&j,(uint32_t)z);test_load(&j,1u,0xffffffffu);test_load(&j,2u,1u);test_tri(&j,ops[z],0u,1u,2u);test_return(&j,1u);job_mock_hal_init(&m);s=run(&j,&m,&c);CHECK(s.error==JOB_OK);CHECK(c.count==2u);CHECK(result_value(&c,0u)==want[z]);}
 {test_job_t j;job_mock_hal_t m;capture_t c={0};uint8_t p[2]={7u,0u};test_job_init(&j,2u);test_load(&j,0u,0u);test_job_ins(&j,JOB_OP_NOT,p,2u);test_return(&j,128u);job_mock_hal_init(&m);run(&j,&m,&c);CHECK(result_value(&c,0u)==0xffffffffu);}}
static void branch_limit_tests(void){test_job_t j;job_mock_hal_t m;capture_t c={0};uint8_t p[3];job_execution_summary_t s;
 test_job_init(&j,4u);test_load(&j,0u,1u);p[0]=0u;job_write_u16_le(p+1,3u);test_job_ins(&j,JOB_OP_JUMP_IF,p,3u);test_load(&j,1u,9u);test_return(&j,1u);job_mock_hal_init(&m);s=run(&j,&m,&c);CHECK(s.error==JOB_OK);CHECK(result_value(&c,0u)==1u);
 test_job_init(&j,5u);job_write_u16_le(p,0u);test_job_ins(&j,JOB_OP_JUMP,p,2u);test_job_finish(&j);job_write_u32_le(j.data+24,5u);{job_hal_t h=job_mock_hal_interface(&m);job_event_sink_t es={capture,&c};memset(&c,0,sizeof c);s=job_process(j.data,j.size,&resources,&h,&es);}CHECK(s.error==JOB_ERR_STEP_LIMIT);CHECK(s.result_sent==1u);}
static void hardware_protocol_tests(void){test_job_t j;job_mock_hal_t m;capture_t c={0};uint8_t p[4];job_execution_summary_t s;job_mock_hal_init(&m);m.adc[2]=3210u;
 test_job_init(&j,8u);job_write_u16_le(p,2u);p[2]=0u;test_job_ins(&j,JOB_OP_ADC_READ,p,3u);test_job_ins(&j,JOB_OP_EMIT,p+2,1u);test_load(&j,1u,5000u);job_write_u16_le(p,3u);p[2]=1u;test_job_ins(&j,JOB_OP_PWM_WRITE,p,3u);job_write_u32_le(p,10u);test_job_ins(&j,JOB_OP_DELAY_MS,p,4u);test_return(&j,1u);s=run(&j,&m,&c);CHECK(s.error==JOB_OK);CHECK(c.count==3u);CHECK(c.bytes[0][0]==JOB_EVENT_ACK);CHECK(c.bytes[1][0]==JOB_EVENT_EMIT);CHECK(c.bytes[2][0]==JOB_EVENT_RESULT);CHECK(m.write_count==1u);CHECK(m.now_us==10000u);CHECK(result_value(&c,0u)==3210u);
 memset(&c,0,sizeof c);job_mock_hal_init(&m);m.next_status=JOB_HAL_FAILURE;test_job_init(&j,9u);job_write_u16_le(p,2u);p[2]=0u;test_job_ins(&j,JOB_OP_ADC_READ,p,3u);test_return(&j,0u);s=run(&j,&m,&c);CHECK(s.error==JOB_ERR_ADC);CHECK(c.count==2u);
 memset(&c,0,sizeof c);job_mock_hal_init(&m);test_job_init(&j,10u);test_load(&j,0u,10001u);job_write_u16_le(p,3u);p[2]=0u;test_job_ins(&j,JOB_OP_PWM_WRITE,p,3u);test_return(&j,0u);s=run(&j,&m,&c);CHECK(s.error==JOB_ERR_OUT_OF_RANGE);CHECK(m.calls==0u);}
static void exact_limits_and_replay(void){test_job_t j;job_mock_hal_t m1,m2;capture_t a={0},b={0};uint8_t r=0u;job_execution_summary_t s;
 test_job_init(&j,11u);test_load(&j,0u,42u);test_job_ins(&j,JOB_OP_EMIT,&r,1u);test_return(&j,1u);job_mock_hal_init(&m1);job_mock_hal_init(&m2);run(&j,&m1,&a);run(&j,&m2,&b);CHECK(a.count==b.count);CHECK(memcmp(&a,&b,sizeof a)==0);
 test_job_finish(&j);job_write_u16_le(j.data+34,0u);{job_hal_t h=job_mock_hal_interface(&m1);job_event_sink_t es={capture,&a};memset(&a,0,sizeof a);s=job_process(j.data,j.size,&resources,&h,&es);}CHECK(s.error==JOB_ERR_EMIT_LIMIT);
 test_job_finish(&j);job_write_u16_le(j.data+32,3u);{job_hal_t h=job_mock_hal_interface(&m1);job_event_sink_t es={capture,&a};memset(&a,0,sizeof a);s=job_process(j.data,j.size,&resources,&h,&es);}CHECK(s.error==JOB_ERR_RESULT_LIMIT);}
static void malformed_property(void){uint32_t seed=0x12345678u,i;job_header_t h;job_validation_error_t e;uint8_t data[JOB_MAX_BYTES];for(i=0;i<20000u;i++){size_t n,k;seed=seed*1664525u+1013904223u;n=seed%(JOB_MAX_BYTES+1u);for(k=0;k<n;k++){seed=seed*1664525u+1013904223u;data[k]=(uint8_t)(seed>>24u);}CHECK(job_validate(data,n,&resources,&h,&e)>=JOB_OK);}}
int main(void){decoder_tests();validator_tests();arithmetic_tests();branch_limit_tests();hardware_protocol_tests();exact_limits_and_replay();malformed_property();printf("%u checks, %u failures\n",tests,failures);return failures?1:0;}
