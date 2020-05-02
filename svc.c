#include "svc.h"

void *svc_init(void) {
    svc *g = (svc *)malloc(sizeof(svc));
    g->num_branches = 1;
    g->branch = (branch **)malloc(sizeof(branch *));
    g->HEAD = g->branch[0];
    g->num_snapshots = 0;
    g->snapshot = (file **)malloc(sizeof(file *) * 0);

    // TODO: INITIALIZE branch master
    g->branch[0] = (branch *)malloc(sizeof(branch));
    g->branch[0]->last_commit = NULL;
    strcpy(g->branch[0]->name, "master");
    g->branch[0]->num_uncommitted = 0;
    g->branch[0]->num_untracked = 0;
    g->branch[0]->uncommitted = (file **)malloc(sizeof(file *) * 0);
    g->branch[0]->untracked = (file **)malloc(sizeof(file *) * 0);
    // return g;  //! svc* != void*
    return g;
}

void cleanup_commit(commit *cur) {
    if (cur == NULL) return;
    cleanup_commit(cur->previous);
    free(cur->message);
    free(cur->committed);
    free(cur);
}

void cleanup(void *helper) {
    if (helper == NULL) return;
    svc *g = (svc *)helper;

    //? Free branch --------------------
    for (size_t i = 0; i < g->num_branches; ++i) {
        cleanup_commit(g->branch[i]->last_commit);
        free(g->branch[i]->uncommitted);
        free(g->branch[i]->untracked);
        free(g->branch[i]);
    }
    free(g->branch);
    //? --------------------------------

    //! Free snapshot -----------------
    for (size_t i = 0; i < g->num_snapshots; ++i) {
        free(g->snapshot[i]->content);
        free(g->snapshot[i]);
    }
    free(g->snapshot);
    //! --------------------------------

    free(g);
}

char *read(char *file_path) {
    //! Always opened
    FILE *fptr = fopen(file_path, "rb");

    //! Jump to the last byte
    fseek(fptr, 0, SEEK_END);

    //! COUNT how many bytes
    size_t n_bytes = ftell(fptr);

    //! Jump back to the first byte
    rewind(fptr);

    char *content = (char *)malloc(sizeof(char) * (n_bytes + 1));

    //! Put all bytes from file to content
    fread(content, n_bytes, 1, fptr);
    content[n_bytes - 1] = '\0';

    fclose(fptr);

    return content;
}

int hash_file(void *helper, char *file_path) {
    if (file_path == NULL) return -1;

    // TODO: fopen() open file
    FILE *fptr = fopen(file_path, "r");

    //! can't read file
    if (fptr == NULL) return -2;

    svc *g = (svc *)helper;

    char *file_contents = read(file_path);
    size_t file_length = strlen(file_contents);

    size_t hash = 0;
    for (size_t i = 0; i < strlen(file_path); ++i) {
        hash = (hash + (unsigned char)file_path[i]) % 1000;
    }
    for (size_t i = 0; i < file_length; ++i) {
        hash = (hash + (unsigned char)file_contents[i]) % 2000000000;
    }

    fclose(fptr);
    return hash;
}

/**
 * https://stackoverflow.com/questions/3489139/how-to-qsort-an-array-of-pointers-to-char-in-c
 */
int CompareFunction(const void *p, const void *q) {
    file **a = (file **)p;
    file **b = (file **)q;

    return strcmp((*a)->name, (*b)->name);
}

//! check file is in previous commit
//? return 1: added
//? return 0: not added
// TODO: file in changed list
size_t check_addition(file *file, commit *prev) {
    if (prev == NULL) return 1;
    for (size_t i = 0; i < prev->num_committed; ++i)
        if (strcmp(file->name, prev->committed[i]->name) == 0) return 0;
    return 1;
}

size_t check_deletion(file *file, commit *prev) {
    if (prev == NULL) return 0;

    for (size_t i = 0; i < prev->num_committed; ++i)
        if (strcmp(file->name, prev->committed[i]->name) == 0) {
            if (file->hash == prev->committed[i]->hash) return 1;
        }
    return 0;
}

