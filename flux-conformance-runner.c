/*
 * flux-conformance-runner.c — C conformance runner for FLUX ISA
 * Runs all 113 test vectors from SuperInstance/flux-conformance
 * Build: gcc -std=c11 -O2 -o runner flux-conformance-runner.c -lm
 * Run:   ./runner vectors.json
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* Flags */
#define F_Z 0x01
#define F_S 0x02
#define F_C 0x04
#define F_O 0x08

/* Opcodes matching conformance_core.py */
enum { HALT=0x00, NOP=0x01, BRK=0x02 };
enum { ADD=0x10, SUB=0x11, MUL=0x12, DIV=0x13, MOD=0x14, NEG=0x15, INC=0x16, DEC=0x17 };
enum { EQ=0x20, NE=0x21, LT=0x22, LE=0x23, GT=0x24, GE=0x25 };
enum { AND=0x30, OR=0x31, XOR=0x32, NOT=0x33, SHL=0x34, SHR=0x35 };
enum { LD=0x40, ST=0x41, PEEK=0x43, POKE=0x44 };
enum { JMP=0x50, JZ=0x51, JNZ=0x52, CALL=0x53, RET=0x54, PUSH=0x55, POP=0x56 };
enum { DUP=0x60, SWAP=0x61, OVER=0x62, ROT=0x63 };
enum { FADD=0x70, FSUB=0x71, FMUL=0x72, FDIV=0x73 };
enum { CONF_G=0x80, CONF_S=0x81, CONF_M=0x82 };
enum { SIGNAL=0x90, BCAST=0x91, LISTEN=0x92 };

#define STACK_MAX 256
#define CODE_MAX  4096
#define MEM_MAX   65536
#define CALL_MAX  256
#define SIG_CH    256
#define SIG_Q     32

typedef struct {
    uint8_t code[CODE_MAX]; int clen;
    double stk[STACK_MAX]; int sp;
    uint8_t mem[MEM_MAX];
    int flags; float conf; int pc;
    int cstk[CALL_MAX]; int csp;
    int halted;
    double sig_q[SIG_CH][SIG_Q]; int sig_h[SIG_CH]; int sig_t[SIG_CH];
} VM;

static void vm_init(VM *v) { memset(v, 0, sizeof(VM)); v->conf = 1.0f; }

static int push(VM *v, double val) {
    if (v->sp >= STACK_MAX) return -1;
    v->stk[v->sp++] = val; return 0;
}
static int pop(VM *v, double *val) {
    if (v->sp <= 0) return -1;
    *val = v->stk[--v->sp]; return 0;
}
static int peek(VM *v, int off, double *val) {
    int i = v->sp - 1 - off;
    if (i < 0 || i >= v->sp) return -1;
    *val = v->stk[i]; return 0;
}

static uint8_t ru8(VM *v) { return v->code[v->pc++]; }
static uint16_t ru16(VM *v) { uint16_t r = v->code[v->pc] | (v->code[v->pc+1]<<8); v->pc+=2; return r; }
static int32_t ri32(VM *v) { int32_t r = v->code[v->pc]|(v->code[v->pc+1]<<8)|(v->code[v->pc+2]<<16)|(v->code[v->pc+3]<<24); v->pc+=4; return r; }

static void set_arith_flags(VM *v, int64_t r, int64_t a, int64_t b, int is_sub) {
    v->flags = 0;
    if (r == 0) v->flags |= F_Z;
    if (r < 0)  v->flags |= F_S;
    if (is_sub) { v->flags |= ((a >= 0 && b >= 0 && a < b) ? F_C : 0); }
    else { uint64_t ua = (uint32_t)a, ub = (uint32_t)b; if (ua + ub > 0xFFFFFFFFUL) v->flags |= F_C; }
    if ((a > 0 && b > 0 && r < 0) || (a < 0 && b < 0 && r > 0)) v->flags |= F_O;
}

static void set_logic_flags(VM *v, int64_t r) {
    v->flags = 0;
    if (r == 0) v->flags |= F_Z;
    if (r < 0)  v->flags |= F_S;
}

