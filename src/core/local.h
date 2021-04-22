#pragma once

#define LOCAL_ENTRY 1
#define LOCAL_EXIT 2

unsigned short local_get(char*);
const char* local_get_name(unsigned short);
void local_init();