#include "svc.h"

void write_to_file_2(char *path, char *contents) {
    FILE *fp;
    /* open the file for writing*/
    fp = fopen(path, "w");

    char *sentence = contents;
    fprintf(fp, "%s", sentence);

    /* close the file*/
    fclose(fp);
}

void test0() {
    void *h = svc_init();
    cleanup(h);
}

void test1() {
    void *helper = svc_init();
    write_to_file_2("hello.py", "print(\"Hello\")\n");
    printf("%d\n", hash_file(helper, "hello.py"));
    printf("%d\n", hash_file(helper, "fake.c"));
    if (svc_commit(helper, "No changes") == NULL) printf("PASS\n");
    printf("%d\n", svc_add(helper, "hello.py"));
    write_to_file_2("Tests/test1.in", "5 3 2\n");
    printf("%d\n", svc_add(helper, "Tests/test1.in"));
    printf("%d\n", svc_add(helper, "Tests/test1.in"));
    printf("%s\n", svc_commit(helper, "Initial commit"));
    void *commit = get_commit(helper, "74cde7");
    printf("%p\n", commit);
    int n_prev;
    char **prev_commits = get_prev_commits(helper, commit, &n_prev);
    if (prev_commits == NULL) printf("PASS\n");
    printf("%d\n", n_prev);
    print_commit(helper, "74cde7");
    // int n;
    // char **branches = list_branches(helper, &n);
    // for (int i = 0; i < n; ++i) printf("%s\n", branches[i]);
    // printf("%d\n", n);
    // for (int i = 0; i < n; ++i) free(branches[i]);
    // free(branches);
    cleanup(helper);
}

// prettier-ignore
void test2() {
    void *helper = svc_init();
    write_to_file_2(
        "COMP2017/svc.h",
        "#ifndef svc_h\n#define svc_h\nvoid *svc_init(void);\n#endif\n");
    write_to_file_2("COMP2017/svc.c",
                    "#include \"svc.h\"\nvoid *svc_init(void) {\n    // TODO: "
                    "implement\n}\n");
    printf("%d\n", svc_add(helper, "COMP2017/svc.h"));
    printf("%d\n", svc_add(helper, "COMP2017/svc.c"));
    printf("%s\n", svc_commit(helper, "Initial commit"));
    printf("%d\n", svc_branch(helper, "random_branch"));
    write_to_file_2(
        "COMP2017/svc.c",
        "#include \"svc.h\"\nvoid *svc_init(void) {\n    return NULL;\n}\n");
    printf("%d\n", svc_rm(helper, "COMP2017/svc.h"));
    printf("%s\n", svc_commit(helper, "Implemented svc_init"));
    remove("COMP2017/svc.h");
    printf("%d\n", svc_reset(helper, "7b3e30"));
    write_to_file_2(
        "COMP2017/svc.c",
        "#include \"svc.h\"\nvoid *svc_init(void) {\n    return NULL;\n}\n");
    printf("%s\n", svc_commit(helper, "Implemented svc_init"));
    void *commit = get_commit(helper, "24829b");

    int n_prev;
    char **prev_commits = get_prev_commits(helper, commit, &n_prev);
    printf("%s\n", prev_commits[0]);
    printf("%d\n", n_prev);
    free(prev_commits);

    write_to_file_2(
        "resolutions/svc.c",
        "#include \"svc.h\"\nvoid *svc_init(void) {\n    return NULL;\n}\n");

    resolution *resolutions = malloc(sizeof(resolution));
    resolutions[0].file_name = "COMP2017/svc.c";
    resolutions[0].resolved_file = "resolutions/svc.c";

    // svc_merge(helper, "random_branch", resolutions, 1);
    printf("%s\n", svc_merge(helper, "random_branch", resolutions, 1));

    free(resolutions);

    commit = get_commit(helper, "48eac3");
    prev_commits = get_prev_commits(helper, commit, &n_prev);
    printf("%s --> %s\n", prev_commits[0], prev_commits[1]);
    printf("%d\n", n_prev);

    free(prev_commits);

    cleanup(helper);
}

int main() {
    test1();
    test2();
}