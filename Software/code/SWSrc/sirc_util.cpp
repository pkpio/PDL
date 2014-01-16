// Title: Basic conversion functions
//
// Copyright: Microsoft 2009
//
// Author: Ken Eguro
//
// Created: 10/23/09 
//
// Version: 1.00
// 
// 
// Changelog: 
//
//----------------------------------------------------------------------------

#include "sirc_util.h"

int hexToFpgaId(const char *mac, unsigned char *id, size_t maxBytes)
{
    if ((mac == 0) || (id == 0))
        return -1;

    size_t r = 0;
    unsigned char i = 0;

    for (;;){
        unsigned char c = *mac++;
        if ((c == ':') || (c == 0)) {
            if (++r > maxBytes)
                return -2;
            *id++ = i;
            i = 0;
            if (c == 0)
                break;
            continue;
        }
        if ((c >= '0') && (c <= '9'))
            i = (i << 4) | (c - '0');
        else if ((c >= 'a') && (c <= 'f'))
            i = (i << 4) | (10 + c - 'a');
        else if ((c >= 'A') && (c <= 'F'))
            i = (i << 4) | (10 + c - 'A');
        else
            return -3;
    }
    return (int)r;
}

int hexToFpgaId(const wchar_t *mac, unsigned char *id, size_t maxBytes)
{
    if ((mac == 0) || (id == 0))
        return -1;

    size_t r = 0;
    unsigned char i = 0;

    for (;;){
        wchar_t c = *mac++;
        if ((c == L':') || (c == 0)) {
            if (++r > maxBytes)
                return -2;
            *id++ = i;
            i = 0;
            if (c == 0)
                break;
            continue;
        }
        if ((c >= L'0') && (c <= L'9'))
            i = (i << 4) | (c - L'0');
        else if ((c >= L'a') && (c <= L'f'))
            i = (i << 4) | (10 + c - L'a');
        else if ((c >= L'A') && (c <= L'F'))
            i = (i << 4) | (10 + c - L'A');
        else
            return -3;
    }
    return (int)r;
}

