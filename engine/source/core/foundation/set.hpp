#pragma once

#include "misc/type_hash.hpp"
#include "foundation/sparse_array.hpp"

namespace Engine
{
#pragma region iterator
    template <typename SparseIteratorType, typename KeyType>
    class ConstSetIterator
    {
    public:
        explicit ConstSetIterator(const SparseIteratorType& iter)
            : InnerIterator(iter)
        {}

        const KeyType& operator*() const { return (*InnerIterator).Element; }

        const KeyType* operator->() const { return InnerIterator->Element; }

        ConstSetIterator& operator++ ()
        {
            ++InnerIterator;
            return *this;
        }

        friend bool operator== (const ConstSetIterator& lhs, const ConstSetIterator& rhs)
        {
            return lhs.InnerIterator == rhs.InnerIterator;
        }

        friend bool operator!= (const ConstSetIterator& lhs, const ConstSetIterator& rhs)
        {
            return !(lhs == rhs);
        }

    protected:
        SparseIteratorType InnerIterator;
    };

    template <typename SparseIteratorType, typename KeyType>
    class SetIterator : public ConstSetIterator<SparseIteratorType, KeyType>
    {
        using Super = ConstSetIterator<SparseIteratorType, KeyType>;
    public:
        explicit SetIterator(const SparseIteratorType& iter)
            : Super(iter)
        {}

        KeyType& operator*() const { return const_cast<KeyType&>(Super::operator *()); }

        KeyType* operator->() const { return const_cast<KeyType*>(Super::operator ->()); }

        SetIterator& operator++ ()
        {
            Super::operator++();
            return *this;
        }

        friend bool operator== (const SetIterator& lhs, const SetIterator& rhs)
        {
            return lhs.InnerIterator == rhs.InnerIterator;
        }

        friend bool operator!= (const SetIterator& lhs, const SetIterator& rhs)
        {
            return !(lhs == rhs);
        }
    };

#pragma endregion iterator

    /**
     * Determine default hash function and equals function of set key
     * @tparam Key
     */
    template <typename Key>
    struct DefaultSetKeyFunc
    {
        static uint32 GetHashCode(const Key& key)
        {
            return Engine::GetHashCode(key);
        }

        static const Key& GetKey(const Key& element)
        {
            return element;
        }

        static bool Equals(const Key& lKey, const Key& rKey)
        {
            return lKey == rKey;
        }
    };

    /** Encapsulates the allocators used by a set in a single type. */
    template <
        typename InSparseArrayAllocator,
        typename InHashAllocator,
        uint32 ElementsPerHashBucket = 2,
        uint32 MinHashBuckets = 8,
        uint32 MinCountOfHashedElements = 4>
    struct SetAllocator
    {
        static uint32 GetNumberOfHashBuckets(uint32 hashedElementsCount)
        {
            if (hashedElementsCount >= MinCountOfHashedElements)
            {
                return Math::RoundUpToPowerOfTwo(hashedElementsCount / ElementsPerHashBucket + MinHashBuckets);
            }

            return 1;
        }

        typedef InSparseArrayAllocator SparseArrayAllocator;
        typedef InHashAllocator        HashAllocator;
    };

    /** index of set in sparse array */
    struct SetElementIndex
    {
        SetElementIndex() = default;

        SetElementIndex(int32 index) : Index(index)
        {}

        bool IsValid() const { return Index != INDEX_NONE; }

        operator int32() const
        {
            return Index;
        }

        // Index in sparse array
        int32 Index{ INDEX_NONE };
    };

    using DefaultSetAllocator = SetAllocator<DefaultAllocator, DefaultAllocator>;

    template <typename ElementType, typename KeyFunc = DefaultSetKeyFunc<ElementType>, typename SetAllocator = DefaultSetAllocator>
    class Set
    {
        struct SetElement
        {
            explicit SetElement(const ElementType& element)
            {
                Element = element;
            }

            explicit SetElement(ElementType&& element)
            {
                Element = element;
            }

            uint32 HashIndex = 0;
            SetElementIndex HashNextId;
            ElementType Element;
        };

        using HashBucketType = typename SetAllocator::HashAllocator::template ElementAllocator<SetElementIndex>;
        using SparseArrayType = SparseArray<SetElement, typename SetAllocator::SparseArrayAllocator>;

        friend class ConstSetIterator<Set, ElementType>;
        friend class SetIterator<Set, ElementType>;
        template <typename K, typename V, typename T, typename U> friend class Map;

