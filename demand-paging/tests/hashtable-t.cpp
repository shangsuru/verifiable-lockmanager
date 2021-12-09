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
  lock->owners = new int[1];
  lock->owners_size = 1;
  const int val = 4;
  lock->owners[0] = val;

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)lock);
  set(hashTable, 42, (void*)newLock());

  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->exclusive, lock->exclusive);
  EXPECT_EQ(value->owners_size, lock->owners_size);

  bool wasFoundInValue = false;
  for (int i = 0; i < value->owners_size; i++) {
    if (value->owners[i] == val) {
      wasFoundInValue = true;
    }
  }
  EXPECT_TRUE(wasFoundInValue);

  bool wasFoundInLock = false;
  for (int i = 0; i < lock->owners_size; i++) {
    if (lock->owners[i] == val) {
      wasFoundInLock = true;
    }
  }
  EXPECT_TRUE(wasFoundInLock);
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
  lock->owners = new int[1];
  lock->owners_size = 1;
  const int val = 4;
  lock->owners[0] = val;

  Lock* anotherLock = new Lock();
  anotherLock->exclusive = false;

  set(hashTable, 32, (void*)lock);
  // Because lock for that key already exists, it should ignore this set
  // function call!
  set(hashTable, 32, (void*)anotherLock);

  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->exclusive, lock->exclusive);
  EXPECT_EQ(value->owners_size, lock->owners_size);
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

TEST(HashTableTest, changeValue) {
  HashTable* hashTable = newHashTable(10);
  Lock* lock = newLock();
  lock->exclusive = true;
  lock->owners = new int[1];
  const int val = 1;
  lock->owners[0] = val;

  set(hashTable, 12, (void*)newLock());
  set(hashTable, 22, (void*)newLock());
  set(hashTable, 32, (void*)lock);
  set(hashTable, 42, (void*)newLock());

  // Change value
  lock->owners = new int[2];
  lock->owners_size = 2;
  lock->owners[0] = 1;
  lock->owners[1] = 2;
  lock->exclusive = false;

  // Check that the values also changed within the table
  Lock* value = (Lock*)get(hashTable, 32);
  EXPECT_EQ(value->owners_size, 2);
  EXPECT_EQ(value->exclusive, false);
};

TEST(HashTableTest, keyBiggerThanSize) {
  HashTable* hashTable = newHashTable(2);
  set(hashTable, 1, (void*)newLock());
  set(hashTable, 2, (void*)newLock());
  set(hashTable, 3, (void*)newLock());

  EXPECT_TRUE(get(hashTable, 1) != nullptr);
  EXPECT_TRUE(get(hashTable, 2) != nullptr);
  EXPECT_TRUE(get(hashTable, 3) != nullptr);
}