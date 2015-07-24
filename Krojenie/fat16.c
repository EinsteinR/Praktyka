
#include "partition.h"
#include "fat16.h"

#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>



#define FAT16_CLUSTER_FREE 0x0000
#define FAT16_CLUSTER_RESERVED_MIN 0xfff0
#define FAT16_CLUSTER_RESERVED_MAX 0xfff6
#define FAT16_CLUSTER_BAD 0xfff7
#define FAT16_CLUSTER_LAST_MIN 0xfff8
#define FAT16_CLUSTER_LAST_MAX 0xffff

#define FAT16_DIRENTRY_DELETED 0xe5
#define FAT16_DIRENTRY_LFNLAST (1 << 6)
#define FAT16_DIRENTRY_LFNSEQMASK ((1 << 6) - 1)

struct fat16_header_struct
{
    uint32_t size;
    uint32_t fat_offset;
    uint32_t fat_size;
    uint16_t sector_size;
    uint16_t cluster_size;
    uint32_t root_dir_offset;
    uint32_t cluster_zero_offset;
};

struct fat16_fs_struct
{
    struct partition_struct* partition;
    struct fat16_header_struct header;
};

struct fat16_file_struct
{
    struct fat16_fs_struct* fs;
    struct fat16_dir_entry_struct dir_entry;
    uint32_t pos;
    uint16_t pos_cluster;
};

struct fat16_dir_struct
{
    struct fat16_fs_struct* fs;
    struct fat16_dir_entry_struct dir_entry;
    uint16_t entry_next;
};

struct fat16_read_callback_arg
{
    uint16_t entry_cur;
    uint16_t entry_num;
    uint32_t entry_offset;
    uint8_t byte_count;
};

static uint8_t fat16_read_header(struct fat16_fs_struct* fs);
static uint8_t fat16_read_root_dir_entry(const struct fat16_fs_struct* fs, uint16_t entry_num, struct fat16_dir_entry_struct* dir_entry);
static uint8_t fat16_read_sub_dir_entry(const struct fat16_fs_struct* fs, uint16_t entry_num, const struct fat16_dir_entry_struct* parent, struct fat16_dir_entry_struct* dir_entry);
static uint8_t fat16_dir_entry_seek_callback(uint8_t* buffer, uint32_t offset, void* p);
static uint8_t fat16_dir_entry_read_callback(uint8_t* buffer, uint32_t offset, void* p);
static uint16_t fat16_get_next_cluster(const struct fat16_fs_struct* fs, uint16_t cluster_num);
static uint16_t fat16_append_clusters(const struct fat16_fs_struct* fs, uint16_t cluster_num, uint16_t count);
static uint8_t fat16_free_clusters(const struct fat16_fs_struct* fs, uint16_t cluster_num);
static uint8_t fat16_write_dir_entry(const struct fat16_fs_struct* fs, const struct fat16_dir_entry_struct* dir_entry);

struct fat16_fs_struct* fat16_open(struct partition_struct* partition)
{
    if(!partition ||
#if FAT16_WRITE_SUPPORT
       !partition->device_write
#else
       0
#endif
      )
        return 0;

    struct fat16_fs_struct* fs = malloc(sizeof(*fs));
    if(!fs)
        return 0;
    memset(fs, 0, sizeof(*fs));

    fs->partition = partition;
    if(!fat16_read_header(fs))
    {
        free(fs);
        return 0;
    }
    
    return fs;
}


void fat16_close(struct fat16_fs_struct* fs)
{
    if(!fs)
        return;

    free(fs);
}

uint8_t fat16_read_header(struct fat16_fs_struct* fs)
{
    uint8_t buffer[25];
    struct partition_struct* partition;
    struct fat16_header_struct* header;
    
    if(!fs || !fs->partition)
        return 0;

    partition = fs->partition;
    header = &fs->header;
    
    uint32_t partition_offset = partition->offset * 512;

    /* ensure we really have a FAT16 fs here */
    memset(buffer, 0, sizeof(buffer));

    if(!partition->device_read(partition_offset + 0x36, buffer, 8) ||
       !partition->device_read(partition_offset + 0x52, buffer + 9, 8))
        return 0;
    if(strcmp_P((char*) buffer, PSTR("FAT16   ")) &&
       strcmp_P((char*) buffer + 9, PSTR("FAT16   ")))
        return 0;

    partition->type = PARTITION_TYPE_FAT16;

    /* read fat parameters */
    if(!partition->device_read(partition_offset + 0x0b, buffer, sizeof(buffer)))
        return 0;

    memset(header, 0, sizeof(*header));
    
    uint16_t bytes_per_sector = ((uint16_t) buffer[0x00]) |
                                ((uint16_t) buffer[0x01] << 8);
    uint8_t sectors_per_cluster = buffer[0x02];
    uint16_t reserved_sectors = ((uint16_t) buffer[0x03]) |
                                ((uint16_t) buffer[0x04] << 8);
    uint8_t fat_copies = buffer[0x05];
    uint16_t max_root_entries = ((uint16_t) buffer[0x06]) |
                                ((uint16_t) buffer[0x07] << 8);
    uint16_t sectors_per_fat = ((uint16_t) buffer[0x0b]) |
                               ((uint16_t) buffer[0x0c] << 8);
    uint32_t sector_count = ((uint32_t) buffer[0x15]) |
                            ((uint32_t) buffer[0x16] << 8) |
                            ((uint32_t) buffer[0x17] << 16) |
                            ((uint32_t) buffer[0x18] << 24);
    
    header->size = sector_count * bytes_per_sector;

    header->fat_offset = /* jump to partition */
                         partition_offset +
                         /* jump to fat */
                         (uint32_t) reserved_sectors * bytes_per_sector;
    header->fat_size = (uint32_t) sectors_per_fat * bytes_per_sector;

    header->sector_size = bytes_per_sector;
    header->cluster_size = (uint32_t) bytes_per_sector * sectors_per_cluster;

    header->root_dir_offset = /* jump to fats */
                              header->fat_offset +
                              /* jump to root directory entries */
                              (uint32_t) fat_copies * sectors_per_fat * bytes_per_sector;

    header->cluster_zero_offset = /* jump to root directory entries */
                                  header->root_dir_offset +
                                  /* skip root directory entries */
                                  (uint32_t) max_root_entries * 32;

    return 1;
}

