#include "job_runtime.h"
#include "job_ir.h"
#include "runa_ir.h"
#include "runa_limits.h"
#include "runa_opcode.h"
#include "runa_capabilities.h"
#include "runa_result.h"
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
static size_t make_extension_job(uint8_t *job, uint32_t job_id, uint16_t module_id,
                                 uint8_t operation, const uint8_t *payload, uint16_t payload_size) {
 size_t extension_size=(size_t)5u+payload_size;size_t instruction_bytes=extension_size+3u;size_t total=RUNA_HEADER_SIZE+instruction_bytes;uint8_t *p=job+RUNA_HEADER_SIZE;
 memset(job,0,total);job[0]='J';job[1]='E';job[2]='X';job[3]='E';job[4]=RUNA_PROTOCOL_VERSION;job[5]=RUNA_IR_VERSION_V2;
 runa_write_u16_le(job+6u,RUNA_HEADER_SIZE);runa_write_u32_le(job+8u,job_id);runa_write_u32_le(job+12u,(uint32_t)total);runa_write_u32_le(job+16u,(uint32_t)instruction_bytes);
 runa_write_u16_le(job+20u,2u);runa_write_u32_le(job+24u,10000u);runa_write_u32_le(job+28u,5000000u);runa_write_u16_le(job+32u,32u);runa_write_u16_le(job+34u,128u);runa_write_u32_le(job+36u,512u);
 p[0]=RUNA_OP_EXT;p[1]=(uint8_t)(3u+payload_size);runa_write_u16_le(p+2u,module_id);p[4]=operation;if(payload_size!=0u)memcpy(p+5u,payload,payload_size);p+=extension_size;p[0]=RUNA_OP_RETURN;p[1]=1u;p[2]=0u;return total;
}
static FILE *open_read(const char *path) {
 FILE *file=NULL;
#ifdef _MSC_VER
 if(fopen_s(&file,path,"rb")!=0)return NULL;
#else
 file=fopen(path,"rb");
#endif
 return file;
}
static FILE *open_write(const char *path) {
 FILE *file=NULL;
#ifdef _MSC_VER
 if(fopen_s(&file,path,"wb")!=0)return NULL;
#else
 file=fopen(path,"wb");
#endif
 return file;
}
int main(int argc,char **argv){FILE *file=NULL;long length;uint8_t data[JOB_MAX_BYTES];size_t count;
 if(argc==4&&strcmp(argv[1],"--capability-batch")==0){FILE *output=open_write(argv[3]);uint8_t header[4],blob[RUNA_MAX_CAPABILITY_BYTES];
  if(output==NULL)return 3;
  file=open_read(argv[2]);if(file==NULL){fclose(output);return 3;}
  while(fread(header,1,4u,file)==4u){uint32_t size=job_read_u32_le(header),status=UINT32_MAX,count_value=0u;uint8_t out[8];runa_capabilities_view_t view;
   if(size<=sizeof blob&&fread(blob,1,size,file)==size){runa_status_t result=runa_capabilities_decode(blob,size,&view);status=(uint32_t)result;if(result==RUNA_OK)count_value=view.module_count;}
   runa_write_u32_le(out,status);runa_write_u32_le(out+4u,count_value);if(fwrite(out,1,sizeof out,output)!=sizeof out){fclose(file);fclose(output);return 6;}
  }fclose(file);fclose(output);return 0;
 }
 if(argc==4&&strcmp(argv[1],"--module-data-batch")==0){FILE *output=open_write(argv[3]);uint8_t header[4];
  if(output==NULL)return 3;
  file=open_read(argv[2]);if(file==NULL){fclose(output);return 3;}
  while(fread(header,1,4u,file)==4u){uint32_t size=job_read_u32_le(header),status=1u;uint16_t module=0u,instruction=0u,sequence=0u,payload=0u;uint8_t blob[64],out[12];
   if(size<=sizeof blob){size_t read_size=fread(blob,1,size,file);if(size>=16u&&read_size==size&&blob[0]==RUNA_EVENT_MODULE_DATA&&blob[1]==1u&&job_read_u16_le(blob+2u)==size&&job_read_u16_le(blob+14u)<=48u&&16u+job_read_u16_le(blob+14u)==size&&job_read_u16_le(blob+8u)!=0u){status=0u;module=job_read_u16_le(blob+8u);instruction=job_read_u16_le(blob+10u);sequence=job_read_u16_le(blob+12u);payload=job_read_u16_le(blob+14u);}}else{fseek(file,(long)size,SEEK_CUR);}
   runa_write_u32_le(out,status);runa_write_u16_le(out+4u,module);runa_write_u16_le(out+6u,instruction);runa_write_u16_le(out+8u,sequence);runa_write_u16_le(out+10u,payload);if(fwrite(out,1,sizeof out,output)!=sizeof out){fclose(file);fclose(output);return 6;}
  }fclose(file);fclose(output);return 0;
 }
 if(argc==4&&strcmp(argv[1],"--encode-batch")==0){FILE *output=open_write(argv[3]);uint8_t job[JOB_MAX_BYTES],payload[252];uint8_t record[7],length_bytes[2];uint32_t job_id;uint16_t module_id,payload_size;uint8_t operation;
  if(output==NULL)return 3;
  file=open_read(argv[2]);
  if(file==NULL){fclose(output);return 3;}
  while(fread(record,1,7u,file)==7u){job_id=job_read_u32_le(record);module_id=job_read_u16_le(record+4u);operation=record[6];if(fread(length_bytes,1,2u,file)!=2u){fclose(file);fclose(output);return 5;}payload_size=job_read_u16_le(length_bytes);if(payload_size>sizeof payload||fread(payload,1,payload_size,file)!=payload_size){fclose(file);fclose(output);return 5;}{size_t job_size=make_extension_job(job,job_id,module_id,operation,payload,payload_size);uint8_t size_bytes[4];runa_write_u32_le(size_bytes,(uint32_t)job_size);if(fwrite(size_bytes,1,4u,output)!=4u||fwrite(job,1,job_size,output)!=job_size){fclose(file);fclose(output);return 6;}}}
  fclose(file);fclose(output);return 0;
 }
 if(argc==3&&strcmp(argv[1],"--batch")==0){uint64_t hash=UINT64_C(1469598103934665603);uint8_t header[4];uint32_t cases=0u;
  file=open_read(argv[2]);if(!file)return 3;
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
