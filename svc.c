#include "svc.h"

void *svc_init(void) {
    svc *g = (svc *)malloc(sizeof(svc));
    g->num_branches = 1;
    g->branch = (branch **)malloc(sizeof(branch *));

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
    g->HEAD = g->branch[0];

    g->num_commits = 0;
    g->commits = (commit **)malloc(sizeof(commit *) * 0);

    // return g;  //! svc* != void*
    return g;
}

void cleanup(void *helper) {
    if (helper == NULL) return;
    svc *g = (svc *)helper;

    //? Free branch --------------------
    for (size_t i = 0; i < g->num_branches; ++i) {
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
    for (size_t i = 0; i < g->num_commits; ++i) {
        free(g->commits[i]->message);
        free(g->commits[i]->committed);
        free(g->commits[i]);
    }
    free(g->commits);

    free(g);
}

char *read(char *file_path, size_t *n_bytes) {
    //! Always opened
    FILE *fptr = fopen(file_path, "rb");

    //! Jump to the last byte
    fseek(fptr, 0, SEEK_END);

    //! COUNT how many bytes
    *n_bytes = ftell(fptr);

    //! Jump back to the first byte
    rewind(fptr);

    char *content = (char *)malloc(sizeof(char) * (*n_bytes));

    //! Put all bytes from file to content
    fread(content, *n_bytes, 1, fptr);

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

    size_t n_bytes;
    char *file_contents = read(file_path, &n_bytes);

    size_t hash = 0;
    for (size_t i = 0; i < strlen(file_path); ++i) {
        hash = (hash + (unsigned char)file_path[i]) % 1000;
    }
    for (size_t i = 0; i < n_bytes; ++i) {
        hash = (hash + (unsigned char)file_contents[i]) % 2000000000;
    }
    fclose(fptr);

    for (size_t i = 0; i < g->num_snapshots; ++i)
        if (g->snapshot[i]->hash == hash) {
            free(file_contents);
            return hash;
        }

    (g->num_snapshots)++;
    g->snapshot =
        (file **)realloc(g->snapshot, sizeof(file *) * g->num_snapshots);

    g->snapshot[g->num_snapshots - 1] = (file *)malloc(sizeof(file));
    g->snapshot[g->num_snapshots - 1]->hash = hash;
    strcpy(g->snapshot[g->num_snapshots - 1]->name, file_path);
    g->snapshot[g->num_snapshots - 1]->content =
        (char *)malloc(sizeof(char) * n_bytes);
    strncpy(g->snapshot[g->num_snapshots - 1]->content, file_contents, n_bytes);
    g->snapshot[g->num_snapshots - 1]->num_char = n_bytes;
    free(file_contents);
    return hash;
}

/**
 * https://stackoverflow.com/questions/3489139/how-to-qsort-an-array-of-pointers-to-char-in-c
 */

size_t check_special_char(char x) {
    if (x >= 'a' && x <= 'z') return 0;
    if (x >= 'A' && x <= 'Z') return 0;
    return 1;
}

int CompareFunction(const void *p, const void *q) {
    char *a = (*(file **)p)->name;
    char *b = (*(file **)q)->name;
    size_t la = strlen(a);
    size_t lb = strlen(b);
    size_t minl = la;
    if (minl > lb) minl = lb;

    for (size_t i = 0; i < minl; ++i) {
        char ua = a[i];
        if (a[i] >= 'a' && a[i] <= 'z') ua = a[i] - 32;  // To lowercase
        char ub = b[i];
        if (b[i] >= 'a' && b[i] <= 'z') ub = b[i] - 32;  // To lowercase

        if (check_special_char(a[i]) == 1 || check_special_char(b[i]) == 1) {
            if (a[i] < b[i])
                return -1;
            else if (a[i] > b[i])
                return 1;
        } else {  // a[i] and b[i] are letters
            if (ua < ub)
                return -1;
            else if (ua > ub)
                return 1;
        }
    }

    return la - lb;
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
    sprintf(hex, "%06lx", num);
    return hex;
}

char *get_commit_id(void *helper, char *message) {
    svc *g = (svc *)helper;
    size_t id = 0;
    for (size_t i = 0; i < strlen(message); ++i)
        id = (id + (unsigned char)message[i]) % 1000;

    file **changed = (file **)malloc(sizeof(file *) * 0);
    size_t num_changed = 0;

    if (g->HEAD->last_commit != NULL) {
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

        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i) {
            FILE *fptr = fopen(g->HEAD->last_commit->committed[i]->name, "r");
            if (fptr == NULL) {
                ++num_changed;
                changed =
                    (file **)realloc(changed, sizeof(file *) * num_changed);
                changed[num_changed - 1] = g->HEAD->last_commit->committed[i];
            } else {
                fclose(fptr);
            }
        }
    }

    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i) {
        ++num_changed;
        changed = (file **)realloc(changed, sizeof(file *) * num_changed);
        changed[num_changed - 1] = g->HEAD->uncommitted[i];
    }

    if (g->HEAD->last_commit != NULL) {
        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i) {
            int new_hash =
                hash_file(helper, g->HEAD->last_commit->committed[i]->name);
            if (new_hash < 0) continue;
            if (g->HEAD->last_commit->committed[i]->hash != new_hash) {
                ++num_changed;
                changed =
                    (file **)realloc(changed, sizeof(file *) * num_changed);
                // changed[num_changed - 1] =
                // g->HEAD->last_commit->committed[i]; we can't assign changed
                // files to the last commit file because that file is the old
                // one We have to find a file from snapshot with hash = new_hash
                for (size_t j = 0; j < g->num_snapshots; ++j)
                    if (g->snapshot[j]->hash == new_hash) {
                        changed[num_changed - 1] = g->snapshot[j];
                        break;
                    }
            }
        }
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

    if (g->HEAD->last_commit != NULL) {
        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i) {
            if (g->HEAD->last_commit->committed[i]->hash !=
                hash_file(helper, g->HEAD->last_commit->committed[i]->name))
                return 0;
        }
    }

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

    if (g->HEAD->last_commit != NULL) {
        size_t unchanged = 1;
        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i) {
            for (size_t j = 0; j < g->HEAD->num_uncommitted; ++j) {
                if (strcmp(g->HEAD->uncommitted[j]->name,
                           g->HEAD->last_commit->committed[i]->name) == 0) {
                    unchanged = 0;
                    break;
                }
            }
            for (size_t j = 0; j < g->HEAD->num_untracked; ++j) {
                if (strcmp(g->HEAD->untracked[j]->name,
                           g->HEAD->last_commit->committed[i]->name) == 0) {
                    unchanged = 0;
                    break;
                }
            }
            if (unchanged == 1) {
                FILE *fptr =
                    fopen(g->HEAD->last_commit->committed[i]->name, "r");
                if (fptr != NULL) {
                    (new_commit->num_committed)++;
                    new_commit->committed = (file **)realloc(
                        new_commit->committed,
                        sizeof(file *) * new_commit->num_committed);
                    new_commit->committed[new_commit->num_committed - 1] =
                        g->HEAD->last_commit->committed[i];
                    fclose(fptr);
                }
            }
        }
    }

    (g->num_commits)++;
    g->commits =
        (commit **)realloc(g->commits, sizeof(commit *) * g->num_commits);
    g->commits[g->num_commits - 1] = new_commit;

    g->HEAD->last_commit = new_commit;
    g->HEAD->num_uncommitted = 0;
    g->HEAD->uncommitted = (file **)realloc(g->HEAD->uncommitted, 0);
    g->HEAD->num_untracked = 0;
    g->HEAD->untracked = (file **)realloc(g->HEAD->untracked, 0);

    return g->HEAD->last_commit->id;
}

