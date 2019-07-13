SC(2, 4, stat)
SC(2, 5, fstat)
SC(2, 6, lstat)
SC(4, 56, clone);
SC(0, 57, fork);
SC(0, 58, vfork);
//SC(3, 59, execve);        // newlib-fix
SC(3, 59, _execve);
SC(1, 63, uname)

/* Extension */
SC(1, 500, __exit)
SC(2, 501, statvfs)
SC(1, 502, nice)
