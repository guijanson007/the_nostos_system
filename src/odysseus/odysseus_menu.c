/***********************************************
 *
 * @File : odysseus_menu.c
 * @Purpose : console menu for odysseus process. Shows the prompt, reads the user commands and executes them.
 * @Author : Guilherme Rodrigues
 * @Date : 25/09/2026
 *
 ************************************************/

#define _GNU_SOURCE

// header
#include "odysseus/odysseus_menu.h"


/*************************************************
 *
 * @Name : odysseusStartMenu
 * @Def : Displays the start menu for the Odysseus application.
 * @Arg : In: tOdysseus = Odysseus instance containing the name to display
 * @Ret : None.
 *
 **************************************************/
void odysseusStartMenu(Odysseus tOdysseus) {
    char *psBuffer = NULL;
 
    asprintf(&psBuffer, MSG_START_MENU, tOdysseus.name);
    println(psBuffer);
    free(psBuffer);
}
/*************************************************
 *
 * @Name : setupInputField
 * @Def : Sets up the input field for user commands, making the text bold.
 * @Arg : None.
 * @Ret : None.
 *
 **************************************************/
void setupInputField() {
    // Make text bold
    write(STDOUT_FILENO, TEXT_BOLD, strlen(TEXT_BOLD));
    write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
}

/*************************************************
 *
 * @Name : resetInputField
 * @Def : Resets the input field formatting to default.
 * @Arg : None.
 * @Ret : None.
 *
 **************************************************/
void resetInputField() {
    // Reset text formatting
    write(STDOUT_FILENO, TEXT_RESET, strlen(TEXT_RESET));
}

/*************************************************
 *
 * @Name : odysseusShowMenu
 * @Def : Displays the command prompt, reads user input, parses and validates
 * the command, and executes it if valid.
 * @Arg : In: tOdysseus = Odysseus instance to use for command execution
 * @Ret : Returns 1 to continue the menu, 0 to exit.
 *
 **************************************************/
int odysseusShowMenu(Odysseus *tOdysseus) {
    OdysseusCommand tCommand = {0};
    const char *psErrorMessage = NULL;
    char *psInput = NULL;
 
    setupInputField();
 
    if (-1 == readLine(STDIN_FILENO, &psInput)) {
        if (EINTR == errno) {
            // Interrupted by signal
            return 0;
        } else {
            // Error reading input
            printErr(ERROR_READ_COMMAND);
            return 0;
        }
    }
 
    resetInputField();
 
    if (NULL == psInput) {
        return 0;
    }
 
    if (0 == parseOdysseusCommand(psInput, &tCommand)) {
        free(psInput);
        return 1;
    }
 
    if (COMMAND_VALID != validateOdysseusCommand(&tCommand, &psErrorMessage)) {
        printErr(psErrorMessage);
        // Print an empty line for better readability
        printErr("");
        free(psInput);
        // Return 1 to continue the menu even after an invalid command
        return 1;
    }
    executeOdysseusCommand(tOdysseus, &tCommand);
    free(psInput);
    println(MSG_COMMAND_OK);
    // Print an empty line for better readability
    println("");
    return 1;
}