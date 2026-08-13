// 用户态测试：调用自定义 syscall 452 (hello)
// 编译: gcc -o /tmp/hello_test hello_test.c
// 运行: /tmp/hello_test    → 期望 syscall(452) returned: 42
// 说明: glibc 不认识新 syscall，直接用数字 452 调 syscall(2)
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	long r = syscall(452);   /* __NR_hello */
	printf("syscall(452) returned: %ld\n", r);
	return 0;
}
