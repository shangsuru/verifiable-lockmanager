#include <gtest/gtest.h>

#include "hashtable.h"
#include "lock.h"

/*
 ********************************
 * GET
 ********************************
 */

TEST(HashTableTest, getWhenListEmpty) {
  HashTable* hashTable = newHashTable(10);
  void* value = get(hashTable, 102);
  EXPECT_EQ(value, nullptr);
};

TEST(HashTableTest, getListNotEmptyKeyExists) {
  HashTable* hashTable = newHashTable(10);
  Lock* lock = newLock();
  lock->exclusive = true;
  lock->num_owners = 4;

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)lock);
  set(hashTable, 42, (void*)newLock());

  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->exclusive, lock->exclusive);
  EXPECT_EQ(value->num_owners, lock->num_owners);
};

TEST(HashTableTest, getElementNotFound) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value, nullptr);
};

/*
 ********************************
 * SET
 ********************************
 */

TEST(HashTableTest, setWhenKeyAlreadyExists) {
  HashTable* hashTable = newHashTable(10);
  Lock* lock = newLock();
  lock->exclusive = true;
  lock->num_owners = 4;

  Lock* anotherLock = new Lock();
  anotherLock->exclusive = false;
  anotherLock->num_owners = 6;

  set(hashTable, 32, (void*)lock);
  // Because lock for that key already exists, it should ignore this set
  // function call!
  set(hashTable, 32, (void*)anotherLock);

  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->exclusive, lock->exclusive);
  EXPECT_EQ(value->num_owners, lock->num_owners);
};

/*
 ********************************
 * CONTAINS
 ********************************
 */

TEST(HashTableTest, containsListEmpty) {
  HashTable* hashTable = newHashTable(10);
  EXPECT_FALSE(contains(hashTable, 32));
};

TEST(HashTableTest, containsTrue) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  EXPECT_TRUE(contains(hashTable, 22));
};

TEST(HashTableTest, containsFalse) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  EXPECT_FALSE(contains(hashTable, 32));
};

/*
 ********************************
 * REMOVE
 ********************************
 */

TEST(HashTableTest, removeEmptyList) {
  HashTable* hashTable = newHashTable(10);
  // Nothing happens
  remove(hashTable, 10);
};

TEST(HashTableTest, removeEndOfList) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));

  remove(hashTable, 42);

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_FALSE(contains(hashTable, 42));
};

TEST(HashTableTest, removeListJustHasOneElement) {
  HashTable* hashTable = newHashTable(10);
  set(hashTable, 12, (void*)newLock());
  EXPECT_TRUE(contains(hashTable, 12));
  remove(hashTable, 12);
  EXPECT_FALSE(contains(hashTable, 12));
}

TEST(HashTableTest, removeBeginningOfList) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));

  remove(hashTable, 12);

  EXPECT_FALSE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));
};

TEST(HashTableTest, removeMiddleOfList) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));

  remove(hashTable, 22);

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_FALSE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));
};

TEST(HashTableTest, removeElementNotFound) {
  HashTable* hashTable = newHashTable(10);

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 42, (void*)newLock());

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_FALSE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));

  remove(hashTable, 32);

  EXPECT_TRUE(contains(hashTable, 12));
  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_FALSE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 42));
};

/*
 ********************************
 * MODIFYING POINTERS
 ********************************
 */

TEST(HashTableTest, changeValue) {
  HashTable* hashTable = newHashTable(10);
  Lock* lock = newLock();  // create a lock
  lock->exclusive = true;
  lock->num_owners = 4;

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)lock);  // insert it into the table
  set(hashTable, 42, (void*)newLock());

  // Change some values on the lock pointer
  lock->num_owners = 6;
  lock->exclusive = false;

  // Check that the values also changed within the table
  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->num_owners, 6);
  EXPECT_EQ(value->exclusive, false);
};

/*
 ********************************
 * BUCKET SIZES
 ********************************
 */

TEST(HashTableTest, computesBucketSizesCorrectly) {
  HashTable* hashTable = newHashTable(4);
  set(hashTable, 4, (void*)newLock());
  set(hashTable, 1, (void*)newLock());
  set(hashTable, 5, (void*)newLock());
  set(hashTable, 9, (void*)newLock());
  set(hashTable, 3, (void*)newLock());

  /**
   * Visualization of the hash table after those five operations:
   * [0] -> 4
   * [1] -> 1, 5, 9
   * [2] ->
   * [3] -> 3
   */

  remove(hashTable, 5);

  // Verify bucket sizes
  // First bucket contains 1 element, a.s.o
  EXPECT_EQ(hashTable->bucketSizes[0], 1);
  EXPECT_EQ(hashTable->bucketSizes[1], 2);
  EXPECT_EQ(hashTable->bucketSizes[2], 0);
  EXPECT_EQ(hashTable->bucketSizes[3], 1);
}