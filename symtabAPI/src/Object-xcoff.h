/*
 * Copyright (c) 1996-2007 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/************************************************************************
 * AIX object files.
 * $Id: Object-xcoff.h,v 1.5 2007/05/30 19:20:52 legendre Exp $
************************************************************************/





#if !defined(_Object_aix_h_)
#define _Object_aix_h_





/************************************************************************
 * header files.
************************************************************************/

#include "common/h/headers.h"
//#include <common/h/Line.h>
#include "symtabAPI/h/Dyn_Symbol.h"
#include "common/h/Types.h"

#include <ext/hash_map>
#include <string>
#include <vector>

using namespace std;
using namespace __gnu_cxx;

extern "C" {
#include <a.out.h>
};

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define __AR_BIG__
#define __AR_SMALL__
#include <ar.h>
class fileOpener;

// Object to represent both the 32-bit and 64-bit archive headers
// for ar files (libraries)
class Archive {
 public:
  Archive (fileOpener *f) : member_name(0), fo_(f) {}
  virtual ~Archive () {}
  
  virtual int read_arhdr() = 0;
  virtual int read_mbrhdr() = 0;

  unsigned long long aout_offset;
  unsigned long long next_offset;
  char *member_name;
  int member_len;
 protected:
  unsigned long long first_offset;
  unsigned long long last_offset;

  fileOpener *fo_;
};

class Archive_32 : private Archive {
 public:
  Archive_32 (fileOpener *file) : Archive(file) {};
  ~Archive_32 () {if (member_name) free(member_name);};
  virtual int read_arhdr();
  virtual int read_mbrhdr();

 private:
  struct fl_hdr filehdr;
  struct ar_hdr memberhdr;
};

class Archive_64 : private Archive {
 public:
  Archive_64 (fileOpener *file) : Archive(file) {};
  ~Archive_64 () {if (member_name) free(member_name);};
  virtual int read_arhdr();
  virtual int read_mbrhdr();

 private:
  struct fl_hdr_big filehdr;
  struct ar_hdr_big memberhdr;
};

// We want to mmap a file once, then go poking around inside it. So we
// need to tie a few things together.

class fileOpener {
 public:
    static vector<fileOpener *> openedFiles;
    static fileOpener *openFile(const string &file);
    static fileOpener *openFile(void *ptr, unsigned size);
    
    void closeFile();

    fileOpener(const string &file) : refcount_(1), 
        file_(file), fd_(0), 
        size_(0), mmapStart_(NULL),
        offset_(0) {}

    fileOpener(void *ptr, unsigned size) : refcount_(1), 
        file_(""), fd_(0), 
        size_(size), mmapStart_(ptr),
        offset_(0) {}
	
    ~fileOpener();

    bool open();
    bool mmap();
    bool unmap();
    bool close();

    bool pread(void *buf, unsigned size, unsigned offset);
    bool read(void *buf, unsigned size);
    bool seek(int offset);
    bool set(unsigned addr);
    // No write :)
    // Get me a pointer into the mapped area
    void *ptr() const;
    void *getPtrAtOffset(unsigned offset) const;
    
    const string &file() const { return file_; }
    int fd() const { return fd_; }
    unsigned size() const { return size_; }
    void *mem_image() const { return mmapStart_; }

 private:
    int refcount_;
    string file_;
    int fd_;
    unsigned size_;
    void *mmapStart_;

    // Where were we last reading?
    unsigned offset_;
};

/************************************************************************
 * class Object
************************************************************************/

class Object : public AObject {
 public:
    // We _really_ never want this to be called...
    Object (const Object &);
    Object&   operator= (const Object &);

public:
    Object(){}	
    Object(string &filename,	
            void (*)(const char *) = log_msg);
    Object(char *mem_image, size_t image_size,
            void (*)(const char *) = log_msg);
    Object(string &filename, string &member_name, OFFSET offset,	
            void (*)(const char *) = log_msg);
    Object(char *mem_image, size_t image_size,string &member_name, OFFSET offset,
            void (*)(const char *) = log_msg);
    ~Object ()
    {
        if(fo_)
    	    fo_->closeFile();
    }
    
    OFFSET getTOCoffset() const { return toc_offset_; }

    // AIX does some weirdness with addresses. We don't want to touch local
    // symbols to get addresses right, but that means that the base address
    // needs to have a small value added back in. On shared objects.
    OFFSET data_reloc () const { return data_reloc_; }
    OFFSET text_reloc () const { return text_reloc_; }
    
    void get_stab_info(char *&stabstr, int &nstabs, void *&stabs, char *&stringpool) const;
    
    void get_line_info(int& nlines, char*& lines,unsigned long& fdptr) const {
	nlines = nlines_;
	lines = (char*) linesptr_; 
	fdptr = linesfdptr_;
    }

    OFFSET getLoadAddress() const { return loadAddress_; }
    OFFSET getEntryAddress() const { return entryAddress_; }
    OFFSET getBaseAddress() const { return baseAddress_; }
    void getModuleLanguageInfo(hash_map<string, supportedLanguages> *mod_langs);
    const char *interpreter_name() const { return NULL; }
    
    ObjectType objType() const;

    void load_object();
    void load_archive(bool is_aout);
    void parse_aout(int offset, bool is_aout);
    bool isEEL() const { return false; }
    

    // We mmap big files and piece them out; this handles the mapping
    // and reading and pointerage and...
    fileOpener *fo_;

    string member_;
    OFFSET offset_;
    OFFSET toc_offset_;
    OFFSET data_reloc_;
    OFFSET text_reloc_;

    OFFSET   loadAddress_;      // The object may specify a load address
                                    //   Set to 0 if it may load anywhere
    OFFSET entryAddress_;
    OFFSET baseAddress_;
    

    int  nstabs_;
    int  nlines_;
    void *stabstr_;
    void *stabs_;
    void *stringpool_;
    void *linesptr_;
    OFFSET linesfdptr_;
};
char * P_cplus_demangle( const char * symbol, bool nativeCompiler, bool includeTypes );



#endif /* !defined(_Object_aix_h_) */