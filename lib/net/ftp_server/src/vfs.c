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

#include "vfs.h"
#include "ftpd.h"
#include "time.h"
#include "utils/character_coding.h"

/* dirent that will be given to callers;
 * note: both APIs assume that only one dirent ever exists
 */
static vfs_dirent_t dir_ent = {0};
extern FTPD_OTA *ftp_ota;

/*
 * start : system platform APIs
 */
/*
 * system platform OTA APIs
 */
__attribute__((weak)) void *net_fopen(const char *path, const char *mode)
{
    printf("err, please add net_update.c to project!!!!!!!!!!!!!!!!\n");
    return NULL;
}
__attribute__((weak)) int net_fwrite(void *fd, unsigned char *buf, int len, int end)
{
    return len;
}
__attribute__((weak)) int net_fclose(void *fd, char is_socket_err)
{
    return 0;
}
__attribute__((weak)) const char *get_root_path(void)
{
    printf("err , please add function : \"const char *get_root_path(void)\" to project!!!!!!!!!!!!!!!!");
    return NULL;
}
/*
 * system platform root path APIs
 */
static const char *vfs_root_path(void)
{
    return get_root_path();
}
/*
 * FTP client request root path to system root path APIs
 */
static const char *vfs_cwd2root_path(const char *path)
{
    if (path == NULL || path[0] == '\0' || !vfs_root_path()) {
        return dir_ent.cwd_dir ? dir_ent.cwd_dir : "/";
    }
    if (!dir_ent.cwd_dir) {
        dir_ent.cwd_dir = ftp_malloc(128);
        if (dir_ent.cwd_dir) {
            strcpy(dir_ent.cwd_dir, "/");
        }
    }
    if (path[0] == '/' || !strstr(path, vfs_root_path())) {
        if (strlen(path) > 1) {
            char *path_cpy = ftp_malloc(strlen(path) + 1);
            if (path_cpy) {
                strcpy(path_cpy, path[0] == '/' ? (path + 1) : path);
                if (!strcmp(path, dir_ent.cwd_dir)) {
                    strcpy(path, vfs_root_path());
                    strcat(path, dir_ent.cwd_dir + 1);
                } else {
                    strcpy(path, vfs_root_path());
                    if (dir_ent.cwd_dir && dir_ent.cwd_dir[1] != '\0') {
                        strcat(path, dir_ent.cwd_dir + 1);
                    }
                    strcat(path, path_cpy);
                }
                ftp_free(path_cpy);
            }
        } else {
            strcpy(path, vfs_root_path());
        }
    }
    /*printf("--->path = %s \n", path);*/
    return path;
}
/*
 * start : FTP file system APIs
 */
int vfs_read(void *buffer, int dummy, int len, vfs_file_t *file)
{
    return fread(buffer, 1, len, file);
}

vfs_dirent_t *vfs_readdir(vfs_dir_t *dir, vfs_file_t **fs, int fnum)
{
    vfs_file_t *pfs = (vfs_file_t *) * (int *)fs;
    /*printf("---> dir_num = %d, all_file_num = %d, send num = %d\n",*/
    /*dir->fileTotalInDir,//文件夹总数*/
    /*dir->file_number,//扫描的所有文件数（文件和文件夹）*/
    /*fnum);//已发送的文件数*/

    if (fnum >= dir->file_number) {
        *fs = NULL;
        return NULL;
    }
    if (!pfs) {
        *fs = fselect(dir, FSEL_FIRST_FILE, 0);
    } else {
        *fs = fselect(dir, FSEL_NEXT_FILE, 0);//FSEL_NEXT_FOLDER_FILE
    }
    if (*fs) {
#ifdef CONFIG_JLFAT_ENABLE
        int fname_len = fget_name(*fs, (u8 *)dir_ent.name, sizeof(dir_ent.name));
        if (fname_len > 2 && dir_ent.name[0] == '\\' && dir_ent.name[1] == 'U') {
            fname_len = unicode_2_utf8((u8 *)dir_ent.name, sizeof(dir_ent.name), (u8 *)&dir_ent.name[2], fname_len - 2);
            dir_ent.name[fname_len] = 0;
        }
#else
        fget_name(*fs, dir_ent.name, sizeof(dir_ent.name));
#endif
        return &dir_ent;
    }
    return NULL;
}

int vfs_stat(vfs_file_t *vfs, const char *filename, vfs_stat_t *st)
{
    struct vfs_attr attr;
    st->st_size = 0;

    if (!vfs) {
        vfs_cwd2root_path(filename);
#ifdef CONFIG_JLFAT_ENABLE
        char e_filename[MAX_FILENAME_LEN];
        int fname_len = long_file_name_encode(filename, (u8 *)e_filename, sizeof(e_filename));
        e_filename[fname_len] = 0;
        filename = e_filename;
#endif
        vfs = fopen(filename, "r");
        if (vfs) {
            fget_attrs(vfs, &attr);
            fclose(vfs);
        } else {
            printf("err in %s ,maybe %s is a dir \n", __func__, filename);
        }
    } else {
        if (fget_attrs(vfs, &attr)) {
            printf("error in %s ,maybe %s is a dir \n", __func__, filename);
        }
    }
    if (!vfs) {
        return -1;
    }
    st->st_size = attr.fsize;
    st->st_mode = attr.attr;
    struct tm tm = {
        .tm_sec = attr.wrt_time.sec,
        .tm_min = attr.wrt_time.min,
        .tm_hour = attr.wrt_time.hour,
        .tm_mday = attr.wrt_time.day,
        .tm_mon = attr.wrt_time.month - 1,
        .tm_year = attr.wrt_time.year - 1900,
    };
    st->st_mtime = mktime(&tm);
    return 0;
}

