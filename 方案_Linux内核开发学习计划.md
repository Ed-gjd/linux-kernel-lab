# Linux 内核开发学习计划（小白→专家）

- 建立日期：2026-08-13
- 学习协议：遵循《学习方法论与协作协议.md》（命令全明文/先方案后执行/每课验收/不重复/真实环境）
- 主环境：playground VM（Ubuntu 24.04.4，内核 6.8.0-136-generic，3G RAM/2 vCPU，64G 盘）
- 方案副本：本文件（C:\Users\pc\cc\）为副本①；`kernel-lab\方案_Linux内核开发学习计划.md` 为副本②（随 GitHub 仓库 linux-kernel-lab 提交）
- 过程记录：`kernel-lab\实验过程记录.md`；复盘：`kernel-lab\复盘.md`

---

## 〇、深度层级刻度（本计划统一使用）

| 层级 | 含义 |
|---|---|
| L1 知道 | 知道有这个机制/文件/API，能在内核树里找到位置 |
| L2 理解 | 能讲清原理、关键数据结构、调用路径 |
| L3 掌握 | 会用官方 API 写出可运行代码，能读懂对应源码 |
| L4 精通 | 读懂完整实现、能定位 bug、修改行为并验证 |
| L5 主导 | 能设计/评审/贡献，独立负责某个子系统的一块 |

> 驱动编程课 = "用内核 API 写模块"（到 L3）；本计划 = "读懂并改内核本身"（往 L4/L5）。

---

## 一、总路线：7 阶段

| 阶段 | 广度 | 深度 | 验收 |
|---|---|---|---|
| 1 地基与环境 | 内核树结构/Kbuild/boot/用户态分界 | L3 | 自编内核启动+自定义printk/syscall（本计划当前阶段） |
| 2 核心机制 | 进程/调度/同步/中断/时间 | 调度同步L3、RCU L2、中断L3 | 锁保护链表+workqueue/tasklet 延迟任务 |
| 3 内存管理 | buddy/slab/VMA/page fault/vmalloc/cgroup/OOM/swap | L4 | ftrace跟踪mmap→page fault全路径 |
| 4 驱动纵深 | 平台总线/DT/PCI/块层/NAPI/DMA | L4 | 块驱动mkfs+mount、NAPI网卡收发ping |
| 5 子系统专精 | 网络栈(sk_buff/netfilter/TCP/eBPF/XDP) 或 VFS(page cache/io_uring) | 一个到L4/L5，另一个L3 | netfilter/XDP 真实流量 或 VFS级文件系统 |
| 6 调试性能稳定 | ftrace/kprobes/perf/lockdep/KASAN/kgdb/kdump+crash | L4 | 制造oops→kdump→crash分析根因 |
| 7 专家层 | LSM/caps/SELinux、namespace+cgroup、KVM、ARM64/RISC-V、RT、上游贡献 | L4-L5 | 上游提交真补丁或独立安全实验 |

时间预估：阶段1-6 约 4-6 个月；阶段7 长期。

---

## 二、阶段 1 里程碑详案：编译自定义内核 → 装进 VM → 自定义 printk/syscall

### 目标
自己从源码编译内核 → 装进 playground VM 并启动 → 在内核源码加自定义 printk + syscall → 重编重装 → 用户态验证。

### 版本选择（为何选 linux-6.8.12）
| 候选 | 与运行内核(6.8.0-136)关系 | 结论 |
|---|---|---|
| **6.8.12** | 同 major.minor，几乎同代 | `/proc/config.gz` 打底 + olddefconfig 几乎零漂移，**首次开机成功率最高** |
| 6.12.103 LTS | 差一个大版本 | 配置漂移更大，后续阶段再迁 |
| 6.19.14 | 差三个大版本 | 漂移大，不适合首次全量编译 |

口诀：**首次编内核 = 挑与正在运行内核同 major.minor 的上游 stable 版**。
源码下载走 TUNA 清华镜像直连（宿主实测 200 OK），不赌 VM 直连 kernel.org。

### 步骤 0｜准备
1. 打基线快照（VMware UI 或 vmrun）：`kernel-0-baseline`
2. 内存已 3G（vmx 实测 memsize=3072），无需提内存；`-j2` 编译
3. 装编译依赖（VM 内）：`sudo apt install -y libssl-dev libelf-dev flex bison bc cpio kmod libncurses-dev`
4. 确认 SecureBoot：`mokutil --sb-state`，enabled 则进 EFI 固件关掉（自制无签名内核会被拦）

### 步骤 1｜下载源码
```bash
cd ~ && mkdir -p kernel-lab && cd kernel-lab
wget https://mirrors.tuna.tsinghua.edu.cn/kernel/v6.x/linux-6.8.12.tar.xz
# 校验（从 TUNA 拿 sha256sum 核对）后解压
tar -xJf linux-6.8.12.tar.xz && cd linux-6.8.12
```

### 步骤 2｜配置
```bash
zcat /proc/config.gz > .config
scripts/config --disable DEBUG_INFO --disable DEBUG_INFO_BTF   # 省 ~10G 磁盘
make olddefconfig
```