/**
 * \ingroup fat16_fs
 * Reads a directory entry of the root directory.
 *
 * \param[in] fs Descriptor of file system to use.
 * \param[in] entry_num The index of the directory entry to read.
 * \param[out] dir_entry Directory entry descriptor which will get filled.
 * \returns 0 on failure, 1 on success
 * \see fat16_read_sub_dir_entry, fat16_read_dir_entry_by_path
 */
uint8_t fat16_read_root_dir_entry(const struct fat16_fs_struct* fs, uint16_t entry_num, struct fat16_dir_entry_struct* dir_entry)
{
    if(!fs || !dir_entry)
        return 0;

    /* we read from the root directory entry */
    const struct fat16_header_struct* header = &fs->header;
    device_read_interval_t device_read_interval = fs->partition->device_read_interval;
    uint8_t buffer[32];

    /* seek to the n-th entry */
    struct fat16_read_callback_arg arg;
    memset(&arg, 0, sizeof(arg));
    arg.entry_num = entry_num;
    if(!device_read_interval(header->root_dir_offset,
                             buffer,
                             sizeof(buffer),
                             header->cluster_zero_offset - header->root_dir_offset,
                             fat16_dir_entry_seek_callback,
                             &arg) ||
       arg.entry_offset == 0
      )
        return 0;

    /* read entry */
    memset(dir_entry, 0, sizeof(*dir_entry));
    if(!device_read_interval(arg.entry_offset,
                             buffer,
                             sizeof(buffer),
                             arg.byte_count,
                             fat16_dir_entry_read_callback,
                             dir_entry))
        return 0;

    return dir_entry->long_name[0] != '\0' ? 1 : 0;
}

/**
 * \ingroup fat16_fs
 * Reads a directory entry of a given parent directory.
 *
 * \param[in] fs Descriptor of file system to use.
 * \param[in] entry_num The index of the directory entry to read.
 * \param[in] parent Directory entry descriptor in which to read directory entry.
 * \param[out] dir_entry Directory entry descriptor which will get filled.
 * \returns 0 on failure, 1 on success
 * \see fat16_read_root_dir_entry, fat16_read_dir_entry_by_path
 */
uint8_t fat16_read_sub_dir_entry(const struct fat16_fs_struct* fs, uint16_t entry_num, const struct fat16_dir_entry_struct* parent, struct fat16_dir_entry_struct* dir_entry)
{
    if(!fs || !parent || !dir_entry)
        return 0;

    /* we are in a parent directory and want to search within its directory entry table */
    if(!(parent->attributes & FAT16_ATTRIB_DIR))
        return 0;

    /* loop through all clusters of the directory */
    uint8_t buffer[32];
    uint32_t cluster_offset;
    uint16_t cluster_size = fs->header.cluster_size;
    uint16_t cluster_num = parent->cluster;
    struct fat16_read_callback_arg arg;

    while(1)
    {
        /* calculate new cluster offset */
        cluster_offset = fs->header.cluster_zero_offset + (uint32_t) (cluster_num - 2) * cluster_size;

        /* seek to the n-th entry */
        memset(&arg, 0, sizeof(arg));
        arg.entry_num = entry_num;
        if(!fs->partition->device_read_interval(cluster_offset,
                                                buffer,
                                                sizeof(buffer),
                                                cluster_size,
                                                fat16_dir_entry_seek_callback,
                                                &arg)
          )
            return 0;

        /* check if we found the entry */
        if(arg.entry_offset)
            break;

        /* get number of next cluster */
        if(!(cluster_num = fat16_get_next_cluster(fs, cluster_num)))
            return 0; /* directory entry not found */
    }

    memset(dir_entry, 0, sizeof(*dir_entry));

    /* read entry */
    if(!fs->partition->device_read_interval(arg.entry_offset,
                                            buffer,
                                            sizeof(buffer),
                                            arg.byte_count,
                                            fat16_dir_entry_read_callback,
                                            dir_entry))
        return 0;

    return dir_entry->long_name[0] != '\0' ? 1 : 0;
}

/**
 * \ingroup fat16_fs
 * Callback function for seeking through subdirectory entries.
 */
uint8_t fat16_dir_entry_seek_callback(uint8_t* buffer, uint32_t offset, void* p)
{
    struct fat16_read_callback_arg* arg = p;

    /* skip deleted or empty entries */
    if(buffer[0] == FAT16_DIRENTRY_DELETED || !buffer[0])
        return 1;

    if(arg->entry_cur == arg->entry_num)
    {
        arg->entry_offset = offset;
        arg->byte_count = buffer[11] == 0x0f ?
                          ((buffer[0] & FAT16_DIRENTRY_LFNSEQMASK) + 1) * 32 :
                          32;
        return 0;
    }

    /* if we read a 8.3 entry, we reached a new directory entry */
    if(buffer[11] != 0x0f)
        ++arg->entry_cur;

    return 1;
}

