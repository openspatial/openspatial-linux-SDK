/*
 * Copyright 2014, Nod Labs
 */
#ifndef __NOD_TEST_OPCODE_H__
#define __NOD_TEST_OPCODE_H__

/*
 * Structure of the commands:
 *  - A command that has MSB set will have a sub-command embedded
 *  - The same is applicable for sub-command if there is a following
 *    sub-sub-command for it, and so on.
 *  - If the MSB is not set, then the value can be present or not
 * When new commands are added, please follow this norm for proper
 * parsing of the test case
 */

/* main opcodes */
#define PAIR            0x0001
#define CONNECT         0x0002
#define READ            0x8003
#define WRITE           0x8004
#define ENABLE          0x8005
#define DISABLE         0x8006
#define DISCONNECT      0x0007
#define REMOVE          0x0008

/* sub opcodes */
#define SERVICES        0x0009
#define CHARACTERISTICS 0x000a
#define NOTIFICATION    0x000b
#define INDICATION      0x000c
#define VALUE           0x000d
#define REPORT          0x000e

#define INVALID         0x4FFF

#endif
