/***********************************************
 *
 * @File : odysseus_persistence.c
 * @Purpose : Saves and restores the gold and the cargo of the odysseus in
 *            a fixed-record binary file (one file per ship), using only
 *            open/read/write/lseek.
 * @Author : Guilherme Rodrigues
 * @Date : 16/11/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "odysseus/odysseus_persistence.h"

/***********************************************
 *
 * @Name : putField
 * @Def : Writes a 32 bit integer at the given position, in the byte order
 *        of the machine.
 * @Arg : Out: pcBuffer = position to write at
 *        In: nValue = value to write
 * @Ret : None.
 *
 ************************************************/
static void putField(unsigned char *pcBuffer, uint32_t nValue) {
    memcpy(pcBuffer, &nValue, STATE_FIELD_SIZE);
}

/***********************************************
 *
 * @Name : getField
 * @Def : Reads a 32 bit integer from the given position, in the byte order
 *        of the machine.
 * @Arg : In: pcBuffer = position to read from
 * @Ret : The value read.
 *
 ************************************************/
static uint32_t getField(const unsigned char *pcBuffer) {
    uint32_t nValue = 0;

    memcpy(&nValue, pcBuffer, STATE_FIELD_SIZE);
    return nValue;
}

/***********************************************
 *
 * @Name : checksum
 * @Def : Sums the bytes of the record that come before the checksum.
 * @Arg : In: pcBuffer = record
 * @Ret : The sum of the bytes.
 *
 ************************************************/
static uint32_t checksum(const unsigned char *pcBuffer) {
    uint32_t nSum = 0;
    int i = 0;

    for (i = 0; i < STATE_SUM_OFFSET; i++) {
        nSum += pcBuffer[i];
    }
    return nSum;
}

/***********************************************
 *
 * @Name : buildStatePath
 * @Def : Builds the path of the state file of the ship (<name>.state, in
 *        the ship folder). Names with a slash are refused so the file can
 *        never leave that folder.
 * @Arg : In: ptOdysseus = odysseus whose file is wanted
 * @Ret : Allocated path (the caller frees it), or NULL on failure.
 *
 ************************************************/
static char *buildStatePath(const Odysseus *ptOdysseus) {
    char *psPath = NULL;

    if (NULL == ptOdysseus->psName || '\0' == ptOdysseus->psName[0] ||
        NULL != strchr(ptOdysseus->psName, '/')) {
        return NULL;
    }
    if (-1 == asprintf(&psPath, STATE_FILE_FORMAT, ptOdysseus->psPath,
                       ptOdysseus->psName)) {
        return NULL;
    }
    return psPath;
}

/***********************************************
 *
 * @Name : encodeState
 * @Def : Serializes the gold and the cargo into a record.
 * @Arg : In: ptCargo = cargo to save
 *        Out: pcBuffer = buffer of STATE_RECORD_SIZE bytes
 * @Ret : 0 on success, -1 if the cargo doesn't fit in the record.
 *
 ************************************************/
static int encodeState(const Cargo *ptCargo, unsigned char *pcBuffer) {
    int i = 0;
    unsigned char *pcSlot = NULL;

    if (0 > ptCargo->nNumSupplies ||
        STATE_MAX_SUPPLIES < ptCargo->nNumSupplies) {
        return -1;
    }
    memset(pcBuffer, 0, STATE_RECORD_SIZE);
    putField(pcBuffer, STATE_MAGIC);
    putField(pcBuffer + STATE_FIELD_SIZE, STATE_VERSION);
    putField(pcBuffer + 2 * STATE_FIELD_SIZE, (uint32_t)ptCargo->nGold);
    putField(pcBuffer + 3 * STATE_FIELD_SIZE, (uint32_t)ptCargo->nNumSupplies);

    for (i = 0; i < ptCargo->nNumSupplies; i++) {
        pcSlot = pcBuffer + STATE_HEADER_SIZE + i * STATE_SLOT_SIZE;
        if (NULL == ptCargo->atSupplies[i].psName ||
            STATE_NAME_SIZE <= strlen(ptCargo->atSupplies[i].psName)) {
            return -1;
        }
        memcpy(pcSlot, ptCargo->atSupplies[i].psName,
               strlen(ptCargo->atSupplies[i].psName));
        putField(pcSlot + STATE_NAME_SIZE,
                 (uint32_t)ptCargo->atSupplies[i].nAmount);
    }
    putField(pcBuffer + STATE_SUM_OFFSET, checksum(pcBuffer));
    return 0;
}

