#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include "clink_ev.h"

#define PROC_CLINK	"clink"

static struct proc_dir_entry *clink_entry = NULL;
unsigned long clink_pid;
static struct sock *nl_sk = NULL;
int link_status = 1;

/*
	Definition of pointer to link with kernel space
*/

#ifdef CONFIG_UPDATE_WIFI_LED
extern void (*wifi_led_2g_update)(int, int, int);
extern void (*wifi_led_5g_update)(int, int, int);

#ifdef CONFIG_UPDATE_RSSI_LED
extern void (*update_2g_rssi)(int, int, int);
extern void (*update_5g_rssi)(int, int, int);
#endif
#endif

#ifdef CONFIG_HAVE_PHY_ETHERNET
extern void (*link_wan_st)(char*);
#endif

#ifdef CONFIG_POWER_SAVING
extern void (*rx_wake_up)(void);
#endif

#ifdef CONFIG_UPDATE_WAN_STATUS
extern void (*update_wan_status)(char*);
#endif

static ssize_t clink_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t bytes;
	char pid[20] = {0};
	int pid_len = 0;

	sprintf(pid, "%ld", clink_pid);
	pid_len = strlen(pid);
	bytes = count < (pid_len - (*ppos)) ? count : (pid_len - (*ppos));

	if (copy_to_user(buf, pid, bytes))
		return -EFAULT;

	*ppos += bytes;

	return bytes;
}

static ssize_t clink_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *off)
{
	char *pid;

	pid = (char *)buf;
	clink_pid = simple_strtol(pid, 0, 10);

	return count;
}

static const struct file_operations clink_proc_fops = {
	.owner = THIS_MODULE,
	.write = clink_proc_write,
	.read = clink_proc_read,
};

int CustomizeInit(void)
{
	clink_pid = -1;

	clink_entry = proc_create(PROC_CLINK, S_IRUGO | S_IFREG, NULL, &clink_proc_fops);

	if (!clink_entry) {
		printk("%s : Unable to create clink\n", __FUNCTION__);
		return -ENOENT;
	}

	return 0;
}

static void recvEvent(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	Event_ptr_t event;

	nlh = (struct nlmsghdr*)skb->data;
	event = (Event_ptr_t)NLMSG_DATA(nlh);

	pr_info("Netlink received event id:%d\n", event->id);
	pr_info("Netlink received event length:%d\n", event->length);
	pr_info("Netlink received event payload:%s\n", event->payload);

}

int sendEvent(Event_ptr_t event)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int msg_size = sizeof(Event_t);
	int res;

	if (clink_pid == -1) {
		pr_err("clink is not ready (pid = %ld)\n", clink_pid);
		return -1;
	}

	skb = nlmsg_new(msg_size, 0);

	if (!skb) {
		pr_err("Allocation failure.\n");
		return -1;
	}

	nlh = nlmsg_put(skb, 0, 1, NLMSG_DONE, msg_size, 0);
	memcpy(nlmsg_data(nlh), event, sizeof(Event_t));

	res = nlmsg_unicast(nl_sk, skb, clink_pid);

	if (res < 0)
		pr_info("nlmsg_unicast() error: %d\n", res);

	return res;
}


int detect_rssi_level(int rssi, int *ref_l)
{
	static int level = -1;
	int Rssi_Quality = 0;

	if (rssi >= -50)
		Rssi_Quality = 100;
	else if (rssi >= -80)
		Rssi_Quality = (24 + ((rssi + 80) * 26) / 10);
	else if (rssi >= -90)
		Rssi_Quality = (((rssi + 90) * 26) / 10);
	else
		Rssi_Quality = 0;

	if (Rssi_Quality > 80) {
		if (level == 2 && link_status != 0)
			goto ignore;
		level = 2;
	} else if (Rssi_Quality > 40) {
		if (level == 1 && link_status != 0)
			goto ignore;
		level = 1;
	} else {
		if (level == 0 && link_status != 0)
			goto ignore;
		level = 0;
	}
	link_status = 1;
	*ref_l = level;
	return 0;
ignore:
	return -1;
}

