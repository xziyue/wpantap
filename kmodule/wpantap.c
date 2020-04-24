// SPDX-License-Identifier: GPL-2.0-only
/*
 * WPAN TAP interface
 *
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <net/mac802154.h>
#include <net/cfg802154.h>

// ring buffer for temporary packet storage
static DEFINE_SPINLOCK(ringbuf_spin);

// the size of ring buffer
#define RINGBUF_SIZE 2048


struct ringbuf_t
{
	void *buf;
	void *head, *tail;
	void *end;
	int size;
	int capacity;
};


static struct ringbuf_t rbuf;

// returns 0 if init is successful
static int ringbuf_init(struct ringbuf_t *rb)
{
	rb->buf = kmalloc(RINGBUF_SIZE, GFP_KERNEL);
	if(rb->buf == NULL){
		printk(KERN_ERR "unable to allocate %d bytes for ring buffer.\n", RINGBUF_SIZE);
		return 1;
	}
	
	rb->size = RINGBUF_SIZE;
	rb->capacity = rb->size - 1;
	rb->head = rb->tail = rb->buf;
	rb->end = rb->buf + rb->size;

	return 0;
}


static void ringbuf_deinit(struct ringbuf_t *rb)
{
	kfree(rb->buf);
	rb->size = rb->capacity = 0;
}


static int ringbuf_bytes_used(struct ringbuf_t *rb)
{
	if(rb->tail >= rb->head){
		return rb->tail - rb->head;
	}
	else{
		return rb->capacity - (rb->head - rb->tail - 1);
	}
}


static int ringbuf_bytes_free(struct ringbuf_t *rb)
{
	return rb->capacity - ringbuf_bytes_used(rb);
}


static int ringbuf_is_empty(struct ringbuf_t *rb)
{
	return ringbuf_bytes_free(rb) == rb->capacity;
}


/*
 * Given a ring buffer rb, a location and a offset to a location within its
 * contiguous buffer, returns the logical location
 */
static void *ringbuf_ll(struct ringbuf_t *rb, void *anchor, int offset)
{
	return rb->buf + (((anchor - rb->buf) + offset) % rb->size);
}

static int ringbuf_get_first_data_size(struct ringbuf_t *rb)
{
	int *size;
	if (ringbuf_is_empty(rb) == 1){
		printk(KERN_ERR "no data is avaliable in the buffer!\n");
		return 0;
	}
	size = (int*)rb->head;
	return *size;
}


static int ringbuf_copy_first_data(struct ringbuf_t *rb, void *p)
{
	int i;
	int size = ringbuf_get_first_data_size(rb);
	char *cp = (char*)p;
	
	printk(KERN_DEBUG "copying data from ring buf\n");
	
	if (size == 0){
		printk(KERN_ERR "data copy failed!\n");
		return 0;
	}
	
	/*
	printk(KERN_DEBUG "first bytes: %02x %02x %02x %02x\n",
		*(char*)ringbuf_ll(rb, rb->head, sizeof(int)),
		*(char*)ringbuf_ll(rb, rb->head, sizeof(int) + 1),
		*(char*)ringbuf_ll(rb, rb->head, sizeof(int) + 2),
		*(char*)ringbuf_ll(rb, rb->head, sizeof(int) + 3));
	*/

	for(i = 0; i < size; ++i){
		cp[i] = *(char*)ringbuf_ll(rb, rb->head, i + sizeof(int));
	}
	
	return size;
}

static void print_ringbuf_stat(void)
{
	struct ringbuf_t *rb = &rbuf;
	size_t htdist = rb->tail - rb->head;
	printk(KERN_DEBUG "ringbuf stat h:%lu t:%lu d:%lu u:%lu f:%lu c:%d\n",
		(size_t)rb->head,
		(size_t)rb->tail,
		htdist,
		(size_t)ringbuf_bytes_used(rb),
		(size_t)ringbuf_bytes_free(rb),
		rb->capacity);
}


