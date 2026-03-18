// hal/host/sys.h

#ifndef SYS_H_
#define SYS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "heap.h"

void sys_init(void);
int sys_exit_requested(void);
void sys_exit_request(void);

void panic(const char *message);
void panic_hook(void (*handler)(void));

bool file_save(const char *name, const uint8_t *data, size_t size);
size_t file_load(const char *name, uint8_t **data);

#endif