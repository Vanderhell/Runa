#include "job_validator.h"
#include "runa_validator.h"
#include <stdio.h>
#include <stdint.h>
int main(int argc,char **argv){uint8_t b[JOB_MAX_BYTES];job_header_t h;job_validation_error_t e;job_resource_table_t r={NULL,0u};runa_decoded_job_t rh;runa_validation_error_t re;runa_module_registry_t registry;runa_resource_table_t rr={NULL,0u};size_t n;FILE*f=NULL;if(argc!=2)return 0;
#ifdef _MSC_VER
if(fopen_s(&f,argv[1],"rb")!=0)return 2;
#else
f=fopen(argv[1],"rb");if(!f)return 2;
#endif
n=fread(b,1,sizeof b,f);fclose(f);(void)job_validate(b,n,&r,&h,&e);runa_registry_init(&registry);(void)runa_validate(b,n,&registry,&rr,&rh,&re);return 0;}
