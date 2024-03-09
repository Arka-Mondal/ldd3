/*
 * main.c -- the scull char module
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>         // printk()
#include <linux/slab.h>           // kmalloc()
#include <linux/fs.h>
#include <linux/errno.h>          // error codes
#include <linux/types.h>
#include <linux/fcntl.h>          // O_ACCMODE
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <linux/uaccess.h>        // copy_(from|to)_user

#include "scull.h"
#include "proc_ops_version.h"     // proc_ops_wrapper() - macro

// parameters which can be set at load time
unsigned int scull_major = SCULL_MAJOR;
unsigned int scull_minor = 0;
unsigned int scull_nr_devs = SCULL_NR_DEVS;
unsigned int scull_quantum = SCULL_QUANTUM;
unsigned int scull_qset = SCULL_QSET;


module_param(scull_major, uint, S_IRUGO);
module_param(scull_minor, uint, S_IRUGO);
module_param(scull_nr_devs, uint, S_IRUGO);
module_param(scull_quantum, uint, S_IRUGO);
module_param(scull_qset, uint, S_IRUGO);

struct scull_dev * scull_devices;

/*
 * scull_trim - empty out the scull device; must be called with
 * the deice lock held.
 * @dev:        scull device
 *
 * Return:
 * always return 0.
 */

int scull_trim(struct scull_dev * dev)
{
  unsigned int qset, i;
  struct scull_qset * next, * curr;

  qset = dev->qset;

  for (curr = dev->data; curr != NULL; curr = next)
  {
    if (curr->data != NULL)
    {
      for (i = 0; i < qset; i++)
        kfree(curr->data[i]);
      kfree(curr->data);
      curr->data = NULL;
    }

    next = curr->next;
    kfree(curr);
  }

  dev->size = 0;
  dev->quantum = scull_quantum;
  dev->qset = scull_qset;
  dev->data = NULL;

  return 0;
}

#ifdef SCULL_DEBUG    // use proc file only if in debugging mode

/*
 * Sequnce iteration methods. The "position" is simply
 * the scull device number.
 */

static void * scull_seq_start(struct seq_file * sfile, loff_t * pos)
{
  if (*pos >= scull_nr_devs)
    return NULL;

  return scull_devices + *pos;
}

static void * scull_seq_next(struct seq_file * sfile, void * v, loff_t * pos)
{
  (*pos)++;

  if (*pos >= scull_nr_devs)
    return NULL;

  return scull_devices + *pos;
}

static int scull_seq_show(struct seq_file * sfile, void * v)
{
  int i;
  struct scull_dev * dev;
  struct scull_qset * qset;

  dev = (struct scull_dev *) v;

  if (mutex_lock_interruptible(&dev->mtx_lock))
    return -ERESTARTSYS;

  seq_printf(sfile, "Device %u: qset: %u, quantum: %u, size: %lu\n",
              (unsigned int) (dev - scull_devices), dev->qset, dev->quantum, dev->size);

  for (qset = dev->data; qset != NULL, qset = qset->next)
  {
    seq_printf(sfile, "\titem at %p, qset %p\n", qset, qset->data);

    if ((qset->data != NULL) && (qset->next == NULL)) // dump only the last item
    {
      for (i = 0; i < dev->qset; i++)
      {
        if (qset->data[i] != NULL)
          seq_printf(sfile, "\t\t%4d: %8p\n", i, dev->data[i]);
      }
    }
  }

  seq_putc(sfile, '\n');
  mutex_unlock(&dev->mtx_lock);

  return 0;
}

static void scull_seq_stop(struct seq_file * sfile, void * v)
{
  return;
}

// Connect the sequnce operators
static struct seq_operations scull_seq_ops = {
  .start = scull_seq_start,
  .next = scull_seq_next,
  .show = scull_seq_show,
  .stop = scull_seq_stop
};


/*
 * To implement the /proc file we need only to make
 * an open method which sets up the sequnce operators.
 */

static int scullseq_proc_open(struct inode * inode, struct file * file)
{
  return seq_open(file, &scull_seq_ops);
}

// Add a set of file operations to the proc files.
static struct file_operations scullseq_proc_ops = {
  .owner        = THIS_MODULE,
  .open         = scullseq_proc_open,
  .read         = seq_read,
  .llseek       = seq_lseek,
  .release      = seq_release
};

/*
 * Create the actual /proc file.
 */

static void scull_create_proc(void)
{
  proc_create("scullseq", 0 /* default mode */, NULL /* parent dir */,
                proc_ops_wrapper(&scullseq_proc_ops, scullseq_proc_ops_n));
}

/*
 * Remove the /proc file
 */

static void scull_remove_proc(void)
{
  // no problem if it was not previously registered
  remove_proc_entry("scullseq", NULL /* parent dir */);
}

