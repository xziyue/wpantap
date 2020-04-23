// SPDX-License-Identifier: GPL-2.0-only
/*
 * WPAN TAP interface
 *
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
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


static int ringbuf_bytes_free(struct ringbuf_t *rb)
{
	if(rb->head >= rb->tail){
		return rb->capacity - (rb->head - rb->tail);
	}else{
		return rb->tail - rb->head - 1;
	}
}

static int ringbuf_bytes_used(struct ringbuf_t *rb)
{
	return rb->capacity - ringbuf_bytes_free(rb);
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
	return rb->buf + ((anchor - rb->buf) + offset) % rb->size;
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
	
	cdata = (char*)data;
	total_size = sizeof(int) + size;
	rbtail = rb->tail;
	
	if(total_size > rb->capacity){
		printk(KERN_ERR "the total size of data (%d) is bigger than the capacity of ring buffer (%d)\n", total_size, rb->capacity);
		return 1;
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
	rb->tail = (void*)rbdata;

	return 0;
}


static void ringbuf_pop_data(struct ringbuf_t *rb)
{	
	int *size;
	int total_size;
	
	if(ringbuf_is_empty(rb) == 1){
		return;
	}
	
	// find the length of current buffer
	size = rb->head;
	total_size = *size + sizeof(int);
	
	rb->head = ringbuf_ll(rb, rb->head, total_size);
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

	read_lock_bh(&fakelb_ifup_phys_lock);
	WARN_ON(current_phy->suspended);
	list_for_each_entry(phy, &fakelb_ifup_phys, list_ifup) {
		if (current_phy == phy)
			continue;

		if (current_phy->page == phy->page &&
		    current_phy->channel == phy->channel) {
			struct sk_buff *newskb = pskb_copy(skb, GFP_ATOMIC);

			if (newskb)
				ieee802154_rx_irqsafe(phy->hw, newskb, 0xcc);
		}
	}
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


static __init int wpantap_init(void)
{	
	int err;
	err = fakelb_init_module();
	if(err != 0){
		return err;
	}

	err = ringbuf_init(&rbuf);
	if(err != 0){
		return err;
	}

	return 0;
}

static __exit void wpantap_deinit(void)
{
	fake_remove_module();
	ringbuf_deinit(&rbuf);
}

module_init(wpantap_init);
module_exit(wpantap_deinit);
MODULE_LICENSE("GPL");

