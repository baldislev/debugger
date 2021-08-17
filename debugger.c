/*
 * TODO: - UPDATE HANDLING LOOPS ACCORDING TO PIAZZA ANSWERS - IF NEEDED
 *       - export debugging code from main to a separate function - OPTIONAL
 *       - if forbidden to use qsort then implement own sort
 *       - check argument passing
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <string.h>
#include <stdbool.h>

#define END_OF_INPUT ("run profile")
#define MAX_LEN (261) ///len(var) + len(" ") + len(reg) + len(\n) = 256 + 1 + 3 + 1 = 261
#define NUM_OF_REGS (24) ///rax,rbx,rcx,rdx,rsi this is also the number of vars.
#define QUAD_MASK (0xFFFFFFFFFFFFFFFF)
#define LONG_MASK (0x00000000FFFFFFFF)
#define WORD_MASK (0x000000000000FFFF)
#define H_BYTE_MASK (0x000000000000FF00)
#define L_BYTE_MASK (0x00000000000000FF)

int run_target(char* programName, char* args[]) {
    pid_t pid;
    pid = fork();
    if(pid > 0){
        return pid;
    } else if(pid == 0){
        if(ptrace(PTRACE_TRACEME,0,NULL,NULL) < 0){
            exit(1);
        }
        execv(programName,args);
    } else {
        exit(1);
    }
    //if we got here then execl failed.
    exit(1);
}

int read_input(char* inputLines[NUM_OF_REGS]){
    char buffer[MAX_LEN];
    int index = 0;
    while(index <= NUM_OF_REGS ){
        fgets(buffer, MAX_LEN, stdin);
        if(buffer[strlen(buffer)-1] == '\n') buffer[strlen(buffer)-1] = '\0';
        if(strcmp(buffer, END_OF_INPUT) == 0){
            break;
        }
        inputLines[index] = malloc(sizeof(char)*strlen(buffer)+1);
        strcpy(inputLines[index], buffer);
        index++;
    }
    return index;
}

void initiate_lines(char* inputLines[]){
    for(int i = 0; i < NUM_OF_REGS; i++){
        inputLines[i] = NULL;
    }
}

void free_memory(char* inputLines[]){
    int i=0;
    while(inputLines[i]){
        free(inputLines[i]);
        i++;
    }
}

static int str_compare(const void* a, const void* b)
{
    return strcmp(*(const char**)a, *(const char**)b);
}

void set_breakpoint(int pid, unsigned long long int address){
    long long int instruction = ptrace(PTRACE_PEEKTEXT, pid, (void*)address, NULL);
    unsigned long long int breakpoint = (instruction & 0xFFFFFFFFFFFFFF00) | 0xCC;
    if(ptrace(PTRACE_POKETEXT, pid, (void*)address, (void*)breakpoint) < 0) {
        exit(1);
    }
}


unsigned long long int slice_value(char* regName, unsigned long long int value){
    unsigned long long int mask = 0;
    int len = (int) strlen(regName);
    if(len == 3){
        if(regName[0] == 'r'){
            mask = QUAD_MASK;
        }
        if(regName[0] == 'e'){
            mask = LONG_MASK;
        }
        if(regName[0] == 's'){ ///regName = %sil
            mask = L_BYTE_MASK;
        }
    }
    if(len == 2){
        if(regName[1] == 'x' || regName[1] == 'i'){ ///second cond is for regName = %si
            mask = WORD_MASK;
        }
        if(regName[1] == 'h'){
            mask = H_BYTE_MASK;
        }
        if(regName[1] == 'l'){
            mask = L_BYTE_MASK;
        }
    }
    value = value & mask;
    if(regName[1] == 'h') value = value>>8;
    return value;
}

unsigned long long int getRegValue(char* regName, struct user_regs_struct* regs){
    unsigned long long int value = 0;
    if(strcmp(regName,"rax") == 0 || strcmp(regName,"eax") == 0 || strcmp(regName,"ax") == 0 || \
       strcmp(regName,"ah") == 0  || strcmp(regName,"al") == 0) {
        value = regs->rax;
    }
    if(strcmp(regName,"rcx") == 0 || strcmp(regName,"ecx") == 0 || strcmp(regName,"cx") == 0 || \
       strcmp(regName,"ch") == 0  || strcmp(regName,"cl") == 0) {
        value = regs->rcx;
    }
    if(strcmp(regName,"rdx") == 0 || strcmp(regName,"edx") == 0 || strcmp(regName,"dx") == 0 || \
       strcmp(regName,"dh") == 0  || strcmp(regName,"dl") == 0) {
        value = regs->rdx;
    }
    if(strcmp(regName,"rbx") == 0 || strcmp(regName,"ebx") == 0 || strcmp(regName,"bx") == 0 || \
       strcmp(regName,"bh") == 0  || strcmp(regName,"bl") == 0) {
        value = regs->rbx;
    }
    if(strcmp(regName,"rsi") == 0 || strcmp(regName,"esi") == 0 || strcmp(regName,"si") == 0 || \
       strcmp(regName,"sil") == 0) {
        value = regs->rsi;
    }

    value = slice_value(regName,value);
    return value;
}

void print_results(char* inputLines[NUM_OF_REGS],
                   struct user_regs_struct* oldRegs,
                   struct user_regs_struct* newRegs){
    char* varName;
    char* regName;
    unsigned long long int oldRegValue;
    unsigned long long int newRegValue;
    for(int i = 0; i < NUM_OF_REGS; i++){
        if(!inputLines[i]){
            break;
        }
        varName = strtok(inputLines[i]," "); /// inserts '\0' at the place of ' '
        regName = varName + strlen(varName) + 1;
        oldRegValue = getRegValue(regName, oldRegs);
        newRegValue = getRegValue(regName, newRegs);

        if(oldRegValue != newRegValue){
            printf("PRF:: %s: %lld->%lld\n", varName, (signed long long int)oldRegValue, (signed long long int)newRegValue);
        }
        ///This will bring back the inputLine to its initial state:
        varName[strlen(varName)] = ' ';
    }
}

int main(int argc, char* argv[]) {

    char* inputLines[NUM_OF_REGS];
    initiate_lines(inputLines);
    int numOfVars = read_input(inputLines);
    qsort(inputLines, numOfVars, sizeof(const char*), str_compare);

    unsigned long long int startAddress = strtoull(argv[1],NULL,16);
    unsigned long long int finishAddress = strtoull(argv[2],NULL,16);

    char* programName = argv[3];
    int numOfArgs = argc - 3; /// programName is the first argument
    char* args[numOfArgs];
    args[numOfArgs] = NULL; /// array should be NULL terminated before passing to execv
    for(int i = 0; i < numOfArgs; i++){
        args[i] = argv[i+3];
    }

    pid_t childPid;
    childPid = run_target(programName,args);

///////////////////////////////////////DEBUGGER:
    int waitStatus;
    struct user_regs_struct regs;
    struct user_regs_struct startRegs;
    struct user_regs_struct finishRegs;

    //wait to stop on first instruction of target
    wait(&waitStatus);
    //Storing old instructions and setting breakpoints:
    unsigned long long int startInstruction = ptrace(PTRACE_PEEKTEXT, childPid, (void*)startAddress, NULL);
    unsigned long long int finishInstruction = ptrace(PTRACE_PEEKTEXT, childPid, (void*)finishAddress, NULL);
    set_breakpoint(childPid, startAddress);
    set_breakpoint(childPid, finishAddress);
    bool toReadStart = true;
    if(ptrace(PTRACE_CONT, childPid, NULL, NULL) < 0){
        exit(1);
    }
    //wait to stop on its first breakpoint:
    wait(&waitStatus);
    while(WIFSTOPPED(waitStatus)){

        if (ptrace(PTRACE_GETREGS, childPid, NULL, &regs) < 0) {
            exit(1);
        }

        regs.rip = regs.rip - 1;
        if(regs.rip == startAddress || regs.rip == finishAddress) {
            if (ptrace(PTRACE_SETREGS, childPid, NULL, &regs) < 0) {
                exit(1);
            }
            if (regs.rip == startAddress) {
                if(toReadStart) {
                    if (ptrace(PTRACE_GETREGS, childPid, NULL, &startRegs) < 0) {
                        exit(1);
                    }
                    toReadStart = false;
                }
                if(ptrace(PTRACE_POKETEXT, childPid, (void*)startAddress, (void*)startInstruction) < 0) {
                    exit(1);
                }
                if(ptrace(PTRACE_SINGLESTEP, childPid, NULL, NULL) < 0){
                    exit(1);
                }
                wait(&waitStatus);
                //setting both because first breakpoint can disable the second one if there is overlapping:
                set_breakpoint(childPid,startAddress);
                set_breakpoint(childPid,finishAddress);
                //Special case:
                if(startAddress == finishAddress){
                    if (ptrace(PTRACE_GETREGS, childPid, NULL, &finishRegs) < 0) {
                        exit(1);
                    }
                    toReadStart = true;
                    print_results(inputLines, &startRegs, &finishRegs);
                }
            }
            if (regs.rip == finishAddress){//finishAdress:
                if(ptrace(PTRACE_POKETEXT, childPid, (void*)finishAddress, (void*)finishInstruction) < 0) {
                    exit(1);
                }
                if(ptrace(PTRACE_SINGLESTEP, childPid, NULL, NULL) < 0){
                    exit(1);
                }
                wait(&waitStatus);
                if(!toReadStart) {
                    if (ptrace(PTRACE_GETREGS, childPid, NULL, &finishRegs) < 0) {
                        exit(1);
                    }
                    toReadStart = true;
                    print_results(inputLines, &startRegs, &finishRegs);
                }
                set_breakpoint(childPid,finishAddress);
            }
        }

        if(ptrace(PTRACE_CONT, childPid, NULL, NULL) < 0){
            exit(1);
        }
        wait(&waitStatus);
    }

    free_memory(inputLines);
    return 0;
}
