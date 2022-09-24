#include "common.h"

register_argument(WORD, pid, "input pid of process");
register_argument(WORD, vaddr, "input virtual addr of process");

#define PFN_MASK_SIZE 8

/*
 * Get physical address of any mapped virtual address in the current process.
 */
uint64_t mem_virt2phy(uint64_t virtaddr)
{
    int fd, retval;
    uint64_t page, physaddr;
    unsigned long virt_pfn;
    int page_size;
    off_t offset;
    char pagemap_path[256];

    /* standard page size */
    page_size = getpagesize();

    sprintf (pagemap_path, "/proc/%ld/pagemap", pid);
    fd = open(pagemap_path, O_RDONLY);
    if (fd < 0)
    {
	printf("%s(): cannot open /proc/self/pagemap: %s\n",
		__func__, strerror(errno));
	return -1;
    }

    virt_pfn = (unsigned long)virtaddr / page_size;
    offset = sizeof(uint64_t) * virt_pfn;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
	printf("%s(): seek error in /proc/self/pagemap: %s\n",
		__func__, strerror(errno));
	close(fd);
	return -1;
    }

    retval = read(fd, &page, PFN_MASK_SIZE);
    close(fd);
    if (retval < 0)
    {
	printf("%s(): cannot read /proc/self/pagemap: %s\n",
		__func__, strerror(errno));
	return -1;
    }
    else if (retval != PFN_MASK_SIZE)
    {
	printf("%s(): read %d bytes from /proc/self/pagemap "
		"but expected %d:\n",
		__func__, retval, PFN_MASK_SIZE);
	return -1;
    }

    /*
     * the pfn (page frame number) are bits 0-54 (see
     * pagemap.txt in linux Documentation)
     */
    if ((page & 0x7fffffffffffffULL) == 0) {
	printf ("not found PFN\n");
	return -1;
    }
    printf ("PFN: 0x%lx\n", page);
    physaddr = ((page & 0x7fffffffffffffULL) * page_size) + ((unsigned long)virtaddr % page_size);

    return (physaddr);
}

int main(int argc, char *argv[])
{
    uint64_t pa = mem_virt2phy(vaddr);
    if (pa != -1)
	printf ("physical addr: 0x%lx\n", pa);
    else
	printf ("translate failed\n");
}