void *get_commit(void *helper, char *commit_id) {
    if (commit_id == NULL) return NULL;
    svc *g = (svc *)helper;
    for (size_t i = 0; i < g->num_branches; ++i) {
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
    if (n_prev == NULL) {
        return NULL;
    }

    if (commit == NULL) {
        *n_prev = 0;
        return NULL;
    }

    struct commit *c = (struct commit *)commit;
    // svc *g = (svc *)helper;
    if (c->previous == NULL) {
        *n_prev = 0;
        return NULL;
    }
    if (get_commit(helper, c->id) == NULL) {
        *n_prev = 0;
        return NULL;
    }

    char **ret = (char **)malloc(sizeof(char *) * 0);
    *n_prev = 0;

    c = c->previous;
    while (c != NULL) {
        (*n_prev)++;
        ret = (char **)realloc(ret, sizeof(char *) * (*n_prev));
        ret[*n_prev - 1] = c->id;
        c = c->previous;
    }

    return ret;
}

char *branch_contains_commit(void *helper, char *commit_id) {
    svc *g = (svc *)helper;
    for (size_t i = 0; i < g->num_branches; ++i) {
        commit *cur = g->branch[i]->last_commit;
        while (cur != NULL) {
            if (strcmp(cur->id, commit_id) == 0) return g->branch[i]->name;
            cur = cur->previous;
        }
    }
    return NULL;
}

void print_commit(void *helper, char *commit_id) {
    if (commit_id == NULL) {
        printf("Invalid commit id\n");
        return;
    }

    // svc *g = (svc *)helper;
    commit *c = get_commit(helper, commit_id);
    if (c == NULL) {
        printf("Invalid commit id\n");
        return;
    }

    int n_prev = 0;
    char **prev_commits;
    prev_commits = get_prev_commits(helper, c, &n_prev);

    printf("%s [%s]: %s\n", c->id, branch_contains_commit(helper, c->id),
           c->message);

    if (n_prev == 0) {
        for (size_t i = 0; i < c->num_committed; ++i)
            printf("    + %s\n", c->committed[i]->name);
    } else {
        commit *prev = c->previous;
        for (size_t i = 0; i < c->num_committed; ++i) {
            size_t flag = 1;
            for (size_t j = 0; j < prev->num_committed; ++j) {
                if (strcmp(c->committed[i]->name, prev->committed[j]->name) ==
                    0) {
                    flag = 0;
                    break;
                }
            }
            if (flag == 1) printf("    + %s\n", c->committed[i]->name);
        }

        for (size_t i = 0; i < prev->num_committed; ++i) {
            size_t flag = 1;
            for (size_t j = 0; j < c->num_committed; ++j) {
                if (strcmp(prev->committed[i]->name, c->committed[j]->name) ==
                    0) {
                    flag = 0;
                    break;
                }
            }
            if (flag == 1) printf("    - %s\n", prev->committed[i]->name);
        }

        for (size_t i = 0; i < prev->num_committed; ++i) {
            for (size_t j = 0; j < prev->num_committed; ++j) {
                if (strcmp(prev->committed[i]->name, c->committed[j]->name) ==
                        0 &&
                    prev->committed[i]->hash != c->committed[j]->hash) {
                    printf("    / %s [%ld --> %ld]\n", c->committed[j]->name,
                           prev->committed[i]->hash, c->committed[j]->hash);
                    break;
                }
            }
        }
    }

    printf("\n    Tracked files (%ld):\n", c->num_committed);
    for (size_t i = 0; i < c->num_committed; ++i)
        printf("    [%10ld] %s\n", c->committed[i]->hash,
               c->committed[i]->name);
    free(prev_commits);
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
        printf("%s\n", g->branch[i]->name);
        list[i] = g->branch[i]->name;
    }

    return list;
}

