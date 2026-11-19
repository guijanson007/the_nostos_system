#define _GNU_SOURCE

#include "utils/utils_transfer.h"
#include "sphragis.h"
#include "utils/utils_pool.h"
#include <fcntl.h>
#include <strings.h>
#include <sys/types.h>

/***********************************************
 *
 * @Name : transferSettings
 * @Def : Gives the constants of a transfer kind (spec 8.4): the text that
 *        starts the HEADER, the flags of the HEADER and the flags of the
 *        DATA frames. Transfers sent as a response (OBJECT, SIGNED) have
 *        0x80 in the HEADER and 0xA0 in DATA; the others 0x00 and 0x20.
 * @Arg : In: eType = kind of transfer
 *        Out: ppsKind = text of the kind (KIND_*)
 *        Out: pcHeaderFlags = flags of the HEADER frame
 *        Out: pcDataFlags = flags of every DATA frame
 * @Ret : TRANSFER_OK, or TRANSFER_ERROR if the kind is unknown.
 *
 ************************************************/
static int transferSettings(TransferType eType, const char **ppsKind,
                            unsigned char *pcHeaderFlags,
                            unsigned char *pcDataFlags) {

    switch (eType) {
        case TRANSFER_CLAIM:
            *ppsKind = KIND_CLAIM;
            *pcHeaderFlags = FLAG_NONE;
            break;
        case TRANSFER_OBJECT:
            *ppsKind = KIND_OBJECT;
            *pcHeaderFlags = FLAG_RESPONSE;
            break;
        case TRANSFER_DELIVERY:
            *ppsKind = KIND_DELIVERY;
            *pcHeaderFlags = FLAG_NONE;
            break;
        case TRANSFER_SIGNED:
            *ppsKind = KIND_SIGNED;
            *pcHeaderFlags = FLAG_RESPONSE;
            break;
        default:
            return TRANSFER_ERROR;
    }
    *pcDataFlags = *pcHeaderFlags | FLAG_BINARY;
    return TRANSFER_OK;
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
 * @Def : Checks a FILE HEADER (<KIND>&<name>&<size>&<md5>) and copies its
 * values into dynamic memory.
 * @Arg : In: ptMsg = frame received
 *        In: eType = transfer expected
 *        Out: ptInfo = name, size and MD5 (free with freeFileInfo)
 * @Ret : TRANSFER_OK if the header is valid, TRANSFER_ERROR otherwise (ptInfo
 * is left empty).
 *
 ************************************************/
int parseHeader(const Message *ptMsg, TransferType eType, FileInfo *ptInfo) {
    const char *psKind = NULL;
    unsigned char cHeaderFlags = FLAG_NONE;
    unsigned char cDataFlags = FLAG_NONE;
    char *psCopy = NULL;
    char *psSave = NULL;
    char *psFieldKind = NULL;
    char *psFieldName = NULL;
    char *psFieldSize = NULL;
    char *psFieldMd5 = NULL;

    if (NULL == ptMsg || NULL == ptInfo ||
        TRANSFER_OK !=
            transferSettings(eType, &psKind, &cHeaderFlags, &cDataFlags)) {
        return TRANSFER_ERROR;
    }
    if (MSG_FILE_HEADER != ptMsg->type || cHeaderFlags != ptMsg->flag ||
        0 == ptMsg->dataLength) {
        return TRANSFER_ERROR;
    }

    memset(ptInfo, 0, sizeof(FileInfo));

    psCopy = strdup(ptMsg->data);
    if (NULL == psCopy) {
        return TRANSFER_ERROR;
    }
    // strtok_r keeps its state in psSave, so parsing is safe in worker threads
    psFieldKind = strtok_r(psCopy, HEADER_SEPARATOR, &psSave);
    psFieldName = strtok_r(NULL, HEADER_SEPARATOR, &psSave);
    psFieldSize = strtok_r(NULL, HEADER_SEPARATOR, &psSave);
    psFieldMd5 = strtok_r(NULL, HEADER_SEPARATOR, &psSave);

    if (NULL == psFieldKind || NULL == psFieldName || NULL == psFieldSize ||
        NULL == psFieldMd5 ||
        NULL != strtok_r(NULL, HEADER_SEPARATOR, &psSave) ||
        0 != strcmp(psFieldKind, psKind) ||
        NULL != strchr(psFieldName, PATH_SEPARATOR_CHAR) ||
        0 == isPositiveInteger(psFieldSize) || !isMd5Text(psFieldMd5)) {
        free(psCopy);
        return TRANSFER_ERROR;
    }

    ptInfo->psFileName = strdup(psFieldName);
    ptInfo->psMd5 = strdup(psFieldMd5);
    ptInfo->nSize = strtol(psFieldSize, NULL, DECIMAL_BASE);
    free(psCopy);

    if (NULL == ptInfo->psFileName || NULL == ptInfo->psMd5) {
        freeFileInfo(ptInfo);
        return TRANSFER_ERROR;
    }
    return TRANSFER_OK;
}

/***********************************************
 *
 * @Name : freeFileInfo
 * @Def : Frees the strings of a FileInfo and leaves it empty, so it is safe
 *        to call it twice or on a struct that was never filled.
 * @Arg : In/Out: ptInfo = info to free (NULL is allowed)
 * @Ret : None.
 *
 ************************************************/
void freeFileInfo(FileInfo *ptInfo) {
    if (NULL == ptInfo) {
        return;
    }
    free(ptInfo->psFileName);
    free(ptInfo->psMd5);
    ptInfo->psFileName = NULL;
    ptInfo->psMd5 = NULL;
    ptInfo->nSize = 0;
}

/***********************************************
 *
 * @Name : sendFrame
 * @Def : Builds a frame with a text payload and sends it.
 * @Arg : In: nFd = socket
 *        In: eType = frame type
 *        In: cFlags = frame flags
 *        In: psText = payload (text)
 * @Ret : TRANSFER_OK, or TRANSFER_PEER_GONE if the write failed.
 *
 ************************************************/
static int sendFrame(int nFd, MessageType eType, unsigned char cFlags,
                     const char *psText) {
    Message tMsg;

    memset(&tMsg, 0, sizeof(Message));
    tMsg.type = eType;
    buildMessage(&tMsg, cFlags, psText);
    if (0 != sendMessage(&tMsg, nFd)) {
        return TRANSFER_PEER_GONE;
    }
    return TRANSFER_OK;
}

/***********************************************
 *
 * @Name : waitFrame
 * @Def : Waits (with the transfer timeout) for one frame of an expected type.
 * @Arg : In: nFd = socket
 *        In: eType = type the frame must have
 *        Out: ptMsg = frame received
 * @Ret : TRANSFER_OK if a frame of the expected type arrived,
 *        TRANSFER_ERROR if the frame has another type or is malformed, or
 *        TRANSFER_PEER_GONE if the peer has left or timed out.
 *
 ************************************************/
static int waitFrame(int nFd, MessageType eType, Message *ptMsg) {
    int nRet = RECEIVE_ERROR;

    // A malformed frame leaves ptMsg untouched: never check a stale frame
    memset(ptMsg, 0, sizeof(Message));
    nRet = receiveMessageTimeout(ptMsg, nFd, TRANSFER_TIMEOUT_SECONDS);
    if (RECEIVE_ERROR == nRet || RECEIVE_TIMEOUT == nRet) {
        return TRANSFER_PEER_GONE;
    }
    if (RECEIVE_OK != nRet || eType != ptMsg->type) {
        return TRANSFER_ERROR;
    }
    return TRANSFER_OK;
}

/***********************************************
 *
 * @Name : readChunk
 * @Def : Reads from a file until the buffer is full or the file ends.
 * @Arg : In: nFile = file descriptor
 *        Out: pcBuffer = where to store the bytes
 *        In: nWanted = bytes to read
 * @Ret : Bytes read (less than nWanted only at end of file or on error).
 *
 ************************************************/
static int readChunk(int nFile, char *pcBuffer, int nWanted) {
    int nTotal = 0;
    int nBytes = 0;

    while (nTotal < nWanted) {
        nBytes = read(nFile, pcBuffer + nTotal, nWanted - nTotal);
        if (0 >= nBytes) {
            break;
        }
        nTotal += nBytes;
    }
    return nTotal;
}

/***********************************************
 *
 * @Name : streamFileOut
 * @Def : Sends the N DATA frames of a file, reading it in chunks of
 *        MESSAGE_DATA_MAX bytes (the file is never loaded whole). The last
 *        frame carries 1 to MESSAGE_DATA_MAX bytes.
 * @Arg : In: nFd = socket
 *        In: nFile = file opened for reading, positioned at the start
 *        In: nSize = bytes to send
 *        In: cDataFlags = flags of the DATA frames
 * @Ret : TRANSFER_OK, TRANSFER_ERROR if the file is shorter than nSize,
 *        TRANSFER_PEER_GONE if a write failed.
 *
 ************************************************/
static int streamFileOut(int nFd, int nFile, long nSize,
                         unsigned char cDataFlags) {
    Message tMsg;
    long nSent = 0;
    int nChunk = 0;

    memset(&tMsg, 0, sizeof(Message));
    tMsg.type = MSG_FILE_DATA;
    while (nSent < nSize) {
        if (nSize - nSent > MESSAGE_DATA_MAX) {
            nChunk = MESSAGE_DATA_MAX;
        } else {
            nChunk = (int)(nSize - nSent);
        }
        if (nChunk != readChunk(nFile, tMsg.data, nChunk)) {
            return TRANSFER_ERROR;
        }
        tMsg.flag = cDataFlags;
        tMsg.dataLength = (unsigned char)nChunk;
        if (0 != sendMessage(&tMsg, nFd)) {
            return TRANSFER_PEER_GONE;
        }
        nSent += nChunk;
    }
    return TRANSFER_OK;
}

/***********************************************
 *
 * @Name : readFileSize
 * @Def : Measures an open file with lseek and rewinds it.
 * @Arg : In: nFile = file descriptor
 * @Ret : Size in bytes, or -1 on error.
 *
 ************************************************/
static long readFileSize(int nFile) {
    off_t tSize = lseek(nFile, 0, SEEK_END);

    if (0 > tSize || 0 > lseek(nFile, 0, SEEK_SET)) {
        return -1;
    }
    return (long)tSize;
}

/***********************************************
 *
 * @Name : sendFile
 * @Def : Sender side of a transfer: HEADER, wait READY, N x DATA, wait the
 *        FINAL ACK. The MD5 is computed with computeMd5 (md5sum).
 * @Arg : In: nFd = socket of the peer
 *        In: eType = kind of transfer (decides the flags)
 *        In: psPath = file to send
 *        In: psFileName = name written in the HEADER
 * @Ret : TRANSFER_OK; TRANSFER_ERROR if the file is missing, empty, too big
 *        or shorter than announced, or the peer broke the protocol;
 *        TRANSFER_REJECTED if the peer answered READY or the ACK with an
 *        error; TRANSFER_SIZE_ERROR / TRANSFER_MD5_ERROR as told by the
 *        peer in the ACK; TRANSFER_PEER_GONE on closed socket or timeout.
 *
 ************************************************/
int sendFile(int nFd, TransferType eType, const char *psPath,
             const char *psFileName) {
    const char *psKind = NULL;
    unsigned char cHeaderFlags = FLAG_NONE;
    unsigned char cDataFlags = FLAG_NONE;
    Message tMsg;
    char *psMd5 = NULL;
    char *psHeader = NULL;
    long nSize = 0;
    int nFile = -1;
    int nRet = TRANSFER_ERROR;

    if (NULL == psPath || NULL == psFileName ||
        TRANSFER_OK !=
            transferSettings(eType, &psKind, &cHeaderFlags, &cDataFlags)) {
        return TRANSFER_ERROR;
    }
    nFile = open(psPath, O_RDONLY);
    if (0 > nFile) {
        return TRANSFER_ERROR;
    }
    nSize = readFileSize(nFile);
    psMd5 = computeMd5(psPath);
    if (0 >= nSize || TRANSFER_MAX_SIZE < nSize || NULL == psMd5 ||
        -1 == asprintf(&psHeader, HEADER_FORMAT, psKind, psFileName, nSize,
                       psMd5)) {
        psHeader = NULL;
    }
    free(psMd5);

    if (NULL != psHeader && MESSAGE_DATA_MAX >= strlen(psHeader)) {
        nRet = sendFrame(nFd, MSG_FILE_HEADER, cHeaderFlags, psHeader);
        if (TRANSFER_OK == nRet) {
            nRet = waitFrame(nFd, MSG_FILE_READY, &tMsg);
        }
        if (TRANSFER_OK == nRet && FLAG_RESPONSE != tMsg.flag) {
            if ((FLAG_RESPONSE | FLAG_ERROR) == tMsg.flag) {
                nRet = TRANSFER_REJECTED;
            } else {
                nRet = TRANSFER_ERROR;
            }
        }
        if (TRANSFER_OK == nRet) {
            nRet = streamFileOut(nFd, nFile, nSize, cDataFlags);
        }
        if (TRANSFER_OK == nRet) {
            nRet = waitFrame(nFd, MSG_FILE_ACK, &tMsg);
        }
        if (TRANSFER_OK == nRet && FLAG_RESPONSE != tMsg.flag) {
            if (0 == strcmp(tMsg.data, REPLY_SIZE_ERROR)) {
                nRet = TRANSFER_SIZE_ERROR;
            } else if (0 == strcmp(tMsg.data, REPLY_MD5_ERROR)) {
                nRet = TRANSFER_MD5_ERROR;
            } else {
                nRet = TRANSFER_REJECTED;
            }
        }
    }
    free(psHeader);
    close(nFile);
    return nRet;
}

/***********************************************
 *
 * @Name : rejectTransfer
 * @Def : Answers a HEADER with READY and the error flags, so the sender
 *        does not start sending DATA.
 * @Arg : In: nFd = socket of the sender
 *        In: psReason = reason (payload of the READY)
 * @Ret : TRANSFER_OK, or TRANSFER_PEER_GONE if the write failed.
 *
 ************************************************/
int rejectTransfer(int nFd, const char *psReason) {
    return sendFrame(nFd, MSG_FILE_READY, FLAG_RESPONSE | FLAG_ERROR, psReason);
}

/***********************************************
 *
 * @Name : writeChunk
 * @Def : Writes a whole buffer to a file, retrying on partial writes.
 * @Arg : In: nFile = file descriptor
 *        In: pcBuffer = bytes to write
 *        In: nLength = number of bytes
 * @Ret : 1 if everything was written, 0 otherwise.
 *
 ************************************************/
static int writeChunk(int nFile, const char *pcBuffer, int nLength) {
    int nTotal = 0;
    int nBytes = 0;

    while (nTotal < nLength) {
        nBytes = write(nFile, pcBuffer + nTotal, nLength - nTotal);
        if (0 >= nBytes) {
            return 0;
        }
        nTotal += nBytes;
    }
    return 1;
}

/***********************************************
 *
 * @Name : streamFileIn
 * @Def : Receives the N DATA frames of a transfer (N = size / 253 rounded
 *        up) and writes each one to the file. All N frames are always read,
 *        even after a size error, so no frame is left on the socket for the
 *        server loop.
 * @Arg : In: nFd = socket of the sender
 *        In: nFile = file opened for writing
 *        In: nSize = bytes announced in the HEADER
 *        In: cDataFlags = flags every DATA frame must have
 * @Ret : TRANSFER_OK; TRANSFER_SIZE_ERROR if a frame has the wrong length or
 *        the file could not be written; TRANSFER_PEER_GONE if the socket
 *        closed, timed out or a frame broke the protocol.
 *
 ************************************************/
static int streamFileIn(int nFd, int nFile, long nSize,
                        unsigned char cDataFlags) {
    Message tMsg;
    long nFrames = (nSize + MESSAGE_DATA_MAX - 1) / MESSAGE_DATA_MAX;
    long nReceived = 0;
    long nExpected = 0;
    long i = 0;
    int nRet = TRANSFER_OK;

    for (i = 0; i < nFrames; i++) {
        if (nSize - nReceived > MESSAGE_DATA_MAX) {
            nExpected = MESSAGE_DATA_MAX;
        } else {
            nExpected = (nSize - nReceived);
        }

        if (TRANSFER_OK != waitFrame(nFd, MSG_FILE_DATA, &tMsg) ||
            cDataFlags != tMsg.flag) {
            return TRANSFER_PEER_GONE;
        }
        if (nExpected != tMsg.dataLength) {
            nRet = TRANSFER_SIZE_ERROR;
        } else if (TRANSFER_OK == nRet &&
                   !writeChunk(nFile, tMsg.data, nExpected)) {
            nRet = TRANSFER_SIZE_ERROR;
        }
        nReceived += nExpected;
    }
    return nRet;
}

/***********************************************
 *
 * @Name : receiveBody
 * @Def : Receiver side once the HEADER has been read and parsed: sends
 *        READY, stores the DATA frames in psDestPath, checks the MD5 of the
 *        stored file with md5sum and sends the FINAL ACK (OK, SIZE_ERROR or
 *        MD5_ERROR). If anything fails, the partial file is deleted.
 * @Arg : In: nFd = socket of the sender
 *        In: eType = kind of transfer
 *        In: psDestPath = where to store the file (created or truncated)
 *        In: ptInfo = parsed HEADER (size and MD5 announced)
 * @Ret : TRANSFER_OK, TRANSFER_SIZE_ERROR, TRANSFER_MD5_ERROR (both already
 *        told to the sender), TRANSFER_PEER_GONE, or TRANSFER_ERROR (bad
 *        arguments, file cannot be created, protocol violation).
 *
 ************************************************/
int receiveBody(int nFd, TransferType eType, const char *psDestPath,
                const FileInfo *ptInfo) {
    const char *psKind = NULL;
    unsigned char cHeaderFlags = FLAG_NONE;
    unsigned char cDataFlags = FLAG_NONE;
    char *psMd5 = NULL;
    int nFile = -1;
    int nRet = TRANSFER_ERROR;

    if (NULL == psDestPath || NULL == ptInfo || NULL == ptInfo->psMd5 ||
        0 >= ptInfo->nSize ||
        TRANSFER_OK !=
            transferSettings(eType, &psKind, &cHeaderFlags, &cDataFlags)) {
        return TRANSFER_ERROR;
    }
    nFile = open(psDestPath, O_WRONLY | O_CREAT | O_TRUNC, TRANSFER_FILE_MODE);
    if (0 > nFile) {
        rejectTransfer(nFd, REPLY_FILE_ERROR);
        return TRANSFER_ERROR;
    }

    nRet = sendFrame(nFd, MSG_FILE_READY, FLAG_RESPONSE, REPLY_READY);
    if (TRANSFER_OK == nRet) {
        nRet = streamFileIn(nFd, nFile, ptInfo->nSize, cDataFlags);
    }
    if (0 != close(nFile) && TRANSFER_OK == nRet) {
        nRet = TRANSFER_ERROR;
    }

    if (TRANSFER_OK == nRet) {
        psMd5 = computeMd5(psDestPath);
        if (NULL == psMd5) {
            nRet = TRANSFER_ERROR;
        } else if (0 != strcasecmp(psMd5, ptInfo->psMd5)) {
            nRet = TRANSFER_MD5_ERROR;
        }
        free(psMd5);
    }

    // Only a verdict about the content is answered; a protocol violation or
    // a dead peer gets no ACK (the sender will give up on its own timeout)
    if (TRANSFER_OK == nRet) {
        nRet = sendFrame(nFd, MSG_FILE_ACK, FLAG_RESPONSE, REPLY_OK);
    } else if (TRANSFER_SIZE_ERROR == nRet) {
        sendFrame(nFd, MSG_FILE_ACK, FLAG_RESPONSE | FLAG_ERROR,
                  REPLY_SIZE_ERROR);
    } else if (TRANSFER_MD5_ERROR == nRet) {
        sendFrame(nFd, MSG_FILE_ACK, FLAG_RESPONSE | FLAG_ERROR,
                  REPLY_MD5_ERROR);
    }

    if (TRANSFER_OK != nRet) {
        unlink(psDestPath);
    }
    return nRet;
}

/***********************************************
 *
 * @Name : receiveFile
 * @Def : Complete receiver side: waits for the HEADER, validates it (kind,
 *        flags, size, MD5) and runs receiveBody. An invalid HEADER is
 *        answered with READY and the error flags.
 * @Arg : In: nFd = socket of the sender
 *        In: eType = kind of transfer expected
 *        In: psDestPath = where to store the file
 *        Out: ptInfo = the HEADER (free with freeFileInfo); filled only if
 *             it was valid
 * @Ret : same codes as receiveBody; TRANSFER_PEER_GONE if no HEADER arrived.
 *
 ************************************************/
int receiveFile(int nFd, TransferType eType, const char *psDestPath,
                FileInfo *ptInfo) {
    Message tMsg;
    int nRet = TRANSFER_ERROR;

    if (NULL == ptInfo) {
        return TRANSFER_ERROR;
    }
    memset(ptInfo, 0, sizeof(FileInfo));

    nRet = waitFrame(nFd, MSG_FILE_HEADER, &tMsg);
    if (TRANSFER_OK != nRet) {
        return nRet;
    }
    return receiveFileFromHeader(nFd, eType, psDestPath, &tMsg, ptInfo);
}

/***********************************************
 *
 * @Name : receiveFileFromHeader
 * @Def : Receiver side for a caller that already read the HEADER frame
 *        (for example to tell it apart from an error reply): validates it
 *        and runs receiveBody. An invalid HEADER is answered with READY
 *        and the error flags.
 * @Arg : In: nFd = socket of the sender
 *        In: eType = kind of transfer expected
 *        In: psDestPath = where to store the file
 *        In: ptHeader = HEADER frame received
 *        Out: ptInfo = the HEADER (free with freeFileInfo); filled only if
 *             it was valid
 * @Ret : same codes as receiveBody.
 *
 ************************************************/
int receiveFileFromHeader(int nFd, TransferType eType, const char *psDestPath,
                          const Message *ptHeader, FileInfo *ptInfo) {
    if (NULL == ptHeader || NULL == ptInfo) {
        return TRANSFER_ERROR;
    }
    memset(ptInfo, 0, sizeof(FileInfo));

    if (TRANSFER_OK != parseHeader(ptHeader, eType, ptInfo)) {
        rejectTransfer(nFd, REPLY_BAD_HEADER);
        return TRANSFER_ERROR;
    }
    return receiveBody(nFd, eType, psDestPath, ptInfo);
}

/***********************************************
 *
 * @Name : buildRecordPath
 * @Def : Builds the path where a server keeps an image it received for a
 *        voyage: <folder>/<Ship>_<VoyageID>_<file>. These are the server's
 *        own files (proof of each delivery or claim), not temporary state.
 * @Arg : In: psFolder = folder of the server (from its config file)
 *        In: psShip = name of the ship
 *        In: nVoyageId = voyage the image belongs to
 *        In: psFileName = name of the file in the HEADER
 * @Ret : Allocated path (the caller frees it), or NULL if a name contains a
 *        slash (it could leave the folder) or memory ran out.
 *
 ************************************************/
char *buildRecordPath(const char *psFolder, const char *psShip, int nVoyageId,
                      const char *psFileName) {
    char *psPath = NULL;

    if (NULL == psFolder || NULL == psShip || NULL == psFileName ||
        NULL != strchr(psShip, PATH_SEPARATOR_CHAR) ||
        NULL != strchr(psFileName, PATH_SEPARATOR_CHAR)) {
        return NULL;
    }
    if (-1 == asprintf(&psPath, RECORD_PATH_FORMAT, psFolder, psShip, nVoyageId,
                       psFileName)) {
        return NULL;
    }
    return psPath;
}

/***********************************************
 *
 * @Name : jobHasArgs
 * @Def : Checks that a job carries what its type needs.
 * @Arg : In: ptJob = job to check
 * @Ret : 1 if usable, 0 otherwise.
 *
 ************************************************/
static int jobHasArgs(const PoolJob *ptJob) {
    if (NULL == ptJob->psPath || 0 > ptJob->nFd) {
        return 0;
    }
    switch (ptJob->eType) {
        case JOB_SEND_FILE:
            return NULL != ptJob->psFileName;
        case JOB_RECEIVE_FILE:
            return 1;
        case JOB_SEAL_DELIVERY:
        case JOB_VERIFY_CLAIM:
            // The HEADER was read by the server loop and travels in tInfo
            return NULL != ptJob->psOwner && NULL != ptJob->tInfo.psMd5 &&
                   NULL != ptJob->tInfo.psFileName;
    }
    return 0;
}

/***********************************************
 *
 * @Name : verifyClaim
 * @Def : Ithaca job: receives the CLAIM image (the HEADER is already in
 *        tInfo), checks it is the object of the voyage and that it carries
 *        the glyph of the destination island. A valid image is kept as the
 *        proof of the claim; an incomplete or rejected one is deleted.
 * @Arg : In/Out: ptJob = psPath where the image is kept, psFileName
 *        expected object name,
 *        psOwner destination island, tInfo the parsed HEADER
 * @Ret : TRANSFER_OK, TRANSFER_WRONG_FILE (name differs from the mission
 *        object), TRANSFER_GLYPH_ERROR (no glyph of the island, or the
 *        image cannot be read), or any code of receiveBody.
 *
 ************************************************/
static int verifyClaim(PoolJob *ptJob) {
    int nRet = TRANSFER_OK;

    if (NULL != ptJob->psFileName &&
        0 != strcmp(ptJob->psFileName, ptJob->tInfo.psFileName)) {
        rejectTransfer(ptJob->nFd, REPLY_BAD_HEADER);
        return TRANSFER_WRONG_FILE;
    }
    nRet =
        receiveBody(ptJob->nFd, TRANSFER_CLAIM, ptJob->psPath, &ptJob->tInfo);
    if (TRANSFER_OK != nRet) {
        return nRet;
    }
    if (1 != SPHRAGIS_has_glyph(ptJob->psOwner, ptJob->psPath)) {
        unlink(ptJob->psPath);
        nRet = TRANSFER_GLYPH_ERROR;
    }
    return nRet;
}

/***********************************************
 *
 * @Name : sealDelivery
 * @Def : Island job: receives the DELIVERY image (HEADER already in
 *        tInfo), seals it with the glyph of psOwner (this island) into
 *        <psPath>.signed and sends it back as SIGNED. After a complete
 *        delivery the island keeps both images; if any step fails both are
 *        deleted, so a partial image is never kept or sealed.
 * @Arg : In/Out: ptJob = psPath where the received image is kept, psOwner
 *        island name, psFileName name used in the SIGNED HEADER (the
 *        received name if NULL), tInfo the parsed HEADER
 * @Ret : TRANSFER_OK, TRANSFER_GLYPH_ERROR if SPHRAGIS could not seal the
 *        image, TRANSFER_ERROR if memory ran out, or any code of
 *        receiveBody / sendFile.
 *
 ************************************************/
static int sealDelivery(PoolJob *ptJob) {
    char *psSignedPath = NULL;
    const char *psName = ptJob->psFileName;
    int nRet = receiveBody(ptJob->nFd, TRANSFER_DELIVERY, ptJob->psPath,
                           &ptJob->tInfo);

    if (NULL == psName) {
        psName = ptJob->tInfo.psFileName;
    }

    if (TRANSFER_OK != nRet) {
        return nRet;
    }
    if (-1 == asprintf(&psSignedPath, SIGNED_PATH_FORMAT, ptJob->psPath)) {
        unlink(ptJob->psPath);
        return TRANSFER_ERROR;
    }
    if (SPHRAGIS_OK !=
        SPHRAGIS_sign_with_glyph(ptJob->psOwner, ptJob->psPath, psSignedPath)) {
        nRet = TRANSFER_GLYPH_ERROR;
    } else {
        nRet = sendFile(ptJob->nFd, TRANSFER_SIGNED, psSignedPath, psName);
    }
    if (TRANSFER_OK != nRet) {
        unlink(ptJob->psPath);
        unlink(psSignedPath);
    }
    free(psSignedPath);
    return nRet;
}

/***********************************************
 *
 * @Name : executeJob
 * @Def : Runs one pool job on the worker thread and stores a TRANSFER_*
 *        code in ptJob->nResult (TRANSFER_OK is JOB_RESULT_OK). Only this
 *        worker touches ptJob->nFd meanwhile. Everything is reentrant: no
 *        static buffers and no strtok.
 *        JOB_SEND_FILE sends psPath as an OBJECT (Ithaca after ACCEPT);
 *        JOB_RECEIVE_FILE waits for a CLAIM HEADER itself and stores the
 *        file in psPath, keeping it (the caller owns it);
 *        JOB_SEAL_DELIVERY and JOB_VERIFY_CLAIM are described above.
 * @Arg : In/Out: ptJob = job to run
 * @Ret : None.
 *
 ************************************************/
void executeJob(PoolJob *ptJob) {
    if (NULL == ptJob) {
        return;
    }
    if (!jobHasArgs(ptJob)) {
        ptJob->nResult = TRANSFER_ERROR;
        return;
    }

    switch (ptJob->eType) {
        case JOB_SEND_FILE:
            ptJob->nResult = sendFile(ptJob->nFd, TRANSFER_OBJECT,
                                      ptJob->psPath, ptJob->psFileName);
            break;
        case JOB_RECEIVE_FILE:
            freeFileInfo(&ptJob->tInfo);
            ptJob->nResult = receiveFile(ptJob->nFd, TRANSFER_CLAIM,
                                         ptJob->psPath, &ptJob->tInfo);
            break;
        case JOB_SEAL_DELIVERY:
            ptJob->nResult = sealDelivery(ptJob);
            break;
        case JOB_VERIFY_CLAIM:
            ptJob->nResult = verifyClaim(ptJob);
            break;
        default:
            ptJob->nResult = TRANSFER_ERROR;
            break;
    }
}
