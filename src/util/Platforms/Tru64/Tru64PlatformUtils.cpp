/*
 * The Apache Software License, Version 1.1
 * 
 * Copyright (c) 1999-2000 The Apache Software Foundation.  All rights
 * reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:  
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 * 
 * 4. The names "Xerces" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written 
 *    permission, please contact apache\@apache.org.
 * 
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation, and was
 * originally based on software copyright (c) 1999, International
 * Business Machines, Inc., http://www.ibm.com .  For more information
 * on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

/*
 * $Id$
 */

// ---------------------------------------------------------------------------
//  Includes
// ---------------------------------------------------------------------------

#if !defined (APP_NO_THREADS)
#include    <pthread.h>
#endif // APP_NO_THREADS

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include    <util/Janitor.hpp>
#include    <util/PlatformUtils.hpp>
#include    <util/RuntimeException.hpp>
#include    <util/Mutexes.hpp>
#include    <util/XMLString.hpp>
#include    <util/XMLUni.hpp>

//
//  These control which transcoding service is used by the Tru64 version.
//  They allow this to be controlled from the build process by just defining
//  one of these values.
//

#if defined (XML_USE_ICU_TRANSCODER)
#include <util/Transcoders/ICU/ICUTransService.hpp>
#else   // use native transcoder
#include <util/Transcoders/Iconv/IconvTransService.hpp>
#endif

//
//  These control which message loading service is used by the Tru64 version.
//  They allow this to be controlled from the build process by just defining
//  one of these values.
//

#if defined (XML_USE_ICU_MESSAGELOADER)
#include <util/MsgLoaders/ICU/ICUMsgLoader.hpp>
#elif defined (XML_USE_ICONV_MESSAGELOADER)
#include <util/MsgLoaders/MsgCatalog/MsgCatalogLoader.hpp>
#else   // use In-memory message loader
#include <util/MsgLoaders/InMemory/InMemMsgLoader.hpp>
#endif


// ---------------------------------------------------------------------------
//  Local Methods
// ---------------------------------------------------------------------------
static void WriteCharStr( FILE* stream, const char* const toWrite)
{
  if (fputs(toWrite, stream) == EOF)
  {
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::Strm_StdErrWriteFailure);
  }
}

static void WriteUStrStdErr( const XMLCh* const toWrite)
{
  char* tmpVal = XMLString::transcode(toWrite);
  ArrayJanitor<char> janText(tmpVal);
  if (fputs(tmpVal, stderr) == EOF)
  {
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::Strm_StdErrWriteFailure);
  }
}

static void WriteUStrStdOut( const XMLCh* const toWrite)
{
  char* tmpVal = XMLString::transcode(toWrite);
  ArrayJanitor<char> janText(tmpVal);
  if (fputs(tmpVal, stdout) == EOF)
  {
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::Strm_StdOutWriteFailure);
  }
}

XMLNetAccessor* XMLPlatformUtils::makeNetAccessor()
{
  return 0;
}

// ---------------------------------------------------------------------------
//  XMLPlatformUtils: Platform init method
// ---------------------------------------------------------------------------

static pthread_mutex_t* gAtomicOpMutex =0 ;

void XMLPlatformUtils::platformInit()
{
  //
  // The gAtomicOpMutex mutex needs to be created 
  // because compareAndSwap and incrementlocation and decrementlocation 
  // does not have the atomic system calls for usage
  // Normally, mutexes are created on first use, but there is a
  // circular dependency between compareAndExchange() and
  // mutex creation that must be broken.

  gAtomicOpMutex = new pthread_mutex_t;	

  if (pthread_mutex_init(gAtomicOpMutex, NULL))
    panic( XMLPlatformUtils::Panic_SystemInit );
}


// ---------------------------------------------------------------------------
//  XMLPlatformUtils: Private Static Methods
// ---------------------------------------------------------------------------

//
//  This method is called by the platform independent part of this class
//  when client code asks to have one of the supported message sets loaded.
//  In our case, we use the ICU based message loader mechanism.
//
XMLMsgLoader* XMLPlatformUtils::loadAMsgSet(const XMLCh* const msgDomain)
{
  XMLMsgLoader* retVal;
  try
  {
#if defined (XML_USE_ICU_MESSAGELOADER)
    retVal = new ICUMsgLoader(msgDomain);
#elif defined (XML_USE_ICONV_MESSAGELOADER)
    retVal = new MsgCatalogLoader(msgDomain);
#else
    retVal = new InMemMsgLoader(msgDomain);
#endif
  }
  catch(...)
  {
    panic(XMLPlatformUtils::Panic_NoDefTranscoder);
  }
  return retVal;
}