// returns 0 if a data block is poped
static int ringbuf_pop_data(struct ringbuf_t *rb)
{	
	int size = ringbuf_get_first_data_size(rb);
	int total_size;
	
	printk(KERN_DEBUG "popping data from ring buffer...\n");
	print_ringbuf_stat();

	if(size == 0){
		return 1;
	}
	
	// find the length of current buffer
	total_size = size + sizeof(int);
	
	rb->head = ringbuf_ll(rb, rb->head, total_size);
	print_ringbuf_stat();

	return 0;
}


// returns 0 if the insertion is successful
static int ringbuf_insert_data(struct ringbuf_t *rb, int size, void *data)
{
	int total_size;
	char *cdata;
	char *rbdata;
	void *rbtail;
	char *temp;
	int i;
	
	if(size == 0){
		printk(KERN_WARNING "trying to insert a buffer of size 0, discarded\n");
		return 0;
	}

	cdata = (char*)data;
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

	for(i = 0; i < size; ++i){
		rbdata = (char*)ringbuf_ll(rb, rbtail, i + sizeof(int));
		*rbdata = cdata[i];
	}

	// modify tail
	rb->tail = (void*)rbdata + 1;
	print_ringbuf_stat();	

	return 0;
}

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

// only create one device
static int numlbs = 1;

static LIST_HEAD(fakelb_phys);
static DEFINE_MUTEX(fakelb_phys_lock);

static LIST_HEAD(fakelb_ifup_phys);
static DEFINE_RWLOCK(fakelb_ifup_phys_lock);

struct fakelb_phy {
	struct ieee802154_hw *hw;

	u8 page;
	u8 channel;

	bool suspended;

	struct list_head list;
	struct list_head list_ifup;
};

static int fakelb_hw_ed(struct ieee802154_hw *hw, u8 *level)
{
	WARN_ON(!level);
	*level = 0xbe;

	return 0;
}

static int fakelb_hw_channel(struct ieee802154_hw *hw, u8 page, u8 channel)
{
	struct fakelb_phy *phy = hw->priv;

	write_lock_bh(&fakelb_ifup_phys_lock);
	phy->page = page;
	phy->channel = channel;
	write_unlock_bh(&fakelb_ifup_phys_lock);
	return 0;
}

static int fakelb_hw_xmit(struct ieee802154_hw *hw, struct sk_buff *skb)
{
	struct fakelb_phy *current_phy = hw->priv, *phy;
	int head_len;

	read_lock_bh(&fakelb_ifup_phys_lock);
	WARN_ON(current_phy->suspended);
	
	
    	printk(KERN_DEBUG "sending packet with WPAN device...\n");
	printk(KERN_DEBUG "skb len:%d data_len %d\n", skb->len, skb->data_len);
	//printk(KERN_DEBUG "first bytes: %02x %02x %02x %02x\n", (char*)skb->data[0], (char*)skb->data[1], (char*)skb->data[2], (char*)skb->data[3]);

	head_len = skb->len - skb->data_len;
    	spin_lock_bh(&ringbuf_spin);
	ringbuf_insert_data(&rbuf, skb->len, skb->data);
	spin_unlock_bh(&ringbuf_spin);
	
	read_unlock_bh(&fakelb_ifup_phys_lock);

	ieee802154_xmit_complete(hw, skb, false);
	return 0;
}

static int fakelb_hw_start(struct ieee802154_hw *hw)
{
	struct fakelb_phy *phy = hw->priv;

	write_lock_bh(&fakelb_ifup_phys_lock);
	phy->suspended = false;
	list_add(&phy->list_ifup, &fakelb_ifup_phys);
	write_unlock_bh(&fakelb_ifup_phys_lock);

	return 0;
}

static void fakelb_hw_stop(struct ieee802154_hw *hw)
{
	struct fakelb_phy *phy = hw->priv;

	write_lock_bh(&fakelb_ifup_phys_lock);
	phy->suspended = true;
	list_del(&phy->list_ifup);
	write_unlock_bh(&fakelb_ifup_phys_lock);
}

static int
fakelb_set_promiscuous_mode(struct ieee802154_hw *hw, const bool on)
{
	return 0;
}

static const struct ieee802154_ops fakelb_ops = {
	.owner = THIS_MODULE,
	.xmit_async = fakelb_hw_xmit,
	.ed = fakelb_hw_ed,
	.set_channel = fakelb_hw_channel,
	.start = fakelb_hw_start,
	.stop = fakelb_hw_stop,
	.set_promiscuous_mode = fakelb_set_promiscuous_mode,
};

