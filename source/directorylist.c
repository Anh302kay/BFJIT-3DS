#include <stdio.h>
#include <3ds.h>
#include <malloc.h>
#include <string.h>

#include "directorylist.h"

#define posToBufferB(x, y) ((int)(y) * 40 + (int)(x))

void scrollBufferUp(DirectoryList* dList, char* bottomBuffer, int scrollAmount) {
    if(dList->viewTop->lastEnt == NULL)
        return;
    
    memmove(&bottomBuffer[posToBufferB(0,scrollAmount+1)], &bottomBuffer[posToBufferB(0, 1)], 40*(29-scrollAmount));
    memset(&bottomBuffer[posToBufferB(0,scrollAmount)], ' ', scrollAmount * 40);

    for(int i = 0; i < scrollAmount; i++) {
        if(dList->viewTop->lastEnt == NULL)
            break;

        dList->viewBottom = dList->viewBottom->lastEnt;
        dList->viewTop = dList->viewTop->lastEnt;
        strcpy(&bottomBuffer[posToBufferB(0,scrollAmount - i)], dList->viewTop->name);
        if(dList->viewTop->isDirectory)
            bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = '/';
        else
            bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
    }
}

void scrollBufferDown(DirectoryList* dList, char* bottomBuffer, int scrollAmount) {
    if(dList->viewBottom->nextEnt == NULL)
        return;
        
    memcpy(&bottomBuffer[posToBufferB(0,1)], &bottomBuffer[posToBufferB(0, scrollAmount+1)], 40*(29-scrollAmount));
    memset(&bottomBuffer[posToBufferB(0,30-scrollAmount)], ' ', scrollAmount * 40);
    for(int i = 0; i < scrollAmount; i++) {
        if(dList->viewBottom->nextEnt == NULL)
            break;

        dList->viewBottom = dList->viewBottom->nextEnt;
        dList->viewTop = dList->viewTop->nextEnt;
        strcpy(&bottomBuffer[posToBufferB(0,30-scrollAmount)], dList->viewBottom->name);
        if(dList->viewBottom->isDirectory)
            bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = '/';
        else
            bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
    }
}


void displayList(DirectoryList* dList, char* bottomBuffer)
{
    dList->currentFile = dList->fileList;
    strcpy(bottomBuffer, "Current Dir: ");
    strcpy(&bottomBuffer[13], dList->currentFile->path);
    bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
    int bufY = 1;
    dList->currentY = 2;
    dList->viewY = 2;
    if(strlen(dList->currentFile->path) > 27)
        bufY++; 
    for(Files* entry = dList->fileList; entry != NULL; entry = entry->nextEnt) {
        if(entry->isDirectory) {
            strcpy(&bottomBuffer[posToBufferB(0,bufY)], entry->name);
            bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = '/';
        }
        else {
            strcpy(&bottomBuffer[posToBufferB(0,bufY)], entry->name);
            bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
        }
        bufY++;
        if(bufY > 29) {
            dList->viewBottom = entry;
            break;
        }
    }
    bottomBuffer[40*30] = '\0';
}
void displayPrevList(DirectoryList* dList, char* bottomBuffer, char* oldLocation)
{
    memset(bottomBuffer, ' ', 40);
    strcpy(bottomBuffer, "Current Dir: ");
    strcpy(&bottomBuffer[13], dList->currentFile->path);
    bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
    int bufY = 1;
    bool first = true;
    u8 scrollStatus = 0; // 0 = not set, 1 = screen starts not scrolled,2 = screen starts scrolled
    if(strlen(dList->currentFile->path) > 27)
        bufY++;
    
    for(Files* file = dList->fileList; file != NULL; file = file->nextEnt) {
        if(strcmp(oldLocation, file->name) == 0) {
            dList->currentFile = file;
            dList->currentY = bufY +1;
            dList->viewY = dList->currentY;
            if(bufY > 29) {
                dList->viewY = 2;
                dList->viewTop = file;
                scrollStatus = 2;
            }
            else
                scrollStatus = 1;
        }
        if(first) {
            dList->viewTop = file;
            first = false;
        }
        if(bufY < 30) {
            strcpy(&bottomBuffer[posToBufferB(0,bufY)], file->name);
            if(file->isDirectory) 
                bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = '/';
            else 
                bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
            if(bufY == 29 && scrollStatus == 1) {
                dList->viewBottom = file;
                break;
            }
        }
        else if (dList->viewTop != dList->fileList) {
            if(bufY + 1 == dList->currentY)
                memset(&bottomBuffer[posToBufferB(0,1)], ' ', 40*29);

            if(bufY + 1 - dList->currentY > 30) {
                dList->viewBottom = file;
                break;
            }

            strcpy(&bottomBuffer[posToBufferB(0,bufY - dList->currentY+2)], file->name);
            
            if(file->isDirectory) 
                bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = '/';
            else 
                bottomBuffer[strnlen(bottomBuffer, 40*30+1)] = ' ';
        }

        if(file->nextEnt == NULL)
            dList->viewBottom = file;

        bufY++;
    }
    bottomBuffer[40*30] = '\0';
}