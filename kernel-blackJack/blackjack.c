#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/random.h>

#define DEVICE_NAME "blackjack"
#define CLASS_NAME "bj"
#define DECK_SIZE 52
#define MAX_CARDS 5 



MODULE_LICENSE("GPL");

enum suit { HEARTS, DIAMONDS, CLUBS, SPADES };
enum value { ACE = 1, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, JACK, QUEEN, KING };

struct card {
    enum suit card_suit;
    enum value card_value;
};

struct card deck[DECK_SIZE];

const char* value_to_string(enum value card_value) {
    switch (card_value) {
        case ACE: return "Ace";
        case TWO: return "Two";
        case THREE: return "Three";
        case FOUR: return "Four";
        case FIVE: return "Five";
        case SIX: return "Six";
        case SEVEN: return "Seven";
        case EIGHT: return "Eight";
        case NINE: return "Nine";
        case TEN: return "Ten";
        case JACK: return "Jack";
        case QUEEN: return "Queen";
        case KING: return "King";
        default: return "";
    }
}

const char* suit_to_string(enum suit card_suit) {
    switch (card_suit) {
        case HEARTS: return "Hearts";
        case DIAMONDS: return "Diamonds";
        case CLUBS: return "Clubs";
        case SPADES: return "Spades";
        default: return "";
    }
}


static int majorNumber; // Stores the device number
static struct class* blackjackClass = NULL;
static struct device* blackjackDevice = NULL;
static DEFINE_MUTEX(blackjack_mutex);
static int player_hand_value = 0;
static int dealer_hand_value = 0;
static bool game_in_progress = false;
static struct card player_hand[MAX_CARDS], dealer_hand[MAX_CARDS];
static int player_num_cards = 0, dealer_num_cards = 0;

char game_outcome_message[100];





// The prototype functions
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
void initialize_deck(void);
void shuffle_deck(void);
void reset_game(void);
struct card deal_card(void);
int calculate_hand_value(struct card *hand, int num_cards);
static char *blackjack_devnode(struct device *dev, umode_t *mode);
void get_card_descriptions(const struct card *hand, int num_cards, char *descriptions);



static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

void initialize_game(void) {
    initialize_deck();
    shuffle_deck();
}

void start_game(void) {
    player_hand[0] = deal_card(); // Deal first card to player
    dealer_hand[0] = deal_card(); // Deal first card to dealer
    player_hand[1] = deal_card(); // Deal second card to player
    dealer_hand[1] = deal_card(); // Deal second card to dealer

    player_hand_value = calculate_hand_value(player_hand, 2);
    dealer_hand_value = calculate_hand_value(dealer_hand, 2);
    game_in_progress = true;

    player_num_cards = 2;
    dealer_num_cards = 2;
}


void initialize_deck(void) {
    int i, j, k = 0;
    for (i = 0; i < 4; i++) { // Four suits
        for (j = 1; j <= 13; j++) { // Thirteen values
            deck[k].card_suit = i;
            deck[k].card_value = j;
            k++;
        }
    }
}

static int deck_position = 0;

struct card deal_card(void) {
    if (deck_position >= DECK_SIZE) {
        shuffle_deck(); // Reshuffle if we've run out of cards
        deck_position = 0;
    }
    return deck[deck_position++];
}


void shuffle_deck(void) {
    int i, j;
    struct card temp;
    for (i = 0; i < DECK_SIZE - 1; i++) {
        j = prandom_u32() % (DECK_SIZE - i) + i;

        // Swap cards at indices i and j
        temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

void reset_game(void) {
    initialize_game();  // Resets and shuffles the deck
    game_in_progress = false;
    player_num_cards = 0;
    dealer_num_cards = 0;
    player_hand_value = 0;
    dealer_hand_value = 0;

    // Reset the game outcome message
    snprintf(game_outcome_message, sizeof(game_outcome_message), "Game reset. Ready for a new round.");

    printk(KERN_INFO "Blackjack: Game Reset\n");
}


int calculate_hand_value(struct card *hand, int num_cards) {
    int value = 0, ace_count = 0, i;

    for (i = 0; i < num_cards; i++) {
        if (hand[i].card_value >= 10) {
            value += 10; // Face cards are worth 10
        } else if (hand[i].card_value == ACE) {
            ace_count++;
            value += 11; // Initially, treat Ace as 11
        } else {
            value += hand[i].card_value; // Other cards have their face value
        }
    }

    // Adjust for Aces if the value exceeds 21
    while (value > 21 && ace_count > 0) {
        value -= 10; // Treat Ace as 1 instead of 11
        ace_count--;
    }

    return value;
}



// Initialization function
static int __init blackjack_init(void) {
    printk(KERN_INFO "Blackjack: Initializing the Blackjack LKM\n");
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    blackjackClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(blackjackClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(blackjackClass);
    }

    // Set the devnode callback to set permissions for the device file
    blackjackClass->devnode = blackjack_devnode;

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
}

static char *blackjack_devnode(struct device *dev, umode_t *mode) {
    if (mode) *mode = 0666; 
    return NULL;
}


// Open function
static int dev_open(struct inode *inodep, struct file *filep) {
    if (!mutex_trylock(&blackjack_mutex)) { // Attempt to acquire the mutex (non-blocking)
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


void get_card_descriptions(const struct card *hand, int num_cards, char *descriptions) {
    int i;
    char card_description[20];
    descriptions[0] = '\0'; // Start with empty string

    for (i = 0; i < num_cards; i++) {
        // Assuming you have functions or logic to convert suit and value to string
        snprintf(card_description, sizeof(card_description), "%s of %s, ",
                 value_to_string(hand[i].card_value), suit_to_string(hand[i].card_suit));
        strcat(descriptions, card_description);
    }
}

void stand_game(void) {
    // Dealer draws cards until reaching 17 or higher
    while (dealer_hand_value < 17) {
        dealer_hand[dealer_num_cards++] = deal_card();
        dealer_hand_value = calculate_hand_value(dealer_hand, dealer_num_cards);
    }
    // Compare hands and determine winner
    
    if (player_hand_value > 21) {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "Player busts with %d. Dealer wins.", player_hand_value);
    } else if (dealer_hand_value > 21) {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "Dealer busts with %d. Player wins.", dealer_hand_value);
    } else if (player_hand_value == 21) {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "Player hits Blackjack! Player wins.");
    } else if (dealer_hand_value == 21) {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "Dealer hits Blackjack! Dealer wins.");
    } else if (player_hand_value > dealer_hand_value) {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "Player wins with %d against dealer's %d.", player_hand_value, dealer_hand_value);
    } else if (player_hand_value < dealer_hand_value) {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "Dealer wins with %d against player's %d.", dealer_hand_value, player_hand_value);
    } else {
        snprintf(game_outcome_message, sizeof(game_outcome_message), "It's a tie with both at %d.", player_hand_value);
    }
    game_in_progress = false; // End the game
}



