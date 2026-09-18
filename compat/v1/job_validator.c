#include "job_validator.h"

static uint8_t expected_size(uint8_t op) {
    switch(op) {
    case JOB_OP_NOP:return 0u; case JOB_OP_LOAD_CONST:return 5u;
    case JOB_OP_MOV:case JOB_OP_NOT:return 2u;
    case JOB_OP_ADD:case JOB_OP_SUB:case JOB_OP_AND:case JOB_OP_OR:case JOB_OP_XOR:case JOB_OP_SHL:case JOB_OP_SHR:
    case JOB_OP_CMP_EQ:case JOB_OP_CMP_NE:case JOB_OP_CMP_LT:case JOB_OP_CMP_LE:case JOB_OP_CMP_GT:case JOB_OP_CMP_GE:return 3u;
    case JOB_OP_JUMP:return 2u; case JOB_OP_JUMP_IF:case JOB_OP_JUMP_IF_NOT:return 3u;
    case JOB_OP_GPIO_READ:case JOB_OP_GPIO_WRITE:case JOB_OP_ADC_READ:case JOB_OP_PWM_WRITE:return 3u;
    case JOB_OP_DELAY_MS:return 4u; case JOB_OP_EMIT:case JOB_OP_RETURN:return 1u;
    default:return UINT8_MAX;
    }
}
const job_resource_t *job_resource_find(const job_resource_table_t *t,uint16_t id){
    size_t i;if(!t||(t->count&&!t->items)||t->count>JOB_MAX_RESOURCES)return NULL;
    for(i=0;i<t->count;i++) {
        if(t->items[i].id==id) return &t->items[i];
    }
    return NULL;
}
static job_error_t resource_ok(const job_resource_table_t *t,uint16_t id,uint8_t type,uint8_t perm){
    const job_resource_t *r=job_resource_find(t,id);if(!r)return JOB_ERR_INVALID_RESOURCE;
    if(r->type!=type) return JOB_ERR_RESOURCE_TYPE;
    if((r->permissions&perm)!=perm) return JOB_ERR_ACCESS_DENIED;
    return JOB_OK;
}
static job_error_t operands_ok(const job_instruction_t *i,const job_resource_table_t *t,uint32_t *detail){
    const uint8_t *p=i->operands;uint16_t id;uint8_t n=expected_size(i->opcode);
    if(n==UINT8_MAX) return JOB_ERR_INVALID_OPCODE;
    if(i->operand_size!=n) return JOB_ERR_INVALID_OPERAND;
    switch(i->opcode){
    case JOB_OP_LOAD_CONST:if(p[0]>=JOB_REGISTER_COUNT)return JOB_ERR_INVALID_REGISTER;break;
    case JOB_OP_MOV:case JOB_OP_NOT:if(p[0]>=JOB_REGISTER_COUNT||p[1]>=JOB_REGISTER_COUNT)return JOB_ERR_INVALID_REGISTER;break;
    case JOB_OP_ADD:case JOB_OP_SUB:case JOB_OP_AND:case JOB_OP_OR:case JOB_OP_XOR:case JOB_OP_SHL:case JOB_OP_SHR:
    case JOB_OP_CMP_EQ:case JOB_OP_CMP_NE:case JOB_OP_CMP_LT:case JOB_OP_CMP_LE:case JOB_OP_CMP_GT:case JOB_OP_CMP_GE:
        if(p[0]>=JOB_REGISTER_COUNT||p[1]>=JOB_REGISTER_COUNT||p[2]>=JOB_REGISTER_COUNT) return JOB_ERR_INVALID_REGISTER;
        break;
    case JOB_OP_JUMP_IF:case JOB_OP_JUMP_IF_NOT:if(p[0]>=JOB_REGISTER_COUNT)return JOB_ERR_INVALID_REGISTER;break;
    case JOB_OP_GPIO_READ:case JOB_OP_ADC_READ:
        id=job_read_u16_le(p);*detail=id;if(p[2]>=JOB_REGISTER_COUNT)return JOB_ERR_INVALID_REGISTER;
        return resource_ok(t,id,i->opcode==JOB_OP_GPIO_READ?JOB_RESOURCE_GPIO:JOB_RESOURCE_ADC,JOB_PERMISSION_READ);
    case JOB_OP_GPIO_WRITE:case JOB_OP_PWM_WRITE:
        id=job_read_u16_le(p);*detail=id;if(p[2]>=JOB_REGISTER_COUNT)return JOB_ERR_INVALID_REGISTER;
        return resource_ok(t,id,i->opcode==JOB_OP_GPIO_WRITE?JOB_RESOURCE_GPIO:JOB_RESOURCE_PWM,JOB_PERMISSION_WRITE);
    case JOB_OP_DELAY_MS:if(job_read_u32_le(p)>JOB_MAX_SINGLE_DELAY_MS)return JOB_ERR_OUT_OF_RANGE;break;
    case JOB_OP_EMIT:if(p[0]>=JOB_REGISTER_COUNT)return JOB_ERR_INVALID_REGISTER;break;
    default:break;
    }return JOB_OK;
}
job_error_t job_validate(const uint8_t *d,size_t n,const job_resource_table_t *t,job_header_t *h,job_validation_error_t *e){
    uint32_t offs[JOB_MAX_INSTRUCTIONS],off=JOB_HEADER_SIZE;uint16_t k;job_error_t rc;job_instruction_t in;
    if(!e||!h) return JOB_ERR_INVALID_FORMAT;
    e->code=JOB_OK;e->instruction_index=UINT16_MAX;e->detail=0u;
    rc=job_decode_header(d,n,h);if(rc){e->code=rc;return rc;}
    for(k=0;k<h->instruction_count;k++){offs[k]=off;rc=job_decode_instruction(d,n,off,&in);if(!rc)rc=operands_ok(&in,t,&e->detail);
        if(rc){e->code=rc;e->instruction_index=k;return rc;}off+=(uint32_t)(2u+in.operand_size);}
    if(off!=h->total_size){e->code=JOB_ERR_INVALID_FORMAT;return e->code;}
    off=JOB_HEADER_SIZE;for(k=0;k<h->instruction_count;k++){uint16_t target;
        (void)job_decode_instruction(d,n,off,&in);
        if(in.opcode==JOB_OP_JUMP)target=job_read_u16_le(in.operands);
        else if(in.opcode==JOB_OP_JUMP_IF||in.opcode==JOB_OP_JUMP_IF_NOT)target=job_read_u16_le(in.operands+1u);
        else{off+=(uint32_t)(2u+in.operand_size);continue;}
        if(target>=h->instruction_count||offs[target]>=h->total_size){e->code=JOB_ERR_INVALID_JUMP;e->instruction_index=k;e->detail=target;return e->code;}
        off+=(uint32_t)(2u+in.operand_size);
    }return JOB_OK;
}