int svc_add(void *helper, char *file_name) {
    if (file_name == NULL) return -1;
    svc *g = (svc *)helper;
    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i)
        if (strcmp(g->HEAD->uncommitted[i]->name, file_name) == 0) return -2;
    if (g->HEAD->last_commit != NULL) {
        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i)
            if (strcmp(g->HEAD->last_commit->committed[i]->name, file_name) ==
                0)
                return -2;
    }

    FILE *fptr = fopen(file_name, "r");
    if (fptr == NULL) return -3;
    fclose(fptr);

    for (size_t i = 0; i < g->HEAD->num_untracked; ++i) {
        if (strcmp(g->HEAD->untracked[i]->name, file_name) == 0) {
            (g->HEAD->num_uncommitted)++;
            g->HEAD->uncommitted =
                (file **)realloc(g->HEAD->uncommitted,
                                 sizeof(file *) * g->HEAD->num_uncommitted);
            g->HEAD->uncommitted[g->HEAD->num_uncommitted - 1] =
                g->HEAD->untracked[i];

            (g->HEAD->num_untracked)--;
            for (size_t j = i; j < g->HEAD->num_untracked; ++j) {
                g->HEAD->untracked[j] = g->HEAD->untracked[j + 1];
            }
            g->HEAD->untracked = (file **)realloc(
                g->HEAD->untracked, sizeof(file *) * g->HEAD->num_untracked);
            return g->HEAD->uncommitted[g->HEAD->num_uncommitted - 1]->hash;
        }
    }

    /// ADD filesystem to uncommitted
    int hash = hash_file(helper, file_name);
    (g->HEAD->num_uncommitted)++;
    g->HEAD->uncommitted = (file **)realloc(
        g->HEAD->uncommitted, sizeof(file *) * g->HEAD->num_uncommitted);
    g->HEAD->uncommitted[g->HEAD->num_uncommitted - 1] =
        g->snapshot[g->num_snapshots - 1];

    return hash;
}

