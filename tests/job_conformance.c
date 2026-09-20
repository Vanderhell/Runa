#include "job_runtime.h"
#include "job_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef struct { uint32_t value; int got_result; } output_t;
static int sink(void *context,const uint8_t *event,size_t size){output_t *o=context;if(size>=24u&&event[0]==JOB_EVENT_RESULT&&event[8]==JOB_RESULT_OK&&job_read_u16_le(event+16)>=4u){o->value=job_read_u32_le(event+20);o->got_result=1;}return 0;}
static uint64_t time_us(void *context){(void)context;return 0u;}
static uint64_t hash_bytes(uint64_t hash, const uint8_t *data, size_t size) {
 size_t i; for(i=0u;i<size;++i){hash^=data[i];hash*=UINT64_C(1099511628211);} return hash;
}
static int run_one(const uint8_t *data,size_t count,uint32_t *value) {
 job_resource_table_t resources={NULL,0u};job_hal_t hal={0};job_event_sink_t events;output_t output={0u,0};job_execution_summary_t summary;
 hal.time_us=time_us;events.send=sink;events.context=&output;summary=job_process(data,count,&resources,&hal,&events);
 if(summary.error!=JOB_OK||!output.got_result)return 0;
 *value=output.value;
 return 1;
}
int main(int argc,char **argv){FILE *file=NULL;long length;uint8_t data[JOB_MAX_BYTES];size_t count;
 if(argc==3&&strcmp(argv[1],"--batch")==0){uint64_t hash=UINT64_C(1469598103934665603);uint8_t header[4];uint32_t cases=0u;
  file=fopen(argv[2],"rb");if(!file)return 3;
  while(fread(header,1,4u,file)==4u){uint32_t size=job_read_u32_le(header);uint32_t value;
   if(size==0u||size>sizeof data||fread(data,1,size,file)!=size||!run_one(data,size,&value)){fclose(file);return 7;}
   hash=hash_bytes(hash,data,size);hash=hash_bytes(hash,(const uint8_t *)&value,sizeof value);++cases;
  }
  fclose(file);printf("%" PRIu64 " %u\n",hash,cases);return 0;
 }
 if(argc!=2)return 2;
#ifdef _MSC_VER
 if(fopen_s(&file,argv[1],"rb")!=0)return 3;
#else
 file=fopen(argv[1],"rb");if(!file)return 3;
#endif
 if(fseek(file,0,SEEK_END)!=0){fclose(file);return 4;}length=ftell(file);if(length<0||length>(long)sizeof data){fclose(file);return 5;}rewind(file);count=fread(data,1,(size_t)length,file);fclose(file);if(count!=(size_t)length)return 6;
 {uint32_t value;if(!run_one(data,count,&value))return 7;printf("%u\n",value);return 0;}}
