#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100
#define u128_l 128

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static ktime_t kt;


struct u128 {
    int num[u128_l];
};

static inline void init_list(struct u128 *n, int num)
{
    int i = 0;
    memset(n->num, 0, u128_l * sizeof(int));
    while (num > 0) {
        n->num[i] = num % 10;
        i++;
        num /= 10;
    }
}

static inline void u128_mul(struct u128 *a, struct u128 *b, struct u128 *output)
{
    int *mcand = b->num;
    int *mplier = a->num;
    int c_in = 0;

    memset(output->num, 0, u128_l * sizeof(int));

    for (int i = 0; i < u128_l; i++) {
        for (int j = 0; j < u128_l; j++) {
            if (i + j > u128_l - 1)
                break;
            int mul = mplier[i] * mcand[j] + c_in + output->num[i + j];
            c_in = mul / 10;
            output->num[i + j] = mul % 10;
        }
    }
}

static inline void u128_sub(struct u128 *a, struct u128 *b, struct u128 *output)
{
    for (int i = 0; i < u128_l; i++) {
        int diff = a->num[i] - b->num[i];
        if (diff < 0) {
            diff += 10;
            if (i + 1 < u128_l)
                a->num[i + 1] -= 1;
        }
        output->num[i] = diff;
    }
}

static inline void u128_add(struct u128 *a, struct u128 *b, struct u128 *output)
{
    int c_in = 0;
    for (int i = 0; i < u128_l; i++) {
        int sum = a->num[i] + b->num[i] + c_in;
        c_in = sum / 10;
        output->num[i] = sum % 10;
    }
}

static void fast_doubling(int k, char *buf)
{
    struct u128 a[1], b[1], const_two[1], t1[1], t2[1], tmp[1], tmp2[1];
    init_list(t1, 0);
    init_list(t2, 0);
    init_list(tmp, 0);
    init_list(tmp2, 0);
    init_list(a, 0);
    init_list(b, 1);
    init_list(const_two, 2);

    int run = 32 - __builtin_clz(k);

    for (int i = 0; i < run; i++) {
        int base, cur_digit;
        base = 1 << run - i - 1;
        cur_digit = k / base;

        // unsigned long long t1 = a*(2*b - a);
        u128_mul(const_two, b, t1);
        u128_sub(t1, a, tmp);
        u128_mul(a, tmp, t1);

        // unsigned long long t2 = b*b + a*a;
        u128_mul(b, b, tmp);
        u128_mul(a, a, tmp2);
        u128_add(tmp, tmp2, t2);

        // a = t1; b = t2; // m *= 2
        for (int i = 0; i < u128_l; i++) {
            a->num[i] = t1->num[i];
            b->num[i] = t2->num[i];
        }

        if (cur_digit == 1) {
            // t1 = a + b; // m++
            u128_add(t1, t2, tmp);
            for (int i = 0; i < u128_l; i++) {
                a->num[i] = b->num[i];
                b->num[i] = tmp->num[i];
            }
        }
        k = k % base;
    }
    char fib_num[u128_l] = "0";
    bool leading_zero = true;
    int head_position = 0;
    for (int i = u128_l - 1; i >= 0; i--) {
        if (a->num[i] != 0 && leading_zero) {
            leading_zero = false;
            head_position = i;
        }
        if (!leading_zero) {
            fib_num[head_position - i] = a->num[i] + '0';
        }
    }
    fib_num[head_position + 1] = '\0';
    copy_to_user(buf, fib_num, u128_l * sizeof(char));
}


static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    // kt = ktime_get(); // kernel
    fast_doubling(*offset, buf);
    // fib_sequence(*offset, buf);
    // kt = ktime_sub(ktime_get(), kt); // kernel
    // kt = ktime_get();  // ker_user

    return 0;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    cdev_init(fib_cdev, &fib_fops);
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
