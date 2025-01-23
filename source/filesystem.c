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
    while(head->nextEnt) {
        temp = head;
        head = head->nextEnt;
        free(temp);
    }
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

void openNewDirectory(Files* head, Files* newDir)
{
    DIR* dir;
    struct dirent* dirent;

    char path[512];
    strncpy(path, newDir->path, MAXFILELENGTH);
    path[strnlen(path, MAXFILELENGTH)] = '/';
    strncat(path, newDir->name, MAXFILELENGTH-2);
    dir = opendir(path);

    // if(dir == NULL)
        // return NULL;

    newDir = head;

    Files* current = head;

    while(( dirent = readdir(dir) ) != NULL) {
        archive_dir_t* dirSt = (archive_dir_t*)dir->dirData->dirStruct;
        FS_DirectoryEntry* entry = &dirSt->entry_data[dirSt->index];

        if(entry->attributes & FS_ATTRIBUTE_HIDDEN)
            continue;

        printf("%s/%s\n", path, dirent->d_name);

        if(current->nextEnt == NULL) {
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
    if(current->nextEnt != NULL)
        freeDirectory(current->nextEnt);

    closedir(dir);
}
