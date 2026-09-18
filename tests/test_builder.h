#ifndef TEST_BUILDER_H
#define TEST_BUILDER_H
#include "job_ir.h"
typedef struct {uint8_t data[JOB_MAX_BYTES];uint32_t size;uint16_t count;uint32_t job_id;} test_job_t;
void test_job_init(test_job_t *,uint32_t);
int test_job_ins(test_job_t *,uint8_t,const uint8_t *,uint8_t);
void test_job_finish(test_job_t *);
int test_load(test_job_t *,uint8_t,uint32_t);
int test_tri(test_job_t *,uint8_t,uint8_t,uint8_t,uint8_t);
int test_return(test_job_t *,uint8_t);
#endif
