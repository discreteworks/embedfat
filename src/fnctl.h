// Flags

enum {
  O_RDONLY = 0x01,
  O_WRONLY = 0x02,
  O_RDWR	 = 0x03,
  O_APPEND = 0x04,
  O_CREAT  = 0x08,
  O_TRUNC  = 0x10
};
/*	O_EXCL,
	O_DSYNC,
	O_NOCTTY,
	O_NONBLOCK,
	O_RSYNC,
	O_SYNC,
};*/

//SEEK
#ifndef SEEK_SET
enum {
  SEEK_SET = 0x0,
  SEEK_CUR,
  SEEK_END
};
#endif

//Errors

enum {
  EACCES = -50,
  EEXIST,
  EINTR,
  EINVAL,
  EIO,
  EISDIR,
  ELOOP,
  EMFILE,
  ENAMETOOLONG,
  ENFILE,
  ENOENT,
  ENOSR,
  ENOSPC,
  ENOTDIR,
  ENXIO,
  EOVERFLOW,
  EROFS,
  EAGAIN,
  ENOMEM,
  ETXTBSY,
  EBADF
};