    public:
        using Iterator = SetIterator<typename SparseArrayType::ConstIterator, ElementType>;
        using ConstIterator = ConstSetIterator<typename SparseArrayType::ConstIterator, ElementType>;

    public:
        Set() = default;

        Set(std::initializer_list<ElementType> initializer)
        {
            Append(initializer);
        }

        template <typename IteratorType>
        Set(IteratorType begin, IteratorType end)
        {
            do
            {
                Add(*begin);
                ++begin;
            }
            while (begin != end);
        }

        Set(const Set& other)
        {
            CopyElement(other);
        }

        Set(Set&& other) noexcept
        {
            MoveElement(Forward<Set&&>(other));
        }

        Set& operator= (const Set& other)
        {
            ENSURE(*this != other);
            // SetElementIndex don't need destruct
            CopyElement(other);
            return *this;
        }

        Set& operator= (Set&& other) noexcept
        {
            ENSURE(*this != other);
            MoveElement(Forward<Set&&>(other));
            return *this;
        }

        /**
         * Adds the specified element to Set.
         * 
         * @param T element
         */
        void Add(const ElementType& element)
        {
            Emplace(element);
        }

        /**
         * Moves the specified element to Set.
         *
         * @param T element
         */
        void Add(ElementType&& element)
        {
            Emplace(element);
        }

        void Append(std::initializer_list<ElementType> initializer)
        {
            Reserve(Elements.Size() + (int32)initializer.size());
            for (auto&& element : initializer)
            {
                Add(element);
            }
        }

        /**
         * Determines whether a Set contains the specified element.
         * 
         * @param T key
         * @return boolean true if contains the specified element, otherwise false.
         */
        template <typename KeyType>
        bool Contains(const KeyType& key) const
        {
            return FindIndex(key).IsValid();
        }

        template <typename KeyType>
        ElementType* Find(const KeyType& key) const
        {
            ElementType* ret = nullptr;
            if (Size() > 0)
            {
                SetElementIndex* elementIndex = &GetFirstIndex(KeyFunc::GetHashCode(key));
                while (elementIndex->IsValid())
                {
                    auto&& setElement = Elements[elementIndex->Index];
                    if (KeyFunc::Equals(KeyFunc::GetKey(setElement.Element), key))
                    {
                        ret = const_cast<ElementType*>(&setElement.Element);
                        break;
                    }
                    else
                    {
                        elementIndex = const_cast<SetElementIndex*>(&setElement.HashNextId);
                    }
                }
            }
            return ret;
        }

        /**
         * Removes the specified element from a Set.
         * 
         * @param T key
         * @return boolean true if remove success.
         */
        template <typename KeyType>
        bool Remove(const KeyType& key)
        {
            bool ret = false;
            if (Size() > 0)
            {
                SetElementIndex* elementIndex = &GetFirstIndex(KeyFunc::GetHashCode(key));
                while (elementIndex->IsValid())
                {
                    auto&& setElement = Elements[elementIndex->Index];
                    if (KeyFunc::Equals(KeyFunc::GetKey(setElement.Element), key))
                    {
                        uint32 pendingRemoveIndex = elementIndex->Index;
                        elementIndex->Index = setElement.HashNextId.Index;
                        Elements.RemoveAt(pendingRemoveIndex);
                        ret = true;
                        break;
                    }
                    else
                    {
                        elementIndex = &setElement.HashNextId;
                    }
                }
            }
            return ret;
        }

        void Clear(int32 slack = 0)
        {
            ENSURE(slack >= 0);
            HashBucket.Resize(slack);
            Elements.Clear(slack);
        }

        int32 Size() const
        {
            return Elements.Size();
        }

        bool IsEmpty() const
        {
            return Size() == 0;
        }

        /**
         * Preallocates enough memory to contain Number elements.
         * 
         * @param count int32
         */
        void Reserve(int32 count)
        {
            if (count > static_cast<int32>(Elements.Size()))
            {
                Elements.Reserve(count);

                const uint32 newBucketCount = SetAllocator::GetNumberOfHashBuckets(static_cast<uint32>(count));
                if (!BucketCount || BucketCount < newBucketCount)
                {
                    BucketCount = newBucketCount;
                    Rehash();
                }
            }
        }

        //TODO: impl
        void Resize(int32 capacity)
        {
            ENSURE(capacity >= 0);
        }

        Iterator begin() { return Iterator(Elements.begin()); }

        ConstIterator begin() const { return ConstIterator(const_cast<const SparseArrayType>(Elements).begin()); }

        Iterator end() { return Iterator(Elements.end()); }