size_t check_change(file *file, commit *prev) {
    if (prev == NULL) return 0;

    for (size_t i = 0; i < prev->num_committed; ++i)
        if (strcmp(file->name, prev->committed[i]->name) == 0) {
            if (file->hash != prev->committed[i]->hash) return 1;
        }
    return 0;
}

char *hex_value(size_t num) {
    char *hex = (char *)malloc(sizeof(char) * 7);
    sprintf(hex, "%lx", num);
    return hex;
}

char *get_commit_id(void *helper, char *message) {
    svc *g = (svc *)helper;
    size_t id = 0;
    for (size_t i = 0; strlen(message); ++i)
        id = (id + (unsigned char)message[i]) % 1000;

    file **changed = (file **)malloc(sizeof(file *) * 0);
    size_t num_changed = 0;

    for (size_t i = 0; i < g->HEAD->num_untracked; ++i) {
        for (size_t j = 0; j < g->HEAD->last_commit->num_committed; ++j) {
            if (strcmp(g->HEAD->last_commit->committed[j]->name,
                       g->HEAD->untracked[i]->name) == 0) {
                ++num_changed;
                changed =
                    (file **)realloc(changed, sizeof(file *) * num_changed);
                changed[num_changed - 1] = g->HEAD->untracked[i];
                break;
            }
        }
    }

    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i) {
        ++num_changed;
        changed = (file **)realloc(changed, sizeof(file *) * num_changed);
        changed[num_changed - 1] = g->HEAD->uncommitted[i];
    }

    qsort(changed, num_changed, sizeof(file *), CompareFunction);

    for (size_t i = 0; i < num_changed; ++i) {
        if (check_addition(changed[i], g->HEAD->last_commit) == 1) {
            id += 376591;
        }
        if (check_deletion(changed[i], g->HEAD->last_commit) == 1) {
            id += 85973;
        }
        if (check_change(changed[i], g->HEAD->last_commit) == 1) {
            id += 9573681;
        }
        for (size_t j = 0; j < strlen(changed[i]->name); ++j) {
            id =
                (id * ((unsigned char)changed[i]->name[j] % 37)) % 15485863 + 1;
        }
    }

    free(changed);
    return hex_value(id);
}

size_t check_unchanged(void *helper) {
    size_t num_changed = 0;
    svc *g = (svc *)helper;

    for (size_t i = 0; i < g->HEAD->num_untracked; ++i) {
        for (size_t j = 0; j < g->HEAD->last_commit->num_committed; ++j)
            if (strcmp(g->HEAD->last_commit->committed[j]->name,
                       g->HEAD->untracked[i]->name) == 0) {
                ++num_changed;
                break;
            }
    }

    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i) ++num_changed;

    if (num_changed == 0) return 1;
    return 0;
}

char *svc_commit(void *helper, char *message) {
    if (message == NULL) return NULL;

    svc *g = (svc *)helper;

    if (check_unchanged(helper) == 1) return NULL;

    commit *new_commit = (commit *)malloc(sizeof(commit));

    char *id = get_commit_id(helper, message);
    strcpy(new_commit->id, id);
    free(id);

    new_commit->message = (char *)malloc(sizeof(char) * (strlen(message) + 1));
    strcpy(new_commit->message, message);
    new_commit->previous = g->HEAD->last_commit;
    new_commit->num_committed = 0;
    new_commit->committed = (file **)malloc(sizeof(file *) * 0);

    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i) {
        (new_commit->num_committed)++;
        new_commit->committed =
            (file **)realloc(new_commit->committed,
                             sizeof(struct file *) * new_commit->num_committed);
        new_commit->committed[new_commit->num_committed - 1] =
            g->HEAD->uncommitted[i];
    }

    return NULL;
}