/**
 * \ingroup fat16_fs
 * Callback function for reading a directory entry.
 */
uint8_t fat16_dir_entry_read_callback(uint8_t* buffer, uint32_t offset, void* p)
{
    struct fat16_dir_entry_struct* dir_entry = p;

    /* there should not be any deleted or empty entries */
    if(buffer[0] == FAT16_DIRENTRY_DELETED || !buffer[0])
        return 0;

    if(!dir_entry->entry_offset)
        dir_entry->entry_offset = offset;
    

    return 0;
}

/**
 * \ingroup fat16_file
 * Retrieves the directory entry of a path.
 *
 * The given path may both describe a file or a directory.
 *
 * \param[in] fs The FAT16 filesystem on which to search.
 * \param[in] path The path of which to read the directory entry.
 * \param[out] dir_entry The directory entry to fill.
 * \returns 0 on failure, 1 on success.
 * \see fat16_read_dir
 */
uint8_t fat16_get_dir_entry_of_path(struct fat16_fs_struct* fs, const char* path, struct fat16_dir_entry_struct* dir_entry)
{
    if(!fs || !path || path[0] == '\0' || !dir_entry)
        return 0;

    if(path[0] == '/')
        ++path;

    /* begin with the root directory */
    memset(dir_entry, 0, sizeof(*dir_entry));
    dir_entry->attributes = FAT16_ATTRIB_DIR;

    if(path[0] == '\0')
        return 1;

    while(1)
    {
        struct fat16_dir_struct* dd = fat16_open_dir(fs, dir_entry);
        if(!dd)
            break;

        /* extract the next hierarchy we will search for */
        const char* sep_pos = strchr(path, '/');
        if(!sep_pos)
            sep_pos = path + strlen(path);
        uint8_t length_to_sep = sep_pos - path;
        
        /* read directory entries */
        while(fat16_read_dir(dd, dir_entry))
        {
            /* check if we have found the next hierarchy */
            if((strlen(dir_entry->long_name) != length_to_sep ||
                strncmp(path, dir_entry->long_name, length_to_sep) != 0))
                continue;

            fat16_close_dir(dd);
            dd = 0;

            if(path[length_to_sep] == '\0')
                /* we iterated through the whole path and have found the file */
                return 1;

            if(dir_entry->attributes & FAT16_ATTRIB_DIR)
            {
                /* we found a parent directory of the file we are searching for */
                path = sep_pos + 1;
                break;
            }

            /* a parent of the file exists, but not the file itself */
            return 0;
        }

        fat16_close_dir(dd);
    }
    
    return 0;
}

/**
 * \ingroup fat16_fs
 * Retrieves the next following cluster of a given cluster.
 *
 * Using the filesystem file allocation table, this function returns
 * the number of the cluster containing the data directly following
 * the data within the cluster with the given number.
 *
 * \param[in] fs The filesystem for which to determine the next cluster.
 * \param[in] cluster_num The number of the cluster for which to determine its successor.
 * \returns The wanted cluster number, or 0 on error.
 */
uint16_t fat16_get_next_cluster(const struct fat16_fs_struct* fs, uint16_t cluster_num)
{
    if(!fs || cluster_num < 2)
        return 0;

    /* read appropriate fat entry */
    uint8_t fat_entry[2];
    if(!fs->partition->device_read(fs->header.fat_offset + 2 * cluster_num, fat_entry, 2))
        return 0;

    /* determine next cluster from fat */
    cluster_num = ((uint16_t) fat_entry[0]) |
                  ((uint16_t) fat_entry[1] << 8);
    
    if(cluster_num == FAT16_CLUSTER_FREE ||
       cluster_num == FAT16_CLUSTER_BAD ||
       (cluster_num >= FAT16_CLUSTER_RESERVED_MIN && cluster_num <= FAT16_CLUSTER_RESERVED_MAX) ||
       (cluster_num >= FAT16_CLUSTER_LAST_MIN && cluster_num <= FAT16_CLUSTER_LAST_MAX))
        return 0;
    
    return cluster_num;
}

/**
 * \ingroup fat16_fs
 * Appends a new cluster chain to an existing one.
 *
 * Set cluster_num to zero to create a completely new one.
 *
 * \param[in] fs The file system on which to operate.
 * \param[in] cluster_num The cluster to which to append the new chain.
 * \param[in] count The number of clusters to allocate.
 * \returns 0 on failure, the number of the first new cluster on success.
 */
