#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int fd = sys_fopen("2.txt", O_RDWR);

    sys_lseek(fd, 512 * 1024 * 1024, SEEK_SET);

    // write 'hello world!' * 10
    for (int i = 0; i < 10; i++)
    {
        sys_fwrite(fd, "hello world!\n", 13);
    }

    // read
    for (int i = 0; i < 10; i++)
    {
        sys_fread(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }

    sys_fclose(fd);

    return 0;
}