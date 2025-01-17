/*
 * Copyright (c) 2011-2023, The DART development contributors
 * All rights reserved.
 *
 * The list of contributors can be found at:
 *   https://github.com/dartsim/dart/blob/master/LICENSE
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "dart/common/allocator/AlignedAllocatorFreeList.hpp"

#include "dart/common/Console.hpp"
#include "dart/common/Logging.hpp"
#include "dart/common/Macros.hpp"
#include "dart/common/Memory.hpp"

namespace dart::common {

//==============================================================================
AlignedAllocatorFreeList::AlignedAllocatorFreeList(
    Allocator& baseAllocator, size_t initialAllocation)
  : mBaseAllocator(baseAllocator)
{
  allocateMemoryBlock(sizeof(MemoryBlockHeader) + initialAllocation);
}

//==============================================================================
AlignedAllocatorFreeList::~AlignedAllocatorFreeList()
{
  // Lock the mutex
  std::lock_guard<std::mutex> lock(mMutex);

  // Forcefully deallocate all the memory blocks if destructing this allocator
  // without deallocating individual memories allocated by this allocator.
  if (mTotalAllocatedSize != 0) {
    MemoryBlockHeader* currBlock = mFirstMemoryBlock;
    while (currBlock) {
      MemoryBlockHeader* currSubBlock = currBlock;
      MemoryBlockHeader* next = currBlock->mNext;
      size_t sizeToDeallocate = 0;

      while (currSubBlock) {
        sizeToDeallocate += currSubBlock->mSize;
        if (!currSubBlock->mIsNextContiguous) {
          next = currSubBlock->mNext;
          break;
        }
        currSubBlock = currSubBlock->mNext;
      }

      mBaseAllocator.deallocate(currBlock, sizeToDeallocate);
      currBlock = next;
    }

    dterr
        << "Forcefully deallocated memory " << mTotalAllocatedSize
        << " of byte(s) that is not deallocated before destructing this memory "
        << "allocator.\n";
    // TODO(JS): Change to DART_FATAL once the issue of calling spdlog in
    // destructor is resolved.

    return;
  }

  // Deallocate memory blocks
  MemoryBlockHeader* curr = mFirstMemoryBlock;
  while (curr) {
    DART_ASSERT(!curr->mIsAllocated); // TODO(JS): This means some of pointers
    // are not deallocated
    MemoryBlockHeader* next = curr->mNext;
    const auto size = curr->mSize;

    curr->~MemoryBlockHeader();
    mBaseAllocator.deallocate(curr, size);

    curr = next;
  }
}

//==============================================================================
const Allocator& AlignedAllocatorFreeList::getBaseAllocator() const
{
  return mBaseAllocator;
}

//==============================================================================
Allocator& AlignedAllocatorFreeList::getBaseAllocator()
{
  return mBaseAllocator;
}

//==============================================================================
void* AlignedAllocatorFreeList::allocate(
    size_t bytes, size_t alignment) noexcept
{
  // Not allowed to allocate zero bytes
  if (bytes == 0 || !ValidateAlignment(bytes, alignment)) {
    return nullptr;
  }

  // Lock the mutex
  std::lock_guard<std::mutex> lock(mMutex);
  // TODO(JS): Instead of using a lock_guard to lock the mutex on every
  // allocation, use a lock-free data structure such as a
  // concurrent_unordered_map or a lock-free linked-list to store the free
  // blocks.

  // Ensure that the first memory block doesn't have the previous block
  DART_ASSERT(mFirstMemoryBlock->mPrev == nullptr);

  // Iterate from the first memory block
  MemoryBlockHeader* curr = mFreeBlock ? mFreeBlock : mFirstMemoryBlock;
  MemoryBlockHeader* prev = nullptr;

  // Search for a memory block that is not used and has sufficient free space
  size_t total_size;
  size_t padding;
  DART_ASSERT(curr);
  while (curr) {
    padding = GetPaddingIncludingHeader<MemoryBlockHeader>(
        curr->asSizeT(), alignment);
    total_size = bytes + padding;
    DART_ASSERT((curr->asSizeT() + padding) % alignment == 0);
    if (!curr->mIsAllocated && total_size <= curr->mSize) {
      break;
    }
    prev = curr;
    curr = curr->mNext;
  }
  // TODO(JS): Instead of using a while loop to iterate through the free blocks,
  // use a more efficient search algorithm such as a buddy allocator which can
  // quickly find a block of the correct size.

  // If failed to find an available memory block, allocate a new memory block
  if (curr == nullptr) {
    // Allocate a sufficient size
    if (!allocateMemoryBlock((mTotalAllocatedBlockSize + bytes) * 2)) {
      return nullptr;
    }
    // TODO(JS): Consider using a more sophisticated method for determining the
    // size of the new block, such as a power of 2 or a geometric series.

    DART_ASSERT(mFreeBlock != nullptr);
    DART_ASSERT(!mFreeBlock->mIsAllocated);

    curr = mFreeBlock;
    padding = GetPaddingIncludingHeader<MemoryBlockHeader>(
        curr->asSizeT(), alignment);
    total_size = bytes + padding;
    DART_ASSERT((curr->asSizeT() + padding) % alignment == 0);
    DART_ASSERT(curr->mSize >= total_size);
  }

  // Shift location of the header if the padding is greater than the size of the
  // header, so that the location of the header is the data pointer minus the
  // size of the header.
  if (padding > sizeof(MemoryBlockHeader)) {
    const MemoryBlockHeader copy = *curr;
    curr = reinterpret_cast<MemoryBlockHeader*>(
        curr->asSizeT() + padding - sizeof(MemoryBlockHeader));
    *curr = copy;
    curr->padding = padding;
  }

  // Split the new memory block for the requested size
  curr->split(total_size, padding);
  DART_ASSERT(curr->mSize == total_size);

  // Mark the current block is allocated
  curr->mIsAllocated = true;

  // Set free block if the next block is free
  if (prev != nullptr && !prev->mIsAllocated) {
    mFreeBlock = prev;
  }

  mTotalAllocatedSize += bytes;

  return static_cast<void*>(curr->asCharPtr() + sizeof(MemoryBlockHeader));
}

//==============================================================================
void AlignedAllocatorFreeList::deallocate(void* pointer, size_t bytes)
{
  // Cannot deallocate nullptr or zero bytes
  if (pointer == nullptr || bytes == 0) {
    return;
  }

  // Lock the mutex
  std::lock_guard<std::mutex> lock(mMutex);

  unsigned char* block_addr
      = static_cast<unsigned char*>(pointer) - sizeof(MemoryBlockHeader);
  MemoryBlockHeader* block = reinterpret_cast<MemoryBlockHeader*>(block_addr);
  DART_ASSERT(block->mIsAllocated);
  block->mIsAllocated = false;

  MemoryBlockHeader* const prev = block->mPrev;
  MemoryBlockHeader* const next = block->mNext;

  if (prev != nullptr && !prev->mIsAllocated && prev->mIsNextContiguous) {
    DART_ASSERT(prev->padding == 0);
    prev->merge(block);
    block = prev;
  }

  if (next != nullptr && !next->mIsAllocated && block->mIsNextContiguous) {
    DART_ASSERT(next->padding == 0);
    block->merge(next);
  }

  if (block->padding != 0) {
    const MemoryBlockHeader copy = *block;
    block = reinterpret_cast<MemoryBlockHeader*>(
        block->asSizeT() + sizeof(MemoryBlockHeader) - copy.padding);
    *block = copy;
    block->padding = 0;
  }

  mFreeBlock = block;

  DART_ASSERT(mTotalAllocatedSize >= bytes);
  mTotalAllocatedSize -= bytes;

  DART_TRACE("Deallocated {} bytes.", bytes);
}

//==============================================================================
bool AlignedAllocatorFreeList::allocateMemoryBlock(size_t sizeToAllocate)
{
  // Allocate memory chunk for header and the actual requested size
  void* memory
      = mBaseAllocator.allocate(sizeToAllocate + sizeof(MemoryBlockHeader));

  // Return false if failed to allocate
  if (memory == nullptr)
    return false;

  // Construct the memory block header, linking the current block as the next
  // block
  mFirstMemoryBlock = mBaseAllocator.constructAt<MemoryBlockHeader>(
      memory,            // address to construct
      sizeToAllocate,    // size of the memory block
      0,                 // padding, initially zero
      nullptr,           // previous memory block
      mFirstMemoryBlock, // next memory block
      false              // whether the next memory block is contiguous
  );

  // Set the new memory block as free block
  mFreeBlock = mFirstMemoryBlock;
  // If mFreeBlock is not null, then it means the free block is not sufficient
  // for the new allocation request. We would consider to keep track the
  // "dangling" free blocks for the next allocation if the memory footage is
  // more critical than speed.

  // Update the allocated size (without memory size for the headers of blocks)
  mTotalAllocatedBlockSize += sizeToAllocate;

  return true;
}

//==============================================================================
void AlignedAllocatorFreeList::print(std::ostream& os, int indent) const
{
  // Lock the mutex
  std::lock_guard<std::mutex> lock(mMutex);

  if (indent == 0) {
    os << "[AlignedAllocatorFreeList]\n";
  }
  const std::string spaces(indent, ' ');
  if (indent != 0) {
    os << spaces << "type: " << getType() << "\n";
  }
  os << spaces << "reserved_size: " << mTotalAllocatedBlockSize << "\n";
  os << spaces << "memory_blocks:\n";
  auto curr = mFirstMemoryBlock;
  while (curr) {
    os << spaces << "- block_addr: " << curr << "\n";
    os << spaces << "  size: " << curr->mSize << "\n";
    os << spaces << "  prev: " << curr->mPrev << "\n";
    os << spaces << "  next: " << curr->mNext << "\n";
    os << spaces << "  is_allocated: " << curr->mIsAllocated << "\n";
    os << spaces << "  is_next_contiguous: " << curr->mIsNextContiguous << "\n";
    curr = curr->mNext;
  }
  os << spaces << "free_block_addr: " << mFreeBlock << "\n";
  os << spaces << "header_size: " << sizeof(MemoryBlockHeader) << "\n";
  os << spaces << "base_allocator:\n";
  mBaseAllocator.print(os, indent + 2);
}

//==============================================================================
AlignedAllocatorFreeList::MemoryBlockHeader::MemoryBlockHeader(
    size_t size,
    size_t padding,
    MemoryBlockHeader* prev,
    MemoryBlockHeader* next,
    bool isNextContiguous)
  : mSize(size),
    padding(padding),
    mPrev(prev),
    mNext(next),
    mIsAllocated(false),
    mIsNextContiguous(isNextContiguous)
{
  if (prev) {
    prev->mNext = this;
  }
}

//==============================================================================
size_t AlignedAllocatorFreeList::MemoryBlockHeader::asSizeT() const
{
  return reinterpret_cast<size_t>(this);
}

//==============================================================================
unsigned char* AlignedAllocatorFreeList::MemoryBlockHeader::asCharPtr()
{
  return reinterpret_cast<unsigned char*>(this);
}

//==============================================================================
const unsigned char* AlignedAllocatorFreeList::MemoryBlockHeader::asCharPtr()
    const
{
  return reinterpret_cast<const unsigned char*>(this);
}

//==============================================================================
void AlignedAllocatorFreeList::MemoryBlockHeader::split(
    size_t sizeToSplit, size_t padding)
{
  DART_ASSERT(sizeToSplit <= mSize);
  DART_ASSERT(!mIsAllocated);

  if (sizeToSplit >= mSize) {
    // TODO(JS): Treat this as en error?
    return;
  }

  DART_ASSERT(mSize > sizeToSplit);

  unsigned char* new_block_addr
      = asCharPtr() + sizeof(MemoryBlockHeader) + sizeToSplit - padding;
  new (new_block_addr)
      MemoryBlockHeader(mSize - sizeToSplit, 0, this, mNext, mIsNextContiguous);
  mNext = reinterpret_cast<MemoryBlockHeader*>(new_block_addr);
  if (mNext->mNext) {
    mNext->mNext->mPrev = mNext;
  }
  DART_ASSERT(mNext != this);
  mIsNextContiguous = true;
  mSize = sizeToSplit;

  DART_ASSERT(isValid());
  DART_ASSERT(mNext->isValid());
}

//==============================================================================
void AlignedAllocatorFreeList::MemoryBlockHeader::merge(
    MemoryBlockHeader* other)
{
  DART_ASSERT(other);
  DART_ASSERT(other->mPrev == this);
  DART_ASSERT(other->mNext != this);
  DART_ASSERT(!other->mIsAllocated);
  DART_ASSERT(this->mNext == other);
  DART_ASSERT(!this->mIsAllocated);
  DART_ASSERT(this->mIsNextContiguous);

  mSize += other->mSize;
  mNext = other->mNext;
  if (other->mNext) {
    other->mNext->mPrev = this;
  }
  mIsNextContiguous = other->mIsNextContiguous;

  other->~MemoryBlockHeader(); // TODO(JS): Need this?

  DART_ASSERT(isValid());
}

//==============================================================================
#ifndef NDEBUG
bool AlignedAllocatorFreeList::MemoryBlockHeader::isValid() const
{
  if (mPrev != nullptr && mPrev->mNext != this) {
    return false;
  }

  if (mNext != nullptr && mNext->mPrev != this) {
    return false;
  }

  return true;
}
#endif

} // namespace dart::common