/***********************************************
 *
 * @Name : odysseusSaveState
 * @Def : Writes the gold and the cargo of the odysseus to its state file,
 *        overwriting the previous record in place with one write. The file
 *        is cut to the record size afterwards, so a longer leftover file
 *        (e.g. a damaged one) becomes a valid record again.
 * @Arg : In: ptOdysseus = odysseus to save
 * @Ret : 0 on success, -1 on failure (a warning is printed).
 *
 ************************************************/
int odysseusSaveState(const Odysseus *ptOdysseus) {
    unsigned char *pcBuffer = NULL;
    char *psPath = NULL;
    int nFd = -1;
    int nRet = -1;

    psPath = buildStatePath(ptOdysseus);
    pcBuffer = malloc(STATE_RECORD_SIZE);
    if (NULL != psPath && NULL != pcBuffer &&
        0 == encodeState(&ptOdysseus->tCargo, pcBuffer)) {
        nFd = open(psPath, O_WRONLY | O_CREAT, STATE_FILE_MODE);
        if (-1 != nFd) {
            if (-1 != lseek(nFd, 0, SEEK_SET) &&
                STATE_RECORD_SIZE == write(nFd, pcBuffer, STATE_RECORD_SIZE) &&
                0 == ftruncate(nFd, STATE_RECORD_SIZE)) {
                nRet = 0;
            }
            close(nFd);
        }
    }
    free(pcBuffer);
    free(psPath);
    if (0 != nRet) {
        printErr(STATE_WARN_SAVE);
    }
    return nRet;
}

/***********************************************
 *
 * @Name : readRecord
 * @Def : Reads the whole state file into a buffer and checks that it has
 *        exactly STATE_RECORD_SIZE bytes.
 * @Arg : In: psPath = path of the state file
 *        Out: pcBuffer = buffer of STATE_RECORD_SIZE bytes
 *        Out: pnMissing = set to 1 if the file doesn't exist
 * @Ret : 0 if a record of the right size was read, -1 otherwise.
 *
 ************************************************/
static int readRecord(const char *psPath, unsigned char *pcBuffer,
                      int *pnMissing) {
    unsigned char cExtra = 0;
    ssize_t tRead = 0;
    size_t tTotal = 0;
    int nFd = open(psPath, O_RDONLY);

    if (-1 == nFd) {
        *pnMissing = (ENOENT == errno);
        return -1;
    }
    while (tTotal < STATE_RECORD_SIZE) {
        tRead = read(nFd, pcBuffer + tTotal, STATE_RECORD_SIZE - tTotal);
        if (0 >= tRead) {
            break;
        }
        tTotal += (size_t)tRead;
    }
    // A file longer than the record is also invalid
    if (STATE_RECORD_SIZE != tTotal || 0 != read(nFd, &cExtra, 1)) {
        close(nFd);
        return -1;
    }
    close(nFd);
    return 0;
}

/***********************************************
 *
 * @Name : decodeState
 * @Def : Validates a record and builds the cargo it describes: magic,
 *        version, checksum, counts, names and amounts are all checked.
 * @Arg : In: pcBuffer = record of STATE_RECORD_SIZE bytes
 *        Out: ptCargo = cargo built (the caller frees its supplies)
 * @Ret : 0 on success, -1 if the record is invalid.
 *
 ************************************************/