int svc_rm(void *helper, char *file_name) {
    if (file_name == NULL) return -1;
    svc *g = (svc *)helper;

    size_t untracked = 1;
    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i)
        if (strcmp(g->HEAD->uncommitted[i]->name, file_name) == 0) {
            untracked = 0;
            break;
        }
    if (g->HEAD->last_commit != NULL) {
        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i)
            if (strcmp(g->HEAD->last_commit->committed[i]->name, file_name) ==
                0) {
                untracked = 0;
                break;
            }
    }

    if (untracked == 1) return -2;
    for (size_t i = 0; i < g->HEAD->num_uncommitted; ++i) {
        if (strcmp(g->HEAD->uncommitted[i]->name, file_name) == 0) {
            (g->HEAD->num_untracked)++;
            g->HEAD->untracked = (file **)realloc(
                g->HEAD->untracked, sizeof(file *) * g->HEAD->num_untracked);
            g->HEAD->untracked[(g->HEAD->num_untracked) - 1] =
                g->HEAD->uncommitted[i];  // on;y take 1 position of i not all
                                          // position of i

            (g->HEAD->num_uncommitted)--;
            for (size_t j = i; j < g->HEAD->num_uncommitted; ++j) {
                g->HEAD->uncommitted[j] = g->HEAD->uncommitted[j + 1];
            }
            g->HEAD->uncommitted =
                (file **)realloc(g->HEAD->uncommitted,
                                 sizeof(file *) * g->HEAD->num_uncommitted);
            return g->HEAD->untracked[g->HEAD->num_untracked - 1]->hash;
        }
    }
    if (g->HEAD->last_commit != NULL)
        for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i) {
            if (strcmp(g->HEAD->last_commit->committed[i]->name, file_name) ==
                0) {
                (g->HEAD->num_untracked)++;
                g->HEAD->untracked =
                    (file **)realloc(g->HEAD->untracked,
                                     sizeof(file *) * g->HEAD->num_untracked);
                g->HEAD->untracked[g->HEAD->num_untracked - 1] =
                    g->HEAD->last_commit->committed[i];
                return g->HEAD->untracked[g->HEAD->num_untracked - 1]->hash;
            }
        }
    return -2;
}

void write_file(file *f) {
    FILE *fptr = fopen(f->name, "wb");
    if (fptr == NULL) return;
    fwrite(f->content, 1, f->num_char, fptr);
    fclose(fptr);
}

int svc_reset(void *helper, char *commit_id) {
    if (commit_id == NULL) return -1;
    svc *g = (svc *)helper;

    size_t flag = 0;

    // find the commit which has id = commit_id
    commit *cur = g->HEAD->last_commit;
    while (cur != NULL) {
        if (strcmp(cur->id, commit_id) == 0) {
            flag = 1;
            break;
        }
        cur = cur->previous;
    }
    if (flag == 0) return -2;
    g->HEAD->num_uncommitted = g->HEAD->num_untracked = 0;
    g->HEAD->uncommitted = (file **)realloc(g->HEAD->uncommitted, 0);
    g->HEAD->untracked = (file **)realloc(g->HEAD->untracked, 0);

    g->HEAD->last_commit = cur;

    for (size_t i = 0; i < g->HEAD->last_commit->num_committed; ++i)
        write_file(g->HEAD->last_commit->committed[i]);

    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions,
                int n_resolutions) {
    // TODO: Implement
    return NULL;
}
