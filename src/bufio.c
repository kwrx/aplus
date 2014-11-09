
#include <aplus.h>
#include <aplus/list.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>
#include <aplus/bufio.h>
#include <aplus/task.h>

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>


/**
 *	\brief List of allocated buffers.
 */
static list_t* list_buffers;

/**
 *	\brief Allocated RAM of all buffers.
 */
static uint64_t buffers_length;

extern task_t* current_task;


/**
 *	\brief BufIO initialization.
 */
int bufio_init() {
	list_init(list_buffers);
	
	return 0;
}


/**
 *	\brief Free al unused buffers.
 */
void bufio_free_unused() {

	list_t* tmp;
	list_init(tmp);
	list_clone(tmp, list_buffers);
	
	list_foreach(value, tmp) {
		//bufio_t* value = (bufio_t*) value;
		
		//if(value->task->state == TASK_STATE_DEAD)
		//	bufio_free(value);
	}
	
	list_destroy(tmp);
}


/**
 *	\brief Alloc new buffer for current task.
 *	\param size Size in Bytes of data.
 *	\return bufio descriptor.
 */
bufio_t* bufio_alloc(size_t size) {
	void* addr = (void*) kmalloc(size);
	if(!addr)
		return 0;
		
	lock();	
	buffers_length += size;


	bufio_t* buf = (bufio_t*) kmalloc(size);
	buf->raw = addr;
	buf->size = size;
	buf->type = 0;
	buf->offset = (off_t) 0;
	buf->task = current_task;


	list_add(list_buffers, (listval_t) buf);

	unlock();
	return buf;
}

/**
 *	\brief Create new buffer from allocated data for current task.
 *	\param raw Buffer allocated address.
 *	\param size Size in Bytes of data.
 *	\return bufio descriptor.
 */
bufio_t* bufio_alloc_raw(void* raw, size_t size) {
	if(!raw)
		return 0;

	lock();	

	buffers_length += size;

	bufio_t* buf = (bufio_t*) kmalloc(size);
	buf->raw = raw;
	buf->size = size;
	buf->type = 0;
	buf->offset = (off_t) 0;
	buf->task = current_task;
	
	list_add(list_buffers, (listval_t) buf);

	unlock();
	
	return buf;
}

/**
 *	\brief Free and remove buffer.
 *	\param buf bufio descriptor.
 */
void bufio_free(bufio_t* buf) {
	lock();
	buffers_length -= buf->size;
	
	kfree(buf->raw);
	kfree(buf);
	
	list_remove(list_buffers, (listval_t) buf);
	
	
	unlock();
}


/**
 *	\brief Sets the position indicator associated with the bufio descriptor to a new position.
 *	\param buf Pointer to a bufio descriptor.
 *	\param offset Number of bytes to offset from dir.
 *	\param dir Position used as reference for the offset\n
	+	SEEK_SET: Beginning of stream.\n
	+	SEEK_CUR: Current position of stream.\n
	+	SEEK_END: End of stream.\n
 *	\return If successful return current position of stream, otherwise, it returns non-zero value.
 */
int bufio_seek(bufio_t* buf, off_t offset, int dir) {

	if(offset > buf->size)
		return -1;
		
	if(dir == SEEK_CUR && (offset + buf->offset) > buf->size)
		return -1;

	switch(dir) {
		case SEEK_SET:
			buf->offset = offset;
			break;
			
		case SEEK_END:
			buf->offset = buf->offset - offset;
			break;
			
		case SEEK_CUR:
			buf->offset += offset;
			break;
			
		default:
			return -1;
	}
	
	return buf->offset;
}

/**
 *	\brief Returns the current value of the position indicator of the stream.\n
 *	\param buf Pointer to a bufio descriptor.
 *	\return On success, the current value of the position indicator is returned.\n
			On failure, -1L is returned.
 */

int bufio_tell(bufio_t* buf) {
	return bufio_seek(buf, 0, SEEK_CUR);
}


/**
 *	\brief Clear entire stream of the buffer descriptor.
 *	\param buf Pointer to a bufio descriptor.
 */ 
void bufio_clear(bufio_t* buf) {
	spinlock_lock(&buf->lock);
	memset(buf->raw, 0, buf->size);
	spinlock_unlock(&buf->lock);
}


/**
 *	\brief This function shall attempt to read nbyte bytes from the stream associated with the bufio descriptor, buf, into the buffer pointed to by ptr
 *	\param buf Pointer to a bufio descriptor.
 *	\param ptr Pointer to output buffer.
 *	\param len Size of data to read.
 *	\return Upon successful completion, shall return a 
			non-negative integer indicating the number of bytes actually read.\n 
			Otherwise, the functions shall return -1 and set errno to indicate 
			the error.
 */
int bufio_read(bufio_t* buf, void* ptr, size_t len) {		
	spinlock_lock(&buf->lock);
	
	len = (len + buf->offset > buf->size) ? (buf->size - buf->offset) : len;
	memcpy(ptr, (void*) ((off_t) buf->raw + buf->offset), len);
	
	buf->offset += (off_t) len;
	
	spinlock_unlock(&buf->lock);
	
	return (int) len;
}


/**
 *	\brief This function shall attempt to write nbyte bytes into the stream associated with the bufio descriptor, buf, from the buffer pointed to by ptr
 *	\param buf Pointer to a bufio descriptor.
 *	\param ptr Pointer to input buffer.
 *	\param len Size of data to write.
 *	\return Upon successful completion, shall return a 
			non-negative integer indicating the number of bytes actually write.\n 
			Otherwise, the functions shall return -1 and set errno to indicate 
			the error.
 */
int bufio_write(bufio_t* buf, void* ptr, size_t len) {		
	
	spinlock_lock(&buf->lock);
	
	len = (len + buf->offset > buf->size) ? (buf->size - buf->offset) : len;
	memcpy((void*) ((off_t) buf->raw + buf->offset), ptr, len);
	
	buf->offset += (off_t) len;
	
	spinlock_unlock(&buf->lock);
	
	return (int) len;
}