uint16_t fat16_append_clusters(const struct fat16_fs_struct* fs, uint16_t cluster_num, uint16_t count)
{
#if FAT16_WRITE_SUPPORT
    if(!fs)
        return 0;

    device_read_t device_read = fs->partition->device_read;
    device_write_t device_write = fs->partition->device_write;
    uint32_t fat_offset = fs->header.fat_offset;
    uint16_t cluster_max = fs->header.fat_size / 2;
    uint16_t cluster_next = 0;
    uint16_t count_left = count;
    uint8_t buffer[2];

    for(uint16_t cluster_new = 0; cluster_new < cluster_max; ++cluster_new)
    {
        if(!device_read(fat_offset + 2 * cluster_new, buffer, sizeof(buffer)))
            return 0;

        /* check if this is a free cluster */
        if(buffer[0] == (FAT16_CLUSTER_FREE & 0xff) &&
           buffer[1] == ((FAT16_CLUSTER_FREE >> 8) & 0xff))
        {
            /* allocate cluster */
            if(count_left == count)
            {
                buffer[0] = FAT16_CLUSTER_LAST_MAX & 0xff;
                buffer[1] = (FAT16_CLUSTER_LAST_MAX >> 8) & 0xff;
            }
            else
            {
                buffer[0] = cluster_next & 0xff;
                buffer[1] = (cluster_next >> 8) & 0xff;
            }

            if(!device_write(fat_offset + 2 * cluster_new, buffer, sizeof(buffer)))
                return 0;

            cluster_next = cluster_new;
            if(--count_left == 0)
                break;
        }
    }

    do
    {
        if(count_left > 0)
            break;

        /* We allocated a new cluster chain. Now join
         * it with the existing one.
         */
        if(cluster_num >= 2)
        {
            buffer[0] = cluster_next & 0xff;
            buffer[1] = (cluster_next >> 8) & 0xff;
            if(!device_write(fat_offset + 2 * cluster_num, buffer, sizeof(buffer)))
                break;
        }

        return cluster_next;

    } while(0);

    /* No space left on device or writing error.
     * Free up all clusters already allocated.
     */
    fat16_free_clusters(fs, cluster_next);

    return 0;
#else
    return 0;
#endif
}

/**
 * \ingroup fat16_fs
 * Frees a cluster chain, or a part thereof.
 *
 * Marks the specified cluster and all clusters which are sequentially
 * referenced by it as free. They may then be used again for future
 * file allocations.
 *
 * \note If this function is used for freeing just a part of a cluster
 *       chain, the new end of the chain is not correctly terminated
 *       within the FAT. This has to be done manually afterwards.
 *
 * \param[in] fs The filesystem on which to operate.
 * \param[in] cluster_num The starting cluster of the chain which to free.
 * \returns 0 on failure, 1 on success.
 */
uint8_t fat16_free_clusters(const struct fat16_fs_struct* fs, uint16_t cluster_num)
{
#if FAT16_WRITE_SUPPORT
    if(!fs || cluster_num < 2)
        return 0;

    uint32_t fat_offset = fs->header.fat_offset;
    uint8_t buffer[2];
    while(cluster_num)
    {
        if(!fs->partition->device_read(fat_offset + 2 * cluster_num, buffer, 2))
            return 0;

        /* get next cluster of current cluster before freeing current cluster */
        uint16_t cluster_num_next = ((uint16_t) buffer[0]) |
                                    ((uint16_t) buffer[1] << 8);

        if(cluster_num_next == FAT16_CLUSTER_FREE)
            return 1;
        if(cluster_num_next == FAT16_CLUSTER_BAD ||
           (cluster_num_next >= FAT16_CLUSTER_RESERVED_MIN &&
            cluster_num_next <= FAT16_CLUSTER_RESERVED_MAX
           )
          )
            return 0;
        if(cluster_num_next >= FAT16_CLUSTER_LAST_MIN && cluster_num_next <= FAT16_CLUSTER_LAST_MAX)
            cluster_num_next = 0;

        /* free cluster */
        buffer[0] = FAT16_CLUSTER_FREE & 0xff;
        buffer[1] = (FAT16_CLUSTER_FREE >> 8) & 0xff;
        if(!fs->partition->device_write(fat_offset + 2 * cluster_num, buffer, 2))
            return 0;

        cluster_num = cluster_num_next;
    }

    return 1;
#else
    return 0;
#endif
}

/**
 * \ingroup fat16_file
 * Opens a file on a FAT16 filesystem.
 *
 * \param[in] fs The filesystem on which the file to open lies.
 * \param[in] dir_entry The directory entry of the file to open.
 * \returns The file handle, or 0 on failure.
 * \see fat16_close_file
 */
struct fat16_file_struct* fat16_open_file(struct fat16_fs_struct* fs, const struct fat16_dir_entry_struct* dir_entry)
{
    if(!fs || !dir_entry || (dir_entry->attributes & FAT16_ATTRIB_DIR))
        return 0;

    struct fat16_file_struct* fd = malloc(sizeof(*fd));
    if(!fd)
        return 0;
    
    memcpy(&fd->dir_entry, dir_entry, sizeof(*dir_entry));
    fd->fs = fs;
    fd->pos = 0;
    fd->pos_cluster = dir_entry->cluster;

    return fd;
}

/**
 * \ingroup fat16_file
 * Closes a file.
 *
 * \param[in] fd The file handle of the file to close.
 * \see fat16_open_file
 */
void fat16_close_file(struct fat16_file_struct* fd)
{
    if(fd)
        free(fd);
}

/**
 * \ingroup fat16_file
 * Reads data from a file.
 * 
 * The data requested is read from the current file location.
 *
 * \param[in] fd The file handle of the file from which to read.
 * \param[out] buffer The buffer into which to write.
 * \param[in] buffer_len The amount of data to read.
 * \returns The number of bytes read, 0 on end of file, or -1 on failure.
 * \see fat16_write_file
 */
