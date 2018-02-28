#ifndef _MEMORYBANK_H
#define _MEMORYBANK_H

#include <string>
#include <cstddef>
#include <memory>


// a class to handle a big block of contiguous memory.
// constructor/destructor to be overloaded for different types of support.
// destructor should release the associated memory

class MemoryBank {
  public:
    MemoryBank(std::string description=""); // constructor
    virtual ~MemoryBank(); // destructor 
  
    void *getBaseAddress(); // get the (virtual) base address of this memory bank
    std::size_t getSize(); // get the total size (bytes) of this memory bank
    std::string getDescription(); // get the description of this memory bank;
    
    void clear(); // write zeroes into the whole memory range
        
  protected:
    void* baseAddress; // base address (virtual) of buffer
    std::size_t size; // size of buffer, in bytes
    std::string description; // description of the memory bank (type/sypport, etc)
};


// factory function to create a MemoryBank instance of a given type
// size: size of the bank, in bytes
// support: type of support to be used. Available choices: malloc,
// description: optional description for the memory bank

std::shared_ptr<MemoryBank> getMemoryBank(size_t size, std::string support, std::string description="");

#endif // #ifndef _MEMORYBANKMANAGER_H