void vfs_close(vfs_file_t *vfs, char err)
{
    if (vfs) {
        if (ftp_ota && ftp_ota->ota_status) {
            ftp_ota->ota_status = 0;
            printf("---> ota update colse \n");
            net_fclose(vfs, err);
        } else {
            fclose(vfs);
        }
    }
}
int vfs_write(void *buffer, int dummy, int len, vfs_file_t *file)
{
    if (ftp_ota && ftp_ota->ota_status) {
        ftp_ota->ota_status = true;
        return net_fwrite(file, buffer, len, 0);
    } else {
        return fwrite(buffer, 1, len, file);
    }
}

void *vfs_open(const char *filename, const char *mode)
{
    if (ftp_ota && ftp_ota->fname && strstr(filename, ftp_ota->fname)) {
        if (ftp_ota->ota_status) {
            net_fclose(NULL, 1);
        }
        ftp_ota->ota_status = true;
        printf("---> ota update %s \n", filename);
        return net_fopen(filename, mode);
    } else {
        vfs_cwd2root_path(filename);
#ifdef CONFIG_JLFAT_ENABLE
        char e_filename[MAX_FILENAME_LEN];
        int fname_len = long_file_name_encode(filename, (u8 *)e_filename, sizeof(e_filename));
        e_filename[fname_len] = 0;
        filename = e_filename;
#endif
        printf("---> file open %s \n", filename);
        return fopen(filename, mode);
    }
}

char *vfs_getcwd(vfs_file_t *vfs, int dummy)
{
    char *cwd = ftp_malloc(128);
    if (cwd) {
        if (dir_ent.cwd_dir) {
            strcpy(cwd, dir_ent.cwd_dir);
        } else {
            strcpy(cwd, vfs_cwd2root_path(NULL));
            dir_ent.cwd_dir = ftp_malloc(128);
            if (dir_ent.cwd_dir) {
                strcpy(dir_ent.cwd_dir, cwd);
            }
        }
    }
    return cwd;
}

vfs_dir_t *vfs_opendir(const char *path)
{
    vfs_cwd2root_path(path);
    return fscan(path, "-a -d -tALL -st", 9);
}

void vfs_closedir(vfs_dir_t *dir)
{
    if (dir) {
        fscan_release(dir);
    }
}

int vfs_rename(char *from, const char *newname)
{
    int err = -1;
    vfs_cwd2root_path(from);
#ifndef CONFIG_JLFAT_ENABLE
    vfs_cwd2root_path(newname);
#endif
#ifdef CONFIG_JLFAT_ENABLE
    char e_filename[MAX_FILENAME_LEN];
    int fname_len = long_file_name_encode(from, (u8 *)e_filename, sizeof(e_filename));
    e_filename[fname_len] = 0;
    from = e_filename;
#endif
    vfs_file_t *vfs = fopen(from, "r");
    if (vfs) {
        err = frename(vfs, newname);
        fclose(vfs);
    }
    return err;
}

int vfs_mkdir(const char *path, int mode)
{
    vfs_cwd2root_path(path);
    return fmk_dir(path, NULL, 0);
}

int vfs_rmdir(const char *path)
{
    vfs_cwd2root_path(path);
#ifdef CONFIG_JLFAT_ENABLE
    char e_filename[MAX_FILENAME_LEN];
    int fname_len = long_file_name_encode(path, (u8 *)e_filename, sizeof(e_filename));
    e_filename[fname_len] = 0;
    path = e_filename;
#endif
    return fdelete_dir(path);
}

int vfs_remove(const char *name)
{
    vfs_cwd2root_path(name);
#ifdef CONFIG_JLFAT_ENABLE
    char e_filename[MAX_FILENAME_LEN];
    int fname_len = long_file_name_encode(name, (u8 *)e_filename, sizeof(e_filename));
    e_filename[fname_len] = 0;
    name = e_filename;
#endif
    return fdelete_by_name(name);
}

int vfs_chdir(const char *dir)
{
    if (dir_ent.cwd_dir && dir && dir[0] != '\0') {
        if (dir[0] != '/') {
            dir_ent.cwd_dir[0] = '/';
            strcpy(dir_ent.cwd_dir + 1, dir);
        } else {
            strcpy(dir_ent.cwd_dir, dir);
        }
        if (dir_ent.cwd_dir[strlen(dir_ent.cwd_dir) - 1] != '/') {
            strcat(dir_ent.cwd_dir, "/");
        }
    }
    vfs_cwd2root_path(NULL);
    return 0;
}

void vfs_chdir_close(const char *dir)
{
    if (dir_ent.cwd_dir) {
        free(dir_ent.cwd_dir);
        dir_ent.cwd_dir = NULL;
    }
}
/*
 * end : FTP file system APIs
 */
/*
 * end : system platform APIs
 */