int16_t fat16_read_file(struct fat16_file_struct* fd, uint8_t* buffer, uint16_t buffer_len)
{
    /* check arguments */
    if(!fd || !buffer || buffer_len < 1)
        return -1;

    /* determine number of bytes to read */
    if(fd->pos + buffer_len > fd->dir_entry.file_size)
        buffer_len = fd->dir_entry.file_size - fd->pos;
    if(buffer_len == 0)
        return 0;
    
    uint16_t cluster_size = fd->fs->header.cluster_size;
    uint16_t cluster_num = fd->pos_cluster;
    uint16_t buffer_left = buffer_len;
    uint16_t first_cluster_offset = fd->pos % cluster_size;

    /* find cluster in which to start reading */
    if(!cluster_num)
    {
        cluster_num = fd->dir_entry.cluster;
        
        if(!cluster_num)
        {
            if(!fd->pos)
                return 0;
            else
                return -1;
        }

        if(fd->pos)
        {
            uint32_t pos = fd->pos;
            while(pos >= cluster_size)
            {
                pos -= cluster_size;
                cluster_num = fat16_get_next_cluster(fd->fs, cluster_num);
                if(!cluster_num)
                    return -1;
            }
        }
    }
    
    /* read data */
    do
    {
        /* calculate data size to copy from cluster */
        uint32_t cluster_offset = fd->fs->header.cluster_zero_offset +
                                  (uint32_t) (cluster_num - 2) * cluster_size + first_cluster_offset;
        uint16_t copy_length = cluster_size - first_cluster_offset;
        if(copy_length > buffer_left)
            copy_length = buffer_left;

        /* read data */
        if(!fd->fs->partition->device_read(cluster_offset, buffer, copy_length))
            return buffer_len - buffer_left;

        /* calculate new file position */
        buffer += copy_length;
        buffer_left -= copy_length;
        fd->pos += copy_length;

        if(first_cluster_offset + copy_length >= cluster_size)
        {
            /* we are on a cluster boundary, so get the next cluster */
            if((cluster_num = fat16_get_next_cluster(fd->fs, cluster_num)))
            {
                first_cluster_offset = 0;
            }
            else
            {
                fd->pos_cluster = 0;
                return buffer_len - buffer_left;
            }
        }

        fd->pos_cluster = cluster_num;

    } while(buffer_left > 0); /* check if we are done */

    return buffer_len;
}

/**
 * \ingroup fat16_file
 * Writes data to a file.
 * 
 * The data is written to the current file location.
 *
 * \param[in] fd The file handle of the file to which to write.
 * \param[in] buffer The buffer from which to read the data to be written.
 * \param[in] buffer_len The amount of data to write.
 * \returns The number of bytes written, 0 on disk full, or -1 on failure.
 * \see fat16_read_file
 */
int16_t fat16_write_file(struct fat16_file_struct* fd, const uint8_t* buffer, uint16_t buffer_len)
{
#if FAT16_WRITE_SUPPORT
    /* check arguments */
    if(!fd || !buffer || buffer_len < 1)
        return -1;
    if(fd->pos > fd->dir_entry.file_size)
        return -1;

    uint16_t cluster_size = fd->fs->header.cluster_size;
    uint16_t cluster_num = fd->pos_cluster;
    uint16_t buffer_left = buffer_len;
    uint16_t first_cluster_offset = fd->pos % cluster_size;

    /* find cluster in which to start writing */
    if(!cluster_num)
    {
        cluster_num = fd->dir_entry.cluster;
        
        if(!cluster_num)
        {
            if(!fd->pos)
            {
                /* empty file */
                fd->dir_entry.cluster = cluster_num = fat16_append_clusters(fd->fs, 0, 1);
                if(!cluster_num)
                    return -1;
            }
            else
            {
                return -1;
            }
        }

        if(fd->pos)
        {
            uint32_t pos = fd->pos;
            uint16_t cluster_num_next;
            while(pos >= cluster_size)
            {
                pos -= cluster_size;
                cluster_num_next = fat16_get_next_cluster(fd->fs, cluster_num);
                if(!cluster_num_next && pos == 0)
                    /* the file exactly ends on a cluster boundary, and we append to it */
                    cluster_num_next = fat16_append_clusters(fd->fs, cluster_num, 1);
                if(!cluster_num_next)
                    return -1;

                cluster_num = cluster_num_next;
            }
        }
    }
    
    /* write data */
    do
    {
        /* calculate data size to write to cluster */
        uint32_t cluster_offset = fd->fs->header.cluster_zero_offset +
                                  (uint32_t) (cluster_num - 2) * cluster_size + first_cluster_offset;
        uint16_t write_length = cluster_size - first_cluster_offset;
        if(write_length > buffer_left)
            write_length = buffer_left;

        /* write data which fits into the current cluster */
        if(!fd->fs->partition->device_write(cluster_offset, buffer, write_length))
            break;

        /* calculate new file position */
        buffer += write_length;
        buffer_left -= write_length;
        fd->pos += write_length;

        if(first_cluster_offset + write_length >= cluster_size)
        {
            /* we are on a cluster boundary, so get the next cluster */
            uint16_t cluster_num_next = fat16_get_next_cluster(fd->fs, cluster_num);
            if(!cluster_num_next && buffer_left > 0)
                /* we reached the last cluster, append a new one */
                cluster_num_next = fat16_append_clusters(fd->fs, cluster_num, 1);
            if(!cluster_num_next)
            {
                fd->pos_cluster = 0;
                break;
            }

            cluster_num = cluster_num_next;
            first_cluster_offset = 0;
        }

        fd->pos_cluster = cluster_num;

    } while(buffer_left > 0); /* check if we are done */

    /* update directory entry */
    if(fd->pos > fd->dir_entry.file_size)
    {
        uint32_t size_old = fd->dir_entry.file_size;

        /* update file size */
        fd->dir_entry.file_size = fd->pos;
        /* write directory entry */
        if(!fat16_write_dir_entry(fd->fs, &fd->dir_entry))
        {
            /* We do not return an error here since we actually wrote
             * some data to disk. So we calculate the amount of data
             * we wrote to disk and which lies within the old file size.
             */
            buffer_left = fd->pos - size_old;
            fd->pos = size_old;
        }
    }

    return buffer_len - buffer_left;

#else
    return -1;
#endif
}

