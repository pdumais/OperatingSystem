#include "vfs.h"
#include "flatfs.h"

file_handle* fopen(char* name, uint64_t access_type)
{
    file_handle* f = (file_handle*)malloc(sizeof(file_handle));    

    f->operations->fopen = &flatfs_fopen;
    f->operations->fread = &flatfs_fread;
    f->operations->fwrite = &flatfs_fwrite;
    f->operations->fseek = &flatfs_fseek;
    f->operations->fclose = &flatfs_fclose;

    if (f->operations->fopen(f,name,access_type)) return f;

    free((void*)f);
    return 0;
}


uint64_t fread(file_handle* f, uint64_t count, char* destination)
{
    return f->operations->fread((system_handle*)f,count,destination);
}

uint64_t fwrite(file_handle* f, uint64_t count, char* destination)
{
    return f->operations->fwrite((system_handle*)f,count,destination);
}

void fclose(file_handle* f)
{
    f->operations->fclose((system_handle*)f);
}

void fseek(file_handle* f, uint64_t count, bool absolute)
{
    f->operations->fseek((system_handle*)f,count,absolute);
    free((void*)f);
}

