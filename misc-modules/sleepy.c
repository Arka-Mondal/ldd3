/*
 * sleepy.c -- a loadable module used to demonstrate simple sleep in Linux
 *
 * Copyright (C) 2024 Arka Mondal

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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>


unsigned int sleepy_major = 0;
unsigned int sleepy_minor = 0;
struct cdev *sleepy_cdev = NULL;

module_param(sleepy_major, uint, S_IRUGO);
module_param(sleepy_minor, uint, S_IRUGO);

static DECLARE_WAIT_QUEUE_HEAD(wq);
static DEFINE_MUTEX(mtx);
static int flag = 0;

int sleepy_open(struct inode *inode, struct file *flip)
{
  return 0;
}

int sleepy_release(struct inode *inode, struct file *flip)
{
  return 0;
}

ssize_t sleepy_read(struct file *flip, char __user *buf, size_t count,
                    loff_t *f_pos)
{
  if (mutex_lock_interruptible(&mtx))
    return -ERESTARTSYS;

  while (flag == 0)
  {
    mutex_unlock(&mtx);
    printk(KERN_DEBUG "process: %i (%s) is going to sleep\n", current->pid, current->comm);
    if (wait_event_interruptible(wq, flag != 0))
      return -ERESTARTSYS;
    // reacquire the mutex first, then check the condition again
    if (mutex_lock_interruptible(&mtx))
      return -ERESTARTSYS;
  }

  flag = 0;
  printk(KERN_DEBUG "process: %i (%s) awoken\n", current->pid, current->comm);
  mutex_unlock(&mtx);

  return count;
}

ssize_t sleepy_write(struct file *flip, const char __user *buf, size_t count,
                     loff_t *f_pos)
{
  if (mutex_lock_interruptible(&mtx))
    return -ERESTARTSYS;

  flag = 1;
  printk(KERN_DEBUG "process: %i (%s) awakening the readers...\n", current->pid, current->comm);
  wake_up_interruptible(&wq);
  mutex_unlock(&mtx);

  return count;
}

struct file_operations sleepy_fops = {
  .owner        = THIS_MODULE,
  .read         = sleepy_read,
  .write        = sleepy_write,
  .open         = sleepy_open,
  .release      = sleepy_release
};

static void sleepy_cleanup_module(void)
{
  dev_t devno;

  devno = MKDEV(sleepy_major, sleepy_minor);

  if (sleepy_cdev != NULL)
  {
    cdev_del(sleepy_cdev);
    kfree(sleepy_cdev);
  }

  unregister_chrdev_region(devno, 1);
}

static int __init sleepy_init_module(void)
{
  int result;
  dev_t devno;

  if (sleepy_major)
  {
    devno = MKDEV(sleepy_major, sleepy_minor);
    result = register_chrdev_region(devno, 1, "sleepy");
  }
  else
  {
    result = alloc_chrdev_region(&devno, sleepy_minor, 1, "sleepy");
    sleepy_major = MAJOR(devno);
  }

  if (result < 0)
  {
    printk(KERN_WARNING "SLEEPY: can't get marjor %d\n", sleepy_major);
    return result;
  }

  sleepy_cdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
  if (sleepy_cdev == NULL)
  {
    result = -ENOMEM;
    goto failed;
  }

  cdev_init(sleepy_cdev, &sleepy_fops);
  sleepy_cdev->owner = THIS_MODULE;
  result = cdev_add(sleepy_cdev, devno, 1);
  if (result)
    goto failed;

  return 0;

failed:
  sleepy_cleanup_module();
  return result;
}

module_init(sleepy_init_module);
module_exit(sleepy_cleanup_module);

MODULE_AUTHOR("Arka Mondal");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple memory based char deivce to demonstrate simple sleep in Linux");
