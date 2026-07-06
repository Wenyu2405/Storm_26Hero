#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x8a35b432, "sme_me_mask" },
	{ 0x587f22d7, "devmap_managed_key" },
	{ 0x1e2abf24, "get_user_pages_fast" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xb4088770, "skb_copy_bits" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x48d88a2c, "__SCT__preempt_schedule" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x75646747, "class_destroy" },
	{ 0x69acdf38, "memcpy" },
	{ 0x94961283, "vunmap" },
	{ 0x37a0cba, "kfree" },
	{ 0xb8aff722, "pcpu_hot" },
	{ 0x7292dfa7, "__put_devmap_managed_page_refs" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xe2964344, "__wake_up" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0xcc5005fe, "msleep_interruptible" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xe0f6dc77, "wake_up_process" },
	{ 0x122c3a7e, "_printk" },
	{ 0x8427cc7b, "_raw_spin_lock_irq" },
	{ 0x8ddd8aad, "schedule_timeout" },
	{ 0x1000e51, "schedule" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xe46021ca, "_raw_spin_unlock_bh" },
	{ 0x6383b27c, "__x86_indirect_thunk_rdx" },
	{ 0x8c6558ad, "__free_pages" },
	{ 0x7cd8d75e, "page_offset_base" },
	{ 0xec957a9, "cdev_add" },
	{ 0xbcb36fe4, "hugetlb_optimize_vmemmap_key" },
	{ 0xd9df1ab9, "nf_unregister_net_hook" },
	{ 0xfe5d4bb2, "sys_tz" },
	{ 0x5e782faf, "vm_insert_page" },
	{ 0x96a072b6, "nf_register_net_hook" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x8c75b508, "init_net" },
	{ 0x2e3443fd, "device_create" },
	{ 0x6ca9b86a, "class_create" },
	{ 0xf1969a8e, "__usecs_to_jiffies" },
	{ 0x4c03a563, "random_kmalloc_seed" },
	{ 0x30b86e62, "kfree_skb_reason" },
	{ 0x4b750f53, "_raw_spin_unlock_irq" },
	{ 0xdf249575, "vmap" },
	{ 0x9ec6ca96, "ktime_get_real_ts64" },
	{ 0x2ef1b23, "kthread_stop" },
	{ 0xd38cd261, "__default_kernel_pte_mask" },
	{ 0xfb578fc5, "memset" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x356461c8, "rtc_time64_to_tm" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0x53be26d8, "kthread_bind" },
	{ 0x1f337bd7, "kthread_create_on_node" },
	{ 0x97651e6c, "vmemmap_base" },
	{ 0x999e8297, "vfree" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0xdea9ed44, "alloc_pages" },
	{ 0x9cbc9023, "__folio_put" },
	{ 0x19edaabb, "device_destroy" },
	{ 0x5d3b8025, "skb_copy" },
	{ 0xc3690fc, "_raw_spin_lock_bh" },
	{ 0xd0c3484c, "kmalloc_trace" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0xf9a482f9, "msleep" },
	{ 0x22d6de43, "cdev_init" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0x1bff00c8, "kmalloc_caches" },
	{ 0xb1b9cfc9, "cdev_del" },
	{ 0x58d0784e, "cdev_alloc" },
	{ 0xe2fd41e5, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "C74240144EA0223B4A35542");
