
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


static list_t* list_buffers;
static uint64_t buffers_length;

extern task_t* current_task;

int bufio_init() {
	list_init(list_buffers);
	
	return 0;
}


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


bufio_t* bufio_alloc(size_t size) {
	void* addr = (void*) kmalloc(size);
	if(!addr)
		return 0;
		
	lock();	
	buffers_length += size;

	bufio_t* buf = (bufio_t*) kmalloc(size);
	buf->raw = addr;
	buf->size = size;
	buf->offset = (off_t) 0;
	buf->task = current_task;
	
	list_add(list_buffers, (listval_t) buf);

	unlock();
	
	return buf;
}

void bufio_free(bufio_t* buf) {
	lock();
	buffers_length -= buf->size;
	
	kfree(buf->raw);
	kfree(buf);
	
	list_remove(list_buffers, (listval_t) buf);
	
	
	unlock();
}

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

int bufio_tell(bufio_t* buf) {
	return bufio_seek(buf, 0, SEEK_CUR);
}

void bufio_clear(bufio_t* buf) {
	spinlock_lock(&buf->lock);
	memset(buf->raw, 0, buf->size);
	spinlock_unlock(&buf->lock);
}

int bufio_read(bufio_t* buf, void* ptr, size_t len) {		
	spinlock_lock(&buf->lock);
	
	len = (len + buf->offset > buf->size) ? (buf->size - buf->offset) : len;
	memcpy(ptr, (void*) ((off_t) buf->raw + buf->offset), len);
	
	buf->offset += (off_t) len;
	
	spinlock_unlock(&buf->lock);
	
	return (int) len;
}

int bufio_write(bufio_t* buf, void* ptr, size_t len) {		
	
	spinlock_lock(&buf->lock);
	
	len = (len + buf->offset > buf->size) ? (buf->size - buf->offset) : len;
	memcpy((void*) ((off_t) buf->raw + buf->offset), ptr, len);
	
	buf->offset += (off_t) len;
	
	spinlock_unlock(&buf->lock);
	
	return (int) len;
}