        ConstIterator end() const { return ConstIterator(const_cast<const SparseArrayType>(Elements).end()); }

    private:
        template <typename InElementType>
        InElementType& Emplace(InElementType&& element)
        {
            uint32 hashCode = KeyFunc::GetHashCode(KeyFunc::GetKey(element));

            SetElementIndex index = FindIndex(KeyFunc::GetKey(element), hashCode);
            if (index.IsValid())
            {
                auto&& setElement = Elements[index.Index];
                setElement.Element.~ElementType();
                setElement.Element = element;
                return setElement.Element;
            }


            CheckRehash(Elements.Size() + 1);
            int32 indexInSparseArray = Elements.AddUnconstructElement();
            SetElement* item = new(Elements.GetData() + indexInSparseArray) SetElement(element);
            LinkElement(SetElementIndex{ indexInSparseArray }, *item, hashCode);
            return item->Element;
        }

        /** Contains key index in sparse array */
        template <typename KeyType>
        SetElementIndex FindIndex(const KeyType& key) const
        {
            return FindIndex(key, KeyFunc::GetHashCode(key));
        }

        /** Contains key index in sparse array */
        template <typename KeyType>
        SetElementIndex FindIndex(const KeyType& key, uint32 hashCode) const
        {
            if (Elements.Size() > 0)
            {
                for (SetElementIndex index = GetFirstIndex(hashCode); index.IsValid(); index = Elements[index].HashNextId)
                {
                    if (KeyFunc::Equals(KeyFunc::GetKey(Elements[index].Element), key))
                    {
                        // Return the first match, regardless of whether the set has multiple matches for the key or not.
                        return index;
                    }
                }
            }
            return SetElementIndex{};
        }

        /** Contains the head of SetElement list */
        SetElementIndex& GetFirstIndex(uint32 hashCode) const
        {
            ENSURE(Size() > 0);
            return ((SetElementIndex*)HashBucket.GetAllocation())[GetHashIndex(hashCode)];
        }

        uint32 GetHashIndex(uint32 hashCode) const
        {
            // Quick mod hashCode % BucketCount, BucketCount must be 2 ^ n
            return hashCode & (BucketCount - 1);
        }

        bool CheckRehash(int32 elementCount)
        {
            uint32 desiredBucketCount = SetAllocator::GetNumberOfHashBuckets(static_cast<uint32>(elementCount));
            if (elementCount > 0 && (!BucketCount || BucketCount < desiredBucketCount))
            {
                BucketCount = desiredBucketCount;
                Rehash();
                return true;
            }
            return false;
        }

        void Rehash()
        {
            // Free the old hash.
            HashBucket.Resize(0);

            if (BucketCount)
            {
                HashBucket.Resize(BucketCount);
                // Init HashBucket
                for (uint32 hashIndex = 0; hashIndex < BucketCount; ++hashIndex)
                {
                    ((SetElementIndex*)HashBucket.GetAllocation())[hashIndex] = SetElementIndex();
                }

                // Add the existing elements to the new hash.
                for (typename SparseArrayType::Iterator iter = Elements.begin(); iter != Elements.end(); ++iter)
                {
                    LinkElement(SetElementIndex(iter.GetIndex()), *iter, KeyFunc::GetHashCode(KeyFunc::GetKey((*iter).Element)));
                }
            }
        }

        void LinkElement(SetElementIndex elementIndex, SetElement& element, uint32 hashCode) const
        {
            // get the index of hash bucket.
            element.HashIndex = GetHashIndex(hashCode);

            // Link the element into the hash bucket.
            element.HashNextId = GetFirstIndex(hashCode);
            GetFirstIndex(hashCode) = elementIndex;
        }

        void CopyElement(const Set& other)
        {
            BucketCount = other.BucketCount;
            HashBucket.Resize(BucketCount);
            Memory::Memcpy(HashBucket.GetAllocation(), const_cast<byte*>(other.HashBucket.GetAllocation()), sizeof(SetElementIndex) * BucketCount);
            Elements = other.Elements;
        }

        void MoveElement(Set&& other)
        {
            BucketCount = other.BucketCount;
            other.BucketCount = 0;
            HashBucket = MoveTemp(other.HashBucket);
            Elements = MoveTemp(other.Elements);
        }

    private:
        /** count of hash bucket */
        uint32 BucketCount{ 0 };
        HashBucketType HashBucket;
        SparseArrayType Elements;
    };
}

template <typename KeyType>
void* operator new(size_t size, const typename Engine::Set<KeyType>::SetElement& element)
{
    return &element.Element;
}