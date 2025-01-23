#pragma once
#include <stdio.h>
#include <dirent.h>

#define MAXFILELENGTH 512

typedef struct Files Files;

typedef struct Files 
{
    Files* lastEnt, *nextEnt;
    bool isDirectory;
    char name[MAXFILELENGTH];
    char path[MAXFILELENGTH];
} Files;

Files* createEntry(const char* path, const char* name, size_t size, bool isDirectory);
void freeEntry(Files* entry);
void freeDirectory(Files* head);

Files* openDirectory(const char* path);
void openNewDirectory(Files* head, Files* newDir);