//
//  This method is called very early in the bootstrapping process. This guy
//  must create a transcoding service and return it. It cannot use any string
//  methods, any transcoding services, throw any exceptions, etc... It just
//  makes a transcoding service and returns it, or returns zero on failure.
//

XMLTransService* XMLPlatformUtils::makeTransService()
#if defined (XML_USE_ICU_TRANSCODER)
{
  return new ICUTransService;
}
#elif defined (XML_USE_ICONV_TRANSCODER)
{
  return new IconvTransService;
}
#else // Use Native transcoding service
{
  return new IconvTransService;
}
#endif


// ---------------------------------------------------------------------------
//  XMLPlatformUtils: The panic method
// ---------------------------------------------------------------------------
void XMLPlatformUtils::panic(const PanicReasons reason)
{
  const char* reasonStr = "Unknown reason";
  if (reason == Panic_NoTransService)
    reasonStr = "Could not load a transcoding service";
  else if (reason == Panic_NoDefTranscoder)
    reasonStr = "Could not load a local code page transcoder";
  else if (reason == Panic_CantFindLib)
    reasonStr = "Could not find the xerces-c DLL";
  else if (reason == Panic_UnknownMsgDomain)
    reasonStr = "Unknown message domain";
  else if (reason == Panic_CantLoadMsgDomain)
    reasonStr = "Cannot load message domain";
  else if (reason == Panic_SynchronizationErr)
    reasonStr = "Cannot synchronize system or mutex";
  else if (reason == Panic_SystemInit)
    reasonStr = "Cannot initialize the system or mutex";

  fprintf(stderr, "%s\n", reasonStr);
    
  exit(-1);
}

// ---------------------------------------------------------------------------
//  XMLPlatformUtils: File Methods
// ---------------------------------------------------------------------------
unsigned int XMLPlatformUtils::curFilePos(FileHandle theFile)
{
  // Get the current position
  int curPos = ftell( (FILE*)theFile);
  if (curPos == -1)
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotGetSize);

  return (unsigned int)curPos;
}

void XMLPlatformUtils::closeFile(FileHandle theFile)
{
  if (fclose((FILE*) theFile))
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotCloseFile);
}

unsigned int XMLPlatformUtils::fileSize(FileHandle theFile)
{
  // Get the current position
  long  int curPos = ftell((FILE*) theFile);
  if (curPos == -1)
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotGetCurPos);

  // Seek to the end and save that value for return
  if (fseek( (FILE*) theFile, 0, SEEK_END) )
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotSeekToEnd);

  long int retVal = ftell((FILE*) theFile);
  if (retVal == -1)
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotSeekToEnd);

  // And put the pointer back
  if (fseek((FILE*) theFile, curPos, SEEK_SET))
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotSeekToPos);

  return (unsigned int)retVal;
}

FileHandle XMLPlatformUtils::openFile(const char* const fileName) 
{
  FileHandle retVal = (FILE*)fopen( fileName , "rb" );

  if (retVal == NULL)
    return 0;
  return retVal;
}

FileHandle XMLPlatformUtils::openFile(const XMLCh* const fileName)
{
  const char* tmpFileName = XMLString::transcode(fileName);
  ArrayJanitor<char> janText((char*)tmpFileName);
  FileHandle retVal = (FILE*)fopen( tmpFileName , "rb" );
    
  if (retVal == NULL)
    return 0;
  return retVal;
}

unsigned int XMLPlatformUtils::readFileBuffer(FileHandle theFile,
					      const unsigned int toRead,
					      XMLByte* const toFill)
{
  size_t noOfItemsRead =
    fread((void*) toFill, 1, toRead, (FILE*) theFile);

  if(ferror((FILE*) theFile))
  {
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotReadFromFile);
  }

  return (unsigned int) noOfItemsRead;
}

void XMLPlatformUtils::resetFile(FileHandle theFile)
{
  // Seek to the start of the file
  if (fseek((FILE*) theFile, 0, SEEK_SET))
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotResetFile);
}



// ---------------------------------------------------------------------------
//  XMLPlatformUtils: Timing Methods
// ---------------------------------------------------------------------------