/**
 * \ingroup fat16_file
 * Repositions the read/write file offset.
 *
 * Changes the file offset where the next call to fat16_read_file()
 * or fat16_write_file() starts reading/writing.
 *
 * If the new offset is beyond the end of the file, fat16_resize_file()
 * is implicitly called, i.e. the file is expanded.
 *
 * The new offset can be given in different ways determined by
 * the \c whence parameter:
 * - \b FAT16_SEEK_SET: \c *offset is relative to the beginning of the file.
 * - \b FAT16_SEEK_CUR: \c *offset is relative to the current file position.
 * - \b FAT16_SEEK_END: \c *offset is relative to the end of the file.
 *
 * The resulting absolute offset is written to the location the \c offset
 * parameter points to.
 * 
 * \param[in] fd The file decriptor of the file on which to seek.
 * \param[in,out] offset A pointer to the new offset, as affected by the \c whence
 *                   parameter. The function writes the new absolute offset
 *                   to this location before it returns.
 * \param[in] whence Affects the way \c offset is interpreted, see above.
 * \returns 0 on failure, 1 on success.
 */
uint8_t fat16_seek_file(struct fat16_file_struct* fd, int32_t* offset, uint8_t whence)
{
    if(!fd || !offset)
        return 0;

    uint32_t new_pos = fd->pos;
    switch(whence)
    {
        case FAT16_SEEK_SET:
            new_pos = *offset;
            break;
        case FAT16_SEEK_CUR:
            new_pos += *offset;
            break;
        case FAT16_SEEK_END:
            new_pos = fd->dir_entry.file_size + *offset;
            break;
        default:
            return 0;
    }

    if(new_pos > fd->dir_entry.file_size && !fat16_resize_file(fd, new_pos))
        return 0;

    fd->pos = new_pos;
    fd->pos_cluster = 0;

    *offset = new_pos;
    return 1;
}

/**
 * \ingroup fat16_file
 * Resizes a file to have a specific size.
 *
 * Enlarges or shrinks the file pointed to by the file descriptor to have
 * exactly the specified size.
 *
 * If the file is truncated, all bytes having an equal or larger offset
 * than the given size are lost. If the file is expanded, the additional
 * bytes are allocated.
 *
 * \note Please be aware that this function just allocates or deallocates disk
 * space, it does not explicitely clear it. To avoid data leakage, this
 * must be done manually.
 *
 * \param[in] fd The file decriptor of the file which to resize.
 * \param[in] size The new size of the file.
 * \returns 0 on failure, 1 on success.
 */
uint8_t fat16_resize_file(struct fat16_file_struct* fd, uint32_t size)
{
    if(!fd)
        return 0;

    uint16_t cluster_num = fd->dir_entry.cluster;
    uint16_t cluster_size = fd->fs->header.cluster_size;
    uint32_t size_new = size;

    do
    {
        if(cluster_num == 0 && size_new == 0)
            /* the file stays empty */
            break;

        /* seek to the next cluster as long as we need the space */
        while(size_new > 0)
        {
            /* get next cluster of file */
            uint16_t cluster_num_next = fat16_get_next_cluster(fd->fs, cluster_num);
            if(cluster_num_next)
            {
                cluster_num = cluster_num_next;

                if(size_new <= cluster_size)
                {
                    /* file will shrink; get the first cluster to deallocate */
                    cluster_num = fat16_get_next_cluster(fd->fs, cluster_num);
                    size_new = 0;
                    break;
                }

                size_new -= cluster_size;
            }
            else
            {
                break;
            }
        }

        if(size_new == 0)
        {
            /* free all clusters no longer needed */
            fat16_free_clusters(fd->fs, cluster_num);
        }
        else
        {
            /* Allocate new cluster chain and append
             * it to the existing one, if available.
             */
            uint16_t cluster_count = size_new / cluster_size;
            if((uint32_t) cluster_count * cluster_size < size_new)
                ++cluster_count;
            uint16_t cluster_new_chain = fat16_append_clusters(fd->fs, cluster_num, cluster_count);
            if(!cluster_new_chain)
                return 0;

            if(!cluster_num)
            {
                cluster_num = cluster_new_chain;
                fd->dir_entry.cluster = cluster_num;
            }
        }

    } while(0);

    /* correct file position */
    if(size < fd->pos)
    {
        fd->pos = size;
        fd->pos_cluster = 0;
    }

    /* write new directory entry */
    fd->dir_entry.file_size = size;
    if(size == 0)
        fd->dir_entry.cluster = 0;
    if(!fat16_write_dir_entry(fd->fs, &fd->dir_entry))
        return 0;

    return 1;
}

/**
 * \ingroup fat16_dir
 * Opens a directory.
 *
 * \param[in] fs The filesystem on which the directory to open resides.
 * \param[in] dir_entry The directory entry which stands for the directory to open.
 * \returns An opaque directory descriptor on success, 0 on failure.
 * \see fat16_close_dir
 */
struct fat16_dir_struct* fat16_open_dir(struct fat16_fs_struct* fs, const struct fat16_dir_entry_struct* dir_entry)
{
    if(!fs || !dir_entry || !(dir_entry->attributes & FAT16_ATTRIB_DIR))
        return 0;

    struct fat16_dir_struct* dd = malloc(sizeof(*dd));
    if(!dd)
        return 0;
    
    memcpy(&dd->dir_entry, dir_entry, sizeof(*dir_entry));
    dd->fs = fs;
    dd->entry_next = 0;

