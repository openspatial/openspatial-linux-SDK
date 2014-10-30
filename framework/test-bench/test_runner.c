/*
 * Copyright 2014, Nod Labs
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_opcodes.h"

#define LOGFILE     "test.log"
#define LINE_BUFFER_MAX   256
#define FILE_NAME_MAX      64
#define CMD_MAX            32

typedef struct test_cmd {
    unsigned short opcode;
    union {
      unsigned short intval;
      unsigned char  strval[32];
    } value;
    struct test_cmd *next;
}TESTCMD;

struct str_to_cmds {
    char str[CMD_MAX];
    unsigned short op;
} optable[] = {
    /* keep this table in lines with opcodes in test_opcodes.h */
  {"pair",            PAIR},
  {"connect",         CONNECT},
  {"read",            READ},
  {"write",           WRITE},
  {"enable",          ENABLE},
  {"disable",         DISABLE},
  {"disconnect",      DISCONNECT},
  {"services",        SERVICES},
  {"characteristics", CHARACTERISTICS},
  {"notification",    NOTIFICATION},
  {"indication",      INDICATION},
  {"value",           VALUE},
  {"report",          REPORT},
  /* nothing below this line pls */
  {"nomore",          INVALID},
};

static void token_to_opcode(const char *token, unsigned short *opcode)
{
  int i = 0;
  do {
    if (strcmp(token, optable[i].str) == 0) {
      *opcode = optable[i].op;
      return;
    }
    i++;
  } while (optable[i].op != INVALID);
  *opcode = INVALID;
  return;
}

static TESTCMD *parse_line(const char *line)
{
    TESTCMD *head, *curr, *tmp;
    char *token;

    head = (TESTCMD *)malloc(sizeof(TESTCMD));
    if (head) {
      head->opcode = INVALID;
      head->next = NULL;
    } else {
      printf("Failed to allocate memory...bailing out\n");
      return NULL;
    }
    curr = head;

    token = strtok((char *)line, " ");
    token_to_opcode(token, &curr->opcode);
    switch (curr->opcode) {
      case CONNECT:
      case DISCONNECT:
      case PAIR:
      case REMOVE:
          /* save the provided bluetooth address */
          token = strtok(NULL, "\n");
          strcpy(curr->value.strval, token);
          break;
      case READ:
      case WRITE:
      case ENABLE:
      case DISABLE:
          /* next is a sub-command */
          tmp = (TESTCMD *)malloc(sizeof(TESTCMD));
          if (tmp) {
            tmp->opcode = INVALID;
            tmp->next = NULL;
          } else {
            printf("Failed to allocate memory...bailing out\n");
            return NULL;
          }
          curr->next = tmp;
          curr = tmp;

          token = strtok(NULL, " ");
          token_to_opcode(token, &curr->opcode);
          switch (curr->opcode) {
            case SERVICES:
                memset(curr->value.strval, 0, 32);
                break;
            case REPORT:
            case CHARACTERISTICS:
            case NOTIFICATION:
            case INDICATION:
                token = strtok(NULL, "\n");
                strcpy(curr->value.strval, token);
                break;
          }
      default:
          curr->opcode = INVALID;
          curr->value.intval = 0xFFFF;
    } /* switch */

    /* TODO: need to take care of releasing this memory somewhere */
    return head;
}

static void dump_cmd(TESTCMD *head)
{
  TESTCMD *tmp = head;
  while(head->next != NULL) {

  }
}
static int execute_commands(TESTCMD* head)
{
  TESTCMD *curr = head;
  while (curr->next != NULL) {
    /* pair and connect commands */
    switch(curr->opcode) {
      case PAIR:
      case CONNECT:
        printf("Opcode: 0x%x Value: %s\n", curr->opcode, curr->value.strval);
        break;
      case REMOVE:
      case DISCONNECT:
        printf("Opcode: 0x%x Value: %s\n", curr->opcode, curr->value.strval);
        break;
      case READ:
        switch(curr->next->opcode) {
          case SERVICES:
          case CHARACTERISTICS:
            /* read services OR read characteristics */
            printf("Curr opcode: 0x%x Next opcode: 0x%x Value: %s\n",
              curr->opcode, curr->next->opcode, curr->next->value.strval ? curr->next->value.strval : NULL);
            break;
          case REPORT:
            /* read services OR read characteristics OR read report */
            printf("Curr opcode: 0x%x Next opcode: 0x%x Value: %d\n",
              curr->opcode, curr->next->opcode, curr->next->value.intval);
            break;
        }
        break;
      case WRITE:
        /* write value (to) "logfile" */
        printf("Curr opcode: 0x%x Next opcode: 0x%x Value: %s\n",
              curr->opcode, curr->next->opcode, curr->next->value.strval ? curr->next->value.strval : NULL);
        break;
      case ENABLE:
      case DISABLE:
        /* enable/disable notification OR enable/disable indication */
        printf("Curr opcode: 0x%x Next opcode: 0x%x Value: %d\n",
            curr->opcode, curr->next->opcode, curr->next->value.intval);
        break;
    } /* switch */
  } /* while */
}

static struct test_cmd cmd_array[5];
int test_execute(FILE *fptr)
{
  char line_buffer[LINE_BUFFER_MAX];
  struct test_cmd *cmdptr;
  while(fptr && !feof(fptr)) {

    if (fgets(line_buffer, LINE_BUFFER_MAX, fptr))
      cmdptr = parse_line(line_buffer);

    dump_command(cmdptr);

    if (execute_commands(cmdptr)) {
      printf("Failed to execute the commands\n");
      return -1;
    }
  }
  return 0;
}

int main()
{
  FILE *fptr;
  char filename[FILE_NAME_MAX];
  int ret = 0;
  printf("########################################\n");
  printf("########## Nod Labs Inc ################\n");
  printf("########################################\n");
  printf("Enter the test case name: ");
  scanf("%s", filename);
  printf("\n");
  fptr = fopen(filename, "r");
  if (!fptr) {
    printf("Failed to open %s\n", filename);
    return -1;
  }

  ret = test_execute(fptr);
  if (ret) {
    printf("Test case %s failed\n", filename);
    printf("Results written into %s\n", LOGFILE);
    return -2;
  }
  printf("Test case %s executed successfully\n", filename);
  return 0;
}
