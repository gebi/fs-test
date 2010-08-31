/*
 * Some tests to check the file system sematics.  Used to verify that
 * CIFS from a windows server do not work properly as a linux home
 * directory.
 * License: GPL v2 or later
 * 
 * needs libsqlite3-dev and build-essential installed
 * compile with: gcc -Wall -lsqlite3 -DTEST_SQLITE fs-test.c -o fs-test
*/

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

#define _GNU_SOURCE /* for asprintf() */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef TEST_SQLITE
/*
 * Test sqlite open, as done by gcompris require the libsqlite3-dev
 * package and linking with -lsqlite3.  A more low level test is
 * below.
 * See also <URL: http://www.sqlite.org./faq.html#q5 >.
 */
#include <sqlite3.h>
#define CREATE_TABLE_USERS                                              \
  "CREATE TABLE users (user_id INT UNIQUE, login TEXT, lastname TEXT, firstname TEXT, birthdate TEXT, class_id INT ); "
int test_sqlite_open(void) {
  char *zErrMsg;
  char *name = "testsqlite.db";
  sqlite3 *db=NULL;
  unlink(name);
  int rc = sqlite3_open(name, &db);
  if( rc ){
    printf("error: sqlite open of %s failed: %s\n", name, sqlite3_errmsg(db));
    sqlite3_close(db);
    return -1;
  }

  /* create tables */
  rc = sqlite3_exec(db,CREATE_TABLE_USERS, NULL,  0, &zErrMsg);
  if( rc != SQLITE_OK ){
    printf("error: sqlite table create failed: %s\n", zErrMsg);
    sqlite3_close(db);
    return -1;
  }
  printf("info: sqlite worked\n");
  sqlite3_close(db);
  return 0;
}
#endif /* TEST_SQLITE */

/*
 * Demonstrate locking issue found in gcompris using sqlite3.  This
 * work with ext3, but not with cifs server on Windows 2003.  This is
 * done in the sqlite3 library.
 * See also
 * <URL:http://www.cygwin.com/ml/cygwin/2001-08/msg00854.html> and the
 * POSIX specification
 * <URL:http://www.opengroup.org/onlinepubs/009695399/functions/fcntl.html>.
 */
int test_gcompris_locking(void) {
  struct flock fl;
  char *name = "testsqlite.db";
  int fd;

  unlink(name);
  fd = open(name, O_RDWR|O_CREAT|O_LARGEFILE, 0644);
  printf("info: testing fcntl locking\n");

  fl.l_whence = SEEK_SET;
  fl.l_pid    = getpid();
  printf("  Read-locking 1 byte from 1073741824");
  fl.l_start  = 1073741824;
  fl.l_len    = 1;
  fl.l_type   = F_RDLCK;
  if (0 != fcntl(fd, F_SETLK, &fl) ) printf(" - error!\n"); else printf("\n");

  printf("  Read-locking 510 byte from 1073741826");
  fl.l_start  = 1073741826;
  fl.l_len    = 510;
  fl.l_type   = F_RDLCK;
  if (0 != fcntl(fd, F_SETLK, &fl) ) printf(" - error!\n"); else printf("\n");

  printf("  Unlocking 1 byte from 1073741824");
  fl.l_start  = 1073741824;
  fl.l_len    = 1;
  fl.l_type   = F_UNLCK;
  if (0 != fcntl(fd, F_SETLK, &fl) ) printf(" - error!\n"); else printf("\n");

  printf("  Write-locking 1 byte from 1073741824");
  fl.l_start  = 1073741824;
  fl.l_len    = 1;
  fl.l_type   = F_WRLCK;
  if (0 != fcntl(fd, F_SETLK, &fl) ) printf(" - error!\n"); else printf("\n");

  printf("  Write-locking 510 byte from 1073741826");
  fl.l_start  = 1073741826;
  fl.l_len    = 510;
  if (0 != fcntl(fd, F_SETLK, &fl) ) printf(" - error!\n"); else printf("\n");

  printf("  Unlocking 2 byte from 1073741824");
  fl.l_start  = 1073741824;
  fl.l_len    = 2;
  fl.l_type   = F_UNLCK;
  if (0 != fcntl(fd, F_SETLK, &fl) ) printf(" - error!\n"); else printf("\n");

  close(fd);
  return 0;
}

