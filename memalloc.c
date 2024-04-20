#include <stdlib.h>
#include <pthread.h>
#include <string.h>
typedef char ALIGN[16];

union header
{
    struct
    {
        size_t size;
        unsigned is_free;
        struct header_t *next;
    } s;
    ALIGN stub; // to align 16 byte
};

typedef union header header_t;

pthread_mutex_t global_malloc_lock; // thread lock in memory
header_t *head, *tail;

header_t *get_free_block(size_t size)
{
    header_t *curr = head;
    while (curr)
    {
        if (curr->s.is_free && curr->s.size >= size)
        {
            return curr;
        }
        curr = curr->s.next;
    }
    return NULL;
}
void *malloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }
    size_t totalsize;
    void *block;
    header_t *header;
    pthread_mutex_lock(&global_malloc_lock);
    header = (header_t *)get_free_block(size);

    if (header != NULL)
    {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)header + 1;
    }

    totalsize = sizeof(header_t) + size;

    block = sbrk(totalsize);
    if (block == (void *)-1)
    {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header->s.is_free = 0;
    header->s.size = size;
    if (head == NULL)
    {
        head = header;
    }
    if (tail != NULL)
    {
        tail->s.next = header;
        header->s.next = NULL;
    }
    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)header + 1; // header+1 is the data, not the header
}

void free(void *block)
{
    header_t *header;
    void *programbreak;
    if (!block)
    {
        return;
    }
    pthread_mutex_lock(&global_malloc_lock);
    programbreak = sbrk(0);
    header = (header_t *)block - 1;

    if (header->s.size + (char *)block == programbreak)
    { // if is programmbreak
        if (head == tail)
        {
            head = tail = NULL;
        }
        else
        {
            header_t *tmp = head;
            while (tmp != NULL)
            {
                if (tmp->s.next == tail)
                {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
            sbrk(0 - sizeof(header_t) - header->s.size); // mallocate negative -> release memory
            pthread_mutex_unlock(&global_malloc_lock);
            return;
        }
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

void *calloc(size_t num, size_t nsize)
{ // allocates memory for an array of num elements of nsize bytes each and returns a pointer to the allocated memory

    void *block;
    if (num == 0 || nsize == 0)
    {
        return NULL;
    }

    size_t size = num * nsize;
    if (num != size / nsize)
    { // Check mul overflow
        return NULL;
    }
    block = malloc(size);
    if (block == NULL)
    {
        return NULL;
    }
    memset(block, 0, size);
    return block;
}

void *realloc(void *block, size_t size)
{
    if (block == NULL || size == 0)
    {
        return NULL;
    }
    header_t *header;
    header = (void *)block - 1;
    if (header->s.size >= size)
    {
        return block;
    }
    void *ret = malloc(size);
    if (ret != NULL)
    {
        memcpy(ret, block, header->s.size);
        free(block);
    }
    return ret;
}
