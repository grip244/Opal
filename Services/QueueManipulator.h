#pragma once

namespace Opal {
namespace Services {

class QueueManipulator {
public:
    template <typename TQueue, typename TList>
    static void MoveInQueue(TQueue queue, TList list, unsigned int fromIndex, unsigned int toIndex) {
        if (fromIndex >= queue->Size || toIndex >= queue->Size) return;
        if (fromIndex == toIndex) return;

        auto song = queue->GetAt(fromIndex);
        auto item = list->GetAt(fromIndex);

        queue->RemoveAt(fromIndex);
        list->RemoveAt(fromIndex);

        queue->InsertAt(toIndex, song);
        list->InsertAt(toIndex, item);
    }

    template <typename TQueue, typename TList, typename TCallback>
    static void RemoveFromQueue(TQueue queue, TList list, unsigned int index, TCallback onEmpty) {
        if (index >= queue->Size) return;

        queue->RemoveAt(index);
        list->RemoveAt(index);

        if (queue->Size == 0) {
            onEmpty();
        }
    }

    template <typename TQueue, typename TList, typename TSong, typename TItem>
    static void AddToQueue(TQueue queue, TList list, TSong song, TItem item) {
        if (song == nullptr) return;
        queue->Append(song);
        list->Append(item);
    }
};

}
}
