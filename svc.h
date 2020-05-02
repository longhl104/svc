#ifndef svc_h
#define svc_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct svc {
    branch *HEAD;
    size_t num_branches;
    branch **branch;  // list of branches

    size_t num_snapshots;
    file **snapshot;
} svc;

typedef struct branch {
    char name[51];
    commit *last_commit;

    size_t num_untracked;
    file **untracked;

    size_t num_uncommitted;
    file **uncommitted;
} branch;

typedef struct commit {
    char id[7];
    char *message;
    commit *previous;

    size_t num_committed;
    file **committed;
} commit;

typedef struct file {
    size_t hash;
    char name[261];
    size_t num_char;
    char *content;
} file;

typedef struct resolution {
    // NOTE: DO NOT MODIFY THIS STRUCT
    char *file_name;
    char *resolved_file;
} resolution;

void *svc_init(void);

void cleanup(void *helper);

int hash_file(void *helper, char *file_path);

char *svc_commit(void *helper, char *message);

void *get_commit(void *helper, char *commit_id);

char **get_prev_commits(void *helper, void *commit, int *n_prev);

void print_commit(void *helper, char *commit_id);

int svc_branch(void *helper, char *branch_name);

int svc_checkout(void *helper, char *branch_name);

char **list_branches(void *helper, int *n_branches);

int svc_add(void *helper, char *file_name);

int svc_rm(void *helper, char *file_name);

int svc_reset(void *helper, char *commit_id);

char *svc_merge(void *helper, char *branch_name, resolution *resolutions,
                int n_resolutions);

#endif