#endif /* SCULL_DEBUG */

/*
 * scull_open - open the scull device
 * @inode:      inode structure for that device
 * @flip:       file pointer to the special "device file" for that device
 *
 * Return:
 * 0 on success or appropriate errno value on error.
 */

int scull_open(struct inode * inode, struct file * flip)
{
  struct scull_dev * dev;       // device information

  dev = container_of(inode->i_cdev, struct scull_dev, cdev);
  flip->private_data = dev;     // save the pointer for other methods

  // trim the length of the device to 0 , if it was open was write-only
  if ((flip->f_flags & O_ACCMODE) == O_WRONLY)
  {
    if (mutex_lock_interruptible(&dev->mtx_lock))
      return -ERESTARTSYS;

    scull_trim(dev);
    mutex_unlock(&dev->mtx_lock);
  }

  return 0;
}

/*
 * scull_open - release the scull device (does nothing)
 * @inode:      inode structure for that device
 * @flip:       file pointer to the special "device file" for that device
 *
 * Return:
 * always return 0.
 */

int scull_release(struct inode * inode, struct file * flip)
{
  return 0;
}

/*
 * scull_follow - follow the list and return the nth element in the linked-list
 * @dev         scull device
 * @n           index of the list [0..)
 *
 * Return:
 * address of nth element on success or NULL on error.
 */

struct scull_qset * scull_follow(struct scull_dev * dev, unsigned long n)
{
  struct scull_qset * qset;

  qset = dev->data;

  // allocate the first qset if needed
  if (qset == NULL)
  {
    qset = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
    if (qset == NULL)
      return NULL;

    dev->data = qset;

    SCULL_QSET_INIT(qset);
  }

  // then follow the list
  for (; n > 0; n--)
  {
    if (qset->next == NULL)
    {
      qset->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
      if (qset->next == NULL)
        return NULL;

      SCULL_QSET_INIT(qset->next);
    }

    qset = qset->next;
  }

  return qset;
}

/*
 * scull_read -   read data from the device
 * @flip:         file pointer to the special "device file" for that device
 * @buf           buffer (userspace)
 * @count:        amount of data requested
 * @f_pos:        current offset
 *
 * Return:
 * number of bytes read on success or appropriate errno value on error.
 */
ssize_t scull_read(struct file * flip, char __user * buf, size_t count,
                    loff_t * f_pos)
{
  unsigned int quantum, qset;
  unsigned long itemsize, item, qindx, qoff, rest;
  ssize_t retval;
  struct scull_dev * dev;
  struct scull_qset * qsetp;

  dev = flip->private_data;
  quantum = dev->quantum;
  qset = dev->qset;
  itemsize = quantum * qset;
  retval = 0;

  if (mutex_lock_interruptible(&dev->mtx_lock))
    return -ERESTARTSYS;

  if (*f_pos >= dev->size)
    goto done;
  if (*f_pos + count > dev->size)
    count = dev->size - *f_pos;

  // find listitem, qset index and offset in that quantum
  item = (unsigned long) *f_pos / itemsize;
  rest = (unsigned long) *f_pos % itemsize;
  qindx = rest / quantum;
  qoff = rest % quantum;

  // follow the list up to the right position
  qsetp = scull_follow(dev, item);

  if ((qsetp == NULL) || (qsetp->data == NULL) || (qsetp->data[qindx] == NULL))
    goto done;

  // read only up to the end of this quantum
  if (count > quantum - qoff)
    count = quantum - qoff;

  if (copy_to_user(buf, qsetp->data[qindx] + qoff, count))
  {
    retval = -EFAULT;
    goto done;
  }

  *f_pos += count;
  retval = count;

done:
  mutex_unlock(&dev->mtx_lock);
  return retval;
}

/*
 * scull_write -  write data to the device
 * @flip:         file pointer to the special "device file" for that device
 * @buf           buffer (userspace)
 * @count:        amount of data requested
 * @f_pos:        current offset
 *
 * Return:
 * number of bytes written on success or appropriate errno value on error.
 */

