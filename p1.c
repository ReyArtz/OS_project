#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>

#define PATH_MAX 4096

struct Snapshot {
    char name[PATH_MAX];
    mode_t permission;
    off_t size;
    char location[PATH_MAX];
    ino_t inode;
    time_t modif_time;
};

void parseDirectory(const char* dirp, struct  Snapshot *snapshot, int *count) {
    struct dirent *entry;
    struct stat info;

    DIR *dir = opendir(dirp);

    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }
    printf("Snapshot: %s\n", dirp);

    while ((entry = readdir(dir)) != NULL) {
        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirp, entry->d_name);

        if (lstat(fullPath, &info) == -1) {
            perror("Error getting file info");
            continue;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        strcpy(snapshot[*count].location, fullPath);
        strcpy(snapshot[*count].name, entry->d_name);
        snapshot[*count].size = info.st_size;
        snapshot[*count].permission = info.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
        snapshot[*count].inode = info.st_ino;
        snapshot[*count].modif_time = info.st_mtime;
        (*count)++;
    }
    closedir(dir);
}

void savesnapshot(const char* filename , struct Snapshot *snapshot, int count) {
    FILE* file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening the file");
        return;
    }

    for (int i = 0; i < count; ++i) {
        fprintf(file, "%s|%s|%ld|%o|%lu\n", snapshot[i].name, snapshot[i].location, 
                (long)snapshot[i].size, snapshot[i].permission, 
                (unsigned long)snapshot[i].inode);
    }

    fclose(file);
}

void compare(struct Snapshot *snap1, struct Snapshot *snap2, int count1, int count2) {
    int rmv = 0, add = 0, move = 0;

    //for remove
    for (int i = 0; i < count1; i++) {
        int flag = 0;
        for (int j = 0; j < count2; j++) {
            if (snap1[i].inode == snap2[j].inode) {
                flag = 1;
                break;
            }
        }
        if (!flag) {
            printf("Remove: %s\n", snap1[i].location);
            rmv = 1;
        }
    }

    //add
    for (int i = 0; i < count2; i++) {
        int flag = 0;
        for (int j = 0; j < count1; j++) {
            if (snap2[i].inode == snap1[j].inode) {
                flag = 1;
                break;
            }
        }
        if (!flag) {
            printf("Added: %s\n", snap2[i].location);
            add = 1;
        }
    }

    //for moved or renamed
    for (int i = 0; i < count1; i++) {
        if (S_ISDIR(snap1[i].permission)) {
            int flag = 1;
            for (int j = 0; j < count2; j++) {
                if (snap1[i].inode == snap2[j].inode && strcmp(snap1[i].location, snap2[j].location) != 0) {
                    printf("Moved or renamed: %s -> %s\n", snap1[i].location, snap2[j].location);
                    flag = 1;
                    break;
                }
            }
            if (flag) {
                move = 1;
            }
        }
    }

    if (!rmv && !add && !move) {
        printf("NO CHANGES\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct Snapshot snaps[PATH_MAX];
    int count = 0;

    parseDirectory(argv[1], snaps, &count);
    savesnapshot("snapshot.txt", snaps, count);
    return EXIT_SUCCESS;
}