/*
 * Test if permissions of freshly created directories allow entries
 * below them.  This was a problem with OpenOffice.org and gcompris.
 * Mounting with option 'sync' seem to solve this problem while
 * slowing down file operations.
 */
int test_subdirectory_creation(void) {
#define LEVELS 5
  char *path = strdup("test");
  int level;
  printf("info: testing subdirectory creation\n");
  for (level = 0; level < LEVELS; level++) {
    char *newpath = NULL;
    if (-1 == mkdir(path, 0777)) {
      printf("  error: Unable to create directory '%s': %s\n",
         path, strerror(errno));
      break;
    }
    asprintf(&newpath, "%s/%s", path, "test");
    free(path);
    path = newpath;
  }
  return 0;
}

/*
 * Test if symlinks can be created.  This was a problem detected with
 * KDE.
 */
int test_symlinks(void) {
  printf("info: testing symlink creation\n");
  unlink("symlink");
  if (-1 == symlink("file", "symlink"))
    printf("  error: Unable to create symlink\n");
  return 0;
}

/*
 * Test is hard links can be created.  This was a problem detected on sshfs.
 */
int test_hardlinks(void) {
  static const char name[] = "file";
  int fd;
  struct stat status;
  nlink_t old_link_count;

  printf("info: testing hard link creation\n");

  fd = open(name, O_RDONLY|O_CREAT, 0644);
  if (-1 == fd) {
    printf("  warning: Unable to touch file to hardlink\n");
    goto cleanup0;
  }

  unlink("hardlink");

  if (-1 == fstat(fd, &status)) {
    printf("  warning: Unable to stat file to hardlink\n");
    goto cleanup1;
  }
  old_link_count = status.st_nlink;

  if (-1 == link(name, "hardlink")) {
    printf("  error: Unable to create hard link\n");
  }

  if (-1 == fstat(fd, &status)) {
    printf("  warning: Unable to stat file to hardlink\n");
    goto cleanup1;
  }

  if (old_link_count + 1 != status.st_nlink) {
    printf(" error: link() succeeded but link count was not incremented\n");
  }

 cleanup1:
  close(fd);

 cleanup0:
  return 0;
}

mode_t touch_get_mode(const char *name, mode_t mode) {
  mode_t retval = 0;
  int fd = open(name, O_RDWR|O_CREAT|O_LARGEFILE, mode);
  if (-1 != fd) {
    struct stat statbuf;

    unlink(name);
    if (-1 != fstat(fd, &statbuf)) {
      retval = statbuf.st_mode & 0x1ff;
    }
    close(fd);
  }
  return retval;
}

/* Try to detect problem discovered using sshfs */
int test_umask(void) {
  mode_t orig_umask = umask(000);
  mode_t newmode;

  printf("info: testing umask effect on file creation\n");
  if (0666 != (newmode = touch_get_mode("foobar", 0666))) {
    printf("  error: Wrong file mode %o when creating using mode 666 and umask 000\n",
           newmode);
  }
  umask(007);
  if (0660 != (newmode = touch_get_mode("foobar", 0666))) {
    printf("  error: Wrong file mode %o when creating using mode 666 and umask 007\n",
           newmode);
  }

  umask (orig_umask);
  return 0;
}


int main(void) {
  printf("Testing POSIX/Unix sematics on file system\n");
  test_symlinks();
  test_hardlinks();
  test_subdirectory_creation();
  test_umask();
#ifdef TEST_SQLITE
  test_sqlite_open();
#endif /* TEST_SQLITE */
  test_gcompris_locking();
  return 0;
}