unsigned long XMLPlatformUtils::getCurrentMillis()
{
  timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return (unsigned long) ((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

XMLCh* XMLPlatformUtils::getFullPath(const XMLCh* const srcPath)
{
  //
  //  NOTE: THe path provided has always already been opened successfully,
  //  so we know that its not some pathological freaky path. It comes in
  //  in native format, and goes out as Unicode always
  //
  char* newSrc = XMLString::transcode(srcPath);
  ArrayJanitor<char> janText(newSrc);

  // Use a local buffer that is big enough for the largest legal path
  char *absPath = new char[PATH_MAX];
  ArrayJanitor<char> janText2(absPath);
  //get the absolute path
  char* retPath = realpath(newSrc, absPath);

  if (!retPath)
  {
    ThrowXML(XMLPlatformUtilsException,
	     XMLExcepts::File_CouldNotGetBasePathName);
  }
  return XMLString::transcode(absPath);
}

bool XMLPlatformUtils::isRelative(const XMLCh* const toCheck)
{
  // Check for pathological case of empty path
  if (!toCheck[0])
    return false;

  //
  //  If it starts with a slash, then it cannot be relative. This covers
  //  both something like "\Test\File.xml" and an NT Lan type remote path
  //  that starts with a node like "\\MyNode\Test\File.xml".
  //
  if (toCheck[0] == XMLCh('/'))
    return false;

  // Else assume its a relative path
  return true;
}

XMLCh* XMLPlatformUtils::weavePaths (
  const XMLCh* const basePath,
  const XMLCh* const relativePath)
{
// Create a buffer as large as both parts and empty it
  XMLCh* tmpBuf = new XMLCh[XMLString::stringLen(basePath)
			   + XMLString::stringLen(relativePath)
			   + 2];
  *tmpBuf = 0;

  //
  //  If we have no base path, then just take the relative path as
  //  is.
  //
  if (!basePath)
  {
    XMLString::copyString(tmpBuf, relativePath);
    return tmpBuf;
  }

  if (!*basePath)
  {
    XMLString::copyString(tmpBuf, relativePath);
    return tmpBuf;
  }

  const XMLCh* basePtr = basePath + (XMLString::stringLen(basePath) - 1);
  if ((*basePtr != chForwardSlash)
      &&  (*basePtr != chBackSlash))
  {
    while ((basePtr >= basePath)
	   &&     ((*basePtr != chForwardSlash) && (*basePtr != chBackSlash)))
    {
      basePtr--;
    }
  }

  // There is no relevant base path, so just take the relative part
  if (basePtr < basePath)
  {
    XMLString::copyString(tmpBuf, relativePath);
    return tmpBuf;
  }

  // After this, make sure the buffer gets handled if we exit early
  ArrayJanitor<XMLCh> janBuf(tmpBuf);

  //
  //  We have some path part, so we need to check to see if we ahve to
  //  weave any of the parts together.
  //
  const XMLCh* pathPtr = relativePath;
  while (true)
  {
    // If it does not start with some period, then we are done
    if (*pathPtr != chPeriod)
      break;

    unsigned int periodCount = 1;
    pathPtr++;
    if (*pathPtr == chPeriod)
    {
      pathPtr++;
      periodCount++;
    }

    // Has to be followed by a \ or / or the null to mean anything
    if ((*pathPtr != chForwardSlash) && (*pathPtr != chBackSlash)
        &&  *pathPtr)
    {
      break;
    }
    if (*pathPtr)
      pathPtr++;

    // If its one period, just eat it, else move backwards in the base
    if (periodCount == 2)
    {
      basePtr--;
      while ((basePtr >= basePath)
	     &&     ((*basePtr != chForwardSlash) && (*basePtr != chBackSlash)))
      {
	basePtr--;
      }

      if (basePtr < basePath)
      {
	// The base cannot provide enough levels, so its in error
	// <TBD>
      }
    }
  }

  // Copy the base part up to the base pointer
  XMLCh* bufPtr = tmpBuf;
  const XMLCh* tmpPtr = basePath;
  while (tmpPtr <= basePtr)
    *bufPtr++ = *tmpPtr++;

  // And then copy on the rest of our path
  XMLString::copyString(bufPtr, pathPtr);

  // Orphan the buffer and return it
  janBuf.orphan();
  return tmpBuf;
}



// -----------------------------------------------------------------------
//  Mutex methods 
// -----------------------------------------------------------------------

#if !defined (APP_NO_THREADS)

class  RecursiveMutex
{
public:
  pthread_mutex_t   mutex;
  int               recursionCount;
  pthread_t         tid;

  RecursiveMutex()
  { 
    if (pthread_mutex_init(&mutex, NULL))
      ThrowXML(XMLPlatformUtilsException, XMLExcepts::Mutex_CouldNotCreate);
    recursionCount = 0;
    tid = 0;
  };

  ~RecursiveMutex()
  {
    if (pthread_mutex_destroy(&mutex))
      ThrowXML(XMLPlatformUtilsException, XMLExcepts::Mutex_CouldNotDestroy);
  };

  void lock()
  {
    if (pthread_equal(tid, pthread_self()))
    {
      recursionCount++;
      return;
    }
    if (pthread_mutex_lock(&mutex) != 0)
      ThrowXML(XMLPlatformUtilsException, XMLExcepts::Mutex_CouldNotLock);
    tid = pthread_self();
    recursionCount = 1;
  };


  void unlock()
  {
    if (--recursionCount > 0)
      return;

    if (pthread_mutex_unlock(&mutex) != 0)
      ThrowXML(XMLPlatformUtilsException, XMLExcepts::Mutex_CouldNotUnlock);
    tid = 0;
  };
};

void* XMLPlatformUtils::makeMutex()
{
  return new RecursiveMutex;
};


void XMLPlatformUtils::closeMutex(void* const mtxHandle)
{
  if (mtxHandle == NULL)
    return;
  RecursiveMutex *rm = (RecursiveMutex *)mtxHandle;
  delete rm;
};


void XMLPlatformUtils::lockMutex(void* const mtxHandle)
{
  if (mtxHandle == NULL)
    return;
  RecursiveMutex *rm = (RecursiveMutex *)mtxHandle;
  rm->lock();
}

void XMLPlatformUtils::unlockMutex(void* const mtxHandle)
{
  if (mtxHandle == NULL)
    return;
  RecursiveMutex *rm = (RecursiveMutex *)mtxHandle;
  rm->unlock();
}

// -----------------------------------------------------------------------
//  Miscellaneous synchronization methods
// -----------------------------------------------------------------------
//atomic system calls in Solaris is only restricted to kernel libraries 
//So, to make operations thread safe we implement static mutex and lock 
//the atomic operations. It makes the process slow but what's the alternative!

void* XMLPlatformUtils::compareAndSwap (void** toFill, 
					const void* const newValue, 
					const void* const toCompare)
{
  //return ((void*)cas32( (uint32_t*)toFill,  (uint32_t)toCompare, (uint32_t)newValue) );
  // the below calls are temporarily made till the above functions are part of user library
  // Currently its supported only in the kernel mode

  if (pthread_mutex_lock( gAtomicOpMutex))
    panic(XMLPlatformUtils::Panic_SynchronizationErr);

  void *retVal = *toFill;
  if (*toFill == toCompare)
    *toFill = (void *)newValue;

  if (pthread_mutex_unlock( gAtomicOpMutex))
    panic(XMLPlatformUtils::Panic_SynchronizationErr);

  return retVal;
}

int XMLPlatformUtils::atomicIncrement(int &location)
{
  //return (int)atomic_add_32_nv( (uint32_t*)&location, 1);

  if (pthread_mutex_lock( gAtomicOpMutex))
    panic(XMLPlatformUtils::Panic_SynchronizationErr);

  int tmp = ++location;

  if (pthread_mutex_unlock( gAtomicOpMutex))
    panic(XMLPlatformUtils::Panic_SynchronizationErr);

  return tmp;
}
int XMLPlatformUtils::atomicDecrement(int &location)
{
  //return (int)atomic_add_32_nv( (uint32_t*)&location, -1);

  if (pthread_mutex_lock( gAtomicOpMutex))
    panic(XMLPlatformUtils::Panic_SynchronizationErr);
	
  int tmp = --location;

  if (pthread_mutex_unlock( gAtomicOpMutex))
    panic(XMLPlatformUtils::Panic_SynchronizationErr);

  return tmp;
}

#else // #if !defined (APP_NO_THREADS)

void XMLPlatformUtils::closeMutex(void* const mtxHandle)
{
}

void XMLPlatformUtils::lockMutex(void* const mtxHandle)
{
}

void* XMLPlatformUtils::makeMutex()
{
  return 0;
}

void XMLPlatformUtils::unlockMutex(void* const mtxHandle)
{
}

void* XMLPlatformUtils::compareAndSwap ( void**      toFill,
                                   const void* const newValue,
                                   const void* const toCompare)
{
  void *retVal = *toFill;
  if (*toFill == toCompare)
    *toFill = (void *)newValue;
  return retVal;
}

int XMLPlatformUtils::atomicIncrement(int &location)
{
  return ++location;
}

int XMLPlatformUtils::atomicDecrement(int &location)
{
  return --location;
}

#endif // APP_NO_THREADS



FileHandle XMLPlatformUtils::openStdInHandle()
{
  return (FileHandle)fdopen(dup(0), "rb");
}

void XMLPlatformUtils::platformTerm()
{
    // We don't have any termination requirements at this time
}
