#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEVICE_NAME "blackjack"
#define CLASS_NAME "bj"

MODULE_LICENSE("GPL");

static int majorNumber; // Stores the device number
static struct class* blackjackClass = NULL;
static struct device* blackjackDevice = NULL;
static DEFINE_MUTEX(blackjack_mutex);
static int player_hand_value = 0;
static int dealer_hand_value = 0;
static bool game_in_progress = false;


// The prototype functions
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

void initialize_game(void) {
    // Reset hand values and game state
    player_hand_value = 0;
    dealer_hand_value = 0;
    game_in_progress = true;

}

// Initialization function
static int __init blackjack_init(void) {
    printk(KERN_INFO "Blackjack: Initializing the Blackjack LKM\n");
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "Blackjack failed to register a major number\n");
        return majorNumber;
    }

    blackjackClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(blackjackClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(blackjackClass);
    }

    blackjackDevice = device_create(blackjackClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(blackjackDevice)) {
        class_destroy(blackjackClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(blackjackDevice);
    }

    printk(KERN_INFO "Blackjack: device class created correctly\n");
    return 0;
}

// Exit function
static void __exit blackjack_exit(void) {
    device_destroy(blackjackClass, MKDEV(majorNumber, 0));
    class_unregister(blackjackClass);
    class_destroy(blackjackClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_INFO "Blackjack: Goodbye from the LKM!\n");
}

// Open function
static int dev_open(struct inode *inodep, struct file *filep) {
    if (!mutex_trylock(&blackjack_mutex)) { // Try to acquire the mutex (non-blocking)
        printk(KERN_ALERT "Blackjack: Device in use by another process");
        return -EBUSY;
    }
    return 0;
}

// Release function
static int dev_release(struct inode *inodep, struct file *filep) {
    mutex_unlock(&blackjack_mutex); // Release the mutex
    return 0;
}



// Read function
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    return 0;
}

// Write function
// In this function I will handle different game commands such as RESET, SHUFFLE, DEAL, //HIT, etc. It will parse the input from the user, update the game state, and prepare responses for //the dev_read function.
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    return len;
}

// Register module functions
module_init(blackjack_init);
module_exit(blackjack_exit);


o