static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    char response[256];
    int response_length = 0;

    if (*offset > 0) {
        return 0;
    }

    if (game_in_progress) {
        char player_cards_descriptions[MAX_CARDS * 20];
        char dealer_cards_descriptions[MAX_CARDS * 20];
        get_card_descriptions(player_hand, player_num_cards, player_cards_descriptions);
        get_card_descriptions(dealer_hand, dealer_num_cards, dealer_cards_descriptions);
        response_length = snprintf(response, sizeof(response),
                                   "Player has %s for a total of %d. Dealer shows %s for a total of %d. Does Player want another card? If so, respond with Hit or Stand for hold.\n",
                                   player_cards_descriptions, player_hand_value, dealer_cards_descriptions, dealer_hand_value);
    } else {
        response_length = snprintf(response, sizeof(response),
                                   "Game over. %s Issue RESET to start a new game, then DEAL to deal cards.\n", game_outcome_message);
    }

    error_count = copy_to_user(buffer, response, response_length);

    if (error_count == 0) {
        *offset = response_length;
        return response_length;
    } else {
        return -EFAULT;
    }
}






// Write function
// In this function I will handle different game commands such as RESET, SHUFFLE, DEAL, 
//HIT, etc. It will parse the input from the user, update the game state, and prepare responses for //the dev_read function.
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char command[256];
    if (copy_from_user(command, buffer, len)) {
        return -EFAULT;
    }

    command[len] = '\0'; // Null-terminate the command string

    if (strcmp(command, "RESET\n") == 0) {
        reset_game();
    } else if (strcmp(command, "SHUFFLE\n") == 0) {
        shuffle_deck();
        printk(KERN_INFO "Blackjack: Deck Shuffled\n");
    } else if (strcmp(command, "DEAL\n") == 0) {
        printk(KERN_INFO "Blackjack: Game in progress status before DEAL: %d\n", game_in_progress);
        if (!game_in_progress) {
            start_game(); // Deals cards and updates the game state
            printk(KERN_INFO "Blackjack: Cards Dealt\n");
        } else {
            printk(KERN_INFO "Blackjack: Game already in progress. Reset first.\n");
            return -EINVAL;
        }
       } else if (strcmp(command, "HIT\n") == 0) {
        if (game_in_progress && player_num_cards < MAX_CARDS) {
            player_hand[player_num_cards++] = deal_card();
            player_hand_value = calculate_hand_value(player_hand, player_num_cards);
            if (player_hand_value > 21) {
                printk(KERN_INFO "Blackjack: Player busts.\n");
	    snprintf(game_outcome_message, sizeof(game_outcome_message), "Player busts. Dealer wins.");
                game_in_progress = false;
            } else {
                printk(KERN_INFO "Blackjack: Player Hits\n");
            }
        } else {
            printk(KERN_INFO "Blackjack: Cannot hit, either game not in progress or max cards reached.\n");
            return -EINVAL;
        }
    } else if (strcmp(command, "STAND\n") == 0) {
        if (game_in_progress) {
            stand_game();
        } else {
            printk(KERN_INFO "Blackjack: Cannot stand, game not in progress.\n");
            return -EINVAL;
        }





    } else {
        printk(KERN_INFO "Blackjack: Unknown Command\n");
        return -EINVAL; // Invalid command
    }


    return len;
}



// Register module functions
module_init(blackjack_init);
module_exit(blackjack_exit);

