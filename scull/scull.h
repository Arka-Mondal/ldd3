/*
 * scull.h -- header file containing the definitions for scull drivers
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

#ifndef _SCULL_H_
#define _SCULL_H_

#ifdef SCULL_DEBUG
# ifdef __KERNEL__
#   define PDEBUG(fmt, ...) printk(KERN_DEBUG, "scull: " fmt __VA_OPT__(,) __VA_ARGS__)
# else
#   define PDEBUG(fmt, ...) fprintf(stderr, fmt __VA_OPT__(,) __VA_ARGS__)
# endif
#else
# define PDEBUG(fmt, ...)
#endif

#define PDEBUG_D(fmt, ...)


#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0     // dynamic major by default
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4   // i.e. scull0 -- scull3
#endif

/*
 * Each scull device is a variable-length region of memory. It uses
 * a linked-list of indirect blocks of memory (quantum).
 *
 * "scull_dev->data" points to the first quantum set (first scull_qset
 * of the linked-list). "scull_qset->data" points to an array of pointers,
 * each pointer points to a memory region of "SCULL_QUANTUM" bytes.
 * The array is SCULL_QSET long.
 */

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000  // 4096
#endif

#ifndef SCULL_QSET
#define SCULL_QSET 1000     // 1024
#endif

// scull quantum set
struct scull_qset {
  void ** data;
  struct scull_qset * next;
};

struct scull_dev {
  struct scull_qset * data; // pinter to the first quantum set
  unsigned int quantum;    // the current quantum size
  unsigned int qset;       // the current array size
  unsigned long size;       // the amount of data stored in this deivce
  unsigned int access_key;  // used by sculluid and scullpriv
  struct mutex mtx_lock;    // mutual exclusion lock
  struct cdev cdev;         // char device structure
};

#define SCULL_QSET_INIT(QSET) ((QSET)->data = NULL, (QSET)->next = NULL)

// defined in main.c
extern unsigned int scull_major;
extern unsigned int scull_nr_devs;
extern unsigned int scull_quantum;
extern unsigned int scull_qset;

// function prototype
int scull_trim(struct scull_dev * dev);
ssize_t scull_read(struct file *, char __user *, size_t, loff_t *);
ssize_t scull_write(struct file *, const char __user *, size_t, loff_t *);
loff_t scull_llseek(struct file *, loff_t, int);

#endif /* _SCULL_H_ */
