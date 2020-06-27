/*
 * boot.h
 * copyright (C) 2020/05/25 dora2ios
 *
 */

#ifndef boot_H
#define boot_H

#include <irecovery/libirecovery.h>
int boot_client(void* buf, size_t sz, int pwn);

#endif
