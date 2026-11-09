#pragma once

#define LOCAL_ENTRY 1
#define LOCAL_EXIT 2

/* LOCAL_ENTRY or LOCAL_EXIT for "@entry" or "@exit", otherwise 0. */
unsigned short local_get(const char*);
const char* local_get_name(unsigned short);
