/***********************************************
 *
 * @File : ithaca.c
 * @Purpose : manages the ithaca process.
 *            Arguments: argv[1] = ithaca file
 *                       argv[2] = voyages file
 * @Author : Axel Boleda i Callizo
 * @Date : 22/09/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include "ithaca/ithaca.h"

int nEv_quit = 0;

/***********************************************
 *
 * @Name : onSigint
 * @Def : SIGINT handler. Only sets the quit flag; printing and freeing are done in main, outside the handler.
 * @Arg : In: nSignum = signal number (unused)
 * @Ret : None.
 *
 ************************************************/
static void onSigint(int nSignum) {
    (void)nSignum;
    nEv_quit = 1;
}

/***********************************************
 *
 * @Name : freeIthaca
 * @Def : Frees the dynamic memory of the ithaca struct.
 * @Arg : In/Out: ptIthaca = ithaca to free
 * @Ret : None.
 *
 ************************************************/
void freeIthaca(Ithaca *ptIthaca) {
    if (NULL == ptIthaca) {
        return;
    }
    free(ptIthaca->psName);
    free(ptIthaca->psMission);
    free(ptIthaca->psIp);
    memset(ptIthaca, 0, sizeof(*ptIthaca));
}

/***********************************************
 *
 * @Name : freeVoyage
 * @Def : Frees the voyages dynamic array and the strings of each voyage.
 * @Arg : In/Out: patVoyages = pointer to the voyages array
 *        In/Out: ptNumVoyages = number of voyages, set to 0
 * @Ret : None.
 *
 ************************************************/
void freeVoyage(Voyage **patVoyages, size_t *ptNumVoyages) {
    size_t i = 0;
 
    if (NULL == patVoyages || NULL == *patVoyages) {
        return;
    }
    for (i = 0; i < *ptNumVoyages; i++) {
        free((*patVoyages)[i].psObject);
        free((*patVoyages)[i].psFile);
        free((*patVoyages)[i].psDestination);
    }
    free(*patVoyages);
    *patVoyages = NULL;
    *ptNumVoyages = 0;
}

/***********************************************
 *
 * @Name : closeFiles
 * @Def : Closes the ithaca and voyages file descriptors.
 * @Arg : In/Out: pnFdIthaca = pointer to the ithaca file descriptor
 *        In/Out: pnFdVoyage = pointer to the voyages file descriptor
 * @Ret : None.
 *
 ************************************************/
void closeFiles(int *pnFdIthaca, int *pnFdVoyage) {
    close(*pnFdIthaca);
    close(*pnFdVoyage);
}
 
/***********************************************
 *
 * @Name : errorLoadingHandler
 * @Def : Prints a loading error and closes both files.
 * @Arg : In: psError = error message to print
 *        In/Out: pnFdIthaca = pointer to the ithaca file descriptor
 *        In/Out: pnFdVoyage = pointer to the voyages file descriptor
 * @Ret : None.
 *
 ************************************************/
void errorLoadingHandler(const char *psError, int *pnFdIthaca, int *pnFdVoyage) {
    printErr((char *)psError);
    closeFiles(pnFdIthaca, pnFdVoyage);
}
 
/***********************************************
 *
 * @Name : loadData
 * @Def : Loads the ithaca and the voyages. On error it prints the message, closes both files and frees
 * @Arg : In/Out: pnFdIthaca = pointer to the ithaca file descriptor
 *        In/Out: pnFdVoyage = pointer to the voyages file descriptor
 *        Out: ptIthaca = ithaca to fill
 *        Out: patVoyages = dynamic array of voyages to fill
 *        Out: ptNumVoyages = number of elements in the voyages array
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
int loadData(int *pnFdIthaca, int *pnFdVoyage, Ithaca *ptIthaca, Voyage **patVoyages, size_t *ptNumVoyages) {
    if (-1 == loadIthacaData(*pnFdIthaca, ptIthaca)) {
        errorLoadingHandler(ERROR_LOAD_ITHACA, pnFdIthaca, pnFdVoyage);
        return -1;
    }
 
    if (-1 == loadVoyageData(*pnFdVoyage, patVoyages, ptNumVoyages)) {
        errorLoadingHandler(ERROR_LOAD_VOYAGE, pnFdIthaca, pnFdVoyage);
        freeIthaca(ptIthaca);
        return -1;
    }
 
    return 0;
}
 
/***********************************************
 *
 * @Name : printData
 * @Def : Prints the ithaca info and its voyages.
 * @Arg : In: ptIthaca = pointer to the ithaca struct
 *        In: atVoyages = dynamic array of voyages
 *        In: tNumVoyages = number of elements in atVoyages
 * @Ret : None.
 *
 ************************************************/
void printData(Ithaca *ptIthaca, Voyage *atVoyages, size_t tNumVoyages) {
    char *psBuffer = NULL;
    size_t i = 0;
 
    if (-1 != asprintf(&psBuffer, "%s - %s - %s - %d", ptIthaca->psName,
                       ptIthaca->psMission, ptIthaca->psIp, ptIthaca->nPort)) {
        println(psBuffer);
        free(psBuffer);
    }
 
    for (i = 0; i < tNumVoyages; i++) {
        if (-1 != asprintf(&psBuffer, "%d: %s - %s - %s - %d", atVoyages[i].nId,
                           atVoyages[i].psObject, atVoyages[i].psFile,
                           atVoyages[i].psDestination, atVoyages[i].nReward)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
}
 
/***********************************************
 *
 * @Name : waitForQuit
 * @Def : Installs the SIGINT handler and waits until the user presses Ctrl+C, then prints the farewell message.
 * @Arg : None.
 * @Ret : None.
 *
 ************************************************/
void waitForQuit() {
    signal(SIGINT, onSigint);
    while (!nEv_quit) {
        pause();
    }
    println(FAREWELL);
}
 
/***********************************************
 *
 * @Name : main
 * @Def : Manages the general flow of the ithaca process.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (ithaca file and voyages file)
 * @Ret : EXIT_SUCCESS if everything went well, EXIT_FAILURE otherwise.
 *
 ************************************************/
int main(int argc, char *argv[]) {
    int nFdIthaca = -1;
    int nFdVoyage = -1;
    Ithaca tIthaca = {0};
    Voyage *atVoyages = NULL;
    size_t tNumVoyages = 0;
 
    if (EXPECTED_ARGC != argc || 0 != strcmp(argv[1], ITHACA_FILE) ||
        0 != strcmp(argv[2], VOYAGES_FILE)) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }
 
    nFdIthaca = open(argv[1], O_RDONLY);
    if (-1 == nFdIthaca) {
        printErr(ERROR_FILE_ITHACA);
        return EXIT_FAILURE;
    }
 
    nFdVoyage = open(argv[2], O_RDONLY);
    if (-1 == nFdVoyage) {
        printErr(ERROR_FILE_VOYAGE);
        close(nFdIthaca);
        return EXIT_FAILURE;
    }
 
    if (-1 == loadData(&nFdIthaca, &nFdVoyage, &tIthaca, &atVoyages,
                       &tNumVoyages)) {
        return EXIT_FAILURE;
    }
    closeFiles(&nFdIthaca, &nFdVoyage);
 
    // DEBUGGING
    printData(&tIthaca, atVoyages, tNumVoyages);
 
    waitForQuit();
    freeIthaca(&tIthaca);
    freeVoyage(&atVoyages, &tNumVoyages);
    return EXIT_SUCCESS;
}