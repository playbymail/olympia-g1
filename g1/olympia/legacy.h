// Copyright (c) 2026 Michael D Henderson. All rights reserved.

//
// Created by Michael Henderson on 1/25/26.
//

#ifndef OLYMPIA_LEGACY_H
#define OLYMPIA_LEGACY_H

#include <stdarg.h>

// BUGFIX (modernization): use varargs */
void log(int k, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
void out(int who, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
void wiout(int who, int ind, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
void wout(int who, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif //OLYMPIA_LEGACY_H
