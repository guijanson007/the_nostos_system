#ifndef _ODYSSEUS_PERSISTENCE_H_
#define _ODYSSEUS_PERSISTENCE_H_

/***********************************************
 *
 * @File : odysseus_persistence.h
 * @Purpose : declarations for saving and restoring the state of the
 *            odysseus (gold and cargo) in a fixed-record binary file, one
 *            file per ship.
 * @Author : Guilherme Rodrigues
 * @Date : 16/11/2026
 *
 ************************************************/

// Project includes
#include "odysseus/odysseus.h"

/*
 * File layout (all integers are 32 bit, in the byte order of the machine,
 * copied field by field into a byte buffer so the struct padding of the
 * compiler never reaches the file):
 *
 *   offset 0                 magic        (STATE_MAGIC)
 *   offset 4                 version      (STATE_VERSION)
 *   offset 8                 gold
 *   offset 12                number of supplies in use
 *   offset 16                STATE_MAX_SUPPLIES slots of STATE_SLOT_SIZE:
 *                              STATE_NAME_SIZE bytes name (NUL padded)
 *                              4 bytes amount
 *   offset STATE_SUM_OFFSET  checksum: sum of every previous byte
 *
 * The file always has exactly STATE_RECORD_SIZE bytes.
 */

// Constant declarations
#define STATE_FILE_FORMAT "%s.state"
#define STATE_FILE_MODE 0644
#define STATE_MAGIC 0x4E4F5354u
#define STATE_VERSION 1u
#define STATE_FIELD_SIZE 4
#define STATE_HEADER_FIELDS 4
#define STATE_HEADER_SIZE (STATE_HEADER_FIELDS * STATE_FIELD_SIZE)
#define STATE_MAX_SUPPLIES 32
#define STATE_NAME_SIZE 64
#define STATE_SLOT_SIZE (STATE_NAME_SIZE + STATE_FIELD_SIZE)
#define STATE_SUM_OFFSET (STATE_HEADER_SIZE + STATE_MAX_SUPPLIES * STATE_SLOT_SIZE)
#define STATE_RECORD_SIZE (STATE_SUM_OFFSET + STATE_FIELD_SIZE)

#define STATE_WARN_SAVE "WARNING: Could not save the state of the odysseus"
#define STATE_WARN_CORRUPT "WARNING: The saved state is invalid, using the initial resources"

// Functions
int odysseusSaveState(const Odysseus *ptOdysseus);
int odysseusLoadState(Odysseus *ptOdysseus);
void odysseusResetState(const Odysseus *ptOdysseus);
void odysseusSaveOnExit(const Odysseus *ptOdysseus);

#endif
