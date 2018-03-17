/*
 * OS Assignment #4
 *
 * @file pfs.c
 */

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>

static char *
strstrip (char *str)
{
  char  *start;
  size_t len;

  len = strlen (str);
  while (len--)
    {
      if (!isspace (str[len]))
        break;
      str[len] = '\0';
    }

  for (start = str; *start && isspace (*start); start++)
    ;
  memmove (str, start, strlen (start) + 1);

  return str;
}

static int
get_proc (const char  *path,
          pid_t       *pid,
          char        *proc_path,
          int          proc_path_len)
{
  char proc[4096];
  pid_t pid2;
  char *path2;
  char *path3;
  char *dir;
  char *name;

  if (!pid)
    pid = &pid2;
  if (!proc_path || proc_path_len <= 0)
    {
      proc_path = proc;
      proc_path_len = sizeof (proc);
    }
  
  path2 = strdup (path);
  path3 = strdup (path);
  dir = dirname (path2);
  name = basename (path3);

  if (strcmp (dir, "/") == 0 && strcmp (name, "/") == 0)
    {
      *pid = 1;
      strcpy (proc_path, "/proc");
      free (path2);
      free (path3);
      return 0;
    }

  if (sscanf (name, "%d-", pid) != 1)
    {
      free (path2);
      free (path3);
      return -ENOENT;
    }
  free (path2);
  free (path3);

  snprintf (proc_path, proc_path_len, "/proc/%d", *pid);

  return 0;
}

static int
pfs_getattr (const char  *path,
             struct stat *stbuf)
{
  char proc_path[4096];
  char *path2;
  char *name;
  pid_t pid;
  struct stat st;
  FILE *f;

  memset (stbuf, 0, sizeof(struct stat));

  path2 = strdup (path);
  name = basename (path2);
  
  if (strcmp (path, "/") == 0)
    {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
      free (path2);
      return 0;
    }

  if (get_proc (name, &pid, proc_path, sizeof (proc_path)) < 0)
    {
      free (path2);
      return -ENOENT;
    }

  free (path2);

  if (stat (proc_path, &st) < 0)
    return -errno;
  
  stbuf->st_mode = S_IFREG | 0644;
  stbuf->st_nlink = 1;
  stbuf->st_uid = st.st_uid;
  stbuf->st_gid = st.st_gid;
  stbuf->st_atim = st.st_atim;
  stbuf->st_mtim = st.st_mtim;
  stbuf->st_ctim = st.st_ctim;

  snprintf (proc_path, sizeof (proc_path), "/proc/%d/status", pid);
  f = fopen (proc_path, "r");
  if (f)
    {
      char status[256];

      while (1)
        {
          char *s;

          if (!fgets (status, sizeof (status), f))
            break;

          status[strlen (status) - 1] = '\0';

          /* extract 'Vm' */
          s = strstr (status, "VmSize:");
          if (s)
            {
              s += 7;
              strstrip (s);
              stbuf->st_size = strtol (s, NULL, 10);
            }
        }

      fclose (f);
    }

  return 0;
}

static int
pfs_readdir (const char            *path,
             void                  *buf,
             fuse_fill_dir_t        filler,
             off_t                  offset,
             struct fuse_file_info *fi)
{
  DIR *dir;
  
  if (strcmp (path, "/") != 0)
    return -ENOENT;

  filler (buf, ".", NULL, 0);
  filler (buf, "..", NULL, 0);

  /* find all running processes */
  dir = opendir ("/proc");
  if (!dir)
    return -errno;

  while (1)
    {
      struct dirent *ent;
      char           child[512];
      char          *name;
      FILE          *f;

      ent = readdir (dir);
      if (!ent)
        break;

      if (strcmp (ent->d_name, ".") == 0
          || strcmp (ent->d_name, "..") == 0
          || strcmp (ent->d_name, "self") == 0)
        continue;

      if (strtol (ent->d_name, NULL, 10) <= 0)
        continue;

      name = NULL;

      snprintf (child, sizeof (child), "/proc/%s/cmdline", ent->d_name);
      f = fopen (child, "r");
      if (f)
        {
          char cmdline[256];

          if (fgets (cmdline, sizeof (cmdline), f) != NULL)
            {
              char *s;

              strstrip (cmdline);
              name = strdup (cmdline);

              for (s = name; *s != 0x00; s++)
                if (*s == '/')
                  *s = '-';

            }
          fclose (f);
        }

      if (name)
        {
          char str[4096];

          snprintf (str, sizeof (str), "%s%s%s",
                    ent->d_name,
                    name[0] == '-' ? "" : "-",
                    name);
          filler (buf, str, NULL, 0);
          free (name);
        }
    }
  closedir (dir);

  return 0;
}

static int pfs_unlink (const char *path)
{
  /* TODO: not yet implemented */
  char proc_path[4096];//get_proc 함수에 사용될 임시 버퍼
  char *path2;//임시 포인터 변수
  char *name;//파일의 이름을 담을 변수
  pid_t pid;//프로세스 아이디를 담을 변수
  path2 = strdup (path);
  name = basename (path2);
  if (get_proc (name, &pid, proc_path, sizeof (proc_path)) < 0)//프로세스 정보를 받아온다.
    {
      free (path2);
      return -ENOENT;//만약 get_proc에 실패했을 경우 -ENOENT 반환
    }
  kill(pid,SIGKILL);//unlink가 호출된 해당 프로세스 pid에 SIGKILL 신호 전달
  return 0;
}

static struct fuse_operations pfs_oper =
{
  .getattr  = pfs_getattr,
  .readdir  = pfs_readdir,
  .unlink   = pfs_unlink
};

int
main (int    argc,
      char **argv)
{
  return fuse_main (argc, argv, &pfs_oper);
}
