/*
 * proc_ops_version.h -- header file containing the helper macros for scull drivers
 *
 * Copyright (C) 2024  Arka Mondal

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _SCULL_PROC_OPS_VERSION_
#define _SCULL_PROC_OPS_VERSION_

#include <linux/version.h>

#ifdef CONFIG_COMPAT
#define __scull_add_proc_ops_compat(fops, nfops) ((nfops)->proc_compat_ioctl = (fops)->compat_ioctl)
#else
#define __scull_add_proc_ops_compat(fops, nfops)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
#define proc_ops_wrapper(fops, nfops) (fops)
#else
#define proc_ops_wrapper(fops, nfops)                         \
({                                                            \
  static struct proc_ops nfops;                               \
                                                              \
  nfops.proc_open = (fops)->open;                             \
  nfops.proc_read = (fops)->read;                             \
  nfops.proc_read_iter = (fops)->read_iter;                   \
  nfops.proc_write = (fops)->write;                           \
  nfops.proc_release = (fops)->release;                       \
  nfops.proc_poll = (fops)->poll;                             \
  nfops.proc_ioctl = (fops)->unlocked_ioctl;                  \
  nfops.proc_mmap = (fops)->mmap;                             \
  nfops.proc_get_unmapped_area = (fops)->get_unmapped_are;    \
  nfops.proc_lseek = (fops)->llseek;                          \
  __scull_add_proc_ops_compat(fops, &nfops);                  \
  &nfops;                                                     \
})
#endif

#endif /* _SCULL_PROC_OPS_VERSION_ */
