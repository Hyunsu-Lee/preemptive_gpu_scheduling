#include <linux/unistd.h>
#include <linux/kernel.h>

asmlinkage long sys_ikernel_hint(void)
{
	printk( "[sys_helloworld] Hello World\n" );

	return 0;
}
