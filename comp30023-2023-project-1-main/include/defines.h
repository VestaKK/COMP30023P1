#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <byteswap.h>
#include <err.h>

#define MAX_NAME_LEN 8
#define TRUE 1
#define FALSE 0
#define FREE(x) { free(x); x=NULL; }
#define LINE "------------------------\n"
#define BUFFER_SIZE 2048
#define PIPE_READ 0
#define PIPE_WRITE 1
#define SHA_HASH_SIZE 64
#define PID_NULL_HANDLE -2

#ifdef DEBUG
#define debug_log(string,...) printf(string, ##__VA_ARGS__);
#else
#define debug_log(string,...)
#endif


typedef unsigned char bool;

#endif