#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "string.h"

static void print_error_line(){
    uint64 epc = read_csr(mepc);
    addr_line* error_line = current->line;
    code_file* error_file;

    int i;
    for(i = 0,error_line = current->line;i < current->line_ind;i++,error_line++)
        if(error_line->addr == epc)
            break;
    if(i == current->line_ind) panic("Can't find error_line\n");

    error_file = current->file + error_line->file;
    sprint("Runtime error at %s/%s:%d\n",(current->dir)[error_file->dir],error_file->file,error_line->line);

    char file_name[100];
    strcpy(file_name,(current->dir)[error_file->dir]);
    int start = strlen((current->dir)[error_file->dir]);
    file_name[start++] = '/';
    strcpy(file_name + start,error_file->file);
    spike_file_t* file = spike_file_open(file_name, O_RDONLY, 0);
    if(file == NULL) panic("Can't open code file\n");

    char file_code[1000];
    spike_file_pread(file, (void*)file_code,sizeof(file_code),0);
    int it = 0;
    for(int pt = 1;pt < error_line->line;pt++){
        while(file_code[it] != '\n')
            it++;
        it++;
    }
    int st = it,len = 0;
    char error_code[100];
    while(file_code[it] != '\n')
        error_code[len++] = file_code[it++];
    error_code[len] = '\0';
    sprint("%s\n",error_code);
    spike_file_close(file);
}

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() {
    print_error_line();
    panic("Illegal instruction!");
}

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
