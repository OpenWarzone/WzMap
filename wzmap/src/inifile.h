#pragma once

#define MAX_LINE_LENGTH    8192

size_t get_private_profile_string(char *, char *, char *, char *, int, char *);
int write_private_profile_string(char *, char *, char *, char *);

const char *IniRead(char *aFilespec, char *aSection, char *aKey, char *aDefault);
void IniWrite(char *aFilespec, char *aSection, char *aKey, char *aValue);