/* Number of dummy devices to be set up by this module. 
   This parameter is suppressed.
*/
//module_param(numlbs, int, 0);
//MODULE_PARM_DESC(numlbs, " number of pseudo devices");

static int fakelb_add_one(struct device *dev)
{
	struct ieee802154_hw *hw;
	struct fakelb_phy *phy;
	int err;

	hw = ieee802154_alloc_hw(sizeof(*phy), &fakelb_ops);
	if (!hw)
		return -ENOMEM;

	phy = hw->priv;
	phy->hw = hw;

	/* 868 MHz BPSK	802.15.4-2003 */
	hw->phy->supported.channels[0] |= 1;
	/* 915 MHz BPSK	802.15.4-2003 */
	hw->phy->supported.channels[0] |= 0x7fe;
	/* 2.4 GHz O-QPSK 802.15.4-2003 */
	hw->phy->supported.channels[0] |= 0x7FFF800;
	/* 868 MHz ASK 802.15.4-2006 */
	hw->phy->supported.channels[1] |= 1;
	/* 915 MHz ASK 802.15.4-2006 */
	hw->phy->supported.channels[1] |= 0x7fe;
	/* 868 MHz O-QPSK 802.15.4-2006 */
	hw->phy->supported.channels[2] |= 1;
	/* 915 MHz O-QPSK 802.15.4-2006 */
	hw->phy->supported.channels[2] |= 0x7fe;
	/* 2.4 GHz CSS 802.15.4a-2007 */
	hw->phy->supported.channels[3] |= 0x3fff;
	/* UWB Sub-gigahertz 802.15.4a-2007 */
	hw->phy->supported.channels[4] |= 1;
	/* UWB Low band 802.15.4a-2007 */
	hw->phy->supported.channels[4] |= 0x1e;
	/* UWB High band 802.15.4a-2007 */
	hw->phy->supported.channels[4] |= 0xffe0;
	/* 750 MHz O-QPSK 802.15.4c-2009 */
	hw->phy->supported.channels[5] |= 0xf;
	/* 750 MHz MPSK 802.15.4c-2009 */
	hw->phy->supported.channels[5] |= 0xf0;
	/* 950 MHz BPSK 802.15.4d-2009 */
	hw->phy->supported.channels[6] |= 0x3ff;
	/* 950 MHz GFSK 802.15.4d-2009 */
	hw->phy->supported.channels[6] |= 0x3ffc00;

	ieee802154_random_extended_addr(&hw->phy->perm_extended_addr);
	/* fake phy channel 13 as default */
	hw->phy->current_channel = 13;
	phy->channel = hw->phy->current_channel;

	hw->flags = IEEE802154_HW_PROMISCUOUS;
	hw->parent = dev;

	err = ieee802154_register_hw(hw);
	if (err)
		goto err_reg;

	mutex_lock(&fakelb_phys_lock);
	list_add_tail(&phy->list, &fakelb_phys);
	mutex_unlock(&fakelb_phys_lock);

	return 0;

err_reg:
	ieee802154_free_hw(phy->hw);
	return err;
}

static void fakelb_del(struct fakelb_phy *phy)
{
	list_del(&phy->list);

	ieee802154_unregister_hw(phy->hw);
	ieee802154_free_hw(phy->hw);
}

static int fakelb_probe(struct platform_device *pdev)
{
	struct fakelb_phy *phy, *tmp;
	int err, i;

	for (i = 0; i < numlbs; i++) {
		err = fakelb_add_one(&pdev->dev);
		if (err < 0)
			goto err_slave;
	}

	dev_info(&pdev->dev, "added %i fake ieee802154 tap device(s)\n", numlbs);
	return 0;

err_slave:
	mutex_lock(&fakelb_phys_lock);
	list_for_each_entry_safe(phy, tmp, &fakelb_phys, list)
		fakelb_del(phy);
	mutex_unlock(&fakelb_phys_lock);
	return err;
}

