/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aPlus.                                          
 *                                                                      
 * aPlus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aPlus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/errno.h>


static char* errmsg[] = {
        
    "Illegal byte sequence",
    "Domain error",
    "Result not representable",

    "Not a tty",
    "Permission denied",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "File exists",

    "Value too large for data type",
    "No space left on device",
    "Out of memory",

    "Resource busy",
    "Interrupted system call",
    "Resource temporarily unavailable",
    "Invalid seek",

    "Cross-device link",
    "Read-only file system",
    "Directory not empty",

    "Connection reset by peer",
    "Operation timed out",
    "Connection refused",
    "Host is down",
    "Host is unreachable",
    "Address in use",

    "Broken pipe",
    "I/O error",
    "No such device or address",
    //"Block device required",
    "No such device",
    "Not a directory",
    "Is a directory",
    "Text file busy",
    "Exec format error",

    "Invalid argument",

    "Argument list too long",
    "Symbolic link loop",
    "Filename too long",
    "Too many open files in system",
    "No file descriptors available",
    "Bad file descriptor",
    "No child process",
    "Bad address",
    "File too large",
    "Too many links",
    "No locks available",

    "Resource deadlock would occur",
    "State not recoverable",
    "Previous owner died",
    "Operation canceled",
    "Function not implemented",
    "No message of desired type",
    "Identifier removed",
    "Device not a stream",
    "No data available",
    "Device timeout",
    "Out of streams resources",
    "Link has been severed",
    "Protocol error",
    "Bad message",
    //"File descriptor in bad state",
    "Not a socket",
    "Destination address required",
    "Message too large",
    "Protocol wrong type for socket",
    "Protocol not available",
    "Protocol not supported",
    //"Socket type not supported",
    "Not supported",
    "Protocol family not supported",
    "Address family not supported by protocol",
    "Address not available",
    "Network is down",
    "Network unreachable",
    "Connection reset by network",
    "Connection aborted",
    "No buffer space available",
    "Socket is connected",
    "Socket not connected",
    //"Cannot send after socket shutdown",
    "Operation already in progress",
    "Operation in progress",
    "Stale file handle",
    //"Remote I/O error",
    "Quota exceeded",
    //"No medium found",
    //"Wrong medium type",
    "No error information",

};


static int errid[] = {

    EILSEQ,
    EDOM,
    ERANGE,

    ENOTTY,
    EACCES,
    EPERM,
    ENOENT,
    ESRCH,
    EEXIST,

    EOVERFLOW,
    ENOSPC,
    ENOMEM,

    EBUSY,
    EINTR,
    EAGAIN,
    ESPIPE,

    EXDEV,
    EROFS,
    ENOTEMPTY,

    ECONNRESET,
    ETIMEDOUT,
    ECONNREFUSED,
    EHOSTDOWN,
    EHOSTUNREACH,
    EADDRINUSE,

    EPIPE,
    EIO,
    ENXIO,
    //ENOTBLK,
    ENODEV,
    ENOTDIR,
    EISDIR,
    ETXTBSY,
    ENOEXEC,

    EINVAL,

    E2BIG,
    ELOOP,
    ENAMETOOLONG,
    ENFILE,
    EMFILE,
    EBADF,
    ECHILD,
    EFAULT,
    EFBIG,
    EMLINK,
    ENOLCK,

    EDEADLK,
    ENOTRECOVERABLE,
    EOWNERDEAD,
    ECANCELED,
    ENOSYS,
    ENOMSG,
    EIDRM,
    ENOSTR,
    ENODATA,
    ETIME,
    ENOSR,
    ENOLINK,
    EPROTO,
    EBADMSG,
    //EBADFD,
    ENOTSOCK,
    EDESTADDRREQ,
    EMSGSIZE,
    EPROTOTYPE,
    ENOPROTOOPT,
    EPROTONOSUPPORT,
    //ESOCKTNOSUPPORT,
    ENOTSUP,
    EPFNOSUPPORT,
    EAFNOSUPPORT,
    EADDRNOTAVAIL,
    ENETDOWN,
    ENETUNREACH,
    ENETRESET,
    ECONNABORTED,
    ENOBUFS,
    EISCONN,
    ENOTCONN,
    //ESHUTDOWN,
    EALREADY,
    EINPROGRESS,
    ESTALE,
    //EREMOTEIO,
    EDQUOT,
    //ENOMEDIUM,
    //EMEDIUMTYPE,

    0,

};




char* strerror(int err) {

    if(err < 0)
        err = 0;

    if(err > 4096)
        err = 0;
    

    int i;
    for(i = 0; errid[i]; i++) {

        if(errid[i] == err)
            return errmsg[i];

    }

    return errmsg[i];

}