static int decodeState(const unsigned char *pcBuffer, Cargo *ptCargo) {
    const unsigned char *pcSlot = NULL;
    uint32_t nNum = 0;
    uint32_t nGold = 0;
    int i = 0;

    memset(ptCargo, 0, sizeof(Cargo));
    if (STATE_MAGIC != getField(pcBuffer) ||
        STATE_VERSION != getField(pcBuffer + STATE_FIELD_SIZE) ||
        checksum(pcBuffer) != getField(pcBuffer + STATE_SUM_OFFSET)) {
        return -1;
    }
    nGold = getField(pcBuffer + 2 * STATE_FIELD_SIZE);
    nNum = getField(pcBuffer + 3 * STATE_FIELD_SIZE);
    if (INT32_MAX < nGold || STATE_MAX_SUPPLIES < nNum) {
        return -1;
    }
    ptCargo->nGold = (int)nGold;

    for (i = 0; i < (int)nNum; i++) {
        pcSlot = pcBuffer + STATE_HEADER_SIZE + i * STATE_SLOT_SIZE;
        if ('\0' == pcSlot[0] ||
            NULL == memchr(pcSlot, '\0', STATE_NAME_SIZE) ||
            0 == getField(pcSlot + STATE_NAME_SIZE) ||
            INT32_MAX < getField(pcSlot + STATE_NAME_SIZE) ||
            0 != addSupply(ptCargo, (const char *)pcSlot,
                           (int)getField(pcSlot + STATE_NAME_SIZE))) {
            return -1;
        }
    }
    // Duplicated names would have been merged by addSupply
    if (ptCargo->nNumSupplies != (int)nNum) {
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : freeCargoSupplies
 * @Def : Frees the supplies of a cargo and leaves it empty.
 * @Arg : In/Out: ptCargo = cargo whose supplies are freed
 * @Ret : None.
 *
 ************************************************/
static void freeCargoSupplies(Cargo *ptCargo) {
    int i = 0;

    for (i = 0; i < ptCargo->nNumSupplies; i++) {
        free(ptCargo->atSupplies[i].psName);
    }
    free(ptCargo->atSupplies);
    ptCargo->atSupplies = NULL;
    ptCargo->nNumSupplies = 0;
    ptCargo->nTotalAmount = 0;
}

/***********************************************
 *
 * @Name : odysseusLoadState
 * @Def : Restores the gold and the cargo saved by a previous execution,
 *        replacing the initial resources of the config file. A missing,
 *        truncated or corrupt file leaves the initial resources untouched.
 * @Arg : In/Out: ptOdysseus = odysseus to restore
 * @Ret : 1 if the state was restored, 0 if the initial resources are kept.
 *
 ************************************************/
int odysseusLoadState(Odysseus *ptOdysseus) {
    unsigned char *pcBuffer = NULL;
    char *psPath = NULL;
    Cargo tLoaded;
    int nMissing = 0;
    int nRet = 0;

    psPath = buildStatePath(ptOdysseus);
    pcBuffer = malloc(STATE_RECORD_SIZE);
    if (NULL == psPath || NULL == pcBuffer) {
        free(psPath);
        free(pcBuffer);
        return 0;
    }

    if (0 != readRecord(psPath, pcBuffer, &nMissing)) {
        if (0 == nMissing) {
            printErr(STATE_WARN_CORRUPT);
        }
    } else if (0 != decodeState(pcBuffer, &tLoaded)) {
        freeCargoSupplies(&tLoaded);
        printErr(STATE_WARN_CORRUPT);
    } else {
        freeCargoSupplies(&ptOdysseus->tCargo);
        ptOdysseus->tCargo = tLoaded;
        nRet = 1;
    }
    free(pcBuffer);
    free(psPath);
    return nRet;
}

/***********************************************
 *
 * @Name : odysseusResetState
 * @Def : Deletes the state file so the next execution starts with the
 *        initial resources of the config file (lost ship, dead end).
 * @Arg : In: ptOdysseus = odysseus whose state is forgotten
 * @Ret : None.
 *
 ************************************************/
void odysseusResetState(const Odysseus *ptOdysseus) {
    char *psPath = buildStatePath(ptOdysseus);

    if (NULL != psPath) {
        unlink(psPath);
        free(psPath);
    }
}

/***********************************************
 *
 * @Name : odysseusSaveOnExit
 * @Def : Final save when the process ends. A ship with no gold and no
 *        cargo (lost at sea or dead end, see 6.8) has nothing to
 *        continue with, so its state is reset instead of saved.
 * @Arg : In: ptOdysseus = odysseus that is exiting
 * @Ret : None.
 *
 ************************************************/
void odysseusSaveOnExit(const Odysseus *ptOdysseus) {
    if (0 >= ptOdysseus->tCargo.nTotalAmount &&
        0 >= ptOdysseus->tCargo.nGold) {
        odysseusResetState(ptOdysseus);
    } else {
        odysseusSaveState(ptOdysseus);
    }
}