static int vm_run(VM *v) {
    while (!v->halted && v->pc < v->clen) {
        uint8_t op = ru8(v);
        double da, db; int64_t ia, ib, ir; uint16_t addr;

        switch (op) {
        case HALT: v->halted = 1; break;
        case NOP: case BRK: if (op==BRK) v->halted=1; break;

        /* Arithmetic */
        case ADD: pop(v,&db); pop(v,&da); ia=(int64_t)da; ib=(int64_t)db; ir=ia+ib; push(v,(double)ir); set_arith_flags(v,ir,ia,ib,0); break;
        case SUB: pop(v,&db); pop(v,&da); ia=(int64_t)da; ib=(int64_t)db; ir=ia-ib; push(v,(double)ir); set_arith_flags(v,ir,ia,ib,1); break;
        case MUL: pop(v,&db); pop(v,&da); ia=(int64_t)da; ib=(int64_t)db; ir=ia*ib; push(v,(double)ir); set_arith_flags(v,ir,ia,ib,0); break;
        case DIV: pop(v,&db); pop(v,&da); ia=(int64_t)da; ib=(int64_t)db; if(!ib)return-1; ir=ia/ib; push(v,(double)ir); set_arith_flags(v,ir,ia,ib,0); break;
        case MOD: pop(v,&db); pop(v,&da); ia=(int64_t)da; ib=(int64_t)db; if(!ib)return-1; ir=ia%ib; if(ir!=0&&((ir^ib)<0))ir+=ib; push(v,(double)ir); set_arith_flags(v,ir,ia,ib,0); break;
        case NEG: pop(v,&da); ir=-(int64_t)da; push(v,(double)ir); set_arith_flags(v,ir,ir,0,0); break;
        case INC: pop(v,&da); ir=(int64_t)da+1; push(v,(double)ir); set_arith_flags(v,ir,ir-1,1,0); break;
        case DEC: pop(v,&da); ir=(int64_t)da-1; push(v,(double)ir); set_arith_flags(v,ir,(int64_t)da,1,1); break;

        /* Comparison */
        case EQ: pop(v,&db); pop(v,&da); ir=((int64_t)da==((int64_t)db))?1:0; push(v,(double)ir); set_logic_flags(v,ir); break;
        case NE: pop(v,&db); pop(v,&da); ir=((int64_t)da!=((int64_t)db))?1:0; push(v,(double)ir); set_logic_flags(v,ir); break;
        case LT: pop(v,&db); pop(v,&da); ir=((int64_t)da<((int64_t)db))?1:0; push(v,(double)ir); set_logic_flags(v,ir); break;
        case LE: pop(v,&db); pop(v,&da); ir=((int64_t)da<=((int64_t)db))?1:0; push(v,(double)ir); set_logic_flags(v,ir); break;
        case GT: pop(v,&db); pop(v,&da); ir=((int64_t)da>((int64_t)db))?1:0; push(v,(double)ir); set_logic_flags(v,ir); break;
        case GE: pop(v,&db); pop(v,&da); ir=((int64_t)da>=((int64_t)db))?1:0; push(v,(double)ir); set_logic_flags(v,ir); break;

        /* Logic/bitwise */
        case AND: pop(v,&db); pop(v,&da); ir=(int64_t)da&(int64_t)db; push(v,(double)ir); set_logic_flags(v,ir); break;
        case OR:  pop(v,&db); pop(v,&da); ir=(int64_t)da|(int64_t)db; push(v,(double)ir); set_logic_flags(v,ir); break;
        case XOR: pop(v,&db); pop(v,&da); ir=(int64_t)da^(int64_t)db; push(v,(double)ir); set_logic_flags(v,ir); break;
        case NOT: pop(v,&da); ir=~(int64_t)da; push(v,(double)ir); set_logic_flags(v,ir); break;
        case SHL: pop(v,&db); pop(v,&da); ir=(int64_t)((uint64_t)(int64_t)da<<(int64_t)db); push(v,(double)ir); set_logic_flags(v,ir); break;
        case SHR: pop(v,&db); pop(v,&da); ir=(int64_t)da>>(int64_t)db; push(v,(double)ir); set_logic_flags(v,ir); break;

        /* Memory */
        case LD: addr=ru16(v); { int32_t val=v->mem[addr]|(v->mem[addr+1]<<8)|(v->mem[addr+2]<<16)|(v->mem[addr+3]<<24); push(v,(double)val); break; }
        case ST: addr=ru16(v); pop(v,&da); { int32_t val=(int32_t)da; v->mem[addr]=(uint8_t)(val); v->mem[addr+1]=(uint8_t)(val>>8); v->mem[addr+2]=(uint8_t)(val>>16); v->mem[addr+3]=(uint8_t)(val>>24); break; }
        case PEEK: pop(v,&da); { int a2=(int)da; int32_t val=v->mem[a2&0xFFFF]|(v->mem[(a2+1)&0xFFFF]<<8)|(v->mem[(a2+2)&0xFFFF]<<16)|(v->mem[(a2+3)&0xFFFF]<<24); push(v,(double)val); break; }
        case POKE: { pop(v,&db); pop(v,&da); int a2=(int)da; int32_t val=(int32_t)db; v->mem[a2&0xFFFF]=(uint8_t)val; v->mem[(a2+1)&0xFFFF]=(uint8_t)(val>>8); v->mem[(a2+2)&0xFFFF]=(uint8_t)(val>>16); v->mem[(a2+3)&0xFFFF]=(uint8_t)(val>>24); break; }

        /* Control flow — JZ/JNZ check FLAGS.Z, not stack */
        case JMP: addr=ru16(v); v->pc=addr; break;
        case JZ:  addr=ru16(v); if(v->flags&F_Z) v->pc=addr; break;
        case JNZ: addr=ru16(v); if(!(v->flags&F_Z)) v->pc=addr; break;
        case CALL: addr=ru16(v); if(v->csp>=CALL_MAX)return-1; v->cstk[v->csp++]=v->pc; v->pc=addr; break;
        case RET: if(v->csp<=0)return-1; v->pc=v->cstk[--v->csp]; break;
        case PUSH: push(v,(double)ri32(v)); break;
        case POP: pop(v,&da); break;

        /* Stack manipulation */
        case DUP: { double p; if(peek(v,0,&p))return-1; push(v,p); break; }
        case SWAP: { double a,b; if(pop(v,&b)||pop(v,&a))return-1; push(v,b); push(v,a); break; }
        case OVER: { double p; if(peek(v,1,&p))return-1; push(v,p); break; }
        case ROT: { double a,b,c; if(pop(v,&c)||pop(v,&b)||pop(v,&a))return-1; push(v,b); push(v,c); push(v,a); break; }

        /* Float */
        case FADD: pop(v,&db); pop(v,&da); push(v,da+db); break;
        case FSUB: pop(v,&db); pop(v,&da); push(v,da-db); break;
        case FMUL: pop(v,&db); pop(v,&da); push(v,da*db); break;
        case FDIV: pop(v,&db); pop(v,&da); push(v,da/db); break;

        /* Confidence */
        case CONF_G: push(v,(double)v->conf); break;
        case CONF_S: pop(v,&da); v->conf=(float)da; if(v->conf<0)v->conf=0; if(v->conf>1)v->conf=1; break;
        case CONF_M: pop(v,&da); v->conf*=(float)da; if(v->conf<0)v->conf=0; if(v->conf>1)v->conf=1; break;

        /* A2A — SIGNAL/BCAST pop value and queue; LISTEN dequeues or pushes 0 */
        case SIGNAL: case BCAST: { uint8_t ch=ru8(v); pop(v,&da); int ci=ch%SIG_CH; if(v->sig_t[ci]-v->sig_h[ci]<SIG_Q) v->sig_q[ci][v->sig_t[ci]%SIG_Q]=da; v->sig_t[ci]++; break; }
        case LISTEN: { uint8_t ch=ru8(v); int ci=ch%SIG_CH; if(v->sig_h[ci]<v->sig_t[ci]){push(v,v->sig_q[ci][v->sig_h[ci]%SIG_Q]);v->sig_h[ci]++;}else push(v,0.0); break; }

        default: return -1;
        }
    }
    return 0;
}

