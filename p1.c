#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>

#define PATH_MAX 4096

struct Snapshot{
    char name[PATH_MAX];
    mode_t permission;
    off_t size;
    char location[PATH_MAX];
    ino_t inode;
}


void parseDirectory(const char* dirp, Snapshot *snapshot,int *count){
    
    struct dirent *entry;
    struct stat info;

    DIR *dir= opendir(dirp);

    if(dir==NULL){
        perror("error opening");
        return;
    }
    printf("snapshot :%s\n",dirp);

    while((entry=readdir(dir))!=NULL){
        char fullPath[PATH_MAX];
        snprintf(fullPath,sizeof(fullPath),"%s/%s",dirp,entry->d_name);

        if(lstat(fullPath, &info)==-1){
            perror("error get file info");
            continue;
        }

        if(strcmp(entry->d_name,".")==0||(strcmp(entry->d_name,"..")==0)){
            continue;
        }

        printf("Name: %s\n",entry->d_name);
        printf("Size %ld\n",info.st_size);
        printf("Permissions: %o\n",info.st_mode &(S_IRWXU | S_IRWXG | S_IRWXO));
        printf("----------------------\n");
        

        strcpy((*snapshot)[*count].location,fullPath);
        strcpy((*snapshot)[*count].name,entry->d_name);
        (*snapshot)[*count].size=info.st_size;
        (*snapshot)[*count].permission=info.st_mode &(S_IRWXU | S_IRWXG | S_IRWXO);
        (*snapshot)[*count].inode=info.st_ino;
        (*count);

    }
    closedir(dir);

}


int main(int argc, char* argv[]){

    if(argc!=2){
        fprintf(stderr,"%s",argv[0]);
        return EXIT_FAILURE;
    }

    parseDirectory(argv[1]);
    return EXIT_SUCCESS;

    
}
