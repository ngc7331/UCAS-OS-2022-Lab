#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

#define IMAGE_FILE "./image"
#define BATCH_FILE_0 "autorun.bat"
#define BATCH_FILE_1 "manual.bat"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_NUM_LOC (OS_SIZE_LOC - 2)
#define TASK_INFO_P_LOC (TASK_NUM_LOC - 8)
#define BATCH_NUM_LOC (TASK_INFO_P_LOC - 2)
#define BATCH_INFO_P_LOC (BATCH_NUM_LOC - 8)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

// task_info_t
typedef struct {
    char name[32];
    uint64_t entrypoint;
    int size;
    int phyaddr;
} task_info_t;

typedef struct {
    char name[32];
    int size;
    int phyaddr;
    int execute_on_load;
} batch_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

#define BATCH_MAXNUM 16
static batch_info_t batchinfo[BATCH_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_batch_file(FILE *img, FILE *batch);
static void write_batch_info(batch_info_t *batchinfo, short batchnum, FILE *img);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img);

int main(int argc, char **argv) {
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

static void create_image(int nfiles, char *files[]) {
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    // open the image file
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    // for each input file
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;

        // open input file
        fp = fopen(*files, "r");
        assert(fp != NULL);

        // read ELF header
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        // create taskinfo
        task_info_t task;
        strcpy(task.name, *files);
        task.entrypoint = get_entrypoint(ehdr);
        task.phyaddr = phyaddr;

        // for each program header
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            // read program header
            read_phdr(&phdr, fp, ph, ehdr);

            // write segment to the image
            write_segment(phdr, fp, img, &phyaddr);

            // update nbytes_kernel
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
        }

        // padding bootblock
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }

        task.size = phyaddr - task.phyaddr;
        if (taskidx >= 0)  // skip bootblock & main
            taskinfo[taskidx] = task;

        fclose(fp);
        files++;
    }

    // write img info (os size / tasks / tasknum)
    write_img_info(nbytes_kernel, taskinfo, tasknum, img);

    // TEST: write batch file(s)
    FILE *batch = fopen(BATCH_FILE_0, "r");
    assert(batch != NULL);
    // name is BATCH_FILE
    strcpy(batchinfo[0].name, BATCH_FILE_0);
    // phyaddr is the current position of STREAM *img
    batchinfo[0].phyaddr = ftell(img);
    // write batch file into img
    write_batch_file(img, batch);
    // size is the current position of STREAM *batch
    batchinfo[0].size = ftell(batch);
    // close
    fclose(batch);
    // execute on load
    batchinfo[0].execute_on_load = 1;

    // the same as above, but disable execute on load
    batch = fopen(BATCH_FILE_1, "r");
    assert(batch != NULL);
    strcpy(batchinfo[1].name, BATCH_FILE_1);
    batchinfo[1].phyaddr = ftell(img);
    write_batch_file(img, batch);
    batchinfo[1].size = ftell(batch);
    fclose(batch);
    batchinfo[1].execute_on_load = 0;

    // write batch info
    write_batch_info(batchinfo, 2, img);
    // TEST: write batch file(s) end

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp) {
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr) {
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr) {
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr) {
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr) {
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr) {
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr) {
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kern, task_info_t *taskinfo,
                           short tasknum, FILE * img) {
    // write image info to some certain places
    // os size, information about app-info sector(s) ...

    // write os size
    int nsecs_kern = NBYTES2SEC(nbytes_kern);
    printf("OS sectors: %d\n", nsecs_kern);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fwrite(&nsecs_kern, sizeof(int), 1, img);

    // write taskinfos to the end of img
    fseek(img, 0, SEEK_END);
    long taskinfos_phyaddr = ftell(img);
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);

    // write task num
    printf("Task num: %d\n", tasknum);
    fseek(img, TASK_NUM_LOC, SEEK_SET);
    fwrite(&tasknum, sizeof(short), 1, img);

    // write task infos phyaddr
    printf("Task infos phyaddr: 0x%lx\n", taskinfos_phyaddr);
    fseek(img, TASK_INFO_P_LOC, SEEK_SET);
    fwrite(&taskinfos_phyaddr, sizeof(long), 1, img);

    fseek(img, 0, SEEK_END);
}

static void write_batch_file(FILE *img, FILE *batch) {
    int buf;
    while ((buf=fgetc(batch)) != EOF)
        fputc(buf, img);
    fputc(0, img);
}

static void write_batch_info(batch_info_t *batchinfo, short batchnum, FILE *img) {
    // write batchinfos to the end of img
    fseek(img, 0, SEEK_END);
    long batchinfos_phyaddr = ftell(img);
    fwrite(batchinfo, sizeof(batch_info_t), batchnum, img);

    // write batch num
    printf("Batch num: %d\n", batchnum);
    fseek(img, BATCH_NUM_LOC, SEEK_SET);
    fwrite(&batchnum, sizeof(short), 1, img);

    // write batch infos phyaddr
    printf("Batch infos phyaddr: 0x%lx\n", batchinfos_phyaddr);
    fseek(img, BATCH_INFO_P_LOC, SEEK_SET);
    fwrite(&batchinfos_phyaddr, sizeof(long), 1, img);
}

/* print an error message and exit */
static void error(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