    return dd;
}

/**
 * \ingroup fat16_dir
 * Closes a directory descriptor.
 *
 * This function destroys a directory descriptor which was
 * previously obtained by calling fat16_open_dir(). When this
 * function returns, the given descriptor will be invalid.
 *
 * \param[in] dd The directory descriptor to close.
 * \see fat16_open_dir
 */
void fat16_close_dir(struct fat16_dir_struct* dd)
{
    if(dd)
        free(dd);
}

/**
 * \ingroup fat16_dir
 * Reads the next directory entry contained within a parent directory.
 *
 * \param[in] dd The descriptor of the parent directory from which to read the entry.
 * \param[out] dir_entry Pointer to a buffer into which to write the directory entry information.
 * \returns 0 on failure, 1 on success.
 * \see fat16_reset_dir
 */
uint8_t fat16_read_dir(struct fat16_dir_struct* dd, struct fat16_dir_entry_struct* dir_entry)
{
    if(!dd || !dir_entry)
        return 0;

    if(dd->dir_entry.cluster == 0)
    {
        /* read entry from root directory */
        if(fat16_read_root_dir_entry(dd->fs, dd->entry_next, dir_entry))
        {
            ++dd->entry_next;
            return 1;
        }
    }
    else
    {
        /* read entry from a subdirectory */
        if(fat16_read_sub_dir_entry(dd->fs, dd->entry_next, &dd->dir_entry, dir_entry))
        {
            ++dd->entry_next;
            return 1;
        }
    }

    /* restart reading */
    dd->entry_next = 0;

    return 0;
}

/**
 * \ingroup fat16_dir
 * Resets a directory handle.
 *
 * Resets the directory handle such that reading restarts
 * with the first directory entry.
 *
 * \param[in] dd The directory handle to reset.
 * \returns 0 on failure, 1 on success.
 * \see fat16_read_dir
 */
uint8_t fat16_reset_dir(struct fat16_dir_struct* dd)
{
    if(!dd)
        return 0;

    dd->entry_next = 0;
    return 1;
}

/**
 * \ingroup fat16_fs
 * Writes a directory entry to disk.
 *
 * \note The file name is not checked for invalid characters.
 *
 * \note The generation of the short 8.3 file name is quite
 * simple. The first eight characters are used for the filename.
 * The extension, if any, is made up of the first three characters
 * following the last dot within the long filename. If the
 * filename (without the extension) is longer than eight characters,
 * the lower byte of the cluster number replaces the last two
 * characters to avoid name clashes. In any other case, it is your
 * responsibility to avoid name clashes.
 *
 * \param[in] fs The filesystem on which to operate.
 * \param[in] dir_entry The directory entry to write.
 * \returns 0 on failure, 1 on success.
 */
uint8_t fat16_write_dir_entry(const struct fat16_fs_struct* fs, const struct fat16_dir_entry_struct* dir_entry)
{
#if FAT16_WRITE_SUPPORT
    if(!fs || !dir_entry)
        return 0;
    
    device_write_t device_write = fs->partition->device_write;
    uint32_t offset = dir_entry->entry_offset;
    uint8_t name_len = strlen(dir_entry->long_name);
    uint8_t lfn_entry_count = (name_len + 12) / 13;
    uint8_t buffer[32];

    /* write 8.3 entry */

    /* generate 8.3 file name */
    memset(&buffer[0], ' ', 11);
    char* name_ext = strrchr(dir_entry->long_name, '.');
    if(name_ext)
    {
        ++name_ext;
        
        uint8_t name_ext_len = strlen(name_ext);
        name_len -= name_ext_len + 1;

        if(name_ext_len > 3)
            name_ext_len = 3;
        
        memcpy(&buffer[8], name_ext, name_ext_len);
    }
    
    if(name_len <= 8)
    {
        memcpy(buffer, dir_entry->long_name, name_len);
    }
    else
    {
        memcpy(buffer, dir_entry->long_name, 8);

        /* Minimize 8.3 name clashes by appending
         * the lower byte of the cluster number.
         */
        uint8_t num = dir_entry->cluster & 0xff;

        buffer[6] = (num < 0xa0) ? ('0' + (num >> 4)) : ('a' + (num >> 4));
        num &= 0x0f;
        buffer[7] = (num < 0x0a) ? ('0' + num) : ('a' + num);
    }

    /* fill directory entry buffer */
    memset(&buffer[11], 0, sizeof(buffer) - 11);
    buffer[0x0b] = dir_entry->attributes;
    buffer[0x1a] = (dir_entry->cluster >> 0) & 0xff;
    buffer[0x1b] = (dir_entry->cluster >> 8) & 0xff;
    buffer[0x1c] = (dir_entry->file_size >> 0) & 0xff;
    buffer[0x1d] = (dir_entry->file_size >> 8) & 0xff;
    buffer[0x1e] = (dir_entry->file_size >> 16) & 0xff;
    buffer[0x1f] = (dir_entry->file_size >> 24) & 0xff;

    /* write to disk */
    if(!device_write(offset + (uint32_t) lfn_entry_count * 32, buffer, sizeof(buffer)))
        return 0;
    
    /* calculate checksum of 8.3 name */
    uint8_t checksum = buffer[0];
    for(uint8_t i = 1; i < 11; ++i)
        checksum = ((checksum >> 1) | (checksum << 7)) + buffer[i];
    
    /* write lfn entries */
    for(uint8_t lfn_entry = lfn_entry_count; lfn_entry > 0; --lfn_entry)
    {
        memset(buffer, 0, sizeof(buffer));
        memset(&buffer[0x01], 0xff, 10);
        memset(&buffer[0x0e], 0xff, 12);
        memset(&buffer[0x1c], 0xff, 4);
        
        buffer[0x00] = lfn_entry;
        if(lfn_entry == lfn_entry_count)
            buffer[0x00] |= FAT16_DIRENTRY_LFNLAST;

        /* set file name */
        const char* long_name_curr = dir_entry->long_name + (lfn_entry - 1) * 13;
        uint8_t i = 1;
        while(i < 0x1f)
        {
            buffer[i++] = *long_name_curr;
            buffer[i++] = 0;

            switch(i)
            {
                case 0x0b:
                    i = 0x0e;
                    break;
                case 0x1a:
                    i = 0x1c;
                    break;
            }

            if(!*long_name_curr++)
                break;
        }
        
        /* mark as lfn entry */
        buffer[0x0b] = 0x0f;

        /* set checksum */
        buffer[0x0d] = checksum;

        /* write entry */
        device_write(offset, buffer, sizeof(buffer));
    
        offset += sizeof(buffer);
    }
    
    return 1;

#else
    return 0;
#endif
}