### 步骤 3｜编译
```bash
make -j2        # 2 vCPU 匹配，3G 内存稳；首次全量约 15-40 分钟
```

### 步骤 4｜安装+引导+验证
```bash
sudo make modules_install
sudo make install
sudo update-initramfs -c -k $(make kernelrelease)
sudo update-grub
sudo reboot
uname -r        # 应显示 6.8.12
sudo dmesg | grep -i "linux version"
```

### 步骤 5｜自定义 printk
`init/main.c` 的 `pr_notice("%s", linux_banner);` 后加：
```c
pr_info("MY-KERNEL: 我自己编译的内核启动成功\n");
```
增量重编 → 装 → 重启 → `sudo dmesg | grep MY-KERNEL`

### 步骤 6｜自定义 syscall（452 hello）
① `arch/x86/entry/syscalls/syscall_64.tbl` 追加：
```
452	common	hello			sys_hello
```
② 新建 `kernel/hello.c`：
```c
// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(hello)
{
    pr_info("hello: 自定义syscall被调用, jiffies=%lu\n", jiffies);
    return 42;
}
```
③ `include/linux/syscalls.h` 加原型：`asmlinkage long sys_hello(void);`
④ `kernel/Makefile` 加：`obj-y += hello.o`
⑤ 重编重装重启，用户态验证：
```c
#include <stdio.h>
#include <unistd.h>
int main(void) { printf("syscall(452) returned: %ld\n", syscall(452)); return 0; }
```
预期：返回 42；`dmesg | grep hello` 有内核日志。

### 必踩的坑
SecureBoot 拦无签名内核 / 1G 内存 OOM(已3G避开) / DEBUG_INFO 不关爆盘 / 依赖缺失 / 重启没网络(配置要拿现有config打底) / dmesg 需 sudo / syscall 号撞车(开跑时核对) / 忘加原型报警告

### 练习
1. 改 printk 级别验证 console_loglevel 对应关系
2. `SYSCALL_DEFINE1(hello, int, val)` 传参并返回 val*2
3. strace 观察
4. kprobe 挂 `__x64_sys_hello`

---

## 三、环境实测基线（2026-08-13，勿重复探测）

### VM 参数（vmx 实测）
| 项 | 值 |
|---|---|
| 路径 | D:\VM\playground\playground.vmx |
| 内存/CPU | memsize=3072 / numvcpus=2 |
| 固件 | firmware=efi（UEFI，含 .nvram）→ SecureBoot 待查 |
| 磁盘 | 64G（125829120×512），host 侧实占 13G |
| 网卡 | eth0=NAT(e1000, enp0s17→192.168.169.66)；eth1=VMnet2 仍在 vmx（OS 层已砍） |
| 内核 | 6.8.0-136-generic |

### 网络三层实测
| 层 | 结果 |
|---|---|
| 宿主 | WLAN 192.168.31.77/24 网关 192.168.31.1；直连 baidu 200 / kernel.org ✗ / github ✗ |
| 本机代理 | mixed 0.0.0.0:10080、socks 0.0.0.0:10081 监听；走代理 kernel.org 200 ✓ |
| WSL | Ubuntu-24.04 WSL2 mirrored，代理 env 已配，kernel.org 走代理 200 ✓ |
| TUNA | mirrors.tuna.tsinghua.edu.cn/kernel/ 直连 200 ✓（有 6.8.12/6.12.103/6.19.14） |

**网络决策**：源包下载走 TUNA 直连；保底本机代理。

---

## 四、阶段 1 验收清单

1. `uname -r` = 6.8.12（自定义），SSH 正常
2. enp0s17 = 192.168.169.66，ping 通网关/宿主
3. `sudo dmesg | grep MY-KERNEL` 有自定义 printk
4. `/tmp/hello_test` 输出 `syscall(452) returned: 42`，`dmesg | grep hello` 有日志
5. grub 旧内核 6.8.0-136-generic 仍在可回滚
6. `df -h /` 编译后余量 ≥5G
7. SecureBoot 已确认处置（关或签名）

## 五、阶段 1 清理清单

- 删 `~/kernel-lab` 源码+build（回收 ~10G）——保留实验物确认后再删
- 删 TUNA 下载的 .tar.xz 缓存
- 自定义内核镜像：保留（继续学习）或移除（boot 旧内核→rm /boot 相关+rm /lib/modules/6.8.12+update-grub）
- 打 `kernel-course-done` 快照；基线快照保留
- 确认 VM 优雅关机、无残留 .lck

## 六、回滚方案
- 软回滚：grub 选旧内核 / `grub-set-default`
- 硬回滚：VM 快照还原
- 内核实验全程高危：每改源码/每重启前打新快照（kernel-0-baseline → kernel-1-printk → kernel-2-syscall → kernel-3-done）

## 七、后续阶段预留
阶段 2 核心机制 → 3 内存管理 → 4 驱动纵深 → 5 子系统专精 → 6 调试 → 7 专家层。每阶段结束回流更新本文件 + 写复盘。
