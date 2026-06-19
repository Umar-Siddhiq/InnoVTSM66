#include <errno.h>
#include <sys/types.h>

extern char __bss_end__;

caddr_t _sbrk(int incr)
{
    static char *heap_end;
    char *prev_heap_end;
    register char *stack_ptr __asm("sp");

    if (heap_end == 0)
    {
        heap_end = &__bss_end__;
    }

    prev_heap_end = heap_end;
    if ((heap_end + incr) >= (stack_ptr - 256))
    {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_end += incr;
    return (caddr_t)prev_heap_end;
}