#ifndef INIPARSER_H
#define INIPARSER_H
#include "headers/types.h" //Basic types!

/***************************************************************************
 PORTABLE ROUTINES FOR WRITING PRIVATE PROFILE STRINGS --  by Joseph J. Graf
 Header file containing prototypes and compile-time configuration.
***************************************************************************/

#define MAX_LINE_LENGTH    256

int_64 get_private_profile_int64(char *section,
    char *entry, int_64 def, char *file_name);
uint_64 get_private_profile_uint64(char *section,
    char *entry, uint_64 def, char *file_name);
int get_private_profile_string(char *section, char *entry, char *def,
    char *buffer, int buffer_len, char *file_name);
int write_private_profile_string(char *section,
	char *entry, char *buffer, char *file_name);
int write_private_profile_int64(char *section,
    char *entry, int_64 value, char *file_name);
int write_private_profile_uint64(char *section,
    char *entry, uint_64 value, char *file_name);

#endif
