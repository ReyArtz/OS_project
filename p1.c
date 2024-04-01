#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>

#define PATH_MAX 4096

void parseDirectory(const char* dirp){
    
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
        snprintf(fullPath,sizeof(fullPath),"%s%s",dirp,entry->d_name);

        if(lstat(fullPath, &info)==-1){
            perror("error get file info");
            continue;
        }

        if(strcmp(entry->d_name,".")==0||(strcmp(entry->d_name,"..")==0)){
            continue;
        }

        printf("Name: %s",entry->d_name);
        printf("Size %ld",info.st_size);
        printf("Permissions: %o",info.st_mode &(S_IRWXU | S_IRWXG | S_IRWXO));


        

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