/*
	size < 0 :
	apcli connected or apcli disconnect
		when rssi != 1, that mean apcli connected
		when rssi == 1, that mean apcli disconnect
	size >= 0 :
	first client connected or last client disconnect
		when size == 1, that mean first client connected
		when size == 0, that mean last client disconnect
*/

#ifdef CONFIG_UPDATE_WIFI_LED
void update_wifi_led_hook(int rssi, int channel, int size)
{
	Event_t event;
	int level = -1, res = 0;

	if (rssi != 1)
		res = detect_rssi_level(rssi, &level);

	sprintf(event.payload, "%d,%d,%d", level, channel, size);
	event.id = UPDATE_WIFI_LED;
	event.length = strlen(event.payload);
	if (res != -1 && sendEvent(&event) == -1)
		link_status = 0;	//sendEvent fail, reset to default
}
#endif

#ifdef CONFIG_HAVE_PHY_ETHERNET
void link_wan_st_hook(char* up_down)
{
	Event_t event;

	strcpy(event.payload, up_down);
	event.id = LINK_WAN_ST;
	event.length = strlen(up_down);

	sendEvent(&event);
}
#endif

#ifdef CONFIG_POWER_SAVING
void rx_wake_up_hook(void)
{
	Event_t event;
	event.id = RX_WAKE_UP;
	sendEvent(&event);
}
#endif

#ifdef CONFIG_UPDATE_WAN_STATUS
void update_wan_status_hook(char* up_down)
{
	Event_t event;
	strcpy(event.payload, up_down);
	event.id = UPDATE_WAN_STATUS;
	event.length = strlen(up_down);
	sendEvent(&event);
}
#endif

/*
	[In WIFI driver]:
	size = 1 -> the first client join. (led on)
	size = 0 -> the last client leave. (led off)
		wifi_led_2g_update(1, channel, size);

	rssi < 1 -> ap client is connected.
	rssi = 1 -> ap client is disconnected.
		update_2g_rssi(rssi,channel,-1);
*/
static int __init clink_init(void)
{

	struct netlink_kernel_cfg cfg = {
		.input = recvEvent,
	};

	pr_info("Inserting Cameo Netlink module.\n");

	nl_sk = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &cfg);
	if (!nl_sk) {
		pr_err("Error creating socket.\n");
		return -1;
	}

	CustomizeInit();

#ifdef CONFIG_UPDATE_WIFI_LED
	wifi_led_2g_update = update_wifi_led_hook;
	wifi_led_5g_update = update_wifi_led_hook;
#ifdef CONFIG_UPDATE_RSSI_LED
	update_2g_rssi = update_wifi_led_hook;
	update_5g_rssi = update_wifi_led_hook;
#endif
#endif

#ifdef CONFIG_HAVE_PHY_ETHERNET
	link_wan_st = link_wan_st_hook;
#endif

#ifdef CONFIG_POWER_SAVING
	rx_wake_up = rx_wake_up_hook;
#endif

#ifdef CONFIG_UPDATE_WAN_STATUS
	update_wan_status = update_wan_status_hook;
#endif

	return 0;
}

static void __exit clink_exit(void)
{
	netlink_kernel_release(nl_sk);

	/*remove hook*/
#ifdef CONFIG_UPDATE_WIFI_LED
	wifi_led_2g_update = NULL;
	wifi_led_5g_update = NULL;
#ifdef CONFIG_UPDATE_RSSI_LED
	update_2g_rssi = NULL;
	update_5g_rssi = NULL;
#endif
#endif

#ifdef CONFIG_HAVE_PHY_ETHERNET
	link_wan_st = NULL;
#endif

#ifdef CONFIG_POWER_SAVING
	rx_wake_up = NULL;
#endif

#ifdef CONFIG_UPDATE_WAN_STATUS
	update_wan_status = NULL;
#endif

	remove_proc_entry(PROC_CLINK, NULL);
	pr_info("Exiting Cameo Netlink module.\n");
}

module_init(clink_init);
module_exit(clink_exit);
MODULE_LICENSE("GPL");

