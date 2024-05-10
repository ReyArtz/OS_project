#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>

#define PATH_MAX 4096
#define MAX_CHILDREN 10

enum DifferenceType {
    NO_DIFFERENCE,
    FILE_ADDED,
    FILE_DELETED,
    FILE_UPDATED,
    FILE_MOVED,
    FILE_RENAMED
};

struct Snapshot {
    char name[PATH_MAX];
    mode_t permission;
    off_t size;
    char location[PATH_MAX];
    ino_t inode;
    time_t modif_time;
};

void parseDirectory(const char* dirp, struct Snapshot *snapshot, int *count) {
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

void saveSnapshot(const char* filename , struct Snapshot *snapshot, int count) {
    FILE* file = fopen(filename, "w");

    if (file == NULL) {
        perror("Error opening the file");
        return;
    }

    for (int i = 0; i < count; ++i) {
        fprintf(file, "%s\n%o\n%ld\n%lu\n%ld\n%s%s%s", snapshot[i].location, snapshot[i].permission, 
                (long)snapshot[i].size, (unsigned long)snapshot[i].inode, 
                (long)snapshot[i].modif_time, ctime(&snapshot[i].modif_time),
                ctime(&snapshot[i].modif_time), ctime(&snapshot[i].modif_time));
    }

    fclose(file);
}

void compare(struct Snapshot *snap1, struct Snapshot *snap2, int count1, int count2) {
    int added = 0, deleted = 0, updated = 0, moved = 0, renamed = 0;

    for (int i = 0; i < count1; i++) {
        int found = 0; 
        for (int j = 0; j < count2; j++) {
            if (snap1[i].inode == snap2[j].inode) {
                found = 1;
                
                if (strcmp(snap1[i].location, snap2[j].location) != 0 ||
                    snap1[i].size != snap2[j].size ||
                    snap1[i].modif_time != snap2[j].modif_time ||
                    snap1[i].permission != snap2[j].permission) {
                    updated++;
                    printf("Updated: %s\n", snap1[i].location);
                }
                break; 
            }
        }
        if (!found) {
            printf("Removed: %s\n", snap1[i].location);
            deleted++;
        }
    }

    // added entries
    for (int i = 0; i < count2; i++) {
        int found = 0; 
        for (int j = 0; j < count1; j++) {
            if (snap2[i].inode == snap1[j].inode) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Added: %s\n", snap2[i].location);
            added++;
        }
    }

    // moved or renamed entries
    for (int i = 0; i < count1; i++) {
        int found = 0;
        for (int j = 0; j < count2; j++) {
            if (snap1[i].inode == snap2[j].inode && strcmp(snap1[i].location, snap2[j].location) != 0) {
                found = 1;
                printf("Moved or renamed: %s -> %s\n", snap1[i].location, snap2[j].location);
                moved++;
                break;
            }
        }
    }

    if (added == 0 && deleted == 0 && updated == 0 && moved == 0 && renamed == 0) {
        printf("NO CHANGES\n");
    }
}

void isolateDirectory(const char* dir, const char* isolated_space_dir, int pipe) {
    struct Snapshot snaps[500];
    int count = 0;

    parseDirectory(dir, snaps, &count);

    for (int i = 0; i < count; i++) {
        if (snaps[i].permission == 0) {
            printf("Analyze and isolate the file %s\n", snaps[i].location);
            char cmd[550];
            sprintf(cmd, " verify for malicious: %s\n", snaps[i].location);
            int res = system(cmd);
            write(pipe, &res, sizeof(res));
            if (res == 0) {
                char mvcmd[600];
                sprintf(mvcmd, "move %s %s", snaps[i].location, isolated_space_dir);
                system(mvcmd);
            } else {
                printf("File %s is safe\n", snaps[i].location);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 5 || strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Usage: %s -o output_dir -s isolated_space_dir dir1 dir2 dir3 ... dirN\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* output_dir = NULL;
    int directories = argc - 5;
    char* isolated_space_dir = NULL;
    int status;
    pid_t child_pid[MAX_CHILDREN];
    int pipe_fd[2];

    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[3], "-s") != 0) {
        fprintf(stderr, "Usage: %s -o output_dir -s isolated_space_dir dir1 dir2 dir3 ... dirN\n", argv[0]);
        return EXIT_FAILURE;
    } else {
        isolated_space_dir = argv[4];
    }

    for (int i = 0; i < directories && i < MAX_CHILDREN; i++) {
        child_pid[i] = fork();

        if (child_pid[i] < 0) {
            perror("Fork failed");
            return EXIT_FAILURE;
        } else if (child_pid[i] == 0) {  
            printf("Child process for directory %s started with PID %d\n", argv[i + 5], getpid());

            struct Snapshot snaps[500];
            int count = 0;
            parseDirectory(argv[i + 5], snaps, &count);
            saveSnapshot(output_dir, snaps, count);

            printf("Snapshot for Directory %s created successfully.\n", argv[i + 5]);

            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < directories && i < MAX_CHILDREN; i++) {
        waitpid(child_pid[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("Child Process terminated with PID %d and exit code %d.\n", child_pid[i], WEXITSTATUS(status));
        }
    }

    printf("Comparing snapshots...\n");
    struct Snapshot snaps[500];
    struct Snapshot snaps2[500];
    int count = 0;
    int count2 = 0;

    FILE* snap1 = fopen("snapshot.txt", "r");
    if (snap1 != NULL) {
        char line[600];

        while (fgets(line, sizeof(line), snap1)) {
            sscanf(line, "%s\n%o\n%ld\n%lu\n%ld\n%*s", snaps[count].location, &snaps[count].permission, 
                &snaps[count].size, &snaps[count].inode, &snaps[count].modif_time);
            count++;
        }
        fclose(snap1);
    }

    parseDirectory(output_dir, snaps2, &count2);

    compare(snaps, snaps2, count, count2);

    return EXIT_SUCCESS;
}
