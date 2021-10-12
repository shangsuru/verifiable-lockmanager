#include <gtest/gtest.h>

#include "hashtable.h"
#include "lock.h"

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

  EXPECT_TRUE(contains(hashTable, 22));
  EXPECT_TRUE(contains(hashTable, 32));
  EXPECT_TRUE(contains(hashTable, 32));
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

TEST(HashTableTest, changeValue) {
  HashTable* hashTable = newHashTable(10);
  Lock* lock = newLock();
  lock->exclusive = true;
  lock->num_owners = 4;

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)lock);
  set(hashTable, 42, (void*)newLock());

  // Change value
  lock->num_owners = 6;
  lock->exclusive = false;

  // Check that the values also changed within the table
  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->num_owners, 6);
  EXPECT_EQ(value->exclusive, false);
};