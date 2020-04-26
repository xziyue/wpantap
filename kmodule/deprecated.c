// this function is not needed temporarily
// returns 0 if the insertion is successful
static int ringbuf_insert_data2(struct ringbuf_t *rb, int size1, void *data1, int size2, void *data2)
{
	int total_size;
	int size = size1 + size2;
	char *cdata1, *cdata2;
	char *rbdata;
	void *rbtail;
	char *temp;
	int i;
	int prev_sum;
	
	if(size == 0){
		printk(KERN_WARNING "trying to insert a buffer of size 0, discarded\n");
		return 0;
	}

	cdata1 = (char*)data1;
	cdata2 = (char*)data2;
	total_size = sizeof(int) + size;
	rbtail = rb->tail;
	
	printk(KERN_DEBUG "inserting data (%d) into ring buffer...\n", size);
	print_ringbuf_stat();

	if(total_size > rb->capacity){
		printk(KERN_ERR "the total size of data (%d) is bigger than the capacity of ring buffer (%d)\n", total_size, rb->capacity);
		return 1;
	}

	// check bytes free
	if(total_size > ringbuf_bytes_free(rb)){
		// pop data until there is enough space
		while(total_size > ringbuf_bytes_free(rb)){
			i = ringbuf_pop_data(rb);
			if (i != 0){
				printk(KERN_ERR "error while popping buffer for insertion\n");
				return 1;
			}
		}
	}

	// insert data into the ring buffer
	temp = (char*)&size;
	
	for(i = 0; i < sizeof(int); ++i){
		rbdata = (char*)ringbuf_ll(rb, rbtail, i);
		*rbdata = temp[i];
	}

	prev_sum = sizeof(int);
	
	for(i = 0; i < size1; ++i){
		rbdata = (char*)ringbuf_ll(rb, rbtail, i + prev_sum);
		*rbdata = cdata1[i];
	}
	
	/*
	printk(KERN_DEBUG "first bytes: %02x %02x %02x %02x\n",
			*(char*)ringbuf_ll(rb, rbtail, prev_sum),
			*(char*)ringbuf_ll(rb, rbtail, prev_sum + 1),
			*(char*)ringbuf_ll(rb, rbtail, prev_sum + 2),
			*(char*)ringbuf_ll(rb, rbtail, prev_sum + 3));
	*/
	
	prev_sum += size1;
	
	for(i = 0; i < size2; ++i){
		rbdata = (char*)ringbuf_ll(rb, rbtail, i + prev_sum);
		*rbdata = cdata2[i];
	}

	// modify tail
	rb->tail = (void*)rbdata + 1;

	print_ringbuf_stat();

	return 0;
}
