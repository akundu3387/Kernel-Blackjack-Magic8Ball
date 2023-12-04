#include <linux/module.h>   
#include <linux/kernel.h>      
#include <linux/fs.h>           
#include <linux/uaccess.h>    
#include <linux/init.h>   
#include <linux/device.h>
#include <linux/random.h>   
#define CLASS_NAME  "magic8ball"
#define DEVICE_NAME "magic8ball"
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

MODULE_LICENSE("GPL");            


static int   majorNumber;                  ///device number 
static char  message[256] = {0};           ///Memory for the string that is passed from userspace
static short size_of_message;              ///Used to remember the size of the string stored

// Prototypes
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static char *magic8ball_devnode(struct device *dev, umode_t *mode);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .release = dev_release,
};


// Static to visibility of the function to within this C file
static struct class*  magic8ballClass  = NULL; 
static struct device* magic8ballDevice = NULL; 

static int __init magic8ball_init(void) {
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        return majorNumber;
    }

    magic8ballClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(magic8ballClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(magic8ballClass);
    }

    magic8ballClass->devnode = magic8ball_devnode;

    magic8ballDevice = device_create(magic8ballClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(magic8ballDevice)) {
        class_destroy(magic8ballClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(magic8ballDevice);
    }

    return 0;
}

static void __exit magic8ball_exit(void) {
    device_destroy(magic8ballClass, MKDEV(majorNumber, 0));
    class_unregister(magic8ballClass);
    class_destroy(magic8ballClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
}



static char *magic8ball_devnode(struct device *dev, umode_t *mode) {
    if (mode) *mode = 0666;
    return NULL;
}


// The device open function that is called each time the device is opened
static int dev_open(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "Magic8Ball: Device has been opened\n");
   return 0;
}

// The device release function that is called whenever the device is closed/released by the userspace program
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "Magic8Ball: Device successfully closed\n");
   return 0;
}



static const char *eight_ball_responses[] = {
    "It is certain.",
    "As I see it, yes.",
    "Reply hazy, try again.",
    "Don't count on it.",
    "It is decidedly so.",
    "Most likely.",
    "Ask again later.",
    "My reply is no.",
    "Without a doubt.",
    "Outlook good.",
    "Better not tell you now.",
    "My sources say no.",
    "Yes definitely.",
    "Yes.",
    "Cannot predict now.",
    "Outlook not so good.",
    "You may rely on it.",
    "Signs point to yes.",
    "Concentrate and ask again.",
    "Very doubtful."
};

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    int error_count = 0;
    unsigned int random_index = 0;

    // Only provide a response if the offset is 0
    if (*offset == 0) {
        // Generate a random index for the response
        get_random_bytes(&random_index, sizeof(random_index));
        random_index = random_index % ARRAY_SIZE(eight_ball_responses);

        // Copy the response to the user
        error_count = copy_to_user(buffer, eight_ball_responses[random_index], strlen(eight_ball_responses[random_index]));

        if (error_count == 0) {            
            *offset = strlen(eight_ball_responses[random_index]); 
            return *offset; // Return the number of characters sent
        } else {
              return -EFAULT; // Return an error code
        }
    } else {
        return 0; // No more characters to read if the offset is not 0
    }
}

module_init(magic8ball_init);
module_exit(magic8ball_exit);