/* ─── Hex decode ─────────────────────────────────────────────────────────── */
static int hex_dec(const char *h, uint8_t *out, int max) {
    int n=0, len=strlen(h);
    for (int i=0; i<len && n<max; ) {
        if (h[i]==' '||h[i]=='\n') { i++; continue; }
        if (!isxdigit((unsigned char)h[i])||!isxdigit((unsigned char)h[i+1])) return -1;
        char buf[3]={h[i],h[i+1],0};
        out[n++]=(uint8_t)strtol(buf,NULL,16);
        i+=2;
    }
    return n;
}

/* ─── Minimal JSON parser ───────────────────────────────────────────────── */
static char *J; static int Jp;
static void jskip(void) { while(J[Jp]&&strchr(" \t\n\r",J[Jp]))Jp++; }
static char *jstr(void) { jskip(); if(J[Jp]!='"')return NULL; Jp++; int s=Jp; while(J[Jp]&&J[Jp]!='"'){if(J[Jp]=='\\')Jp++;Jp++;} J[Jp]=0; char*r=&J[s]; Jp++; return r; }
static double jnum(void) { jskip(); char buf[64]; int i=0; if(J[Jp]=='-')buf[i++]=J[Jp++]; while(J[Jp]>='0'&&J[Jp]<='9'||J[Jp]=='.'||J[Jp]=='e'||J[Jp]=='E'||J[Jp]=='+')buf[i++]=J[Jp++]; buf[i]=0; return atof(buf); }
static int jbool(void) { jskip(); if(J[Jp]=='t'){Jp+=4;return 1;} if(J[Jp]=='f'){Jp+=5;return 0;} return 0; }
static void jexpect(char c) { jskip(); if(J[Jp]==c)Jp++; }