static int fakelb_remove(struct platform_device *pdev)
{
	struct fakelb_phy *phy, *tmp;

	mutex_lock(&fakelb_phys_lock);
	list_for_each_entry_safe(phy, tmp, &fakelb_phys, list)
		fakelb_del(phy);
	mutex_unlock(&fakelb_phys_lock);
	return 0;
}

static struct platform_device *ieee802154fake_dev;

static struct platform_driver ieee802154fake_driver = {
	.probe = fakelb_probe,
	.remove = fakelb_remove,
	.driver = {
			.name = "ieee802154tap",
	},
};

static int fakelb_init_module(void)
{
	ieee802154fake_dev = platform_device_register_simple(
			     "ieee802154tap", -1, NULL, 0);

	//pr_warn("fakelb driver is marked as deprecated, please use mac802154_hwsim!\n");

	return platform_driver_register(&ieee802154fake_driver);
}

static void fake_remove_module(void)
{
	platform_driver_unregister(&ieee802154fake_driver);
	platform_device_unregister(ieee802154fake_dev);
}


static ssize_t wpantap_chr_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *file = iocb->ki_filp;
	ssize_t len = iov_iter_count(to);
	ssize_t size = 0;
	ssize_t ret;
	int rbempty;
	void *data;

	if(!file){
		return -EBADFD;
	}
	
	printk(KERN_DEBUG "entering read opration\n");

	while(1){
		// busy wait for data
		
		//printk(KERN_DEBUG "executing read busy-wait\n");
		spin_lock_bh(&ringbuf_spin);

		rbempty = ringbuf_is_empty(&rbuf);
		if (rbempty == 0){
			// if there is data to fetch
			printk(KERN_DEBUG "readable buffer found in ring buffer\n");
			size = ringbuf_get_first_data_size(&rbuf);
			if (size == 0){
				printk(KERN_ERR "no data in the buffer to fetch\n");
				goto term;
			}
			
			data = kmalloc(size, GFP_KERNEL);
			ringbuf_copy_first_data(&rbuf, data);
			copy_to_iter(data, size, to);
			kfree(data);
			
			ringbuf_pop_data(&rbuf);
			
			goto term;
		}
		spin_unlock_bh(&ringbuf_spin);
		schedule();
	}
	
	
term:
	spin_unlock_bh(&ringbuf_spin);
	ret = min_t(ssize_t, size, len);
	
	if(ret > 0){
		iocb->ki_pos = ret;
	}
	
	return size;
}

static ssize_t wpantap_chr_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	return 0;
}


static const struct file_operations wpantap_fops = {
	.owner	= THIS_MODULE,
	.llseek = no_llseek,
	.read_iter  = wpantap_chr_read_iter,
	.write_iter = wpantap_chr_write_iter,
	//.poll	= tun_chr_poll,
	//.unlocked_ioctl	= tun_chr_ioctl,
};

static struct miscdevice wpantap_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wpantap",
	.nodename = "net/wpantap",
	.fops = &wpantap_fops,
};


static int file_dev_init(void)
{
	int ret = misc_register(&wpantap_miscdev);
	if(ret != 0){
		printk(KERN_ERR "unable to register misc device\n");
	}
	return ret;
}

static void file_dev_deinit(void)
{
	misc_deregister(&wpantap_miscdev);
}



static __init int wpantap_init(void)
{	
	printk(KERN_DEBUG "prepare to initialize wpantap...\n");
	int err;
	err = fakelb_init_module();
	if(err != 0) goto err_fakelb;

	err = ringbuf_init(&rbuf);
	if(err != 0) goto err_ringbuf;
	
	err = file_dev_init();
	if(err != 0) goto err_miscdev;
	
	printk(KERN_INFO "wpantap started succesfully\n");

	return 0;

err_miscdev:
	file_dev_deinit();
err_ringbuf:
	ringbuf_deinit(&rbuf);
err_fakelb:
	fake_remove_module();
	return err;
}

static __exit void wpantap_deinit(void)
{
	fake_remove_module();
	ringbuf_deinit(&rbuf);
	file_dev_deinit();
	printk(KERN_INFO "wpantap exited succesfully\n");
}

module_init(wpantap_init);
module_exit(wpantap_deinit);
MODULE_LICENSE("GPL");