ssize_t scull_write(struct file * flip, const char __user * buf, size_t count,
                   loff_t * f_pos)
{
  unsigned int quantum, qset;
  unsigned long itemsize, item, qindx, qoff, rest;
  ssize_t retval;
  struct scull_dev * dev;
  struct scull_qset * qsetp;

  dev = flip->private_data;
  quantum = dev->quantum;
  qset = dev->qset;
  itemsize = quantum * qset;
  retval = -ENOMEM;

  if (mutex_lock_interruptible(&dev->mtx_lock))
    return -ERESTARTSYS;

  // find listitem, qset index and offset in that quantum
  item = (unsigned long) *f_pos / itemsize;
  rest = (unsigned long) *f_pos % itemsize;
  qindx = rest / quantum;
  qoff = rest % quantum;

  // follow the list up to the right position
  qsetp = scull_follow(dev, item);
  if (qsetp == NULL)
    goto done;

  if (qsetp->data == NULL)
  {
    qsetp->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
    if (qsetp->data == NULL)
      goto done;

    memset(qsetp->data, 0, qset * sizeof(char *));
  }

  if (qsetp->data[qindx] == NULL)
  {
    qsetp->data[qindx] = kmalloc(quantum * sizeof(char), GFP_KERNEL);
    if (qsetp->data[qindx] == NULL)
      goto done;
  }

  // writes only up to the end of this quantum
  if (count > quantum - qoff)
    count = quantum - qoff;

  if (copy_from_user(qsetp->data[qindx] + qoff, buf, count))
  {
    retval = -EFAULT;
    goto done;
  }

  *f_pos += count;
  retval = count;

  // update the size
  if (dev->size < *f_pos)
    dev->size = *f_pos;

done:
  mutex_unlock(&dev->mtx_lock);
  return retval;
}

/*
 * scull_llseek - changes the offset of the device
 * @flip:         file pointer to the special "device file" for that device
 * @buf           given offset
 * @whence:       condition for the offset changing
 *
 * Return:
 * new offset on success or appropriate errno value on error.
 */

loff_t scull_llseek(struct file * flip, loff_t off, int whence)
{
  struct scull_dev * dev;
  loff_t newpos;

  dev = flip->private_data;

  switch (whence)
  {
    case 0:   // SEEK_SET
      newpos = off;
      break;
    case 1:   // SEEK_CUR
      newpos = flip->f_pos + off;
      break;
    case 2:   // SEEK_END
      newpos = dev->size + off;
      break;
    default:  // should never hanppen
      return -EINVAL;
  }

  if (newpos < 0)
    return -EINVAL;
  flip->f_pos = newpos;

  return newpos;
}

struct file_operations scull_fops = {
  .owner        = THIS_MODULE,
  .llseek       = scull_llseek,
  .read         = scull_read,
  .write        = scull_write,
  .open         = scull_open,
  .release      = scull_release
};

/*
 * scull_setup_cdev - setup char_dev structure for a device
 * @dev:        scull device
 * @index:      index of @dev in "scull_devices" array
 */

static void scull_setup_cdev(struct scull_dev * dev, unsigned int index)
{
  int err, devno;

  devno = MKDEV(scull_major, scull_minor + index);

  cdev_init(&dev->cdev, &scull_fops);
  dev->cdev.owner = THIS_MODULE;
  err = cdev_add(&dev->cdev, devno, 1);
  if (err)
    printk(KERN_NOTICE "SCULL Error %d: adding scull%d\n", err, index);
}

/*
 * here cleanup module is used to deal with initialization
 * failures too.
 */

static void scull_cleanup_module(void)
{
  unsigned int i;
  dev_t devno;

  devno = MKDEV(scull_major, scull_minor);

  if (scull_devices != NULL)
  {
    for (i = 0; i < scull_nr_devs; i++)
    {
      scull_trim(scull_devices + i);
      cdev_del(&scull_devices[i].cdev);
    }

    kfree(scull_devices);
  }

#ifdef SCULL_DEBUG
  scull_remove_proc();
#endif

  // cleanup_module is never called if registering failed
  unregister_chrdev_region(devno, scull_nr_devs);
}

/*
 * scull_init_module - module initialization function
 */

static int __init scull_init_module(void)
{
  int result;
  unsigned int i;
  dev_t devno;

  if (scull_major)
  {
    devno = MKDEV(scull_major, scull_minor);
    result = register_chrdev_region(devno, scull_nr_devs, "scull");
  }
  else
  {
    result = alloc_chrdev_region(&devno, scull_minor, scull_nr_devs, "scull");
    scull_major = MAJOR(devno);
  }

  if (result < 0)
  {
    printk(KERN_WARNING "SCULL: can't get marjor %d\n", scull_major);
    return result;
  }

  scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
  if (scull_devices == NULL)
  {
    result = -ENOMEM;
    goto failed;
  }

  memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

  for (i = 0; i < scull_nr_devs; i++)
  {
    scull_devices[i].quantum = scull_quantum;
    scull_devices[i].qset = scull_qset;
    mutex_init(&scull_devices[i].mtx_lock);
    scull_setup_cdev(scull_devices + i, i);
  }

#ifdef SCULL_DEBUG
  scull_create_proc();
#endif

  return 0;

failed:
  scull_cleanup_module();
  return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);

MODULE_AUTHOR("Arka Mondal");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple memory based char deivce");