/* ─── Main ───────────────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
    if (argc<2) { printf("Usage: %s <vectors.json>\n",argv[0]); return 1; }
    FILE *f=fopen(argv[1],"r"); if(!f){fprintf(stderr,"Cannot open %s\n",argv[1]);return 1;}
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    J=malloc(sz+1); fread(J,1,sz,f); J[sz]=0; fclose(f);

    Jp=0; jexpect('[');
    int pass=0,fail=0,total=0;
    char fails[120][256]; int fi=0;

    while(1) {
        jskip(); if(J[Jp]==']')break;
        if(J[Jp]==','){Jp++;continue;}
        jexpect('{');

        char name[256]="", hex[4096]="";
        double istk[STACK_MAX], estk[STACK_MAX];
        int ic=0, ec=0, eflags=-1, eps=0;

        while(1) {
            jskip(); if(J[Jp]=='}'){Jp++;break;}
            if(J[Jp]==','){Jp++;continue;}
            char *key=jstr(); jexpect(':');
            if(strcmp(key,"name")==0) strncpy(name,jstr(),255);
            else if(strcmp(key,"bytecode_hex")==0) strncpy(hex,jstr(),4095);
            else if(strcmp(key,"initial_stack")==0) {
                jexpect('['); ic=0;
                while(1){jskip();if(J[Jp]==']'){Jp++;break;}if(J[Jp]==','){Jp++;continue;}istk[ic++]=jnum();}
            } else if(strcmp(key,"expected_stack")==0) {
                jexpect('['); ec=0;
                while(1){jskip();if(J[Jp]==']'){Jp++;break;}if(J[Jp]==','){Jp++;continue;}estk[ec++]=jnum();}
            } else if(strcmp(key,"expected_flags")==0) eflags=(int)jnum();
            else if(strcmp(key,"allow_float_epsilon")==0) eps=jbool();
        }

        total++; VM vm; vm_init(&vm);
        vm.clen = hex_dec(hex, vm.code, CODE_MAX);
        for(int i=0;i<ic;i++) push(&vm,istk[i]);

        int rc = vm_run(&vm);
        int ok = 1; char msg[512]="";

        if(rc) { ok=0; snprintf(msg,512,"VM error (rc=%d)",rc); }

        if(ok && vm.sp!=ec) { ok=0; snprintf(msg,512,"stack len: got %d, expected %d",vm.sp,ec); }

        if(ok) for(int i=0;i<ec;i++) {
            if(eps) { if(fabs(vm.stk[i]-estk[i])>1e-4){ok=0;snprintf(msg,512,"stack[%d]: got %g, expected %g",i,vm.stk[i],estk[i]);break;} }
            else { if(vm.stk[i]!=estk[i]){ok=0;snprintf(msg,512,"stack[%d]: got %g, expected %g",i,vm.stk[i],estk[i]);break;} }
        }

        if(ok && eflags!=-1 && vm.flags!=eflags) { ok=0; snprintf(msg,512,"flags: got 0x%02x, expected 0x%02x",vm.flags,eflags); }

        if(ok) { pass++; printf("  PASS  %s\n",name); }
        else { fail++; printf("  FAIL  %s — %s\n",name,msg); if(fi<120)snprintf(fails[fi++],256,"FAIL %s: %s",name,msg); }
    }

    printf("\n%d/%d passed (%d failed)\n",pass,total,fail);
    if(fail) { printf("\n--- Failures ---\n"); for(int i=0;i<fi;i++) printf("  %s\n",fails[i]); }
    free(J);
    return fail>0;
}
