#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define BUFSIZE 100
#define TESTCNT 3

int symlinktest1();
int symlinktest2();
int symlinktest3();

int stdout = 1;
char buf[BUFSIZE];
char buf_link[BUFSIZE];

char *test_name_list[TESTCNT] = {
    "Simple Symlink",
    "Linked Symlink",
    "Miss Symlink"};

int (*test_list[TESTCNT])(void) = {
    symlinktest1,
    symlinktest2,
    symlinktest3};

int symlinktest1()
{
    int fd;

    fd = open("symlinkdummy", O_CREATE | O_RDWR);

    if (fd < 0)
    {
        printf(stdout, "error: create file for symbolic link failed!\n");
        exit();
    }

    strcpy(buf, "Symbolic Test Running...");

    if (write(fd, buf, 512) != 512)
    {
        printf(stdout, "error: write file for symbolic link failed\n");
        exit();
    }

    sync();
    close(fd);

    symlink("symlinkdummy", "linksymlinkdummy");

    fd = open("linksymlinkdummy", O_RDONLY);

    read(fd, buf_link, sizeof(buf_link));

    close(fd);

    if (strcmp(buf, buf_link))
    {
        printf(2, "Test Fail...!\n[Original]: %s\n[Symbolic]: %s\n", buf, buf_link);
        return -1;
    }

    printf(2, "  [Test Result]\n  Original: %s\n  Symbolic: %s\n", buf, buf_link);

    unlink("symlinkdummy");
    unlink("linksymlinkdummy");

    return 0;
}

int symlinktest2()
{
    int fd;
    char *file_name_list[5] = {
        "test1",
        "test2",
        "test3",
        "test4",
        "test5",
    };

    fd = open(file_name_list[0], O_CREATE | O_RDWR);

    if (fd < 0)
    {
        printf(stdout, "error: create file for symbolic link failed!\n");
        exit();
    }

    strcpy(buf, "Symbolic Recursive Test Success!!");

    if (write(fd, buf, 512) != 512)
    {
        printf(stdout, "error: write file for symbolic link failed\n");
        exit();
    }

    sync();
    read(fd, buf_link, sizeof(buf_link));
    close(fd);

    for (int i = 1; i < 5; ++i)
    {
        symlink(file_name_list[i - 1], file_name_list[i]);
    }

    fd = open(file_name_list[1], O_RDONLY);

    read(fd, buf_link, sizeof(buf_link));

    printf(2, "%s\n", buf_link);

    close(fd);

    if (strcmp(buf, buf_link))
    {
        printf(2, "Test Fail : [Original]: %s, \n[Symbolic]: %s\n", buf, buf_link);
        return -1;
    }
    printf(2, "  [Test Result]\n    Original: %s\n    Symbolic: %s\n", buf, buf_link);

    for (int i = 0; i < 5; ++i)
    {
        unlink(file_name_list[i]);
    }
    return 0;
}

int symlinktest3()
{
    int fd;

    fd = open("missfile", O_CREATE | O_RDWR);

    if (fd < 0)
    {
        printf(stdout, "error: create file for symbolic link failed!\n");
        exit();
    }

    strcpy(buf, "Symbolic Missing Test Running...");

    if (write(fd, buf, 512) != 512)
    {
        printf(stdout, "error: write file for symbolic link failed\n");
        exit();
    }

    sync();
    close(fd);

    symlink("missfile", "linkfile");
    unlink("missfile");
    fd = open("linkfile", O_RDONLY);
    if (fd < 0)
    {
        unlink("linkfile");
        printf(2, "  [Test Result]\n    Reference file is missed.\n", buf, buf_link);
        return 0;
    }

    return -1;
}

int main(int argc, char *argv[])
{
    printf(stdout, "soft symbolic test\n\n");

    for (int i = 0; i < TESTCNT; ++i)
    {
        printf(2, "[%s] Start\n", test_name_list[i]);
        if (test_list[i]() != 0)
        {
            printf(2, "%s failed\n", test_name_list[i]);
            exit();
        }
        printf(2, "[%s] Finish\n\n", test_name_list[i]);
    }

    exit();
}