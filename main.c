//cpwd — tiny and handy password manager
//this is a port of npwd utility by Nadim Kobeissi (https://github.com/kaepora/npwd) to C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "scrypt/readpass.h"
#include "scrypt/crypto_scrypt.h"
#include "poison.h"

const int salt_size = 1024;
const int account_name_size = 1016;
const char salt_prefix[] = "npwd";
const char command_echo[] = "echo \'";

extern int main (int argc, char** argv) {
	//OS-specific part
	#ifdef WIN32
		char command_end[7] = "\'|clip\0"; // Windows
	#elif __MACH__
		char command_end[9] = "\'|pbcopy\0"; // Mac OS X
	#else
		//if it's not Windows nor OS X then assume it's Linux or OpenBSD
	
		//choose only one string via commenting unnecessary line!
		char command_end[29] = "\'|xclip -selection clipboard\0";	//Linux/OpenBSD with xclip package installed
		//char command_end[27] = "\'|xsel --clipboard --input\0";	//Linux/OpenBSD with xsel package installed
	#endif
	

	char command[100] = {0};
	char salt[salt_size] = {0};
	char account[account_name_size];

	char* key;
	
	int i;
	//if registration mode enabled and/or account name was defined as a command line argument
	if (argc > 1) {
		//registration mode
		if (strcmp("-r", argv[1]) == 0) {
			//registration mode, account name was defined as a command line argument
			if (argc > 2) {
				//copy at most 1016 bytes of argv[2] (to avoid buffer overflow) to account
				//maximum length of arguments is too big: http://www.in-ulm.de/~mascheck/various/argmax
				memcpy(account, argv[2], strlen(argv[2]) < account_name_size ? strlen(argv[2]) : account_name_size);
	
				// lowercasing account name for usability
				for (i = 0; account[i]; i++)
	 				account[i] = tolower(account[i]);
 		
	 			//salt: "npwd"+lowercased account+"npwd"
				strncat(salt, salt_prefix, strlen(salt_prefix));
				strncat(salt, account, strlen(account));
				strncat(salt, salt_prefix, strlen(salt_prefix));
			
			} else { // registration mode, ask a user to enter account name
				printf("account: ");
				if (fflush(stdin)) {
					fprintf(stderr, "fflush() error\n");
					return EXIT_FAILURE;
				}
				if (fgets((char *)account, account_name_size, stdin) == NULL) {
					fprintf(stderr, "fgets() error\n");
					return EXIT_FAILURE;
				};
		
				// lowercasing account name for usability
				for (i = 0; account[i]; i++)
 					account[i] = tolower(account[i]);
 		
 				//salt: "npwd"+lowercased account without newline+"npwd"
				strncat(salt, salt_prefix, strlen(salt_prefix));
				strncat(salt, account, strlen(account)-1);
				strncat(salt, salt_prefix, strlen(salt_prefix));
			
			}
			
			//get master key twice because it's registration mode
		 	if (tarsnap_readpass(&key, "key", "repeat", 1)) {
				fprintf(stderr, "tarsnap_readpass() error\n");
				return EXIT_FAILURE;
			}
		}
		//normal mode, account name was defined as a command line argument-------------------------
		else {
		
			//copy at most 1016 bytes of argv[1] (to avoid buffer overflow) to account
			//maximum length of arguments is too big: http://www.in-ulm.de/~mascheck/various/argmax
			memcpy(account, argv[1], strlen(argv[1]) < 1016 ? strlen(argv[1]) : 1016);
	
			//lowercasing account name for usability
			for (i = 0; account[i]; i++)
	 			account[i] = tolower(account[i]);
 		
	 		//salt: "npwd"+lowercased account+"npwd"
			strncat(salt, salt_prefix, strlen(salt_prefix));
			strncat(salt, account, strlen(account));
			strncat(salt, salt_prefix, strlen(salt_prefix));
		
			//get master key once
	 		if (tarsnap_readpass(&key, "key", NULL, 1)) {
				fprintf(stderr, "tarsnap_readpass() error\n");
				return EXIT_FAILURE;
			}		
		}
	}
	//no additional parameters: normal mode, ask a user to enter account name----------------------
	else {
		printf("account: ");
		if (fflush(stdin)) {
			fprintf(stderr, "fflush() error\n");
			return EXIT_FAILURE;
		}
		if (fgets((char *)account, 1016, stdin) == NULL) {
			fprintf(stderr, "fgets() error\n");
			return EXIT_FAILURE;
		};
		
		//lowercasing account name for usability
		for (i = 0; account[i]; i++)
 			account[i] = tolower(account[i]);
 		
 		//salt: "npwd"+lowercased account without newline+"npwd"
		strncat(salt, salt_prefix, strlen(salt_prefix));
		strncat(salt, account, strlen(account)-1);
		strncat(salt, salt_prefix, strlen(salt_prefix));
		
		//get master key once
	 	if (tarsnap_readpass(&key, "key", NULL, 1)) {
			fprintf(stderr, "tarsnap_readpass() error\n");
			return EXIT_FAILURE;
		}
	}

	const int hash_size = 16;
	unsigned char hash[hash_size];
	//launch key derivation function to get hash of master key and account name
	if (crypto_scrypt((uint8_t*)key, (size_t)strlen(key), (uint8_t*)salt, (size_t)strlen(salt), 131072, 8, 1, hash, hash_size)) {
    	fprintf(stderr, "crypto_scrypt() error\n");
		return EXIT_FAILURE;
	}
   
	//clear a buffer for a master key and for salt as soon as we can
	free(key);
	memset(salt, 0, sizeof(salt));

	//first let's take an unencoded hash to clipboard!
	//to do that, compose and execute a command "command_echo"+unencoded hash+"command_end"
	char password_letters[3];
	strncat(command, command_echo, strlen(command_echo));
	for (i = 0; i < 16; i++) {
		// output format is 2 lowercase hex symbols and null-byte for strncat, totally 3 bytes
		snprintf(password_letters, 3, "%02x", hash[i]);
		strncat(command, password_letters, 3);
	}
	//clear a buffer with encoded password as soon as we can
	memset(hash, 0, sizeof(hash));
	memset(password_letters, 0, sizeof(password_letters));
	strncat(command, command_end, strlen(command_end));
	
	if (system(command)) {
	    fprintf(stderr, "system(fill_clipboard) error\n");
		return EXIT_FAILURE;
   	};
    
    //clear a buffer with encoded password as soon as we can
	memset(command, 0, sizeof(command));
	
	//notify user and pause for 15 seconds
	printf("\nIn clipboard!\n");
	sleep(10);
	
	//clear a clipboard via composing and executing command "command_echo"+"command_end"
	strncat(command, command_echo, strlen(command_echo));
	strncat(command, command_end, strlen(command_end));
	if (system(command)) {
	    fprintf(stderr, "system(clear_clipboard) error\n");
		return EXIT_FAILURE;
    }
	
	return EXIT_SUCCESS;
}
