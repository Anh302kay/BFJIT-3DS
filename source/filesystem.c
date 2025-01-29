#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include <filesystem.h>

Files* createEntry(const char* path, const char* name, size_t size, bool isDirectory)
{
    if(size > MAXFILELENGTH)
        return NULL;
    Files* entry = malloc(sizeof(Files));
    strncpy(entry->path, path, size);
    strncpy(entry->name, name, size);
    entry->isDirectory = isDirectory;
    entry->nextEnt = NULL;
    entry->lastEnt = NULL;
    return entry;
    // return NULL;
}

void freeEntry(Files* entry)
{
    free(entry);
}

void freeDirectory(Files* head) 
{
    if(head->lastEnt != NULL) // incase clearing part of list
        head->lastEnt->nextEnt = NULL;

    Files* temp;
    while(head->nextEnt != NULL) {
        temp = head;
        head = head->nextEnt;
        free(temp);
    }
    head = NULL;
}

Files* openDirectory(const char* path)
{
    FS_Archive sdmcArchive;
    FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

    Handle dirHandle;
    FS_Archive archive;
    FSUSER_OpenDirectory(&dirHandle, sdmcArchive, fsMakePath(PATH_ASCII, path));
    u32 entriesRead = 1;
    FS_DirectoryEntry entry;

    Files* head = NULL;
    Files* current = NULL;

    while(entriesRead) {
        entriesRead = 0;
        FSDIR_Read(dirHandle, &entriesRead, 1, &entry);

        if(!entriesRead)
            break;
            
        if(entry.attributes & FS_ATTRIBUTE_HIDDEN)
            continue;
        
        char name[262] = {0};

        for(int i = 0; i < 262; i++) {
            if(entry.name[i] > 255)
                name[i] = 2;
            name[i] = entry.name[i];
            if(entry.name[i] == 0)
                break;
        }

        Files* file = createEntry(path, name, 256, entry.attributes & FS_ATTRIBUTE_DIRECTORY);

        // printf("%s/%s\n", path, name);

        if(head == NULL)
            head = file;

        if(current == NULL)
            current = file;
        else {
            current->nextEnt = file;
            file->lastEnt = current;
            current = file;
        }

    }

    FSDIR_Close(dirHandle);
    svcCloseHandle(dirHandle);
    FSUSER_CloseArchive(sdmcArchive);
    return head;
}

static void loadDirectory(Files* head, const char* path)
{
    FS_Archive sdmcArchive;
    FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

    Handle dirHandle;
    FS_Archive archive;
    FSUSER_OpenDirectory(&dirHandle, sdmcArchive, fsMakePath(PATH_ASCII, path));
    u32 entriesRead = 1;
    FS_DirectoryEntry entry;
    Files* current = head;

    bool notEnough = false;

    while(entriesRead) {
        entriesRead = 0;
        FSDIR_Read(dirHandle, &entriesRead, 1, &entry);

        if(!entriesRead)
            break;

        if(entry.attributes & FS_ATTRIBUTE_HIDDEN)
            continue;

        char name[262] = {0};

        for(int i = 0; i < 262; i++) {
            if(entry.name[i] > 255)
                name[i] = 2;
            name[i] = entry.name[i];
            if(entry.name[i] == 0)
                break;
        }

        // printf("%s/%s\n", path, name);

        if(current->nextEnt == NULL) {
            if(!notEnough) {
                strncpy(current->path, path, MAXFILELENGTH);
                strncpy(current->name, name, 256);
                current->isDirectory = entry.attributes & FS_ATTRIBUTE_DIRECTORY;
                notEnough = true;
            }
            Files* file = createEntry(path, name, MAXFILELENGTH, entry.attributes & FS_ATTRIBUTE_DIRECTORY);
            current->nextEnt = file;
            file->lastEnt = current;
            current = file;
        } else {
            strncpy(current->path, path, MAXFILELENGTH);
            strncpy(current->name, name, MAXFILELENGTH);
            current->isDirectory = entry.attributes & FS_ATTRIBUTE_DIRECTORY;
            current = current->nextEnt;
        }


    }
    if(!notEnough)
        freeDirectory(current);

    FSDIR_Close(dirHandle);
    svcCloseHandle(dirHandle);
    FSUSER_CloseArchive(sdmcArchive);
}

void openNewDirectory(Files* head, Files* newDir)
{
    char path[512];
    strncpy(path, newDir->path, MAXFILELENGTH);
    if(strnlen(path, MAXFILELENGTH) > 1)
    path[strnlen(path, MAXFILELENGTH)] = '/';
    strncat(path, newDir->name, MAXFILELENGTH-1);
    newDir = head;
    loadDirectory(head, path);
}

void openPreviousDirectory(Files* head, Files* current) 
{
    current = head;
    char* slash = getSlash(head->path);
    slash[0] = '\0';
    if(strnlen(head->path, 5) == 0)
        strncpy(head->path, "/", MAXFILELENGTH);
    loadDirectory(head, head->path);
}