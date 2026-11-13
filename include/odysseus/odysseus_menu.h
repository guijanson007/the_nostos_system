#ifndef __ODYSSEUS_MENU_H__
#define __ODYSSEUS_MENU_H__

/***********************************************
 *
 * @File : odysseus_menu.h
 * @Purpose : console menu for odysseus process
 * @Author : Guilherme Rodrigues
 * @Date : 25/09/2026
 *
 ************************************************/

// Own includes
#include "odysseus/odysseus_parser.h"
#include "odysseus/odysseus_command.h"

//Constants
#define TEXT_BOLD "\033[1m"
#define TEXT_RESET "\033[0m"
#define PROMPT "$ "
#define MSG_START_MENU "Odysseus %s is ready to sail.\n"
#define MSG_COMMAND_OK "Command OK"
#define ERROR_READ_COMMAND "Failed to read command"

//Functions
void odysseusStartMenu(Odysseus tOdysseus);
int odysseusShowMenu(Odysseus *tOdysseus, volatile sig_atomic_t *pnQuit);

#endif // !#ifndef __ODYSSEUS_MENU_H__
