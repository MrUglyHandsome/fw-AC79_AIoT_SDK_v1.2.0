/* Copyright (c) 2013, Philipp Tölke
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef INCLUDE_VFS_H
#define INCLUDE_VFS_H
#include "fs/fs.h"
#include "time.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_FILENAME_LEN 256

#define vfs_load_plugin(x)
#define bcopy(src, dest, len) memmove(dest, src, len)

#ifndef __time_t_defined
typedef struct {
    short date;
    short time;
} time_t;

struct tm {
    int tm_year;
    int tm_mon;
    int tm_mday;
    int tm_hour;
    int tm_min;
    int tm_sec;
};
#endif // __time_t_defined

typedef struct vfscan vfs_dir_t;
/*typedef FILE vfs_file_t;*/
typedef void vfs_file_t;
typedef vfs_file_t vfs_t;
typedef struct {
    long st_size;
    char st_mode;
    time_t st_mtime;
} vfs_stat_t;
typedef struct {
    char name[MAX_FILENAME_LEN];
    char *cwd_dir;
} vfs_dirent_t;

#define VFS_ISDIR(st_mode) (st_mode == F_ATTR_DIR)
#define VFS_ISREG(st_mode) (!(st_mode == F_ATTR_DIR))
#define VFS_IRWXU 0
#define VFS_IRWXG 0
#define VFS_IRWXO 0

/*#define FTP_ALLOC_DEBUG //alloc debug*/

#ifdef FTP_ALLOC_DEBUG
void *ftp_malloc(int size);
void ftp_free(void *buf);
#else
#define ftp_malloc 	malloc
#define ftp_free 	free
#endif

int vfs_read(void *buffer, int dummy, int len, vfs_file_t *file);
vfs_dirent_t *vfs_readdir(vfs_dir_t *dir, vfs_file_t **fs, int fnum);
int vfs_stat(vfs_file_t *vfs, const char *filename, vfs_stat_t *st);
void vfs_close(vfs_file_t *vfs, char err);
int vfs_write(void *buffer, int dummy, int len, vfs_file_t *file);
void *vfs_open(const char *filename, const char *mode);
char *vfs_getcwd(vfs_file_t *vfs, int dummy);
vfs_dir_t *vfs_opendir(const char *path);
void vfs_closedir(vfs_dir_t *dir);
int vfs_rename(char *from, const char *newname);
int vfs_mkdir(const char *path, int mode);
int vfs_rmdir(const char *path);
int vfs_remove(const char *name);
int vfs_chdir(const char *dir);
void vfs_chdir_close(const char *dir);
struct tm *gmtime(const time_t *time);
struct tm *gmtime_r(const time_t *timep, struct tm *result);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INCLUDE_VFS_H */
