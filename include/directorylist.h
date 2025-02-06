#pragma once
#include <stdio.h>
#include <3ds.h>
#include "filesystem.h"

typedef struct DirectoryList {
    Files* fileList;
    Files* currentFile;
    Files* viewTop;
    Files* viewBottom;

    u16 currentY;
    u16 viewY;
} DirectoryList;

void scrollBufferUp(DirectoryList* dList, char* bottomBuffer, int scrollAmount);
void scrollBufferDown(DirectoryList* dList, char* bottomBuffer, int scrollAmount);

void displayList(DirectoryList* dList, char* bottomBuffer);
void displayPrevList(DirectoryList* dList, char* bottomBuffer, char* oldLocation);