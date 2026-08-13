// SPDX-License-Identifier: GPL-2.0
/*
 * 自定义系统调用 hello（syscall 452）
 * 文件位置: kernel/hello.c（新增）
 * 挂进构建: kernel/Makefile 加 obj-y += hello.o
 *
 * 说明:
 * - SYSCALL_DEFINE0 宏会生成 __x64_sys_hello（64位入口）和 __ia32_sys_hello（compat）
 *   两个 wrapper，真正函数体是 __do_sys_hello
 * - 返回 42；用户态 syscall(452) 拿到 42
 * - pr_info 写进内核日志，dmesg 可查
 */
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(hello)
{
	pr_info("hello: 自定义syscall被调用, jiffies=%lu\n", jiffies);
	return 42;
}
