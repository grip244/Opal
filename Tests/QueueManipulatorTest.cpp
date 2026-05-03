#include <iostream>
#include <vector>
#include <cassert>
#include "Services/QueueManipulator.h"

using namespace Opal::Services;

// Mock items
struct MockSong {
    int id;
    bool operator==(const MockSong& other) const { return id == other.id; }
};
struct MockItem {
    int id;
    bool operator==(const MockItem& other) const { return id == other.id; }
};

// Mock queue class mimicking WinRT collection interface needed for the generic logic
template <typename T>
struct MockQueue {
    std::vector<T*> items;
    unsigned int Size = 0;

    void Append(T* item) {
        items.push_back(item);
        Size = items.size();
    }

    void RemoveAt(unsigned int index) {
        items.erase(items.begin() + index);
        Size = items.size();
    }

    void InsertAt(unsigned int index, T* item) {
        items.insert(items.begin() + index, item);
        Size = items.size();
    }

    T* GetAt(unsigned int index) {
        return items[index];
    }
};

void TestAddToQueue() {
    MockQueue<MockSong> queue;
    MockQueue<MockItem> list;

    MockSong s1{1};
    MockItem i1{1};

    QueueManipulator::AddToQueue(&queue, &list, &s1, &i1);

    assert(queue.Size == 1);
    assert(list.Size == 1);
    assert(queue.GetAt(0)->id == 1);
    assert(list.GetAt(0)->id == 1);

    // Test adding null song (expect no change)
    QueueManipulator::AddToQueue(&queue, &list, (MockSong*)nullptr, &i1);
    assert(queue.Size == 1);
    assert(list.Size == 1);
}

void TestRemoveFromQueue() {
    MockQueue<MockSong> queue;
    MockQueue<MockItem> list;

    MockSong s1{1}, s2{2};
    MockItem i1{1}, i2{2};

    queue.Append(&s1); queue.Append(&s2);
    list.Append(&i1); list.Append(&i2);

    bool isEmpty = false;
    // Remove index 0
    QueueManipulator::RemoveFromQueue(&queue, &list, 0, [&isEmpty]() { isEmpty = true; });

    assert(queue.Size == 1);
    assert(list.Size == 1);
    assert(queue.GetAt(0)->id == 2);
    assert(list.GetAt(0)->id == 2);
    assert(!isEmpty);

    // Remove remaining item
    QueueManipulator::RemoveFromQueue(&queue, &list, 0, [&isEmpty]() { isEmpty = true; });
    assert(queue.Size == 0);
    assert(list.Size == 0);
    assert(isEmpty); // Callback should have fired

    // Out of bounds test
    QueueManipulator::RemoveFromQueue(&queue, &list, 0, [](){});
    assert(queue.Size == 0); // Unchanged
}

void TestMoveInQueue() {
    MockQueue<MockSong> queue;
    MockQueue<MockItem> list;

    MockSong s1{1}, s2{2}, s3{3};
    MockItem i1{1}, i2{2}, i3{3};

    queue.Append(&s1); queue.Append(&s2); queue.Append(&s3);
    list.Append(&i1); list.Append(&i2); list.Append(&i3);

    // Move from index 0 to 2
    QueueManipulator::MoveInQueue(&queue, &list, 0, 2);

    assert(queue.GetAt(0)->id == 2);
    assert(queue.GetAt(1)->id == 3);
    assert(queue.GetAt(2)->id == 1);

    assert(list.GetAt(0)->id == 2);
    assert(list.GetAt(1)->id == 3);
    assert(list.GetAt(2)->id == 1);

    // Out of bounds
    QueueManipulator::MoveInQueue(&queue, &list, 3, 0); // No op
    assert(queue.GetAt(0)->id == 2); // Unchanged

    // Same index
    QueueManipulator::MoveInQueue(&queue, &list, 1, 1); // No op
    assert(queue.GetAt(1)->id == 3); // Unchanged
}

int main() {
    std::cout << "Running QueueManipulator tests...\n";
    TestAddToQueue();
    std::cout << "TestAddToQueue passed.\n";
    TestRemoveFromQueue();
    std::cout << "TestRemoveFromQueue passed.\n";
    TestMoveInQueue();
    std::cout << "TestMoveInQueue passed.\n";
    std::cout << "All QueueManipulator tests passed successfully!\n";
    return 0;
}