/**
 * \ingroup fat16_file
 * Creates a file.
 *
 * Creates a file and obtains the directory entry of the
 * new file. If the file to create already exists, the
 * directory entry of the existing file will be returned
 * within the dir_entry parameter.
 *
 * \note The file name is not checked for invalid characters.
 *
 * \note The generation of the short 8.3 file name is quite
 * simple. The first eight characters are used for the filename.
 * The extension, if any, is made up of the first three characters
 * following the last dot within the long filename. If the
 * filename (without the extension) is longer than eight characters,
 * the lower byte of the cluster number replaces the last two
 * characters to avoid name clashes. In any other case, it is your
 * responsibility to avoid name clashes.
 *
 * \param[in] parent The handle of the directory in which to create the file.
 * \param[in] file The name of the file to create.
 * \param[out] dir_entry The directory entry to fill for the new file.
 * \returns 0 on failure, 1 on success.
 * \see fat16_delete_file
 */
uint8_t fat16_create_file(struct fat16_dir_struct* parent, const char* file, struct fat16_dir_entry_struct* dir_entry)
{
#if FAT16_WRITE_SUPPORT
    if(!parent || !file || !file[0])
        return 0;

    /* check if the file already exists */
    while(1)
    {
        if(!fat16_read_dir(parent, dir_entry))
            break;

        if(strcmp(file, dir_entry->long_name) == 0)
        {
            fat16_reset_dir(parent);
            return 1;
        }
    }

    memset(dir_entry, 0, sizeof(*dir_entry));
    strncpy(dir_entry->long_name, file, sizeof(dir_entry->long_name) - 1);

    /* search for a place where to write the directory entry to disk */
    uint8_t free_dir_entries_needed = strlen(file) / 13 + 1 + 1;
    uint8_t free_dir_entries_found = 0;
    struct fat16_fs_struct* fs = parent->fs;
    uint16_t cluster_num = parent->dir_entry.cluster;
    uint32_t dir_entry_offset = 0;
    uint32_t offset = 0;
    uint32_t offset_to = 0;

    if(cluster_num == 0)
    {
        /* we read/write from the root directory entry */
        offset = fs->header.root_dir_offset;
        offset_to = fs->header.cluster_zero_offset;
        dir_entry_offset = offset;
    }
    
    while(1)
    {
        if(offset == offset_to)
        {
            if(cluster_num == 0)
                /* We iterated through the whole root directory entry
                 * and could not find enough space for the directory entry.
                 */
                return 0;

            if(offset)
            {
                /* We reached a cluster boundary and have to
                 * switch to the next cluster.
                 */

                uint16_t cluster_next = fat16_get_next_cluster(fs, cluster_num);
                if(!cluster_next)
                {
                    cluster_next = fat16_append_clusters(fs, cluster_num, 1);
                    if(!cluster_next)
                        return 0;

                    /* we appended a new cluster and know it is free */
                    dir_entry_offset = fs->header.cluster_zero_offset +
                                       (uint32_t) (cluster_next - 2) * fs->header.cluster_size;

                    /* TODO: This cluster has to be zeroed in an efficient way, or at least
                     *       every 32th byte should be set to FAT16_DIRENTRY_DELETED.
                     */
                    break;
                }
                cluster_num = cluster_next;
            }

            offset = fs->header.cluster_zero_offset +
                     (uint32_t) (cluster_num - 2) * fs->header.cluster_size;
            offset_to = offset + fs->header.cluster_size;
            dir_entry_offset = offset;
            free_dir_entries_found = 0;
        }
        
        /* read next lfn or 8.3 entry */
        uint8_t first_char;
        if(!fs->partition->device_read(offset, &first_char, sizeof(first_char)))
            return 0;

        /* check if we found a free directory entry */
        if(first_char == FAT16_DIRENTRY_DELETED || !first_char)
        {
            /* check if we have the needed number of available entries */
            ++free_dir_entries_found;
            if(free_dir_entries_found >= free_dir_entries_needed)
                break;

            offset += 32;
        }
        else
        {
            offset += 32;
            dir_entry_offset = offset;
            free_dir_entries_found = 0;
        }
    }
    
    /* write directory entry to disk */
    dir_entry->entry_offset = dir_entry_offset;
    if(!fat16_write_dir_entry(fs, dir_entry))
        return 0;
    
    return 1;
    
#else
    return 0;
#endif
}
