#include "job_runtime.h"
#include "job_ir.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { uint32_t value; int got_result; } output_t;
static int sink(void *context,const uint8_t *event,size_t size){output_t *o=context;if(size>=24u&&event[0]==JOB_EVENT_RESULT&&event[8]==JOB_RESULT_OK&&job_read_u16_le(event+16)>=4u){o->value=job_read_u32_le(event+20);o->got_result=1;}return 0;}
static uint64_t time_us(void *context){(void)context;return 0u;}
int main(int argc,char **argv){FILE *file=NULL;long length;uint8_t data[JOB_MAX_BYTES];size_t count;job_resource_table_t resources={NULL,0u};job_hal_t hal={0};job_event_sink_t events;output_t output={0u,0};job_execution_summary_t summary;
 if(argc!=2)return 2;
#ifdef _MSC_VER
 if(fopen_s(&file,argv[1],"rb")!=0)return 3;
#else
 file=fopen(argv[1],"rb");if(!file)return 3;
#endif
 if(fseek(file,0,SEEK_END)!=0){fclose(file);return 4;}length=ftell(file);if(length<0||length>(long)sizeof data){fclose(file);return 5;}rewind(file);count=fread(data,1,(size_t)length,file);fclose(file);if(count!=(size_t)length)return 6;
 hal.time_us=time_us;events.send=sink;events.context=&output;summary=job_process(data,count,&resources,&hal,&events);if(summary.error!=JOB_OK||!output.got_result)return 7;printf("%u\n",output.value);return 0;}
