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
    DIR* dir;
    struct dirent* dirent;
    dir = opendir(path);

    if(dir == NULL)
        return NULL;

    Files* head = NULL;
    Files* current = NULL;

    while(( dirent = readdir(dir) ) != NULL) {
        archive_dir_t* dirSt = (archive_dir_t*)dir->dirData->dirStruct;
        FS_DirectoryEntry* entry = &dirSt->entry_data[dirSt->index];

        if(entry->attributes & FS_ATTRIBUTE_HIDDEN)
            continue;
        
        Files* file = createEntry(path, dirent->d_name, 256, entry->attributes & FS_ATTRIBUTE_DIRECTORY);

        printf("%s/%s\n", path, dirent->d_name);

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

    closedir(dir);
    return head;
}

static void loadDirectory(Files* head, const char* path)
{
    DIR* dir;
    struct dirent* dirent;
    dir = opendir(path);

    // if(dir == NULL)
        // return NULL;

    // newDir = head;
    Files* current = head;

    bool notEnough = false;

    while(( dirent = readdir(dir) ) != NULL) {
        archive_dir_t* dirSt = (archive_dir_t*)dir->dirData->dirStruct;
        FS_DirectoryEntry* entry = &dirSt->entry_data[dirSt->index];

        if(entry->attributes & FS_ATTRIBUTE_HIDDEN)
            continue;

        printf("%s/%s\n", path, dirent->d_name);

        if(current->nextEnt == NULL) {
            if(!notEnough) {
                strncpy(current->path, path, MAXFILELENGTH);
                strncpy(current->name, dirent->d_name, 256);
                current->isDirectory = entry->attributes & FS_ATTRIBUTE_DIRECTORY;
                notEnough = true;
            }
            Files* file = createEntry(path, dirent->d_name, MAXFILELENGTH, entry->attributes & FS_ATTRIBUTE_DIRECTORY);
            current->nextEnt = file;
            file->lastEnt = current;
            current = file;
        } else {
            strncpy(current->path, path, MAXFILELENGTH);
            strncpy(current->name, dirent->d_name, MAXFILELENGTH);
            current->isDirectory = entry->attributes & FS_ATTRIBUTE_DIRECTORY;
            current = current->nextEnt;
        }


    }
    if(!notEnough)
        freeDirectory(current);

    closedir(dir);
}

void openNewDirectory(Files* head, Files* newDir)
{
    char path[512];
    strncpy(path, newDir->path, MAXFILELENGTH);
    // if(path[1] != '\0')
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
    loadDirectory(head, head->path);
}