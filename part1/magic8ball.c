#include <linux/module.h>   
#include <linux/kernel.h>      
#include <linux/fs.h>           
#include <linux/uaccess.h>    
#include <linux/init.h>         
#include <linux/random.h>   


static int   majorNumber;                  ///< Stores the device number 
static char  message[256] = {0};           ///< Memory for the string that is passed from userspace
static short size_of_message;              ///< Used to remember the size of the string stored

//prototypes
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .release = dev_release,
};


//restricts the visibility of the function to within this C file as mentioned in project desciption
static int __init magic8ball_init(void){
   printk(KERN_INFO "Magic8Ball: Initializing the Magic8Ball LKM\n");

   //dynamically allocate a major number for the device
   majorNumber = register_chrdev(0, "magic8ball", &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "Magic8Ball failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "Magic8Ball: registered correctly with major number %d\n", majorNumber);

   return 0;
}

// LKM cleanup
static void __exit magic8ball_exit(void){
   unregister_chrdev(majorNumber, "magic8ball");
   printk(KERN_INFO "Magic8Ball: Goodbye from the LKM!\n");
}


static int dev_open(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "Magic8Ball: Device has been opened\n");
   return 0;
}

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
    get_random_bytes(&random_index, sizeof(random_index));
    random_index = random_index % ARRAY_SIZE(eight_ball_responses); 

    // Copy the selected response to the user
    error_count = copy_to_user(buffer, eight_ball_responses[random_index], strlen(eight_ball_responses[random_index]));

    if (error_count == 0) {            
        printk(KERN_INFO "Magic8Ball: Sent response '%s' to the user\n", eight_ball_responses[random_index]);
        return strlen(eight_ball_responses[random_index]); // Return the number of characters sent
    } else {
        printk(KERN_ALERT "Magic8Ball: Failed to send response to the user\n");
        return -EFAULT; // Return an error code
    }
}
