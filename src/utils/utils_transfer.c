
#include "utils/utils_transfer.h"
 

static int transferSettings(TransferType eType, const char **ppsKind, unsigned char *pcHeaderFlags, unsigned char *pcReplyFlags) {

    switch (eType) {
        case TRANSFER_CLAIM:
            *ppsKind = KIND_CLAIM;
            *pcHeaderFlags = FLAG_NONE;
            *pcReplyFlags = FLAG_RESPONSE;
            return TRANSFER_OK;
        case TRANSFER_OBJECT:
            *ppsKind = KIND_OBJECT;
            *pcHeaderFlags = FLAG_RESPONSE;
            *pcReplyFlags = FLAG_NONE;
            return TRANSFER_OK;
        case TRANSFER_DELIVERY:
            *ppsKind = KIND_DELIVERY;
            *pcHeaderFlags = FLAG_NONE;
            *pcReplyFlags = FLAG_RESPONSE;
            return TRANSFER_OK;
        case TRANSFER_SIGNED:
            *ppsKind = KIND_SIGNED;
            *pcHeaderFlags = FLAG_RESPONSE;
            *pcReplyFlags = FLAG_NONE;
            return TRANSFER_OK;
    }

    return TRANSFER_ERROR;
}

/***********************************************
 *
 * @Name : isMd5Text
 * @Def : Checks that a string is exactly MD5_LENGTH hex characters.
 * @Arg : In: psText = string to check
 * @Ret : 1 if it is a valid MD5, 0 otherwise.
 *
 ************************************************/
static int isMd5Text(const char *psText) {
    int i = 0;

    if (MD5_LENGTH != strlen(psText)) {
        return 0;
    }
    for (i = 0; i < MD5_LENGTH; i++) {
        if (!(('0' <= psText[i] && psText[i] <= '9') ||
              ('a' <= psText[i] && psText[i] <= 'f') ||
              ('A' <= psText[i] && psText[i] <= 'F'))) {
            return 0;
        }
    }
    return 1;
}

/***********************************************
 *
 * @Name : parseHeader
 * @Def : Checks a FILE HEADER (<KIND>&<name>&<size>&<md5>) and copies its values into dynamic memory.
 * @Arg : In: ptMsg = frame received
 *        In: eType = transfer expected
 *        Out: ptInfo = name, size and MD5 (free with freeFileInfo)
 * @Ret : TRANSFER_OK if the header is valid, TRANSFER_ERROR otherwise (ptInfo is left empty).
 *
 ************************************************/
int parseHeader(const Message *ptMsg, TransferType eType, FileInfo *ptInfo) {
    const char *psKind = NULL;
    unsigned char cHeaderFlags = FLAG_NONE;
    unsigned char cReplyFlags = FLAG_NONE;
    char *psCopy = NULL;
    char *psSave = NULL;
    char *psFieldKind = NULL;
    char *psFieldName = NULL;
    char *psFieldSize = NULL;
    char *psFieldMd5 = NULL;
    char *psEnd = NULL;
    long nSize = 0;
    int i = 0;

    if (NULL == ptMsg || NULL == ptInfo ||
        TRANSFER_OK != transferSettings(eType, &psKind, &cHeaderFlags,
                                        &cReplyFlags)) {
        return TRANSFER_ERROR;
    }
    if (MSG_FILE_HEADER != ptMsg->type || cHeaderFlags != ptMsg->flag ||
        0 == ptMsg->dataLength) {
        return TRANSFER_ERROR;
    }

    psCopy = strdup(ptMsg->data);
    if (NULL == psCopy) {
        return TRANSFER_ERROR;
    }
    psFieldKind = strtok_r(psCopy, HEADER_SEPARATOR, &psSave);
    psFieldName = strtok_r(NULL, HEADER_SEPARATOR, &psSave);
    psFieldSize = strtok_r(NULL, HEADER_SEPARATOR, &psSave);
    psFieldMd5 = strtok_r(NULL, HEADER_SEPARATOR, &psSave);

     if (NULL != psFieldMd5) {
        nSize = strtol(psFieldSize, &psEnd, DECIMAL_BASE);
    }
    if (NULL == psFieldMd5 ||
        NULL != strtok_r(NULL, HEADER_SEPARATOR, &psSave) ||
        0 != strcmp(psFieldKind, psKind) || '\0' != *psEnd || 0 >= nSize ||
        !isMd5Text(psFieldMd5)) {
        free(psCopy);
        return TRANSFER_ERROR;
    }

    ptInfo->psFileName = strdup(psFieldName);
    ptInfo->psMd5 = strdup(psFieldMd5);
    ptInfo->nSize = nSize;
    free(psCopy);

    if (NULL == ptInfo->psFileName || NULL == ptInfo->psMd5) {
        freeFileInfo(ptInfo);
        return TRANSFER_ERROR;
    }
    return TRANSFER_OK;
}