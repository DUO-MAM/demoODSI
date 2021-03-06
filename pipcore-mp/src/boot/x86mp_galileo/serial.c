/*******************************************************************************/
/*  © Université Lille 1, The Pip Development Team (2015-2017)                 */
/*                                                                             */
/*  This software is a computer program whose purpose is to run a minimal,     */
/*  hypervisor relying on proven properties such as memory isolation.          */
/*                                                                             */
/*  This software is governed by the CeCILL license under French law and       */
/*  abiding by the rules of distribution of free software.  You can  use,      */
/*  modify and/ or redistribute the software under the terms of the CeCILL     */
/*  license as circulated by CEA, CNRS and INRIA at the following URL          */
/*  "http://www.cecill.info".                                                  */
/*                                                                             */
/*  As a counterpart to the access to the source code and  rights to copy,     */
/*  modify and redistribute granted by the license, users are provided only    */
/*  with a limited warranty  and the software's author,  the holder of the     */
/*  economic rights,  and the successive licensors  have only  limited         */
/*  liability.                                                                 */
/*                                                                             */
/*  In this respect, the user's attention is drawn to the risks associated     */
/*  with loading,  using,  modifying and/or developing or reproducing the      */
/*  software by the user in light of its specific status of free software,     */
/*  that may mean  that it is complicated to manipulate,  and  that  also      */
/*  therefore means  that it is reserved for developers  and  experienced      */
/*  professionals having in-depth computer knowledge. Users are therefore      */
/*  encouraged to load and test the software's suitability as regards their    */
/*  requirements in conditions enabling the security of their systems and/or   */
/*  data to be ensured and,  more generally, to use and operate it in the      */
/*  same conditions as regards security.                                       */
/*                                                                             */
/*  The fact that you are presently reading this means that you have had       */
/*  knowledge of the CeCILL license and that you accept its terms.             */
/*******************************************************************************/

/**
 * \file serial.c
 * \brief Serial driver for debugging purposes
 */
#include "serial.h"
#include "port.h"
#include "lock.h"
#include "debug.h"
#include "galileo-support.h"
volatile int kprintf_lock = 0;

#define PORT 0x3f8 //!< Serial port COM1 number

/**
 * \fn void initSerial()
 * \brief Initializes the serial port
 */
void initSerial() {
  initGalileoSerial(DEBUG_SERIAL);
}

/**
 * \fn int serialReceived()
 * \brief Checks whether we received some data on the serial port
 * \return 1 if some data is available, 0 else
 */
int serialReceived() {
       return 0;
}

/**
 * \fn char readSerial()
 * \brief Gets a character from the serial port
 * \return The character
 */
char readSerial() {
  return galileoSerialGetc();
}

/**
 * \fn int isTransmitEmpty()
 * \brief Checks whether our serial line buffer is empty or not
 * \return 0 if it's not empty, 1 else
 */
int isTransmitEmpty() {
       return 0;
}

/**
 * \fn void writeSerial(char a)
 * \brief Writes a character into the serial port
 * \param a The character to write
 */
void writeSerial(char a) {
  galileoSerialPrintc(a);
}



void slputs_sync(char* str)
{
    MP_LOCK(kprintf_lock);
    krn_puts(str);
    MP_UNLOCK(kprintf_lock);
}