void *get_commit(void *helper, char *commit_id) {
    if (commit_id == NULL) return NULL;
    svc *g = (svc *)helper;
    for (size_t i = 0; g->num_branches; ++i) {
        // recursive each commit -> check its if id = commit_id
        commit *cur = g->branch[i]->last_commit;
        while (cur != NULL) {
            if (strcmp(cur->id, commit_id) == 0) return cur;
            cur = cur->previous;
        }
    }
    return NULL;
}

char **get_prev_commits(void *helper, void *commit, int *n_prev) {
    // TODO: Implement
    return NULL;
}

void print_commit(void *helper, char *commit_id) {
    // TODO: Implement
}

/**
 *  return 1: valid
 *  return 0: invalid
 **/
size_t check_valid_branch_name(char *name) {
    for (size_t i = 0; i < strlen(name); ++i)
        if (name[i] != '_' && name[i] != '-' && name[i] != '/' &&
            (name[i] < 'a' || name[i] > 'z') &&
            (name[i] < 'A' || name[i] > 'Z') &&
            (name[i] < '0' || name[i] > '9'))
            return 0;
    return 1;
}

int svc_branch(void *helper, char *branch_name) {
    if (branch_name == NULL || check_valid_branch_name(branch_name) == 0)
        return -1;

    svc *g = (svc *)helper;
    for (size_t i = 0; i < g->num_branches; ++i)
        if (strcmp(g->branch[i]->name, branch_name) == 0) {
            return -2;
        }

    if (g->HEAD->num_uncommitted > 0) return -3;

    (g->num_branches)++;
    g->branch =
        (branch **)realloc(g->branch, sizeof(branch *) * g->num_branches);

    branch *new_branch = (branch *)malloc(sizeof(branch));
    new_branch->last_commit = g->HEAD->last_commit;
    strcpy(new_branch->name, branch_name);
    new_branch->num_uncommitted = 0;
    new_branch->num_untracked = 0;
    new_branch->uncommitted = (file **)malloc(sizeof(file *) * 0);
    new_branch->untracked = (file **)malloc(sizeof(file *) * 0);

    g->branch[g->num_branches - 1] = new_branch;
    return 0;
}

int svc_checkout(void *helper, char *branch_name) {
    if (branch_name == NULL) return -1;
    svc *g = (svc *)helper;

    size_t flag = 0;
    size_t pos = 0;
    for (size_t i = 0; i < g->num_branches; ++i)
        if (strcmp(g->branch[i]->name, branch_name) == 0) {
            flag = 1;
            pos = i;
            break;
        }
    if (flag == 0) return -1;

    if (g->HEAD->num_uncommitted > 0) return -2;
    g->HEAD = g->branch[pos];
    return 0;
}

char **list_branches(void *helper, int *n_branches) {
    if (n_branches == NULL) return NULL;
    svc *g = (svc *)helper;

    *n_branches = g->num_branches;
    char **list = (char **)malloc(sizeof(char *) * (*n_branches));
    for (size_t i = 0; i < g->num_branches; ++i) {
        list[0] =
            (char *)malloc(sizeof(char) * (strlen(g->branch[i]->name) + 1));
        strcpy(list[0], g->branch[i]->name);
    }

    return list;
}

int svc_add(void *helper, char *file_name) {
    // TODO: Implement
    return 0;
}

int svc_rm(void *helper, char *file_name) {
    // TODO: Implement
    return 0;
}

void delete_commits(commit *target, commit *cur) {
    if (cur == target) return;
    // recursion
    delete_commits(target, cur->previous);

    free(cur->committed);
    free(cur->message);
    free(cur);
}

int svc_reset(void *helper, char *commit_id) {
    if (commit_id == NULL) return -1;
    svc *g = (svc *)helper;

    size_t flag = 0;
    commit *cur = g->HEAD->last_commit;
    while (cur != NULL) {
        if (strcmp(cur->id, commit_id) == 0) {
            flag = 1;
            break;
        }
        cur = cur->previous;
    }
    if (flag == 0) return -2;

    delete_commits(cur, g->HEAD->last_commit);
    g->HEAD->last_commit = cur;
    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions,
                int n_resolutions) {
    // TODO: Implement
    return NULL;
}