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

#ifndef DEFINEUTILH
#define DEFINEUTILH 1

extern int hexToFpgaId(const char *mac, unsigned char *id, size_t maxBytes);
extern int hexToFpgaId(const wchar_t *mac, unsigned char *id, size_t maxBytes);

#endif //DEFINEUTILH
